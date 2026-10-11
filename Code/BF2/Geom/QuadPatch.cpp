
// Terrain patch geometry, detail layers and the quadtree culler that indexes them.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "QuadPatch.h"
#include "Terrain.h"
#include "XpLightManager.h"
#include "../Math/Frustum.h"
#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"
#include "../Physics/Heightmap.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"

#include <cfloat>
#include <cmath>
#include <new>
#include <cstdio>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage, unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                          // --ref, sub_10014510, free
// BufferManager.cpp - sub_10011A50 (em_terrain): drop an index buffer from the tracking sets,
// releasing the set's reference. The vertex twin (0x100119A0) is bufferManagerRemoveVertexBuffer.
void  bufferManagerRemoveIndexBuffer(void* wrapper);
// QueryManager.cpp - the Query holders' inline `if (!--q->refCount) { sub_10003FA0(q); free(q); }`.
void  queryReleaseRef(void* query);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);
} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                               // ModuleGeom.cpp

namespace {

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

// The eight-corner plane test both walks run: how many corners of the box lie in front of `plane`
// (normal xyz, d), corners in the shipped order.
inline int cornersInFront(const TerrainAabb& b, const float* p)
{
    const double nx = p[0], ny = p[1], nz = p[2], d = p[3];
    int n = 0;
    n += (b.max.y * ny + b.min.x * nx + b.max.z * nz + d > 0.0);
    n += (b.max.y * ny + b.min.x * nx + b.min.z * nz + d > 0.0);
    n += (b.min.y * ny + b.min.x * nx + b.max.z * nz + d > 0.0);
    n += (b.min.y * ny + b.min.x * nx + b.min.z * nz + d > 0.0);
    n += (b.max.y * ny + b.max.x * nx + b.max.z * nz + d > 0.0);
    n += (b.max.y * ny + b.max.x * nx + b.min.z * nz + d > 0.0);
    n += (b.min.y * ny + b.max.x * nx + b.max.z * nz + d > 0.0);
    n += (b.min.y * ny + b.max.x * nx + b.min.z * nz + d > 0.0);
    return n;
}

// The LOD-table lookup every patch draw makes: the 400-byte record's rows of 16 masks, starts in
// row `lod`, counts in row `lod + 3`.
inline unsigned int lodStart(const TerrainCullerNode* node)
{
    return reinterpret_cast<const unsigned int*>(node->lodTable)[node->crackMask + 16 * node->lod];
}
inline unsigned int lodCount(const TerrainCullerNode* node)
{
    return reinterpret_cast<const unsigned int*>(node->lodTable)[node->crackMask + 16 * (node->lod + 3)];
}

// The (w - 1) / w, 1 / (2w) pair of a texture's width that colorlighttex takes (sub_10014940's
// unsigned width, computed in double as shipped).
inline void colorLightTexOf(void* texture, float out[2])
{
    const double w = static_cast<double>(static_cast<dx9rend::EngineTexture*>(texture)->width);
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[0] = static_cast<float>((w - 1.0) / w);
    out[1] = static_cast<float>(1.0 / (w + w));
}

inline bool lod0(const TerrainCullerNode* n) { return n->visible && n->lod == 0; }
inline bool lod1(const TerrainCullerNode* n) { return n->visible && n->lod == 1; }
inline bool lod0n(const TerrainCullerNode* n) { return n && n->visible && n->lod == 0; }
inline bool lod1n(const TerrainCullerNode* n) { return n && n->visible && n->lod == 1; }

// sub_10074AB0 - the image's printf-into-a-std::string helper: vsnprintf into a 0xFFF-byte
// buffer, or the "WARNING: ..." text when it does not fit (StaticMesh.cpp carries the same body).
void formatString(vc71::string& out, const char* format, int a, int b, int c)
{
    char buffer[0x1000];
    const int written = std::snprintf(buffer, 0xFFF, format, a, b, c);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

inline unsigned int layerCount(const vc71::vector_layout& v)
{
    return vc71::vectorSizeIn(v, 4u);
}

} // anonymous namespace


// QuadPatchDetailLayer

QuadPatchDetailLayer::QuadPatchDetailLayer(Terrain* owner)
{
    indexBuffer = 0;
    legacyIndices = 0;
    indexCount = 0;
    fileWord0C = 0;
    *reinterpret_cast<unsigned int*>(fileBytes10) = 4u;
    selectorX = 0.0f;
    selectorY = 0.0f;
    selectorZ = 0.0f;
    detailIndex = 0;
    fileVec24.x = 0.0f; fileVec24.y = 0.0f; fileVec24.z = 0.0f;
    fileInt30 = 0;
    terrain = owner;
    lodTables = 0;
}

// returns the layer's detail texture index. The low-quality-B arm parses the fields
// and seeks past the tables and indices without keeping any of them.
int QuadPatchDetailLayer::load(TerrainRawReader& reader)
{
    Terrain* const t = terrain;
    if (t->m_lowQualityB)
    {
        reader.readU32(&detailIndex);
        reader.readVec3(&fileVec24);
        reader.readU32(&fileInt30);
        reader.readU32(&indexCount);
        reader.readU32(&fileWord0C);
        reader.read(fileBytes10, 4);
        int tableCount = 0;
        reader.readU32(&tableCount);
        const int position = reader.tell();
        reader.seek(0, position + 2 * (static_cast<int>(indexCount) + 12 * tableCount));
        return detailIndex;
    }

    reader.readU32(&detailIndex);
    const int selector = detailIndex % 3;
    selectorZ = (selector == 0) ? 1.0f : 0.0f;
    selectorX = (selector == 2) ? 1.0f : 0.0f;
    selectorY = (selector == 1) ? 1.0f : 0.0f;
    reader.readVec3(&fileVec24);
    reader.readU32(&fileInt30);
    reader.readU32(&indexCount);
    reader.readU32(&fileWord0C);
    reader.read(fileBytes10, 4);

    if (t->m_rawHasVersion && t->m_rawVersion >= 0x18u)
    {
        // N records of 400 bytes; record i links to children 1+4i..4+4i while they exist, and
        // reads its three 64-byte rows as (row, row+192) u32 pairs.
        int tableCount = 0;
        reader.readU32(&tableCount);
        lodTables = static_cast<unsigned char*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(400 * tableCount)));
        int offset = 0;
        int childOffset = 400;
        int firstChild = 1;
        for (int record = 0; record < tableCount; ++record)
        {
            unsigned char* const table = lodTables + offset;
            unsigned char** const children = reinterpret_cast<unsigned char**>(table + 384);
            if (firstChild >= tableCount)
            {
                children[3] = 0; children[2] = 0; children[1] = 0; children[0] = 0;
            }
            else
            {
                children[0] = lodTables + childOffset;
                children[1] = lodTables + childOffset + 400;
                children[2] = lodTables + childOffset + 800;
                children[3] = lodTables + childOffset + 1200;
            }
            unsigned char* row = table;
            for (int r = 0; r < 3; ++r)
            {
                reader.readU32(row);
                reader.readU32(row + 192);
                row += 64;
            }
            offset += 400;
            childOffset += 1600;
            firstChild += 4;
        }
        indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(indexCount, 8u, 101u);
        void* const bits = dx9rend::indexBufferLock(indexBuffer, 0, 0, 0);
        if (bits)
            reader.read(bits, 2u * indexCount);
        dx9rend::indexBufferUnlock(indexBuffer);
        return detailIndex;
    }

    // The pre-0x18 arm: a host copy of the indices, one zeroed 400-byte record seeded with the
    // count, the legacy table builder, then the upload. 1.50 content never comes here.
    legacyIndices = ::renddx9abi::engineAlloc(2u * indexCount);
    reader.read(legacyIndices, 2u * indexCount);
    void* const storage = ::renddx9abi::engineAlloc(0x190);
    if (storage)
        ::memset(storage, 0, 0x190u);
    lodTables = static_cast<unsigned char*>(storage);
    ::memset(lodTables, 0, 0x190u);                                               // a second, unguarded clear, as shipped
    *reinterpret_cast<unsigned int*>(lodTables + 192) = indexCount;
    RD9_FRONTIER_AT("dice::hfe::geom::QuadPatchDetailLayer::load (sub_101BB490 - the pre-0x18 LOD "
                    "table builder)", this, 0x00);
    indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(indexCount, 8u, 101u);
    void* const bits = dx9rend::indexBufferLock(indexBuffer, 0, 0, 0);
    if (bits)
        ::memcpy(bits, legacyIndices, 2u * indexCount);
    dx9rend::indexBufferUnlock(indexBuffer);
    ::renddx9abi::engineFree(legacyIndices);
    return detailIndex;
}

// 0x101BB750, read in full. The layer's index buffer loses the layer's
// reference and then its BufferManager tracking-set entry - sub_10011A50 on dword_1023B978, which
// releases the set's own reference - so, unlike the Terrain's shared buffers, it is gone when this
// returns. Then the LOD records: the flat array (array free) on 0x18+ content, else the pre-0x18
// record tree. Both version words are compared unsigned (`cmp word ..., 1 / jb`, `..., 18h / jb`).
// The removal is handed the member again, not a copy taken before the release: the same value,
//   compared by value only, even had the release freed it (it cannot while the set holds a ref).
QuadPatchDetailLayer::~QuadPatchDetailLayer()
{
    if (indexBuffer)
    {
        dx9rend::indexBufferRelease(indexBuffer);                                 // --ref, sub_10014300 + free at zero
        dx9rend::bufferManagerRemoveIndexBuffer(indexBuffer);                     // sub_10011A50 (re-read, 0x101BB774)
    }
    Terrain* const t = terrain;
    if (t->m_rawHasVersion && t->m_rawVersion >= 0x18u)
        ::renddx9abi::engineFree(lodTables);
    else
        freeLodTableTree(lodTables);
}

