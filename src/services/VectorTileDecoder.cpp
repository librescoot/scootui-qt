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

static Feature parseFeature(PbReader &r)
{
    Feature f;
    while (!r.atEnd()) {
        auto [field, wire] = r.readTag();
        switch (field) {
        case 3: // type (GeomType enum)
            f.type = static_cast<int>(r.readVarint());
            break;
        case 4: // geometry (packed uint32)
            f.geometry = r.readPackedUint32();
            break;
        default:
            r.skip(wire);
            break;
        }
    }
    return f;
}

static Layer parseLayer(PbReader &r)
{
    Layer layer;
    while (!r.atEnd()) {
        auto [field, wire] = r.readTag();
        switch (field) {
        case 1: // name
            layer.name = QString::fromUtf8(r.readBytes());
            break;
        case 2: { // feature
            PbReader sub = r.readSubmessage();
            layer.features.append(parseFeature(sub));
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

    // First integer: command (MoveTo=1, count=1)
    // uint32_t cmd = geometry[0];
    // int cmdId = cmd & 0x7;   // should be 1 (MoveTo)
    // int count = cmd >> 3;    // should be 1

    // Parameters are zigzag-encoded
    uint32_t rawX = geometry[1];
    uint32_t rawY = geometry[2];

    auto zigzag = [](uint32_t n) -> int32_t {
        return static_cast<int32_t>((n >> 1) ^ -(n & 1));
    };

    return QPointF(zigzag(rawX), zigzag(rawY));
}

} // namespace VectorTile
