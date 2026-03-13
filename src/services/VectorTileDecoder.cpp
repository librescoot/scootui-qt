#include "VectorTileDecoder.h"
#include <zlib.h>
#include <cstring>

namespace VectorTile {

// ---------------------------------------------------------------------------
// Gzip decompression
// ---------------------------------------------------------------------------

QByteArray gunzip(const QByteArray &compressed)
{
    if (compressed.isEmpty())
        return {};

    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));

    // 15 + 16 = gzip auto-detect
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
        return {};

    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    QByteArray result;
    result.reserve(compressed.size() * 4);

    char buf[16384];
    int ret;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buf);
        stream.avail_out = sizeof(buf);
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            return {};
        }
        result.append(buf, sizeof(buf) - stream.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
    return result;
}

// ---------------------------------------------------------------------------
// Minimal protobuf wire-format reader
// ---------------------------------------------------------------------------

class PbReader
{
public:
    PbReader(const QByteArray &ba)
        : m_data(reinterpret_cast<const uint8_t *>(ba.constData()))
        , m_end(m_data + ba.size())
    {}

    PbReader(const uint8_t *data, int size)
        : m_data(data), m_end(data + size)
    {}

    bool atEnd() const { return m_data >= m_end; }

    uint64_t readVarint()
    {
        uint64_t result = 0;
        int shift = 0;
        while (m_data < m_end) {
            uint8_t b = *m_data++;
            result |= static_cast<uint64_t>(b & 0x7F) << shift;
            if ((b & 0x80) == 0)
                return result;
            shift += 7;
        }
        return result;
    }

    QByteArray readBytes()
    {
        uint64_t len = readVarint();
        if (m_data + len > m_end)
            len = m_end - m_data;
        QByteArray result(reinterpret_cast<const char *>(m_data), static_cast<int>(len));
        m_data += len;
        return result;
    }

    PbReader readSubmessage()
    {
        uint64_t len = readVarint();
        if (m_data + len > m_end)
            len = m_end - m_data;
        PbReader sub(m_data, static_cast<int>(len));
        m_data += len;
        return sub;
    }

    QVector<uint32_t> readPackedUint32()
    {
        uint64_t len = readVarint();
        const uint8_t *end = m_data + len;
        if (end > m_end) end = m_end;
        QVector<uint32_t> result;
        while (m_data < end) {
            uint64_t v = 0;
            int shift = 0;
            while (m_data < end) {
                uint8_t b = *m_data++;
                v |= static_cast<uint64_t>(b & 0x7F) << shift;
                if ((b & 0x80) == 0) break;
                shift += 7;
            }
            result.append(static_cast<uint32_t>(v));
        }
        return result;
    }

    uint32_t readFixed32()
    {
        uint32_t val = 0;
        if (m_data + 4 <= m_end) {
            std::memcpy(&val, m_data, 4);
            m_data += 4;
        }
        return val;
    }

    uint64_t readFixed64()
    {
        uint64_t val = 0;
        if (m_data + 8 <= m_end) {
            std::memcpy(&val, m_data, 8);
            m_data += 8;
        }
        return val;
    }

    void skip(int wireType)
    {
        switch (wireType) {
        case 0: readVarint(); break;
        case 1: m_data += 8; break;
        case 2: { uint64_t len = readVarint(); m_data += len; } break;
        case 5: m_data += 4; break;
        default: m_data = m_end; break;
        }
    }

    std::pair<int, int> readTag()
    {
        uint64_t t = readVarint();
        return {static_cast<int>(t >> 3), static_cast<int>(t & 7)};
    }

private:
    const uint8_t *m_data;
    const uint8_t *m_end;
};

// ---------------------------------------------------------------------------
// Vector tile parsing
// ---------------------------------------------------------------------------

static int32_t zigzagDecode(uint32_t n)
{
    return static_cast<int32_t>((n >> 1) ^ -(n & 1));
}