// sub_101BB6B0 - the pre-0x18 layer records are separate 400-byte blocks (built by sub_101BB490, a
// [FRONTIER] of load()) linked through +0x180..+0x18C: the four children's subtrees when the first
// link is set (only it is tested; each recursion tests its own argument), then the record.
void QuadPatchDetailLayer::freeLodTableTree(unsigned char* table)
{
    if (!table)
        return;
    unsigned char* const* const children = reinterpret_cast<unsigned char* const*>(table + 0x180);
    if (children[0])
    {
        freeLodTableTree(children[0]);
        freeLodTableTree(children[1]);
        freeLodTableTree(children[2]);
        freeLodTableTree(children[3]);
    }
    ::renddx9abi::engineFree(table);
}


// QuadPatch

QuadPatch::QuadPatch(Terrain* owner)
{
    terrain = owner;
    detailLayers.first = 0; detailLayers.last = 0; detailLayers.end = 0;
    center.x = 0.0f; center.y = 0.0f; center.z = 0.0f;
    vertexBuffer24 = 0;
    unresolved28 = 0;
    legacyVertexTable = 0;
    lastDrawnFrame = 0; unresolved3C = 0;
    lowDetailmap = 0;
    colormap = 0;
    lightmap = 0;
    heightVertexBuffer = 0;
    normalVertexBuffer = 0;
    vertexBuffer = 0;
    gridX = 0; gridZ = 0; worldX = 0; worldZ = 0; scaledX = 0; scaledZ = 0;
    worldXf = 0.0f; zero8C = 0.0f; worldZf = 0.0f; zero94 = 0.0f;
    maxHeight = 0.0f; minHeight = 0.0f;                                           // integer 0 stores (0x1018A150..)
    sphereCentre.x = 0.0f; sphereCentre.y = 0.0f; sphereCentre.z = 0.0f;
    sphereRadius = 0.0f;
    for (int i = 0; i < 4; ++i) unresolvedB4[i] = 0;
    for (int i = 0; i < 14; ++i) lightRecord[i] = 0.0f;                          // [14] stays untouched, as shipped
    lightRecord[15] = 1.0f;
    lightRecord[16] = 1.0f;
    hasNoDetailLayers = 0;
    unresolved110 = 0;
    unresolved114 = -1;
    detailMap[0] = 0; detailMap[1] = 0;
}

namespace {

// dword_1026743C - the RaQuadPatchAdapter server (0x3CC bytes, Geom\RaQuadPatchAdapter.cpp). Its
// only creators (sub_101BBE20 / sub_101BBEB0, and the uncalled sub_101BBDD0) are reached from
// QuadPatch::drawWater's Ra submit, which stops before building it (see drawWater below) - so on the
// rebuilt client nothing ever sets this, and it is kept only so the teardown below has the
// shipped shape (the server is on RW-150's open list with the rest of the Ra water submit).
void* g_raQuadPatchAdapterServer = 0;

// sub_101BBD70, called by every QuadPatch destructor (0x1018A36D): destroy the server if there is
// one - the first patch destroyed finds it, the others find null - and clear the global either way.
// The non-null arm is read, not written: the server object is not modelled (its embedded adapter's
// lighting block at +0x278 goes through sub_10044120 (raLightingParametersDestroy), its object
// block at +0x3C through sub_1004D900, the pool through sub_101BC8B0, then the +0x04 block and the
// server are freed).
void raQuadPatchAdapterServerDestroy()
{
    if (g_raQuadPatchAdapterServer)
    {
        RD9_FRONTIER_AT("dice::hfe::geom::RaQuadPatchAdapter server teardown (sub_101BBD70's non-null "
                        "arm: sub_10044120 on +0x278, sub_1004D900 on +0x3C, sub_101BC8B0, the frees) - "
                        "the server is never built on this client", g_raQuadPatchAdapterServer, 0x00);
    }
    g_raQuadPatchAdapterServer = 0;
}

} // anonymous namespace

// Releases detail layers, textures, vertex buffers, adapter state, then the layer vector.
// The legacy vertex table was freed during load and must not be freed again.
QuadPatch::~QuadPatch()
{
    for (QuadPatchDetailLayer** it = reinterpret_cast<QuadPatchDetailLayer**>(detailLayers.first);
         it != reinterpret_cast<QuadPatchDetailLayer**>(detailLayers.last); ++it)
    {
        QuadPatchDetailLayer* const layer = *it;
        if (layer)
        {
            layer->~QuadPatchDetailLayer();
            ::renddx9abi::engineFree(layer);
        }
    }

    terrainReleaseTexture(colormap);                                              // +0x4C
    terrainReleaseTexture(lightmap);                                              // +0x50
    for (int k = 0; k < 2; ++k)
        terrainReleaseTexture(detailMap[k]);                                      // +0x40, +0x44
    terrainReleaseTexture(lowDetailmap);                                          // +0x48

    if (heightVertexBuffer) dx9rend::vertexBufferRelease(heightVertexBuffer);     // +0x54
    if (vertexBuffer24)     dx9rend::vertexBufferRelease(vertexBuffer24);         // +0x24
    if (normalVertexBuffer) dx9rend::vertexBufferRelease(normalVertexBuffer);     // +0x5C
    if (vertexBuffer)   dx9rend::vertexBufferRelease(vertexBuffer);               // +0x60

    raQuadPatchAdapterServerDestroy();

    if (detailLayers.first)                                                       // ~vector: _Tidy
        ::renddx9abi::engineFree(detailLayers.first);
    detailLayers.first = 0;
    detailLayers.last  = 0;
    detailLayers.end   = 0;
}

// the patch's file record: six header fields, the placement, the vertex buffer, the
// detail layers, the per-patch textures.
void QuadPatch::load(TerrainRawReader& reader)
{
    reader.readU32(&gridX);
    reader.readU32(&gridZ);
    reader.readU32(&maxHeight);
    reader.readU32(&minHeight);
    reader.readVec3(&sphereCentre);
    reader.readU32(&sphereRadius);

    Terrain* const t = terrain;
    const int size = t->m_patchSize;
    worldX = gridX * size;
    worldZ = gridZ * size;
    const float scaleX = t->m_primaryWorldScale.x;
    const float scaleZ = t->m_primaryWorldScale.z;
    scaledX = static_cast<int>(static_cast<double>(worldX) * scaleX);
    scaledZ = static_cast<int>(static_cast<double>(worldZ) * scaleZ);
    zero8C = 0.0f;
    zero94 = 0.0f;
    worldXf = static_cast<float>(static_cast<double>(worldX) * scaleX);
    worldZf = static_cast<float>(static_cast<double>(worldZ) * scaleZ);
    scaledXEnd = static_cast<int>(static_cast<double>(size) * scaleX) + scaledX;
    scaledZEnd = static_cast<int>(static_cast<double>(size) * scaleZ) + scaledZ;

    // 0x1018A6ED: `lea ecx, [esi+88h]; call sub_100E11B0` - the centre starts as the world origin
    // (worldXf, 0, worldZf) and moves half a patch in x and z.
    const Vec3* const origin = terrainOriginScratch(*reinterpret_cast<const Vec3*>(&worldXf));
    center.y = origin->y;
    center.x = origin->x;
    center.z = origin->z;
    center.x = static_cast<float>(static_cast<double>(size) * scaleX * 0.5 + center.x);
    center.z = static_cast<float>(static_cast<double>(size) * scaleZ * 0.5 + center.z);

    const int vertexCount = (size + 1) * (size + 1);
    if (t->m_rawHasVersion && t->m_rawVersion >= 0x18u)
    {
        if (t->m_compactVertices)
        {
            vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
                static_cast<unsigned int>(vertexCount), 20u, 8u, "QuadPatch_SingleCompact");
            void* const bits = dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0);
            if (bits)
                reader.read(bits, static_cast<unsigned int>(20 * vertexCount));
        }
        else
        {
            vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
                static_cast<unsigned int>(vertexCount), 24u, 8u, "QuadPatch_SingleCompact");
            unsigned char* bits = static_cast<unsigned char*>(dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0));
            if (bits)
                reader.read(bits, static_cast<unsigned int>(24 * vertexCount));
            if (!t->m_rawHasVersion || t->m_rawVersion < 0x1Au)
            {
                // 0x18/0x19 content: clear the border-code byte and the pad of every vertex.
                if (bits)
                {
                    for (int i = 0; i < vertexCount; ++i)
                    {
                        bits[3] = 0;
                        bits[0] = 0;
                        bits += 24;
                    }
                }
            }
        }
    }
    else
    {
        // The pre-0x18 arm: the packed xz table pairs with the terrain's legacy base grid, two
        // discarded vec4s, then per-vertex 16-byte records and a normal table packed by
        // sub_10049E80. 1.50 content never comes here.
        vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
            static_cast<unsigned int>(vertexCount), 24u, 8u, "QuadPatch_SingleCompact");
        unsigned char* const bits = static_cast<unsigned char*>(dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0));
        legacyVertexTable = ::renddx9abi::engineAlloc(static_cast<unsigned int>(4 * vertexCount));
        reader.read(legacyVertexTable, static_cast<unsigned int>(4 * vertexCount));
        if (bits)
        {
            unsigned int* v = reinterpret_cast<unsigned int*>(bits);
            for (int i = 0; i < vertexCount; ++i)
            {
                v[0] = static_cast<const unsigned int*>(t->m_legacyBaseGridTable)[i];
                v[1] = static_cast<const unsigned int*>(legacyVertexTable)[i];
                v += 6;
            }
        }
        float scratch[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        reader.readVec4(scratch);
        reader.readVec4(scratch);
        const int count2 = (t->m_patchSize + 1) * (t->m_patchSize + 1);
        if (bits)
        {
            float* v = reinterpret_cast<float*>(bits + 16);
            for (int i = 0; i < count2; ++i)
            {
                float in[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                reader.read(in, 16);
                v[0]  = in[2];
                v[-2] = in[0];
                v[-1] = in[1];
                v += 6;
            }
        }
        const int count3 = (t->m_patchSize + 1) * (t->m_patchSize + 1);
        void* const storage = ::renddx9abi::engineAlloc(static_cast<unsigned int>(12 * count3));
        float* normals = 0;
        if (storage)
        {
            normals = static_cast<float*>(storage);
            for (int i = 0; i < count3; ++i)
            {
                normals[3 * i] = 0.0f; normals[3 * i + 1] = 0.0f; normals[3 * i + 2] = 0.0f;
            }
        }
        const int side = t->m_patchSize + 1;
        reader.read(normals, static_cast<unsigned int>(12 * side * side));
        if (bits)
        {
            RD9_FRONTIER("dice::hfe::geom::QuadPatch::load (sub_10049E80 - the pre-0x18 normal packer "
                         "into vertex +20)", 0x00);
        }
        ::renddx9abi::engineFree(normals);
    }
    dx9rend::vertexBufferUnlock(vertexBuffer);
    fileEndOffset = reader.tell();
    loadDetailLayers(reader);

    // "%02ix%02i" of the grid position re-based on the centre patch, and the patch index.
    const int half = t->m_patchCount / 2;
    const int nameX = half + gridX;
    const int nameZ = half + gridZ;
    vc71::string name;
    formatString(name, "%02ix%02i", nameX, nameZ, 0);
    patchIndex = nameX + nameZ * t->m_patchCount;

    if (!hasNoDetailLayers)
    {
        const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(2);
        {
            vc71::string colormapName(t->m_colormapBaseName.c_str());
            colormapName += name;
            colormap = dx9rend::textureManagerGetTexture(colormapName);
        }
        if (!t->m_lowQuality)
            loadDetailMaps();
        dx9rend::textureManagerSwapDefaultMipClass(4);                            // the answer is discarded
        vc71::string lowName(t->m_lowDetailmapBaseName.c_str());
        lowName += name;
        {
            vc71::string dds(lowName.c_str());
            dds += ".dds";
            if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
                lowDetailmap = dx9rend::textureManagerGetTexture(lowName);
        }
        dx9rend::textureManagerSwapDefaultMipClass(savedClass);
    }
    const int savedClass7 = dx9rend::textureManagerSwapDefaultMipClass(7);
    {
        vc71::string lightmapName(t->m_lightmapBaseName.c_str());
        lightmapName += name;
        lightmap = dx9rend::textureManagerGetTexture(lightmapName);
    }
    dx9rend::textureManagerSwapDefaultMipClass(savedClass7);
}

