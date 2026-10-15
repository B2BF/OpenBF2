// Collects changed decal objects into texture/shadow batches, then draws and clears them.
// [INFERRED] source placement; [CHOSEN, not recovered] filename.
// Batches hold at most ten objects and 12288 vertices. Draws preserve kind/texture/shadow ordering.
// Geometry and lighting fields use narrow access seams until their owners expose typed accessors.
// Fade computations depend on the renderer's 24-bit floating-point precision.

#ifdef BF2_IMAGE_RENDDX9

#include "DecalRenderer.h"
#include "DecalManager.h"
#include "DecalObject.h"

#include "../XpLightManager.h"                        // LightManager (getSkyLight, getAmbientColor)
#include "../../Game/Objects/IObject.h"               // getVisualAbsoluteTransformation, PE +0x8C
#include "../../Game/Objects/Visual/LightSource.h"    // getColor +0x198 / getDirection +0x17C
#include "../../Math/Mat4.h"
#include "../../RendDX9/RendDX9Base.h"                // effect / state-manager seams
#include "../../RendDX9/ShaderParameter.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendMisc/IRenderer.h"
#include <string>

#include <algorithm>
#include <stddef.h>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
// Declared per TU, as FontSystem.cpp / NameTagRenderer.cpp / MeshTemplate.cpp do.
extern void* g_shaderManager;                                              // dword_1023B97C
extern void* g_bufferManager;                                              // dword_1023B978
extern void* g_rendererBase;                                               // dword_1023B970
extern void* g_raShaderConstants;                                          // dword_1023DDBC
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
int   bufferManagerSwapPool(int value);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexBufferRelease(void* wrapper);                                  // --ref, sub_10014510, free
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);
} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                       // dword_1024B914
// StaticMesh.cpp (published in StaticMesh.h) - sub_1008DD80.
const float* staticMeshPackVec4(const float v[3], float w);

namespace {

// dword_1023AD9C / dword_1023AD98 - two initialised file-scope floats, both 0xB8D1B717 = -0.0001f,
// handed to D3DRS_DEPTHBIAS (0xC3) and D3DRS_SLOPESCALEDEPTHBIAS (0xAF) as their bit patterns
// (0x101AA6F8 / 0x101AA70F). Nothing else references either.
const unsigned int kDecalDepthBiasBits           = 0xB8D1B717u;
const unsigned int kDecalSlopeScaleDepthBiasBits = 0xB8D1B717u;

const unsigned int kVertexCapacity = 0x3000;          // 12288 - the buffer and the batch split
const unsigned int kVertexStride   = 0x2C;            // 44
const unsigned int kLockDiscard    = 0x2000;          // D3DLOCK_DISCARD
const unsigned int kLockNoOverwrite= 0x1000;          // D3DLOCK_NOOVERWRITE
const unsigned int kTriangleList   = 4;               // D3DPT_TRIANGLELIST

// The 20-byte clipped vertex DecalObject::add leaves at DecalRecord +0x50 is DecalObject.h's
// DecalVertex (read at xyz as three floats, u and v as two dwords).

// The 44-byte vertex drawBatch writes (0x101A9F9E..0x101A9FF0). [INFERRED] member names; the
// declaration the effect binds is inside the .fx.
struct DecalGpuVertex
{
    float        x, y, z;                              // +0x00  the clipped position
    float        nx, ny, nz;                           // +0x0C  DecalRecord::normal
    unsigned int diffuse;                              // +0x18  DecalRecord::diffuse
    float        u, v;                                 // +0x1C
    float        instance;                             // +0x24  (float)(short)instanceIndex
    float        alpha;                                // +0x28
};

static_assert(sizeof(DecalVertex) == 0x14, "0x101A9FC0 strides the source by 0x14");
static_assert(sizeof(DecalGpuVertex) == 0x2C, "0x101A9FBD strides the buffer by 0x2C");

DecalManager* decalManager()
{
    return static_cast<DecalManager*>(g_DecalManager);   // re-read at every use, as shipped
}

dx9rend::IRenderer* renderer()
{
    return static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase);
}

