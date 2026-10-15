// Owns undergrowth resources, settings, and wind state.
// Loading and rendering are implemented in the companion translation units.

#ifdef BF2_IMAGE_RENDDX9

#include "UndergrowthSystem.h"

#include "../Physics/Heightmap.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"

#include <cmath>
#include <cstddef>
#include <new>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
// BufferManager.cpp - declared per TU, as the other clients do.
extern void* g_bufferManager;                                              // dword_1023B978
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void  vertexBufferRelease(void* wrapper);                                  // --ref, sub_10014510, free
void  indexBufferRelease(void* wrapper);                                   // --ref, sub_10014300, free
}

namespace geom {

namespace {

// sub_1000ED40, __stdcall(value, low, high), read at the instruction level:
//     fld low ; fld high ; fld value
//     fcomi st, st(1) ; fcmovnb st, st(1)      value >= high  -> high   (CF clear; NaN keeps value)
//     fcomi st, st(2) ; fcmovb  st, st(2)      below low, OR unordered -> low
// An unordered compare sets CF, so a NaN comes back as the LOW bound.
float clampRange(float value, float low, float high)
{
    float v = value;
    if (v >= high)
        v = high;
    if (!(v >= low))
        v = low;
    return v;
}

// A shader parameter slot's release as clear() inlines it: release through vtable +0x08, then null.
void releaseParameter(dx9rend::ShaderParameter*& parameter)
{
    if (parameter)
    {
        parameter->release();                                                // vtable +0x08
        parameter = 0;
    }
}

world::IHeightmapCluster* heightmapCluster()
{
    // `g_hfeClassManager->getSingleton(std::string("HeightmapCluster"))`, unchecked, as at every
    // shipped site in this class. The cluster is BF2.exe's; its interface slots match in both
    // flavours (the HemiMapManager precedent), so the calls below are typed.
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(
        classManager->getSingleton(renddx9abi::HostString("HeightmapCluster"))));
}

} // anonymous namespace


// the constructor. Field order as the instruction stream stores it. Members the shipped
// constructor does NOT write are left out of the list: m_patchVertexCount / m_patchIndexCount (+0x58),
// m_terrain (+0xD8), the cell-grid words (+0xE0..+0xE8), the wind speed/amplitude/phase tables
// (+0x10C..+0x1C8), m_heightmap (+0x568) and the density map's size (+0x578). The profiler stat at
// +0x300 (sub_10047F20(this+0x300, "Undergrowth")) is stat-only scaffolding and is not reproduced (the
// RenderQueue precedent). The atlas is allocated last, after every field is set.

UndergrowthSystem::UndergrowthSystem()
    : m_refCount(1)
    , m_test(1)
    , m_test2(20.0f)
    , m_loaded(false)
    , m_loadStarted(false)
    , m_enabled(true)
    , m_pad013(0)
    , m_levelPath()
    , m_viewDistance(50.0f)
    , m_viewDistanceFadeScale(0.5f)
    , m_viewDistanceHeightScale(0.4f)
    , m_viewDistanceHeight2Scale(0.4f)
    , m_viewDistanceStreamingScale(1.0f)
    , m_patchSubdivide(15)
    , m_swaySpeed(1.0f)
    , m_swaySpeedDiff(0.2f)
    , m_swayScale(0.15f)
    , m_swayScaleDiff(0.01f)
    , m_densityMap(0)
    , m_atlasTexture(0)
    , m_effect(0)
    , m_paramTexLayer0(0), m_paramTexLayer1(0), m_paramTexLayer2(0), m_param078(0)
    , m_paramWorldViewProjection(0), m_paramWorldView(0), m_paramPosOffsetAndScale(0)
    , m_paramTerrainTexCoordScaleAndOffset(0), m_paramCameraPos(0), m_paramSwayOffset(0)
    , m_paramFadeAndHeightScaleOffset(0), m_paramSunColor(0), m_paramGIColor(0)
    , m_paramStaticPointColor(0), m_paramFogRange(0), m_paramFogColor(0)
    , m_paramAmbientIlluminationRange(0), m_paramWaterFogRange(0), m_paramWaterFogColor(0)
    , m_paramPointLightPosAtten(0), m_paramPointLightColor(0), m_paramAlphaRef(0)
    , m_paramTransparencyX8(0), m_paramCameraToWater(0), m_paramShadowMap(0)
    , m_paramShadowProjMatrix(0), m_paramShadowTrapMatrix(0)
    , m_lightingScale(1.8f)
    , m_cells(0)
    , m_patchCount(0)
    , m_patches(0)
    , m_materials()
    , m_alphaRef(0.15f)
    , m_atlas(0)
    , m_random()
    , m_dynamicShadowsEnable(true)
    , m_simpleShaderEnable(false)
    , m_zPassPatchCount(0)
    , m_viewDistanceScale(1.0f)
    , m_viewDistanceFade(0.2f)
    , m_editorMode(false)
{
    m_lastRebuildPosition.x = 0.0f;                                          // +0x2F4..+0x2FC
    m_lastRebuildPosition.y = 0.0f;
    m_lastRebuildPosition.z = 0.0f;
    m_invHeightmapScale.x = 0.0f;                                            // +0x56C..+0x574
    m_invHeightmapScale.y = 0.0f;
    m_invHeightmapScale.z = 0.0f;

    // 0x1014AF31..0x1014AF73: `new TextureAtlas` - engineAlloc(0x14), and the constructor only on a
    // non-null block.
    void* const storage = ::renddx9abi::engineAlloc(sizeof(dx9rend::TextureAtlas));
    m_atlas = storage ? dx9rend::textureAtlasCtor(storage) : 0;
}