// the detail layers: detailTextureCount + 1 null slots, then layer records until a
// -1 marker. The record's own detail index says which slot it fills; the low-quality arm parses
// each record on the stack and keeps nothing.
void QuadPatch::loadDetailLayers(TerrainRawReader& reader)
{
    Terrain* const t = terrain;
    for (int i = 0; i < t->m_detailTextureCount + 1; ++i)
    {
        QuadPatchDetailLayer* const null = 0;
        vc71::vectorPushBackIn(detailLayers, 4u, &null);
    }
    maxDetailLayerIndex = -1;
    int marker = 0;
    reader.readU32(&marker);
    while (marker != -1)
    {
        int detailIndex;
        if (t->m_lowQuality)
        {
            QuadPatchDetailLayer layer(t);
            detailIndex = layer.load(reader);
        }
        else
        {
            void* const storage = ::renddx9abi::engineAlloc(0x3C);
            QuadPatchDetailLayer* const layer = storage ? new (storage) QuadPatchDetailLayer(t) : 0;
            detailIndex = layer->load(reader);                                    // unguarded, as shipped
            reinterpret_cast<QuadPatchDetailLayer**>(detailLayers.first)[detailIndex] = layer;
        }
        if (detailIndex < t->m_detailTextureCount)
        {
            if (maxDetailLayerIndex >= detailIndex)
                detailIndex = maxDetailLayerIndex;
            maxDetailLayerIndex = detailIndex;
        }
        reader.readU32(&marker);
    }
    if (maxDetailLayerIndex == -1)
        hasNoDetailLayers = 1;
}

// the two packed detail maps "<detailmapBase>%02ix%02i_1" / "_2", under mip class 6,
// each only when a layer index reaches its three-channel block.
void QuadPatch::loadDetailMaps()
{
    const int savedClass = dx9rend::textureManagerSwapDefaultMipClass(6);
    int threshold = 0;
    for (int k = 0; k < 2; ++k)
    {
        Terrain* const t = terrain;
        const int half = t->m_patchCount / 2;
        vc71::string name;
        formatString(name, "%02ix%02i_%i", half + gridX, half + gridZ, k + 1);
        if (!detailMap[k] && threshold <= maxDetailLayerIndex)
        {
            vc71::string full(t->m_detailmapBaseName.c_str());
            full += name;
            detailMap[k] = dx9rend::textureManagerGetTexture(full);
        }
        threshold += 3;
    }
    dx9rend::textureManagerSwapDefaultMipClass(savedClass);
}


// QuadPatch - the three bodies at 0x10188660..0x10188C0F, They
// sit in the attested QuadPatch.cpp run (just before loadDetailMaps, 0x10188CA0), but only the
// dormant editable terrain calls them: TerrainEditable::update (0x1010C630) culls and keys its
// visible-patch multimap with the first two, QuadPatchEditable's VB rebuild (0x1019DF30) runs the
// third straight after creating the normal buffer. The loaded game path never reaches them.

namespace {

// "HeightmapCluster" through ClassManager +0x48 - the temporary std::string lives only for the call.
inline ::dice::hfe::world::IHeightmapCluster* heightmapCluster()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

// PE 0x100777A0 (135 B), read in full - the engine's in-place normalise: |v|^2 = (x*x + y*y) + z*z
// kept as a float; within FLT_EPSILON (flt_10208958) of 1 the vector is left alone; below
// FLT_EPSILON it is zeroed and -20001 (0xFFFFB1DF) answered; otherwise each component is scaled by
// 1 / sqrt(|v|^2) (flt_101DDDE4 = 1.0f). An unordered |v|^2 fails both tests and takes the scale.
// Every caller here drops the answer. (The same body is transcribed beside its other users:
// Overgrowth/OverGrowthCellRender.cpp, StaticMeshMaterial.cpp, the particle and nametag TUs.)
int normalizeInPlace(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
    return 0;
}

} // anonymous namespace

// 0x10188660, 29 B (`retn 4`) - Frustum::inside(centre, radius) (0x10079A60) on the patch's bounding
// sphere: `push [ecx+0ACh]; add ecx, 0A0h; push ecx; mov ecx, [ebp+8]`. The answer is the byte in al.
bool QuadPatch::isVisible(const Frustum* frustum)
{
    return frustum->inside(sphereCentre, sphereRadius);
}

// 0x10188BE0, 47 B (`retn 4`) - the squared distance from `position` to the patch centre (+0x18):
// the three differences stay on the x87 stack and the sum is dx*dx + dz*dz + dy*dy in that order,
// answered in st0 (TerrainEditable::update stores it as its multimap key, 0x1010C745). Float
// arithmetic throughout - RendDX9's x87 runs at 24-bit precision (the RW-240 rule).
float QuadPatch::getSquaredDistance(const Vec3& position)
{
    const float dx = position.x - center.x;
    const float dy = position.y - center.y;
    const float dz = position.z - center.z;
    return dx * dx + dz * dz + dy * dy;
}


// Builds per-vertex normals from the primary heightmap into the normal buffer.
// Preserves repeated heightmap queries and the double-weighted centre sample.
// The optional half-float arm packs 8-byte vertices into the 12-byte-stride allocation.