// The Ra constants block (g_raShaderConstants) word at a retail x86 byte offset.
unsigned char* raConstantAt(unsigned int byteOffset)
{
    return dx9rend::raShaderConstantsAt(byteOffset);
}

// sub_1000ED40 (34 B, __stdcall, `retn 0Ch`): fld low; fld high; fld value; fcomi st,st1; fcmovnb;
// fcomi st,st2; fcmovb. The high bound first; an unordered compare sets CF, so NaN answers `low`.
// (The same reading UndergrowthSystemLoad.cpp's clampRange and OverGrowthRender.cpp carry.)
float clampRange(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}


// RendDX9's OWN objects, reached by retail offset. Identical in both flavours; kept narrow and named
// so the owners (the geometry classes, XpLightManager.cpp) can replace them with typed accessors.
// [CHOSEN, not recovered] seams.


// The geometry instance's primary slot +0x28 - the XpLightManager render record assigned to it
// (StaticMesh 0x10103BA0 `mov eax,[ecx+34Ch]`, SkinnedMesh 0x101A8A20 `+0x33C`; the "matrix triple"
// the SkinnedMesh draw chases is this record's +0x2F8 / +0x338 / +0x3B8). No typed IGeometry
// interface exists in the tree yet.
void* geometryLightRecord(void* geometry)
{
    typedef void* (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>((*static_cast<void***>(geometry))[0x28 / 4])(geometry);
}

// The geometry instance's primary slot +0xB0 - the renderer frame ordinal the instance was last
// drawn in (StaticMesh.h: "+0xAC/+0xB0 ... the current frame's").
unsigned int geometryDrawnFrame(void* geometry)
{
    typedef unsigned int (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>((*static_cast<void***>(geometry))[0xB0 / 4])(geometry);
}

// XpLightManager.h's LightShadowRecord (0x498 bytes in the PE): matrices[4] at +0x2F8, matrices[7]
// at +0x3B8, atlasBounds at +0x438, and the first tail byte +0x494.
const Mat4* lightRecordShadowMatrix(void* record)
{
    return &static_cast<LightShadowRecord*>(record)->matrices[4];
}

const Mat4* lightRecordTrapezoidMatrix(void* record)
{
    return &static_cast<LightShadowRecord*>(record)->matrices[7];
}

const float* lightRecordViewPort(void* record)
{
    return static_cast<LightShadowRecord*>(record)->atlasBounds;
}

bool lightRecordUsesSecondShadowMap(void* record)
{
    return static_cast<LightShadowRecord*>(record)->tail494[0] != 0;
}

// The LightManager's two shadow-map textures (+0x21C the directional map, +0x230 the point/spot
// atlas) are read through XpLightManager.h's lightManagerShadowMapTexture /
// lightManagerPointShadowMapTexture.

// the sort predicate _Sort receives in edx (0x101AA75C):
//     key = (((texture & 0xFFFFC000) | (kind << 26)) << 4) | (shadowMap >> 14)
// compared unsigned (`cmp; sbb; neg`). The texture's top four bits are shifted out.
unsigned int batchSortKey(const DecalBatch& batch)
{
    const unsigned int texture = static_cast<unsigned int>(reinterpret_cast<size_t>(batch.texture));
    const unsigned int shadow  = static_cast<unsigned int>(reinterpret_cast<size_t>(batch.shadowMap));
    return (((texture & 0xFFFFC000u) | (static_cast<unsigned int>(batch.kind) << 26)) << 4) |
           (shadow >> 14);
}

bool batchLess(const DecalBatch& a, const DecalBatch& b)
{
    return batchSortKey(a) < batchSortKey(b);
}

} // anonymous namespace


// Acquires the decal effect, dynamic default-pool vertex buffer, and parameter handles.
// beginPass initializes the cursor; per-batch setup initializes pointer arrays before use.

DecalRenderer::DecalRenderer()
    : m_batches()
    , m_byte14(0)
    , m_boundTexture(0)
    , m_boundShadowMap(0)
    , m_enabled(true)
{
    m_viewColumn.x = 0.0f;
    m_viewColumn.y = 0.0f;
    m_viewColumn.z = 0.0f;
    m_viewColumn.w = 0.0f;

    for (int i = 0; i < 16; ++i)
        m_worldViewProjection.m[i] = 0.0f;
    m_worldViewProjection.m[15] = 1.0f;                     // +0x6C
    m_worldViewProjection.m[10] = 1.0f;                     // +0x58
    m_worldViewProjection.m[5]  = 1.0f;                     // +0x44
    m_worldViewProjection.m[0]  = 1.0f;                     // +0x30

    m_fadeDistanceAndInterval.x = 0.0f;                     // +0x70
    m_fadeDistanceAndInterval.y = 0.0f;                     // +0x74
    m_fadeDistanceAndInterval.z = 0.0f;                     // +0x78
    m_fadeDistanceAndInterval.w = 200.0f;                   // +0x7C

    for (int i = 0; i < 10; ++i)
    {
        m_shadowViewPorts[i].x = 0.0f;
        m_shadowViewPorts[i].y = 0.0f;
        m_shadowViewPorts[i].z = 0.0f;
        m_shadowViewPorts[i].w = 0.0f;
    }

    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager,
                                        "mods/xpack/shaders/Decals"));

    const int previousPool = dx9rend::bufferManagerSwapPool(0);
    m_vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, kVertexCapacity,
                                                 kVertexStride, std::string("DecalRenderer"),
                                                 0x208u);
    dx9rend::bufferManagerSwapPool(previousPool);

    // sub_1001E420 each - the counted lookup; it guards neither the effect nor the answer.
    m_paramFadeDistanceAndInterval = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("DecalFadeDistanceAndInterval"));          // +0x1B8
    m_paramInstanceTransformations = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("InstanceTransformations"));               // +0x1BC
    m_paramShadowTransformations = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("ShadowTransformations"));                 // +0x1C0
    m_paramShadowTrapezoidMats = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("ShadowTrapezoidMats"));                   // +0x1C4
    m_paramShadowViewPortMaps = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("ShadowViewPortMaps"));                    // +0x1C8
    m_paramAmbientColor = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("AmbientColor"));                          // +0x1CC
    m_paramSunColor = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("SunColor"));                              // +0x1D0
    m_paramSunDirection = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("SunDirection"));                          // +0x1D4
    m_paramWorldViewProjection = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("WorldViewProjection"));                   // +0x1D8
    m_paramTexLayer0 = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("TEXLAYER0"));                             // +0x1DC
    m_paramShadowMapTex = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("ShadowMapTex"));                          // +0x1E0
    m_paramFogRange = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("fogRange"));                              // +0x1E4
    m_paramFogColor = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("fogColor"));                              // +0x1E8
    m_paramWaterFogColor = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("WaterFogColor"));                         // +0x1EC
    m_paramWaterFogRange = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("WaterFogRange"));                         // +0x1F0
    m_paramCameraToWater = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("CameraToWater"));                         // +0x1F4
    m_paramWorldSpaceCamPos = dx9rend::effectGetShaderParameterTyped(
        m_effect, std::string("WorldSpaceCamPos"));                      // +0x1F8
}


