
// Editable terrain patches, weight maps, brushes and lightmap generation.

#ifdef BF2_IMAGE_RENDDX9

#include "QuadPatchEditable.h"
#include "TerrainEditable.h"
#include "Terrain.h"
#include "QuadPatch.h"
#include "RoadEditableRenderer.h"             // RoadEditableRenderer::m_mode (+0x18) - the road-mask pass
#include "UndergrowthSystem.h"                // g_UndergrowthSystem, slot +0x5C editorPaint
#include "XpLightManager.h"                   // LightManager::getDynamicShadowsEnabled (+0xF0)
#include "HemiMapManager/HemiMapDependencies.h"   // the three renderers' enable seams

#include "../Physics/Heightmap.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/EnvMapSystem/EnvMapManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager, fileManager
#include "../../RendDX9Abi/Vc71String.h"

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
// BufferManager.cpp / StateManager.cpp, declared per TU as every other client does.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);  // sub_10011FF0 on the global
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                     // --ref, sub_10014510, free
void  bufferManagerRemoveVertexBuffer(void* wrapper);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);                                      // --ref, sub_10014300, free
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);

extern IBase* g_Renderer;                                                     // dword_1023D098 - BF2Render.cpp


// [CHOSEN, not recovered] the ortho top-down render's seam, re-declared for the road-mask pass
// (0x101A0680 calls sub_10029DA0) and the colormap burn (0x1019C630, R3). RenderFlags is defined in
// BF2Render.cpp (the struct renderScene takes, 24 bytes) and that TU publishes no header for it; this
// definition is a token-FOR-token copy (the ODR requires it - comments aside) and must change with it
// (SurroundingPatchEditable.cpp carries the same copy). [RETIRE-WHEN: RW-277] - when BF2Render.cpp's
// owner publishes the struct and the seam in a header. The function's body is the one read in BF2Render.cpp
// (rendererRenderOrthoTopDown, PE 0x10029DA0) - it consumes the caller's reference on `target`.

struct RenderFlags
{
    unsigned char useRenderView;
    unsigned char field01;
    unsigned char world;
    unsigned char worldAlt;
    unsigned char field04;
    unsigned char staticMeshes;
    unsigned char skinnedMeshes;
    unsigned char decals;
    unsigned char particles;
    unsigned char field09;
    unsigned char field0a;
    unsigned char skyDome;
    unsigned char undergrowth;
    unsigned char field0d;
    unsigned char postProduction;
    unsigned char colourCorrection;
    unsigned char hud;
    unsigned char aiDebug;
    unsigned char field12;
    unsigned char pad13;
    void*         renderView;
};
#if defined(_M_IX86)
static_assert(sizeof(RenderFlags) == 24, "renderScene's toggle block is 24 bytes - see BF2Render.cpp");
#endif
void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil,
                                Vec3 position, float orthoExtent, RenderFlags flags);
} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                          // ModuleGeom.cpp, dword_1024B914
extern IBase* g_RoadEditableRenderer;                                         // ModuleGeom.cpp - "RoadEditableRenderer"

namespace {

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

inline ::dice::hfe::world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

inline dx9rend::EngineTexture* engineTexture(void* texture)
{
    return static_cast<dx9rend::EngineTexture*>(texture);
}

// sub_10074AB0 - the image's printf-into-a-std::string helper: _vsnprintf into a 0xFFF-byte window,
// the shipped warning text when it does not fit (QuadPatch.cpp and StaticMesh.cpp carry the same body).
void formatString(vc71::string& out, const char* format, ...)
{
    char buffer[0x1000];
    va_list args;
    va_start(args, format);
    const int written = _vsnprintf(buffer, 0xFFF, format, args);
    va_end(args);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

// The "lock, fill every texel of mip 0, unlock" every texture-creating body here open-codes:
// textureLock(t, 0, 0x7FFFFFFF) (sub_10014BA0), width * height dwords from the record's bits, then
// textureUnlock (sub_10014D30).
void fillTexture(dx9rend::EngineTexture* texture, unsigned int value)
{
    dx9rend::TextureLockRecord* const record = dx9rend::textureLock(texture, 0, 0x7FFFFFFF);
    unsigned int* texel = static_cast<unsigned int*>(record->pBits);
    unsigned int* const end = texel + record->width * record->height;
    for (; texel != end; ++texel)
        *texel = value;
    dx9rend::textureUnlock(texture, record);
}

// the checkerboard D3DXFillTexture callback init gives a patch without a colormap
// (`retn 10h`; the texel size is not read). State: QuadPatchEditable +0x128 (last u, last v, the
// current cell's u / v start, the cell counters). The compares are the listing's: a coordinate below
// the last one resets that axis; a step of more than 0.125 (flt_1021DD38) opens the next cell.
void WINAPI checkerboardFill(D3DXVECTOR4* out, const D3DXVECTOR2* texCoord, const D3DXVECTOR2*,
                             void* data)
{
    float* const state = static_cast<float*>(data);
    float& lastU = state[0];
    float& lastV = state[1];
    float& cellU = state[2];
    float& cellV = state[3];
    int& countU = reinterpret_cast<int*>(state)[4];
    int& countV = reinterpret_cast<int*>(state)[5];
    if (texCoord->x < lastU)
    {
        countU = 0;
        cellU = 0.0f;
    }
    if (texCoord->y < lastV)
    {
        countV = 0;
        cellV = 0.0f;
    }
    if (texCoord->x - cellU > 0.125f)
    {
        countU += 1;
        cellU = texCoord->x;
    }
    if (texCoord->y - cellV > 0.125f)
    {
        countV += 1;
        cellV = texCoord->y;
    }
    if ((countV + countU) % 2 == 0)
    {
        out->x = 0.81f;
        out->y = 0.81f;
        out->z = 0.75f;
    }
    else
    {
        out->x = 0.94f;
        out->y = 0.94f;
        out->z = 0.91f;
    }
    out->w = 1.0f;
    lastU = texCoord->x;
    lastV = texCoord->y;
}

} // anonymous namespace


// PatchDetailLayer

PatchDetailLayer::PatchDetailLayer()
{
    created = 0;
    data = 0;
}

PatchDetailLayer::~PatchDetailLayer()
{
    if (data)
    {
        ::renddx9abi::engineFree(data);                                       // operator delete[] (sub_1005F370)
        data = 0;
    }
}

void PatchDetailLayer::init(QuadPatchEditable* newOwner, int newWidth)
{
    owner = newOwner;
    byteSize = newWidth * newWidth;
    width = newWidth;
}

// a second create only logs.
void PatchDetailLayer::create()
{
    if (created)
    {
        ::renddx9abi::logf("[QuadPatchEditable.cpp:207] TBM: PatchDetailLayer already created");
        return;
    }
    created = 1;
    data = static_cast<unsigned char*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(byteSize)));   // operator new[] (sub_1005F340)
}

// a log line, never a stop: every caller carries on with the null map.
void PatchDetailLayer::assertCreated()
{
    if (!created)
        ::renddx9abi::logf("[QuadPatchEditable.cpp:234] TBM: PatchDetailLayer is not created");
}

// The bound is `position > byteSize` (signed), so byteSize itself passes, and the
// refusal only logs - the address is answered regardless.
unsigned char* PatchDetailLayer::at(int position)
{
    assertCreated();
    if (position < 0 || position > byteSize)
        ::renddx9abi::logf("[QuadPatchEditable.cpp:128] TBM: pos out of range!");
    return data + position;
}

// the texels of heightmap quad (x, z): `width / patchSize` per side. No created test.
void PatchDetailLayer::clearQuad(int x, int z)
{
    const int block = width / owner->editableTerrain->m_patchSize;
    for (int i = 0; i < block; ++i)
        for (int j = 0; j < block; ++j)
            data[width * (j + z * block) + i + x * block] = 0;
}

// the mean weight over quad (x, z): column by column, each column's rows summed in order.
float PatchDetailLayer::averageWeight(int x, int z)
{
    assertCreated();
    float sum = 0.0f;
    const int w = width;
    const int block = w / owner->editableTerrain->m_patchSize;
    int count = 0;
    for (int col = 0; col < block; ++col)
    {
        for (int row = 0; row < block; ++row)
            sum = sum + static_cast<float>(data[x * block + col + w * (z * block + row)]);
        count += block;
    }
    return sum / static_cast<float>(count);
}

// any non-zero weight in quad (x, z) or the one-texel border around it; each sample is
// clamped to [0, width - 1] (the upper clamp first).
bool PatchDetailLayer::anyWeight(int x, int z)
{
    assertCreated();
    const int block = width / owner->editableTerrain->m_patchSize;
    const int w = width;
    const int px0 = x * block;
    const int pz0 = z * block;
    for (int i = -1; i <= block; ++i)
    {
        for (int j = -1; j <= block; ++j)
        {
            int px = px0 + i;
            int pz = pz0 + j;
            if (px >= w - 1)
                px = w - 1;
            if (px < 0)
                px = 0;
            if (pz >= w - 1)
                pz = w - 1;
            if (pz < 0)
                pz = 0;
            if (data[pz * w + px])
                return true;
        }
    }
    return false;
}


// PatchDetailLayers

void PatchDetailLayers::init(QuadPatchEditable* owner, int width)
{
    for (int i = 0; i < 6; ++i)
        records[i].init(owner, width);
}

bool PatchDetailLayers::isLayerSet(int layer, int x, int z)
{
    return (layerMask3(x, z) & static_cast<unsigned char>(1 << layer)) != 0;
}

// at most three layers: 5, 4, 3 first, then 0, 1, 2, stopping at the third found.
unsigned char PatchDetailLayers::layerMask3(int x, int z)
{
    unsigned char mask = 0;
    int found = 0;
    for (int layer = 5; layer > 2; --layer)
    {
        if (records[layer].anyWeight(x, z))
        {
            ++found;
            mask |= static_cast<unsigned char>(1 << layer);
            if (found == 3)
                return mask;
        }
    }
    for (int layer = 0; layer < 3; ++layer)
    {
        if (records[layer].anyWeight(x, z))
        {
            ++found;
            mask |= static_cast<unsigned char>(1 << layer);
            if (found == 3)
                break;
        }
    }
    return mask;
}

// the layer with the largest mean weight (strictly larger wins; 0 when none exceeds 0).
unsigned char PatchDetailLayers::dominantLayer(int x, int z)
{
    float best = 0.0f;
    unsigned char layer = 0;
    for (int i = 0; i < 6; ++i)
    {
        const float weight = records[i].averageWeight(x, z);
        if (weight > best)
        {
            best = weight;
            layer = static_cast<unsigned char>(i);
        }
    }
    return layer;
}

unsigned char PatchDetailLayers::layerMask(int x, int z)
{
    unsigned char mask = 0;
    for (int i = 0; i < 6; ++i)
        if (hasLayer(i, x, z))                                                // virtual (+0x14)
            mask |= static_cast<unsigned char>(1 << i);
    return mask;
}

// The guard only logs (`layer < 0 || layer > 6`, so 6 passes silently).
bool PatchDetailLayers::hasLayer(int layer, int x, int z)
{
    if (layer < 0 || layer > 6)
        ::renddx9abi::logf("[QuadPatchEditable.cpp:282] TBM: layer out of range");
    return records[layer].anyWeight(x, z);
}

// the same guard, then the record's quad cleared.
void PatchDetailLayers::clearLayerQuad(int layer, int x, int z)
{
    if (layer < 0 || layer > 6)
        ::renddx9abi::logf("[QuadPatchEditable.cpp:292] TBM: layer out of range");
    records[layer].clearQuad(x, z);
}

void PatchDetailLayers::freeAll()
{
    for (int i = 0; i < 6; ++i)
    {
        if (records[i].created && records[i].data)
        {
            ::renddx9abi::engineFree(records[i].data);
            records[i].data = 0;
        }
        records[i].created = 0;
    }
}

void PatchDetailLayers::createAll()
{
    for (int i = 0; i < 6; ++i)
        records[i].create();
}

// each map's created test (it only logs), then the map zeroed.
void PatchDetailLayers::clearAll()
{
    for (int i = 0; i < 6; ++i)
    {
        records[i].assertCreated();
        std::memset(records[i].data, 0, static_cast<unsigned int>(records[i].byteSize));
    }
}

// each map copied from the other set's, by this set's sizes.
void PatchDetailLayers::copyFrom(const PatchDetailLayers* other)
{
    for (int i = 0; i < 6; ++i)
        std::memcpy(records[i].data, other->records[i].data, static_cast<unsigned int>(records[i].byteSize));
}

// true once all six maps are created (it stops at the first that is not).
bool PatchDetailLayers::areAllCreated()
{
    int count = 0;
    for (const PatchDetailLayer* record = records; record->created; ++record)
        if (++count >= 6)
            return true;
    return false;
}