void QuadPatch::buildNormalVertexBuffer()
{
    void* const bits = dx9rend::vertexBufferLock(normalVertexBuffer, 0, 0, 0);
    if (!bits)
    {
        dx9rend::vertexBufferUnlock(normalVertexBuffer);
        return;
    }

    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapCluster();
    ::dice::hfe::world::IHeightmap* const heightmap = cluster->getPrimaryHeightmap();   // +0x40
    const int n = terrain->m_patchSize;
    const int step = static_cast<int>(cluster->getPrimaryScale().x);            // +0x54
    cluster->getPrimaryScale();                                                  // +0x54 again - answer unused

    const int side = n + 2;
    Vec3* samples = 0;
    {
        void* const storage = ::renddx9abi::engineAlloc(static_cast<unsigned int>(12 * (side * side)));
        if (storage)
        {
            samples = static_cast<Vec3*>(storage);
            for (int i = 0; i < side * side; ++i)
            {
                samples[i].x = 0.0f;
                samples[i].y = 0.0f;
                samples[i].z = 0.0f;
            }
        }
    }
    int unusedSize = 0;
    heightmap->getSize(unusedSize, unusedSize);                                  // +0x2C, one local twice

    for (int row = 0, zOffset = -step; row < side; ++row, zOffset += step)
    {
        Vec3* sample = samples + row * side;
        for (int col = 0, xOffset = -step; col < side; ++col, xOffset += step)
        {
            const int x = scaledX + xOffset;                                     // +0x78, re-read
            const int z = scaledZ + zOffset;                                     // +0x7C, re-read
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);
            const float h0 = cluster->getHeightInWorldCoords(fx, fz);           // +0x6C
            const float fx1 = static_cast<float>(x + step);
            const float h1 = cluster->getHeightInWorldCoords(fx1, fz);
            Vec3 alongX;
            alongX.x = fx1 - fx;
            alongX.y = h1 - h0;
            alongX.z = fz - fz;
            const float h0Again = cluster->getHeightInWorldCoords(fx, fz);      // asked again
            const float fz1 = static_cast<float>(z + step);
            const float h2 = cluster->getHeightInWorldCoords(fx, fz1);
            Vec3 alongZ;
            alongZ.x = fx - fx;
            alongZ.y = h2 - h0Again;
            alongZ.z = fz1 - fz;
            normalizeInPlace(alongX);
            normalizeInPlace(alongZ);
            sample->x = alongZ.y * alongX.z - alongZ.z * alongX.y;
            sample->y = alongZ.z * alongX.x - alongX.z * alongZ.x;
            sample->z = alongX.y * alongZ.x - alongZ.y * alongX.x;
            normalizeInPlace(*sample);
            ++sample;
        }
    }

    const bool halfFloats = terrain->m_halfFloatSurrounding != 0;                // byte +0x3A8
    if (halfFloats)
    {
        unsigned short* out = static_cast<unsigned short*>(bits);
        for (int r = 0; r < n + 1; ++r)
        {
            for (int c = 0; c < n + 1; ++c)
            {
                const Vec3& left   = samples[(r + 1) * side + c];
                const Vec3& centre = samples[(r + 1) * side + c + 1];
                const Vec3& up     = samples[r * side + c + 1];
                Vec3 average;
                average.x = left.x + centre.x;
                average.y = left.y + centre.y;
                average.z = left.z + centre.z;
                average.x = average.x + up.x;
                average.y = average.y + up.y;
                average.z = average.z + up.z;
                average.x = average.x + centre.x;                                // the centre again
                average.y = average.y + centre.y;
                average.z = average.z + centre.z;
                average.x = average.x * 0.25f;
                average.y = average.y * 0.25f;
                average.z = average.z * 0.25f;
                normalizeInPlace(average);
                unsigned short half = 0;
                float value = average.x;
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(&half), &value, 1);
                *out++ = half;
                value = average.y;
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(&half), &value, 1);
                *out++ = half;
                value = average.z;
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(&half), &value, 1);
                *out++ = half;
                value = 0.0f;
                D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(&half), &value, 1);
                *out++ = half;
            }
        }
    }
    else
    {
        float* out = static_cast<float*>(bits);
        for (int r = 0; r < n + 1; ++r)
        {
            for (int c = 0; c < n + 1; ++c)
            {
                const Vec3& left   = samples[(r + 1) * side + c];
                const Vec3& centre = samples[(r + 1) * side + c + 1];
                const Vec3& up     = samples[r * side + c + 1];
                Vec3 average;
                average.x = left.x + centre.x;
                average.y = left.y + centre.y;
                average.z = left.z + centre.z;
                average.x = average.x + up.x;
                average.y = average.y + up.y;
                average.z = average.z + up.z;
                average.x = average.x + centre.x;                                // the centre again
                average.y = average.y + centre.y;
                average.z = average.z + centre.z;
                average.x = average.x * 0.25f;
                average.y = average.y * 0.25f;
                average.z = average.z * 0.25f;
                normalizeInPlace(average);
                out[0] = average.x;
                out[1] = average.y;
                out[2] = average.z;
                out += 3;
            }
        }
    }

    ::renddx9abi::engineFree(samples);                                           // sub_1005F370, no null test
    dx9rend::vertexBufferUnlock(normalVertexBuffer);
}


// TerrainCullerNode / TerrainCuller

TerrainCullerNode::TerrainCullerNode()
{
    patch = 0;
    lodTable = 0;
    layerLodTables.first = 0; layerLodTables.last = 0; layerLodTables.end = 0;
    occlusionQuery = 0;
    bounds.min.x = 0.0f; bounds.min.y = 0.0f; bounds.min.z = 0.0f;
    bounds.max.x = 0.0f; bounds.max.y = 0.0f; bounds.max.z = 0.0f;
    bounds.cornerLength = 0.0f; bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.centre.x = 0.0f; bounds.centre.y = 0.0f; bounds.centre.z = 0.0f;
    for (int i = 0; i < 24; ++i) corners[i] = 0.0f;
    fileByteEC = 0;
    hasPatchList = 0;
    fileByteEE = 0;
    childrenWalked = 0;
    visible = 0;
    cameraInside = 0;
    crackMask = 0;
    unresolved10C = 0;
    lod = 2;
    unresolved104 = -1;
    child[0] = 0; child[1] = 0; child[2] = 0; child[3] = 0;
}

TerrainCuller::TerrainCuller()
{
    terrain = 0;
    nodes = 0;
    param10 = 1;
    param14 = 1;
    param18 = 100;
    {
        // Default LOD factor is the exact float bit pattern for 0.4.
        const unsigned int u = 0x3ECCCCCDu;
        ::memcpy(&lodFactor20, &u, sizeof(u));
    }
    lodFactor24 = 0.9f;
    lodFactor28 = 2.0f;
    lodFactor2C = 2.7f;
    lodFactor30 = 3.0f;
    visible.first = 0; visible.last = 0; visible.end = 0;
    gathered.first = 0; gathered.last = 0; gathered.end = 0;
    frustum = 0;
    cameraPosition = 0;
    gatherCentre = 0;
    gatherRadius = 0.0f;
    gatherRadiusSq = 0.0f;
    param68 = 2;
    param6C = 3;
    flag70 = 1;
    flag71 = 1;
    versionFlag78 = 0;
}

void TerrainCuller::init(Terrain* owner, int a, int b)
{
    terrain = owner;
    param10 = a;
    param14 = b;
}

// 0x100E3CC0, read in full: the occlusion query's reference - only
// pre-0x18 nodes (sub_100E4690) carry one - then ~vector on the per-layer table list.
TerrainCullerNode::~TerrainCullerNode()
{
    if (occlusionQuery)
        dx9rend::queryReleaseRef(occlusionQuery);                                 // --ref; sub_10003FA0 + free at zero
    if (layerLodTables.first)                                                     // ~vector: _Tidy
        ::renddx9abi::engineFree(layerLodTables.first);
    layerLodTables.first = 0;
    layerLodTables.last  = 0;
    layerLodTables.end   = 0;
}

// Destroys array nodes for modern terrain files or recursively frees legacy nodes.
// Requires terrain ownership and completed node initialization; then frees gather/visible lists.
TerrainCuller::~TerrainCuller()
{
    Terrain* const t = terrain;
    if (t->m_rawHasVersion && t->m_rawVersion >= 0x18u)
    {
        if (nodes)
        {
            unsigned int* const block = reinterpret_cast<unsigned int*>(nodes) - 1;
            const int count = static_cast<int>(*block);
            for (int i = count - 1; i >= 0; --i)                                  // `sub ecx,1 / js` - signed
                nodes[i].~TerrainCullerNode();
            ::renddx9abi::engineFree(block);
        }
    }
    else
    {
        TerrainCullerNode* const root = nodes;
        if (root->child[0])
        {
            deleteNodeTree(root->child[0]);
            deleteNodeTree(root->child[1]);
            deleteNodeTree(root->child[2]);
            deleteNodeTree(root->child[3]);
        }
        root->~TerrainCullerNode();
        ::renddx9abi::engineFree(root);
    }

    if (gathered.first)                                                           // ~vector (+0x44): _Tidy
        ::renddx9abi::engineFree(gathered.first);
    gathered.first = 0;
    gathered.last  = 0;
    gathered.end   = 0;
    if (visible.first)                                                            // ~vector (+0x34): _Tidy
        ::renddx9abi::engineFree(visible.first);
    visible.first = 0;
    visible.last  = 0;
    visible.end   = 0;
}

// sub_100E3D10 - the pre-0x18 tree, one node at a time: the four subtrees when the first child is
// set (`node` itself is not tested, and only child[0] is), then the node's destructor and free.
void TerrainCuller::deleteNodeTree(TerrainCullerNode* node)
{
    if (node->child[0])
    {
        deleteNodeTree(node->child[0]);
        deleteNodeTree(node->child[1]);
        deleteNodeTree(node->child[2]);
        deleteNodeTree(node->child[3]);
    }
    node->~TerrainCullerNode();
    ::renddx9abi::engineFree(node);
}

