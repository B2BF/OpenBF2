// Terrain visibility, shader constants, diffuse passes and water rendering.
// The grid-water path is selected when ShaderVersion is zero, as in stock shaders.
// Remaining boundaries include detail lighting, dynamic-shadow range gathering (RW-261),
// Ra water submission and low-quality reload. Profiling is omitted.
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "Terrain.h"
#include "QuadPatch.h"
#include "XpLightManager.h"
#include "StaticMesh.h"
#include "../Game/Objects/Visual/LightSource.h"   // the mask-light pass's lights
#include "../Math/Frustum.h"
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

#include <cmath>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_rendererBase;       // RendDX9Init.cpp   - dword_1023B970 (the Renderer object)
extern void* g_shaderManager;      // ShaderManager.cpp - dword_1023B97C
extern void* g_raShaderConstants;  // RenderQueues.cpp  - dword_1023DDBC (the Ra constants block)

bool  effectHasTechnique(void* effect, const vc71::string& name);          // Effect.cpp,  sub_1001DC20
void  updateShaderDefines(void* shaderManager, void* base);                // ShaderManager.cpp, 0x10023730
void  shaderManagerReloadEffects(void* shaderManager);
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

namespace {

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

inline unsigned char* constants()
{
    return static_cast<unsigned char*>(dx9rend::g_raShaderConstants);
}
inline float& constantF(unsigned int byteOffset)
{
    return *reinterpret_cast<float*>(constants() + byteOffset);
}
inline void*& constantP(unsigned int byteOffset)
{
    return *reinterpret_cast<void**>(constants() + byteOffset);
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
    unsigned char* const base = static_cast<unsigned char*>(dx9rend::g_rendererBase);
    if (base[0x514])
        return true;
    if (base[0x515])
        return false;
    unsigned int* const first = *reinterpret_cast<unsigned int**>(base + 0xC0);
    unsigned int* const last  = *reinterpret_cast<unsigned int**>(base + 0xC4);
    return first && ((last - first) >> 1) && *reinterpret_cast<int*>(first) > 0;
}

inline unsigned char* lightManagerRaw()
{
    return reinterpret_cast<unsigned char*>(g_LightManager);
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
    // Renderer vslot +0x6C (`return this + 264`), then the counter.
    return *reinterpret_cast<unsigned int*>(
        static_cast<unsigned char*>(dx9rend::g_rendererBase) + 264 + byteOffset);
}

vc71::vector_layout& terrainShadowRecords()
{
    return *reinterpret_cast<vc71::vector_layout*>(lightManagerRaw() + 0x79C);
}

unsigned int terrainShadowRecordCount()
{
    const vc71::vector_layout& v = terrainShadowRecords();
    return v.first ? static_cast<unsigned int>((v.last - v.first) / 1176) : 0u;
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
        // GlobalTime is read before the vslot +0x90 call (0x100DBB52);
        // WaterCycleTime = frac(GlobalTime / waterAnimSpeed) (`fdivr`, `fld st0`, `fistp`, `fild`,
        // `fsubr`).
        const float globalTime = constantF(148);                                      // GlobalTime
        const float cycle = globalTime / getWaterAnimSpeed();                         // vslot +0x90
        constantF(412) = cycle - static_cast<float>(static_cast<int>(cycle));        // WaterCycleTime
    }
    else
    {
        // Select and blend two water frames from time/(animationSpeed * 0.2).
        // setupWater must supply a nonempty frame list; sampling has no null/empty guards.
        const float globalTime = constantF(148);                                      // GlobalTime
        const float cycleLength = getWaterAnimSpeed() * kWater2DCycleScale;           // vslot +0x90, fmul
        const float cycle = globalTime / cycleLength;                                 // fdivr
        m_waterTextureList->phase = cycle - static_cast<float>(static_cast<int>(cycle));
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
            const vc71::vector_layout& records = terrainShadowRecords();
            if (records.first)
            {
                const unsigned int count = static_cast<unsigned int>((records.last - records.first) / 1176);
                if (static_cast<float>(count) != 0.0f)
                {
                    ::memcpy(op.shadowProjMat.value, records.first + 760, 64);
                    ::memcpy(op.shadowTrapMat.value, records.first + 952, 64);
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
    param(m_hDynamicPointColor)->setValue(constants() + 588, 0);
    param(m_hNightTimeMode)->setValue(constants() + 604, 0);
    param(m_hNightOccludedSkyIntensity)->setValue(constants() + 612, 0);
    param(m_hFogRange)->setValue(constants() + 168, 0);
    param(m_hFogColor)->setValue(constants() + 188, 0);
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
        halfTexel[0] = 0.5f / static_cast<float>(static_cast<unsigned short>(accumulation->width));
        halfTexel[1] = 0.5f / static_cast<float>(static_cast<unsigned short>(accumulation->height));
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
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("COLORLIGHTTEX"))->setValue(colorLightTex, 0);
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
                dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("DETAILTEX"))->setValue(detailTex, 0);
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
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("STCOLORLIGHTTEX"))->setValue(value, 0);
        }
        {
            const double w = static_cast<double>(
                static_cast<dx9rend::EngineTexture*>(m_surrounding[s]->lowDetailmap)->width);
            float value[2] = { 0.0f, 0.0f };
            value[0] = static_cast<float>((w - 1.0) / w);
            value[1] = static_cast<float>(1.0 / (w + w));
            dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("STLOWDETAILTEX"))->setValue(value, 0);
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