// the destructor. Sixteen parameters released through their +0x08 in field
// order +0x1B8..+0x1F4, then the effect (+0x08), then the vertex buffer (the inline `--ref;
// free`), and only then the seventeenth parameter (+0x1F8) - out of field order, as
// shipped. Every release is null-tested. The batch vector's destructor runs last (0x101AA62B..).

DecalRenderer::~DecalRenderer()
{
    dx9rend::ShaderParameter* const first16[16] =
    {
        m_paramFadeDistanceAndInterval, m_paramInstanceTransformations,
        m_paramShadowTransformations, m_paramShadowTrapezoidMats, m_paramShadowViewPortMaps,
        m_paramAmbientColor, m_paramSunColor, m_paramSunDirection, m_paramWorldViewProjection,
        m_paramTexLayer0, m_paramShadowMapTex, m_paramFogRange, m_paramFogColor,
        m_paramWaterFogColor, m_paramWaterFogRange, m_paramCameraToWater
    };
    for (int i = 0; i < 16; ++i)
    {
        if (first16[i])
            first16[i]->release();                                         // vtable +0x08
    }
    if (m_effect)
        dx9rend::effectRelease(m_effect);                                  // vtable +0x08
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_paramWorldSpaceCamPos)
        m_paramWorldSpaceCamPos->release();
}