// one node's file record: eight u16 neighbour indices, the bounds, three bytes and
// the patch index. Written in the shipped read order.
void TerrainCuller::loadNode(TerrainRawReader& reader, TerrainCullerNode* node)
{
    float minMax[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    unsigned short neighbourIndex[8];
    for (int i = 0; i < 8; ++i)
        reader.readU16(&neighbourIndex[i]);
    reader.readVec3(minMax);
    reader.readVec3(minMax + 3);
    reader.readByte(&node->fileByteEC);
    reader.readByte(&node->hasPatchList);
    reader.readByte(&node->fileByteEE);
    unsigned char patchIndex = 0;
    reader.readByte(&patchIndex);

    node->allNeighbours = 1;
    for (int i = 0; i < 8; ++i)
    {
        if (neighbourIndex[i] == 0xFFFFu)
        {
            node->neighbour[i] = 0;
            node->allNeighbours = 0;
        }
        else
            node->neighbour[i] = nodes + neighbourIndex[i];
    }
    node->bounds.min.x = minMax[0]; node->bounds.min.y = minMax[1]; node->bounds.min.z = minMax[2];
    node->bounds.max.x = minMax[3]; node->bounds.max.y = minMax[4]; node->bounds.max.z = minMax[5];
    node->bounds.dirty = 1;
    aabbCorners(node->corners, &node->bounds.min.x);                                // lea ecx, [esi+8Ch]
    if (patchIndex == 0xFFu)
        node->patch = 0;
    else
        node->patch = terrain->m_patches[patchIndex];
}

// the node array (a count word ahead of it), then per node: the four children by
// position, the record, and the LOD tables - the terrain's own for a leaf (plus each detail
// layer's table), the parent's child tables for an inner node that asks for them.
void TerrainCuller::load(TerrainRawReader& reader)
{
    Terrain* const t = terrain;
    if (t->m_rawHasVersion && t->m_rawVersion >= 0x1Au)
        versionFlag78 = 1;
    reader.readU32(&nodeCount);
    const int count = nodeCount;
    unsigned int* const storage = static_cast<unsigned int*>(
        ::renddx9abi::engineAlloc(static_cast<unsigned int>(272 * count + 4)));
    TerrainCullerNode* first = 0;
    if (storage)
    {
        *storage = static_cast<unsigned int>(count);
        first = reinterpret_cast<TerrainCullerNode*>(storage + 1);
        for (int i = 0; i < count; ++i)
            new (first + i) TerrainCullerNode();
    }
    nodes = first;
    if (count <= 0)
        return;

    for (int k = 0; k < count; ++k)
    {
        TerrainCullerNode* const node = nodes + k;
        const int firstChild = 1 + 4 * k;
        if (firstChild >= count)
        {
            node->child[3] = 0; node->child[2] = 0; node->child[1] = 0; node->child[0] = 0;
        }
        else
        {
            node->child[0] = nodes + firstChild;
            node->child[1] = nodes + firstChild + 1;
            node->child[2] = nodes + firstChild + 2;
            node->child[3] = nodes + firstChild + 3;
        }
        loadNode(reader, node);
        node->childrenWalked = 0;
        node->lod = 0;
        if (node->hasPatchList)
        {
            node->lodTable = t->m_lodTables;
            QuadPatch* const patch = node->patch;                                 // unguarded, as shipped
            const unsigned int layers = layerCount(patch->detailLayers);
            for (unsigned int i = 0; i < layers; ++i)
            {
                QuadPatchDetailLayer* const layer =
                    reinterpret_cast<QuadPatchDetailLayer**>(patch->detailLayers.first)[i];
                unsigned char* const table = layer ? layer->lodTables : 0;
                vc71::vectorPushBackIn(node->layerLodTables, 4u, &table);
            }
        }
        else if (!node->fileByteEC)
        {
            node->lodTable = 0;
        }
        else
        {
            TerrainCullerNode* const parent = nodes + (k - 1) / 4;
            const int which = (k - 1) % 4;
            node->lodTable = *reinterpret_cast<unsigned char**>(parent->lodTable + 384 + 4 * which);
            const unsigned int layers = layerCount(node->patch->detailLayers);    // unguarded, as shipped
            for (unsigned int i = 0; i < layers; ++i)
            {
                unsigned char* const parentTable =
                    reinterpret_cast<unsigned char**>(parent->layerLodTables.first)[i];
                unsigned char* const table =
                    parentTable ? *reinterpret_cast<unsigned char**>(parentTable + 384 + 4 * which) : 0;
                vc71::vectorPushBackIn(node->layerLodTables, 4u, &table);
            }
        }
    }
}


// TerrainAabb - sub_100E7180 / sub_101C5EB0.

const float* TerrainAabb::corner(float out[3], int index) const
{
    switch (index)
    {
    case 0:  out[1] = min.y; out[0] = min.x; out[2] = min.z; break;
    case 1:  out[1] = min.y; out[0] = min.x; out[2] = max.z; break;
    case 2:  out[1] = max.y; out[0] = min.x; out[2] = min.z; break;
    case 3:  out[1] = max.y; out[0] = min.x; out[2] = max.z; break;
    case 4:  out[1] = min.y; out[0] = max.x; out[2] = min.z; break;
    case 5:  out[1] = min.y; out[0] = max.x; out[2] = max.z; break;
    case 6:  out[1] = max.y; out[0] = max.x; out[2] = min.z; break;
    case 7:  out[1] = max.y; out[0] = max.x; out[2] = max.z; break;
    default: out[1] = 0.0f; out[2] = 0.0f; out[0] = 0.0f; break;
    }
    return out;
}

void TerrainAabb::refresh()
{
    float best = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        float c[3];
        corner(c, i);
        const double d = static_cast<double>(c[0]) * c[0] + static_cast<double>(c[1]) * c[1]
                       + static_cast<double>(c[2]) * c[2];
        if (d > best)
            best = static_cast<float>(d);
    }
    cornerLength = static_cast<float>(std::sqrt(static_cast<double>(best)));
    best = 0.0f;
    const double sumX = static_cast<double>(max.x) + min.x;
    float sumY = max.y + min.y;
    float sumZ = max.z + min.z;
    sumZ = sumZ * 0.5f;
    centre.z = sumZ;
    centre.x = static_cast<float>(sumX * 0.5);
    centre.y = sumY * 0.5f;
    for (int i = 0; i < 8; ++i)
    {
        float c[3];
        corner(c, i);
        const double dy = static_cast<double>(c[1]) - centre.y;
        const double dz = static_cast<double>(c[2]) - centre.z;
        const double dx = static_cast<double>(c[0]) - centre.x;
        const double d = dz * dz + dy * dy + dx * dx;
        if (d > best)
            best = static_cast<float>(d);
    }
    radius = static_cast<float>(std::sqrt(static_cast<double>(best)));
    const float ex = max.x - min.x;
    const float ey = max.y - min.y;
    const float ez = max.z - min.z;
    extent.z = ez;
    extent.x = ex;
    extent.y = ey;
    dirty = 0;
}


// TerrainCuller - the per-frame walk.

// clear the visible list, take the view's frustum and camera, the LOD distance from
// the zoom (floored at param14), the root walk, the crack fixup.
void TerrainCuller::update()
{
    if (visible.first)                                                            // sub_100E7520(0, 0)
        visible.last = visible.first;
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();                   // base +0x48
    frustum = view->getFrustumPtr();                                              // +0x74
    cameraPosition = view->getViewMatrixPtr()->m + 12;                            // +0x64, row 3
    int lod = static_cast<int>(static_cast<long long>(
        view->getZoom() * static_cast<double>(param18) * 0.75));                  // +0x28
    if (param14 >= lod)
        lod = param14;
    lodDistance = lod;
    if (lastLodDistance != lod)
    {
        terrain->m_lightingDirty = 1;
        lastLodDistance = lodDistance;
    }
    walkRoot(nodes);
    if (flag70)
        fixCracks();
}

// the root: the bounding sphere against the frustum, the camera's xz against the box
// (flag71), else the eight corners against every plane; then the mode dispatch.
void TerrainCuller::walkRoot(TerrainCullerNode* node)
{
    const unsigned char cameraTest = flag71;
    TerrainAabb& box = node->bounds;
    if (box.dirty)
        box.refresh();
    const float radius = box.radius;
    if (box.dirty)
        box.refresh();
    node->visible = frustum->inside(box.centre, radius) ? 1 : 0;
    if (!node->visible)
        return;
    bool dispatch = false;
    if (cameraTest)
    {
        const float* const cam = cameraPosition;
        if (cam[0] >= box.min.x && cam[0] <= box.max.x && cam[2] >= box.min.z && cam[2] <= box.max.z)
        {
            node->visible = 1;
            node->cameraInside = 1;
            dispatch = true;
        }
    }
    if (!dispatch)
    {
        node->cameraInside = 0;
        const float* plane = reinterpret_cast<const float*>(frustum);
        int inside = 0;
        for (int i = 0; i < 6; ++i, plane += 4)
        {
            const int n = cornersInFront(box, plane);
            if (n == 8)
            {
                node->visible = 0;
                return;
            }
            if (n)
            {
                node->visible = 1;
                dispatch = true;
                break;
            }
            ++inside;
        }
        if (!dispatch)
        {
            node->visible = (inside == 6) ? 1 : 0;
            if (inside != 6)
                return;
        }
    }
    switch (param6C)
    {
    case 3:
        walkMode3(node);
        break;
    case 1: case 2: case 4: case 5: case 6:
        RD9_FRONTIER("dice::hfe::geom::TerrainCuller::walkRoot (modes 1/2/4/5/6 - sub_100E4F10 / "
                     "sub_100E53A0 / sub_100E5DD0 / sub_100E6270 / sub_100E6640, unread; the shipped "
                     "mode is 3)", 0x28);
        break;
    default:
        break;
    }
}

// mode 3, per child: the sphere, the camera's xz when the parent held it, else the
// corner planes; the LOD from the xz distance to the box plus the camera's height above the
// terrain; then a push (fileByteEE) or a descent.
void TerrainCuller::walkMode3(TerrainCullerNode* parent)
{
    parent->childrenWalked = 0;
    for (int c = 0; c < 4; ++c)
    {
        TerrainCullerNode* const child = parent->child[c];
        const unsigned char parentCameraInside = parent->cameraInside;
        TerrainAabb& box = child->bounds;
        if (box.dirty)
            box.refresh();
        const float radius = box.radius;
        if (box.dirty)
            box.refresh();
        const bool inSphere = frustum->inside(box.centre, radius);
        child->visible = inSphere ? 1 : 0;
        if (!inSphere)
            continue;
        bool lodBlock = false;
        const float* const cam = cameraPosition;
        if (parentCameraInside
            && cam[0] >= box.min.x && cam[0] <= box.max.x && cam[2] >= box.min.z && cam[2] <= box.max.z)
        {
            child->visible = 1;
            child->cameraInside = 1;
            lodBlock = true;
        }
        else
        {
            child->cameraInside = 0;
            const float* plane = reinterpret_cast<const float*>(frustum);
            int inside = 0;
            bool decided = false;
            for (int i = 0; i < 6; ++i, plane += 4)
            {
                const int n = cornersInFront(box, plane);
                if (n == 8)
                {
                    child->visible = 0;
                    decided = true;
                    break;
                }
                if (n)
                {
                    child->visible = 1;
                    lodBlock = true;
                    decided = true;
                    break;
                }
                ++inside;
            }
            if (!decided)
            {
                child->visible = (inside == 6) ? 1 : 0;
                lodBlock = (inside == 6);
            }
        }
        if (!lodBlock)
            continue;

        // The xz distance from the camera to the box (0 inside), plus the camera's height above
        // the terrain; the FPU C0|C2 test is "below min or unordered".
        double dx;
        if (!(cam[0] >= box.min.x))
            dx = static_cast<double>(cam[0]) - box.min.x;
        else if (cam[0] <= box.max.x)
            dx = 0.0;
        else
            dx = static_cast<double>(box.max.x) - cam[0];
        double dz;
        if (!(cam[2] >= box.min.z))
            dz = static_cast<double>(cam[2]) - box.min.z;
        else if (cam[2] <= box.max.z)
            dz = 0.0;
        else
            dz = static_cast<double>(box.max.z) - cam[2];
        const double distance = std::sqrt(dz * dz + dx * dx) + terrain->m_cameraHeightAboveTerrain;
        const double lodDist = static_cast<double>(lodDistance);
        if (distance > lodDist)
        {
            if (distance > lodDist * lodFactor30)
                setLod(child, 2);
            else
                setLod(child, 1);
        }
        else
        {
            if (child->child[0])
            {
                for (int g = 0; g < 4; ++g)
                    if (child->child[g]->visible)
                        setLod(child->child[g], 0);
            }
            child->lod = 0;
        }
        if (child->fileByteEE)
            vc71::vectorPushBackIn(visible, 4u, &child);
        else
            walkMode3(child);
        ++parent->childrenWalked;
    }
}

