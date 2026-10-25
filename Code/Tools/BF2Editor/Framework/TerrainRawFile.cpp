// Code/Tools/BF2Editor/Framework/TerrainRawFile.cpp - see TerrainRawFile.h. Editor-only code.
#include "Framework/EditorConfig.h"
#include "Framework/TerrainRawFile.h"

#include <cstdio>
#include <cstring>

namespace bf2editor
{

namespace
{

// The terrain's LOD records (Terrain::loadLodTablesAndBaseGrid): 85 of them, the first 21 read as 38 words
// (two per row, sixteen more in the second and third rows), the rest as 6 (two per row).
const unsigned kLinkedLodRecords = 21;
const unsigned kLinkedLodWords = 38;
const unsigned kPlainLodRecords = 64;
const unsigned kPlainLodWords = 6;
const unsigned kTerrainLodWords = kLinkedLodRecords * kLinkedLodWords + kPlainLodRecords * kPlainLodWords;
// A patch layer's LOD record (QuadPatchDetailLayer::load): two words per row, three rows.
const unsigned kLayerLodWords = 6;
// TerrainRawReader::readLine's buffer.
const size_t kMaxLine = 0x1000;
// A culler node in the file: eight neighbour words, two corners, four bytes.
const size_t kCullerNodeBytes = 16 + 24 + 4;

class Reader
{
public:
    explicit Reader(const std::string& bytes) : m_bytes(bytes), m_at(0) {}

