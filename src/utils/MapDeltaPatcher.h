#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QUuid>
#include <QVariant>

#include <zlib.h>

#include <fcntl.h>
#include <unistd.h>

#include <functional>
#include <memory>

// Applies a published map delta to the installed MBTiles file.
//
// The delta is not a patch of the SQLite file. A tile payload is a gzip stream
// whose bytes change wholesale for a one-feature edit, and the file layout
// depends on insert order, so a binary diff of the file saves almost nothing.
// Instead the installed file is serialised into a canonical stream (schema,
// rows in a fixed order, tile payloads decompressed), the HDiffPatch delta is
// applied to that stream, and the result is turned back into an MBTiles file.
//
// The rebuilt file holds the same schema, rows and decompressed tiles as the
// published one but is not byte identical (the tile payloads are recompressed),
// so callers must record the published digest, not the file's own.
//
// The stream format is defined by tools/canonical-mbtiles.py in
// librescoot/osm-tiles; this is the reader and writer for the dashboard.
class MapDeltaPatcher
{
public:
    enum class Result { Ok, Cancelled, Failed };
    struct Outcome {
        Result result = Result::Failed;
        QString error;
        bool ok() const { return result == Result::Ok; }
    };

    struct Header {
        QString algorithm;
        QString format;
        QString region;
        QString baseSha256;
        QString baseCanonicalSha256;
        QString targetCanonicalSha256;
        qint64 baseCanonicalSize = 0;
        qint64 targetCanonicalSize = 0;
        qint64 targetSize = 0;
    };

    static constexpr char kFormat[] = "mbtiles-canonical-1";

    // Peak extra disk use of applyDelta(), for the caller's free-space check:
    // both canonical streams exist together while hpatchz runs, and the rebuilt
    // file is written while the target stream is still there.
    static qint64 diskNeeded(const Header &h, qint64 patchSize)
    {
        return h.baseCanonicalSize + h.targetCanonicalSize + patchSize
            + std::max(h.targetSize, h.baseCanonicalSize);
    }

