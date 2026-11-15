// Terrain visibility, shader constants, diffuse passes and water rendering.
// The grid-water path is selected when ShaderVersion is zero, as in stock shaders.
// Ra water and spot-light profiling retain named boundaries. Older per-frame paths still omit
// their diagnostic-only profiling brackets; those omissions are documented at their callers.
#ifdef BF2_IMAGE_RENDDX9

#if defined(__APPLE__)
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered]
#endif
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "Terrain.h"
#include "QuadPatch.h"
#include "XpLightManager.h"
#include "StaticMesh.h"
#include "../Game/Objects/Visual/LightSource.h"   // the mask-light pass's lights
#include "../Math/Frustum.h"
#include "../RendDX9/RendDX9Math.h"       // the RendDX9.dll Math copies
#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"
#include "../Game/Main/IVideoOptions.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendMisc/EnvMapSystem/EnvMapManager.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../RendDX9/Ra/RaStreamDataEditable.h"
#include "../RendMisc/IRenderView.h"
#include "../RendMisc/IRenderer.h"      // getRenderStats (+0x6C)

#include <cmath>
#include <cstring>
#include <new>
#include <cstdint>
#include "../GeomMisc/FSQuadDrawer.h"   // textureHalfTexelSize, PE 0x100846A0

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_rendererBase;       // RendDX9Init.cpp   - dword_1023B970 (the Renderer object)
extern void* g_shaderManager;      // ShaderManager.cpp - dword_1023B97C
extern void* g_raShaderConstants;  // RenderQueues.cpp  - dword_1023DDBC (the Ra constants block)
extern double g_waterClock;        // BF2Render.cpp     - [CHOSEN, not recovered] GlobalTime as a double

bool  effectHasTechnique(void* effect, const std::string& name);          // Effect.cpp,  sub_1001DC20
void  updateShaderDefines(void* shaderManager, void* base);                // ShaderManager.cpp, 0x10023730
void  renderQueueFlush(unsigned int slotByteOffset, int flushArg);         // RenderQueues.cpp
void  vertexBufferBindStream(void* wrapper, unsigned int stream);          // BufferManager.cpp, sub_100146C0
void  indexBufferBind(void* wrapper);
void  vertexDeclarationBind(void* wrapper);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);               // StateManager.cpp, sub_10005820

} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                       // ModuleGeom.cpp, dword_1024B914
void* terrainShadowArrayParameter(unsigned int ordinal);
void geometryRendererMatrixProduct(Mat4&, const Mat4&, const Mat4&);

namespace {

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

// The Ra constants block (g_raShaderConstants) words, by their retail x86 byte offsets.
inline unsigned char* constantAt(unsigned int byteOffset)
{
    return dx9rend::raShaderConstantsAt(byteOffset);
}
inline float& constantF(unsigned int byteOffset)
{
    return *reinterpret_cast<float*>(constantAt(byteOffset));
}
inline void*& constantP(unsigned int byteOffset)
{
    return *reinterpret_cast<void**>(constantAt(byteOffset));
}

inline ::dice::hfe::world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

// PE sub_1000A6B0 - is multisampling active. BF2Render.cpp keeps its copy in an anonymous
// namespace, so this is the same file-local probe FSQuadDrawer.cpp carries: the forced-on byte
// +0x514 wins, then the forced-off +0x515, else the first entry of the base's mode vector at +0xC0.
bool isMultisampleActive()
{
    const dx9rend::RendDX9Base* const base =
        static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return dx9rend::multiSampleListActive(base);
}

// dword_10259568 bit 0 & co: the once-only stat registrations are not reproduced; the once-only
// value seeds (the texprojscale / texprojoffset pair of sub_100D95D0) are.
unsigned int g_texProjOnce = 0;                                            // dword_1025929C
float        g_texProjScale[4];                                            // dword_1025928C..98
float        g_texProjOffset[4];                                           // flt_1025927C..88

// The .data constants the water arms read. byte_10239200: the morph limits are squared; the
// flt_10239208 / flt_10239204 pair is WaterScroll.zw; byte_1023920C gates the two cell tests.
const unsigned char kSquaredMorphLimits = 1;                               // byte_10239200
const float         kWaterScrollZ       = 0.3f;                            // flt_10239208
const float         kWaterScrollW       = 0.5f;                            // flt_10239204
const unsigned char kWaterCellTests     = 3;                               // byte_1023920C
// flt_101DDF30 (0x3E4CCCCD) - the 2D water's cycle: one pass over the frame list every
// waterAnimSpeed * 0.2 seconds of GlobalTime (updateWaterConstants, 0x100DBBB6).
const float         kWater2DCycleScale  = 0.2f;
// [CHOSEN, not recovered] The 3D water's wrap period in cycles (updateWaterConstants): about 7 to
// 28 hours for the stock waterAnimSpeed range of 50 to 200. Below it a float keeps the cycle to
// about 6e-5, a small fraction of a texel.
const double        kWater3DCyclePeriod = 1000.0;

// EnvMapManager - the singleton dword_10267A34, now published by RendModule. Its real
// level/frame lifecycle supplies the borrowed +64 texture used by the detail pass.

} // anonymous namespace

IBase* terrainEnvMapManager()
{
    return Rend::g_EnvMapManager;
}


// The shared helpers QuadPatch.cpp and SurroundingPatch.cpp reach (declared in Terrain.h).

unsigned int& terrainRendererStat(unsigned int byteOffset)
{
    // Renderer vslot +0x6C (getRenderStats, retail `return this + 264`), then the counter.
    unsigned char* const stats = static_cast<unsigned char*>(
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderStats());
    return *reinterpret_cast<unsigned int*>(stats + byteOffset);
}

std::vector<LightShadowRecord>& terrainShadowRecords()
{
    return lightManagerShadowRecords(g_LightManager);                     // +0x79C
}

unsigned int terrainShadowRecordCount()
{
    return static_cast<unsigned int>(terrainShadowRecords().size());
}

bool terrainFrustumIntersectsBox(const Frustum* frustum, const float* minMax)
{
    const float* plane = reinterpret_cast<const float*>(frustum);           // m_planes[0] at +0
    for (unsigned int i = 0; i < 6; ++i, plane += 4)
    {
        float x, y, z;
        switch (frustum->m_planeOutcodes[i])
        {
        case 0:  x = minMax[0]; y = minMax[1]; z = minMax[2]; break;
        case 1:  x = minMax[3]; y = minMax[1]; z = minMax[2]; break;
        case 2:  x = minMax[0]; y = minMax[4]; z = minMax[2]; break;
        case 3:  x = minMax[3]; y = minMax[4]; z = minMax[2]; break;
        case 4:  x = minMax[0]; y = minMax[1]; z = minMax[5]; break;
        case 5:  x = minMax[3]; y = minMax[1]; z = minMax[5]; break;
        case 6:  x = minMax[0]; y = minMax[4]; z = minMax[5]; break;
        case 7:  x = minMax[3]; y = minMax[4]; z = minMax[5]; break;
        default: x = 0.0f; y = 0.0f; z = 0.0f; break;
        }
        if (static_cast<double>(z) * plane[2] + static_cast<double>(x) * plane[0]
            + static_cast<double>(y) * plane[1] + plane[3] > 0.0)
            return false;
    }
    return true;
}

// Typed sky-light direction/color accessors preserve host interface semantics.
const float* terrainSkyLightDirection(world::LightSource* light)
{
    return &light->getDirection().x;                                               // +0x17C
}

const float* terrainSkyLightColor(world::LightSource* light)
{
    return &light->getColor().x;                                                   // +0x198
}


// The two arms Renderer::renderFrame calls, and update (slot +0x28).

// the culler's per-frame step, then the water constants.
void Terrain::update()
{
    m_culler->update();
    updateWaterConstants();                                               // vslot +0xE0
}

// the first arm: push, update, the constants, the main draw, the water, pop.
void Terrain::renderWorldPassBegin()
{
    dx9rend::stateManagerPush();
    update();                                                             // vslot +0x28
    applyFrameConstants();
    if (m_drawMain)
        drawMain();
    if (m_flag35A)
        drawWater();                                                      // vslot +0xD8
    dx9rend::stateManagerPop();
}

// the closing arm: the diffuse passes, then the ring.
void Terrain::renderWorldPassEnd()
{
    dx9rend::stateManagerPush();
    if (m_drawMain)
        drawFinal();
    if (m_flag359 && m_surroundingBuilt)
        drawSurrounding();
    dx9rend::stateManagerPop();
}


// (slot +0xE0) - the water quality latch, then the water constants of the Ra block.

