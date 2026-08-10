#include "utils/ZstdDecompressor.h"

#include <QFile>

#include <zstd.h>
#include <zstd_errors.h>
#include <unistd.h>
#include <vector>

ZstdDecompressor::Outcome ZstdDecompressor::decompressFile(
    const QString &srcPath,
    const QString &destPath,
    qint64 expectedSize,
    const std::function<void(qint64)> &progress,
    const std::function<bool()> &shouldCancel)
{
    QFile in(srcPath);
    if (!in.open(QIODevice::ReadOnly))
        return {Result::LocalFailure, QStringLiteral("Could not open the downloaded archive")};

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {Result::LocalFailure, QStringLiteral("Could not open the map file for writing")};

    ZSTD_DStream *stream = ZSTD_createDStream();
    if (!stream) {
        out.close();
        QFile::remove(destPath);
        return {Result::LocalFailure, QStringLiteral("Could not allocate the decompressor")};
    }

    struct StreamGuard {
        ZSTD_DStream *s;
        ~StreamGuard() { ZSTD_freeDStream(s); }
    } guard{stream};

    // destPath is always a .part, never the installed file, so a half-written
    // one is worth nothing to anybody and goes away with the failure.
    auto abandon = [&out, &destPath](Result result, const QString &msg) -> Outcome {
        out.close();
        QFile::remove(destPath);
        return {result, msg};
    };

    const size_t inCap = ZSTD_DStreamInSize();
    const size_t outCap = ZSTD_DStreamOutSize();
    std::vector<char> inBuf(inCap);
    std::vector<char> outBuf(outCap);

    qint64 written = 0;
    bool frameComplete = false;

    while (true) {
        // One check per input block is enough to feel immediate: a block is
        // ~128 KB, which libzstd chews through in single-digit milliseconds.
        if (shouldCancel && shouldCancel())
            return abandon(Result::Cancelled, QStringLiteral("Installation cancelled"));

        const qint64 read = in.read(inBuf.data(), static_cast<qint64>(inCap));
        if (read < 0)
            return abandon(Result::LocalFailure,
                           QStringLiteral("Could not read the downloaded archive"));
        if (read == 0)
            break;

        ZSTD_inBuffer input{inBuf.data(), static_cast<size_t>(read), 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{outBuf.data(), outCap, 0};
            const size_t ret = ZSTD_decompressStream(stream, &output, &input);
            if (ZSTD_isError(ret)) {
                const QString reason = QString::fromLatin1(ZSTD_getErrorName(ret));
                // libzstd sizes its decode window and internal buffers from the
                // first frame header, so an allocation failure surfaces here
                // rather than at ZSTD_createDStream(). On a 1 GB board running
                // valhalla alongside the dashboard that is a real outcome, and
                // it says nothing about the input: the .zst cost a couple of
                // hundred megabytes of cellular data and its sha256 already
                // matched, so it must survive to the next attempt.
                const ZSTD_ErrorCode code = ZSTD_getErrorCode(ret);
                if (code == ZSTD_error_memory_allocation
                    || code == ZSTD_error_workSpace_tooSmall) {
                    return abandon(Result::LocalFailure,
                                   QStringLiteral("Not enough memory to unpack map data: ")
                                       + reason);
                }
                return abandon(Result::SourceCorrupt,
                               QStringLiteral("Map data is corrupt: ") + reason);
            }
            if (output.pos > 0) {
                const qint64 w = out.write(outBuf.data(), static_cast<qint64>(output.pos));
                if (w != static_cast<qint64>(output.pos))
                    return abandon(Result::LocalFailure,
                                   QStringLiteral("Could not write map data (disk full?)"));
                written += w;
                if (progress)
                    progress(written);
            }
            // A zero return means a frame boundary was reached cleanly. Any
            // other value means the decoder still wants input.
            frameComplete = (ret == 0);
        }
    }

    // Both of these mean the compressed file is not the archive it claims to
    // be, even though its sha256 matched what was published. Handing that back
    // as SourceCorrupt is what breaks the retry loop: the caller drops the
    // file, and the next attempt fetches it again instead of re-failing on the
    // same bytes forever.
    if (!frameComplete)
        return abandon(Result::SourceCorrupt, QStringLiteral("Map data is incomplete"));

    if (expectedSize > 0 && written != expectedSize) {
        return abandon(Result::SourceCorrupt,
                       QStringLiteral("Map data is the wrong size (%1 of %2 bytes)")
                           .arg(written)
                           .arg(expectedSize));
    }

    if (!out.flush())
        return abandon(Result::LocalFailure,
                       QStringLiteral("Could not write map data (disk full?)"));
    if (::fsync(out.handle()) != 0)
        return abandon(Result::LocalFailure,
                       QStringLiteral("Could not flush map data to disk"));
    out.close();
    return {Result::Ok, {}};
}