PatchDetailLayer* PatchDetailLayers::getLayer(int layer)
{
    if (layer < 0 || layer > 6)
        ::renddx9abi::logf("[QuadPatchEditable.cpp:256] TBM: layer out of range");
    return records + layer;
}


// MergedPatchDetailLayers - every query goes to `source` until this set's own maps are all created.

void MergedPatchDetailLayers::init(QuadPatchEditable*, int)
{
    ::renddx9abi::logf("[QuadPatchEditable.h:107] TBM: dont use this on MergedPatchDetailLayers");
}

bool MergedPatchDetailLayers::isLayerSet(int layer, int x, int z)
{
    if (areAllCreated())
        return (layerMask3(x, z) & static_cast<unsigned char>(1 << layer)) != 0;   // virtual (+0x08)
    return source->isLayerSet(layer, x, z);
}

unsigned char MergedPatchDetailLayers::layerMask3(int x, int z)
{
    if (areAllCreated())
        return PatchDetailLayers::layerMask3(x, z);
    return source->layerMask3(x, z);
}

unsigned char MergedPatchDetailLayers::dominantLayer(int x, int z)
{
    if (areAllCreated())
        return PatchDetailLayers::dominantLayer(x, z);
    return source->dominantLayer(x, z);
}

unsigned char MergedPatchDetailLayers::layerMask(int x, int z)
{
    if (areAllCreated())
        return PatchDetailLayers::layerMask(x, z);
    return source->layerMask(x, z);
}

bool MergedPatchDetailLayers::hasLayer(int layer, int x, int z)
{
    if (areAllCreated())
        return PatchDetailLayers::hasLayer(layer, x, z);
    return source->hasLayer(layer, x, z);
}

void MergedPatchDetailLayers::clearLayerQuad(int layer, int x, int z)
{
    if (areAllCreated())
    {
        PatchDetailLayers::clearLayerQuad(layer, x, z);
        return;
    }
    source->clearLayerQuad(layer, x, z);
}

PatchDetailLayer* MergedPatchDetailLayers::getLayer(int layer)
{
    if (areAllCreated())
        return PatchDetailLayers::getLayer(layer);
    return source->getLayer(layer);                                           // the base body on the source
}


// QuadPatchEditable

// QuadPatch's constructor, the owner twice (+0x04 through the base, +0x124 typed), the
// checker state, the name, the three layer sets (records created = 0, data = 0; the merged set's
// source is left to init), then the flags and the texture / buffer slots. +0x328 and the file
// indices are not initialised.
QuadPatchEditable::QuadPatchEditable(TerrainEditable* owner)
    : QuadPatch(owner),
      m_colormapEdit(0),
      editableTerrain(owner),
      m_checkerLastU(0.0f),
      m_checkerLastV(0.0f),
      m_checkerCellU(0.0f),
      m_checkerCellV(0.0f),
      m_checkerCountU(0),
      m_checkerCountV(0)
{
    m_importPending = 0;
    m_colormapEdited = 0;
    m_compDetailMap[0] = 0;
    m_compDetailMap[1] = 0;
    detailMap[0] = 0;
    detailMap[1] = 0;
    m_undergrowthOverlay = 0;
    m_overgrowthOverlay = 0;
    m_overgrowthShadowOverlay = 0;
    m_materialOverlay = 0;
    m_roadMask = 0;
    for (int i = 0; i < 6; ++i)
        m_layerIndexBuffer[i] = 0;
    for (int i = 0; i < 6; ++i)
        m_layerTriangleCount[i] = 0;
}

// (slot +0x00 is 0x101A1E60, which adds the free). The overlays, the edited colormap and
// the road mask (owner references only, not nulled), the six layer index buffers; then the members - the
// merged set, the edit set and the imported set, each freeing its six maps last to first - the name
// and ~QuadPatch. The +0x2DC compressed detail maps are never released.
QuadPatchEditable::~QuadPatchEditable()
{
    terrainReleaseTexture(m_undergrowthOverlay);                              // +0x2E4
    terrainReleaseTexture(m_overgrowthOverlay);                               // +0x2E8
    terrainReleaseTexture(m_overgrowthShadowOverlay);                         // +0x2EC
    terrainReleaseTexture(m_materialOverlay);                                 // +0x2F0
    terrainReleaseTexture(m_colormapEdit);                                    // +0x120
    terrainReleaseTexture(m_roadMask);                                        // +0x2F4
    for (int i = 0; i < 6; ++i)
        if (m_layerIndexBuffer[i])
            dx9rend::indexBufferRelease(m_layerIndexBuffer[i]);               // +0x2F8..
}

// The patch size is asked once (E+0x4C); both halvings are signed, as shipped.
void QuadPatchEditable::getFileIndices(int* x, int* z)
{
    TerrainEditable* const te = editableTerrain;
    const int patchSize = te->getPatchSize();                                 // E+0x4C
    *x = te->m_heightmapSizeX / patchSize / 2 + gridX;                        // +0x350, +0x68
    *z = te->m_heightmapSizeZ / patchSize / 2 + gridZ;                        // +0x354, +0x6C
}

// Places the patch at grid (x, z), loads its maps, and builds buffers and layer sets.
// Colormaps prefer the editor copy, then the game copy, then a checkerboard; lightmaps
// default to white. Missing detail maps are created and saved immediately.
// Temporarily enables managed textures. Loaded detail maps defer import until update().
void QuadPatchEditable::init(int x, int z)
{
    TerrainEditable* const te = editableTerrain;
    gridX = x;
    gridZ = z;
    worldX = x * te->m_patchSize;
    worldZ = z * te->m_patchSize;
    scaledX = static_cast<int>(te->m_primaryWorldScale.x) * worldX;
    scaledZ = static_cast<int>(te->m_primaryWorldScale.z) * worldZ;
    m_fileIndexX = x + te->m_patchCount / 2;
    m_fileIndexZ = z + te->m_patchCount / 2;
    formatString(m_fileName, "%02ix%02i", m_fileIndexX, m_fileIndexZ);

    // "<save path><editor colormap base><name>" - the editor base name (P+0xFC) is asked first.
    vc71::string editPath;
    {
        const vc71::string editorBase = te->getEditorColormapBaseName();
        editPath = ::renddx9abi::toVc71String(te->getLevelSavePath());       // E+0x90
        editPath += editorBase;
        editPath += m_fileName;
    }
    // "<save path><colormap base><name>" - the base name (E+0x6C) is asked first.
    vc71::string gamePath;
    {
        TerrainEditableNameResult colormapBase = te->getColormapBaseName();
        gamePath = ::renddx9abi::toVc71String(te->getLevelSavePath());
        gamePath += ::renddx9abi::toVc71String(colormapBase);
        gamePath += m_fileName;
    }

    const int savedManaged = dx9rend::textureManagerSwapManaged(1);
    vc71::string editDds(editPath);
    editDds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(editDds)))
    {
        colormap = dx9rend::textureManagerGetTexture(editPath);
        m_colormapEdited = 1;
    }
    else
    {
        vc71::string gameDds(gamePath);
        gameDds += ".dds";
        if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(gameDds)))
        {
            colormap = dx9rend::textureManagerGetTexture(gamePath);
        }
        else
        {
            colormap = dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(te->m_patchColormapSize),
                static_cast<unsigned int>(te->m_patchColormapSize), 21, 1, 0, 0);
            dx9rend::TextureLockRecord* const record = dx9rend::textureLock(engineTexture(colormap), 0, 0x7FFFFFFF);
            dx9rend::textureUnlock(engineTexture(colormap), record);
            IDirect3DTexture9* texture = 0;
            engineTexture(colormap)->baseTexture->QueryInterface(dx9rend::kIID_Texture9,
                                                                 reinterpret_cast<void**>(&texture));
            D3DXFillTexture(texture, checkerboardFill, &m_checkerLastU);      // state at +0x128
            m_colormapEdited = 1;
        }
    }

    m_colormapEdit = 0;
    m_undergrowthOverlay = 0;
    m_overgrowthOverlay = 0;
    m_overgrowthShadowOverlay = 0;
    m_materialOverlay = 0;

    // "<save path><lightmap base><name>" - the base name (E+0x74) is asked first.
    vc71::string lightPath;
    {
        TerrainEditableNameResult lightmapBase = te->getLightmapBaseName();
        lightPath = ::renddx9abi::toVc71String(te->getLevelSavePath());
        lightPath += ::renddx9abi::toVc71String(lightmapBase);
        lightPath += m_fileName;
    }
    vc71::string lightDds(lightPath);
    lightDds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(lightDds)))
    {
        lightmap = dx9rend::textureManagerGetTexture(lightPath);
    }
    else
    {
        lightmap = dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(te->m_patchColormapSize),
            static_cast<unsigned int>(te->m_patchColormapSize), 21, 1, 0, 0);
        fillTexture(engineTexture(lightmap), 0xFFFFFFFFu);
    }

    // "<save path><editor detail base><name>_1" / "_2" - the editor base name (P+0xF4) first.
    m_detailMapsFromFile = 0;
    vc71::string detailBase;
    {
        const vc71::string editorBase = te->getEditorDetailmapBaseName();
        detailBase = ::renddx9abi::toVc71String(te->getLevelSavePath());
        detailBase += editorBase;
        detailBase += m_fileName;
    }
    for (int i = 0; i < 2; ++i)
    {
        vc71::string path;
        formatString(path, "%s_%i", detailBase.c_str(), i + 1);
        vc71::string dds(path);
        dds += ".dds";
        if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
        {
            detailMap[i] = dx9rend::textureManagerGetTexture(path);
            m_detailMapsFromFile = 1;
        }
        else
        {
            detailMap[i] = dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(te->m_patchDetailmapSize),
                static_cast<unsigned int>(te->m_patchDetailmapSize), 21, 1, 0, 0);
            fillTexture(engineTexture(detailMap[i]), i ? 0xFF000000u : 0xFFFF0000u);
            dx9rend::textureManagerSaveTexture(engineTexture(detailMap[i]), path);
        }
    }

    lowDetailmap = dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(te->m_lowDetailmapSize),
        static_cast<unsigned int>(te->m_lowDetailmapSize), 21, 1, 0, 0);
    fillTexture(engineTexture(lowDetailmap), 0x00FF0000u);
    dx9rend::textureManagerSwapManaged(savedManaged);

    for (int i = 0; i < 6; ++i)
        m_layerIndexBuffer[i] = dx9rend::bufferManagerCreateIndexBuffer(
            static_cast<unsigned int>(6 * te->m_patchSize * te->m_patchSize), 8u, 101u);

    {
        const Vec3& scale = te->getPrimaryWorldScale();                        // E+0x34
        zero8C = 0.0f;
        zero94 = 0.0f;
        worldXf = static_cast<float>(x * te->m_patchSize) * scale.x;
        worldZf = scale.z * static_cast<float>(z * te->m_patchSize);
    }
    computeBounds();
    rebuildVertexBuffers();

    m_detailLayers.init(this, te->m_patchDetailmapSize);
    m_detailLayers.createAll();
    m_detailLayers.clearAll();
    m_detailLayersEdit.init(this, te->m_patchDetailmapSize);
    m_mergedLayers.source = &m_detailLayers;
    m_mergedLayers.PatchDetailLayers::init(this, te->m_patchDetailmapSize);   // the base init (the merged one refuses)
    if (m_detailMapsFromFile)
        m_importPending = 1;
}

// the height range (+0x98 / +0x9C, seeded FLT_MIN / FLT_MAX) and the bounding sphere
// (+0xA0 / +0xAC) of the patch's (patchSize + 1)^2 heightmap samples. The compares are the listing's:
// `max < h` raises, `!(min < h)` lowers. Scale and size are asked through ITerrainEditable.
void QuadPatchEditable::computeBounds()
{
    const float scale = editableTerrain->getPrimaryWorldScale().x;            // E+0x34
    const int side = editableTerrain->getPatchSize() + 1;                     // E+0x4C
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    Vec3* const points = static_cast<Vec3*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(12 * side * side)));
    if (points)
    {
        for (int i = 0; i < side * side; ++i)
        {
            points[i].x = 0.0f;
            points[i].y = 0.0f;
            points[i].z = 0.0f;
        }
    }
    maxHeight = FLT_MIN;
    minHeight = FLT_MAX;
    for (int row = 0; row < side; ++row)
    {
        Vec3* point = points + side * row;
        for (int col = 0; col < side; ++col)
        {
            const float worldZPos = static_cast<float>(worldZ + row) * scale;
            const float worldXPos = static_cast<float>(worldX + col) * scale;
            const float height = cluster->getHeightInWorldCoords(worldXPos, worldZPos);   // +0x6C
            if (maxHeight < height)
                maxHeight = height;
            if (!(minHeight < height))
                minHeight = height;
            point->x = static_cast<float>(worldX + col) * scale;
            point->y = height;
            point->z = static_cast<float>(worldZ + row) * scale;
            ++point;
        }
    }
    D3DXComputeBoundingSphere(reinterpret_cast<const D3DXVECTOR3*>(points), static_cast<DWORD>(side * side),
                              12, reinterpret_cast<D3DXVECTOR3*>(&sphereCentre), &sphereRadius);
    ::renddx9abi::engineFree(points);
}