// (through the deleting destructor 0x10151FB0, slot +0x70). The body: clear() if loaded,
// then the atlas deleted. Everything after that is the member destruction the compiler generated, in
// reverse declaration order - written out here in that order: the stat (not reproduced), the two
// plain vectors, the material vector, the 27 parameters from +0xD4 down to +0x6C (including +0x78,
// which clear() never releases) and the effect, then the atlas texture, and the path string last.
// None of these member releases nulls its slot.

UndergrowthSystem::~UndergrowthSystem()
{
    if (m_loaded)
        clear();

    if (m_atlas)
    {
        dx9rend::textureAtlasDtor(m_atlas);
        ::renddx9abi::engineFree(m_atlas);
    }

    // sub_10032E40(this + 0x300) - the profiler stat's destructor, not reproduced.

    m_drawOrder.clear();                                                     // +0x2E4
    m_visibleCells.clear();                                                  // +0x2D4
    m_materials.clear();                                                     // +0xF8: 101518E0 + free

    dx9rend::ShaderParameter* const parameters[] =
    {
        m_paramShadowTrapMatrix, m_paramShadowProjMatrix, m_paramShadowMap, m_paramCameraToWater,
        m_paramTransparencyX8, m_paramAlphaRef, m_paramPointLightColor, m_paramPointLightPosAtten,
        m_paramWaterFogColor, m_paramWaterFogRange, m_paramAmbientIlluminationRange, m_paramFogColor,
        m_paramFogRange, m_paramStaticPointColor, m_paramGIColor, m_paramSunColor,
        m_paramFadeAndHeightScaleOffset, m_paramSwayOffset, m_paramCameraPos,
        m_paramTerrainTexCoordScaleAndOffset, m_paramPosOffsetAndScale, m_paramWorldView,
        m_paramWorldViewProjection, m_param078, m_paramTexLayer2, m_paramTexLayer1, m_paramTexLayer0
    };                                                                       // +0xD4 .. +0x6C
    for (unsigned int i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i)
        if (parameters[i])
            parameters[i]->release();                                        // vtable +0x08
    if (m_effect)
        dx9rend::effectRelease(m_effect);                                    // +0x68, vtable +0x08

    if (m_atlasTexture && --m_atlasTexture->refCount == 0)                   // +0x64
    {
        dx9rend::textureDtor(m_atlasTexture);
        ::renddx9abi::engineFree(m_atlasTexture);
    }
    // m_levelPath's destructor follows as the last member; m_materials' finds the vector empty.
}

void UndergrowthSystem::operator delete(void* memory)
{
    ::renddx9abi::engineFree(memory);
}


// The IBase quartet.

void UndergrowthSystem::addRef()                  // ICF_incrementDwordAt0x4, 0x1000CB20
{
    ++m_refCount;
}

int UndergrowthSystem::getRef()                   // ICF_getDwordAt0x4, 0x101A9A10
{
    return m_refCount;
}

// decrement, and at zero dispatch the scalar deleting destructor (slot +0x70, flag 1).
int UndergrowthSystem::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// IBase (1) and IID_IUndergrowthSystem (0xB801DCFF, dword_1021AE90) answer `this`.
void* UndergrowthSystem::queryInterface(unsigned int iid) const
{
    if (iid == 1)
        return const_cast<UndergrowthSystem*>(this);
    return iid != 0xB801DCFFu ? 0 : const_cast<UndergrowthSystem*>(this);
}