void Terrain::updateWaterConstants()
{
    // byte_10258F70 - g_waterQualityLatched, Terrain.cpp. The latch: a quality that differs from
    // the applied one sets it, unless the quality is still -1 (never read), which clears it and
    // reads video option 0 now; an equal pair re-reads only while the latch is clear.
    bool read;
    if (m_waterQuality == m_waterQualityApplied)
        read = terrainWaterQualityLatched() == 0;
    else
    {
        terrainWaterQualityLatched() = 1;
        read = (m_waterQuality == -1);
        if (read)
            terrainWaterQualityLatched() = 0;
    }
    if (read)
        m_waterQuality = dx9rend::rendererGetVideoOptions()->getQuality(0);   // base +0x74, then +0x04
    if (m_waterQuality != m_waterQualityApplied)
    {
        setupWater();                                                     // vslot +0xD4
        m_waterQualityApplied = m_waterQuality;
    }

    // Both arms compute in float: the x87 runs under the single-precision control word D3D9 sets for
    // a device created without D3DCREATE_FPU_PRESERVE (behaviour flags 0x50 - NameTagRenderer.cpp
    // records the same), so every product, quotient and difference below is rounded to float, and
    // is written as one (RW-240: this arm was computed in double, which moves
    // the phase and the frame pair at every cycle boundary). The truncations are 32-bit FISTPs
    // under a chop control word - a C cast toward zero, 0x80000000 when out of range, as this
    // build's cvttss2si.
    if (m_water3D)
    {
        // Retail reads GlobalTime before the vslot +0x90 call (0x100DBB52) and writes
        // WaterCycleTime = frac(GlobalTime / waterAnimSpeed) (`fdivr`, `fld st0`, `fistp`, `fild`,
        // `fsubr`). The 3D water shaders offset their texture by WaterScroll * WaterCycleTime and
        // WaterCycleTime * 10 + 0.7 * u + 1.13 * v, so the wrap makes the surface jump every
        // waterAnimSpeed / 2 seconds.
        // [CHOSEN, not recovered] B2BF/OpenBF2 issue 69: the cycle runs on unwrapped, at the same
        // rate, from the double-precision copy of GlobalTime. It wraps only every
        // kWater3DCyclePeriod cycles to keep the float constant precise in long sessions; at that
        // period every texture offset jumps by a whole number for scroll values with one decimal,
        // which covers the stock levels. The 2D arm below stays retail: it uses the constant as a
        // 0..1 frame blend.
        const double cycle = dx9rend::g_waterClock / getWaterAnimSpeed();             // vslot +0x90
        constantF(412) = static_cast<float>(std::fmod(cycle, kWater3DCyclePeriod));  // WaterCycleTime
    }
    else
    {
        // Select and blend two water frames from time/(animationSpeed * 0.2).
        // setupWater must supply a nonempty frame list; sampling has no null/empty guards.
        const float globalTime = constantF(148);                                      // GlobalTime
        const float cycleLength = getWaterAnimSpeed() * kWater2DCycleScale;           // vslot +0x90, fmul
        const float cycle = globalTime / cycleLength;                                 // fdivr
#if defined(__APPLE__)
        m_waterTextureList->phase = cycle - static_cast<float>(bf2x87::toIntTruncate(cycle));
#else
        m_waterTextureList->phase = cycle - static_cast<float>(static_cast<int>(cycle));
#endif
        dx9rend::raShaderConstantsSetEngineTexture(0x1A8, m_waterTextureList->currentFrame());  // sub_10014EC0, WaterMapFrame0
        dx9rend::raShaderConstantsSetEngineTexture(0x1B0, m_waterTextureList->nextFrame());     // sub_10014EF0, WaterMapFrame1
        constantF(412) = m_waterTextureList->frameBlend();                                      // sub_10014F30, WaterCycleTime
    }

    Vec2 scroll;
    constantF(392) = getWaterScroll(&scroll)->x;                          // vslot +0x88, called twice as shipped
    constantF(396) = getWaterScroll(&scroll)->y;
    constantF(400) = kWaterScrollZ;
    constantF(404) = kWaterScrollW;

    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float seaLevel = cluster->getSeaWaterLevel();                   // vslot +0x60, three calls as shipped
    constantF(444) = cluster->getSeaWaterLevel();                         // WaterHeight
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();       // base +0x48
        const float* const m = view->getViewMatrixPtr()->m;               // +0x64
        constantF(472) = m[13] - cluster->getSeaWaterLevel();             // CameraToWater
    }
    constantF(480) = 1.0f;                                                // WaterClipDir
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
        dx9rend::Ra::RaObjectParameters& op =
            *reinterpret_cast<dx9rend::Ra::RaObjectParameters*>(m_objectParameters);
        ::memcpy(op.viewProjection.value, view->getViewProjectionMatrixPtr()->m, 64);   // +0x84, sub_10030E80
        float* const world = op.world.value;
        world[0] = 1.0f; world[1] = 0.0f; world[2] = 0.0f; world[3] = 0.0f; world[4] = 0.0f;
        world[5] = 1.0f; world[6] = 0.0f; world[7] = 0.0f; world[8] = 0.0f; world[9] = 0.0f;
        world[10] = 1.0f; world[11] = 0.0f; world[12] = 0.0f; world[13] = 0.0f; world[14] = 0.0f;
        world[15] = 1.0f;
        world[12] = 0.0f;
        world[13] = seaLevel;
        world[14] = 0.0f;
        op.tail220 = 0;
        if (g_LightManager->getDynamicShadowsEnabled())                   // +0xF0
        {
            const std::vector<LightShadowRecord>& records = terrainShadowRecords();
            if (!records.empty())
            {
                const unsigned int count = static_cast<unsigned int>(records.size());
                if (static_cast<float>(count) != 0.0f)
                {
                    ::memcpy(op.shadowProjMat.value, &records[0].matrices[4], 64);   // +0x2F8
                    ::memcpy(op.shadowTrapMat.value, &records[0].matrices[7], 64);   // +0x3B8
                }
            }
        }
    }
    setWaterRecordConstants();
}

// sub_1018F8D0 on the seven-float record: WaterColor, then transparency and reflectivity by the
// camera's side of the water plane.
void Terrain::setWaterRecordConstants()
{
    const bool under = dx9rend::cameraIsUnderWater();
    const float* const r = m_waterRecord;
    constantF(452) = r[0];
    constantF(456) = r[1];
    constantF(460) = r[2];
    constantF(464) = 1.0f;
    constantF(488) = r[3 + 2 * (under ? 1 : 0)];                          // WaterTransparency
    constantF(496) = r[4 + 2 * (under ? 1 : 0)];                          // WaterReflectivity
}


// sub_100DB980 / sub_100DB9A0 - renderer.waterColor's get and set arms (RendererConsole.cpp): the
// record's first three floats, unguarded (the record exists from the constructor on). Placement
// beside the other water-record users is [INFERRED] from the address order.
Vec3 Terrain::getWaterColor() const
{
    const float* const r = m_waterRecord;
    Vec3 colour;
    colour.x = r[0]; colour.y = r[1]; colour.z = r[2];
    return colour;
}

void Terrain::setWaterColor(Vec3 colour)
{
    float* const r = m_waterRecord;
    r[0] = colour.x; r[1] = colour.y; r[2] = colour.z;
}

// sub_100DB9E0 / 0x100DBA00 / 0x100DBA50 / 0x100DBA90 - the terrain console's waterTransparency
// and waterReflectivity arms: the above-water floats of the record (+0x0C / +0x10), unguarded.
float Terrain::getWaterTransparency() const { return m_waterRecord[3]; }
float Terrain::getWaterReflectivity() const { return m_waterRecord[4]; }
void  Terrain::setWaterTransparency(float value) { m_waterRecord[3] = value; }
void  Terrain::setWaterReflectivity(float value) { m_waterRecord[4] = value; }

// The unreachable water-record forwarders (no code or data references in 1.50). The record's
// colour accessors scale by 255 / 1/255 (sub_1018F940 / sub_1018F970); names are [INFERRED].

// PE 0x100DB9C0. Unreachable in 1.50: no code or data references.
Vec3* Terrain::getWaterColor255(Vec3* out)
{
    const float* const r = m_waterRecord;
    out->x = r[0] * 255.0f;
    out->y = r[1] * 255.0f;
    out->z = r[2] * 255.0f;
    return out;
}

// PE 0x100DBA20. Unreachable in 1.50: no code or data references.
void Terrain::setWaterColor255(Vec3 colour)
{
    float* const r = m_waterRecord;
    r[0] = colour.x * 0.003921569f;                                           // 0x3B808081
    r[1] = colour.y * 0.003921569f;
    r[2] = colour.z * 0.003921569f;
}

// PE 0x100DBA70. Unreachable in 1.50: no code or data references.
void Terrain::setWaterTransparencyBelow(float value)
{
    m_waterRecord[5] = value;
}

// PE 0x100DBAB0. Unreachable in 1.50: no code or data references.
void Terrain::setWaterReflectivityBelow(float value)
{
    m_waterRecord[6] = value;
}

// PE 0x100DB610 (also 0x100FBFA0): the StaticMesh copy walks the same records and compares the same
// word with its own pointer. Unreachable in 1.50: no code or data references. The shipped test
// compares the record's +0x454 word with the Terrain pointer itself.
LightShadowRecord* Terrain::findOwnedShadowRecord()
{
    typedef std::vector<LightShadowRecord> RecordVector;
    RecordVector::iterator end = lightManagerPointShadowRecords(g_LightManager).end();            // +0x7DC
    RecordVector::iterator it = lightManagerPointShadowRecords(g_LightManager).begin();
    for (; it != end; ++it)
    {
        if (static_cast<const void*>(it->source) == static_cast<const void*>(this))
            break;
    }
    return it != lightManagerPointShadowRecords(g_LightManager).end() ? &*it : 0;
}

// sub_100DB030 / sub_100DB000 - terrain.ambientColor: +0x2FC as stored; the set raises +0x41C.
Vec3* Terrain::getAmbientColor(Vec3* out) const
{
    *out = m_ambientColor;
    return out;
}

void Terrain::setAmbientColor(float r, float g, float b)
{
    m_ambientColor.x = r;
    m_ambientColor.y = g;
    m_ambientColor.z = b;
    m_lightingDirty = 1;
}

// sub_100311D0 / sub_10031200 - terrain.GIColorHigh / GIColorLow's set arms: the colour as given.
// [INFERRED] placement; the shipped bodies sit outside the terrain's address range.
void Terrain::setGIColorHigh(const Vec3& colour) { m_giColorHigh = colour; }
void Terrain::setGIColorLow(const Vec3& colour)  { m_giColorLow = colour; }


// the constants worker: matVIEWPROJ, the camera, the colours and fog, the water
// colour and maps, then - when the lighting is dirty - the morph limits and the tiling constants.