// the main draw. The shadow-range gather, the accumulation target, the technique,
// pass 7 for the gathered ranges, pass 0 per visible node, the light pass, the target restore.

void Terrain::drawMain()
{
    vc71::vector_layout* const list = m_culler->visiblePatches();
    TerrainCullerNode** it = 0;
    if (m_shadowPatches.first)                                            // sub_100E1C20(0, 0)
        m_shadowPatches.last = m_shadowPatches.first;
    QuadPatch* previous = 0;
    if (m_shadowRanges.first)                                             // sub_100E2430(0, ...)
        m_shadowRanges.last = m_shadowRanges.first;

    const vc71::vector_layout& records = terrainShadowRecords();
    if (records.first
        && static_cast<unsigned int>((records.last - records.first) / 1176) != 0u
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
        it = reinterpret_cast<TerrainCullerNode**>(list->first);
        TerrainCullerNode** const end = reinterpret_cast<TerrainCullerNode**>(list->last);
        while (it != end)
        {
            QuadPatch* const patch = (*it)->patch;
            if (previous != patch)
            {
                if (m_indexBufferCount < static_cast<unsigned int>(cursor + 12))
                    break;
                previous = patch;
                if (patch->gatherShadowRanges(&m_indexBufferBits, &cursor))
                    vc71::vectorPushBackIn(m_shadowPatches, 4u, &patch);
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
        if (dx9rend::effectHasTechnique(m_effect, vc71::string("Hi_Terrain")))
            technique = "Hi_Terrain";
        else
        {
            // ShaderManager_updateShaderDefines(dword_1023B97C, this) as shipped - the callee
            // re-reads the renderer base from its global, which is what the tree's signature takes.
            dx9rend::updateShaderDefines(dx9rend::g_shaderManager, dx9rend::g_rendererBase);
            dx9rend::shaderManagerReloadEffects(dx9rend::g_shaderManager);
            if (dx9rend::effectHasTechnique(m_effect, vc71::string("Hi_Terrain")))
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
    dx9rend::effectSetTechniqueByName(m_effect, vc71::string(technique));
    dx9rend::effectBegin(m_effect);

    if (g_LightManager->getDynamicShadowsEnabled())                       // +0xF0
    {
        if (m_shadowRanges.first)
        {
            const unsigned int count = static_cast<unsigned int>((m_shadowRanges.last - m_shadowRanges.first) / 24);
            if (count)
            {
                dx9rend::effectBeginPass(m_effect, 7u);
                dx9rend::indexBufferBind(m_indexBuffer);
                param(m_hShadowMap)->setValue(*reinterpret_cast<void**>(lightManagerRaw() + 0x21C), 0);
                for (QuadPatch** p = reinterpret_cast<QuadPatch**>(m_shadowPatches.first);
                     p != reinterpret_cast<QuadPatch**>(m_shadowPatches.last); ++p)
                    (*p)->drawShadow();
            }
        }
    }
    dx9rend::indexBufferBind(m_lodIndexBuffer);
    dx9rend::effectBeginPass(m_effect, 0u);
    it = reinterpret_cast<TerrainCullerNode**>(list->first);
    while (it != reinterpret_cast<TerrainCullerNode**>(list->last))
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
        IDirect3DSurface9* const backBuffer = *reinterpret_cast<IDirect3DSurface9**>(
            static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x76C);
        dx9rend::Ra::raRendererDevice()->StretchRect(backBuffer, 0, surface, 0, D3DTEXF_NONE);   // device +0x88
        surface->Release();
        dx9rend::stateManagerClear(1u, 0u, 0.0f, 0u);
    }
    else
        lightManagerRestoreRendererTarget0();
    dx9rend::effectEnd(m_effect);
}


// the ShaderVersion-1 detail/light pass. read in full: pass 16, multipleshadowmap from
// LightManager +0x230, the dynamic index buffer relocked and refilled per patch from the static
// light list (unk_10259B20, constructed at static-init time by sub_100EF100 and filled by
// LightManager / StaticMesh writers this tree does not model), the clip-plane and point/spot
// light draws (sub_10188D80, sub_1018AC00 unread). Reachable only when Shaders/TerrainShader
// defines an int "ShaderVersion" of 1, which the shipped shader does not; it stops when reached.

void Terrain::drawDetailPass()
{
    RD9_FRONTIER("dice::hfe::geom::Terrain::drawDetailPass (sub_100DF990 - read; needs the static "
                 "light list unk_10259B20 and sub_1018AC00 / sub_10188D80, unwritten)", 0xB8);
}


// Draws visible dynamic point lights on gathered terrain patches.
// Attenuation uses dynamicLightRangeScale, while the gather radius remains unscaled.

void Terrain::drawMaskLights()
{
    static vc71::vector_layout s_lights = { 0, 0, 0, 0 };                // unk_102592A4 (the once-only seed and atexit)
    if (s_lights.first)                                                   // sub_100E1C20(0, 0)
        s_lights.last = s_lights.first;
    g_LightManager->getVisibleLightsByMask(s_lights, 2313u, 0u);          // +0x18
    for (char* p = s_lights.first; p != s_lights.last; p += 4)            // the end re-read every pass
    {
        world::LightSource* const light = *reinterpret_cast<world::LightSource**>(p);
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
        vc71::vector_layout* const list = m_culler->gatheredPatches();
        TerrainCullerNode** it = reinterpret_cast<TerrainCullerNode**>(list->first);
        while (it != reinterpret_cast<TerrainCullerNode**>(list->last))
            (*it)->patch->drawPointLight(it);                            // sub_1018C7F0, advances `it`
    }
}


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
            TerrainDetailTexture* const record =
                reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[i];
            if (record)
            {
                record->~TerrainDetailTexture();                          // vslot 0 with 1: the deleting destructor
                ::renddx9abi::engineFree(record);
            }
        }
        if (m_detailTextures.first)                                       // sub_100E1C20(0, 0)
            m_detailTextures.last = m_detailTextures.first;
    }
    else
    {
        reader.seek(0, m_fileOffsetDetail);
        loadDetailTextures(reader);
    }
    for (int j = 0; j < m_patchCount * m_patchCount; ++j)
        RD9_FRONTIER("dice::hfe::geom::QuadPatch - sub_1018A4F0 (the per-patch detail reload "
                     "Terrain::reloadForLowQuality runs; unread, 185 B)", 0x18);
    if (!m_lowQuality)
        RD9_FRONTIER_AT("dice::hfe::geom::TerrainCuller - sub_100E3D60 (the culler's reload; unread, "
                        "642 B)", m_culler, 0x18);
    reader.close();
}