// Releases level resources and clears cells, patches, density data, and material storage.
// Keeps the destructor-only +0x78 handle. Fog range is released twice, with the second
// release seeing null; water fog color is released before water fog range.

void UndergrowthSystem::clear()
{
    if (m_atlasTexture)                                                      // +0x64
        dx9rend::textureReleaseRef(m_atlasTexture);                          // --ref; dtor + free; null
    if (m_effect)                                                            // +0x68
    {
        dx9rend::effectRelease(m_effect);
        m_effect = 0;
    }
    releaseParameter(m_paramTexLayer0);                                      // +0x6C
    releaseParameter(m_paramTexLayer1);                                      // +0x70
    releaseParameter(m_paramTexLayer2);                                      // +0x74  (+0x78 skipped)
    releaseParameter(m_paramWorldViewProjection);                            // +0x7C
    releaseParameter(m_paramWorldView);                                      // +0x80
    releaseParameter(m_paramPosOffsetAndScale);                              // +0x84
    releaseParameter(m_paramTerrainTexCoordScaleAndOffset);                  // +0x88
    releaseParameter(m_paramCameraPos);                                      // +0x8C
    releaseParameter(m_paramSwayOffset);                                     // +0x90
    releaseParameter(m_paramFadeAndHeightScaleOffset);                       // +0x94
    releaseParameter(m_paramSunColor);                                       // +0x98
    releaseParameter(m_paramGIColor);                                        // +0x9C
    releaseParameter(m_paramStaticPointColor);                               // +0xA0
    releaseParameter(m_paramFogRange);                                       // +0xA4
    releaseParameter(m_paramFogColor);                                       // +0xA8
    releaseParameter(m_paramFogRange);                                       // +0xA4 again
    releaseParameter(m_paramAmbientIlluminationRange);                       // +0xAC
    releaseParameter(m_paramWaterFogColor);                                  // +0xB4 before
    releaseParameter(m_paramWaterFogRange);                                  // +0xB0
    releaseParameter(m_paramPointLightPosAtten);                             // +0xB8
    releaseParameter(m_paramPointLightColor);                                // +0xBC
    releaseParameter(m_paramAlphaRef);                                       // +0xC0
    releaseParameter(m_paramTransparencyX8);                                 // +0xC4
    releaseParameter(m_paramCameraToWater);                                  // +0xC8
    releaseParameter(m_paramShadowMap);                                      // +0xCC
    releaseParameter(m_paramShadowProjMatrix);                               // +0xD0
    releaseParameter(m_paramShadowTrapMatrix);                               // +0xD4

    delete[] m_cells;                                                        // sub_1005F370(p - 4)
    m_cells = 0;
    delete[] m_patches;                                                      // (3)
    m_patches = 0;
    delete[] m_densityMap;                                                   // sub_1005F370, unguarded
    m_densityMap = 0;
    m_patchCount = 0;

    m_materials.clear();                                                     // +0xF8, freed

    m_patchVertexCount = 0;
    m_patchIndexCount = 0;
    m_loaded = false;
}

// the patch record's destructor: the vertex buffer (+0x10), then the index buffer.
UndergrowthPatch::~UndergrowthPatch()
{
    if (vertexBuffer)
        dx9rend::vertexBufferRelease(vertexBuffer);                          // --ref, sub_10014510
    if (indexBuffer)
        dx9rend::indexBufferRelease(indexBuffer);                            // --ref, sub_10014300
}


// The small slots.

bool UndergrowthSystem::isLoaded() const { return m_loaded; }                    // +0x14

// +0x18, only a loaded system reloads - through the virtual isLoaded and load, the
// non-virtual clear in between, and the patch rebuild at the scaled view distance. In a game nothing
// writes m_levelPath (only load's dead editor arm does), so this hands load an empty path - as shipped.
void UndergrowthSystem::reload()
{
    if (!isLoaded())
        return;
    clear();
    load(m_levelPath);                                                       // its own member, by ref
    rebuildPatches(true, m_viewDistanceScale * m_viewDistance);              // (1, ...)
}

// +0x1C, the level-unload sweep's call: clear, then hand every distinct atlas page back
// to the TextureManager's cache. The atlas itself survives (its entries emptied).
void UndergrowthSystem::unload()
{
    clear();
    if (!m_atlas)
        return;
    std::vector<dx9rend::EngineTexture*> pages;
    dx9rend::textureAtlasCollectPages(m_atlas, pages);
    dx9rend::textureAtlasReleaseAll(m_atlas);
    for (unsigned int i = 0; i < pages.size(); ++i)
        dx9rend::textureManagerEvictFromCache(pages[i]);
}