// the node's visible children first, then the node; 3 and above clamp to 2.
void TerrainCuller::setLod(TerrainCullerNode* node, int lod)
{
    if (node->child[0])
    {
        for (int c = 0; c < 4; ++c)
            if (node->child[c]->visible)
                setLod(node->child[c], lod);
    }
    node->lod = lod;
    if (lod >= 3)
        node->lod = 2;
}

// the crack mask. Version 0x1A content: per visible node, the neighbours 1 (bit 1),
// 6 (bit 2), 3 (bit 4) and 4 (bit 8) that sit one LOD finer decide the mask; a LOD-2 node with any
// LOD-0 neighbour drops to LOD 1 first. Older content: a LOD-2 node with a null or LOD-0 neighbour
// drops to LOD 1 and the mask stays 0.
void TerrainCuller::fixCracks()
{
    TerrainCullerNode** const first = reinterpret_cast<TerrainCullerNode**>(visible.first);
    TerrainCullerNode** const last  = reinterpret_cast<TerrainCullerNode**>(visible.last);
    if (versionFlag78)
    {
        for (TerrainCullerNode** it = first; it != last; ++it)
        {
            TerrainCullerNode* const n = *it;
            const int lod = n->lod;
            n->crackMask = 0;
            TerrainCullerNode* const nb1 = n->neighbour[1];
            TerrainCullerNode* const nb3 = n->neighbour[3];
            TerrainCullerNode* const nb4 = n->neighbour[4];
            TerrainCullerNode* const nb6 = n->neighbour[6];
            if (n->allNeighbours)
            {
                if (lod == 1)
                {
                    if (lod0(nb1))
                    {
                        n->crackMask = 1;
                        if (lod0(nb3))       n->crackMask = 5;
                        else if (lod0(nb4))  n->crackMask = 9;
                    }
                    else if (lod0(nb6))
                    {
                        n->crackMask = 2;
                        if (lod0(nb3))       n->crackMask = 6;
                        else if (lod0(nb4))  n->crackMask = 10;
                    }
                    else if (lod0(nb3))      n->crackMask = 4;
                    else if (lod0(nb4))      n->crackMask = 8;
                }
                else if (lod == 2)
                {
                    bool anyLod0 = false;
                    for (int i = 0; i < 8; ++i)
                        if (n->neighbour[i]->lod == 0) { anyLod0 = true; break; }
                    if (anyLod0)
                    {
                        n->lod = 1;
                        if (lod0(nb1))
                        {
                            n->crackMask = 1;
                            if (lod0(nb3))       n->crackMask = 1 | 4;
                            else if (lod0(nb4))  n->crackMask = 1 | 8;
                        }
                        else if (lod0(nb6))
                        {
                            n->crackMask = 2;
                            if (lod0(nb3))       n->crackMask = 2 | 4;
                            else if (lod0(nb4))  n->crackMask = 2 | 8;
                        }
                        else if (lod0(nb3))      n->crackMask = 4;
                        else if (lod0(nb4))      n->crackMask = 8;
                    }
                    if (lod1(nb1))
                    {
                        n->crackMask |= 1;
                        if (lod1(nb3))       n->crackMask |= 4;
                        else if (lod1(nb4))  n->crackMask |= 8;
                    }
                    else if (lod1(nb6))
                    {
                        n->crackMask |= 2;
                        if (lod1(nb3))       n->crackMask |= 4;
                        else if (lod1(nb4))  n->crackMask |= 8;
                    }
                    else if (lod1(nb3))      n->crackMask |= 4;
                    else if (lod1(nb4))      n->crackMask |= 8;
                }
            }
            else
            {
                if (lod == 1)
                {
                    if (lod0n(nb1))
                    {
                        n->crackMask = 1;
                        if (lod0n(nb3))      n->crackMask = 5;
                        else if (nb4 && nb4->visible && nb4->lod == 0) n->crackMask = 9;
                    }
                    else if (lod0n(nb6))
                    {
                        n->crackMask = 2;
                        if (lod0n(nb3))      n->crackMask = 6;
                        else if (lod0n(nb4)) n->crackMask = 10;
                    }
                    else if (lod0n(nb3))     n->crackMask = 4;
                    else if (lod0n(nb4))     n->crackMask = 8;
                }
                else if (lod == 2)
                {
                    bool anyLod0 = false;
                    for (int i = 0; i < 8; ++i)
                    {
                        TerrainCullerNode* const nb = n->neighbour[i];
                        if (nb && nb->lod == 0) { anyLod0 = true; break; }
                    }
                    if (anyLod0)
                    {
                        n->lod = 1;
                        if (lod0n(nb1))
                        {
                            n->crackMask = 1;
                            if (lod0n(nb3))      n->crackMask = 1 | 4;
                            else if (lod0n(nb4)) n->crackMask = 1 | 8;
                        }
                        else if (lod0n(nb6))
                        {
                            n->crackMask = 2;
                            if (lod0n(nb3))      n->crackMask = 2 | 4;
                            else if (lod0n(nb4)) n->crackMask = 2 | 8;
                        }
                        else if (lod0n(nb3))     n->crackMask = 4;
                        else if (lod0n(nb4))     n->crackMask = 8;
                    }
                    if (lod1n(nb1))
                    {
                        n->crackMask |= 1;
                        if (lod1n(nb3))      n->crackMask |= 4;
                        else if (lod1n(nb4)) n->crackMask |= 8;
                    }
                    else if (lod1n(nb6))
                    {
                        n->crackMask |= 2;
                        if (lod1n(nb3))      n->crackMask |= 4;
                        else if (lod1n(nb4)) n->crackMask |= 8;
                    }
                    else if (lod1n(nb3))     n->crackMask |= 4;
                    else if (lod1n(nb4))     n->crackMask |= 8;
                }
            }
        }
    }
    else
    {
        for (TerrainCullerNode** it = first; it != last; ++it)
        {
            TerrainCullerNode* const n = *it;
            const bool lod2 = n->lod == 2;
            n->crackMask = 0;
            if (lod2)
            {
                for (int i = 0; i < 8; ++i)
                {
                    TerrainCullerNode* const nb = n->neighbour[i];
                    if (nb && nb->lod == 0)
                    {
                        n->lod = 1;
                        break;
                    }
                }
            }
        }
    }
}

// clear the gathered list, take the centre and radius, walk from the root.
void TerrainCuller::gatherSphere(const float* centre, float radius)
{
    if (gathered.first)                                                           // sub_100E7520(0, 0)
        gathered.last = gathered.first;
    gatherRadius = radius;
    gatherRadiusSq = radius * radius;
    gatherCentre = centre;
    gatherNode(nodes);
}

// children first; an inner node whose sphere reaches the gather sphere and that
// carries a patch is pushed. Leaves (no children) are never considered, as shipped.
void TerrainCuller::gatherNode(TerrainCullerNode* node)
{
    if (!node->child[0])
        return;
    for (int c = 0; c < 4; ++c)
        gatherNode(node->child[c]);
    if (node->bounds.dirty)
        node->bounds.refresh();
    const float radius = node->bounds.radius;
    if (node->bounds.dirty)
        node->bounds.refresh();
    const float* const centre = gatherCentre;
    const double dz = static_cast<double>(centre[2]) - node->bounds.centre.z;
    const double dx = static_cast<double>(centre[0]) - node->bounds.centre.x;
    const double dy = static_cast<double>(centre[1]) - node->bounds.centre.y;
    if (static_cast<double>(radius) + gatherRadius > std::sqrt(dz * dz + dx * dx + dy * dy))
    {
        if (node->patch)
            vc71::vectorPushBackIn(gathered, 4u, &node);
    }
}

// Gathers sphere-overlapping patches from this frame's visible terrain tree.
void TerrainCuller::gatherVisibleSphere(const float* centre, float radius)
{
    if (gathered.first)                                                           // sub_100E7520(0, 0)
        gathered.last = gathered.first;
    gatherCentre = centre;
    gatherRadiusSq = radius * radius;
    gatherRadius = radius;
    if (nodes->visible)
        gatherVisibleNode(nodes);
}

// Gathers visible leaf patches whose spheres strictly overlap the gather sphere.
// Traversed internal nodes require four valid children. Arithmetic stays in float precision
// to preserve the renderer's 24-bit x87 behavior (RW-240).
void TerrainCuller::gatherVisibleNode(TerrainCullerNode* node)
{
    if (node->childrenWalked != 0)
    {
        for (int c = 0; c < 4; ++c)
        {
            if (node->child[c]->visible)
                gatherVisibleNode(node->child[c]);
        }
        return;
    }
    if (node->bounds.dirty)
        node->bounds.refresh();
    const float radius = node->bounds.radius;
    if (node->bounds.dirty)
        node->bounds.refresh();
    const float* const centre = gatherCentre;
    const float dx = centre[0] - node->bounds.centre.x;
    const float dy = centre[1] - node->bounds.centre.y;
    const float dz = centre[2] - node->bounds.centre.z;
    const float distance = std::sqrt(dz * dz + dx * dx + dy * dy);          // the float overload
    const float reach = radius + gatherRadius;
    if (reach > distance && node->patch)
        vc71::vectorPushBackIn(gathered, 4u, &node);
}