// the batch kind of an object: 2 when its slot +0x08 answers a static
// shadow light record; 1 when it has a geometry (+0x20) whose +0xB0 equals the renderer's frame
// ordinal (+0x54) - geometry asked first; else 0.

int DecalRenderer::classify(DecalObject* object)
{
    if (object->getShadowRecord())                        // slot +0x08
        return 2;
    void* const geometry = object->ownerGeometry();         // +0x20
    if (geometry)
    {
        const unsigned int drawnFrame = geometryDrawnFrame(geometry);      // geometry +0xB0
        if (drawnFrame == renderer()->getFrameOrdinal())                   // renderer +0x54
            return 1;
    }
    return 0;
}


// see the file header. Details kept from the stream:
//   * the early out is `!enabled || instances.size() == 0`, before any local is built;
//   * the "changed" byte is read AND cleared for every active object, drawable or not;
//   * the view's inverse matrix (+0x68 - on the caller's copy) gives +0x20..+0x2C its third column,
//     which nothing in the image reads back;
//   * a batch is flushed at exactly 10 objects (`== 0Ah`) with its kind kept, and at the end of
//     each bucket when it holds any object; boundTexture / boundShadowMap restart at 0 per bucket.

void DecalRenderer::collect(dx9rend::IRenderView& view, DecalObjectMap& instances)
{
    if (!m_enabled || instances.size() == 0)
        return;

    std::vector<DecalObject*> buckets[3];                                  // [ebp-6Ch..-3Ch]
    for (DecalObjectMap::iterator it = instances.begin(); it != instances.end(); ++it)
    {
        DecalObject* const object = it->second;
        const unsigned char changed = object->renderFlag();
        object->renderFlag() = false;
        if (changed && object->getNumDecals() != 0)      // slot +0x20
            buckets[classify(object)].push_back(object);
    }

    const Mat4* const inverseView = view.getInverseViewMatrixPtr();        // +0x68
    m_viewColumn.x = inverseView->m[2];
    m_viewColumn.y = inverseView->m[6];
    m_viewColumn.z = inverseView->m[10];
    m_viewColumn.w = inverseView->m[14];

    DecalBatch batch;                                                      // [ebp-3Ch]
    for (int kind = 0; kind < 3; ++kind)
    {
        m_boundTexture = 0;                                                // +0x18
        m_boundShadowMap = 0;                                              // +0x1C
        batch.reset(kind);

        std::vector<DecalObject*>& bucket = buckets[kind];
        for (unsigned int i = 0; i < bucket.size(); ++i)
        {
            addObject(view, bucket[i], batch);
            if (batch.objects.size() == 10)
            {
                pushBatch(batch);
                batch.vertexCount = 0;
                batch.texture = 0;
                batch.shadowMap = 0;
                batch.objects.clear();
                batch.records.clear();
            }
        }
        if (batch.objects.size() != 0)
            pushBatch(batch);
    }
}


// Adds static records at full alpha and dynamic records with age fading. view is unused.
// Captures the object slot before any flush; an object can occupy a slot even when all
// its own records fade out, provided earlier records remain in the batch.