// +0x24, the wind. For each of the sixteen waves: advance the phase by speed * time and
// wrap it into [0, 2pi) with fmod against the double 6.2831854820251465 (dbl_1020AAE8, the float 2pi
// widened); then x/y = amplitude * sin/cos(phase) and z/w are multiplied by the amplitude IN place -
// every frame, so they compound. Transcribed, not tidied.
void UndergrowthSystem::update(float time)
{
    for (int i = 0; i < 16; ++i)
    {
        const float phase = static_cast<float>(
            std::fmod(static_cast<double>(time * m_windSpeed[i] + m_windPhase[i]),
                      6.2831854820251465));
        m_windPhase[i] = phase;
        const float amplitude = m_windAmplitude[i];
        UndergrowthWindWave& wave = m_windWaves[i];
        wave.x = std::sin(phase);                                            // fsincos
        wave.y = std::cos(phase);
        wave.x = amplitude * wave.x;
        wave.y = amplitude * wave.y;
        wave.z = amplitude * wave.z;
        wave.w = amplitude * wave.w;
    }
}

// +0x28, the GeneralHeight of the material painted under (x, z) - the first material
// whose id matches the density byte, 0 when none does.
float UndergrowthSystem::getTypeHeightAt(float x, float z)
{
    const int id = densityAt(x, z);
    for (unsigned int i = 0; m_materials.data() && i < m_materials.size(); ++i)
    {
        if (m_materials[i].id == id)
            return m_materials[i].generalHeight;
    }
    return 0.0f;
}

// +0x2C, how far the grass reaches above `position`, faded out with the viewer distance.
//   * unloaded -> 0;
//   * the four corner materials' GeneralHeight, bilinear in the density cell;
//   * fade = (|viewer - position| - h2 * dist) / ((1 - h2) * dist), clamped 0..1 (h2 =
//     ViewDistanceHeight2Scale, dist = ViewDistance);
//   * result = fade * height - (position.y - terrain height); negative results become 0.
// The cluster's getPrimaryHeightmap (+0x40) is called and its result discarded, as shipped.
float UndergrowthSystem::getGrassHeightAbove(const Vec3& position, const Vec3& viewer)
{
    if (!m_loaded)
        return 0.0f;

    world::IHeightmapCluster* const cluster = heightmapCluster();
    cluster->getPrimaryHeightmap();                                          // +0x40, discarded
    const float groundY = cluster->getHeightInWorldCoords(position.x, position.z);   // +0x6C

    int ids[4];
    float fracX, fracZ;
    densityCornersAt(position.x, position.z, ids, &fracX, &fracZ);

    float heights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int corner = 0; corner < 4; ++corner)
    {
        for (unsigned int i = 0; m_materials.data() && i < m_materials.size(); ++i)
        {
            if (m_materials[i].id == ids[corner])
            {
                heights[corner] = m_materials[i].generalHeight;
                break;
            }
        }
    }

    const float top = (heights[1] - heights[0]) * fracX + heights[0];
    const float dx = viewer.x - position.x;
    const float dy = viewer.y - position.y;
    const float dz = viewer.z - position.z;
    const float distance = std::sqrt(dz * dz + dx * dx + dy * dy);
    const float fade = clampRange(
        (distance - m_viewDistanceHeight2Scale * m_viewDistance)
            / ((1.0f - m_viewDistanceHeight2Scale) * m_viewDistance),
        0.0f, 1.0f);
    const float height = ((heights[3] - heights[2]) * fracX + heights[2] - top) * fracZ + top;
    const float result = fade * height - (position.y - groundY);
    if (result < 0.0f)
        return 0.0f;
    return result;
}

void UndergrowthSystem::setEnabled(bool enabled) { m_enabled = enabled; }           // +0x30
bool UndergrowthSystem::getEnabled() const { return m_enabled; }                    // +0x34
void UndergrowthSystem::setDynamicShadowsEnable(bool enable) { m_dynamicShadowsEnable = enable; }
bool UndergrowthSystem::getDynamicShadowsEnable() const { return m_dynamicShadowsEnable; }
bool UndergrowthSystem::getSimpleShaderEnable() const { return m_simpleShaderEnable; }
void UndergrowthSystem::setZPassPatchCount(unsigned int count) { m_zPassPatchCount = count; }
unsigned int UndergrowthSystem::getZPassPatchCount() const { return m_zPassPatchCount; }
float UndergrowthSystem::getViewDistance() const { return m_viewDistance; }         // +0x64
void UndergrowthSystem::setViewDistanceScale(float scale)                           // +0x68
{
    m_viewDistanceScale = clampRange(scale, 0.0f, 1.0f);
}
float UndergrowthSystem::getViewDistanceScale() const { return m_viewDistanceScale; }