// QuadPatch - the draws.

namespace {

// The per-patch constants the three draws set first: scaletransxz, scalebaseuv, the stream, and
// colorlighttex from the lightmap's width.
void beginPatchDraw(QuadPatch* patch)
{
    Terrain* const t = patch->terrain;
    float scaleTrans[4];
    scaleTrans[0] = static_cast<float>(t->m_primaryWorldScale.x * 255.0);
    scaleTrans[1] = static_cast<float>(t->m_primaryWorldScale.z * 255.0);
    scaleTrans[2] = patch->worldXf;
    scaleTrans[3] = patch->worldZf;
    param(t->m_hScaleTransXZ)->setValue(scaleTrans, 0);
    param(t->m_hScaleBaseUV)->setFloat(static_cast<float>(255.0 / static_cast<double>(t->m_patchSize)));
    dx9rend::vertexBufferBindStream(patch->vertexBuffer, t->m_compactVertices ? 1u : 0u);
    float colorLightTex[2];
    colorLightTexOf(patch->lightmap, colorLightTex);
    param(t->m_hColorLightTex)->setValue(colorLightTex, 0);
}

inline void drawPatchRange(Terrain* t, unsigned int primCount, unsigned int vertexCount, unsigned int start)
{
    dx9rend::rendererDrawIndexedPrimitive(primCount, static_cast<unsigned int>(t->m_primitiveType),
                                          0, 0u, vertexCount, start);
}

} // anonymous namespace

// pass 0: every visible node of this patch, by its LOD range; the "TerrainLightmap"
// arm merges contiguous ranges of one LOD, the "TerrainMorphing" arm draws them one by one.
void QuadPatch::draw(TerrainCullerNode**& it)
{
    static unsigned int s_once = 0;                                               // dword_10262164
    static unsigned int s_vertexCount = 0;                                        // dword_10262160
    Terrain* const t = terrain;
    if ((s_once & 1u) == 0)
    {
        const int n = t->m_patchSize;
        s_once |= 1u;
        s_vertexCount = static_cast<unsigned int>((n + 1) * (n + 1));
    }
    beginPatchDraw(this);
    vc71::vector_layout* const list = t->m_culler->visiblePatches();
    TerrainCullerNode** const end = reinterpret_cast<TerrainCullerNode**>(list->last);
    if (t->m_mergeDrawRanges == 0)
    {
        while (it != end)
        {
            TerrainCullerNode* const node = *it;
            if (node->patch != this)
                break;
            const int lod = node->lod;
            if (t->m_currentLod != lod)
                t->setMorphConstants(lod);
            const unsigned int count = lodCount(node);
            const unsigned int start = lodStart(node);
            const unsigned int prims = count / 3;
            drawPatchRange(t, prims, s_vertexCount, start);
            ++terrainRendererStat(616);
            terrainRendererStat(620) += prims;
            ++it;
        }
        return;
    }
    while (it != end)
    {
        TerrainCullerNode* const node = *it;
        if (node->patch != this)
            break;
        const int lod = node->lod;
        unsigned int start = lodStart(node);
        unsigned int count = lodCount(node);
        if (t->m_currentLod != lod)
            t->setMorphConstants(lod);
        ++it;
        bool flushed = false;
        while (it != end)
        {
            TerrainCullerNode* const next = *it;
            if (next->patch != this)
                break;
            const unsigned int nextStart = lodStart(next);
            const unsigned int nextCount = lodCount(next);
            if (lod != next->lod || nextStart != start + count)
            {
                if (lod != next->lod || nextCount + nextStart != start)
                {
                    drawPatchRange(t, count / 3, s_vertexCount, start);
                    flushed = true;
                    break;
                }
                start -= nextCount;
            }
            count += nextCount;
            ++it;
        }
        if (!flushed && static_cast<int>(count) > 0)
            drawPatchRange(t, count / 3, s_vertexCount, start);
    }
}

// Draws gathered nodes while advancing it, switching passes and morph constants as needed.
// The default mode uses uncracked index ranges. Counts primitives, without merging ranges.
void QuadPatch::drawGatheredNodes(TerrainCullerNode**& it, vc71::vector_layout* list,
                                  int passLod0, int passOther)
{
    static unsigned int s_once = 0;                                               // dword_10261A0C
    static unsigned int s_vertexCount = 0;                                        // dword_10261A08
    static const int kRangeMode = 1;                                              // dword_1023A5D0 [.data 1, no writer]
    Terrain* const t = terrain;
    if ((s_once & 1u) == 0)
    {
        const int n = t->m_patchSize;
        s_once |= 1u;
        s_vertexCount = static_cast<unsigned int>((n + 1) * (n + 1));
    }
    beginPatchDraw(this);
    if (it == reinterpret_cast<TerrainCullerNode**>(list->last))
        return;
    do
    {
        TerrainCullerNode* const node = *it;
        if (node->patch != this)
            break;
        const int pass = (node->lod == 0) ? passLod0 : passOther;
        if (t->m_currentPass != pass)
        {
            t->m_currentPass = pass;                                              // stored first, as shipped
            dx9rend::effectBeginPass(t->m_effect, static_cast<unsigned int>(pass));
        }
        const int lod = node->lod;
        if (t->m_currentLod != lod)
            t->setMorphConstants(lod);
        unsigned int start = lodStart(node);                                      // the crack-fixed pair,
        unsigned int count = lodCount(node);                                      //   loaded first
        if (kRangeMode == 1)
        {
            const unsigned int* const table = reinterpret_cast<const unsigned int*>(node->lodTable);
            start = table[16 * lod];
            count = table[16 * (lod + 3)];
        }
        const int prims = static_cast<int>(count) / 3;
        drawPatchRange(t, static_cast<unsigned int>(prims), s_vertexCount, start);
        terrainRendererStat(0x270) += static_cast<unsigned int>(prims);           // Renderer +0x6C, then +0x270
        ++it;
    } while (it != reinterpret_cast<TerrainCullerNode**>(list->last));
}

// (132 B) - "TerrainPointLight": this patch's run of the culler's gathered list (the
// terrain's m_culler, gatheredPatches 0x100E2EE0) through drawGatheredNodes with pass 13 at LOD 0
// and pass 1 above it. Terrain::drawMaskLights is the live caller (the ShaderVersion-1 detail pass,
// dead on stock shaders, is the other). The once-registered profiler timer (0x10262168,
// "TerrainPointLight") and its begin/end (sub_10046EE0 / sub_10046EF0) are stat-only scaffolding
// this tree does not reproduce. Its sibling 0x1018C880 ("TerrainSpotLight", passes 12 / 1) has only
// the dead detail pass as a caller and is not written.
void QuadPatch::drawPointLight(TerrainCullerNode**& it)
{
    drawGatheredNodes(it, terrain->m_culler->gatheredPatches(), 13, 1);
}

