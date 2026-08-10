#pragma once

#include <QString>
#include <functional>

// Streaming file-to-file zstd decompression.
//
// The frame's content checksum is validated by libzstd as it decodes, and the
// output length is checked against the expected size, so callers do not need a
// second hash pass over the decompressed file. That matters here: sha256 runs
// at roughly 14 MB/s on the i.MX6, so hashing a 725 MB tar would add close to a
// minute to every install.
class ZstdDecompressor
{
public:
    enum class Result {
        Ok,
        // The compressed input is unusable: libzstd rejected the stream, the
        // frame ended early, or it decoded to a length the manifest disagrees
        // with. Retrying the same bytes fails the same way, so the caller
        // should throw them away.
        SourceCorrupt,
        // Something on this machine went wrong (could not open a file, a short
        // write, fsync failed, libzstd could not allocate its decode buffers).
        // Says nothing about the input, which is worth keeping if it was
        // expensive to fetch.
        LocalFailure,
        // shouldCancel() asked us to stop.
        Cancelled,
    };

    struct Outcome {
        Result result = Result::LocalFailure;
        // Suitable for the UI. Set on every result except Ok.
        QString error;

        bool ok() const { return result == Result::Ok; }
    };

    // Decompresses srcPath onto destPath, truncating destPath first.
    //
    // expectedSize is the decompressed length the manifest promises; pass 0 to
    // skip the length check. progress, when set, receives the running
    // decompressed byte count. shouldCancel, when set, is polled between reads
    // and stops the decode as soon as it returns true.
    //
    // Both callbacks run on the calling thread, which is expected to be a
    // worker: a large region takes tens of seconds. srcPath is never touched;
    // destPath is removed on any outcome other than Ok, so a failed decode
    // never leaves a half-written file behind.
    static Outcome decompressFile(const QString &srcPath,
                                  const QString &destPath,
                                  qint64 expectedSize,
                                  const std::function<void(qint64)> &progress,
                                  const std::function<bool()> &shouldCancel);
};