// the per-patch streams: +0x54 the heights (4 bytes per sample, divided by the vertical
// scale) and a fresh +0x5C that buildNormalVertexBuffer fills. The old wrappers are released, not
// nulled. The x == -1024 re-sample (when the scaled height is below one unit) asks the cluster for
// the same coordinate again - -1024.0f (0xC4800000) is the column's own x.
void QuadPatchEditable::rebuildVertexBuffers()
{
    if (heightVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(heightVertexBuffer);
        dx9rend::vertexBufferRelease(heightVertexBuffer);
    }
    if (normalVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(normalVertexBuffer);
        dx9rend::vertexBufferRelease(normalVertexBuffer);
    }
    *reinterpret_cast<unsigned char*>(&unresolvedB0) = 1;                     // byte store at +0xB0

    const int side = editableTerrain->m_patchSize + 1;
    heightVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        static_cast<unsigned int>(side * side), 4u, 8u, "QuadPatchEditable");
    float* const heights = static_cast<float*>(dx9rend::vertexBufferLock(heightVertexBuffer, 0, 0, 0));
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const int step = static_cast<int>(editableTerrain->getPrimaryWorldScale().x);   // E+0x34
    const float scaleY = editableTerrain->getPrimaryWorldScale().y;                  // E+0x34 again
    const int zEnd = scaledZ + step * side;
    const int xEnd = scaledX + step * side;
    int next = 0;
    for (int z = scaledZ; z < zEnd; z += step)
    {
        const float zPos = static_cast<float>(z);
        const float inv = 1.0f / scaleY;
        for (int x = scaledX; x < xEnd; x += step)
        {
            float height = cluster->getHeightInWorldCoords(static_cast<float>(x), zPos) * inv;
            if (scaleY * height < 1.0f && x == -1024)
                height = cluster->getHeightInWorldCoords(-1024.0f, zPos) * inv;
            heights[next++] = height;
        }
    }
    dx9rend::vertexBufferUnlock(heightVertexBuffer);

    const int side2 = editableTerrain->m_patchSize + 1;
    normalVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        static_cast<unsigned int>(side2 * side2), 12u, 8u, "QuadPatchEditable");
    buildNormalVertexBuffer();
}

// (slot +0x04) - one patch under the technique TerrainEditable::drawPatches began.
// Techniques 0..2 and 7 draw the whole base grid; 4 / 5 / 6 / 8 the grid with their overlay as
// TEXLAYER0 (no overlay set for another technique draws nothing); 3 draws every detail layer with
// triangles, one pass each (1 side-projected, 2 environment-mapped, else 0). The technique is
// compared unsigned (a negative one draws nothing).
void QuadPatchEditable::draw(int)
{
    TerrainEditable* const te = editableTerrain;
    const unsigned int technique = static_cast<unsigned int>(te->m_shaderTechnique);
    if (technique == 0)
        param(te->m_hTexLayer[1])->setValue(lightmap, 0);
    if (editableTerrain->m_drawDetailLayerAsColor == -1)
        param(editableTerrain->m_hTexLayer[0])->setValue(colormap, 0);
    else
        param(editableTerrain->m_hTexLayer[0])->setValue(detailMap[editableTerrain->m_drawDetailLayerAsColor / 3], 0);
    {
        const Vec3& scale = editableTerrain->getPrimaryWorldScale();          // E+0x34
        float scaleTransXZ[4] = { scale.x, scale.z, worldXf, worldZf };
        param(editableTerrain->m_hScaleTransXZ)->setValue(scaleTransXZ, 0);
    }
    dx9rend::vertexBufferBindStream(heightVertexBuffer, 1);                   // +0x54
    dx9rend::vertexBufferBindStream(normalVertexBuffer, 2);                   // +0x5C

    if (technique > 2)
    {
        if (technique == 7)
        {
            param(editableTerrain->m_hTexLayer[2])->setValue(lightmap, 0);
        }
        else if (technique != 3)
        {
            void* overlay;
            switch (technique)
            {
            case 4: overlay = m_undergrowthOverlay; break;
            case 5: overlay = m_overgrowthOverlay; break;
            case 6: overlay = m_overgrowthShadowOverlay; break;
            case 8: overlay = m_materialOverlay; break;
            default: return;
            }
            param(editableTerrain->m_hTexLayer[0])->setValue(overlay, 0);
        }
        else
        {
            // Technique 3: the detail layers.
            param(editableTerrain->m_hTexLayer[4])->setValue(lightmap, 0);
            param(editableTerrain->m_hTexLayer[3])->setValue(editableTerrain->m_lowDetailTexture, 0);
            param(editableTerrain->m_hTexLayer[0])->setValue(m_colormapEdit ? static_cast<void*>(m_colormapEdit) : colormap, 0);
            param(editableTerrain->m_hTexLayer[5])->setValue(lowDetailmap, 0);
            for (int layer = 0; layer < 6; ++layer)
            {
                if (!m_layerTriangleCount[layer])
                    continue;
                const TerrainDetailTexture* const record =
                    reinterpret_cast<TerrainDetailTexture**>(editableTerrain->m_detailTextures.first)[layer];
                param(editableTerrain->m_hTexLayer[1])->setValue(record->texture, 0);
                void* detail;
                if (editableTerrain->m_useCompDetailMaps && m_compDetailMap[layer / 3])
                    detail = m_compDetailMap[layer / 3];
                else
                    detail = detailMap[layer / 3];
                param(editableTerrain->m_hTexLayer[2])->setValue(detail, 0);
                {
                    float selector[3] = { 0.0f, 0.0f, 0.0f };
                    selector[2 - layer % 3] = 1.0f;
                    dx9rend::effectGetShaderParameterBorrowedTyped(static_cast<dx9rend::Effect*>(editableTerrain->m_effect),
                                                                   vc71::string("COMPONENTSELECTOR"))->setValue(selector, 0);
                }
                {
                    float tiling[4] = { record->sideTiling.x, record->sideTiling.y, record->topTiling, record->yOffset };
                    dx9rend::effectGetShaderParameterBorrowedTyped(static_cast<dx9rend::Effect*>(editableTerrain->m_effect),
                                                                   vc71::string("NEARTEXTILING"))->setValue(tiling, 0);
                }
                dx9rend::indexBufferBind(m_layerIndexBuffer[layer]);
                unsigned int pass;
                if (record->planeMap)
                {
                    pass = 1;
                    param(editableTerrain->m_hTexLayer[1])->setValue(record->sideTexture, 0);
                }
                else if (record->envMap)
                {
                    pass = 2;
                    // The live EnvMapManager's borrowed texture (dword_10267A34 +0x64), unguarded.
                    dx9rend::effectGetShaderParameterBorrowedTyped(static_cast<dx9rend::Effect*>(editableTerrain->m_effect),
                                                                   vc71::string("TEXLAYER7"))->setValue(
                        static_cast<Rend::EnvMapManager*>(terrainEnvMapManager())->m_currentTexture, 0);
                }
                else
                {
                    pass = 0;
                }
                dx9rend::effectBeginPass(editableTerrain->m_effect, pass);
                const int sideVertices = editableTerrain->m_patchSize + 1;
                dx9rend::rendererDrawIndexedPrimitive(
                    static_cast<unsigned int>(m_layerTriangleCount[layer]), 4u, 0, 0,
                    static_cast<unsigned int>(sideVertices * sideVertices), 0);
            }
            return;
        }
    }

    // The whole patch over the base grid's LOD-0 range: 129^2 = 16641 vertices, as shipped.
    dx9rend::indexBufferBind(editableTerrain->m_lodIndexBuffer);              // +0xD8
    dx9rend::rendererDrawIndexedPrimitive(
        static_cast<unsigned int>(editableTerrain->m_lodRangeTriangleCount[0]),
        static_cast<unsigned int>(editableTerrain->m_primitiveType), 0, 0, 16641u, 0);
}

// all six layers, then the pending flag cleared.
void QuadPatchEditable::importAllLayers()
{
    for (int layer = 0; layer < 6; ++layer)
        importLayer(layer);
    m_importPending = 0;
}

// one detail-map channel (layer % 3 of map layer / 3) into the imported weight map:
// read-only lock of mip 0, the surface pitch is ignored (rows advance by `width` texels). When both
// the edit and the merged sets exist, the edit set is cleared and the merged set takes a copy of the
// imported one. Then the layer's index buffer.
void QuadPatchEditable::importLayer(int layer)
{
    const int width = editableTerrain->m_patchDetailmapSize;
    IDirect3DSurface9* const surface =
        dx9rend::textureGetLevelSurface(engineTexture(detailMap[layer / 3]), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT rect;
    surface->LockRect(&rect, 0, D3DLOCK_READONLY);
    const int shift = 8 * (layer % 3);
    if (width > 0)
    {
        const unsigned int* row = static_cast<const unsigned int*>(rect.pBits);
        int rowStart = 0;
        for (int r = 0; r < width; ++r)
        {
            const unsigned int* texel = row;
            for (int c = 0; c < width; ++c)
            {
                unsigned char* const weight = m_detailLayers.getLayer(layer)->at(c + rowStart);
                *weight = static_cast<unsigned char>(*texel++ >> shift);
            }
            rowStart += width;
            row += width;
        }
    }
    if (m_detailLayersEdit.areAllCreated() && m_mergedLayers.areAllCreated())
    {
        m_detailLayersEdit.clearAll();
        m_mergedLayers.copyFrom(&m_detailLayers);
    }
    surface->UnlockRect();
    surface->Release();
    rebuildLayerIndexBuffer(layer);
}

// two triangles per heightmap quad the merged set reports for `layer`, as u16 indices
// into the (patchSize + 1)^2 grid; the lock is not tested. The count is triangles (indices / 3).
void QuadPatchEditable::rebuildLayerIndexBuffer(int layer)
{
    const int n = editableTerrain->getPatchSize();                            // E+0x4C
    int count = 0;
    unsigned short* const indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_layerIndexBuffer[layer], 0, 0, 0));
    for (int z = 0; z < n; ++z)
    {
        for (int x = 0; x < n; ++x)
        {
            if (m_mergedLayers.hasLayer(layer, x, z))
            {
                const unsigned short row  = static_cast<unsigned short>((n + 1) * z);
                const unsigned short next = static_cast<unsigned short>((n + 1) * (z + 1));
                const unsigned short a = static_cast<unsigned short>(row + x);
                const unsigned short b = static_cast<unsigned short>(row + x + 1);
                const unsigned short c = static_cast<unsigned short>(next + x);
                const unsigned short d = static_cast<unsigned short>(next + x + 1);
                indices[count + 0] = a;
                indices[count + 1] = b;
                indices[count + 2] = c;
                indices[count + 3] = c;
                indices[count + 4] = b;
                indices[count + 5] = d;
                count += 6;
            }
        }
    }
    dx9rend::indexBufferUnlock(m_layerIndexBuffer[layer]);
    m_layerTriangleCount[layer] = count / 3;
}