void Terrain::applyFrameConstants()
{
    dx9rend::Effect* const fx = static_cast<dx9rend::Effect*>(m_effect);
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();        // base +0x50
    {
        // Fetch projection first, then multiply the inverse-view matrix by it.
        const Mat4* const proj  = view->getProjectionMatrixPtr();         // +0x6C
        const Mat4* const viewM = view->getInverseViewMatrixPtr();        // +0x68
        reinterpret_cast<Mat4*>(m_viewProj)->fullMult(*viewM, *proj);
    }
    param(m_hMatViewProj)->setValue(m_viewProj, 0);
    {
        const float* const camera = view->getViewMatrixPtr()->m + 12;     // +0x64 row 3
        const float* const packed = staticMeshPackVec4(camera, 1.0f);
        m_cameraPosition[1] = packed[1];
        m_cameraPosition[0] = packed[0];
        m_cameraPosition[2] = packed[2];
        m_cameraPosition[3] = packed[3];
    }
    if (!m_heightmapCluster)
        m_heightmapCluster = heightmapClusterByName();
    if (m_heightmapCluster)
    {
        const float cameraY = m_cameraPosition[1];
        const Vec3* const p = terrainOriginScratch(*reinterpret_cast<const Vec3*>(m_cameraPosition));
        const double h = m_heightmapCluster->getHeightInWorldCoords(*p);   // vslot +0x68
        const double d = fabs(static_cast<double>(cameraY) - h);
        m_cameraHeightAboveTerrain = static_cast<float>(d);
        m_cameraPosition[3] = static_cast<float>(d + 1.0);
    }
    param(m_hCameraPos)->setValue(m_cameraPosition, 0);
    param(m_hAmbientColor)->setValue(&m_ambientColor, 0);
    param(m_hSunColor)->setValue(&m_sunColor, 0);
    param(m_hGIColor)->setValue(&m_giColor, 0);
    {
        float single[3];
        single[0] = static_cast<float>(constantF(560) * 0.25);
        single[1] = static_cast<float>(constantF(564) * 0.25);
        single[2] = static_cast<float>(constantF(568) * 0.25);
        param(m_hSinglePointColor1X)->setValue(single, 0);
    }
    param(m_hDynamicPointColor)->setValue(constantAt(588), 0);
    param(m_hNightTimeMode)->setValue(constantAt(604), 0);
    param(m_hNightOccludedSkyIntensity)->setValue(constantAt(612), 0);
    param(m_hFogRange)->setValue(constantAt(168), 0);
    param(m_hFogColor)->setValue(constantAt(188), 0);
    {
        float range[4];                                                   // sub_100380E0: +208..+220
        range[0] = constantF(208); range[1] = constantF(212); range[2] = constantF(216); range[3] = constantF(220);
        param(m_hAmbientIlluminationRange)->setValue(range, 0);
        float colour[4];                                                  // sub_10038080: +228..+240
        colour[0] = constantF(228); colour[1] = constantF(232); colour[2] = constantF(236); colour[3] = constantF(240);
        param(m_hAmbientIlluminationColor)->setValue(colour, 0);
    }
    dx9rend::postProductionUpdateFog(dx9rend::underwaterDepth());         // sub_1004FDA0(sub_100F7B00())
    param(m_hCameraToWater)->setFloat(constantF(472));

    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    float seaLevel = cluster->getSeaWaterLevel();                         // +0x60
    {
        const float* const m = view->getViewMatrixPtr()->m;
        constantF(464) = static_cast<float>(
            (20.0 - m[13] + seaLevel) * 0.050000001 * m_waterAlphaHeightMul + m_waterAlphaAdd);   // WaterColor.w
    }
    {
        float rgb[3] = { constantF(452), constantF(456), constantF(460) };
        const float* const packed = staticMeshPackVec4(rgb, 1.0f);
        float value[4] = { packed[0], packed[1], packed[2], packed[3] };
        param(m_hTerrainWaterColor)->setValue(value, 0);
    }
    seaLevel = static_cast<float>(seaLevel * 0.33333334);
    param(m_hWaterHeight)->setValue(&seaLevel, 0);
    if (constantP(420))
        param(m_hCausticsMap)->setValue(constantP(420), 0);
    if (constantP(428))
        param(m_hCausticsMapFrame0)->setValue(constantP(428), 0);
    if (constantP(436))
        param(m_hCausticsMapFrame1)->setValue(constantP(436), 0);
    {
        float xyz[3] = { constantF(392), constantF(396), constantF(400) };
        const float* const packed = staticMeshPackVec4(xyz, 1.0f);
        float value[4] = { packed[0], packed[1], packed[2], packed[3] };
        param(m_hWaterScroll)->setValue(value, 0);
    }
    param(m_hWaterCycleTime)->setFloat(constantF(412));

    if (!m_lightingDirty)
        return;
    m_lightingDirty = 0;
    param(m_hTexLayer[4])->setValue(m_lowDetailTexture, 0);
    {
        const TerrainCuller* const culler = m_culler;
        const double lod = static_cast<double>(culler->lodDistance);
        const float near0 = static_cast<float>(lod * culler->lodFactor20);
        const float far0  = static_cast<float>(lod * culler->lodFactor24);
        const float near1 = static_cast<float>(lod * culler->lodFactor28);
        const double far1 = lod * culler->lodFactor2C;
        const double inv0 = 1.0 / (far0 - near0);
        if (kSquaredMorphLimits)
        {
            const double a = inv0;
            const double b = near0 * inv0;
            m_morphLimits[0] = static_cast<float>(a * a);
            m_morphLimits[1] = static_cast<float>(b * b);
            const double inv1 = 1.0 / (far1 - near1);
            const double c = near1 * inv1;
            m_morphLimits[2] = static_cast<float>(inv1 * inv1);
            m_morphLimits[3] = static_cast<float>(c * c);
            m_morphLimits[4] = 0.0f;
            m_morphLimits[5] = 0.0f;
        }
        else
        {
            m_morphLimits[0] = static_cast<float>(inv0);
            m_morphLimits[1] = static_cast<float>(inv0 * near0);
            const double inv1 = 1.0 / (far1 - near1);
            m_morphLimits[2] = static_cast<float>(inv1);
            m_morphLimits[3] = static_cast<float>(inv1 * near1);
            m_morphLimits[4] = -1.0f;
            m_morphLimits[5] = -1.0f;
        }
    }
    m_currentLod = -1;
    if (m_drawMain)
    {
        dx9rend::EngineTexture* const accumulation = dx9rend::rendererGetHemiScratchTexture();   // Renderer +0x774
        param(m_hTexLayer[1])->setValue(accumulation, 0);
        float halfTexel[2];
        {
            const Math::Vec2 half = textureHalfTexelSize(accumulation);       // PE 0x100846A0, call 0x100D9B10
            halfTexel[0] = half.x;
            halfTexel[1] = half.y;
        }
        if ((g_texProjOnce & 1u) == 0)
        {
            g_texProjOnce |= 1u;
            g_texProjScale[0] = 0.5f; g_texProjScale[1] = -0.5f; g_texProjScale[2] = 1.0f; g_texProjScale[3] = 1.0f;
        }
        if ((g_texProjOnce & 2u) == 0)
        {
            g_texProjOnce |= 2u;
            g_texProjOffset[2] = 0.0f;
            g_texProjOffset[3] = 0.0f;
            g_texProjOffset[0] = halfTexel[0] + 0.5f;
            g_texProjOffset[1] = halfTexel[1] + 0.5f;
        }
        param(m_hTexProjScale)->setValue(g_texProjScale, 0);
        param(m_hTexProjOffset)->setValue(g_texProjOffset, 0);
        {
            float scaleTransY[4] = { m_primaryWorldScale.y, 1.0f, 0.0f, 0.0f };
            param(m_hScaleTransY)->setValue(scaleTransY, 0);
        }
        float texScale[4];
        texScale[0] = static_cast<float>(255.0 / static_cast<double>(m_patchSize));
        texScale[1] = -0.0061514801f;
        texScale[2] = texScale[0];
        texScale[3] = 0.0f;
        param(m_hTexScale)->setValue(texScale, 0);
        const float farTop = (m_lowQualityB == 0) ? m_farTopTilingHi : m_farTopTilingLow;
        {
            float farTiling[4] = { m_farSideTiling.x, m_farSideTiling.y, farTop, m_farYOffset };
            param(m_hFarTexTiling)->setValue(farTiling, 0);
        }
        {
            float yPlane[4] = { farTop * texScale[0], farTop * texScale[1], farTop * texScale[2], farTop * texScale[3] };
            param(m_hYPlaneTexScaleAndFarTile)->setValue(yPlane, 0);
        }
        {
            const double w = static_cast<double>(
                static_cast<dx9rend::EngineTexture*>(m_patches[0]->lightmap)->width);
            float colorLightTex[2] = { 0.0f, 0.0f };
            colorLightTex[0] = static_cast<float>((w - 1.0) / w);
            colorLightTex[1] = static_cast<float>(1.0 / (w + w));
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, std::string("COLORLIGHTTEX"))->setValue(colorLightTex, 0);
        }
        if (m_patchCount * m_patchCount > 0)
        {
            int i = 0;
            bool found = true;
            while (!m_patches[i]->detailMap[0])
            {
                ++i;
                if (i >= m_patchCount * m_patchCount)
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                const double w = static_cast<double>(
                    static_cast<dx9rend::EngineTexture*>(m_patches[i]->detailMap[0])->width);
                float detailTex[2] = { 0.0f, 0.0f };
                detailTex[0] = static_cast<float>((w - 1.0) / w);
                detailTex[1] = static_cast<float>(1.0 / (w + w));
                dx9rend::effectGetShaderParameterBorrowedTyped(fx, std::string("DETAILTEX"))->setValue(detailTex, 0);
            }
        }
    }
    if (m_flag359 && m_surroundingBuilt)
    {
        {
            float stScaleTransY[4] = { m_secondaryWorldScale.y, 1.0f, 0.0f, 0.0f };
            param(m_hStScaleTransY)->setValue(stScaleTransY, 0);
        }
        {
            float stTexScale[4];
            stTexScale[0] = static_cast<float>(1.0 / static_cast<double>(m_patchSize) / m_secondaryWorldScale.x);
            stTexScale[1] = static_cast<float>(m_secondaryWorldScale.y * 400.0 * 0.0000153787);
            stTexScale[2] = stTexScale[0];
            stTexScale[3] = 0.0f;
            param(m_hStTexScale)->setValue(stTexScale, 0);
        }
        {
            const double patches = static_cast<double>(m_patchCount);
            const double quarter = m_secondaryWorldScale.x * 0.25;
            float stFar[4];
            stFar[0] = static_cast<float>(patches * m_farSideTiling.x / quarter);
            stFar[1] = m_farSideTiling.y;
            stFar[2] = static_cast<float>(
                ((m_lowQualityB == 0) ? patches * m_farTopTilingHi : patches * m_farTopTilingLow) / quarter);
            stFar[3] = m_farYOffset;
            param(m_hStFarTexTiling)->setValue(stFar, 0);
        }
        int s = 0;
        while (!m_surrounding[s])
        {
            ++s;
            if (s >= 8)
                return;
        }
        {
            const double w = static_cast<double>(
                static_cast<dx9rend::EngineTexture*>(m_surrounding[s]->colormap)->width);
            float value[2] = { 0.0f, 0.0f };
            value[0] = static_cast<float>((w - 1.0) / w);
            value[1] = static_cast<float>(1.0 / (w + w));
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, std::string("STCOLORLIGHTTEX"))->setValue(value, 0);
        }
        {
            const double w = static_cast<double>(
                static_cast<dx9rend::EngineTexture*>(m_surrounding[s]->lowDetailmap)->width);
            float value[2] = { 0.0f, 0.0f };
            value[0] = static_cast<float>((w - 1.0) / w);
            value[1] = static_cast<float>(1.0 / (w + w));
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, std::string("STLOWDETAILTEX"))->setValue(value, 0);
        }
    }
}

// The block every patch draw inlines when the LOD changes: morphdeltaselector (one-hot LOD times
// primary.y), nearfarmorphlimits (this LOD's pair) and the morphdeltaadder rows, which exist only
// for version 0x1A content.
void Terrain::setMorphConstants(int lod)
{
    m_currentLod = lod;
    const float py = m_primaryWorldScale.y;
    float selector[4];
    selector[0] = static_cast<float>(lod == 0);
    selector[1] = static_cast<float>(lod == 1);
    selector[2] = static_cast<float>(lod == 2);
    selector[3] = static_cast<float>(lod == 3);
    selector[0] = selector[0] * py;
    selector[1] = selector[1] * py;
    selector[2] = selector[2] * py;
    selector[3] = selector[3] * py;
    param(m_hMorphDeltaSelector)->setValue(selector, 0);
    param(m_hNearFarMorphLimits)->setValue(&m_morphLimits[2 * lod], 0);
    float adder[12];
    for (int i = 0; i < 12; ++i)
        adder[i] = 0.0f;
    if (m_rawHasVersion && m_rawVersion >= 0x1Au)
    {
        const float zero = 0.0f * py;
        if (lod)
        {
            adder[2] = py;  adder[0] = zero; adder[1] = zero; adder[3] = zero;
            if (lod == 1)
            {
                adder[4] = py;  adder[5] = py * 0.0f; adder[6] = adder[5]; adder[7] = adder[5];
                adder[10] = py; adder[8] = zero; adder[9] = zero; adder[11] = zero;
            }
            else
            {
                adder[6] = py;  adder[4] = zero; adder[5] = zero; adder[7] = zero;
                adder[9] = py;  adder[8] = zero; adder[10] = zero; adder[11] = zero;
            }
        }
        else
        {
            adder[2] = py;  adder[0] = zero; adder[1] = zero; adder[3] = zero;
            adder[6] = py;  adder[4] = zero; adder[5] = zero; adder[7] = zero;
            adder[10] = py; adder[8] = zero; adder[9] = zero; adder[11] = zero;
        }
    }
    param(m_hMorphDeltaAdder)->setValue(adder, 0);
}