    bool raw(void* out, size_t n)
    {
        if (n > remaining())
            return false;
        std::memcpy(out, m_bytes.data() + m_at, n);
        m_at += n;
        return true;
    }
    template <class T> bool value(T& out) { return raw(&out, sizeof(T)); }
    bool block(std::string& out, size_t n)
    {
        if (n > remaining())
            return false;
        out.assign(m_bytes, m_at, n);
        m_at += n;
        return true;
    }
    template <class T> bool array(std::vector<T>& out, size_t count)
    {
        if (count > remaining() / sizeof(T))
            return false;
        out.resize(count);
        return count == 0 || raw(&out[0], count * sizeof(T));
    }
    // The bytes up to the next '\n', which is consumed; nothing else is stripped.
    bool line(std::string& out)
    {
        const size_t end = m_bytes.find('\n', m_at);
        if (end == std::string::npos || end - m_at >= kMaxLine)
            return false;
        out.assign(m_bytes, m_at, end - m_at);
        m_at = end + 1;
        return true;
    }
    size_t remaining() const { return m_bytes.size() - m_at; }
    size_t at() const { return m_at; }

private:
    const std::string& m_bytes;
    size_t             m_at;
};

class Writer
{
public:
    void raw(const void* data, size_t n) { m_out.append(static_cast<const char*>(data), n); }
    template <class T> void value(const T& v) { raw(&v, sizeof(T)); }
    template <class T> void array(const std::vector<T>& v)
    {
        if (!v.empty())
            raw(&v[0], v.size() * sizeof(T));
    }
    void line(const std::string& s)
    {
        m_out += s;
        m_out += '\n';
    }
    std::string& out() { return m_out; }

private:
    std::string m_out;
};

// The vertices Terrain::generateSurroundingGrid makes over [x0..x1] x [z0..z1] at one step.
unsigned gridVertices(int x0, int z0, int x1, int z1, int step)
{
    return static_cast<unsigned>((z1 - z0) / step + 1) * static_cast<unsigned>((x1 - x0) / step + 1);
}

std::string at(const char* what, size_t offset)
{
    char text[160];
    std::snprintf(text, sizeof(text), "%s (at byte %u)", what, static_cast<unsigned>(offset));
    return text;
}

} // namespace

unsigned TerrainRawFile::ringVertexCount(int slot) const
{
    if (secondaryScale[0] == 0.0f)
        return 0;
    // Terrain::buildSurroundingGeometry over the primary heightmap's span (patchCount * patchSize cells): the
    // side grid in three strips, the corner grid in five blocks.
    const int span = static_cast<int>(static_cast<double>(patchCount * patchSize) * primaryScale[0] / secondaryScale[0]);
    const int quarter = span / 4;
    const int half = span / 2;
    if (span <= 0)
        return 0;
    if (slot % 2 == 0)
        return gridVertices(0, 0, quarter, quarter, 1) + gridVertices(quarter, 0, half, quarter, 2)
             + gridVertices(0, quarter, half, half, 2) + gridVertices(half, 0, span, half, 4)
             + gridVertices(0, half, span, span, 4);
    return gridVertices(0, 0, span, quarter, 1) + gridVertices(0, quarter, span, half, 2)
         + gridVertices(0, half, span, span, 4);
}

bool TerrainRawFile::parse(const std::string& bytes, std::string& error)
{
    *this = TerrainRawFile();
    Reader r(bytes);
    if (!r.value(version) || !r.value(hasVersion))
    {
        error = "the file has no header";
        return false;
    }
    if (version != 0x1A || hasVersion != 1)
    {
        char text[96];
        std::snprintf(text, sizeof(text), "version 0x%X (flag %u): only version 0x1A files are read", version, hasVersion);
        error = text;
        return false;
    }
    if (!r.raw(primaryScale, sizeof(primaryScale)) || !r.raw(secondaryScale, sizeof(secondaryScale)) || !r.value(word2C)
        || !r.value(maxHeight) || !r.value(minHeight) || !r.value(patchSize) || !r.value(subdividePatches)
        || !r.value(patchCount) || !r.value(detailmapSize) || !r.value(colormapSize))
    {
        error = at("the header ends early", r.at());
        return false;
    }
    if (patchSize == 0 || patchSize > 255 || patchCount == 0 || patchCount > 64)
    {
        error = "an implausible patch size or patch count";
        return false;
    }
    if (!r.line(colormapBaseName) || !r.line(detailmapBaseName) || !r.line(lowDetailmapBaseName)
        || !r.line(lightmapBaseName))
    {
        error = at("a base name is missing", r.at());
        return false;
    }
    unsigned detailCount = 0;
    if (!r.raw(farSideTiling, sizeof(farSideTiling)) || !r.value(farTopTilingHi) || !r.value(farTopTilingLow)
        || !r.value(farYOffset) || !r.raw(sunColor, sizeof(sunColor)) || !r.raw(giColor, sizeof(giColor))
        || !r.raw(waterColor, sizeof(waterColor)) || !r.value(detailCount))
    {
        error = at("the header ends before the detail textures", r.at());
        return false;
    }
    if (detailCount > 64)
    {
        error = "an implausible detail texture count";
        return false;
    }
    for (unsigned n = 0; n < detailCount; ++n)
    {
        TerrainRawDetailTexture texture;
        if (!r.line(texture.name) || !r.value(texture.planeMap) || !r.raw(texture.sideTiling, sizeof(texture.sideTiling))
            || !r.value(texture.topTiling) || !r.value(texture.yOffset) || !r.value(texture.envMap))
        {
            error = at("a detail texture record ends early", r.at());
            return false;
        }
        detailTextures.push_back(texture);
    }

    unsigned lodIndexCount = 0;
    if (!r.array(lodWords, kTerrainLodWords) || !r.value(lodIndexCount) || !r.array(lodIndices, lodIndexCount)
        || !r.value(compactVertices))
    {
        error = at("the LOD tables end early", r.at());
        return false;
    }

    const size_t vertexBytes = static_cast<size_t>(compactVertices ? 20 : 24) * (patchSize + 1) * (patchSize + 1);
    patches.resize(static_cast<size_t>(patchCount) * patchCount);
    for (size_t p = 0; p < patches.size(); ++p)
    {
        TerrainRawPatch& patch = patches[p];
        int marker = 0;
        if (!r.value(patch.gridX) || !r.value(patch.gridZ) || !r.value(patch.maxHeight) || !r.value(patch.minHeight)
            || !r.raw(patch.sphereCentre, sizeof(patch.sphereCentre)) || !r.value(patch.sphereRadius)
            || !r.block(patch.vertices, vertexBytes) || !r.value(marker))
        {
            error = at("a patch ends early", r.at());
            return false;
        }
        while (marker != -1)
        {
            TerrainRawPatchLayer layer;
            layer.marker = marker;
            unsigned indexCount = 0, tableCount = 0;
            if (!r.value(layer.detailIndex) || !r.raw(layer.vector24, sizeof(layer.vector24)) || !r.value(layer.int30)
                || !r.value(indexCount) || !r.value(layer.word0C) || !r.raw(layer.bytes10, sizeof(layer.bytes10))
                || !r.value(tableCount) || tableCount > r.remaining() / (4 * kLayerLodWords)
                || !r.array(layer.lodWords, static_cast<size_t>(tableCount) * kLayerLodWords)
                || !r.array(layer.indices, indexCount) || !r.value(marker))
            {
                error = at("a patch's detail layer ends early", r.at());
                return false;
            }
            patch.layers.push_back(layer);
        }
    }

    int slot = 0;
    if (!r.value(slot))
    {
        error = at("the surrounding ring is missing", r.at());
        return false;
    }
    while (slot != -1)
    {
        if (slot < 0 || slot > 7 || ring.size() >= 8)
        {
            error = at("an implausible ring slot", r.at());
            return false;
        }
        TerrainRawRingPatch sector;
        sector.slot = slot;
        const size_t count = ringVertexCount(slot);
        if (!r.value(sector.maxY) || !r.value(sector.minY) || !r.raw(sector.centre, sizeof(sector.centre))
            || !r.value(sector.radius) || !r.block(sector.heights, 4 * count) || !r.block(sector.normals, 12 * count)
            || !r.value(slot))
        {
            error = at("a ring sector ends early", r.at());
            return false;
        }
        ring.push_back(sector);
    }

    unsigned nodeCount = 0;
    if (!r.value(nodeCount) || nodeCount > r.remaining() / kCullerNodeBytes)
    {
        error = at("the culler ends early", r.at());
        return false;
    }
    culler.resize(nodeCount);
    for (size_t n = 0; n < culler.size(); ++n)
    {
        TerrainRawCullerNode& node = culler[n];
        if (!r.raw(node.neighbour, sizeof(node.neighbour)) || !r.raw(node.minimum, sizeof(node.minimum))
            || !r.raw(node.maximum, sizeof(node.maximum)) || !r.value(node.byteEC) || !r.value(node.hasPatchList)
            || !r.value(node.byteEE) || !r.value(node.patchIndex))
        {
            error = at("a culler node ends early", r.at());
            return false;
        }
    }
    if (r.remaining())
    {
        char text[96];
        std::snprintf(text, sizeof(text), "%u bytes follow the culler", static_cast<unsigned>(r.remaining()));
        error = text;
        return false;
    }
    return true;
}

std::string TerrainRawFile::render() const
{
    Writer w;
    w.value(version);
    w.value(hasVersion);
    w.raw(primaryScale, sizeof(primaryScale));
    w.raw(secondaryScale, sizeof(secondaryScale));
    w.value(word2C);
    w.value(maxHeight);
    w.value(minHeight);
    w.value(patchSize);
    w.value(subdividePatches);
    w.value(patchCount);
    w.value(detailmapSize);
    w.value(colormapSize);
    w.line(colormapBaseName);
    w.line(detailmapBaseName);
    w.line(lowDetailmapBaseName);
    w.line(lightmapBaseName);
    w.raw(farSideTiling, sizeof(farSideTiling));
    w.value(farTopTilingHi);
    w.value(farTopTilingLow);
    w.value(farYOffset);
    w.raw(sunColor, sizeof(sunColor));
    w.raw(giColor, sizeof(giColor));
    w.raw(waterColor, sizeof(waterColor));
    w.value(static_cast<unsigned>(detailTextures.size()));
    for (size_t n = 0; n < detailTextures.size(); ++n)
    {
        const TerrainRawDetailTexture& texture = detailTextures[n];
        w.line(texture.name);
        w.value(texture.planeMap);
        w.raw(texture.sideTiling, sizeof(texture.sideTiling));
        w.value(texture.topTiling);
        w.value(texture.yOffset);
        w.value(texture.envMap);
    }
    w.array(lodWords);
    w.value(static_cast<unsigned>(lodIndices.size()));
    w.array(lodIndices);
    w.value(compactVertices);
    for (size_t p = 0; p < patches.size(); ++p)
    {
        const TerrainRawPatch& patch = patches[p];
        w.value(patch.gridX);
        w.value(patch.gridZ);
        w.value(patch.maxHeight);
        w.value(patch.minHeight);
        w.raw(patch.sphereCentre, sizeof(patch.sphereCentre));
        w.value(patch.sphereRadius);
        w.raw(patch.vertices.data(), patch.vertices.size());
        for (size_t l = 0; l < patch.layers.size(); ++l)
        {
            const TerrainRawPatchLayer& layer = patch.layers[l];
            w.value(layer.marker);
            w.value(layer.detailIndex);
            w.raw(layer.vector24, sizeof(layer.vector24));
            w.value(layer.int30);
            w.value(static_cast<unsigned>(layer.indices.size()));
            w.value(layer.word0C);
            w.raw(layer.bytes10, sizeof(layer.bytes10));
            w.value(static_cast<unsigned>(layer.lodWords.size() / kLayerLodWords));
            w.array(layer.lodWords);
            w.array(layer.indices);
        }
        w.value(-1);
    }
    for (size_t s = 0; s < ring.size(); ++s)
    {
        const TerrainRawRingPatch& sector = ring[s];
        w.value(sector.slot);
        w.value(sector.maxY);
        w.value(sector.minY);
        w.raw(sector.centre, sizeof(sector.centre));
        w.value(sector.radius);
        w.raw(sector.heights.data(), sector.heights.size());
        w.raw(sector.normals.data(), sector.normals.size());
    }
    w.value(-1);
    w.value(static_cast<unsigned>(culler.size()));
    for (size_t n = 0; n < culler.size(); ++n)
    {
        const TerrainRawCullerNode& node = culler[n];
        w.raw(node.neighbour, sizeof(node.neighbour));
        w.raw(node.minimum, sizeof(node.minimum));
        w.raw(node.maximum, sizeof(node.maximum));
        w.value(node.byteEC);
        w.value(node.hasPatchList);
        w.value(node.byteEE);
        w.value(node.patchIndex);
    }
    return w.out();
}

} // namespace bf2editor
