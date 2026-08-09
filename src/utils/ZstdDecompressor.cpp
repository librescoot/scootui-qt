#include "utils/ZstdDecompressor.h"

#include <QFile>

#include <zstd.h>
#include <unistd.h>
#include <vector>

bool ZstdDecompressor::decompressFile(const QString &srcPath,
                                      const QString &destPath,
                                      qint64 expectedSize,
                                      const std::function<void(qint64)> &progress,
                                      QString *errorString)
{
    auto fail = [errorString](const QString &msg) {
        if (errorString)
            *errorString = msg;
        return false;
    };

    QFile in(srcPath);
    if (!in.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Could not open the downloaded archive"));

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Could not open the map file for writing"));

    ZSTD_DStream *stream = ZSTD_createDStream();
    if (!stream)
        return fail(QStringLiteral("Could not allocate the decompressor"));

    const size_t inCap = ZSTD_DStreamInSize();
    const size_t outCap = ZSTD_DStreamOutSize();
    std::vector<char> inBuf(inCap);
    std::vector<char> outBuf(outCap);

    qint64 written = 0;
    bool frameComplete = false;

    while (true) {
        const qint64 read = in.read(inBuf.data(), static_cast<qint64>(inCap));
        if (read < 0) {
            ZSTD_freeDStream(stream);
            return fail(QStringLiteral("Could not read the downloaded archive"));
        }
        if (read == 0)
            break;

        ZSTD_inBuffer input{inBuf.data(), static_cast<size_t>(read), 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{outBuf.data(), outCap, 0};
            const size_t ret = ZSTD_decompressStream(stream, &output, &input);
            if (ZSTD_isError(ret)) {
                const QString reason = QString::fromLatin1(ZSTD_getErrorName(ret));
                ZSTD_freeDStream(stream);
                return fail(QStringLiteral("Map data is corrupt: ") + reason);
            }
            if (output.pos > 0) {
                const qint64 w = out.write(outBuf.data(), static_cast<qint64>(output.pos));
                if (w != static_cast<qint64>(output.pos)) {
                    ZSTD_freeDStream(stream);
                    return fail(QStringLiteral("Could not write map data (disk full?)"));
                }
                written += w;
                if (progress)
                    progress(written);
            }
            // A zero return means a frame boundary was reached cleanly. Any
            // other value means the decoder still wants input.
            frameComplete = (ret == 0);
        }
    }

    ZSTD_freeDStream(stream);

    if (!frameComplete)
        return fail(QStringLiteral("Map data is incomplete"));

    if (expectedSize > 0 && written != expectedSize) {
        return fail(QStringLiteral("Map data is the wrong size (%1 of %2 bytes)")
                        .arg(written)
                        .arg(expectedSize));
    }

    if (!out.flush())
        return fail(QStringLiteral("Could not write map data (disk full?)"));
    if (::fsync(out.handle()) != 0)
        return fail(QStringLiteral("Could not flush map data to disk"));
    out.close();
    return true;
}