// sub_100D8570 - the discard lock of the dynamic index buffer, once per frame.
void Terrain::lockDynamicIndexBuffer()
{
    if (!m_indexBufferLocked)
    {
        m_indexBufferBits = dx9rend::indexBufferLock(m_indexBuffer, 0, 0, 0x2000u);   // D3DLOCK_DISCARD
        m_indexBufferLocked = 1;
    }
}

// PE 0x100D85A0. Unreachable in 1.50: no code or data references; drawMain inlines the same
// unlock.
void Terrain::unlockDynamicIndexBuffer()
{
    if (m_indexBufferLocked)
    {
        dx9rend::indexBufferUnlock(m_indexBuffer);
        m_indexBufferBits = 0;
        m_indexBufferLocked = 0;
    }
}


// PE 0x100DB2F0. The argument is not read.
void Terrain::drawDepthComplexity(int)
{
    update();
    Mat4 viewProjection = Mat4::getIdentity();
    dx9rend::IRenderView* const view =
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup();
    const Mat4* const projection = view->getProjectionMatrixPtr();
    const Mat4* const inverseView = view->getInverseViewMatrixPtr();
    const float* const a = inverseView->m;
    const float* const b = projection->m;
    // PE 0x10077A30: each component has its own shipped association and store order.
    // [CHOSEN, not recovered] Native macOS retains the x87 intermediate width.
#if defined(__APPLE__)
    using DepthReal = bf2x87::Real;
#else
    using DepthReal = double;
#endif
    viewProjection.m[0] = static_cast<float>(((((DepthReal(a[0]) * DepthReal(b[0])) + (DepthReal(b[8]) * DepthReal(a[2]))) + (DepthReal(a[1]) * DepthReal(b[4]))) + (DepthReal(a[3]) * DepthReal(b[12]))));
    viewProjection.m[4] = static_cast<float>(((((DepthReal(a[5]) * DepthReal(b[4])) + (DepthReal(b[8]) * DepthReal(a[6]))) + (DepthReal(a[7]) * DepthReal(b[12]))) + (DepthReal(b[0]) * DepthReal(a[4]))));
    viewProjection.m[8] = static_cast<float>(((((DepthReal(a[11]) * DepthReal(b[12])) + (DepthReal(a[8]) * DepthReal(b[0]))) + (DepthReal(a[10]) * DepthReal(b[8]))) + (DepthReal(a[9]) * DepthReal(b[4]))));
    viewProjection.m[12] = static_cast<float>(((((DepthReal(b[8]) * DepthReal(a[14])) + (DepthReal(a[13]) * DepthReal(b[4]))) + (DepthReal(a[15]) * DepthReal(b[12]))) + (DepthReal(a[12]) * DepthReal(b[0]))));
    viewProjection.m[1] = static_cast<float>(((((DepthReal(b[13]) * DepthReal(a[3])) + (DepthReal(a[1]) * DepthReal(b[5]))) + (DepthReal(b[1]) * DepthReal(a[0]))) + (DepthReal(b[9]) * DepthReal(a[2]))));
    viewProjection.m[5] = static_cast<float>(((((DepthReal(a[5]) * DepthReal(b[5])) + (DepthReal(b[1]) * DepthReal(a[4]))) + (DepthReal(a[7]) * DepthReal(b[13]))) + (DepthReal(a[6]) * DepthReal(b[9]))));
    viewProjection.m[9] = static_cast<float>(((((DepthReal(b[5]) * DepthReal(a[9])) + (DepthReal(b[1]) * DepthReal(a[8]))) + (DepthReal(a[11]) * DepthReal(b[13]))) + (DepthReal(a[10]) * DepthReal(b[9]))));
    viewProjection.m[13] = static_cast<float>(((((DepthReal(b[1]) * DepthReal(a[12])) + (DepthReal(b[13]) * DepthReal(a[15]))) + (DepthReal(b[9]) * DepthReal(a[14]))) + (DepthReal(b[5]) * DepthReal(a[13]))));
    viewProjection.m[2] = static_cast<float>(((((DepthReal(b[14]) * DepthReal(a[3])) + (DepthReal(a[1]) * DepthReal(b[6]))) + (DepthReal(b[2]) * DepthReal(a[0]))) + (DepthReal(b[10]) * DepthReal(a[2]))));
    viewProjection.m[6] = static_cast<float>(((((DepthReal(a[5]) * DepthReal(b[6])) + (DepthReal(b[2]) * DepthReal(a[4]))) + (DepthReal(a[7]) * DepthReal(b[14]))) + (DepthReal(a[6]) * DepthReal(b[10]))));
    viewProjection.m[10] = static_cast<float>(((((DepthReal(b[6]) * DepthReal(a[9])) + (DepthReal(b[2]) * DepthReal(a[8]))) + (DepthReal(a[11]) * DepthReal(b[14]))) + (DepthReal(a[10]) * DepthReal(b[10]))));
    viewProjection.m[14] = static_cast<float>(((((DepthReal(b[2]) * DepthReal(a[12])) + (DepthReal(b[14]) * DepthReal(a[15]))) + (DepthReal(b[10]) * DepthReal(a[14]))) + (DepthReal(b[6]) * DepthReal(a[13]))));
    viewProjection.m[3] = static_cast<float>(((((DepthReal(a[0]) * DepthReal(b[3])) + (DepthReal(b[15]) * DepthReal(a[3]))) + (DepthReal(b[11]) * DepthReal(a[2]))) + (DepthReal(a[1]) * DepthReal(b[7]))));
    viewProjection.m[7] = static_cast<float>(((((DepthReal(b[11]) * DepthReal(a[6])) + (DepthReal(a[5]) * DepthReal(b[7]))) + (DepthReal(b[3]) * DepthReal(a[4]))) + (DepthReal(a[7]) * DepthReal(b[15]))));
    viewProjection.m[11] = static_cast<float>(((((DepthReal(a[8]) * DepthReal(b[3])) + (DepthReal(b[7]) * DepthReal(a[9]))) + (DepthReal(a[11]) * DepthReal(b[15]))) + (DepthReal(a[10]) * DepthReal(b[11]))));
    viewProjection.m[15] = static_cast<float>(((((DepthReal(b[11]) * DepthReal(a[14])) + (DepthReal(b[15]) * DepthReal(a[15]))) + (DepthReal(b[7]) * DepthReal(a[13]))) + (DepthReal(a[12]) * DepthReal(b[3]))));
    param(m_hMatViewProj)->setValue(&viewProjection, 0);
    float scaleTransY[4] = { m_primaryWorldScale.y, 1.0f, 0.0f, 0.0f };
    param(m_hScaleTransY)->setValue(scaleTransY, 0);
    if (m_compactVertices)
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::indexBufferBind(m_lodIndexBuffer);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    const Mat4* const cameraMatrix = view->getViewMatrixPtr();
    float camera[3] = { cameraMatrix->m[12], cameraMatrix->m[13], cameraMatrix->m[14] };
    {
        const std::string name("CAMERAPOS");
        dx9rend::effectGetShaderParameterBorrowedTyped(
            static_cast<dx9rend::Effect*>(m_effect), name)->setValue(camera, 0);
    }
    dx9rend::stateManagerPush();
    dx9rend::effectSetTechniqueByName(m_effect, std::string("RPDirectX9DepthComplexity"));
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    std::vector<TerrainCullerNode*>* const list = m_culler->visiblePatches();
    TerrainCullerNode** it = list->data();
    // Shipped defect: the obsolete patch draw only emits an error and never advances it.
    while (it != list->data() + list->size())
        (*it)->patch->drawDepthComplexity(it, 0);
    m_culler->filterOccluded();

    dx9rend::effectBeginPass(m_effect, 1);
    it = list->data();
    while (it != list->data() + list->size())
    {
        QuadPatch* const patch = (*it)->patch;
        param(m_hTexLayer[0])->setValue(patch->colormap, 0);
        patch->drawDepthComplexity(it, 1);
    }
    if (m_compactVertices)
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::indexBufferBind(m_lodIndexBuffer);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::effectBeginPass(m_effect, 2);
    it = list->data();
    while (it != list->data() + list->size())
    {
        QuadPatch* const patch = (*it)->patch;
        param(m_hTexLayer[0])->setValue(patch->lightmap, 0);
        patch->drawDepthComplexity(it, 2);
    }
    dx9rend::effectBeginPass(m_effect, 3);
    it = list->data();
    while (it != list->data() + list->size())
    {
        QuadPatch* const patch = (*it)->patch;
        param(m_hTexLayer[0])->setValue(patch->colormap, 0);
        param(m_hTexLayer[1])->setValue(patch->lightmap, 0);
        patch->drawDepthComplexity(it, 3);
    }
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
}


// PE 0x100DB810. [INFERRED] Method and argument names.
void Terrain::drawTechniquePass(Frustum* source, int pass, int preserveColorMask)
{
    std::vector<TerrainCullerNode*>* list;
    if (source)
    {
        m_culler->gatherVisibleFrustum(source);
        list = m_culler->gatheredPatches();
    }
    else
        list = m_culler->visiblePatches();

    const bool high = dx9rend::effectHasTechnique(m_effect, std::string("Hi_Terrain"));
    dx9rend::effectSetTechniqueByName(m_effect, std::string(high ? "Hi_Terrain" : "Low_Terrain"));
    dx9rend::effectBegin(m_effect);
    dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::indexBufferBind(m_lodIndexBuffer);
    dx9rend::effectBeginPass(m_effect, static_cast<unsigned int>(pass));
    if (static_cast<unsigned char>(preserveColorMask) == 0)
        dx9rend::stateManagerSetRenderState(168u, 0u);

    TerrainCullerNode** it = list->data();
    while (it != list->data() + list->size())
    {
        QuadPatch* const patch = (*it)->patch;
        param(m_hTexLayer[0])->setValue(patch->lightmap, 0);
        patch->drawTechniquePass(it, list);
        ++terrainRendererStat(612);
    }
    if (static_cast<unsigned char>(preserveColorMask) == 0)
        dx9rend::stateManagerSetRenderState(168u, 15u);
    dx9rend::effectEnd(m_effect);
}

// the main draw. The shadow-range gather, the accumulation target, the technique,
// pass 7 for the gathered ranges, pass 0 per visible node, the light pass, the target restore.