// Parse a Value submessage to a string representation
static QString parseValue(PbReader &r)
{
    QString result;
    while (!r.atEnd()) {
        auto [field, wire] = r.readTag();
        switch (field) {
        case 1: // string_value
            result = QString::fromUtf8(r.readBytes());
            break;
        case 2: { // float_value (wire type 5)
            uint32_t bits = r.readFixed32();
            float val;
            std::memcpy(&val, &bits, sizeof(float));
            result = QString::number(val);
            break;
        }
        case 3: { // double_value (wire type 1)
            uint64_t bits = r.readFixed64();
            double val;
            std::memcpy(&val, &bits, sizeof(double));
            result = QString::number(val);
            break;
        }
        case 4: // int_value (int64, varint)
            result = QString::number(static_cast<int64_t>(r.readVarint()));
            break;
        case 5: // uint_value (uint64, varint)
            result = QString::number(r.readVarint());
            break;
        case 6: { // sint_value (sint64, zigzag varint)
            uint64_t v = r.readVarint();
            int64_t sv = static_cast<int64_t>((v >> 1) ^ -(v & 1));
            result = QString::number(sv);
            break;
        }
        case 7: // bool_value
            result = r.readVarint() ? QStringLiteral("true") : QStringLiteral("false");
            break;
        default:
            r.skip(wire);
            break;
        }
    }
    return result;
}

// Raw feature data before property resolution
struct RawFeature {
    int type = 0;
    QVector<uint32_t> geometry;
    QVector<uint32_t> tags;
};

static Layer parseLayer(PbReader &r)
{
    Layer layer;
    QStringList keys;
    QStringList values;
    QVector<RawFeature> rawFeatures;

    while (!r.atEnd()) {
        auto [field, wire] = r.readTag();
        switch (field) {
        case 1: // name
            layer.name = QString::fromUtf8(r.readBytes());
            break;
        case 2: { // feature
            PbReader sub = r.readSubmessage();
            RawFeature rf;
            while (!sub.atEnd()) {
                auto [ff, fw] = sub.readTag();
                switch (ff) {
                case 2: rf.tags = sub.readPackedUint32(); break;
                case 3: rf.type = static_cast<int>(sub.readVarint()); break;
                case 4: rf.geometry = sub.readPackedUint32(); break;
                default: sub.skip(fw); break;
                }
            }
            rawFeatures.append(std::move(rf));
            break;
        }
        case 3: // keys (repeated string)
            keys.append(QString::fromUtf8(r.readBytes()));
            break;
        case 4: { // values (repeated Value message)
            PbReader sub = r.readSubmessage();
            values.append(parseValue(sub));
            break;
        }
        case 5: // extent
            layer.extent = static_cast<uint32_t>(r.readVarint());
            break;
        default:
            r.skip(wire);
            break;
        }
    }

    // Resolve feature tags to properties using layer keys/values
    layer.features.reserve(rawFeatures.size());
    for (const auto &rf : rawFeatures) {
        Feature f;
        f.type = rf.type;
        f.geometry = rf.geometry;
        for (int i = 0; i + 1 < rf.tags.size(); i += 2) {
            int keyIdx = static_cast<int>(rf.tags[i]);
            int valIdx = static_cast<int>(rf.tags[i + 1]);
            if (keyIdx < keys.size() && valIdx < values.size()) {
                f.properties.insert(keys[keyIdx], values[valIdx]);
            }
        }
        layer.features.append(std::move(f));
    }

    return layer;
}

Tile parse(const QByteArray &data)
{
    Tile tile;
    PbReader r(data);
    while (!r.atEnd()) {
        auto [field, wire] = r.readTag();
        if (field == 3 && wire == 2) { // layers
            PbReader sub = r.readSubmessage();
            tile.layers.append(parseLayer(sub));
        } else {
            r.skip(wire);
        }
    }
    return tile;
}

// ---------------------------------------------------------------------------
// Geometry decoding
// ---------------------------------------------------------------------------

QPointF decodePoint(const QVector<uint32_t> &geometry)
{
    if (geometry.size() < 3)
        return {};

    uint32_t rawX = geometry[1];
    uint32_t rawY = geometry[2];

    return QPointF(zigzagDecode(rawX), zigzagDecode(rawY));
}

QVector<QPointF> decodeLineString(const QVector<uint32_t> &geometry)
{
    QVector<QPointF> points;
    int i = 0;
    int32_t cursorX = 0, cursorY = 0;

    while (i < geometry.size()) {
        uint32_t cmd = geometry[i++];
        int cmdId = cmd & 0x7;
        int count = cmd >> 3;

        if (cmdId == 1 || cmdId == 2) { // MoveTo or LineTo
            for (int j = 0; j < count && i + 1 < geometry.size(); ++j) {
                cursorX += zigzagDecode(geometry[i++]);
                cursorY += zigzagDecode(geometry[i++]);
                points.append(QPointF(cursorX, cursorY));
            }
        } else if (cmdId == 7) { // ClosePath
            // Not relevant for linestrings
        }
    }

    return points;
}

} // namespace VectorTile