// the low-detail map rebuilt from the detail maps: cleared, then every layer whose record
// says low-detail type 1 (into red, bits 16..23) or 2 (into blue, bits 0..7) adds its channel's
// weights, saturating at 255. Three surfaces locked (flags 0), unlocked and released in lock order.
void QuadPatchEditable::rebuildLowDetailMap()
{
    const int width = editableTerrain->m_patchDetailmapSize;
    fillTexture(engineTexture(lowDetailmap), 0u);
    IDirect3DSurface9* const lowSurface =
        dx9rend::textureGetLevelSurface(engineTexture(lowDetailmap), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT lowRect;
    lowSurface->LockRect(&lowRect, 0, 0);
    unsigned int* const target = static_cast<unsigned int*>(lowRect.pBits);
    IDirect3DSurface9* const surface0 =
        dx9rend::textureGetLevelSurface(engineTexture(detailMap[0]), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT rect0;
    surface0->LockRect(&rect0, 0, 0);
    IDirect3DSurface9* const surface1 =
        dx9rend::textureGetLevelSurface(engineTexture(detailMap[1]), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT rect1;
    surface1->LockRect(&rect1, 0, 0);
    const unsigned int* const sources[2] = {
        static_cast<const unsigned int*>(rect0.pBits), static_cast<const unsigned int*>(rect1.pBits) };

    for (int layer = 0; layer < 6; ++layer)
    {
        TerrainDetailTexture* const record =
            reinterpret_cast<TerrainDetailTexture**>(editableTerrain->m_detailTextures.first)[layer];
        const int type = record->getLowDetailType();                          // record slot +0x50
        if (type != 1 && type != 2)
            continue;
        const int shiftIn = 8 * (layer % 3);
        const int shiftOut = (type == 2) ? 0 : 16;
        const unsigned int* const source = sources[layer / 3];
        for (int k = 0; k < width * width; ++k)
        {
            const unsigned char value = static_cast<unsigned char>(source[k] >> shiftIn);
            if (!value)
                continue;
            int sum = value + static_cast<unsigned char>(target[k] >> shiftOut);
            if (sum >= 255)
                sum = 255;
            target[k] = (static_cast<unsigned int>(sum) << shiftOut) | (target[k] & ~(255u << shiftOut));
        }
    }

    lowSurface->UnlockRect();
    surface0->UnlockRect();
    surface1->UnlockRect();
    lowSurface->Release();
    surface0->Release();
    surface1->Release();
}

// (nullsub_12, `retn 4`) - empty in 1.50.
void QuadPatchEditable::debugTest(int)
{
}

// the technique overlays: patchSize^2, D3DFMT_A8R8G8B8 (21), one mip, cleared to 0
void QuadPatchEditable::createUndergrowthOverlay()
{
    if (m_undergrowthOverlay)
        return;
    const int size = editableTerrain->m_patchSize;
    m_undergrowthOverlay = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 1, 0, 0));
    fillTexture(m_undergrowthOverlay, 0u);
}

// The releases (0x1019C410 / 0x1019C4D0 / 0x1019C590 / 0x1019C5E0): the owner reference, then the same
// pointer value - released or not - out of the TextureManager's cache (sub_100191C0), then nulled.
void QuadPatchEditable::releaseUndergrowthOverlay()
{
    if (!m_undergrowthOverlay)
        return;
    terrainReleaseTexture(m_undergrowthOverlay);
    dx9rend::textureManagerEvictFromCache(m_undergrowthOverlay);
    m_undergrowthOverlay = 0;
}

void QuadPatchEditable::createOvergrowthOverlay()
{
    if (m_overgrowthOverlay)
        return;
    const int size = editableTerrain->m_patchSize;
    m_overgrowthOverlay = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 1, 0, 0));
    fillTexture(m_overgrowthOverlay, 0u);
}

void QuadPatchEditable::releaseOvergrowthOverlay()
{
    if (!m_overgrowthOverlay)
        return;
    terrainReleaseTexture(m_overgrowthOverlay);
    dx9rend::textureManagerEvictFromCache(m_overgrowthOverlay);
    m_overgrowthOverlay = 0;
}

void QuadPatchEditable::createOvergrowthShadowOverlay()
{
    if (m_overgrowthShadowOverlay)
        return;
    const int size = editableTerrain->m_patchSize;
    m_overgrowthShadowOverlay = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 1, 0, 0));
    fillTexture(m_overgrowthShadowOverlay, 0u);
}

void QuadPatchEditable::releaseOvergrowthShadowOverlay()
{
    if (!m_overgrowthShadowOverlay)
        return;
    terrainReleaseTexture(m_overgrowthShadowOverlay);
    dx9rend::textureManagerEvictFromCache(m_overgrowthShadowOverlay);
    m_overgrowthShadowOverlay = 0;
}

void QuadPatchEditable::releaseMaterialOverlay()
{
    if (!m_materialOverlay)
        return;
    terrainReleaseTexture(m_materialOverlay);
    dx9rend::textureManagerEvictFromCache(m_materialOverlay);
    m_materialOverlay = 0;
}


// Editing helpers return surfaces to the caller for release. Texture removal drops the
// owner reference before cache eviction, using the same pointer value for both operations.
// Keep float precision (RW-240); evaluate cosine in double before narrowing.

namespace {

// dword_1023AB28 - the material overlay's palette for materials 1..15 (index 0 is never read: a
// zero material takes the grey arm). A8R8G8B8, read from the image bytes.
const unsigned int kMaterialPalette[16] =
{
    0xFF000000u, 0xFF0000FFu, 0xFF00FFFFu, 0xFF00FF00u, 0xFFFFFF00u, 0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFFFFu,
    0xFF000077u, 0xFF007777u, 0xFF007700u, 0xFF777700u, 0xFF770000u, 0xFF770077u, 0xFF777777u, 0xFF333333u,
};

// A locked rect's row length in texels: `Pitch >> 2`, unsigned as shipped (`shr`).
inline int texelPitch(const D3DLOCKED_RECT& locked)
{
    return static_cast<int>(static_cast<unsigned int>(locked.Pitch) >> 2);
}

// sub_100A2FE0 - ceil of a float through the CRT's double ceil (`fld; fstp qword; call ceil`), the
// result left in st0 for the caller's chop.
inline int ceilChop(float value)
{
    return static_cast<int>(std::ceil(static_cast<double>(value)));
}

} // anonymous namespace

// the colour helpers (QuadPatchEditable.h)
// (`retn 10h`, four floats BY value). Each * 255.0f (flt_101E14C0), chopped (fistp under a
// truncating control word), the low byte kept - the third first, then the fourth unmasked (only its
// low byte survives the two shifts that follow), the second, the first.
unsigned int terrainEditPackColor(float c0, float c1, float c2, float c3)
{
    unsigned int packed = static_cast<unsigned int>(static_cast<int>(c2 * 255.0f)) & 0xFFu;
    packed |= static_cast<unsigned int>(static_cast<int>(c3 * 255.0f)) << 8;
    packed <<= 8;
    packed |= static_cast<unsigned int>(static_cast<int>(c1 * 255.0f)) & 0xFFu;
    packed <<= 8;
    packed |= static_cast<unsigned int>(static_cast<int>(c0 * 255.0f)) & 0xFFu;
    return packed;
}

// (ecx = out, edx = colour) - each byte, the lowest first, * 0.0039215689f (flt_101E2B98).
void terrainEditUnpackColor(float out[4], unsigned int color)
{
    out[0] = static_cast<float>(color & 0xFFu) * 0.0039215689f;
    out[1] = static_cast<float>((color >> 8) & 0xFFu) * 0.0039215689f;
    out[2] = static_cast<float>((color >> 16) & 0xFFu) * 0.0039215689f;
    out[3] = static_cast<float>(color >> 24) * 0.0039215689f;
}

// (thiscall on dst, `retn 8`) - dst scaled by (1 - t) and stored, all four first; then
// t * src computed for all four and added.
void terrainEditLerpColor(float dst[4], const float src[4], float t)
{
    const float keep = 1.0f - t;
    dst[0] = keep * dst[0];
    dst[1] = keep * dst[1];
    dst[2] = keep * dst[2];
    dst[3] = keep * dst[3];
    const float add0 = t * src[0];
    const float add1 = t * src[1];
    const float add2 = t * src[2];
    const float add3 = t * src[3];
    dst[0] = dst[0] + add0;
    dst[1] = dst[1] + add1;
    dst[2] = dst[2] + add2;
    dst[3] = dst[3] + add3;
}

// the colormap's edit copy
// only when there is none: a patchColormapSize^2 A8R8G8B8 texture (one mip), stored
// first, then the colormap copied into it by D3DX (no rectangles, D3DX_DEFAULT filter, no colour key;
// the answer not tested) and both surfaces released, the copy's first.
void QuadPatchEditable::makeColormapEditCopy()
{
    if (m_colormapEdit)
        return;
    m_colormapEdit = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(editableTerrain->m_patchColormapSize),
        static_cast<unsigned int>(editableTerrain->m_patchColormapSize), 21, 1, 0, 0));
    IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
    destination->Release();
    source->Release();
}

// the copy's owner reference, then the member's value out of the cache, then nulled.
void QuadPatchEditable::releaseColormapEdit()
{
    if (!m_colormapEdit)
        return;
    terrainReleaseTexture(m_colormapEdit);
    dx9rend::textureManagerEvictFromCache(m_colormapEdit);
    m_colormapEdit = 0;
}

// with a copy: the copy loaded back over the colormap (D3DX, as above), the copy's surface
// released before the colormap's, then the copy dropped (a tail jump into 0x1019C350).
void QuadPatchEditable::commitColormapEdit()
{
    if (!m_colormapEdit)
        return;
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
    source->Release();
    destination->Release();
    releaseColormapEdit();
}

// with a copy: every texel of it (patchColormapSize^2, x outer, z inner, the pitch
// honoured) set to 0, under a (0, 0, size, size) lock with flags 0.
void QuadPatchEditable::clearColormapEdits()
{
    if (!m_colormapEdit)
        return;
    const int size = editableTerrain->m_patchColormapSize;
    RECT rect = { 0, 0, size, size };
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, &rect, 0);
    unsigned int* const bits = static_cast<unsigned int*>(locked.pBits);
    for (int x = 0; x < size; ++x)
        for (int z = 0; z < size; ++z)
            bits[x + z * texelPitch(locked)] = 0;
    surface->UnlockRect();
    surface->Release();
}

// the detail layers
// the layer's channel cleared in its detail map, then ALL SIX imported weight maps
// zeroed (+0x164 clearAll), the edit and merged sets freed, and the layer's triangle count reset
// (its index buffer is not rebuilt). Lock (0, 0, w, w), flags 0; the rows advance by `w` texels -
// the surface pitch is ignored.
void QuadPatchEditable::clearDetailLayer(int layer)
{
    const int width = editableTerrain->m_patchDetailmapSize;
    const int channel = layer % 3;
    IDirect3DSurface9* const surface =
        dx9rend::textureGetLevelSurface(engineTexture(detailMap[layer / 3]), 0, 0x7FFFFFFF);
    RECT rect = { 0, 0, editableTerrain->m_patchDetailmapSize, editableTerrain->m_patchDetailmapSize };
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, &rect, 0);
    if (width > 0)
    {
        const unsigned int mask = ~(255u << (8 * channel));
        unsigned int* row = static_cast<unsigned int*>(locked.pBits);
        for (int r = 0; r < width; ++r, row += width)
        {
            unsigned int* texel = row;
            for (int c = 0; c < width; ++c)
                *texel++ &= mask;
        }
    }
    m_detailLayers.clearAll();                                                // +0x164 slot +0x24
    m_detailLayersEdit.freeAll();                                             // +0x1E0 slot +0x1C
    m_mergedLayers.freeAll();                                                 // +0x25C slot +0x1C
    surface->UnlockRect();
    surface->Release();
    m_layerTriangleCount[layer] = 0;
}

void QuadPatchEditable::clearAllDetailLayers()
{
    for (int layer = 0; layer < 6; ++layer)
        clearDetailLayer(layer);
}

// the edit and merged sets freed, then the layer's channel of its detail map rewritten
// from the imported weights (texel = (weight << shift) | (texel & ~(255 << shift)); lock (0, 0, w, w),
// flags 0, rows advance by `w` texels), and the layer's index buffer rebuilt.
void QuadPatchEditable::revertLayer(int layer)
{
    const int width = editableTerrain->m_patchDetailmapSize;
    m_detailLayersEdit.freeAll();
    m_mergedLayers.freeAll();
    const int channel = layer % 3;
    IDirect3DSurface9* const surface =
        dx9rend::textureGetLevelSurface(engineTexture(detailMap[layer / 3]), 0, 0x7FFFFFFF);
    RECT rect = { 0, 0, width, width };
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, &rect, 0);
    const int shift = 8 * channel;
    if (width > 0)
    {
        const unsigned int mask = ~(255u << shift);
        unsigned int* row = static_cast<unsigned int*>(locked.pBits);
        int rowStart = 0;
        for (int r = 0; r < width; ++r, rowStart += width, row += width)
        {
            unsigned int* texel = row;
            for (int c = 0; c < width; ++c, ++texel)
            {
                const unsigned int weight = *m_detailLayers.getLayer(layer)->at(c + rowStart);
                *texel = (*texel & mask) | (weight << shift);
            }
        }
    }
    surface->UnlockRect();
    surface->Release();
    rebuildLayerIndexBuffer(layer);
}

void QuadPatchEditable::revertAllLayers()
{
    for (int layer = 0; layer < 6; ++layer)
        revertLayer(layer);
}

// only for an edited colormap (+0x32A) whose edit and merged sets both exist: the merged
// weights copied into the imported set (+0x164 slot +0x28 with the merged set - its own maps, not the
// forwarders), each set freed when it still exists (asked again), then the low-detail map rebuilt.
// The argument is not read.
void QuadPatchEditable::commitLayer(int)
{
    if (!m_colormapEdited)
        return;
    if (!m_detailLayersEdit.areAllCreated())
        return;
    if (!m_mergedLayers.areAllCreated())
        return;
    m_detailLayers.copyFrom(&m_mergedLayers);
    if (m_detailLayersEdit.areAllCreated())
        m_detailLayersEdit.freeAll();
    if (m_mergedLayers.areAllCreated())
        m_mergedLayers.freeAll();
    rebuildLowDetailMap();
}

void QuadPatchEditable::commitAllLayers()
{
    for (int layer = 0; layer < 6; ++layer)
        commitLayer(layer);
}