// PE 0x100E0940.
void Terrain::drawMain()
{
    std::vector<TerrainCullerNode*>* const list = m_culler->visiblePatches();
    TerrainCullerNode** it = 0;
    m_shadowPatches.clear();                                              // sub_100E1C20(0, 0)
    QuadPatch* previous = 0;
    m_shadowRanges.clear();                                               // sub_100E2430(0, ...)

    if (terrainShadowRecordCount() != 0u                                  // the LightManager's +0x79C records
        && g_LightManager->getShadowUpdatesEnabled()                      // +0x128
        && m_shadowGatherThreshold > 0.1f
        && m_indexBufferCount > 1000u)
    {
        m_shadowExtrusionScale = m_shadowGatherThreshold;
        const Vec3 sun = g_LightManager->getSunDirection();               // +0x1A4
        const double v = 0.64300001 - (-static_cast<double>(sun.y));      // flt_1021A31C
        int cursor = 0;
        m_shadowExtrusionScale = static_cast<float>((v + 1.0) * m_shadowExtrusionScale);
        lockDynamicIndexBuffer();
        it = list->data();
        TerrainCullerNode** const end = list->data() + list->size();
        while (it != end)
        {
            QuadPatch* const patch = (*it)->patch;
            if (previous != patch)
            {
                if (m_indexBufferCount < static_cast<unsigned int>(cursor + 12))
                    break;
                previous = patch;
                if (patch->gatherShadowRanges(&m_indexBufferBits, &cursor))
                    m_shadowPatches.push_back(patch);
            }
            ++it;
        }
        if (m_indexBufferLocked)
        {
            dx9rend::indexBufferUnlock(m_indexBuffer);
            m_indexBufferBits = 0;
            m_indexBufferLocked = 0;
        }
    }

    if (!isMultisampleActive())
    {
        lightManagerSetRenderTargetTexture(0, dx9rend::rendererGetHemiScratchTexture());   // sub_10005920(0, +0x774, 0, 0x7FFFFFFF)
        dx9rend::stateManagerClear(1u, 0xFF000000u, 0.0f, 0u);           // sub_100057E0(1, -16777216, 0, 0)
    }
    dx9rend::renderQueueFlush(0x44u, 1);                                  // RSMTAux0..2 through their +0x08 slot
    dx9rend::renderQueueFlush(0x54u, 1);
    dx9rend::renderQueueFlush(0x64u, 1);
    dx9rend::stateManagerSetRenderState(15u, 0u);                         // D3DRS_ALPHATESTENABLE
    if (m_compactVertices)
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);

    const char* technique;
    if (!m_lowQuality)
    {
        if (dx9rend::effectHasTechnique(m_effect, std::string("Hi_Terrain")))
            technique = "Hi_Terrain";
        else
        {
            // ShaderManager_updateShaderDefines(dword_1023B97C, this) as shipped - the callee
            // re-reads the renderer base from its global, which is what the tree's signature takes.
            dx9rend::updateShaderDefines(dx9rend::g_shaderManager, dx9rend::g_rendererBase);
            dx9rend::shaderManagerReloadAllEffects(dx9rend::g_shaderManager);
            if (dx9rend::effectHasTechnique(m_effect, std::string("Hi_Terrain")))
                technique = "Hi_Terrain";
            else
            {
                if (m_lowQuality != 1)
                {
                    m_lowQuality = 1;
                    m_lowQualityB = 1;
                    reloadForLowQuality();
                }
                technique = "Low_Terrain";
            }
        }
    }
    else
        technique = "Low_Terrain";
    dx9rend::effectSetTechniqueByName(m_effect, std::string(technique));
    dx9rend::effectBegin(m_effect);

    if (g_LightManager->getDynamicShadowsEnabled())                       // +0xF0
    {
        const unsigned int count = static_cast<unsigned int>(m_shadowRanges.size());
        if (count)
        {
            dx9rend::effectBeginPass(m_effect, 7u);
            dx9rend::indexBufferBind(m_indexBuffer);
            param(m_hShadowMap)->setValue(lightManagerShadowMapTexture(g_LightManager), 0);  // +0x21C
            for (std::vector<QuadPatch*>::iterator p = m_shadowPatches.begin();
                 p != m_shadowPatches.end(); ++p)
                (*p)->drawShadow();
        }
    }
    dx9rend::indexBufferBind(m_lodIndexBuffer);
    dx9rend::effectBeginPass(m_effect, 0u);
    it = list->data();
    while (it != list->data() + list->size())
    {
        param(m_hTexLayer[0])->setValue((*it)->patch->lightmap, 0);
        (*it)->patch->draw(it);                                           // sub_1018BE20, advances `it`
        ++terrainRendererStat(612);
    }
    if (dx9rend::rendererGetHemiRenderMode() != 4)                        // sub_10009F00, base +0x20
    {
        if (m_effect && *dx9rend::effectShaderVersion(m_effect) == 1)
            drawDetailPass();
        else
            drawMaskLights();
    }
    if (isMultisampleActive())
    {
        IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(
            dx9rend::rendererGetHemiScratchTexture(), 0, 0x7FFFFFFFu);
        IDirect3DSurface9* const backBuffer = dx9rend::rendererBackBufferSurface();   // Renderer +0x76C
        dx9rend::Ra::raRendererDevice()->StretchRect(backBuffer, 0, surface, 0, D3DTEXF_NONE);   // device +0x88
        surface->Release();
        dx9rend::stateManagerClear(1u, 0u, 0.0f, 0u);
    }
    else
        lightManagerRestoreRendererTarget0();
    dx9rend::effectEnd(m_effect);
}


// the ShaderVersion-1 detail/light pass. read in full: pass 16, multipleshadowmap from
namespace {

// PE 0x100E2550/0x100E29D0/0x100E1110: fixed-capacity projection arrays and their range list.
struct TerrainLightBatch
{
    Mat4* matrices;
    Vec4* positions;
    Vec4* sizes;
    int capacity;
    int count;
    std::vector<TerrainShadowRange> ranges;

    // PE 0x100E2550; range storage is reserved by 0x100E2320.
    explicit TerrainLightBatch(int limit) : capacity(limit), count(0)
    {
        matrices = static_cast<Mat4*>(::renddx9abi::engineAlloc(limit * sizeof(Mat4)));
        if (matrices)
            for (int i = 0; i != limit; ++i)
                matrices[i] = Mat4::getIdentity();
        positions = static_cast<Vec4*>(::renddx9abi::engineAlloc(limit * sizeof(Vec4)));
        if (positions)
            std::memset(positions, 0, limit * sizeof(Vec4));
        sizes = static_cast<Vec4*>(::renddx9abi::engineAlloc(limit * sizeof(Vec4)));
        if (sizes)
            std::memset(sizes, 0, limit * sizeof(Vec4));
        ranges.reserve(limit);
    }

    // Cleanup at PE 0x100E2CBB/0x100E0423/0x100E08E3 precedes the range-vector destructor.
    ~TerrainLightBatch()
    {
        ::renddx9abi::engineFree(matrices);
        ::renddx9abi::engineFree(positions);
        ::renddx9abi::engineFree(sizes);
    }

    void clear() { ranges.clear(); count = 0; }

    // PE 0x100E29D0.
    bool append(const TerrainShadowRange& range, const Mat4& matrix, const Vec4& position, const Vec4& size)
    {
        if (count >= capacity)
            return false;
        matrices[count] = matrix;
        positions[count] = position;
        sizes[count] = size;
        ++count;
        ranges.push_back(range);
        return true;
    }

    // PE 0x100E1110.
    void apply() const
    {
        if (count > 0)
        {
            param(terrainShadowArrayParameter(0))->setValue(matrices, count);
            param(terrainShadowArrayParameter(1))->setValue(positions, count);
            param(terrainShadowArrayParameter(2))->setValue(sizes, count);
        }
        param(terrainShadowArrayParameter(3))->setInt(count);
    }
};

// PE 0x10079340's matrix, followed by both sequential PE 0x10077A30 products.
void terrainReceiverProjection(const LightShadowRecord& record, Mat4& output)
{
    static const Mat4 bias = {{ 0.5f,0,0,0, 0,-0.5f,0,0, 0,0,1,0, 0.5f,0.5f,0,1 }};
    output = Mat4::getIdentity();
    geometryRendererMatrixProduct(output, record.matrices[1], record.matrices[2]);
    geometryRendererMatrixProduct(output, output, bias);
}

float terrainInverseRangeSquared(float radius)
{
#if defined(__APPLE__)
    // [CHOSEN, not recovered] Retain the PE's x87 intermediates on the native port.
    return bf2x87::store(bf2x87::div(bf2x87::wide(1.0f),
        bf2x87::mul(bf2x87::wide(radius), bf2x87::wide(radius))));
#else
    return 1.0f / (radius * radius);
#endif
}

float terrainScaledInverseRangeSquared(float radius, float scale)
{
#if defined(__APPLE__)
    // [CHOSEN, not recovered] The scaled radius is not stored before squaring in the PE.
    const bf2x87::Real scaled = bf2x87::mul(bf2x87::wide(scale), bf2x87::wide(radius));
    return bf2x87::store(bf2x87::div(bf2x87::wide(1.0f), bf2x87::mul(scaled, scaled)));
#else
    const float scaled = scale * radius;
    return 1.0f / (scaled * scaled);
#endif
}

float terrainConeCosine(float degrees)
{
    const float radians = 0.01745329238474369049f; // PE 0x1021A318
#if defined(__APPLE__)
    // [CHOSEN, not recovered] Retain the multiply through the native x87 cosine policy.
    return bf2x87::store(bf2x87::cosValue(bf2x87::mul(bf2x87::wide(degrees), bf2x87::wide(radians))));
#else
    return static_cast<float>(std::cos(degrees * radians));
#endif
}

// PE 0x100DF900 and 0x100E2BD0: append every record of each overlapping dynamic wrapper.
void terrainCollectDynamicRecords(QuadPatch* patch, std::vector<const LightShadowRecord*>& output)
{
    // PE 0x10239214 is 1 and has no writer or data-table reference in the image.
    const std::vector<XpLight*>& lights = xpDynamicFrameLights();
    for (XpLight* const* it = lights.data(); it != lights.data() + lights.size(); ++it)
    {
        world::LightSource* const source = xpLightSource(*it);
        const float radius = xpLightRadius(*it);
        if (patch->intersectsLightSphere(source->getAbsolutePosition(), radius))
        {
            const std::vector<LightShadowRecord>& records = xpLightShadowRecords(*it);
            for (const LightShadowRecord* record = records.data();
                 record != records.data() + records.size(); ++record)
                output.push_back(record);
        }
    }
}

// PE 0x100E2A50. The image's 0x10239210 draw switch is fixed at 1.
void terrainFlushShadowBatch(TerrainLightBatch& batch, QuadPatch* patch,
                             TerrainCullerNode** first, std::vector<TerrainCullerNode*>* visible)
{
    if (batch.count == 0)
        return;
    batch.apply();
    if (!dx9rend::rendererPerformanceFlag(12))
    {
        patch->drawShadowBatch(first, visible, batch.ranges);
        ++terrainRendererStat(0x264);
    }
    batch.clear();
}

// PE 0x100E2C90: the per-patch shadow batch's teardown - the last flush, then the cursor moved past
// the patch's visible nodes (the list end is read once). The three array frees and the range
// vector's release follow in ~TerrainLightBatch.
void terrainFinishShadowBatch(TerrainLightBatch& batch, QuadPatch* patch, TerrainCullerNode**& node,
                              std::vector<TerrainCullerNode*>* visible)
{
    terrainFlushShadowBatch(batch, patch, node, visible);
    TerrainCullerNode** const end = visible->data() + visible->size();
    while (node != end && (*node)->patch == patch)
        ++node;
}

// PE 0x100E2AF0. Even an empty batch uploads count zero and traverses the gather; the cursor is
// a copy, so the caller's range start does not move.
void terrainFlushPointBatch(TerrainLightBatch& batch, TerrainCullerNode** first,
                            TerrainCullerNode**& end)
{
    batch.apply();
    while (first != end)
    {
        QuadPatch* const patch = (*first)->patch;
        patch->drawPointLight(first);
    }
    batch.clear();
}

// PE 0x100E2B60. The spot-light twin of terrainFlushPointBatch.
void terrainFlushSpotBatch(TerrainLightBatch& batch, TerrainCullerNode** first,
                           TerrainCullerNode**& end)
{
    batch.apply();
    while (first != end)
    {
        QuadPatch* const patch = (*first)->patch;
        patch->drawSpotLight(first);
    }
    batch.clear();
}

void terrainAppendProjection(TerrainLightBatch& batch, const TerrainShadowRange& range,
                             const LightShadowRecord& record, const Mat4& matrix)
{
    const Vec4 position = { record.atlasScaleOffset[0], record.atlasScaleOffset[1], 0.0f, 0.0f };
    const Vec4 size = { record.atlasScaleOffset[2], record.atlasScaleOffset[3], 0.0f, 0.0f };
    batch.append(range, matrix, position, size);
}

} // anonymous namespace