void DecalRenderer::addObject(dx9rend::IRenderView& view, DecalObject* object, DecalBatch& batch)
{
    int objectIndex = static_cast<int>(batch.objects.size());              // [ebp-4]

    for (unsigned int i = 0; i < object->staticCount(); ++i)
    {
        DecalRecord* const record = object->staticRecords()[i];
        record->alpha = 1.0f;
        addRecord(batch, record, object, &objectIndex);
    }

    for (unsigned int j = object->ringOldest() & object->ringMask();
         j != object->ringInsert();
         j = (j + 1) & object->ringMask())
    {
        DecalRecord* const record = object->ringRecords()[j];
        const float t = (record->age - decalManager()->fadeOutTime()) /
                        decalManager()->fadeOutDuration();                 // fsub +0x14, fdiv +0x18
        record->alpha = 1.0f - clampRange(t, 0.0f, 1.0f);                  // sub_1000ED40, fsubr 1.0
        addRecord(batch, record, object, &objectIndex);
    }

    if (batch.records.size() != 0)
        batch.objects.push_back(object);                                   // 0x10126F30 on growth
}


// Adds a visible-alpha record, flushing on texture/shadow change or the 12288-vertex limit.
// NaN alpha is retained. A flush includes the current object, then restarts its slot at zero.

void DecalRenderer::addRecord(DecalBatch& batch, DecalRecord* record, DecalObject* object,
                              int* objectIndex)
{
    if (record->alpha < decalManager()->minimumAlpha())                   // +0x2B8
        return;

    bool changed = false;
    dx9rend::EngineTexture* shadowMap = 0;
    bool haveShadowMap = false;
    if (batch.kind == 1)
    {
        void* const geometry = object->ownerGeometry();     // DecalObject +0x20
        if (geometry && geometryLightRecord(geometry))                     // geometry +0x28
        {
            if (lightRecordUsesSecondShadowMap(geometryLightRecord(geometry)))   // +0x28 again
                shadowMap = lightManagerPointShadowMapTexture(g_LightManager);   // +0x230
            else
                shadowMap = lightManagerShadowMapTexture(g_LightManager);        // +0x21C
            haveShadowMap = true;
        }
    }
    else if (batch.kind == 2)
    {
        shadowMap = lightManagerShadowMapTexture(g_LightManager);              // +0x21C
        haveShadowMap = true;
    }
    if (haveShadowMap && shadowMap != m_boundShadowMap)
    {
        m_boundShadowMap = shadowMap;
        changed = true;
    }

    if (record->texture != m_boundTexture)                                 // record +0x30
    {
        m_boundTexture = record->texture;
        changed = true;
    }

    if ((batch.vertexCount + record->vertexCount >= kVertexCapacity || changed) &&
        batch.records.size() != 0)
    {
        batch.objects.push_back(object);
        pushBatch(batch);
        batch.reset(batch.kind);
        *objectIndex = 0;
    }

    record->instanceIndex = static_cast<short>(*objectIndex);             // `mov cx, [ebx]`
    batch.records.push_back(record);
    batch.vertexCount += record->vertexCount;
    batch.texture = m_boundTexture;
    batch.shadowMap = m_boundShadowMap;
}

// m_batches.push_back(batch): in place (the member copy 0x101AB990)
// while it fits, else _Insert_n (0x101AC520 -> 0x101AC0D0).
void DecalRenderer::pushBatch(const DecalBatch& batch)
{
    m_batches.push_back(batch);
}


// Sorts and draws queued batches with the live view, then releases batch storage.
// Returns false when disabled or empty. Equal-key draw order depends on std::sort.