// the merged weights of [x0, x1) x [z0, z1) written into both detail maps: map i gets
// (w(3i + 2) << 16) + (w(3i + 1) << 8) + w(3i) - alpha 0 - at texel (x, z), the pitch honoured; the
// weight index is x + z * w (x outer, z inner). Both maps locked over (0, 0, w, w) with flags 0 first,
// unlocked and released in map order last. The layer argument is not read.
void QuadPatchEditable::writeLayersBack(int x0, int x1, int z0, int z1, int)
{
    const int width = editableTerrain->m_patchDetailmapSize;
    RECT rect = { 0, 0, width, width };
    IDirect3DSurface9* surfaces[2];
    D3DLOCKED_RECT locked[2];
    for (int i = 0; i < 2; ++i)
    {
        surfaces[i] = dx9rend::textureGetLevelSurface(engineTexture(detailMap[i]), 0, 0x7FFFFFFF);
        surfaces[i]->LockRect(&locked[i], &rect, 0);
    }
    for (int x = x0; x < x1; ++x)
    {
        int position = x + z0 * width;
        for (int z = z0; z < z1; ++z, position += width)
        {
            for (int i = 0; i < 2; ++i)
            {
                unsigned int* const texel =
                    static_cast<unsigned int*>(locked[i].pBits) + x + z * texelPitch(locked[i]);
                const unsigned int high   = *m_mergedLayers.getLayer(3 * i + 2)->at(position);
                const unsigned int middle = *m_mergedLayers.getLayer(3 * i + 1)->at(position);
                const unsigned int low    = *m_mergedLayers.getLayer(3 * i)->at(position);
                *texel = low + ((middle + (high << 8)) << 8);
            }
        }
    }
    for (int i = 0; i < 2; ++i)
    {
        surfaces[i]->UnlockRect();
        surfaces[i]->Release();
    }
}

// Adjusts edit weights with saturation, then merges the imported weights to total 255.
// All-zero imported weights can reach unchecked division when the edit total is below 255.
void QuadPatchEditable::addLayerWeight(int layer, int position, int amount)
{
    int sum = 0;
    if (amount >= 0)
    {
        unsigned char* const cell = m_detailLayersEdit.getLayer(layer)->at(position);
        int value = amount + *m_detailLayersEdit.getLayer(layer)->at(position);
        if (value > 255)
            value = 255;
        *cell = static_cast<unsigned char>(value);
        sum = static_cast<unsigned char>(value);
    }
    else
    {
        for (int i = 0; i < 6; ++i)
        {
            unsigned char* const cell = m_detailLayersEdit.getLayer(i)->at(position);
            const int raw = amount + *m_detailLayersEdit.getLayer(i)->at(position);
            const unsigned char value = static_cast<unsigned char>(raw <= 0 ? 0 : raw);
            sum += value;
            *cell = value;
        }
    }

    bool full = false;
    if (amount > 0)
    {
        if (sum == 255)
        {
            for (int i = 0; i < 6; ++i)
                if (i != layer)
                    *m_detailLayersEdit.getLayer(i)->at(position) = 0;
            full = true;
        }
        else
        {
            int others = 0;
            for (int i = 0; i < 6; ++i)
                if (i != layer)
                    others += *m_detailLayersEdit.getLayer(i)->at(position);
            if (others && others + sum > 255)
            {
                const int remaining = 255 - sum;
                for (int i = 0; i < 6; ++i)
                {
                    if (i == layer)
                        continue;
                    PatchDetailLayer* const record = m_detailLayersEdit.getLayer(i);  // asked twice, as shipped
                    unsigned char* const cell = m_detailLayersEdit.getLayer(i)->at(position);
                    const int product = *record->at(position) * remaining;
                    *cell = static_cast<unsigned char>(static_cast<__int64>(
                        static_cast<float>(product) / static_cast<float>(others)));
                }
            }
        }
    }

    if (full || sum == 255)
    {
        *m_mergedLayers.getLayer(layer)->at(position) = 255;
        for (int i = 0; i < 6; ++i)
            if (i != layer)
                *m_mergedLayers.getLayer(i)->at(position) = 0;
        return;
    }

    int mergedSum = 0;
    for (int i = 0; i < 6; ++i)
    {
        unsigned char* const cell = m_mergedLayers.getLayer(i)->at(position);
        const unsigned char value = *m_detailLayersEdit.getLayer(i)->at(position);
        mergedSum += value;
        *cell = value;
    }
    if (!mergedSum)
    {
        for (int i = 0; i < 6; ++i)
        {
            PatchDetailLayer* const imported = m_detailLayers.getLayer(i);
            unsigned char* const cell = m_mergedLayers.getLayer(i)->at(position);
            *cell = *imported->at(position);
        }
        return;
    }
    if (mergedSum >= 255)
        return;
    const int remaining = 255 - mergedSum;
    int importedSum = 0;
    for (int i = 0; i < 6; ++i)
        importedSum += *m_detailLayers.getLayer(i)->at(position);
    const float importedSumF = static_cast<float>(importedSum);
    for (int i = 0; i < 6; ++i)
    {
        unsigned char* const cell = m_mergedLayers.getLayer(i)->at(position);
        const int product = *m_detailLayers.getLayer(i)->at(position) * remaining;
        *cell = static_cast<unsigned char>(*cell + static_cast<unsigned char>(static_cast<__int64>(
            static_cast<float>(product) / importedSumF)));
    }
}

// the brushes
// (nullsub_11, `retn 10h`) - empty in 1.50: TerrainEditable::brushHeightmapPatches reaches
// every patch under the brush and nothing happens here.
void QuadPatchEditable::brushHeightmap(float, float, float, float)
{
}

// Paints detail weights with cosine falloff and rebuilds layer index buffers.
// Requires brush overlap and an edited colormap. The brush grid uses colormap size;
// detail weights use detail-map size, so these dimensions must agree.
void QuadPatchEditable::paintDetail(float x, float z, float radius, int strength, int layer)
{
    TerrainEditable* const te = editableTerrain;
    const float xr = x - static_cast<float>(scaledX);                         // +0x78
    const float zr = z - static_cast<float>(scaledZ);                         // +0x7C
    if (!(xr + radius > 0.0f))
        return;
    const float extent = static_cast<float>(static_cast<int>(
        static_cast<float>(te->m_patchSize) * te->m_primaryWorldScale.x));
    if (!(xr - radius < extent) || !(zr + radius > 0.0f) || !(zr - radius < extent)
        || !m_colormapEdited)
        return;
    if (!m_colormapEdit)
        makeColormapEditCopy();

    const int size = te->m_patchColormapSize;                                 // +0x48 (sic)
    const float scale = static_cast<float>(size) / extent;
    const float xs = xr * scale;
    const float zs = zr * scale;
    const float rs = scale * radius;
    int x0 = static_cast<int>(xs - rs);
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
        const int top = ceilChop(xs + rs);
        if (size >= top)
            xEnd = top;
    }
    int z0 = static_cast<int>(zs - rs);
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
        const int top = ceilChop(zs + rs);
        if (size >= top)
            zEnd = top;
    }
    const int width = te->m_patchDetailmapSize;                               // +0x44
    if (!m_detailLayersEdit.areAllCreated())
    {
        m_detailLayersEdit.createAll();
        m_detailLayersEdit.clearAll();
    }
    if (!m_mergedLayers.areAllCreated())
    {
        m_mergedLayers.createAll();
        m_mergedLayers.clearAll();
        m_mergedLayers.copyFrom(&m_detailLayers);
    }

    for (int px = x0; px < xEnd; ++px)
    {
        int position = px + z0 * width;
        for (int pz = z0; pz < zEnd; ++pz, position += width)
        {
            const float dx = static_cast<float>(px) - xs;
            const float dz = static_cast<float>(pz) - zs;
            const float distance = std::sqrt(dz * dz + dx * dx);
            if (distance > rs)
                continue;
            const float inverse = 1.0f / rs;
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));   // flt_1022106C
            if (!(0.0 < falloff))
                falloff = 0.0;
            const int amount = static_cast<int>(static_cast<float>(static_cast<double>(strength) * falloff));
            if (!amount)
                continue;
            addLayerWeight(layer, position, amount);
        }
    }
    writeLayersBack(x0, xEnd, z0, zEnd, layer);
    for (int i = 0; i < 6; ++i)
        rebuildLayerIndexBuffer(i);
}

// (`retn 14h`) - the colour brush: the same overlap test, edited-colormap gate, edit copy
// and texel window as paintDetail (colormap-sized). The copy (+0x120) is locked first, then the
// colormap, both whole with flags 0. Per texel within the radius: t = |cos weight * strength / 255|;
// a positive strength lerps the copy's texel toward the colour, a negative one back toward the
// colormap's texel by |t|, a zero strength stores 0. The colour is R/B-swapped before use
// ({c2, c1, c0, c3} of its bytes, low first): an argument 0x00BBGGRR paints red RR. Both surfaces are
// unlocked and released, the colormap's first.
void QuadPatchEditable::paintColor(float x, float z, float radius, int strength, unsigned int color)
{
    TerrainEditable* const te = editableTerrain;
    const float xr = x - static_cast<float>(scaledX);
    const float zr = z - static_cast<float>(scaledZ);
    const int extentInt = static_cast<int>(static_cast<float>(te->m_patchSize) * te->m_primaryWorldScale.x);
    if (!(xr + radius > 0.0f))
        return;
    const float extent = static_cast<float>(extentInt);
    if (!(xr - radius < extent) || !(zr + radius > 0.0f) || !(zr - radius < extent)
        || !m_colormapEdited)
        return;
    if (!m_colormapEdit)
        makeColormapEditCopy();

    const int size = te->m_patchColormapSize;
    const float scale = static_cast<float>(size) / extent;
    const float xs = xr * scale;
    const float zs = zr * scale;
    const float rs = scale * radius;
    const float inverse = 1.0f / rs;
    int x0 = static_cast<int>(xs - rs);
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
        const int top = ceilChop(xs + rs);
        if (size >= top)
            xEnd = top;
    }
    int z0 = static_cast<int>(zs - rs);
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
        const int top = ceilChop(zs + rs);
        if (size >= top)
            zEnd = top;
    }

    IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT editLocked;
    editSurface->LockRect(&editLocked, 0, 0);
    IDirect3DSurface9* const colormapSurface =
        dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT colormapLocked;
    colormapSurface->LockRect(&colormapLocked, 0, 0);
    unsigned int* const editBits = static_cast<unsigned int*>(editLocked.pBits);
    const unsigned int* const colormapBits = static_cast<const unsigned int*>(colormapLocked.pBits);

    for (int px = x0; px < xEnd; ++px)
    {
        const float dx = static_cast<float>(px) - xs;
        const float dxSquared = dx * dx;
        for (int pz = z0; pz < zEnd; ++pz)
        {
            const float dz = static_cast<float>(pz) - zs;
            const float distance = std::sqrt(dz * dz + dxSquared);
            if (distance > rs)
                continue;
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            float colour[4];
            terrainEditUnpackColor(colour, color);
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));
            if (!(0.0 < falloff))
                falloff = 0.0;
            const float target[4] = { colour[2], colour[1], colour[0], colour[3] };
            const float t = std::fabs(static_cast<float>(falloff * static_cast<double>(strength)) * 0.0039215689f);
            float result[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float current[4];
            terrainEditUnpackColor(current, *edit);
            float base[4];
            terrainEditUnpackColor(base, *original);
            if (strength > 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, target, t);
            }
            else if (strength < 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, base, std::fabs(t));
            }
            *edit = terrainEditPackColor(result[0], result[1], result[2], result[3]);
        }
    }
    colormapSurface->UnlockRect();
    colormapSurface->Release();
    editSurface->UnlockRect();
    editSurface->Release();
}