// PE 0x100DF990. The zero switches at 0x102592D4/D1/D0/CC/C8 and the one switch at
// 0x10239218 have no image writers or data-table references. Clip-plane and direct-draw debug
// arms are consequently unreachable; the renderer's performance flags remain live queries.
void Terrain::drawDetailPass()
{
    std::vector<TerrainCullerNode*>* const visible = m_culler->visiblePatches();
    dx9rend::effectBeginPass(m_effect, 16);
    param(m_hMultipleShadowMap)->setValue(lightManagerPointShadowMapTexture(g_LightManager), 0);
    static std::vector<TerrainShadowRange> ranges;                  // PE 0x102592E8
    static std::vector<const LightShadowRecord*> dynamicRecords;   // PE 0x102592D8
    ranges.clear();
    dynamicRecords.clear();

    if (!dx9rend::rendererPerformanceFlag(8))
    {
        unsigned short* const indices = static_cast<unsigned short*>(
            dx9rend::indexBufferLock(m_indexBuffer, 0, m_indexBufferCount, 0x2000));
        int cursor = 0;
        QuadPatch* previous = 0;
        for (TerrainCullerNode* const* node = visible->data(); node != visible->data() + visible->size(); ++node)
        {
            QuadPatch* const patch = (*node)->patch;
            if (patch != previous)
            {
                if (m_indexBufferCount < static_cast<unsigned int>(cursor) + 12u)
                    break;
                // shipped quirk: this scratch list is not cleared between patches.
                terrainCollectDynamicRecords(patch, dynamicRecords);
                for (const LightShadowRecord* const* record = dynamicRecords.data();
                     record != dynamicRecords.data() + dynamicRecords.size(); ++record)
                {
                    if (m_indexBufferCount < static_cast<unsigned int>(cursor) + 12u)
                        break;
                    if (!dx9rend::rendererPerformanceFlag(9))
                        patch->gatherShadowRecord(indices, &cursor, const_cast<LightShadowRecord*>(*record),
                                                  ranges, 1.0f, 0);
                }
            }
            previous = patch;
        }
        dx9rend::indexBufferUnlock(m_indexBuffer);
        dx9rend::indexBufferBind(m_indexBuffer);
    }
    else if (dx9rend::rendererPerformanceFlag(10))
    {
        QuadPatch* previous = 0;
        for (TerrainCullerNode* const* node = visible->data(); node != visible->data() + visible->size(); ++node)
        {
            QuadPatch* const patch = (*node)->patch;
            if (patch != previous)
            {
                terrainCollectDynamicRecords(patch, dynamicRecords);
                for (const LightShadowRecord* const* record = dynamicRecords.data();
                     record != dynamicRecords.data() + dynamicRecords.size(); ++record)
                {
                    TerrainShadowRange range; // start and distance are deliberately unwritten.
                    range.patchIndex = -1;
                    range.count = 0;
                    range.node = *node;
                    range.shadowRecord = reinterpret_cast<const unsigned char*>(*record);
                    ranges.push_back(range);
                }
            }
            previous = patch;
        }
    }

    if (!dx9rend::rendererPerformanceFlag(11))
    {
        TerrainShadowRange* range = ranges.data();
        TerrainCullerNode** node = visible->data();
        while (node != visible->data() + visible->size())
        {
            QuadPatch* const patch = (*node)->patch;
            std::vector<TerrainCullerNode*>* const batchVisible = m_culler->visiblePatches();
            TerrainLightBatch batch(1);
            world::LightSource* previous = 0;
            while (range != ranges.data() + ranges.size() && range->node->patch == patch)
            {
                const LightShadowRecord& shadow = *reinterpret_cast<const LightShadowRecord*>(range->shadowRecord);
                if (!shadow.casters.empty())
                {
                    if (shadow.source != previous)
                    {
                        terrainFlushShadowBatch(batch, patch, node, batchVisible);
                        previous = shadow.source;
                    }
                    const Mat4& transform = shadow.source->getAbsoluteTransformation();
                    param(m_hLightPos1)->setValue(transform.m + 12, 0);
                    param(m_hLightAttSqrInv1)->setFloat(terrainInverseRangeSquared(shadow.source->getAttenuationRange2()));
                    Mat4 projection;
                    terrainReceiverProjection(shadow, projection);
                    param(m_hLightViewProj)->setValue(&projection, 0);
                    param(m_hViewportMap)->setValue(shadow.atlasBounds, 0);
                    if (batch.count >= batch.capacity)
                        terrainFlushShadowBatch(batch, patch, node, batchVisible);
                    terrainAppendProjection(batch, *range, shadow, projection);
                }
                ++range;
            }
            terrainFinishShadowBatch(batch, patch, node, batchVisible);
        }
        dx9rend::stateManagerSetRenderState(27, 0);
    }
    dx9rend::stateManagerSetRenderState(168, 15);
    dx9rend::indexBufferBind(m_lodIndexBuffer);

    const std::vector<XpLight*>& pointLights = xpStaticFrameLights();
    for (XpLight* const* light = pointLights.data(); light != pointLights.data() + pointLights.size(); ++light)
    {
        XpLight* const wrapper = *light;
        world::LightSource* const source = xpLightSource(wrapper);
        if (xpLightType(wrapper) == 2)
            continue;
        struct PointRecord { Vec3 position; float inverseRange; Vec3 color; } point;
        static_assert(sizeof(PointRecord) == 7 * sizeof(float), "point-light shader payload");
        point.position = { 0.0f, 0.0f, 0.0f };
        point.color = { 0.0f, 0.0f, 0.0f };
        point.position = source->getAbsolutePosition();
        const float radius = source->getAttenuationRange2();
        point.inverseRange = terrainScaledInverseRangeSquared(radius, g_LightManager->getDynamicLightRangeScale());
        point.color = source->getColor();
        param(m_hPointLight)->setValue(&point, 0);
        const std::vector<LightShadowRecord>& records = xpLightShadowRecords(wrapper);
        const float gatherRadius = xpLightRadius(wrapper);
        m_culler->gatherVisibleSphere(&source->getAbsolutePosition().x, gatherRadius);
        std::vector<TerrainCullerNode*>* const gathered = m_culler->gatheredPatches();
        TerrainCullerNode** first = gathered->data();
        TerrainCullerNode** end = gathered->data() + gathered->size();
        TerrainLightBatch batch(6);
        for (const LightShadowRecord* shadow = records.data(); shadow != records.data() + records.size(); ++shadow)
        {
            if (shadow->casters.empty())
                continue;
            Mat4 projection;
            terrainReceiverProjection(*shadow, projection);
            param(m_hLightViewProj)->setValue(&projection, 0);
            param(m_hViewportMap)->setValue(shadow->atlasBounds, 0);
            const TerrainShadowRange empty = {};
            if (batch.count >= batch.capacity)
            {
                terrainFlushPointBatch(batch, first, end);
                if (batch.count >= batch.capacity)
                    continue;
            }
            terrainAppendProjection(batch, empty, *shadow, projection);
        }
        terrainFlushPointBatch(batch, first, end);
    }

    const std::vector<XpLight*>& spotLights = xpStaticFrameLights();
    // PE 0x102592B8 has no element writer/reader. [UNRESOLVED] element type;
    // [CHOSEN, not recovered] pointer-sized words retain the always-empty native container.
    static std::vector<std::uintptr_t> unusedRecords;
    unusedRecords.clear();
    XpLight* const* light = spotLights.data();
    while (light != spotLights.data() + spotLights.size())
    {
        XpLight* const wrapper = *light;
        world::LightSource* const source = xpLightSource(wrapper);
        if (xpLightType(wrapper) != 1)
        {
            struct SpotRecord { Vec3 position; float inverseRange; Vec3 color; float outer; Vec3 direction; float inner; } spot;
            static_assert(sizeof(SpotRecord) == 12 * sizeof(float), "spot-light shader payload");
            spot.position = { 0.0f, 0.0f, 0.0f };
            spot.color = { 0.0f, 0.0f, 0.0f };
            spot.direction = { 0.0f, 0.0f, 0.0f };
            spot.position = source->getAbsolutePosition();
            spot.inverseRange = terrainInverseRangeSquared(source->getAttenuationRange2());
            spot.color = source->getColor();
            const float innerAngle = source->getConeAngle1();
            spot.outer = terrainConeCosine(source->getConeAngle2());
            const Mat4& transform = source->getAbsoluteTransformation();
            spot.direction.x = transform.m[8];
            spot.direction.y = transform.m[9];
            spot.direction.z = transform.m[10];
            spot.inner = terrainConeCosine(innerAngle);
            param(m_hSpotLight)->setValue(&spot, 0);
            while (light != spotLights.data() + spotLights.size() && *light == wrapper)
                ++light;
            const std::vector<LightShadowRecord>& records = xpLightShadowRecords(wrapper);
            if (!records.empty())
            {
                xpLightGatherTerrain(wrapper, m_culler);
                std::vector<TerrainCullerNode*>* const gathered = m_culler->gatheredPatches();
                TerrainCullerNode** first = gathered->data();
                TerrainCullerNode** end = gathered->data() + gathered->size();
                TerrainLightBatch batch(6);
                for (const LightShadowRecord* shadow = records.data(); shadow != records.data() + records.size(); ++shadow)
                {
                    if (shadow->casters.empty())
                        continue;
                    Mat4 projection;
                    terrainReceiverProjection(*shadow, projection);
                    param(m_hLightViewProj)->setValue(&projection, 0);
                    param(m_hViewportMap)->setValue(shadow->atlasBounds, 0);
                    const TerrainShadowRange empty = {};
                    if (batch.count >= batch.capacity)
                    {
                        terrainFlushSpotBatch(batch, first, end);
                        if (batch.count >= batch.capacity)
                            continue;
                    }
                    terrainAppendProjection(batch, empty, *shadow, projection);
                }
                terrainFlushSpotBatch(batch, first, end);
            }
        }
        // shipped defect: advances again after consuming equal wrappers, skipping the next
        // entry or stepping past end. The original loop still tests inequality with end.
        ++light;
    }
}


