#pragma once

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <functional>

class HDiffPatcher
{
public:
    enum class Result { Ok, Cancelled, Failed };
    struct Outcome {
        Result result = Result::Failed;
        QString error;
        bool ok() const { return result == Result::Ok; }
    };

    static Outcome apply(const QString &decoder, const QString &base, const QString &patch,
                         const QString &output, const QString &baseDigest,
                         const QString &targetDigest, qint64 targetSize,
                         const std::function<bool()> &cancelled)
    {
        const auto fail = [&output](Result result, const QString &error) {
            QFile::remove(output);
            return Outcome{result, error};
        };
        const auto digest = [&cancelled](const QString &path) -> QString {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return {};
            QCryptographicHash hash(QCryptographicHash::Sha256);
            char buf[65536];
            while (!file.atEnd()) {
                if (cancelled()) return {};
                const qint64 n = file.read(buf, sizeof(buf));
                if (n <= 0) return {};
                hash.addData(QByteArrayView(buf, n));
            }
            return QString::fromLatin1(hash.result().toHex());
        };
        if (digest(base) != baseDigest)
            return fail(cancelled() ? Result::Cancelled : Result::Failed, QStringLiteral("Routing base verification failed"));
        QFile::remove(output);
        QProcess process;
        process.setProgram(decoder);
        process.setArguments({QStringLiteral("-s-8m"), base, patch, output});
        process.start();
        if (!process.waitForStarted(10000))
            return fail(Result::Failed, QStringLiteral("Could not start routing delta decoder"));
        QElapsedTimer timer;
        timer.start();
        while (!process.waitForFinished(100)) {
            if (process.state() == QProcess::NotRunning) break;
            if (cancelled() || timer.elapsed() > 15 * 60 * 1000 || QFileInfo(output).size() > targetSize) {
                process.kill();
                process.waitForFinished();
                return fail(cancelled() ? Result::Cancelled : Result::Failed, QStringLiteral("Routing delta interrupted"));
            }
        }
        if (cancelled()) return fail(Result::Cancelled, {});
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
            || QFileInfo(output).size() != targetSize || digest(output) != targetDigest
            || digest(base) != baseDigest)
            return fail(cancelled() ? Result::Cancelled : Result::Failed, QStringLiteral("Routing delta verification failed"));
        return {Result::Ok, {}};
    }
};