bool DecalRenderer::render(dx9rend::IRenderView* view)
{
    if (!m_enabled || m_batches.size() == 0)
        return false;

    const Mat4* const projection  = view->getProjectionMatrixPtr();        // +0x6C
    const Mat4* const inverseView = view->getInverseViewMatrixPtr();       // +0x68
    m_worldViewProjection.fullMult(*inverseView, *projection);

    m_fadeDistanceAndInterval.x = decalManager()->fadeDistance();         // +0x2B0
    m_fadeDistanceAndInterval.y = decalManager()->startFadeDistance();    // +0x2B4

    dx9rend::stateManagerPush();
    dx9rend::stateManagerSetRenderState(0xC3u, kDecalDepthBiasBits);       // D3DRS_DEPTHBIAS
    dx9rend::stateManagerSetRenderState(0xAFu, kDecalSlopeScaleDepthBiasBits);   // slopescale
    m_paramFadeDistanceAndInterval->setValue(&m_fadeDistanceAndInterval, 0);   // +0x18

    m_boundTexture = 0;
    m_boundShadowMap = 0;
    std::sort(m_batches.begin(), m_batches.end(), batchLess);

    int pass = 3;                                                          // [ebp-8]
    for (unsigned int i = 0; i < m_batches.size(); ++i)
    {
        const DecalBatch& batch = m_batches[i];
        if (batch.kind != pass)
        {
            if (pass != 3)
                dx9rend::effectEnd(m_effect);
            pass = batch.kind;
            beginPass(batch.kind);
        }
        drawBatch(view, batch);
    }
    if (pass != 3)
        dx9rend::effectEnd(m_effect);

    m_batches.clear();                         // 0x101ABC60; retail's VC7.1 clear also freed storage
    dx9rend::stateManagerPop();
    return true;
}


// Binds lighting, camera, fog, and transform constants and begins the requested effect pass.
// Resets the vertex cursor so the next lock discards previous data.

void DecalRenderer::beginPass(int kind)
{
    m_vertexCursor = 100000;                                               // 0x186A0

    Vec4 ambient;
    const float* packed = staticMeshPackVec4(&g_LightManager->getAmbientColor().x, 1.0f);
    ambient.x = packed[0];
    ambient.y = packed[1];
    ambient.z = packed[2];
    ambient.w = packed[3];
    m_paramAmbientColor->setValue(&ambient, 0);                            // +0x1CC

    Vec4 sunColor;
    sunColor.x = 0.0f; sunColor.y = 0.0f; sunColor.z = 0.0f; sunColor.w = 0.0f;
    Vec4 sunDirection;
    sunDirection.x = 0.0f; sunDirection.y = 0.0f; sunDirection.z = 0.0f; sunDirection.w = 0.0f;
    world::LightSource* const sun = g_LightManager->getSkyLight();         // +0x28
    if (sun)
    {
        packed = staticMeshPackVec4(&sun->getColor().x, 1.0f);             // +0x198
        sunColor.x = packed[0];
        sunColor.y = packed[1];
        sunColor.z = packed[2];
        sunColor.w = packed[3];
        packed = staticMeshPackVec4(&sun->getDirection().x, 1.0f);         // +0x17C
        sunDirection.x = packed[0];
        sunDirection.y = packed[1];
        sunDirection.z = packed[2];
        sunDirection.w = packed[3];
    }
    m_paramSunColor->setValue(&sunColor, 0);                               // +0x1D0
    m_paramSunDirection->setValue(&sunDirection, 0);                       // +0x1D4
    m_paramWorldViewProjection->setValue(&m_worldViewProjection, 0);       // +0x1D8

    const Mat4* const cameraView = renderer()->getRenderViewDup()->getViewMatrixPtr();  // +0x50, +0x64
    m_paramWorldSpaceCamPos->setValue(&cameraView->m[12], 0);              // +0x1F8, view + 0x30

    m_paramFogRange->setValue(raConstantAt(0xA8), 0);                    // +0x1E4
    m_paramFogColor->setValue(raConstantAt(0xBC), 0);                    // +0x1E8
    m_paramWaterFogColor->setValue(raConstantAt(0x10C), 0);              // +0x1EC, sub_10037EB0
    m_paramWaterFogRange->setValue(raConstantAt(0xF8), 0);               // +0x1F0, sub_10037EA0
    m_paramCameraToWater->setValue(raConstantAt(0x1D8), 0);              // +0x1F4

    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, static_cast<unsigned int>(kind));
}