// Draws visible dynamic point lights on gathered terrain patches.
// Attenuation uses dynamicLightRangeScale, while the gather radius remains unscaled.

void Terrain::drawMaskLights()
{
    static std::vector<world::LightSource*> s_lights;                     // unk_102592A4 (the once-only seed and atexit)
    s_lights.clear();                                                     // sub_100E1C20(0, 0)
    g_LightManager->getVisibleLightsByMask(s_lights, 2313u, 0u);          // +0x18
    for (std::size_t i = 0; i != s_lights.size(); ++i)                    // the end re-read every pass
    {
        world::LightSource* const light = s_lights[i];
        struct PointLightRecord                                           // pointlight, 0x1C bytes
        {
            Vec3  position;                                               // [ebp-0x24]
            float invRangeSq;                                             // [ebp-0x18]
            Vec3  color;                                                  // [ebp-0x14]
        } record;
        record.position.x = 0.0f;
        record.position.y = 0.0f;
        record.position.z = 0.0f;
        record.color.x = 0.0f;
        record.color.y = 0.0f;
        record.color.z = 0.0f;
        record.position = light->getAbsolutePosition();                  // IObject PE +0x68
        const float range = light->getAttenuationRange2();               // LightSource PE +0x190
        const float scaled = g_LightManager->getDynamicLightRangeScale() * range;   // +0x2E0
        record.invRangeSq = 1.0f / (scaled * scaled);                    // fld st0; fmul st1; fdivr 1.0f
        record.color = light->getColor();                                // LightSource PE +0x198
        param(m_hPointLight)->setValue(&record, 0);                      // +0x264
        const float radius = light->getVisualBoundingRadius();           // PE +0x90 - asked first
        m_culler->gatherVisibleSphere(&light->getAbsolutePosition().x, radius);
        std::vector<TerrainCullerNode*>* const list = m_culler->gatheredPatches();
        TerrainCullerNode** it = list->data();
        while (it != list->data() + list->size())
            (*it)->patch->drawPointLight(it);                            // sub_1018C7F0, advances `it`
    }
}


// PE 0x100DDF90.
// the Low_Terrain fallback: the low-detail texture, the detail records dropped
// (low quality) or re-read from their file offset, then each patch's reload and the culler's.

void Terrain::reloadForLowQuality()
{
    loadLowDetailTexture();
    TerrainRawReader reader;
    reader.open(m_levelPath, 2, 0, 0);                                    // sub_100763A0(+0x740, 2, 0, 0)
    if (m_lowQuality)
    {
        for (int i = 0; i < m_detailTextureCount; ++i)
        {
            TerrainDetailTexture* const record = m_detailTextures.data()[i];   // unchecked, as shipped
            if (record)
            {
                record->~TerrainDetailTexture();                          // vslot 0 with 1: the deleting destructor
                ::renddx9abi::engineFree(record);
            }
        }
        m_detailTextures.clear();                                         // sub_100E1C20(0, 0)
    }
    else
    {
        reader.seek(0, m_fileOffsetDetail);
        loadDetailTextures(reader);
    }
    for (int j = 0; j < m_patchCount * m_patchCount; ++j)
        m_patches[j]->reloadDetailLayers(reader);
    if (!m_lowQuality)
        m_culler->reloadDetailLayers();
    reader.close();
}


// the +0xBC arm's diffuse passes: the technique again, per visible node's patch the
// colormap and the low-detail map, then the patch's detail passes.

void Terrain::drawFinal()
{
    std::vector<TerrainCullerNode*>* const list = m_culler->visiblePatches();
    if (m_compactVertices)
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::effectSetTechniqueByName(m_effect, std::string(m_lowQuality ? "Low_Terrain" : "Hi_Terrain"));
    dx9rend::effectBegin(m_effect);
    if (m_flag35B)
    {
        // sub_10074300(6, __FILE__, 2557, "") << the message - dice's debug stream, kept as one line.
        ::renddx9abi::logf("[Terrain.cpp:2557] makeDynamicLightmap is obsolete!");
    }
    m_currentPass = 0;
    if (dx9rend::rendererGetHemiRenderMode() == 4)
        dx9rend::stateManagerClear(1u, 0x808080u, 0.0f, 0u);             // sub_100057E0(1, 8421504, ...)
    lightManagerRestoreRendererTarget0();
    TerrainCullerNode** it = list->data();
    while (it != list->data() + list->size())
    {
        QuadPatch* const patch = (*it)->patch;
        param(m_hTexLayer[0])->setValue(patch->colormap, 0);
        param(m_hTexLayer[5])->setValue(patch->lowDetailmap, 0);
        patch->drawFinal(it);                                             // sub_1018C910, advances `it`
    }
    dx9rend::effectEnd(m_effect);
}


// the surrounding ring: Shared_SurroundingTerrain, the full declaration, the corner
// grid for the even slots, the side grid for the odd ones.

void Terrain::drawSurrounding()
{
    dx9rend::effectSetTechniqueByName(m_effect, std::string("Shared_SurroundingTerrain"));
    dx9rend::effectBegin(m_effect);
    dx9rend::vertexDeclarationBind(m_surroundingFullDeclaration);
    dx9rend::vertexBufferBindStream(m_cornerVertexBuffer, 0);
    dx9rend::indexBufferBind(m_cornerIndexBuffer);
    dx9rend::vertexBufferBindStream(m_sideVertexBuffer2, 2);
    for (int s = 0; s < 8; s += 2)
        if (m_surrounding[s])
            m_surrounding[s]->draw(0);
    dx9rend::vertexBufferBindStream(m_sideVertexBuffer, 0);
    dx9rend::indexBufferBind(m_sideIndexBuffer);
    for (int s = 1; s < 8; s += 2)
        if (m_surrounding[s])
            m_surrounding[s]->draw(0);
    dx9rend::effectEnd(m_effect);
}


// (slot +0xD8) - the water: the sea level, then the Ra arm when the distant technique
// exists and its ShaderVersion reads 1, else the grid arm.

void Terrain::drawWater()
{
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float seaLevel = cluster->getSeaWaterLevel();
    if (m_waterDistantTechnique && *m_waterDistantTechnique->shaderVersion() == 1)   // sub_1004AFD0 on +0x718
        drawWaterRa(seaLevel);
    else
        drawWaterGrid(seaLevel);
}

// sub_100DBE30 - which water RaShader a cell draws with: the far one outside the margin around
// the camera, the distant one without dynamic shadows or when no shadow frustum touches the
// cell, else the near one.
dx9rend::Ra::RaShader* Terrain::pickWaterTechnique(const float* box, const float* camera)
{
    const float margin = m_waterTechniqueMargin;
    if (box[0] - margin >= camera[0] || box[3] + margin <= camera[0]
        || box[1] - margin >= camera[1] || box[4] + margin <= camera[1]
        || box[2] - margin >= camera[2] || box[5] + margin <= camera[2])
        return m_waterDistantTechnique2;
    if (!g_LightManager->getDynamicShadowsEnabled())
        return m_waterDistantTechnique;
    const std::vector<LightShadowRecord>& records = terrainShadowRecords();
    if (!terrainShadowRecordCount())
        return m_waterDistantTechnique;
    unsigned int i = 0;
    for (const LightShadowRecord* r = records.data();
         !terrainFrustumIntersectsBox(&r->frustum, box); ++r)
    {
        if (++i >= terrainShadowRecordCount())
            return m_waterDistantTechnique;
    }
    return m_waterTechnique;
}

// sub_100DBF50 - the Ra water: the nodes within the far distance of the camera, one patch-sized
// box around each node's patch centre, the technique, the patch's submit.
void Terrain::drawWaterRa(float seaLevel)
{
    const double size = static_cast<double>(m_patchSize);
    const float w = static_cast<float>(size * m_primaryWorldScale.x);
    const float d = static_cast<float>(size * m_primaryWorldScale.z);
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();        // base +0x50
    float camera[3];
    {
        const float* const m = view->getViewMatrixPtr()->m;
        camera[0] = m[12]; camera[1] = m[13]; camera[2] = m[14];
    }
    view->getFrustumPtr();                                                // answered and dropped, as shipped
    const float* const cameraRow = view->getViewMatrixPtr()->m + 12;
    const float radius = view->getFrustumPtr()->getFarDist();
    m_culler->gatherSphere(cameraRow, radius);
    std::vector<TerrainCullerNode*>* const list = m_culler->gatheredPatches();
    TerrainCullerNode** it = list->data();
    if (it == list->data() + list->size())
        return;
    const float hw = w * 0.5f;
    const float hd = d * 0.5f;
    do
    {
        QuadPatch* const patch = (*it)->patch;
        const float cx = patch->center.x;
        const float cz = patch->center.z;
        TerrainAabb box;
        box.min.x = cx - hw; box.min.y = seaLevel - 0.1f; box.min.z = cz - hd;
        box.max.x = hw + cx; box.max.y = seaLevel + 0.1f; box.max.z = hd + cz;
        box.cornerLength = 0.0f; box.radius = 0.0f; box.dirty = 1;
        box.extent.x = 0.0f; box.extent.y = 0.0f; box.extent.z = 0.0f;
        box.centre.x = 0.0f; box.centre.y = 0.0f; box.centre.z = 0.0f;
        raMaterialShaderSlot(*static_cast<RaMaterialParameters*>(m_materialParameters)) =
            pickWaterTechnique(&box.min.x, camera);                       // material +0x14C
        patch->drawWater(it, list, m_materialParameters, m_objectParameters);   // sub_1018D800, advances `it`
    }
    while (it != list->data() + list->size());
}