// Base editor hooks intentionally do nothing; the editor subclass supplies behavior.
void UndergrowthSystem::editorHook50() {}
void UndergrowthSystem::editorHook54() {}
void UndergrowthSystem::editorHook58(int) {}
void UndergrowthSystem::editorPaint(float, float, float, int) {}
void UndergrowthSystem::editorHook60() {}

// +0x74, the image's shared `return 0`.
bool UndergrowthSystem::isEditable() const { return false; }

// +0x40, simpleShaderEnable. Only a change does anything; on a loaded system every
// patch gets NEW buffers - the vertex stride follows the shader (24 bytes simple, 16 full) - and the
// patches are rebuilt at the scaled view distance.
void UndergrowthSystem::setSimpleShaderEnable(bool enable)
{
    if (m_simpleShaderEnable == enable)
        return;
    m_simpleShaderEnable = enable;
    if (!m_loaded)
        return;

    const unsigned int stride = 8u * (enable ? 1u : 0u) + 16u;
    for (unsigned int i = 0; i < m_patchCount; ++i)
    {
        UndergrowthPatch& patch = m_patches[i];
        if (patch.vertexBuffer)
            dx9rend::vertexBufferRelease(patch.vertexBuffer);
        if (patch.indexBuffer)
            dx9rend::indexBufferRelease(patch.indexBuffer);
        patch.vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager,
                                                         m_patchVertexCount, stride,
                                                         std::string("UndergrowthSystem"), 8u);
        patch.indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_patchIndexCount, 8u,
                                                                    101u);  // D3DFMT_INDEX16
        if (patch.cell)
        {
            patch.cell->patch = 0;
            patch.cell = 0;
        }
    }
    rebuildPatches(true, m_viewDistanceScale * m_viewDistance);              // (1, ...)
}


// The two density queries. worldToDensityCell (0x10145B90) is the render path's.


// the density byte of the cell under (x, z), bounded by the map's own size
// (+0x578 x +0x57C); 0 outside.
int UndergrowthSystem::densityAt(float x, float z)
{
    int col, row;
    float fracX, fracZ;
    worldToDensityCell(x, z, &col, &row, &fracX, &fracZ);
    if (col < 0)
        return 0;
    if (col >= m_densityWidth || row < 0 || row >= m_densityHeight)
        return 0;
    return m_densityMap[row * m_densityWidth + col];
}

// the four density bytes around (x, z) - (c, r), (c+1, r), (c, r+1), (c+1, r+1) - each 0
// outside the map, plus the fractions. The bounds AND the row stride here come from the primary
// heightmap's size (getPrimaryHeightmap()->getSize), not from the density map's own +0x578/+0x57C.
void UndergrowthSystem::densityCornersAt(float x, float z, int ids[4], float* fracX, float* fracZ)
{
    world::IHeightmapCluster* const cluster = heightmapCluster();
    world::IHeightmap* const heightmap = cluster->getPrimaryHeightmap();     // +0x40
    int width, height;
    heightmap->getSize(width, height);                                       // IHeightmap +0x2C

    int col, row;
    worldToDensityCell(x, z, &col, &row, fracX, fracZ);

    const bool colIn  = col >= 0 && col < width;
    const bool col1In = col + 1 >= 0 && col + 1 < width;
    const bool rowIn  = row >= 0 && row < height;
    const bool row1In = row + 1 >= 0 && row + 1 < height;
    ids[0] = (colIn && rowIn)   ? m_densityMap[width * row + col] : 0;
    ids[1] = (col1In && rowIn)  ? m_densityMap[width * row + col + 1] : 0;
    ids[2] = (colIn && row1In)  ? m_densityMap[width * (row + 1) + col] : 0;
    ids[3] = (col1In && row1In) ? m_densityMap[width * (row + 1) + col + 1] : 0;
}


// the reflection factory. As in every factory of this image, the allocation's null
// result only skips the constructor; queryInterface is then dispatched through the (possibly null)
// pointer regardless. Preserved.

IBase* __fastcall createUndergrowthSystem(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(UndergrowthSystem));
    UndergrowthSystem* const self = storage ? new (storage) UndergrowthSystem() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