// Draws a batch with discard when the ring reaches capacity, otherwise nooverwrite.
// Returns false on lock failure, true after drawing or for an empty batch.

bool DecalRenderer::drawBatch(dx9rend::IRenderView* view, const DecalBatch& batch)
{
    if (batch.records.size() == 0)
        return true;

    setInstanceTransforms(batch);
    switch (batch.kind)
    {
    case 1:
        setDynamicShadowParameters(batch);
        break;
    case 2:
        setStaticShadowParameters(batch);
        break;
    default:
        break;
    }

    if (batch.shadowMap && m_boundShadowMap != batch.shadowMap)
    {
        m_paramShadowMapTex->setValue(batch.shadowMap, 0);                 // +0x1E0
        m_boundShadowMap = batch.shadowMap;
    }
    if (batch.texture && m_boundTexture != batch.texture)
    {
        m_paramTexLayer0->setValue(batch.texture, 0);                      // +0x1DC
        m_boundTexture = batch.texture;
    }

    unsigned int written = 0;                                              // [ebp-4]
    unsigned int start;                                                    // [ebp-10h]
    unsigned int flags;
    if (m_vertexCursor + batch.vertexCount >= kVertexCapacity)
    {
        m_vertexCursor = 0;
        start = 0;
        flags = kLockDiscard;
    }
    else
    {
        start = m_vertexCursor;
        flags = kLockNoOverwrite;
    }
    unsigned char* const locked = static_cast<unsigned char*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, start, batch.vertexCount, flags));
    if (!locked)
        return false;

    for (unsigned int r = 0; r < batch.records.size(); ++r)
    {
        const DecalRecord* const record = batch.records[r];
        const DecalVertex* const source = record->vertices;
        for (unsigned int k = 0; k < record->vertexCount; ++k)
        {
            DecalGpuVertex* const out = reinterpret_cast<DecalGpuVertex*>(locked + written * kVertexStride);
            out->y = source[k].position.y;
            out->x = source[k].position.x;
            out->z = source[k].position.z;
            ++written;
            out->nx = record->normal.x;
            out->ny = record->normal.y;
            out->nz = record->normal.z;
            out->diffuse = record->diffuse;                                // +0x14
            out->u = source[k].u;
            out->v = source[k].v;
            out->instance = static_cast<float>(record->instanceIndex);     // movsx; fild
            out->alpha = record->alpha;                                    // +0x28
            ++m_vertexCursor;
        }
    }

    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    if (written > 0)
        dx9rend::rendererDrawPrimitive(written / 3, kTriangleList, start);
    return true;
}


// one visual transform per batch object: the owner's (+0x1C)
// IObject::getVisualAbsoluteTransformation (PE +0x8C - a host call, typed in both flavours on
// ObjectDrawer.cpp's precedent), or the shared identity (sub_100791E0) for an ownerless (ground)
// object; then InstanceTransformations (+0x1BC) gets the pointer array and the object count (+0x28).
// No bound check against the ten slots - collect keeps a batch to ten objects.

void DecalRenderer::setInstanceTransforms(const DecalBatch& batch)
{
    for (unsigned int i = 0; i < batch.objects.size(); ++i)
    {
        world::IObject* const owner =
            static_cast<world::IObject*>(batch.objects[i]->ownerObject());
        if (owner)
            m_instanceTransforms[i] = &owner->getVisualAbsoluteTransformation();   // PE +0x8C
        else
            m_instanceTransforms[i] = &Mat4::getIdentity();
    }
    const unsigned int count = static_cast<unsigned int>(batch.objects.size());
    m_paramInstanceTransformations->setMatrixPointerArray(
        reinterpret_cast<const void* const*>(m_instanceTransforms), count);      // +0x28
}