// the diffuse passes. "TerrainDiffusePassLod0": per detail layer (the cached layer
// first, then the rest, one hidden by +0x3C0), this patch's LOD-0 nodes through the layer's index
// buffer and pass 4 / 5 (side) / 11 (envmap) / 14 (layer 6). "TerrainDiffusePassLodLower": the
// nodes above LOD 0 (every node at low quality) through the LOD index buffer and pass 3 (14 for a
// patch without layers when dword_10262644 is set - which nothing writes).
void QuadPatch::drawFinal(TerrainCullerNode**& it)
{
    static unsigned int s_once = 0;                                               // dword_10262B1C
    static unsigned int s_vertexCount = 0;                                        // dword_10262B18
    const int kLowerPass14 = 0;                                                   // dword_10262644 [UNRESOLVED writer - none]
    Terrain* const t = terrain;
    if ((s_once & 1u) == 0)
    {
        const int n = t->m_patchSize;
        s_once |= 1u;
        s_vertexCount = static_cast<unsigned int>((n + 1) * (n + 1));
    }
    beginPatchDraw(this);
    TerrainCullerNode** const begin = it;
    vc71::vector_layout* const list = t->m_culler->visiblePatches();
    TerrainCullerNode** const end = reinterpret_cast<TerrainCullerNode**>(list->last);
    currentDetailMap = -1;
    int previous = -2;
    if (!t->m_lowQualityB)
    {
        for (int layerIter = -1; layerIter < 7; ++layerIter)
        {
            int layer = layerIter;
            if (layerIter == previous)
                continue;
            if (layerIter == -1)
            {
                const int cached = t->m_currentDetailLayer;
                if (cached == -1)
                    continue;
                layer = cached;
                previous = cached;
            }
            if (t->m_hiddenDetailLayer != -1 && t->m_hiddenDetailLayer == layer)
                continue;
            QuadPatchDetailLayer* const detailLayer =
                reinterpret_cast<QuadPatchDetailLayer**>(detailLayers.first)[layer];
            if (!detailLayer)
                continue;
            it = begin;
            bool bound = false;
            while (it != end)
            {
                TerrainCullerNode* const node = *it;
                if (node->patch != this)
                    break;
                if (node->lod != 0)
                {
                    ++it;
                    continue;
                }
                if (!bound)
                {
                    TerrainDetailTexture* const record =
                        reinterpret_cast<TerrainDetailTexture**>(t->m_detailTextures.first)[detailLayer->detailIndex];
                    if (t->m_currentLod != 0)
                        t->setMorphConstants(0);
                    dx9rend::indexBufferBind(detailLayer->indexBuffer);
                    int pass;
                    if (layer == 6)
                        pass = 14;
                    else
                    {
                        const int map = detailLayer->detailIndex / 3;
                        if (currentDetailMap != map)
                        {
                            currentDetailMap = map;
                            param(t->m_hTexLayer[2])->setValue(detailMap[map], 0);
                        }
                        // The real EnvMapManager now owns the borrowed texture at +64.
                        void* envTexture = record->planeMap ? 0 :
                            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(terrainEnvMapManager()) + 0x64);
                        if (layer == t->m_currentDetailLayer)
                        {
                            if (record->planeMap)
                                pass = 5;
                            else if (envTexture && record->envMap)
                                pass = 11;
                            else
                                pass = 4;
                        }
                        else
                        {
                            t->m_currentDetailLayer = layer;
                            if (record->planeMap)
                            {
                                pass = 5;
                                param(t->m_hTexLayer[6])->setValue(record->sideTexture, 0);
                            }
                            else if (envTexture && record->envMap)
                            {
                                pass = 11;
                                param(t->m_hTexLayer[6])->setValue(envTexture, 0);
                            }
                            else
                                pass = 4;
                            float tiling[4];                                      // neartextiling
                            tiling[0] = record->sideTiling.x;
                            tiling[1] = record->sideTiling.y;
                            ::memcpy(&tiling[2], &record->topTiling, 4);          // the word as stored
                            ::memcpy(&tiling[3], &record->yOffset, 4);
                            param(t->m_hNearTexTiling)->setValue(tiling, 0);
                            param(t->m_hTexLayer[3])->setValue(record->texture, 0);
                            param(t->m_hComponentSelector)->setValue(&detailLayer->selectorX, 0);
                        }
                    }
                    if (t->m_currentPass != pass)
                    {
                        dx9rend::effectBeginPass(t->m_effect, static_cast<unsigned int>(pass));
                        t->m_currentPass = pass;
                    }
                    bound = true;
                }
                const unsigned int* const table =
                    reinterpret_cast<unsigned int**>(node->layerLodTables.first)[layer];
                const unsigned int count = table[48];
                if (!count)
                {
                    ++it;
                    continue;
                }
                if ((t->m_drawDebugFlags & 2) == 0)
                {
                    const unsigned int prims = count / 3;
                    drawPatchRange(t, prims, s_vertexCount, table[0]);
                    ++terrainRendererStat(628);
                    terrainRendererStat(632) += prims;
                    ++it;
                    continue;
                }
                // The merge (debug bit 2): the shipped loop re-reads the first node's table for every
                // follower (v40 is never re-derived) - reproduced as read.
                unsigned int start = table[0];
                unsigned int total = count;
                ++it;
                bool flushed = false;
                while (it != end)
                {
                    TerrainCullerNode* const next = *it;
                    if (next->patch != this)
                        break;
                    const unsigned int nextCount = table[48];
                    if (next->lod != 0)
                    {
                        flushed = true;
                        drawPatchRange(t, total / 3, s_vertexCount, start);
                        break;
                    }
                    if (table[0] != start + count)
                    {
                        if (nextCount + table[0] != start)
                        {
                            flushed = true;
                            drawPatchRange(t, total / 3, s_vertexCount, start);
                            break;
                        }
                        start -= nextCount;
                    }
                    total += nextCount;
                    ++it;
                }
                if (!flushed && static_cast<int>(total) > 0)
                    drawPatchRange(t, total / 3, s_vertexCount, start);
            }
        }
    }

    // "TerrainDiffusePassLodLower"
    dx9rend::indexBufferBind(t->m_lodIndexBuffer);
    if (hasNoDetailLayers && kLowerPass14)
    {
        if (t->m_currentPass != 14)
        {
            dx9rend::effectBeginPass(t->m_effect, 14u);
            t->m_currentPass = 14;
        }
    }
    else if (t->m_currentPass != 3)
    {
        dx9rend::effectBeginPass(t->m_effect, 3u);
        t->m_currentPass = 3;
    }
    it = begin;
    while (it != end)
    {
        TerrainCullerNode* const node = *it;
        if (node->patch != this)
            break;
        const int lod = node->lod;
        if (lod || t->m_lowQualityB)
        {
            if (t->m_currentLod != lod)
                t->setMorphConstants(lod);
            if ((t->m_drawDebugFlags & 1) == 0)
            {
                unsigned int count = lodCount(node);
                unsigned int start = lodStart(node);
                ++it;
                bool flushed = false;
                while (it != end)
                {
                    TerrainCullerNode* const next = *it;
                    if (next->patch != this)
                        break;
                    const unsigned int nextStart = lodStart(next);
                    const unsigned int nextCount = lodCount(next);
                    if (lod != next->lod || nextStart != start + count)
                    {
                        if (lod != next->lod || nextCount + nextStart != start)
                        {
                            drawPatchRange(t, count / 3, s_vertexCount, start);
                            flushed = true;
                            break;
                        }
                        start -= nextCount;
                    }
                    count += nextCount;
                    ++it;
                }
                if (!flushed && static_cast<int>(count) > 0)
                    drawPatchRange(t, count / 3, s_vertexCount, start);
                continue;
            }
            const int count = static_cast<int>(lodCount(node));
            drawPatchRange(t, static_cast<unsigned int>(count / 3), s_vertexCount, lodStart(node));
        }
        ++it;
    }
    lastDrawnFrame = dx9rend::rendererFrameOrdinal();                             // base +0x54
}

// pass 7: for each LOD 0..2, every gathered shadow range of this patch at that LOD,
// with the record's two light matrices and this patch's lightmap.
void QuadPatch::drawShadow()
{
    static unsigned int s_once = 0;                                               // dword_102634E0
    static unsigned int s_vertexCount = 0;                                        // dword_10263270
    Terrain* const t = terrain;
    beginPatchDraw(this);
    if ((s_once & 2u) == 0)
    {
        const int n = t->m_patchSize;
        s_once |= 2u;
        s_vertexCount = static_cast<unsigned int>((n + 1) * (n + 1));
    }
    for (int lod = 0; lod < 3; ++lod)
    {
        for (unsigned int k = 0; ; ++k)
        {
            const vc71::vector_layout& ranges = t->m_shadowRanges;
            if (!ranges.first || k >= static_cast<unsigned int>((ranges.last - ranges.first) / 24))
                break;
            const unsigned int* const record = reinterpret_cast<const unsigned int*>(ranges.first + 24 * k);
            const TerrainCullerNode* const node = reinterpret_cast<const TerrainCullerNode*>(record[3]);
            const unsigned char* const shadow = reinterpret_cast<const unsigned char*>(record[2]);
            if (node->lod == lod && static_cast<int>(record[5]) == patchIndex)
            {
                param(t->m_hLightViewProj)->setValue(shadow + 952, 0);
                param(t->m_hLightViewProjOrtho)->setValue(shadow + 760, 0);
                param(t->m_hTexLayer[0])->setValue(lightmap, 0);
                if (t->m_currentLod != lod)
                    t->setMorphConstants(lod);
                const unsigned int prims = record[1] / 3;
                drawPatchRange(t, prims, s_vertexCount, record[0]);
                terrainRendererStat(624) += prims;
            }
        }
    }
}

// one call of sub_1018AC00 per dynamic-shadow record of the LightManager; answers
// whether any range was written. The record vector stays empty while no template carries flag
// 0x800 (the directional casters' test, XpLightManager.cpp ~1159), so the loop body - unread - is
// not entered today. objectManager's `castsDynamicShadow` row sets that flag on about 100 stock
// templates and is held for this reason (RW-261): with it and dynamic shadows on, this stops.
bool QuadPatch::gatherShadowRanges(void** lockedIndices, int* cursor)
{
    unsigned char any = 0;
    const vc71::vector_layout& records = terrainShadowRecords();
    for (unsigned int i = 0; ; ++i)
    {
        if (!records.first || i >= static_cast<unsigned int>((records.last - records.first) / 1176))
            break;
        (void)lockedIndices;
        (void)cursor;
        RD9_FRONTIER("dice::hfe::geom::QuadPatch::gatherShadowRanges (sub_1018AC00 - the shadow "
                     "record vs patch range gather into Terrain +0x36C; unread, 3.8 KB)", 0x00);
    }
    return any != 0;
}

// the Ra water submit: an RaQuadPatchAdapter over this patch's streams, a lighting
// block with this patch's lightmap, the sky light into the light record, the cell's transform,
// then one pooled submit per merged LOD range into RSMTWater through the adapter server
// (dword_1026743C: sub_101BBE20 binds the settings, sub_101BBEB0 takes a pooled adapter and
// queues the entry). All of it is read; none of it is written, because no shipped shader defines
// the "ShaderVersion" that selects this arm (see TerrainDraw.cpp) - it stops at the adapter.
void QuadPatch::drawWater(TerrainCullerNode**& it, vc71::vector_layout* list,
                          void* materialParameters, void* objectParameters)
{
    static unsigned int s_once = 0;                                               // dword_1026326C
    static unsigned int s_vertexCount = 0;                                        // dword_10263268
    (void)materialParameters;
    (void)objectParameters;
    if ((s_once & 1u) == 0)
    {
        const int n = terrain->m_patchSize;
        s_once |= 1u;
        s_vertexCount = static_cast<unsigned int>((n + 1) * (n + 1));
    }
    RD9_FRONTIER("dice::hfe::geom::QuadPatch::drawWater (sub_1018D800 - the RaQuadPatchAdapter "
                 "(sub_101BBCB0) and its pooled server dword_1026743C (sub_101BBE20 / sub_101BBEB0 / "
                 "sub_101BC7C0), read but unwritten; reachable only when a water RaShader defines "
                 "ShaderVersion == 1)", 0x00);
    // Continue mode only: skip this patch's entries so the caller's loop can end. Not recovered.
    TerrainCullerNode** const end = reinterpret_cast<TerrainCullerNode**>(list->last);
    while (it != end && (*it)->patch == this)
        ++it;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