    static bool readHeader(const QString &path, Header *header, QString *error)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("cannot open map delta");
            return false;
        }
        QByteArray magic = f.read(4);
        QByteArray len = f.read(4);
        if (magic != "MDP1" || len.size() != 4) {
            if (error) *error = QStringLiteral("not a map delta");
            return false;
        }
        const quint32 headerLen = fromBe32(len);
        if (headerLen == 0 || headerLen > 64 * 1024) {
            if (error) *error = QStringLiteral("map delta header is implausible");
            return false;
        }
        const QByteArray json = f.read(headerLen);
        QJsonParseError parse;
        const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
        if (json.size() != int(headerLen) || parse.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) *error = QStringLiteral("map delta header is malformed");
            return false;
        }
        const QJsonObject o = doc.object();
        header->algorithm = o.value(QStringLiteral("algorithm")).toString();
        header->format = o.value(QStringLiteral("format")).toString();
        header->region = o.value(QStringLiteral("region")).toString();
        header->baseSha256 = o.value(QStringLiteral("base_sha256")).toString();
        header->baseCanonicalSha256 = o.value(QStringLiteral("base_canonical_sha256")).toString();
        header->targetCanonicalSha256 = o.value(QStringLiteral("target_canonical_sha256")).toString();
        header->baseCanonicalSize = qint64(o.value(QStringLiteral("base_canonical_size")).toDouble());
        header->targetCanonicalSize = qint64(o.value(QStringLiteral("target_canonical_size")).toDouble());
        header->targetSize = qint64(o.value(QStringLiteral("target_size")).toDouble());
        return true;
    }

    // Writes the canonical stream of an MBTiles file. Returns the SHA-256 of
    // the stream, or an empty string on failure or cancellation.
    static QString canonicalize(const QString &mbtiles, const QString &output,
                                const std::function<bool()> &cancelled, QString *error)
    {
        const QString connection = QStringLiteral("mapdelta_read_") + QUuid::createUuid().toString(QUuid::Id128);
        QString digest;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(mbtiles);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            if (!db.open()) {
                if (error) *error = QStringLiteral("cannot open installed map");
            } else {
                digest = canonicalizeOpen(db, output, cancelled, error);
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connection);
        return digest;
    }

    static Outcome applyDelta(const QString &decoder, const QString &installedMap,
                              const QString &delta, const QString &output, const QString &workDir,
                              const QString &expectedBaseSha256,
                              const std::function<bool()> &cancelled)
    {
        const QString tag = QUuid::createUuid().toString(QUuid::Id128).left(12);
        const QString baseCanon = workDir + QStringLiteral("/map-base-") + tag + QStringLiteral(".canon");
        const QString targetCanon = workDir + QStringLiteral("/map-target-") + tag + QStringLiteral(".canon");
        const QString patch = workDir + QStringLiteral("/map-patch-") + tag + QStringLiteral(".hdiff");
        const auto cleanup = [&] {
            QFile::remove(baseCanon);
            QFile::remove(targetCanon);
            QFile::remove(patch);
        };
        const auto fail = [&](Result result, const QString &message) {
            cleanup();
            QFile::remove(output);
            return Outcome{result, message};
        };
        const auto stopped = [&] { return cancelled && cancelled(); };

        Header h;
        QString error;
        if (!readHeader(delta, &h, &error))
            return fail(Result::Failed, error);
        if (h.algorithm != QLatin1String("hdiffpatch") || h.format != QLatin1String(kFormat))
            return fail(Result::Failed, QStringLiteral("unsupported map delta format"));
        if (!expectedBaseSha256.isEmpty() && h.baseSha256 != expectedBaseSha256)
            return fail(Result::Failed, QStringLiteral("map delta is for a different base"));
        if (!isSha256(h.baseCanonicalSha256) || !isSha256(h.targetCanonicalSha256)
            || h.baseCanonicalSize <= 0 || h.targetCanonicalSize <= 0 || h.targetSize <= 0)
            return fail(Result::Failed, QStringLiteral("map delta header is incomplete"));

        if (!extractPayload(delta, patch, &error))
            return fail(Result::Failed, error);
        if (stopped()) return fail(Result::Cancelled, {});

        const QString baseDigest = canonicalize(installedMap, baseCanon, cancelled, &error);
        if (stopped()) return fail(Result::Cancelled, {});
        if (baseDigest.isEmpty())
            return fail(Result::Failed, error);
        if (baseDigest != h.baseCanonicalSha256)
            return fail(Result::Failed, QStringLiteral("installed map is not the base of this delta"));

        QProcess process;
        process.setProgram(decoder);
        process.setArguments({QStringLiteral("-s-8m"), baseCanon, patch, targetCanon});
        process.start();
        if (!process.waitForStarted(10000))
            return fail(Result::Failed, QStringLiteral("could not start map delta decoder"));
        QElapsedTimer timer;
        timer.start();
        while (!process.waitForFinished(100)) {
            if (process.state() == QProcess::NotRunning) break;
            if (stopped() || timer.elapsed() > 30 * 60 * 1000
                || QFileInfo(targetCanon).size() > h.targetCanonicalSize) {
                process.kill();
                process.waitForFinished();
                return fail(stopped() ? Result::Cancelled : Result::Failed,
                            QStringLiteral("map delta decoding interrupted"));
            }
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
            || QFileInfo(targetCanon).size() != h.targetCanonicalSize)
            return fail(Result::Failed, QStringLiteral("map delta decoding failed"));
        QFile::remove(baseCanon);
        QFile::remove(patch);

        const QString targetDigest = hashFile(targetCanon, cancelled);
        if (stopped()) return fail(Result::Cancelled, {});
        if (targetDigest != h.targetCanonicalSha256)
            return fail(Result::Failed, QStringLiteral("reconstructed map failed verification"));

        const Outcome built = rebuild(targetCanon, output, cancelled);
        QFile::remove(targetCanon);
        if (!built.ok()) {
            QFile::remove(output);
            return built;
        }
        if (QFileInfo(output).size() <= 0)
            return fail(Result::Failed, QStringLiteral("rebuilt map is empty"));
        return {Result::Ok, {}};
    }

    // Rebuilds an MBTiles file from a canonical stream. The stream is trusted
    // to be verified by the caller; this still rejects malformed input.
    static Outcome rebuild(const QString &canonical, const QString &output,
                           const std::function<bool()> &cancelled)
    {
        QFile::remove(output);
        QFile in(canonical);
        if (!in.open(QIODevice::ReadOnly))
            return {Result::Failed, QStringLiteral("cannot read canonical map")};
        Reader reader(&in);
        const auto bad = [&](const QString &message) {
            return Outcome{Result::Failed, message};
        };

        const QString connection = QStringLiteral("mapdelta_write_") + QUuid::createUuid().toString(QUuid::Id128);
        Outcome outcome;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(output);
            if (!db.open()) {
                outcome = bad(QStringLiteral("cannot create rebuilt map"));
            } else {
                outcome = rebuildOpen(db, reader, cancelled);
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connection);
        if (!outcome.ok()) {
            QFile::remove(output);
            return outcome;
        }
        const int fd = ::open(QFile::encodeName(output).constData(), O_RDONLY);
        if (fd >= 0) {
            const int rc = ::fsync(fd);
            ::close(fd);
            if (rc != 0) {
                QFile::remove(output);
                return bad(QStringLiteral("cannot sync rebuilt map"));
            }
        }
        return outcome;
    }

    static QByteArray gunzip(const QByteArray &data, bool *ok = nullptr)
    {
        z_stream s{};
        if (inflateInit2(&s, 15 + 32) != Z_OK) { if (ok) *ok = false; return {}; }
        QByteArray out;
        out.reserve(data.size() * 3);
        s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
        s.avail_in = uInt(data.size());
        char buf[65536];
        int rc = Z_OK;
        while (rc == Z_OK) {
            s.next_out = reinterpret_cast<Bytef *>(buf);
            s.avail_out = sizeof(buf);
            rc = inflate(&s, Z_NO_FLUSH);
            out.append(buf, int(sizeof(buf) - s.avail_out));
        }
        inflateEnd(&s);
        if (ok) *ok = (rc == Z_STREAM_END);
        return rc == Z_STREAM_END ? out : QByteArray();
    }

    static QByteArray gzip(const QByteArray &data, int level = 6)
    {
        z_stream s{};
        if (deflateInit2(&s, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return {};
        QByteArray out;
        out.resize(int(deflateBound(&s, uLong(data.size()))));
        s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
        s.avail_in = uInt(data.size());
        s.next_out = reinterpret_cast<Bytef *>(out.data());
        s.avail_out = uInt(out.size());
        const int rc = deflate(&s, Z_FINISH);
        const int used = int(out.size() - s.avail_out);
        deflateEnd(&s);
        if (rc != Z_STREAM_END) return {};
        out.resize(used);
        return out;
    }

private:
    static quint32 fromBe32(const QByteArray &b)
    {
        const auto *p = reinterpret_cast<const uchar *>(b.constData());
        return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
    }

    static bool isSha256(const QString &s)
    {
        if (s.size() != 64) return false;
        for (QChar c : s)
            if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f'))) return false;
        return true;
    }

    static bool extractPayload(const QString &delta, const QString &out, QString *error)
    {
        QFile f(delta);
        if (!f.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("cannot open map delta");
            return false;
        }
        f.read(4);
        const quint32 headerLen = fromBe32(f.read(4));
        f.seek(8 + headerLen);
        const QByteArray lenBytes = f.read(8);
        if (lenBytes.size() != 8) {
            if (error) *error = QStringLiteral("truncated map delta");
            return false;
        }
        quint64 payloadLen = 0;
        for (int i = 0; i < 8; ++i)
            payloadLen = (payloadLen << 8) | quint8(lenBytes[i]);
        if (qint64(payloadLen) != f.size() - f.pos()) {
            if (error) *error = QStringLiteral("truncated map delta");
            return false;
        }
        QFile o(out);
        if (!o.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error) *error = QStringLiteral("cannot stage map delta");
            return false;
        }
        char buf[1 << 16];
        while (!f.atEnd()) {
            const qint64 n = f.read(buf, sizeof(buf));
            if (n <= 0 || o.write(buf, n) != n) {
                if (error) *error = QStringLiteral("cannot stage map delta");
                return false;
            }
        }
        return o.flush();
    }

    static QString hashFile(const QString &path, const std::function<bool()> &cancelled)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        char buf[1 << 16];
        while (!f.atEnd()) {
            if (cancelled && cancelled()) return {};
            const qint64 n = f.read(buf, sizeof(buf));
            if (n <= 0) return {};
            hash.addData(QByteArrayView(buf, n));
        }
        return QString::fromLatin1(hash.result().toHex());
    }

    // Canonical value encoding, see tools/canonical-mbtiles.py.
    static void putBe(QByteArray &b, quint64 v, int bytes)
    {
        for (int i = bytes - 1; i >= 0; --i)
            b.append(char((v >> (8 * i)) & 0xff));
    }

    static void putText(QByteArray &b, char tag, const QByteArray &raw)
    {
        b.append(tag);
        putBe(b, quint64(raw.size()), 4);
        b.append(raw);
    }

    // `type` is SQLite's own typeof() of the value, so NULL, an empty string
    // and an empty blob stay distinct however the Qt driver maps them.
    static void putTypedValue(QByteArray &b, const QString &type, const QVariant &v)
    {
        if (type == QLatin1String("null")) {
            b.append('n');
        } else if (type == QLatin1String("integer")) {
            b.append('i');
            putBe(b, quint64(v.toLongLong()), 8);
        } else if (type == QLatin1String("real")) {
            b.append('f');
            const double d = v.toDouble();
            quint64 bits;
            static_assert(sizeof(bits) == sizeof(d));
            memcpy(&bits, &d, sizeof(bits));
            putBe(b, bits, 8);
        } else if (type == QLatin1String("blob")) {
            putText(b, 'b', v.toByteArray());
        } else {
            putText(b, 's', v.toString().toUtf8());
        }
    }

    static QString canonicalizeOpen(QSqlDatabase &db, const QString &output,
                                    const std::function<bool()> &cancelled, QString *error)
    {
        const auto fail = [&](const QString &m) { if (error) *error = m; return QString(); };
        QFile out(output);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return fail(QStringLiteral("cannot stage installed map"));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        const auto writeOut = [&](const QByteArray &data) -> bool {
            hash.addData(data);
            return out.write(data) == data.size();
        };
        if (!writeOut("MBTC1\n"))
            return fail(QStringLiteral("cannot stage installed map"));

        QSqlQuery schema(db);
        schema.setForwardOnly(true);
        if (!schema.exec(QStringLiteral("SELECT type, name, tbl_name, sql FROM sqlite_master "
                                        "WHERE sql IS NOT NULL ORDER BY type, name")))
            return fail(QStringLiteral("cannot read installed map"));
        while (schema.next()) {
            QByteArray chunk("S");
            for (int i = 0; i < 4; ++i) putTypedValue(chunk, QStringLiteral("text"), schema.value(i));
            if (!writeOut(chunk)) return fail(QStringLiteral("cannot stage installed map"));
        }

        QStringList tables;
        QSqlQuery names(db);
        if (!names.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' "
                                       "AND name NOT LIKE 'sqlite_%' ORDER BY name")))
            return fail(QStringLiteral("cannot read installed map"));
        while (names.next()) tables << names.value(0).toString();

        for (const QString &table : tables) {
            QStringList columns;
            QList<QPair<int, QString>> primary;
            QSqlQuery info(db);
            if (!info.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table)))
                return fail(QStringLiteral("cannot read installed map"));
            while (info.next()) {
                columns << info.value(1).toString();
                if (info.value(5).toInt() > 0)
                    primary.append({info.value(5).toInt(), info.value(1).toString()});
            }
            std::sort(primary.begin(), primary.end());
            QStringList keys;
            if (table == QLatin1String("tiles"))
                keys = {QStringLiteral("zoom_level"), QStringLiteral("tile_column"), QStringLiteral("tile_row")};
            else if (!primary.isEmpty())
                for (const auto &p : primary) keys << p.second;
            else
                keys = columns;
            QStringList quoted;
            for (const QString &k : keys) quoted << QStringLiteral("\"%1\"").arg(k);

            QByteArray head("T");
            putTypedValue(head, QStringLiteral("text"), table);
            if (!writeOut(head)) return fail(QStringLiteral("cannot stage installed map"));

            const int tileIndex = table == QLatin1String("tiles") ? int(columns.indexOf(QStringLiteral("tile_data"))) : -1;
            QSqlQuery rows(db);
            rows.setForwardOnly(true);
            QStringList select;
            for (const QString &c : columns)
                select << QStringLiteral("typeof(\"%1\"), \"%1\"").arg(c);
            if (!rows.exec(QStringLiteral("SELECT %1 FROM \"%2\" ORDER BY %3")
                               .arg(select.join(QLatin1Char(',')), table, quoted.join(QLatin1Char(',')))))
                return fail(QStringLiteral("cannot read installed map"));
            int count = 0;
            while (rows.next()) {
                if ((++count & 0x3ff) == 0 && cancelled && cancelled()) return QString();
                QByteArray chunk("R");
                for (int i = 0; i < columns.size(); ++i) {
                    const QString type = rows.value(2 * i).toString();
                    QVariant v = rows.value(2 * i + 1);
                    if (i == tileIndex && type == QLatin1String("blob")) {
                        bool ok = false;
                        const QByteArray raw = gunzip(v.toByteArray(), &ok);
                        if (!ok) return fail(QStringLiteral("installed map has an unreadable tile"));
                        v = raw;
                    }
                    putTypedValue(chunk, type, v);
                }
                if (!writeOut(chunk)) return fail(QStringLiteral("cannot stage installed map"));
            }
            if (!writeOut("E")) return fail(QStringLiteral("cannot stage installed map"));
        }
        if (!out.flush()) return fail(QStringLiteral("cannot stage installed map"));
        return QString::fromLatin1(hash.result().toHex());
    }

    class Reader
    {
    public:
        explicit Reader(QFile *f) : m_file(f), m_buf(1 << 20, Qt::Uninitialized) {}

        bool atEnd() { return m_pos >= m_len && !fill(); }

        bool read(char *dst, qint64 n)
        {
            while (n > 0) {
                if (m_pos >= m_len && !fill()) return false;
                const qint64 take = std::min<qint64>(n, m_len - m_pos);
                memcpy(dst, m_buf.constData() + m_pos, size_t(take));
                dst += take; m_pos += take; n -= take;
            }
            return true;
        }

        bool readByte(char *c) { return read(c, 1); }

        bool readBytes(qint64 n, QByteArray *out)
        {
            if (n < 0 || n > (qint64(1) << 31)) return false;
            out->resize(int(n));
            return read(out->data(), n);
        }

        bool readBe(int bytes, quint64 *v)
        {
            char b[8];
            if (!read(b, bytes)) return false;
            quint64 r = 0;
            for (int i = 0; i < bytes; ++i) r = (r << 8) | quint8(b[i]);
            *v = r;
            return true;
        }

        bool readValue(QVariant *v)
        {
            char t;
            if (!readByte(&t)) return false;
            switch (t) {
            case 'n': *v = QVariant(QMetaType::fromType<QString>()); return true;  // null-typed: binds NULL
            case 'i': { quint64 x; if (!readBe(8, &x)) return false; *v = qlonglong(x); return true; }
            case 'f': {
                quint64 x; if (!readBe(8, &x)) return false;
                double d; memcpy(&d, &x, sizeof(d)); *v = d; return true;
            }
            case 's': case 'b': {
                quint64 n; if (!readBe(4, &n)) return false;
                QByteArray raw; if (!readBytes(qint64(n), &raw)) return false;
                // A zero-length value must stay non-null: a null QString or
                // QByteArray binds as NULL, not as '' or an empty blob.
                if (t == 's')
                    *v = raw.isEmpty() ? QVariant(QStringLiteral("")) : QVariant(QString::fromUtf8(raw));
                else
                    *v = raw.isEmpty() ? QVariant(QByteArray("", 0)) : QVariant(raw);
                return true;
            }
            default: return false;
            }
        }

    private:
        bool fill()
        {
            m_len = m_file->read(m_buf.data(), m_buf.size());
            m_pos = 0;
            return m_len > 0;
        }

        QFile *m_file;
        QByteArray m_buf;
        qint64 m_pos = 0;
        qint64 m_len = 0;
    };

    static Outcome rebuildOpen(QSqlDatabase &db, Reader &r, const std::function<bool()> &cancelled)
    {
        const auto bad = [](const QString &m) { return Outcome{Result::Failed, m}; };
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=OFF"));
        pragma.exec(QStringLiteral("PRAGMA synchronous=OFF"));

        char magic[6];
        if (!r.read(magic, 6) || memcmp(magic, "MBTC1\n", 6) != 0)
            return bad(QStringLiteral("canonical map has the wrong format"));

        struct Entry { QString type, name, tbl, sql; };
        QList<Entry> schema;
        char token;
        if (!r.readByte(&token)) return bad(QStringLiteral("canonical map is empty"));
        while (token == 'S') {
            QVariant v[4];
            for (auto &x : v) if (!r.readValue(&x)) return bad(QStringLiteral("canonical map is truncated"));
            schema.append({v[0].toString(), v[1].toString(), v[2].toString(), v[3].toString()});
            if (!r.readByte(&token)) return bad(QStringLiteral("canonical map is truncated"));
        }

        QSqlQuery q(db);
        if (!db.transaction()) return bad(QStringLiteral("cannot start rebuild"));
        for (const auto &e : schema) {
            if (e.type != QLatin1String("table") || e.name.startsWith(QLatin1String("sqlite_"))) continue;
            if (!q.exec(e.sql)) return bad(QStringLiteral("cannot rebuild map schema"));
        }

        qint64 tileRows = 0;
        while (token == 'T') {
            QVariant nameValue;
            if (!r.readValue(&nameValue)) return bad(QStringLiteral("canonical map is truncated"));
            const QString table = nameValue.toString();
            QStringList columns;
            QSqlQuery info(db);
            if (!info.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table)))
                return bad(QStringLiteral("cannot rebuild map schema"));
            while (info.next()) columns << info.value(1).toString();
            if (columns.isEmpty()) return bad(QStringLiteral("canonical map names an unknown table"));
            const int tileIndex = table == QLatin1String("tiles") ? int(columns.indexOf(QStringLiteral("tile_data"))) : -1;
            QStringList marks(columns.size(), QStringLiteral("?"));
            QSqlQuery insert(db);
            if (!insert.prepare(QStringLiteral("INSERT INTO \"%1\" VALUES (%2)").arg(table, marks.join(QLatin1Char(',')))))
                return bad(QStringLiteral("cannot rebuild map rows"));

            if (!r.readByte(&token)) return bad(QStringLiteral("canonical map is truncated"));
            qint64 n = 0;
            while (token == 'R') {
                if ((++n & 0x3ff) == 0 && cancelled && cancelled())
                    return {Result::Cancelled, {}};
                for (int i = 0; i < columns.size(); ++i) {
                    QVariant v;
                    if (!r.readValue(&v)) return bad(QStringLiteral("canonical map is truncated"));
                    if (i == tileIndex && !v.isNull()) {
                        const QByteArray packed = gzip(v.toByteArray());
                        if (packed.isEmpty()) return bad(QStringLiteral("cannot compress a rebuilt tile"));
                        v = packed;
                    }
                    insert.addBindValue(v);
                }
                if (!insert.exec()) return bad(QStringLiteral("cannot write a rebuilt row"));
                if (tileIndex >= 0) ++tileRows;
                if (!r.readByte(&token)) return bad(QStringLiteral("canonical map is truncated"));
            }
            if (token != 'E') return bad(QStringLiteral("canonical map is malformed"));
            if (!r.readByte(&token)) { token = 0; break; }
        }
        if (token != 0) return bad(QStringLiteral("canonical map has trailing data"));

        for (const auto &e : schema) {
            if (e.type == QLatin1String("table") || e.name.startsWith(QLatin1String("sqlite_"))) continue;
            if (!q.exec(e.sql)) return bad(QStringLiteral("cannot rebuild map indexes"));
        }
        if (!db.commit()) return bad(QStringLiteral("cannot commit rebuilt map"));

        QSqlQuery check(db);
        if (!check.exec(QStringLiteral("PRAGMA integrity_check")) || !check.next()
            || check.value(0).toString() != QLatin1String("ok"))
            return bad(QStringLiteral("rebuilt map failed its integrity check"));
        QSqlQuery count(db);
        if (!count.exec(QStringLiteral("SELECT count(*) FROM tiles")) || !count.next()
            || count.value(0).toLongLong() != tileRows || tileRows == 0)
            return bad(QStringLiteral("rebuilt map has the wrong tile count"));
        return {Result::Ok, {}};
    }
};