// kind 1: per object, when its geometry has a render record (+0x28,
// called three times - test, matrix, viewport): ShadowTransformations[i] = the record's +0x2F8 and
// ShadowViewPortMaps[i] = its +0x438..+0x444; otherwise the identity and a zero viewport. Then the
// two parameters, each with the object count.

void DecalRenderer::setDynamicShadowParameters(const DecalBatch& batch)
{
    for (unsigned int i = 0; i < batch.objects.size(); ++i)
    {
        void* const geometry = batch.objects[i]->ownerGeometry();
        if (geometry && geometryLightRecord(geometry))
        {
            m_shadowTransforms[i] = lightRecordShadowMatrix(geometryLightRecord(geometry));
            const float* const viewPort = lightRecordViewPort(geometryLightRecord(geometry));
            m_shadowViewPorts[i].y = viewPort[1];
            m_shadowViewPorts[i].z = viewPort[2];
            m_shadowViewPorts[i].w = viewPort[3];
            m_shadowViewPorts[i].x = viewPort[0];
        }
        else
        {
            m_shadowTransforms[i] = &Mat4::getIdentity();
            m_shadowViewPorts[i].y = 0.0f;
            m_shadowViewPorts[i].z = 0.0f;
            m_shadowViewPorts[i].w = 0.0f;
            m_shadowViewPorts[i].x = 0.0f;
        }
    }
    const unsigned int count = static_cast<unsigned int>(batch.objects.size());
    m_paramShadowTransformations->setMatrixPointerArray(
        reinterpret_cast<const void* const*>(m_shadowTransforms), count);  // +0x1C0, +0x28
    m_paramShadowViewPortMaps->setValue(m_shadowViewPorts, count);         // +0x1C8, +0x18
}


// kind 2: per object, its slot +0x08 record (called once): the
// record's +0x2F8 and +0x3B8 matrices and its viewport; otherwise the identity twice (two
// sub_100791E0 calls) and a zero viewport. Then ShadowTransformations, ShadowTrapezoidMats and
// ShadowViewPortMaps, each with the object count.

void DecalRenderer::setStaticShadowParameters(const DecalBatch& batch)
{
    for (unsigned int i = 0; i < batch.objects.size(); ++i)
    {
        void* const record = batch.objects[i]->getShadowRecord();   // slot +0x08
        if (record)
        {
            m_shadowTransforms[i] = lightRecordShadowMatrix(record);
            m_shadowTrapezoidMats[i] = lightRecordTrapezoidMatrix(record);
            const float* const viewPort = lightRecordViewPort(record);
            m_shadowViewPorts[i].y = viewPort[1];
            m_shadowViewPorts[i].z = viewPort[2];
            m_shadowViewPorts[i].w = viewPort[3];
            m_shadowViewPorts[i].x = viewPort[0];
        }
        else
        {
            m_shadowTransforms[i] = &Mat4::getIdentity();
            m_shadowTrapezoidMats[i] = &Mat4::getIdentity();
            m_shadowViewPorts[i].y = 0.0f;
            m_shadowViewPorts[i].z = 0.0f;
            m_shadowViewPorts[i].w = 0.0f;
            m_shadowViewPorts[i].x = 0.0f;
        }
    }
    unsigned int count = static_cast<unsigned int>(batch.objects.size());
    m_paramShadowTransformations->setMatrixPointerArray(
        reinterpret_cast<const void* const*>(m_shadowTransforms), count);  // +0x1C0
    count = static_cast<unsigned int>(batch.objects.size());
    m_paramShadowTrapezoidMats->setMatrixPointerArray(
        reinterpret_cast<const void* const*>(m_shadowTrapezoidMats), count);   // +0x1C4
    count = static_cast<unsigned int>(batch.objects.size());
    m_paramShadowViewPortMaps->setValue(m_shadowViewPorts, count);         // +0x1C8
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