// sub_100DCA50 - the grid water: every patch-sized cell of the frustum's xz bounds, tested against
// the frustum, its technique picked, its lightmap taken from the patch under it, then one cell
// draw each. The (int) truncations are the shipped FPU control-word conversions.
void Terrain::drawWaterGrid(float seaLevel)
{
    const double size = static_cast<double>(m_patchSize);
    const float w = static_cast<float>(size * m_primaryWorldScale.x);
    const float d = static_cast<float>(size * m_primaryWorldScale.z);
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();        // base +0x50
    float camera[3];
    {
        const float* const m = view->getViewMatrixPtr()->m;
        camera[0] = m[12]; camera[1] = m[13]; camera[2] = m[14];
    }
    const Frustum* const frustum = view->getFrustumPtr();
    const float* const fb = reinterpret_cast<const float*>(frustum);      // [72..77]: m_boundingBox min/max
    m_waterObjectRingCursor = 0;
    if (fb[76] - fb[73] > 100000.0)
        return;
    if (fb[77] - fb[74] > 100000.0)
        return;
    const float invW = static_cast<float>(1.0 / w);
    const int colStart = static_cast<int>(invW * fb[72]) - 1;
    const int colEnd   = static_cast<int>(invW * fb[75]);
    const float invD = static_cast<float>(1.0 / d);
    const int rowEnd   = static_cast<int>(invD * fb[77]);
    const double rowF  = fb[74] / d;
    const int rowStart = static_cast<int>(rowF) - 1;
    if (rowStart > rowEnd)
        return;
    int rowNext = static_cast<int>(rowF);
    for (int row = rowStart; row <= rowEnd; ++row, ++rowNext)
    {
        if (colStart > colEnd)
            continue;
        const float z0 = static_cast<float>(static_cast<double>(row) * d);
        const float z1 = static_cast<float>(static_cast<double>(rowNext) * d);
        const float yMin = static_cast<float>(seaLevel - 0.1);
        const float yMax = static_cast<float>(seaLevel + 0.1);
        int col = colStart;
        int next;
        do
        {
            const float x0 = static_cast<float>(static_cast<double>(col) * w);
            next = col + 1;
            const float x1 = static_cast<float>(static_cast<double>(next) * w);
            TerrainAabb box;
            box.min.x = x0; box.min.y = yMin; box.min.z = z0;
            box.max.x = x1; box.max.y = yMax; box.max.z = z1;
            box.cornerLength = 0.0f; box.radius = 0.0f; box.dirty = 1;
            box.extent.x = 0.0f; box.extent.y = 0.0f; box.extent.z = 0.0f;
            box.centre.x = 0.0f; box.centre.y = 0.0f; box.centre.z = 0.0f;
            box.refresh();                                                // sub_101C5EB0, twice as shipped
            if (box.dirty)
                box.refresh();
            const bool sphereOk = (kWaterCellTests & 1) == 0 || rendmath::frustumInside(*frustum, box.centre, box.radius);
            if (sphereOk && ((kWaterCellTests & 2) == 0 || terrainFrustumIntersectsBox(frustum, &box.min.x)))
            {
                void*& technique =
                    raMaterialShaderSlot(*static_cast<RaMaterialParameters*>(m_materialParameters));   // +0x14C
                technique = pickWaterTechnique(&box.min.x, camera);
                const int c = m_patchCount / 2 + static_cast<int>(invW * x0);
                const int r = m_patchCount / 2 + static_cast<int>(invD * z0);
                void* lightmap;
                if (c < 0 || c >= m_patchCount || r < 0 || r >= m_patchCount)
                {
                    technique = m_waterSurroundingTechnique;
                    lightmap = 0;
                }
                else
                {
                    QuadPatch* const patch = m_patches[c + r * m_patchCount];
                    if (seaLevel + m_waterCullHeightOffset < patch->minHeight)
                    {
                        col = next;
                        continue;
                    }
                    lightmap = patch->lightmap;
                }
                drawWaterCell(x0, z0, x1, z1, lightmap);
            }
            col = next;
        }
        while (next <= colEnd);
    }
}

// sub_100DC400 - one water cell: the 65x65 grid stream object and the 256-entry object-parameter
// ring built on first use, then a render queue entry into RSMTWater with the cell's world matrix,
// the view-projection, the shadow matrices, the lightmap, the water technique and the sky light.
void Terrain::drawWaterCell(float x0, float z0, float x1, float z1, void* lightmap)
{
    namespace Ra = dx9rend::Ra;
    // [CHOSEN, not recovered] Skip excess cells instead of overrunning the ring
    // (issue #33), following marekzajac97/BF2-HUD-fix. Those cells remain undrawn.
    if (m_waterObjectRingCursor >= 256)
        return;
    if (!m_waterGridGeometry)
    {
        // sub_1005F340(140292): a count word, then 256 RaObjectParameters (retail 548 bytes each).
        unsigned char* const ring = static_cast<unsigned char*>(::renddx9abi::engineAlloc(
            static_cast<unsigned int>(kWaterObjectRingHeader + 256 * sizeof(Ra::RaObjectParameters))));
        unsigned char* objects = 0;
        if (ring)
        {
            *reinterpret_cast<int*>(ring) = 256;
            objects = ring + kWaterObjectRingHeader;
            for (int i = 0; i < 256; ++i)
                Ra::raObjectParametersConstruct(objects + sizeof(Ra::RaObjectParameters) * i);
        }
        m_waterObjectRing = objects;
        if (!m_waterGridGeometry)
        {
            std::vector<const Ra::RaVertexElementDesc*> elements;
            elements.push_back(&Ra::g_desc_1023EF04);                     // Position
            elements.push_back(&Ra::g_desc_1023ED4C);                     // TBase2D
            elements.push_back(&Ra::g_desc_1023EF2C);                     // TLightMap2D
            void* const storage = ::renddx9abi::engineAlloc(sizeof(Ra::RaStreamDataEditable));   // retail 0x1A4
            Ra::RaStreamDataEditable* const grid = storage
                ? new (storage) Ra::RaStreamDataEditable(4225u, 24576u, 28u, elements, Ra::raRendererDevice())
                : 0;
            m_waterGridGeometryTemp = grid;
            grid->lock();                                                 // vslot +0x24, unguarded as shipped
            // 65 x 65 vertices of (x, 0, z, u, v, u2, v2) over the unit square, in 28-byte records.
            static float vertices[4225 * 7];
            for (int i = 0; i < 4225 * 7; ++i)
                vertices[i] = 0.0f;
            {
                float* v = vertices;
                for (int row = 0; row < 65; ++row)
                {
                    const float zf = static_cast<float>(static_cast<double>(row) * 0.015625);
                    for (int col = 0; col < 65; ++col, v += 7)
                    {
                        const float xf = static_cast<float>(static_cast<double>(col) * 0.015625);
                        v[1] = 0.0f;
                        v[0] = xf;
                        v[2] = zf;
                        v[3] = xf;
                        v[4] = zf;
                        v[5] = xf;
                        v[6] = zf;
                    }
                }
            }
            static unsigned short indices[24576];
            {
                unsigned short* p = indices;
                for (int row = 0; row < 64; ++row)
                {
                    const short base = static_cast<short>(65 * row);
                    for (int col = 0; col < 64; ++col)
                    {
                        *p++ = static_cast<unsigned short>(base + col);
                        *p++ = static_cast<unsigned short>(base + col + 1);
                        *p++ = static_cast<unsigned short>(base + col + 66);
                        *p++ = static_cast<unsigned short>(base + col);
                        *p++ = static_cast<unsigned short>(base + col + 66);
                        *p++ = static_cast<unsigned short>(base + col + 65);
                    }
                }
            }
            m_waterGridGeometryTemp->append(vertices, 28, 4225, indices, 24576);   // vslot +0x2C
            m_waterGridGeometryTemp->unlock();                            // vslot +0x28
            m_waterGridGeometry = m_waterGridGeometryTemp;
            m_waterGridGeometryTemp = 0;
            // The element vector's storage is freed as it leaves scope.
        }
    }

    dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
    entry->materialParameters = m_materialParameters;
    entry->material = m_waterGridGeometry;
    entry->materialParameters = m_materialParameters;                     // stored twice, as shipped
    Ra::RaObjectParameters& op = *reinterpret_cast<Ra::RaObjectParameters*>(
        m_waterObjectRing + sizeof(Ra::RaObjectParameters) * m_waterObjectRingCursor);
    m_waterObjectRingCursor = m_waterObjectRingCursor + 1;

    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float seaLevel = cluster->getSeaWaterLevel();
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();    // base +0x50
        ::memcpy(op.viewProjection.value, view->getViewProjectionMatrixPtr()->m, 64);   // +0x84
    }
    float* const world = op.world.value;
    world[0] = 1.0f; world[1] = 0.0f; world[2] = 0.0f; world[3] = 0.0f;
    world[4] = 0.0f; world[5] = 1.0f; world[6] = 0.0f; world[7] = 0.0f;
    world[10] = 1.0f; world[8] = 0.0f; world[9] = 0.0f; world[11] = 0.0f;
    world[15] = 1.0f; world[12] = 0.0f; world[13] = 0.0f; world[14] = 0.0f;
    world[0]  = x1 - x0;
    world[10] = z1 - z0;
    world[12] = x0;
    world[13] = seaLevel;
    world[14] = z0;
    op.tail220 = 0;
    if (g_LightManager->getDynamicShadowsEnabled())
    {
        const std::vector<LightShadowRecord>& records = terrainShadowRecords();
        if (!records.empty())
        {
            ::memcpy(op.shadowProjMat.value, &records[0].matrices[4], 64);   // +0x2F8
            ::memcpy(op.shadowTrapMat.value, &records[0].matrices[7], 64);   // +0x3B8
        }
    }
    entry->objectParameters = &op;
    entry->lighting.lightMapOffset.value[0] = 1.0f;
    entry->lighting.lightMapOffset.value[1] = 1.0f;
    entry->lighting.lightMapOffset.value[2] = 0.0f;
    entry->lighting.lightMapOffset.value[3] = 0.0f;
    {
        IUnknown*& slot = *reinterpret_cast<IUnknown**>(&entry->lighting.lightMap.value);
        if (lightmap)
        {
            IUnknown* const base = static_cast<dx9rend::EngineTexture*>(lightmap)->baseTexture;
            if (slot != base)
            {
                if (slot)
                    slot->Release();
                slot = base;
                if (base)
                    base->AddRef();
            }
        }
        else if (slot)
        {
            slot->Release();
            slot = 0;
        }
    }
    entry->shader = static_cast<Ra::RaShader*>(
        raMaterialShaderSlot(*static_cast<RaMaterialParameters*>(m_materialParameters)));   // +0x14C
    if (world::LightSource* const sky = g_LightManager->getSkyLight())    // +0x28
    {
        float* const record = static_cast<float*>(entry->lightArray.lights.value);
        const float* const dir = terrainSkyLightDirection(sky);           // +0x17C
        record[4] = dir[1];
        record[5] = dir[2];
        record[3] = dir[0];
        const float* const colour = terrainSkyLightColor(sky);            // +0x198
        record[6] = colour[0];
        record[7] = colour[1];
        record[8] = colour[2];
    }
    dx9rend::renderQueueAddToSlot(9u, entry);                             // raShaderServer->m_specialManagers[RSMTWater]->+0x04
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