// Blends the editable colormap toward a layer's tiled color texture for positive strength,
// or back toward the original colormap for negative strength. The layer texture must exist.
// Negative v wrapping intentionally uses texture width rather than height.
void QuadPatchEditable::paintColorFromTexture(float x, float z, float radius, int strength, int layer)
{
    TerrainEditable* const te = editableTerrain;
    const float xr = x - static_cast<float>(scaledX);
    const float zr = z - static_cast<float>(scaledZ);
    const int extentInt = static_cast<int>(static_cast<float>(te->m_patchSize) * te->m_primaryWorldScale.x);
    if (!(xr + radius > 0.0f))
        return;
    const float extent = static_cast<float>(extentInt);
    if (!(xr - radius < extent) || !(zr + radius > 0.0f) || !(zr - radius < extent)
        || !m_colormapEdited)
        return;
    if (!m_colormapEdit)
        makeColormapEditCopy();

    const int size = te->m_patchColormapSize;
    const float scale = static_cast<float>(size) / extent;
    const float xs = xr * scale;
    const float zs = zr * scale;
    const float rs = scale * radius;
    const float inverse = 1.0f / rs;
    int x0 = static_cast<int>(xs - rs);
    if (x0 <= 0)
        x0 = 0;
    int xEnd = size;
    {
        const int top = ceilChop(xs + rs);
        if (size >= top)
            xEnd = top;
    }
    int z0 = static_cast<int>(zs - rs);
    if (z0 <= 0)
        z0 = 0;
    int zEnd = size;
    {
        const int top = ceilChop(zs + rs);
        if (size >= top)
            zEnd = top;
    }

    IDirect3DSurface9* const colormapSurface =
        dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF);
    D3DLOCKED_RECT colormapLocked;
    colormapSurface->LockRect(&colormapLocked, 0, 0);
    IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT editLocked;
    editSurface->LockRect(&editLocked, 0, 0);
    const TerrainDetailTextureEditable* const record = static_cast<const TerrainDetailTextureEditable*>(
        reinterpret_cast<TerrainDetailTexture**>(te->m_detailTextures.first)[layer]);
    const int textureHeight = static_cast<int>(dx9rend::textureDims(record->colorTexture)[1]);
    const int textureWidth = static_cast<int>(dx9rend::textureDims(record->colorTexture)[0]);
    IDirect3DSurface9* const textureSurface = dx9rend::textureGetLevelSurface(record->colorTexture, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT textureLocked;
    textureSurface->LockRect(&textureLocked, 0, 0);
    const unsigned int* const colormapBits = static_cast<const unsigned int*>(colormapLocked.pBits);
    unsigned int* const editBits = static_cast<unsigned int*>(editLocked.pBits);
    const unsigned int* const textureBits = static_cast<const unsigned int*>(textureLocked.pBits);

    for (int px = x0; px < xEnd; ++px)
    {
        const float pxF = static_cast<float>(px);
        const float dx = pxF - xs;
        const float dxSquared = dx * dx;
        for (int pz = z0; pz < zEnd; ++pz)
        {
            const float pzF = static_cast<float>(pz);
            const float dz = pzF - zs;
            const float distance = std::sqrt(dz * dz + dxSquared);
            if (distance > rs)
                continue;
            const float inverseScale = 1.0f / scale;
            int u = static_cast<int>(std::floor(static_cast<double>(
                (inverseScale * pxF + static_cast<float>(scaledX)) * record->colorTextureTiling.x + 0.5f)));
            u = u % textureWidth;
            if (u < 0)
                u += textureWidth;
            int v = static_cast<int>(std::floor(static_cast<double>(
                (inverseScale * pzF + static_cast<float>(scaledZ)) * record->colorTextureTiling.y + 0.5f)));
            v = v % textureHeight;
            if (v < 0)
                v += textureWidth;                                            // the width (shipped)
            double falloff = std::cos(static_cast<double>(distance * inverse * 1.5707964f));
            const unsigned int* const source = textureBits + u + v * texelPitch(textureLocked);
            const unsigned int* const original = colormapBits + px + pz * texelPitch(colormapLocked);
            unsigned int* const edit = editBits + px + pz * texelPitch(editLocked);
            if (!(0.0 < falloff))
                falloff = 0.0;
            const float t = std::fabs(static_cast<float>(falloff * static_cast<double>(strength)) * 0.0039215689f);
            float paint[4];
            terrainEditUnpackColor(paint, *source);
            float result[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float current[4];
            terrainEditUnpackColor(current, *edit);
            float base[4];
            terrainEditUnpackColor(base, *original);
            if (strength > 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, paint, t);
            }
            else if (strength < 0)
            {
                result[0] = current[0]; result[1] = current[1]; result[2] = current[2]; result[3] = current[3];
                terrainEditLerpColor(result, base, std::fabs(t));
            }
            *edit = terrainEditPackColor(result[0], result[1], result[2], result[3]);
        }
    }
    colormapSurface->UnlockRect();
    colormapSurface->Release();
    editSurface->UnlockRect();
    editSurface->Release();
    textureSurface->UnlockRect();
    textureSurface->Release();
}

// the tile imports (TerrainEditable's import slots, after tgasplit)
// the base detail map from "<directory>/tx<name>.tga": converted by the terrain's
// makeUncompDDS (E+0xFC) into `directory`; when "<tile>.dds" then exists, detail map 0 is dropped
// (owner release + evict) and reloaded from it, and layers 0..2 re-imported (0x1019D2D0). The .tga is
// deleted either way (FileManager +0x20).
void QuadPatchEditable::importBaseLayers(const vc71::string& directory)
{
    vc71::string tile(directory);
    tile += "/tx";
    tile += m_fileName;
    {
        vc71::string tga(tile);
        tga += ".tga";
        editableTerrain->makeUncompDDS(::renddx9abi::toHostString(tga), ::renddx9abi::toHostString(directory));
    }
    vc71::string dds(tile);
    dds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
    {
        terrainReleaseTexture(detailMap[0]);
        dx9rend::textureManagerEvictFromCache(detailMap[0]);
        detailMap[0] = dx9rend::textureManagerGetTexture(tile);
        importLayer(0);
        importLayer(1);
        importLayer(2);
    }
    vc71::string tga(tile);
    tga += ".tga";
    ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(tga));
}

// one layer from "<directory>/tx<name>.tga" (converted into "<directory>/" - with the
// slash, unlike the other two imports): when the .dds exists, the edit and merged sets are made (as
// the detail brush makes them), the converted texture is loaded and locked (0, 0, w, w) with flags 0,
// and every texel whose byte (layer % 3) is not zero is added to the layer as a weight (the brush's
// arithmetic, rows advance by `w` texels); the texture is dropped (owner release + evict) before its
// surface is unlocked and released, then the whole patch is written back and all six index buffers
// rebuilt. The .tga is deleted either way.
void QuadPatchEditable::importLayerTile(int layer, const vc71::string& directory)
{
    const int width = editableTerrain->m_patchDetailmapSize;
    vc71::string tile(directory);
    tile += "/tx";
    tile += m_fileName;
    vc71::string outputDirectory;
    formatString(outputDirectory, "%s/", directory.c_str());
    {
        vc71::string tga(tile);
        tga += ".tga";
        editableTerrain->makeUncompDDS(::renddx9abi::toHostString(tga), ::renddx9abi::toHostString(outputDirectory));
    }
    vc71::string dds(tile);
    dds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
    {
        if (!m_detailLayersEdit.areAllCreated())
        {
            m_detailLayersEdit.createAll();
            m_detailLayersEdit.clearAll();
        }
        if (!m_mergedLayers.areAllCreated())
        {
            m_mergedLayers.createAll();
            m_mergedLayers.clearAll();
            m_mergedLayers.copyFrom(&m_detailLayers);
        }
        dx9rend::EngineTexture* const texture = engineTexture(dx9rend::textureManagerGetTexture(tile));
        IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(texture, 0, 0x7FFFFFFF);
        RECT rect = { 0, 0, editableTerrain->m_patchDetailmapSize, editableTerrain->m_patchDetailmapSize };
        D3DLOCKED_RECT locked;
        surface->LockRect(&locked, &rect, 0);
        const int shift = 8 * (layer % 3);
        if (width > 0)
        {
            const unsigned int* row = static_cast<const unsigned int*>(locked.pBits);
            int rowStart = 0;
            for (int r = 0; r < width; ++r, rowStart += width, row += width)
            {
                const unsigned int* texel = row;
                for (int c = 0; c < width; ++c, ++texel)
                {
                    const unsigned char amount = static_cast<unsigned char>(*texel >> shift);
                    if (!amount)
                        continue;
                    addLayerWeight(layer, rowStart + c, amount);
                }
            }
        }
        terrainReleaseTexture(texture);
        dx9rend::textureManagerEvictFromCache(texture);
        surface->UnlockRect();
        surface->Release();
        writeLayersBack(0, width, 0, width, layer);
        for (int i = 0; i < 6; ++i)
            rebuildLayerIndexBuffer(i);
    }
    vc71::string tga(tile);
    tga += ".tga";
    ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(tga));
}

// the colormap from "<directory>/tx<name>.tga" (converted into `directory`): when the
// .dds exists the colormap is dropped (owner release + evict) and reloaded from it, the patch marked
// edited (+0x32A) and its edit copy cleared. The .tga is deleted either way.
void QuadPatchEditable::importColormapTile(const vc71::string& directory)
{
    vc71::string tile(directory);
    tile += "/tx";
    tile += m_fileName;
    {
        vc71::string tga(tile);
        tga += ".tga";
        editableTerrain->makeUncompDDS(::renddx9abi::toHostString(tga), ::renddx9abi::toHostString(directory));
    }
    vc71::string dds(tile);
    dds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
    {
        terrainReleaseTexture(colormap);
        dx9rend::textureManagerEvictFromCache(colormap);
        colormap = dx9rend::textureManagerGetTexture(tile);
        m_colormapEdited = 1;
        clearColormapEdits();
    }
    vc71::string tga(tile);
    tga += ".tga";
    ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(tga));
}

// lightmaps
// the lightmap dropped (owner release + evict, nulled) and reloaded under a managed-flag
// swap to 1 (sub_10017E20): "<save path><lightmap base><name>" - the base name (E+0x74) asked before
// the save path (E+0x90) - else a patchColormapSize^2 A8R8G8B8 texture filled white.
void QuadPatchEditable::reloadLightmap()
{
    TerrainEditable* const te = editableTerrain;
    if (lightmap)
    {
        terrainReleaseTexture(lightmap);
        dx9rend::textureManagerEvictFromCache(lightmap);
        lightmap = 0;
    }
    const int savedManaged = dx9rend::textureManagerSwapManaged(1);
    vc71::string path;
    {
        TerrainEditableNameResult lightmapBase = te->getLightmapBaseName();   // E+0x74
        path = ::renddx9abi::toVc71String(te->getLevelSavePath());           // E+0x90
        path += ::renddx9abi::toVc71String(lightmapBase);
        path += m_fileName;
    }
    vc71::string dds(path);
    dds += ".dds";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds)))
    {
        lightmap = dx9rend::textureManagerGetTexture(path);
    }
    else
    {
        lightmap = dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(te->m_patchColormapSize),
            static_cast<unsigned int>(te->m_patchColormapSize), 21, 1, 0, 0);
        fillTexture(engineTexture(lightmap), 0xFFFFFFFFu);
    }
    dx9rend::textureManagerSwapManaged(savedManaged);
}

// one patch under TerrainEditable's "lightmapGeneration" pass 0: TEXLAYER0 = the
// colormap, scaletransxz = (scale.x, scale.z, worldXf, worldZf) with the scale asked through E+0x34,
// the height stream on 1, the terrain's LOD index buffer, and one draw of the LOD-0 range: its
// triangle count (+0x11C) as primitives and, for a triangle list (+0x1A8 == 4), three vertices per
// primitive - else primitives + 2.
void QuadPatchEditable::drawForLightmapGeneration()
{
    TerrainEditable* const te = editableTerrain;
    param(te->m_hTexLayer[0])->setValue(colormap, 0);                         // +0x1D8
    {
        const Vec3& scale = te->getPrimaryWorldScale();                       // E+0x34
        float scaleTransXZ[4] = { scale.x, scale.z, worldXf, worldZf };
        param(te->m_hScaleTransXZ)->setValue(scaleTransXZ, 0);                // +0x1B8
    }
    dx9rend::vertexBufferBindStream(heightVertexBuffer, 1);                   // +0x54
    dx9rend::indexBufferBind(te->m_lodIndexBuffer);                           // +0xD8
    const unsigned int primitives = static_cast<unsigned int>(te->m_lodRangeTriangleCount[0]);
    const unsigned int vertices = (te->m_primitiveType == 4) ? 3 * primitives : primitives + 2;
    dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(te->m_lodRangeTriangleCount[0]),
                                          static_cast<unsigned int>(te->m_primitiveType), 0, 0, vertices, 0);
}

// (scaledX, 0, scaledZ) .. + patchSize * the primary scale (E+0x4C, then E+0x34). The box's
// derived fields are left as the caller built them.
void QuadPatchEditable::getWorldBounds(TerrainAabb& bounds)
{
    TerrainEditable* const te = editableTerrain;
    const int patchSize = te->getPatchSize();                                 // E+0x4C
    const Vec3& scale = te->getPrimaryWorldScale();                           // E+0x34
    bounds.min.y = 0.0f;
    bounds.min.x = static_cast<float>(scaledX);
    bounds.min.z = static_cast<float>(scaledZ);
    const float size = static_cast<float>(patchSize);
    bounds.max.y = 0.0f;
    bounds.max.x = size * scale.x + static_cast<float>(scaledX);
    bounds.max.z = size * scale.z + static_cast<float>(scaledZ);
}

// the road mask and its consumers
// owner release, evict, nulled.
void QuadPatchEditable::releaseRoadMask()
{
    if (!m_roadMask)
        return;
    terrainReleaseTexture(m_roadMask);
    dx9rend::textureManagerEvictFromCache(m_roadMask);
    m_roadMask = 0;
}

