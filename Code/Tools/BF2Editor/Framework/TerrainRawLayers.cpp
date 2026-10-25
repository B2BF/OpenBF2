// Code/Tools/BF2Editor/Framework/TerrainRawLayers.cpp - see TerrainRawLayers.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/TerrainRawLayers.h"

#include <cstring>

namespace bf2editor
{

namespace
{

class Reader
{
public:
    explicit Reader(const std::string& bytes) : m_bytes(bytes), m_at(0) {}
    bool raw(void* out, size_t n)
    {
        if (m_at + n > m_bytes.size())
            return false;
        std::memcpy(out, m_bytes.data() + m_at, n);
        m_at += n;
        return true;
    }
    bool skip(size_t n)
    {
        if (m_at + n > m_bytes.size())
            return false;
        m_at += n;
        return true;
    }
    // TerrainRawReader::readLine: the bytes up to the delimiter (at most 0x1000), the delimiter consumed.
    bool line(std::string& out)
    {
        const size_t end = m_bytes.find('\n', m_at);
        if (end == std::string::npos || end - m_at > 0x1000)
            return false;
        out = m_bytes.substr(m_at, end - m_at);
        if (!out.empty() && out[out.size() - 1] == '\r')
            out.erase(out.size() - 1);
        m_at = end + 1;
        return true;
    }

private:
    const std::string& m_bytes;
    size_t             m_at;
};

} // namespace

bool TerrainRawLayers::read(const std::string& bytes, std::string& error)
{
    Reader r(bytes);
    unsigned short words[2] = { 0, 0 };
    if (!r.raw(words, 4))
    {
        error = "no header";
        return false;
    }
    version = words[0];
    hasVersion = words[1];
    // Two world scales (2 x 12), the four header words (16) and the subdivide byte, then the patch count and the two
    // map sizes - Terrain::load's reads; of them only the patch count is kept.
    std::string ignored;
    if (!r.skip(24 + 16 + 1) || !r.raw(&patchCount, 4) || !r.skip(8))
    {
        error = "the header ends early";
        return false;
    }
    for (int i = 0; i < 4; ++i)                 // the colormap, detail map, low-detail map and lightmap base names
    {
        if (!r.line(ignored))
        {
            error = "a base name line is missing";
            return false;
        }
    }
    unsigned count = 0;
    if (!r.raw(farSideTiling, 8) || !r.raw(&farTopTilingHi, 4))
    {
        error = "the far tiling ends early";
        return false;
    }
    farTopTilingLow = farTopTilingHi;
    if (hasVersion != 0 && version >= 0x17 && !r.raw(&farTopTilingLow, 4))
    {
        error = "the far tiling ends early";
        return false;
    }
    if (!r.raw(&farYOffset, 4) || !r.skip(36) || !r.raw(&count, 4))     // then the sun, GI and water colours
    {
        error = "the header ends before the detail textures";
        return false;
    }
    if (count > 64)
    {
        error = "an implausible detail texture count";
        return false;
    }
    layers.clear();
    for (unsigned n = 0; n < count; ++n)
    {
        TerrainRawLayer layer;
        unsigned char planeMap = 0, envMap = 0;
        if (!r.line(layer.name) || !r.raw(&planeMap, 1) || !r.raw(layer.sideTiling, 8) || !r.raw(&layer.topTiling, 4)
            || !r.raw(&layer.yOffset, 4) || !r.raw(&envMap, 1))
        {
            error = "a detail texture record ends early";
            return false;
        }
        layer.planeMap = planeMap;
        layer.envMap = envMap != 0;
        layers.push_back(layer);
    }
    return true;
}

} // namespace bf2editor
