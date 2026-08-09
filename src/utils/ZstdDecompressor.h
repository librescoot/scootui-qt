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
    // Decompresses srcPath onto destPath, truncating destPath first.
    //
    // expectedSize is the decompressed length the manifest promises; pass 0 to
    // skip the length check. progress, when set, receives the running
    // decompressed byte count. On failure returns false and, when errorString
    // is non-null, sets it to a message suitable for the UI.
    static bool decompressFile(const QString &srcPath,
                               const QString &destPath,
                               qint64 expectedSize,
                               const std::function<void(qint64)> &progress,
                               QString *errorString);
};