// Renders roads from above and writes nonzero mask materials into the primary heightmap.
// Requires size == patchSize for the vertical flip. Restores draw toggles but leaves
// texture management enabled; the mask surface reference is retained.
void QuadPatchEditable::renderRoadMask(int size)
{
    if (m_roadMask)
        releaseRoadMask();
    TerrainEditable* const te = editableTerrain;
    const int supersample = te->m_roadSamplingSize;                           // +0x7C0
    const int patchSize = te->m_patchSize;                                    // +0x38
    dx9rend::stateManagerPush();

    unsigned char* const toggles = static_cast<unsigned char*>(static_cast<void*>(dx9rend::g_Renderer));
    const unsigned char saved1701 = toggles[1701];
    const unsigned char saved1693 = toggles[1693];
    const unsigned char saved1713 = toggles[1713];
    const unsigned char saved1705 = toggles[1705];
    const unsigned char saved1697 = toggles[1697];
    const unsigned char saved1717 = toggles[1717];
    const unsigned char saved1702 = toggles[1702];
    const unsigned char saved1703 = toggles[1703];
    const unsigned char saved1698 = toggles[1698];
    const unsigned char saved1714 = toggles[1714];
    const unsigned char saved1700 = toggles[1700];
    te->m_primaryHeightmap->getSeaWaterLevel();                               // +0x64, dropped
    dx9rend::rendererGetHemiRenderMode();                                     // sub_10009F00, dropped
    g_LightManager->getDynamicShadowsEnabled();                               // +0xF0, dropped
    te->getShaderTechnique();                                                 // E+0x8C, dropped

    toggles[1693] = 0;
    toggles[1701] = 0;
    toggles[1713] = 0;
    toggles[1705] = 0;
    toggles[1697] = 0;
    bundledMeshSetHemiEnabled(0);                                             // dword_102597F4 +0x10
    toggles[1717] = 0;
    toggles[1702] = 0;
    toggles[1703] = 0;
    toggles[1698] = 0;
    staticMeshRendererSetHemiEnabled(0);                                      // +0x18
    toggles[1714] = 0;
    toggles[1700] = 1;
    roadCompiledRendererSetHemiEnabled(1);                                    // +0x18
    static_cast<RoadEditableRenderer*>(g_RoadEditableRenderer)->m_mode = 1;   // +0x18

    // The shipped block zeroes +0x00..+0x12 and leaves +0x13 and the view word (+0x14) as stack
    // garbage. [CHOSEN, not recovered] all 24 bytes zeroed: the render reads +0x14 only when +0x00 is
    // set, and it is 0 (the same choice rendererMakeTopWorldScreen makes, RW-273).
    dx9rend::RenderFlags flags;
    std::memset(&flags, 0, sizeof(flags));
    dx9rend::textureManagerSwapManaged(1);                                    // the old value dropped

    const int side = size * supersample;
    IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
    IDirect3DSurface9* renderTarget = 0;
    IDirect3DSurface9* depthStencil = 0;
    device->CreateRenderTarget(static_cast<UINT>(side), static_cast<UINT>(side), D3DFMT_A8R8G8B8,
                               D3DMULTISAMPLE_NONE, 0, FALSE, &renderTarget, 0);              // +0x70
    device->CreateDepthStencilSurface(static_cast<UINT>(side), static_cast<UINT>(side), D3DFMT_D24S8,
                                      D3DMULTISAMPLE_NONE, 0, FALSE, &depthStencil, 0);       // +0x74
    const float patchSizeF = static_cast<float>(patchSize);
    const float extent = patchSizeF * te->m_primaryWorldScale.x;
    Vec3 position;
    position.x = (static_cast<float>(gridX) + 0.5f) * extent;
    position.y = 500.0f;
    position.z = patchSizeF * te->m_primaryWorldScale.z * (static_cast<float>(gridZ) + 0.5f);
    dx9rend::rendererRenderOrthoTopDown(renderTarget, depthStencil, position, extent, flags);

    toggles[1693] = saved1693;
    toggles[1701] = saved1701;
    toggles[1713] = saved1713;
    toggles[1705] = saved1705;
    toggles[1697] = saved1697;
    bundledMeshSetHemiEnabled(saved1697);
    toggles[1717] = saved1717;
    toggles[1702] = saved1702;
    toggles[1703] = saved1703;
    toggles[1698] = saved1698;
    staticMeshRendererSetHemiEnabled(saved1698);
    toggles[1714] = saved1714;
    toggles[1700] = saved1700;
    roadCompiledRendererSetHemiEnabled(saved1700);
    static_cast<RoadEditableRenderer*>(g_RoadEditableRenderer)->m_mode = 0;

    dx9rend::EngineTexture* const big = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(side), static_cast<unsigned int>(side), 21, 1, 0, 0));
    IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(bigSurface, 0, 0, renderTarget, 0, 0, 0xFFFFFFFFu, 0);
    m_roadMask = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 1, 0, 0));
    IDirect3DSurface9* const maskSurface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT bigLocked;
    bigSurface->LockRect(&bigLocked, 0, 0);
    D3DLOCKED_RECT maskLocked;
    maskSurface->LockRect(&maskLocked, 0, 0);
    const unsigned int* const bigBits = static_cast<const unsigned int*>(bigLocked.pBits);
    unsigned int* const maskBits = static_cast<unsigned int*>(maskLocked.pBits);
    if (size > 0)
    {
        int sourceRow = 0;
        int targetRow = patchSize - 1;
        for (int rows = size; rows; --rows)
        {
            int sourceColumn = 0;
            for (int x = 0; x < size; ++x)
            {
                unsigned int* const target = maskBits + x + targetRow * texelPitch(maskLocked);
                *target = 0;
                if (supersample > 0)
                {
                    for (int sy = 0; sy < supersample; ++sy)
                    {
                        for (int sx = 0; sx < supersample; ++sx)
                        {
                            const unsigned int value =
                                bigBits[sourceColumn + sx + (sourceRow + sy) * texelPitch(bigLocked)];
                            if (value)
                                *target = value;
                        }
                    }
                }
                sourceColumn += supersample;
            }
            sourceRow += supersample;
            --targetRow;
        }
    }
    for (int z = 0; z < size; ++z)
    {
        for (int x = 0; x < size; ++x)
        {
            const unsigned char material = static_cast<unsigned char>(maskBits[x + z * texelPitch(maskLocked)]);
            if (material)
                te->m_primaryHeightmap->setMaterialInGrid(x + patchSize * gridX, z + patchSize * gridZ, material);
        }
    }
    bigSurface->UnlockRect();
    maskSurface->UnlockRect();
    bigSurface->Release();
    renderTarget->Release();
    depthStencil->Release();
    terrainReleaseTexture(big);
    dx9rend::textureManagerEvictFromCache(big);
    dx9rend::stateManagerPop();
}

// the road mask at the patch size, then every non-zero mask texel (its low byte) burnt
// into the undergrowth: UndergrowthSystem +0x5C (an empty slot in the plain system - only the editor's
// derived system paints) at ((n * gridX + x * (n / n)) * scale.x, (n * gridZ + z * (n / n)) * scale.x),
// radius 2, type 0. The mask's surface is unlocked, never released; the mask is dropped (owner
// release + evict, nulled).
void QuadPatchEditable::burnRoadsInUndergrowth()
{
    TerrainEditable* const te = editableTerrain;
    const int n = te->m_patchSize;
    const float scale = te->m_primaryWorldScale.x;
    const float ratio = static_cast<float>(n) / static_cast<float>(n);
    renderRoadMask(n);
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, 0, 0);
    const unsigned int* const bits = static_cast<const unsigned int*>(locked.pBits);
    for (int z = 0; z < n; ++z)
    {
        for (int x = 0; x < n; ++x)
        {
            if (!static_cast<unsigned char>(bits[x + z * texelPitch(locked)]))
                continue;
            const float pointZ = (static_cast<float>(n * gridZ) + static_cast<float>(z) * ratio) * scale;
            const float pointX = (static_cast<float>(n * gridX) + static_cast<float>(x) * ratio) * scale;
            static_cast<UndergrowthSystem*>(g_UndergrowthSystem)->editorPaint(pointX, pointZ, 2.0f, 0);
        }
    }
    surface->UnlockRect();
    releaseRoadMask();                                                        // 0x1019F300's body, inlined
}

// the patch's materials into the primary heightmap: per heightmap quad (z outer, x inner,
// n = the patch size asked through E+0x4C) the merged set's dominant layer (+0x0C) and that record's
// material byte (+0x14), then the road mask at the patch size and every non-zero mask texel's low byte
// on top. The mask's surface is unlocked, never released; the mask is dropped (owner release +
// evict, nulled).
void QuadPatchEditable::buildMaterialMap()
{
    TerrainEditable* const te = editableTerrain;
    const int n = te->getPatchSize();                                         // E+0x4C
    for (int z = 0; z < n; ++z)
    {
        int x = 0;
        do
        {
            const unsigned char layer = m_mergedLayers.dominantLayer(x, z);
            const TerrainDetailTexture* const record =
                reinterpret_cast<TerrainDetailTexture**>(te->m_detailTextures.first)[layer];
            te->m_primaryHeightmap->setMaterialInGrid(x + n * gridX, z + n * gridZ, record->materialId);
            ++x;
        }
        while (x < n);
    }
    renderRoadMask(n);
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, 0, 0);
    const unsigned int* const bits = static_cast<const unsigned int*>(locked.pBits);
    for (int z = 0; z < n; ++z)
    {
        for (int x = 0; x < n; ++x)
        {
            const unsigned char material = static_cast<unsigned char>(bits[x + z * texelPitch(locked)]);
            if (material)
                te->m_primaryHeightmap->setMaterialInGrid(x + n * gridX, z + n * gridZ, material);
        }
    }
    surface->UnlockRect();
    releaseRoadMask();                                                        // 0x1019F300's body, inlined
}

// Rebuilds the material/height overlay only for technique 8, replacing any previous overlay.
// Writes contiguous patch-sized rows without using lock pitch. Height display retains the
// low-byte conversion and subsequent color scaling, including overflow wrapping.
void QuadPatchEditable::createMaterialOverlay()
{
    if (m_materialOverlay)
        releaseMaterialOverlay();
    TerrainEditable* const te = editableTerrain;
    if (te->m_shaderTechnique != 8)
        return;
    buildMaterialMap();
    const int n = te->m_patchSize;
    if (!m_materialOverlay)
        m_materialOverlay = engineTexture(dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(n), static_cast<unsigned int>(n), 21, 1, 0, 0));
    IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_materialOverlay, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT locked;
    surface->LockRect(&locked, 0, 0);
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float scale = te->getPrimaryWorldScale().x;                         // E+0x34
    if (n > 0)
    {
        unsigned int* row = static_cast<unsigned int*>(locked.pBits);
        for (int z = 0; z < n; ++z, row += n)
        {
            unsigned int* texel = row;
            for (int x = 0; x < n; ++x, ++texel)
            {
                unsigned int value;
                if (te->m_showHeightmap)
                {
                    const float pointX = static_cast<float>(x + n * gridX) * scale;
                    const float pointZ = static_cast<float>(z + n * gridZ) * scale;
                    const float height = cluster->getHeightInWorldCoords(pointX, pointZ);
                    const float grey = static_cast<float>(static_cast<unsigned char>(static_cast<__int64>(height)));
                    value = terrainEditPackColor(grey, grey, grey, 255.0f);
                }
                else
                {
                    const float pointX = static_cast<float>(x + n * gridX) * scale;
                    const float pointZ = static_cast<float>(z + n * gridZ) * scale;
                    const unsigned char material = cluster->getMaterialInWorldCoords(pointX, pointZ);
                    if (material && material <= 15)
                    {
                        value = kMaterialPalette[material];
                    }
                    else
                    {
                        const float grey = static_cast<float>(material);
                        value = terrainEditPackColor(grey, grey, grey, 255.0f);
                    }
                }
                *texel = value;
            }
        }
    }
    surface->UnlockRect();
    surface->Release();
}


// R3 - saving, every body read in RendDX9.dll 1.50 ("RendDX9.dll (2).i64"; Hex-Rays for
// the shape, the listing for each push order, receiver and release order). Paths are the terrain's
// getLevelSavePath() + a level-relative name; textureManagerSaveTexture appends ".dds".


// (`retn 4`) - both detail maps saved as "<save><editor detail base><name>_<1|2><suffix>"
// (the editor base name, P+0xF4, asked before the save path, E+0x90).
void QuadPatchEditable::saveDetailMaps(const vc71::string& suffix)
{
    TerrainEditable* const te = editableTerrain;
    vc71::string base;
    {
        const vc71::string editorBase = te->getEditorDetailmapBaseName();     // P+0xF4
        base = ::renddx9abi::toVc71String(te->getLevelSavePath());           // E+0x90
        base += editorBase;
        base += m_fileName;
    }
    for (int i = 0; i < 2; ++i)
    {
        vc71::string number;
        formatString(number, "_%i", i + 1);
        vc71::string name(base);
        name += number;
        name += suffix;
        dx9rend::textureManagerSaveTexture(engineTexture(detailMap[i]), name);   // sub_10017F90, answer dropped
    }
}