// the +0xBC arm's diffuse passes: the technique again, per visible node's patch the
// colormap and the low-detail map, then the patch's detail passes.

void Terrain::drawFinal()
{
    vc71::vector_layout* const list = m_culler->visiblePatches();
    if (m_compactVertices)
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::effectSetTechniqueByName(m_effect, vc71::string(m_lowQuality ? "Low_Terrain" : "Hi_Terrain"));
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
    TerrainCullerNode** it = reinterpret_cast<TerrainCullerNode**>(list->first);
    while (it != reinterpret_cast<TerrainCullerNode**>(list->last))
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
    dx9rend::effectSetTechniqueByName(m_effect, vc71::string("Shared_SurroundingTerrain"));
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
    const vc71::vector_layout& records = terrainShadowRecords();
    if (!terrainShadowRecordCount())
        return m_waterDistantTechnique;
    unsigned int i = 0;
    for (const char* r = records.first;
         !terrainFrustumIntersectsBox(reinterpret_cast<const Frustum*>(r + 16), box); r += 1176)
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
    vc71::vector_layout* const list = m_culler->gatheredPatches();
    TerrainCullerNode** it = reinterpret_cast<TerrainCullerNode**>(list->first);
    if (it == reinterpret_cast<TerrainCullerNode**>(list->last))
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
        *reinterpret_cast<dx9rend::Ra::RaShader**>(static_cast<unsigned char*>(m_materialParameters) + 332) =
            pickWaterTechnique(&box.min.x, camera);
        patch->drawWater(it, list, m_materialParameters, m_objectParameters);   // sub_1018D800, advances `it`
    }
    while (it != reinterpret_cast<TerrainCullerNode**>(list->last));
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
            const bool sphereOk = (kWaterCellTests & 1) == 0 || frustum->inside(box.centre, box.radius);
            if (sphereOk && ((kWaterCellTests & 2) == 0 || terrainFrustumIntersectsBox(frustum, &box.min.x)))
            {
                dx9rend::Ra::RaShader*& technique = *reinterpret_cast<dx9rend::Ra::RaShader**>(
                    static_cast<unsigned char*>(m_materialParameters) + 332);
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
    if (!m_waterGridGeometry)
    {
        // sub_1005F340(140292): a count word, then 256 RaObjectParameters.
        unsigned char* const ring = static_cast<unsigned char*>(::renddx9abi::engineAlloc(140292u));
        unsigned char* objects = 0;
        if (ring)
        {
            *reinterpret_cast<int*>(ring) = 256;
            objects = ring + 4;
            for (int i = 0; i < 256; ++i)
                Ra::raObjectParametersConstruct(objects + 548 * i);
        }
        m_waterObjectRing = objects;
        if (!m_waterGridGeometry)
        {
            vc71::vector_layout elements = { 0, 0, 0, 0 };
            const Ra::RaVertexElementDesc* desc;
            desc = &Ra::g_desc_1023EF04; vc71::vectorPushBackIn(elements, 4u, &desc);   // Position
            desc = &Ra::g_desc_1023ED4C; vc71::vectorPushBackIn(elements, 4u, &desc);   // TBase2D
            desc = &Ra::g_desc_1023EF2C; vc71::vectorPushBackIn(elements, 4u, &desc);   // TLightMap2D
            void* const storage = ::renddx9abi::engineAlloc(0x1A4);
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
            if (elements.first)
                ::renddx9abi::engineFree(elements.first);
        }
    }

    dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
    entry->materialParameters = m_materialParameters;
    entry->material = m_waterGridGeometry;
    entry->materialParameters = m_materialParameters;                     // stored twice, as shipped
    Ra::RaObjectParameters& op = *reinterpret_cast<Ra::RaObjectParameters*>(
        m_waterObjectRing + 548 * m_waterObjectRingCursor);
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
        const vc71::vector_layout& records = terrainShadowRecords();
        if (records.first && static_cast<unsigned int>((records.last - records.first) / 1176) != 0u)
        {
            ::memcpy(op.shadowProjMat.value, records.first + 760, 64);
            ::memcpy(op.shadowTrapMat.value, records.first + 952, 64);
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
    entry->shader = *reinterpret_cast<Ra::RaShader**>(static_cast<unsigned char*>(m_materialParameters) + 332);
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