// (`retn 4`) - the lightmap saved as "<save><base><name>".
void QuadPatchEditable::saveLightmap(const vc71::string& base)
{
    vc71::string name(::renddx9abi::toVc71String(editableTerrain->getLevelSavePath()));   // E+0x90
    name += base;
    name += m_fileName;
    dx9rend::textureManagerSaveTexture(engineTexture(lightmap), name);
}

// only for an edited colormap (+0x32A): the edit copy committed (0x1019EF80), then the
// colormap saved as "<save><editor colormap base><name>" (P+0xFC asked first).
void QuadPatchEditable::saveEditedColormap()
{
    if (!m_colormapEdited)
        return;
    commitColormapEdit();
    TerrainEditable* const te = editableTerrain;
    vc71::string name;
    {
        const vc71::string editorBase = te->getEditorColormapBaseName();      // P+0xFC
        name = ::renddx9abi::toVc71String(te->getLevelSavePath());
        name += editorBase;
        name += m_fileName;
    }
    dx9rend::textureManagerSaveTexture(engineTexture(colormap), name);
}

// for each compressed detail map that exists: its owner reference released, the same
// pointer evicted (sub_100191C0), the slot nulled, and "<save><editor detail base><name>_<i>_tmp.dds"
// deleted (FileManager +0x20). A missing map deletes nothing.
void QuadPatchEditable::removeCompDetailMaps()
{
    TerrainEditable* const te = editableTerrain;
    for (int i = 0; i < 2; ++i)
    {
        if (!m_compDetailMap[i])
            continue;
        terrainReleaseTexture(m_compDetailMap[i]);
        dx9rend::textureManagerEvictFromCache(m_compDetailMap[i]);
        m_compDetailMap[i] = 0;
        vc71::string name;
        {
            const vc71::string editorBase = te->getEditorDetailmapBaseName(); // P+0xF4
            name = ::renddx9abi::toVc71String(te->getLevelSavePath());
            name += editorBase;
            name += m_fileName;
            vc71::string suffix;
            formatString(suffix, "_%i_tmp", i + 1);
            name += suffix;
        }
        name += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(name));
    }
}

// the compressed detail maps (drawn instead of the detail maps while useCompDetailMaps):
// the old ones removed (0x1019F120), both detail maps saved with the suffix "_tmp", then per map i nvdxt
// (E+0xF4) compiles "<save><editor detail base><name>_<i>_tmp.dds" into "<save><editor detail
// directory>" (P+0xF8) with the detail settings (+0x800); the uncompressed input is deleted, the
// "_tmp_.dds" output moved onto "_tmp.dds" (no leftover delete), and "<...>_<i>_tmp" loaded by name
// (sub_10019260) into +0x2DC / +0x2E0.
// The name goes through the texture manager, which cuts at the last dot - a dotted save path breaks it.
void QuadPatchEditable::makeCompDetailMaps()
{
    removeCompDetailMaps();
    {
        const vc71::string suffix("_tmp");
        saveDetailMaps(suffix);
    }
    TerrainEditable* const te = editableTerrain;
    for (int i = 0; i < 2; ++i)
    {
        vc71::string temp;
        {
            const vc71::string editorBase = te->getEditorDetailmapBaseName(); // P+0xF4
            vc71::string base(::renddx9abi::toVc71String(te->getLevelSavePath()));
            base += editorBase;
            base += m_fileName;
            vc71::string suffix;
            formatString(suffix, "_%i_tmp", i + 1);
            temp = base;
            temp += suffix;
        }
        {
            // The argument blocks are built right to left: the options, the directory, the file.
            const TerrainEditableString options(te->m_detailmapSettings);     // +0x800
            vc71::string outputDirectory;
            {
                const vc71::string directory = te->getEditorDetailmapDirectory();   // P+0xF8
                outputDirectory = ::renddx9abi::toVc71String(te->getLevelSavePath());
                outputDirectory += directory;
            }
            vc71::string file(temp);
            file += ".dds";
            te->runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(outputDirectory), options);
        }
        {
            vc71::string input(temp);
            input += ".dds";
            ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(input));
        }
        {
            vc71::string target(temp);
            target += ".dds";
            vc71::string source(temp);
            source += "_.dds";
            ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                               ::renddx9abi::makeHostString(target));
        }
        m_compDetailMap[i] = engineTexture(dx9rend::textureManagerGetTexture(temp));
    }
}

// Burns the patch colormap, optionally rendering edited layers before DDS compilation.
// Deletes the destination first and preserves the compatibility target-release sequence.
// [CHOSEN, not recovered] Unused render-flag storage is zero-initialized.
void QuadPatchEditable::burnColormap(const vc71::string& base)
{
    TerrainEditable* const te = editableTerrain;
    const int size = te->m_patchColormapSize;                                 // +0x48
    vc71::string path;
    {
        TerrainEditableNameResult colormapBase = te->getColormapBaseName();  // E+0x6C
        path = ::renddx9abi::toVc71String(te->getLevelSavePath());           // E+0x90
        path += ::renddx9abi::toVc71String(colormapBase);
        path += m_fileName;
    }
    {
        vc71::string dds(path);
        dds += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(dds));   // +0x20
    }
    if (hasNoDetailLayers)                                                    // +0x10C
        return;
    if (!m_colormapEdited)                                                    // +0x32A
    {
        vc71::string target(::renddx9abi::toVc71String(te->getLevelSavePath()));
        target += base;
        target += m_fileName;
        vc71::string dds(target);
        dds += ".dds";
        const bool missing = !::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(dds));   // +0x1C
        if (missing)
            dx9rend::textureManagerSaveTexture(engineTexture(colormap), target);
        return;
    }

    dx9rend::stateManagerPush();
    unsigned char* const toggles = static_cast<unsigned char*>(static_cast<void*>(dx9rend::g_Renderer));
    const unsigned char saved1693 = toggles[1693];
    const unsigned char saved1713 = toggles[1713];
    const unsigned char saved1705 = toggles[1705];
    const unsigned char saved1697 = toggles[1697];
    const unsigned char saved1717 = toggles[1717];
    const unsigned char saved1702 = toggles[1702];
    const unsigned char saved1703 = toggles[1703];
    const unsigned char saved1698 = toggles[1698];
    const unsigned char saved1714 = toggles[1714];
    const unsigned char saved1700 = toggles[1700];
    const float savedSeaLevel = te->m_primaryHeightmap->getSeaWaterLevel();  // +0x64
    const int savedRenderMode = dx9rend::rendererGetHemiRenderMode();
    const bool savedShadows = g_LightManager->getDynamicShadowsEnabled();     // +0xF0
    const int savedTechnique = te->getShaderTechnique();                      // E+0x8C

    toggles[1693] = 1;
    toggles[1713] = 0;
    toggles[1705] = 0;
    toggles[1697] = 0;
    bundledMeshSetHemiEnabled(0);                                             // dword_102597F4 +0x10
    toggles[1717] = 0;
    toggles[1702] = 0;
    toggles[1703] = 0;
    toggles[1698] = 0;
    staticMeshRendererSetHemiEnabled(0);                                      // +0x18
    toggles[1714] = 0;
    toggles[1700] = 1;
    roadCompiledRendererSetHemiEnabled(1);                                    // +0x18
    te->m_primaryHeightmap->setSeaWaterLevel(-110.0f);                        // +0x60,
    dx9rend::rendererSetHemiRenderMode(4);
    g_LightManager->setDynamicShadowsEnabled(false);                          // +0xEC
    dx9rend::RenderFlags flags;
    std::memset(&flags, 0, sizeof(flags));
    flags.field01 = 1;                                                        // the terrain arm
    flags.field12 = 1;                                                        // -> the terrain's +0x35A
    te->setShaderTechnique(0);                                                // E+0x88
    const int savedManaged = dx9rend::textureManagerSwapManaged(1);

    IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
    IDirect3DSurface9* renderTarget = 0;
    IDirect3DSurface9* depthStencil = 0;
    device->CreateRenderTarget(static_cast<UINT>(size), static_cast<UINT>(size), D3DFMT_A8R8G8B8,
                               D3DMULTISAMPLE_NONE, 0, FALSE, &renderTarget, 0);              // +0x70
    device->CreateDepthStencilSurface(static_cast<UINT>(size), static_cast<UINT>(size), D3DFMT_D24S8,
                                      D3DMULTISAMPLE_NONE, 0, FALSE, &depthStencil, 0);       // +0x74
    const float patchSizeF = static_cast<float>(te->m_patchSize);
    const float extent = patchSizeF * te->m_primaryWorldScale.x;
    Vec3 position;
    position.x = (static_cast<float>(gridX) + 0.5f) * extent;
    position.y = 500.0f;
    position.z = patchSizeF * te->m_primaryWorldScale.z * (static_cast<float>(gridZ) + 0.5f);
    dx9rend::rendererRenderOrthoTopDown(renderTarget, depthStencil, position, extent, flags);

    toggles[1693] = saved1693;
    toggles[1713] = saved1713;
    toggles[1705] = saved1705;
    toggles[1697] = saved1697;
    bundledMeshSetHemiEnabled(saved1697);
    toggles[1717] = saved1717;
    toggles[1702] = saved1702;
    toggles[1703] = saved1703;
    toggles[1698] = saved1698;
    staticMeshRendererSetHemiEnabled(saved1698);
    toggles[1714] = saved1714;
    toggles[1700] = saved1700;
    roadCompiledRendererSetHemiEnabled(saved1700);
    te->m_primaryHeightmap->setSeaWaterLevel(savedSeaLevel);
    dx9rend::rendererSetHemiRenderMode(savedRenderMode);
    g_LightManager->setDynamicShadowsEnabled(savedShadows);
    te->setShaderTechnique(savedTechnique);

    dx9rend::EngineTexture* const big = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 0, 0, 0));   // levels 0: the full chain
    IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(bigSurface, 0, 0, renderTarget, 0, 0, 0xFFFFFFFFu, 0);
    dx9rend::EngineTexture* const flipped = engineTexture(dx9rend::textureManagerCreateEmpty(
        static_cast<unsigned int>(size), static_cast<unsigned int>(size), 21, 0, 0, 0));
    IDirect3DSurface9* const flippedSurface = dx9rend::textureGetLevelSurface(flipped, 0, 0x7FFFFFFF);
    D3DLOCKED_RECT bigLocked;
    bigSurface->LockRect(&bigLocked, 0, 0);
    D3DLOCKED_RECT flippedLocked;
    flippedSurface->LockRect(&flippedLocked, 0, 0);
    const unsigned int* const bigBits = static_cast<const unsigned int*>(bigLocked.pBits);
    unsigned int* const flippedBits = static_cast<unsigned int*>(flippedLocked.pBits);
    for (int x = 0; x < size; ++x)
    {
        int row = size - 1;
        for (int z = 0; z < size; ++z, --row)
            flippedBits[x + z * texelPitch(flippedLocked)] = bigBits[x + row * texelPitch(bigLocked)] & 0x00FFFFFFu;
    }
    flippedSurface->UnlockRect();
    bigSurface->UnlockRect();
    {
        vc71::string name(path);
        name += "_u8888";
        dx9rend::textureManagerSaveTexture(flipped, name);
    }
    {
        // The argument blocks are built right to left: the options, the directory, the file.
        const TerrainEditableString options(te->m_colormapSettings);           // +0x7C8
        vc71::string outputDirectory;
        {
            const vc71::string directory = te->getColormapDirectory();         // P+0xEC
            outputDirectory = ::renddx9abi::toVc71String(te->getLevelSavePath());
            outputDirectory += directory;
        }
        vc71::string file(path);
        file += "_u8888.dds";
        te->runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(outputDirectory), options);   // E+0xF4
    }
    {
        vc71::string target(path);
        target += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(target));
    }
    {
        vc71::string target(path);
        target += ".dds";
        vc71::string source(path);
        source += "_u8888_.dds";
        ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                           ::renddx9abi::makeHostString(target));   // +0x24
    }
    {
        vc71::string input(path);
        input += "_u8888.dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(input));
    }
    {
        vc71::string leftover(path);
        leftover += "_u8888_.dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(leftover));
    }
    flippedSurface->Release();
    bigSurface->Release();
    terrainReleaseTexture(big);
    dx9rend::textureManagerEvictFromCache(big);
    terrainReleaseTexture(flipped);
    dx9rend::textureManagerEvictFromCache(flipped);
    dx9rend::textureManagerSwapManaged(savedManaged);
    renderTarget->Release();
    depthStencil->Release();
    dx9rend::stateManagerPop();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
