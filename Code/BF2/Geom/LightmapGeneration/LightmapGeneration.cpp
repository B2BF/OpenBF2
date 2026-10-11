// The renderer's lightmap generator (LightmapGeneration.h), recovered from RendDX9.dll 1.50,
// with both traces (intensity and colour).
//
// Shipped defects kept, each beside its body: the occluder sort builds and drops its map; the GI top
// light's shadow intensity is stored and then overwritten with 1; bit 0 of generationMethod reads
// the sun's tile at index -1 when no sun was gathered; the per-block addLight table is never freed;
// the traces release neither the saved depth-stencil reference nor (on the way out) restore the
// duplicate view through its own setter; the resume log closes its file twice on its failed!/
// ASSERT arms; the light marker's WorldView diagonal is replaced by the light's scale.
#ifdef BF2_IMAGE_RENDDX9

#include "LightmapGeneration.h"
#include "ILightSampleable.h"
#include "../IGeometryTemplate.h"
#include "../ITerrainEditable.h"
#include "../StaticMesh.h"                  // m_template, staticMeshRootParent
#include "../StaticMeshTemplate.h"          // m_isRoad / m_isVegetation / m_isWater
#include "../XpLightManager.h"

#include "../../Core/System.h"
#include "../../Math/Frustum.h"
#include "../../Math/Mat4.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../../RendDX9/Texture.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendMisc/IRenderer.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/Vc71String.h"
#include "../../../RendDX9Abi/Vc71Vector.h"

// The engine interfaces below are imported for their virtual order; in the retail flavour the
// renderer's own IBase/IClassManager (HfeAbi.h) stand in for the host headers, exactly as
// XpLightManager.cpp brackets the same includes.
#if !BF2_PLUGIN_ABI_NATIVE
#define CORE_IBASE_H
#define CORE_CLASSMANAGER_H
#endif
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Game/Objects/ObjectLayoutAccess.h"
#include "../../Game/Objects/Visual/LightSource.h"
#include "../../Game/Objects/Visual/LightSourceTemplate.h"
#include "../../Physics/Heightmap.h"
#include "../../Physics/HeightmapCluster.h"
#include "../../Scene/Object/ObjectManager.h"
#if !BF2_PLUGIN_ABI_NATIVE
#undef CORE_CLASSMANAGER_H
#undef CORE_IBASE_H
#endif

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>

namespace dice {
namespace hfe {

extern System* g_system;            // PluginEntry.cpp
extern IBase* objectManager;        // PluginEntry.cpp

namespace dx9rend {

extern void* g_rendererBase;
extern void* g_shaderManager;
extern void* g_bufferManager;

// BufferManager.cpp, StateManager.cpp and RendDX9Draw.cpp seams, declared per TU as the other
// geometry classes declare them.
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const vc71::string& name, unsigned int usage);
int   bufferManagerSwapPool(int value);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type, unsigned int startVertex);
void  rendererStatsBeginFrame();

} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;

// GeomMisc/FontSystem/FontSystem.cpp - Font::Ported and its +0x10 load / +0x18 drawText slots.
IBase* createFontPortedDirect();
bool   fontPortedLoad(IBase* font, const vc71::string& path);
void   fontPortedDrawText(IBase* font, float x, float y, const vc71::string& text);

LightmapGeneration* g_LightmapGeneration = 0;

namespace {

// Raised when the progress callback answers false; every trace clears it.
bool g_traceAborted = false;
// The object being traced by traceAllObjects and the count it shows; the single-object traces
// clear the count.
int  g_traceObjectIndex = 0;
int  g_traceObjectCount = 0;
// The direction of the last sun the trace set a view up for; bit 4 of generationMethod reads it
// back.
Vec3 g_traceSunDirection = { 0.0f, 0.0f, 0.0f };

const unsigned int kStaticMeshTemplateClassId = 0x870CA653u;  // GeometryTemplate_StaticMesh_reflectionTypeId
// Core/Ids.h values kept file-local: Ids.h also defines CID_LightManager / IID_ILightManager, which
// XpLightManager.h declares extern, so including both would define those twice
// (ObjectGeometryConsole.cpp's kIID_IGeometry precedent). ITerrainEditable.h supplies
// kIID_ITerrainEditable.
const unsigned int kIIDLightSampleable    = 3955786235u;   // geom::IID_ILightSampleable (0xEBC881FB)
const unsigned int kIIDLightmapGeneration = 3087129850u;   // geom::IID_ILightmapGeneration (0xB801DCFA)
const unsigned int kCIDRenderView         = 820301u;       // dx9rend::CID_RenderView
const unsigned int kIIDRenderView         = 820201u;       // dx9rend::IID_IRenderView
const unsigned int kUnsetBits = 0xBF800000u;                  // -1.0f: "use the light's own value"
const unsigned int kTargetSize = 0x400u;                      // the 1024 x 1024 target
const unsigned int kFormatA8R8G8B8 = 21u;
const unsigned int kFormatD24S8 = 75u;

// The mesh sampler's triangle record as far as the back-face test reads it (the
// LightSampleBlock::triangles vector's element).
struct SampleTriangle
{
    unsigned char unread00[0x24];
    Vec3          normal[3];        // +0x24 / +0x30 / +0x3C
    unsigned char unread48[0x14];
};

// The light marker clone's declaration: one FLOAT3 position.
const D3DVERTEXELEMENT9 kLightMeshDeclaration[2] =
{
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0 }
};

#if defined(_M_IX86)
static_assert(sizeof(SampleTriangle) == 92, "the trace divides the triangle vector by 92");
#endif

inline bool isUnset(float value)
{
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits == kUnsetBits;
}

// Host strings: the retail flavour's references are VC7.1 strings; only the static spelling of
// the reference changes (NametagManager.cpp / XpLightManager.cpp).
#if BF2_PLUGIN_ABI_NATIVE
inline const std::string& hostStdString(const ::renddx9abi::HostString& value) { return value; }
inline const ::renddx9abi::HostString& fromHostStdString(const std::string& value) { return value; }
#else
inline const std::string& hostStdString(const ::renddx9abi::HostString& value)
{
    return *reinterpret_cast<const std::string*>(&value);
}
inline const ::renddx9abi::HostString& fromHostStdString(const std::string& value)
{
    return *reinterpret_cast<const ::renddx9abi::HostString*>(&value);
}
#endif

// The light and object template reads XpLightManager.cpp makes: the retail flavour reads the
// retail layout's fields, the native flavour the rebuilt classes' accessors.
inline const world::LightSourceTemplate* lightSourceTemplatePointer(const world::LightSource* light)
{
#if BF2_PLUGIN_ABI_NATIVE
    return static_cast<const world::LightSourceTemplate*>(light->getTemplate());
#else
    return *reinterpret_cast<const world::LightSourceTemplate* const*>(
        reinterpret_cast<const unsigned char*>(light) + 0x144);
#endif
}

inline unsigned int lightSourceFlags(const world::LightSource* light)
{
#if BF2_PLUGIN_ABI_NATIVE
    return light->getFlags();
#else
    return *reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(light) + 0x04);
#endif
}

inline const world::IObjectTemplate* objectTemplatePointer(const world::IObject* object)
{
#if BF2_PLUGIN_ABI_NATIVE
    return object->getTemplate();
#else
    return *reinterpret_cast<const world::IObjectTemplate* const*>(
        reinterpret_cast<const unsigned char*>(object) + 0x14);
#endif
}

inline world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));                    // ClassManager +0x48
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(found));
}

inline world::ObjectManager* hostObjectManager()
{
    return reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager);
}

inline dx9rend::IRenderer* renderer()
{
    return static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase);
}

inline IDirect3DDevice9* renderDevice()
{
    return static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());   // Renderer +0x0C
}

// The geometry instances are RendDX9's own (StaticMesh & co.) and have no typed IGeometry in this
// tree; each slot is reached through the instance's table (OverGrowthLoad.cpp's precedent).
inline void* geometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / sizeof(void*)];
}
inline void geometryDrawSilhouette(void* geometry, void* renderView, int legacyArm)   // +0x1C
{
    typedef void (__thiscall* Fn)(void*, void*, int);
    reinterpret_cast<Fn>(geometrySlot(geometry, 0x1C))(geometry, renderView, legacyArm);
}
inline IGeometryTemplate* geometryGetTemplate(void* geometry)                           // +0x38
{
    typedef void* (__thiscall* Fn)(void*);
    return static_cast<IGeometryTemplate*>(reinterpret_cast<Fn>(geometrySlot(geometry, 0x38))(geometry));
}
// +0x40 - the selected lod's local box: the engine's box (MeshBounds), whose derived fields
// refresh exactly as a TerrainAabb's do.
inline TerrainAabb* geometryLocalBounds(void* geometry)
{
    typedef void* (__thiscall* Fn)(void*);
    return static_cast<TerrainAabb*>(reinterpret_cast<Fn>(geometrySlot(geometry, 0x40))(geometry));
}
inline void geometrySetGeom(void* geometry, int geom)                                   // +0x6C
{
    typedef void (__thiscall* Fn)(void*, int);
    reinterpret_cast<Fn>(geometrySlot(geometry, 0x6C))(geometry, geom);
}
inline int geometryGetGeomCount(void* geometry)                                         // +0x74
{
    typedef int (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>(geometrySlot(geometry, 0x74))(geometry);
}
inline void geometrySetLod(void* geometry, int lod)                                     // +0x78
{
    typedef void (__thiscall* Fn)(void*, int);
    reinterpret_cast<Fn>(geometrySlot(geometry, 0x78))(geometry, lod);
}
inline int geometryGetLodCount(void* geometry)                                          // +0x80
{
    typedef int (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>(geometrySlot(geometry, 0x80))(geometry);
}

// The image's printf-into-a-string helper (TerrainLightSampling.cpp carries the same body).
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

// "%dmin %dsec", or "%dsec" under a minute; the seconds are unsigned. The trace inlines the
// same body four times.
void formatTime(vc71::string& out, unsigned int seconds)
{
    const unsigned int minutes = seconds / 60u;
    if (minutes)
        formatString(out, "%dmin %dsec", minutes, seconds - minutes * 60u);
    else
        formatString(out, "%dsec", seconds - minutes * 60u);
}

// Clamps to [low, high], testing the high bound first; a NaN fails the first compare and takes
// the low bound.
inline float clampValue(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// The trace's inline clamp to [0, 1]; a NaN passes through unchanged.
inline float clampUnit(float value)
{
    if (value > 1.0f)
        return 1.0f;
    if (value < 0.0f)
        return 0.0f;
    return value;
}

// The colour trace's clear colour: opaque, each channel (int)(c * 255) truncated toward zero and
// cut to its low byte.
inline unsigned int opaqueClearColour(const Vec3& colour)
{
    const unsigned int red = static_cast<unsigned int>(static_cast<int>(colour.x * 255.0f)) & 0xFFu;
    const unsigned int green = static_cast<unsigned int>(static_cast<int>(colour.y * 255.0f)) & 0xFFu;
    const unsigned int blue = static_cast<unsigned int>(static_cast<int>(colour.z * 255.0f)) & 0xFFu;
    return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

// Normalises a 2-vector in place: left alone within FLT_EPSILON of unit length, zeroed (answering
// -20001) below 1.4210855e-14, otherwise scaled by 1 / sqrt(|v|^2).
int normalize2(float* v)
{
    const float lengthSquared = v[0] * v[0] + v[1] * v[1];
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < 1.4210855e-14f)
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v[0] = inverse * v[0];
    v[1] = inverse * v[1];
    return 0;
}

// The engine's in-place normalise (QuadPatch.cpp carries the same body): left alone within
// FLT_EPSILON of unit length, zeroed below FLT_EPSILON, otherwise scaled.
int normalize3(float* v)
{
    const float lengthSquared = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v[0] = inverse * v[0];
    v[1] = inverse * v[1];
    v[2] = inverse * v[2];
    return 0;
}

// The summed component distance of two Vec4s is within FLT_EPSILON (w first).
inline bool vec4ApproxEqual(const float* a, const float* b)
{
    const float distance = ((std::fabs(a[3] - b[3]) + std::fabs(a[2] - b[2]))
                            + std::fabs(a[1] - b[1])) + std::fabs(a[0] - b[0]);
    return distance <= FLT_EPSILON;
}

// Row by row.
inline bool mat4ApproxEqual(const Mat4& a, const Mat4& b)
{
    return vec4ApproxEqual(a.m, b.m) && vec4ApproxEqual(a.m + 4, b.m + 4)
        && vec4ApproxEqual(a.m + 8, b.m + 8) && vec4ApproxEqual(a.m + 12, b.m + 12);
}

// Every component within FLT_EPSILON (a NaN fails).
inline bool vec3ApproxEqual(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= FLT_EPSILON && std::fabs(a.y - b.y) <= FLT_EPSILON
        && std::fabs(a.z - b.z) <= FLT_EPSILON;
}

// Rotates `v` in place by the matrix's upper 3x3 (row vector), z first.
inline void rotateInPlace(const Mat4& matrix, Vec3& v)
{
    const float* const m = matrix.m;
    const float z = m[10] * v.z + m[2] * v.x + m[6] * v.y;
    const float y = m[9] * v.z + m[1] * v.x + m[5] * v.y;
    const float x = m[8] * v.z + m[4] * v.y + v.x * m[0];
    v.x = x;
    v.y = y;
    v.z = z;
}

// A point through the full matrix; all three rows are formed before any is stored.
inline void transformPoint(const Mat4& matrix, const Vec3& in, Vec3& out)
{
    const float* const m = matrix.m;
    const float z = m[2] * in.x + m[10] * in.z + m[6] * in.y + m[14];
    const float y = m[1] * in.x + m[9] * in.z + m[5] * in.y + m[13];
    const float x = m[8] * in.z + m[4] * in.y + in.x * m[0] + m[12];
    out.x = x;
    out.y = y;
    out.z = z;
}

// a x b, in the shipped operand order.
inline void cross(float* out, const float* a, const float* b)
{
    out[0] = b[2] * a[1] - b[1] * a[2];
    out[1] = b[0] * a[2] - a[0] * b[2];
    out[2] = a[0] * b[1] - b[0] * a[1];
}

// out = transpose(in).
inline void mat4Transpose(Mat4& out, const Mat4& in)
{
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            out.m[row * 4 + column] = in.m[column * 4 + row];
}

// out = a * b, each element with the shipped association and store order.
void mat4Mult(Mat4& out, const Mat4& left, const Mat4& right)
{
    const float* const a = left.m;
    const float* const b = right.m;
    float* const o = out.m;
    o[0]  = ((a[0] * b[0] + b[8] * a[2]) + a[1] * b[4]) + a[3] * b[12];
    o[4]  = ((a[5] * b[4] + b[8] * a[6]) + a[7] * b[12]) + b[0] * a[4];
    o[8]  = ((a[11] * b[12] + a[8] * b[0]) + a[10] * b[8]) + a[9] * b[4];
    o[12] = ((b[8] * a[14] + a[13] * b[4]) + a[15] * b[12]) + a[12] * b[0];
    o[1]  = ((b[13] * a[3] + a[1] * b[5]) + b[1] * a[0]) + b[9] * a[2];
    o[5]  = ((a[5] * b[5] + b[1] * a[4]) + a[7] * b[13]) + a[6] * b[9];
    o[9]  = ((b[5] * a[9] + b[1] * a[8]) + a[11] * b[13]) + a[10] * b[9];
    o[13] = ((b[1] * a[12] + b[13] * a[15]) + b[9] * a[14]) + b[5] * a[13];
    o[2]  = ((b[14] * a[3] + a[1] * b[6]) + b[2] * a[0]) + b[10] * a[2];
    o[6]  = ((a[5] * b[6] + b[2] * a[4]) + a[7] * b[14]) + a[6] * b[10];
    o[10] = ((b[6] * a[9] + b[2] * a[8]) + a[11] * b[14]) + a[10] * b[10];
    o[14] = ((b[2] * a[12] + b[14] * a[15]) + b[10] * a[14]) + b[6] * a[13];
    o[3]  = ((a[0] * b[3] + b[15] * a[3]) + b[11] * a[2]) + a[1] * b[7];
    o[7]  = ((b[11] * a[6] + a[5] * b[7]) + b[3] * a[4]) + a[7] * b[15];
    o[11] = ((a[8] * b[3] + b[7] * a[9]) + a[11] * b[15]) + a[10] * b[11];
    o[15] = ((b[11] * a[14] + b[15] * a[15]) + b[7] * a[13]) + a[12] * b[3];
}

// The image's look-at: rows 0..2 are right, up and forward, the translation `eye`.
// A forward vector shorter than FLT_EPSILON leaves the identity; one within FLT_EPSILON of +-up
// keeps identity right/up rows and a zero translation.
void lookAt(Mat4& out, const Vec3& eye, const Vec3& target, const Vec3& up)
{
    float* const m = out.m;
    for (int i = 0; i < 16; ++i)
        m[i] = 0.0f;
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
    const float fx = target.x - eye.x;
    const float fy = target.y - eye.y;
    const float fz = target.z - eye.z;
    m[10] = fz;
    m[8] = fx;
    m[9] = fy;
    const float length = std::sqrt((m[8] * m[8] + m[9] * m[9]) + m[10] * m[10]);
    if (length < FLT_EPSILON)
    {
        m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f; m[0] = 1.0f;
        m[5] = 1.0f; m[4] = 0.0f; m[6] = 0.0f; m[7] = 0.0f;
        m[10] = 1.0f; m[8] = 0.0f; m[9] = 0.0f; m[11] = 0.0f;
        m[15] = 1.0f; m[13] = 0.0f; m[14] = 0.0f; m[12] = 0.0f;
        return;
    }
    const float inverse = 1.0f / length;
    m[8] = inverse * m[8];
    m[9] = inverse * m[9];
    m[10] = inverse * m[10];
    if (std::fabs(((m[10] * up.z + m[9] * up.y) + m[8] * up.x) - 1.0f) < FLT_EPSILON)
        return;
    if (std::fabs(((m[10] * up.z + m[9] * up.y) + m[8] * up.x) + 1.0f) < FLT_EPSILON)
        return;
    cross(m, &up.x, m + 8);
    normalize3(m);
    cross(m + 4, m + 8, m);
    normalize3(m + 4);
    m[12] = eye.x;
    m[13] = eye.y;
    m[14] = eye.z;
}

// Grows the box to hold `p` (max tested before min on each axis), then dirties it.
void expandBox(TerrainAabb& box, const Vec3& p)
{
    if (p.x > box.max.x) box.max.x = p.x;
    if (p.x < box.min.x) box.min.x = p.x;
    if (p.y > box.max.y) box.max.y = p.y;
    if (p.y < box.min.y) box.min.y = p.y;
    if (p.z > box.max.z) box.max.z = p.z;
    if (p.z < box.min.z) box.min.z = p.z;
    box.dirty = 1;
}

// The box's radius and centre, refreshed while dirty.
inline float boxRadius(TerrainAabb& box)
{
    if (box.dirty)
        box.refresh();
    return box.radius;
}
inline const Vec3& boxCentre(TerrainAabb& box)
{
    if (box.dirty)
        box.refresh();
    return box.centre;
}

// Does the sphere's bounding cube overlap the box? (min at +0, max at +0xC only).
bool aabbSphereOverlap(const TerrainAabb& box, Vec3 centre, float radius)
{
    const float low[3]  = { centre.x - radius, centre.y - radius, centre.z - radius };
    const float high[3] = { centre.x + radius, centre.y + radius, centre.z + radius };
    const float* const boxMin = &box.min.x;
    const float* const boxMax = &box.max.x;
    for (int i = 0; i < 3; ++i)
    {
        if (low[i] > boxMax[i])
            return false;
        if (boxMin[i] > high[i])
            return false;
    }
    return true;
}

// Every corner of a 24-float box through the full matrix, with the shipped per-corner operand
// order: corner 0's y and x rows associate differently from corners 1..7 (XpLightManager.cpp's
// ShadowBoxCorners::transform is the same body).
void transformCorners(float* corners, const Mat4& world)
{
    const float* const m = world.m;
    for (unsigned int i = 0; i < 8; ++i)
    {
        float* const c = corners + 3 * i;
        const float x = c[0];
        const float y = c[1];
        const float z = c[2];
        const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
        const float ty = i == 0 ? x * m[1] + y * m[5] + z * m[9] + m[13]
                                : x * m[1] + z * m[9] + y * m[5] + m[13];
        const float tx = i == 0 ? z * m[8] + x * m[0] + y * m[4] + m[12]
                                : y * m[4] + z * m[8] + x * m[0] + m[12];
        c[0] = tx;
        c[1] = ty;
        c[2] = tz;
    }
}

// The smallest and largest corner per axis; a component moves only on a strict, ordered compare.
Vec3 cornersMinimum(const float* corners)
{
    Vec3 result = { corners[0], corners[1], corners[2] };
    for (unsigned int i = 1; i < 8; ++i)
    {
        const float* const c = corners + 3 * i;
        if (result.x > c[0]) result.x = c[0];
        if (result.y > c[1]) result.y = c[1];
        if (result.z > c[2]) result.z = c[2];
    }
    return result;
}
Vec3 cornersMaximum(const float* corners)
{
    Vec3 result = { corners[0], corners[1], corners[2] };
    for (unsigned int i = 1; i < 8; ++i)
    {
        const float* const c = corners + 3 * i;
        if (result.x < c[0]) result.x = c[0];
        if (result.y < c[1]) result.y = c[1];
        if (result.z < c[2]) result.z = c[2];
    }
    return result;
}

// False as soon as one frustum plane has all eight corners strictly in front of it. Corner 0's
// distance is (z*n.z + x*n.x) + y*n.y + d, corners 1..7's (y*n.y + z*n.z) + x*n.x + d
// (Overgrowth/OverGrowthRender.cpp's boxCornersInsideFrustum is the same body).
bool cornersInsideFrustum(const float* corners, const Frustum& frustum)
{
    for (unsigned int plane = 0; plane < 6; ++plane)
    {
        const Plane3& p = frustum.m_planes[plane];
        int outside = 0;
        if (corners[2] * p.normal.z + corners[0] * p.normal.x + corners[1] * p.normal.y + p.d > 0.0f)
            outside += 1;
        for (unsigned int i = 1; i < 8; ++i)
        {
            const float* const c = corners + 3 * i;
            if (c[1] * p.normal.y + c[2] * p.normal.z + c[0] * p.normal.x + p.d > 0.0f)
                outside += 1;
        }
        if (outside == 8)
            return false;
    }
    return true;
}

// The phong response curve: the value raised to 0.1, 0.15, 0.2, 0.25 or 0.3 (the image's double
// constants) for modes 1..5, unchanged otherwise.
float responseCurve(int mode, float value)
{
    switch (mode)
    {
    case 1: return static_cast<float>(std::pow(static_cast<double>(value), 0.10000000149011612));
    case 2: return static_cast<float>(std::pow(static_cast<double>(value), 0.15000000596046448));
    case 3: return static_cast<float>(std::pow(static_cast<double>(value), 0.20000000298023224));
    case 4: return static_cast<float>(std::pow(static_cast<double>(value), 0.25));
    case 5: return static_cast<float>(std::pow(static_cast<double>(value), 0.30000001192092896));
    default: return value;
    }
}

// The device's current viewport, field by field.
void getViewport(unsigned int& x, unsigned int& y, unsigned int& width, unsigned int& height,
                 float& minZ, float& maxZ)
{
    D3DVIEWPORT9 viewport;
    renderDevice()->GetViewport(&viewport);
    x = viewport.X;
    y = viewport.Y;
    width = viewport.Width;
    height = viewport.Height;
    minZ = viewport.MinZ;
    maxZ = viewport.MaxZ;
}

// The trace's inline viewport stores, gathered into one helper.
inline void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                        float minZ, float maxZ)
{
    D3DVIEWPORT9 viewport;
    viewport.X = x;
    viewport.Y = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinZ = minZ;
    viewport.MaxZ = maxZ;
    renderDevice()->SetViewport(&viewport);
}

// The last light of the published generator (the global, not the caller), or null.
LightInfo* lastLight()
{
    LightmapGeneration* const generator = g_LightmapGeneration;
    if (!generator || !generator->m_lights.data() || generator->m_lights.size() == 0)
        return 0;
    return &generator->m_lights.back();
}

// The sky light's azimuth in radians: its direction reversed onto the xz plane,
// normalised, acos of the z component, mirrored to 2*pi - angle on the negative x side. Zero
// without a sky light.
float sunAzimuth()
{
    if (!g_LightManager->getSkyLight())                                       // +0x28
        return 0.0f;
    const Vec3& direction = g_LightManager->getSkyLight()->getDirection();    // +0x28, +0x17C
    float flat[2] = { direction.x * -1.0f, direction.z * -1.0f };
    normalize2(flat);
    const float angle = std::acos(clampValue(flat[0] * 0.0f + flat[1], -1.0f, 1.0f));
    if (flat[0] < 0.0f)
        return 6.2831855f - angle;
    return angle;
}

// The first block whose box holds `position` (x and z inclusive, y within -9999..9999), else 0.
// The shipped body is a member that never reads its object.
unsigned int nearestBlock(const Vec3& position, ILightSampling* sampler)
{
    for (unsigned int i = 0; i < sampler->getBlockCount(); ++i)
    {
        const LightSampleBlock block = sampler->getBlock(i);
        if (!(block.bounds.min.x <= position.x))
            continue;
        if (!(position.y >= -9999.0f))
            continue;
        if (!(position.z >= block.bounds.min.z))
            continue;
        if (!(position.x <= block.bounds.max.x))
            continue;
        if (!(position.y <= 9999.0f))
            continue;
        if (position.z <= block.bounds.max.z)
            return i;
    }
    return 0;
}

// Fills `record` for an object that is alive and does not move: its position and transformation,
// its geometry, the geometry box's corners in world space, the box centre in world space and its
// radius (capped at 20000). False for objects farther than 10000 from the origin and objects
// without geometry. The shipped body is a member that never reads its object.
bool fillObjectRecord(world::IObject* object, OccluderRecord* record)
{
    if (!object)
        return false;
    const unsigned int flags = object->getFlags();
    if ((flags & world::IObject::OFHasMobilePhysics) || !(flags & world::IObject::OFIsAlive))
        return false;
    record->object = object;
    record->position = object->getAbsolutePosition();                        // +0x68
    record->transform = object->getAbsoluteTransformation();                 // +0x80
    void* const geometry = world::object_detail::LayoutAccess::geometry(object);
    record->geometry = geometry;
    if (!geometry)
        return false;
    const Vec3& p = record->position;
    if (std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) > 10000.0f)       // a NaN distance passes
        return false;
    aabbCorners(record->corners, &geometryLocalBounds(geometry)->min.x);
    transformCorners(record->corners, record->transform);
    record->centre = boxCentre(*geometryLocalBounds(geometry));
    rotateInPlace(record->transform, record->centre);
    record->centre.x = record->centre.x + record->position.x;
    record->centre.y = record->centre.y + record->position.y;
    record->centre.z = record->centre.z + record->position.z;
    record->radius = boxRadius(*geometryLocalBounds(geometry));
    if (record->radius > 20000.0f)
        record->radius = 20000.0f;
    return true;
}

} // anonymous namespace


// vector<bool>::resize(count, false). [CHOSEN, not recovered] the word handling follows VC7.1's
// resize semantics (insert falses at the end / trim and mask the last word); the image's insert
// and erase bodies are not transcribed. Every bit below `count` is written by the trace before it
// is read.
void LightFacingBits::resize(unsigned int count)
{
    if (size < count)
    {
        words.resize((count + 31u) >> 5, 0u);
        const unsigned int old = size;
        size = count;
        for (unsigned int i = old; i < count; ++i)
            clear(i);
    }
    else if (count < size)
    {
        const unsigned int needed = (count + 31u) >> 5;
        if (needed < words.size())
            words.resize(needed);
        size = count;
        if (count % 32u)
            words[needed - 1] &= (1u << (count % 32u)) - 1u;
    }
}


// The debug defaults: position (1360, 118, 700), radius 10, a directional light toward
// normalise(0, 0.3, 1), intensity 1, shadow 0.8, a 8 degree view of 150 m, self-shadowing, 100
// occluders. colorRed stays unset.
LightInfo::LightInfo()
{
    position.x = 1360.0f;
    position.y = 118.0f;
    position.z = 700.0f;
    radius = 10.0f;
    isLocal = 0;
    source = 0;
    direction.x = 0.0f;
    direction.z = 0.0f;
    direction.y = 1.0f;
    intensity = 1.0f;
    shadowIntensity = 0.8f;
    fov = 8.0f;
    viewDistance = 150.0f;
    selfShadowing = 1;
    objectShadows = 0;
    softness = 0.0f;
    maxOccluders = 100;
    isGI = 0;
    renderView = 0;
    spotSource = 0;
    direction.x = 0.0f;
    direction.y = 0.3f;
    direction.z = 1.0f;
    normalize3(&direction.x);
}

// Memberwise; the render view pointer is copied without a reference.
LightInfo::LightInfo(const LightInfo& other)
    : position(other.position),
      radius(other.radius),
      isLocal(other.isLocal),
      source(other.source),
      direction(other.direction),
      intensity(other.intensity),
      shadowIntensity(other.shadowIntensity),
      fov(other.fov),
      viewDistance(other.viewDistance),
      selfShadowing(other.selfShadowing),
      objectShadows(other.objectShadows),
      softness(other.softness),
      maxOccluders(other.maxOccluders),
      isGI(other.isGI),
      colorRed(other.colorRed),
      renderView(other.renderView),
      spotSource(other.spotSource),
      facing(other.facing)
{
}

// Releases the render view's reference, then the back-face bits.
LightInfo::~LightInfo()
{
    if (renderView)
        renderView->release();
}

void LightInfo::setDirection(const Vec3& value)
{
    direction = value;
    normalize3(&direction.x);
}
const Vec3& LightInfo::getDirection() const { return direction; }
void  LightInfo::setShadowIntensity(float value) { shadowIntensity = value; }
float LightInfo::getShadowIntensity() const { return shadowIntensity; }
void  LightInfo::setIntensity(float value) { intensity = value; }
float LightInfo::getIntensity() const { return intensity; }
void  LightInfo::setFov(float value) { fov = value; }
float LightInfo::getFov() const { return fov; }
void  LightInfo::setSelfShadowing(bool value) { selfShadowing = value; }
bool  LightInfo::getSelfShadowing() const { return selfShadowing != 0; }
void  LightInfo::setObjectShadows(bool value) { objectShadows = value; }
bool  LightInfo::getObjectShadows() const { return objectShadows != 0; }
void  LightInfo::setViewDistance(float value) { viewDistance = value; }
float LightInfo::getViewDistance() const { return viewDistance; }
void  LightInfo::setSoftness(float value) { softness = value; }
float LightInfo::getSoftness() const { return softness; }
void  LightInfo::setMaxOccluders(int value) { maxOccluders = value; }
int   LightInfo::getMaxOccluders() const { return maxOccluders; }


// A zero position, the identity transformation, a zero box; object and geometry stay
// unset (fillObjectRecord writes them).
OccluderRecord::OccluderRecord()
{
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    for (int i = 0; i < 16; ++i)
        transform.m[i] = 0.0f;
    transform.m[15] = 1.0f;
    transform.m[10] = 1.0f;
    transform.m[5] = 1.0f;
    transform.m[0] = 1.0f;
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    radius = 0.0f;
    for (int i = 0; i < 24; ++i)
        corners[i] = 0.0f;
}


// An empty dirty box, zero corners and sphere, no children, no records.
OccluderNode::OccluderNode()
    : children(0)
{
    bounds.min.x = 0.0f; bounds.min.y = 0.0f; bounds.min.z = 0.0f;
    bounds.max.x = 0.0f; bounds.max.y = 0.0f; bounds.max.z = 0.0f;
    bounds.cornerLength = 0.0f;
    bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.centre.x = 0.0f; bounds.centre.y = 0.0f; bounds.centre.z = 0.0f;
    for (int i = 0; i < 24; ++i)
        corners[i] = 0.0f;
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    radius = 0.0f;
}

// The four children (array form: the count word ahead of them, destroyed last to
// first, freed with the word), then the record list.
OccluderNode::~OccluderNode()
{
    if (children)
    {
        unsigned int* const cookie = reinterpret_cast<unsigned int*>(children) - 1;
        for (unsigned int i = *cookie; i-- > 0; )
            children[i].~OccluderNode();
        ::renddx9abi::engineFree(cookie);
    }
}


LightmapGeneration::LightmapGeneration()
    : m_refCount(1),
      m_occluderTree(0),
      m_resolutionModifier(1.0f),
      m_renderTerrain(1),
      m_shadowsNearCamera(-1.0f),
      m_onlyNearest(0),
      m_firstBlock(1),
      m_blockSize(16),
      m_sampleOffset(0.02f),
      m_noPhongAngle(0),
      m_generationMethod(6),
      m_phongAngleMode(0),
      m_globalSoftness(0.0f),
      m_directlyLightedThreshold(0.01f),
      m_cameraNearPlane(0.001f),
      m_font(0),
      m_targetTexture(0),
      m_targetTexture2(0),
      m_sysMemTargetTexture(0),
      m_sysMemTargetTexture2(0),
      m_depthStencil(0),
      m_lightMesh(0),
      m_lightEffect(0),
      m_unresolvedA8(0),
      m_quadEffect(0),
      m_lightMeshVertices(0),
      m_lightMeshIndices(0),
      m_lightMeshFaces(0),
      m_lightMeshVertexCount(0),
      m_quadVertexBuffer(0),
      m_progressCallback(0),
      m_progressUser(0),
      m_numberOfGILights(0),
      m_giLightsIntensity(-1.0f),
      m_giLightsShadowIntensity(-1.0f),
      m_giLightsFov(-1.0f),
      m_giLightsViewDistance(50.0f),
      m_useGITopLight(0),
      m_giTopLightIntensity(0.1f),
      m_sunLightIntensity(-1.0f),
      m_sunLightShadowIntensity(-1.0f),
      m_sunLightFov(8.0f),
      m_enableIntensityLightmaps(1),
      m_enableBackfaceCulling(1),
      m_lightmapFormat(1),
      m_skyWhite(0.0f),
      m_logPath(""),
      m_waterSunIntensity(0.8f)
{
    m_skyColor.x = 0.3f; m_skyColor.y = 0.3f; m_skyColor.z = 0.3f;
    m_ambientColor.x = 0.3f; m_ambientColor.y = 0.3f; m_ambientColor.z = 0.3f;
    m_giLightColor.x = 0.3f; m_giLightColor.y = 0.3f; m_giLightColor.z = 0.3f;
    m_giLightsBaseDir.x = 0.0f; m_giLightsBaseDir.y = 0.5f; m_giLightsBaseDir.z = -0.5f;

    m_font = createFontPortedDirect();
    fontPortedLoad(m_font, vc71::string("Fonts/BF1942.font"));                // Font::Ported +0x10

    // Two render targets in the default pool, then two system-memory read-back copies.
    const int savedManaged = dx9rend::textureManagerSwapManaged(0);
    m_targetTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        kTargetSize, kTargetSize, kFormatA8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    m_targetTexture2 = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        kTargetSize, kTargetSize, kFormatA8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    dx9rend::textureManagerSwapManaged(savedManaged);
    const int savedSystem = dx9rend::textureManagerSwapManaged(2);
    m_sysMemTargetTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        kTargetSize, kTargetSize, kFormatA8R8G8B8, 1, 0, 0));
    m_sysMemTargetTexture2 = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        kTargetSize, kTargetSize, kFormatA8R8G8B8, 1, 0, 0));
    dx9rend::textureManagerSwapManaged(savedSystem);
    renderDevice()->CreateDepthStencilSurface(kTargetSize, kTargetSize,
                                              static_cast<D3DFORMAT>(kFormatD24S8),
                                              D3DMULTISAMPLE_NONE, 0, TRUE, &m_depthStencil, 0);

    // The image runs its light/tree reset here too; both are still empty.
    m_lights.clear();
    if (m_occluderTree)
    {
        m_occluderTree->~OccluderNode();
        ::renddx9abi::engineFree(m_occluderTree);
        m_occluderTree = 0;
    }

    // The light marker: a 0.1 m sphere, cloned to a position-only declaration (D3DXMESH_MANAGED).
    D3DXCreateSphere(renderDevice(), 0.1f, 20, 20, reinterpret_cast<ID3DXMesh**>(&m_lightMesh), 0);
    ID3DXMesh* clone = 0;
    static_cast<ID3DXMesh*>(m_lightMesh)->CloneMesh(0x220, kLightMeshDeclaration,
                                                     renderDevice(), &clone);
    m_lightMesh->Release();
    m_lightMesh = clone;
    ID3DXMesh* const mesh = static_cast<ID3DXMesh*>(m_lightMesh);
    mesh->GetVertexBuffer(&m_lightMeshVertices);
    mesh->GetIndexBuffer(&m_lightMeshIndices);
    m_lightMeshVertexCount = mesh->GetNumVertices();
    m_lightMeshFaces = mesh->GetNumFaces();

    m_lightEffect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "shaders/LightGeom"));
    dx9rend::effectSetTechniqueByName(m_lightEffect, vc71::string("Pointlight"));

    // A two-triangle screen quad of Vec2 positions (written in the default pool).
    m_quadVertexCount = 6;
    m_quadStride = 8;
    const int savedPool = dx9rend::bufferManagerSwapPool(0);
    m_quadBytes = m_quadVertexCount * m_quadStride;
    m_quadVertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, m_quadVertexCount,
                                                     m_quadStride, vc71::string("LightmapGeneration"),
                                                     D3DUSAGE_WRITEONLY);
    dx9rend::bufferManagerSwapPool(savedPool);
    m_quadEffect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "shaders/QuadGeom"));
    dx9rend::effectSetTechniqueByName(m_quadEffect, vc71::string("TexturedQuad"));
    float* const quad = static_cast<float*>(dx9rend::vertexBufferLock(m_quadVertexBuffer, 0, 6, 0));
    if (!quad)
        ::renddx9abi::logf("LightmapGeneration.cpp:746: Failed to lock vertex buffer");
    // Written whether or not the lock succeeded, as shipped.
    quad[0] = -1.0f; quad[1] = 1.0f;
    quad[2] = 1.0f;  quad[3] = 1.0f;
    quad[4] = -1.0f; quad[5] = -1.0f;
    quad[6] = -1.0f; quad[7] = -1.0f;
    quad[8] = 1.0f;  quad[9] = 1.0f;
    quad[10] = 1.0f; quad[11] = -1.0f;
    dx9rend::vertexBufferUnlock(m_quadVertexBuffer);
}

// The two effects keep the references the shader manager handed out (shipped); the
// tree is destroyed but its pointer left set.
LightmapGeneration::~LightmapGeneration()
{
    if (m_occluderTree)
    {
        m_occluderTree->~OccluderNode();
        ::renddx9abi::engineFree(m_occluderTree);
    }
    if (m_font)
        m_font->release();
    dx9rend::EngineTexture* const textures[4] =
        { m_targetTexture, m_targetTexture2, m_sysMemTargetTexture, m_sysMemTargetTexture2 };
    for (int i = 0; i < 4; ++i)
    {
        dx9rend::EngineTexture* const texture = textures[i];
        if (texture && --texture->refCount == 0)
        {
            dx9rend::textureDtor(texture);
            ::renddx9abi::engineFree(texture);
        }
    }
    if (m_depthStencil)
        m_depthStencil->Release();
    if (m_lightMeshVertices)
        m_lightMeshVertices->Release();
    if (m_lightMeshIndices)
        m_lightMeshIndices->Release();
    if (m_lightMesh)
        m_lightMesh->Release();
    if (m_unresolvedA8)
        m_unresolvedA8->Release();
    if (m_quadVertexBuffer)
        dx9rend::vertexBufferRelease(m_quadVertexBuffer);
    // m_logPath, m_lights, m_occluderList and m_occluders follow as members, in that order.
}

void LightmapGeneration::addRef() { ++m_refCount; }
int  LightmapGeneration::getRef() { return m_refCount; }

// Answers the count after the decrement; zero destroys and frees.
int LightmapGeneration::release()
{
    const int count = --m_refCount;
    if (count == 0)
    {
        this->~LightmapGeneration();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// IBase (1) and IID_ILightmapGeneration answer the object.
void* LightmapGeneration::queryInterface(unsigned int iid) const
{
    LightmapGeneration* const self = const_cast<LightmapGeneration*>(this);
    if (iid == 1u)
        return self;
    return iid == kIIDLightmapGeneration ? self : 0;
}

void LightmapGeneration::setProgressCallback(LightmapProgressCallback callback, void* user)
{
    m_progressCallback = callback;
    m_progressUser = user;
}

void  LightmapGeneration::setOnlyNearest(bool value) { m_onlyNearest = value; }
bool  LightmapGeneration::getOnlyNearest() const { return m_onlyNearest != 0; }
void  LightmapGeneration::setFirstBlock(int value) { m_firstBlock = value; }
int   LightmapGeneration::getFirstBlock() const { return m_firstBlock; }
void  LightmapGeneration::setBlockSize(int value) { m_blockSize = value; }
int   LightmapGeneration::getBlockSize() const { return m_blockSize; }
void  LightmapGeneration::setSampleOffset(float value) { m_sampleOffset = value; }
float LightmapGeneration::getSampleOffset() const { return m_sampleOffset; }
void  LightmapGeneration::setShadowsNearCamera(float value) { m_shadowsNearCamera = value; }
float LightmapGeneration::getShadowsNearCamera() const { return m_shadowsNearCamera; }

// Store, then clamp each component to [0, 1].
void LightmapGeneration::setSkyColor(const Vec3& value)
{
    m_skyColor = value;
    m_skyColor.x = clampUnit(m_skyColor.x);
    m_skyColor.y = clampUnit(m_skyColor.y);
    m_skyColor.z = clampUnit(m_skyColor.z);
}
void LightmapGeneration::setAmbientColor(const Vec3& value)
{
    m_ambientColor = value;
    m_ambientColor.x = clampUnit(m_ambientColor.x);
    m_ambientColor.y = clampUnit(m_ambientColor.y);
    m_ambientColor.z = clampUnit(m_ambientColor.z);
}
void LightmapGeneration::setGILightColor(const Vec3& value)
{
    m_giLightColor = value;
    m_giLightColor.x = clampUnit(m_giLightColor.x);
    m_giLightColor.y = clampUnit(m_giLightColor.y);
    m_giLightColor.z = clampUnit(m_giLightColor.z);
}

void LightmapGeneration::pushDefaultLight()
{
    m_lights.push_back(LightInfo());
}


// Nothing while a generator exists; otherwise allocate and construct one (a failed
// allocation publishes null) and register it as the simple singleton "LightmapGeneration".
void createLightmapGenerationSingleton()
{
    if (g_LightmapGeneration)
        return;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(LightmapGeneration));
    g_LightmapGeneration = storage ? new (storage) LightmapGeneration() : 0;
    ::dice::hfe::classManager->registerSimpleSingleton(
        ::renddx9abi::HostString("LightmapGeneration"), g_LightmapGeneration);  // ClassManager +0x50
}

// The registration is left in place, as shipped.
void destroyLightmapGenerationSingleton()
{
    if (LightmapGeneration* const generator = g_LightmapGeneration)
    {
        generator->~LightmapGeneration();
        ::renddx9abi::engineFree(generator);
    }
    g_LightmapGeneration = 0;
}


namespace {

// vector<OccluderRecord*>::operator=. [CHOSEN, not recovered] storage handling:
// an empty source frees the target, otherwise the target is refilled in order.
void assignRecords(Vc71Vec<OccluderRecord*>& to, const Vc71Vec<OccluderRecord*>& from)
{
    if (&to == &from)
        return;
    if (from.size() == 0)
    {
        to.clear();
        return;
    }
    to.resize(0);
    for (unsigned int i = 0; i < from.size(); ++i)
        to.push_back(from[i]);
}

// ObjectManager +0xB4, getAllRegisteredObjects - copied into a TU-local map in key order (the
// shipped traceAllObjects copy-constructs it). The retail flavour walks the host's VC7.1 tree
// (OverGrowthLoad.cpp's HostObjectWalk over +0x34 is the same walk).
#if !BF2_PLUGIN_ABI_NATIVE
struct HostObjectNode
{
    HostObjectNode* left;      // +0x00
    HostObjectNode* parent;    // +0x04
    HostObjectNode* right;     // +0x08
    unsigned int    key;       // +0x0C
    world::IObject* object;    // +0x10
    unsigned char   color;     // +0x14
    unsigned char   isNil;     // +0x15
};
struct HostObjectMap
{
    void*           comp;      // +0x00
    HostObjectNode* head;      // +0x04
    unsigned int    size;      // +0x08
};

HostObjectNode* nextNode(HostObjectNode* node)
{
    if (node->isNil)
        return node;
    HostObjectNode* right = node->right;
    if (!right->isNil)
    {
        while (!right->left->isNil)
            right = right->left;
        return right;
    }
    HostObjectNode* parent = node->parent;
    while (!parent->isNil && node == parent->right)
    {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}
#endif

void copyRegisteredObjects(std::map<unsigned int, world::IObject*>& out)
{
#if BF2_PLUGIN_ABI_NATIVE
    out = hostObjectManager()->getAllRegisteredObjects();
#else
    typedef HostObjectMap* (__thiscall* GetAllFn)(void*);
    void* const manager = ::dice::hfe::objectManager;
    HostObjectMap* const map =
        reinterpret_cast<GetAllFn>((*static_cast<void***>(manager))[0xB4 / 4])(manager);
    for (HostObjectNode* node = map->head->left; node != map->head; node = nextNode(node))
        out.insert(out.end(), std::make_pair(node->key, node->object));
#endif
}

// The registered objects one by one, in key order, without a copy (rebuildOccluders' walk).
class RegisteredObjectWalk
{
public:
#if BF2_PLUGIN_ABI_NATIVE
    RegisteredObjectWalk()
        : m_map(hostObjectManager()->getAllRegisteredObjects()), m_it(m_map.begin()) {}
    bool done() const { return m_it == m_map.end(); }
    world::IObject* object() const { return m_it->second; }
    void next() { ++m_it; }
private:
    const std::map<unsigned int, world::IObject*>& m_map;
    std::map<unsigned int, world::IObject*>::const_iterator m_it;
#else
    RegisteredObjectWalk()
    {
        typedef HostObjectMap* (__thiscall* GetAllFn)(void*);
        void* const manager = ::dice::hfe::objectManager;
        m_map = reinterpret_cast<GetAllFn>((*static_cast<void***>(manager))[0xB4 / 4])(manager);
        m_node = m_map->head->left;
    }
    bool done() const { return m_node == m_map->head; }
    world::IObject* object() const { return m_node->object; }
    void next() { m_node = nextNode(m_node); }
private:
    HostObjectMap*  m_map;
    HostObjectNode* m_node;
#endif
    RegisteredObjectWalk(const RegisteredObjectWalk&);
    RegisteredObjectWalk& operator=(const RegisteredObjectWalk&);
};

} // anonymous namespace


// The node's corners and sphere are refreshed from its box. Over 16 records and under depth 11 it
// gets four children (new[4]) quartering x and z - the y half extent is multiplied by 0, and each
// child then takes the node's own y range - and each child splits the records whose sphere
// touches its box (aabbSphereOverlap). Otherwise the node keeps the records.
void LightmapGeneration::splitOccluderNode(const Vc71Vec<OccluderRecord*>& records,
                                           OccluderNode* node, int depth)
{
    aabbCorners(node->corners, &node->bounds.min.x);
    node->centre = boxCentre(node->bounds);
    node->radius = boxRadius(node->bounds);
    if (!records.data() || records.size() <= 16u || depth >= 11)
    {
        assignRecords(node->records, records);
        return;
    }

    if (node->bounds.dirty)
        node->bounds.refresh();
    const float halfZ = node->bounds.extent.z * 0.5f;
    const float halfY = node->bounds.extent.y * 0.0f;
    const float halfX = node->bounds.extent.x * 0.5f;
    void* const block = ::renddx9abi::engineAlloc(4 + 4 * sizeof(OccluderNode));
    OccluderNode* children = 0;
    if (block)
    {
        *static_cast<unsigned int*>(block) = 4;
        children = reinterpret_cast<OccluderNode*>(static_cast<unsigned int*>(block) + 1);
        for (int i = 0; i < 4; ++i)
            new (&children[i]) OccluderNode();                     // `eh vector constructor iterator`
    }
    node->children = children;

    const Vec3& min = node->bounds.min;
    const float zeroX = halfX * 0.0f;
    const float zeroY = halfY * 0.0f;
    const float zeroZ = halfZ * 0.0f;
    const float offsetX[4] = { zeroX, halfX, zeroX, halfX };
    const float offsetZ[4] = { zeroZ, zeroZ, halfZ, halfZ };
    for (int i = 0; i < 4; ++i)
    {
        TerrainAabb& box = node->children[i].bounds;
        box.min.x = offsetX[i] + min.x;
        box.min.y = zeroY + min.y;
        box.min.z = offsetZ[i] + min.z;
        box.max.x = halfX + box.min.x;
        box.max.y = halfY + box.min.y;
        box.max.z = halfZ + box.min.z;
    }
    for (int i = 0; i < 4; ++i)
    {
        node->children[i].bounds.min.y = node->bounds.min.y;
        node->children[i].bounds.max.y = node->bounds.max.y;
    }
    for (int i = 0; i < 4; ++i)
    {
        Vc71Vec<OccluderRecord*> inside;
        for (unsigned int r = 0; r < records.size(); ++r)
        {
            OccluderRecord* const record = records[r];
            if (aabbSphereOverlap(node->children[i].bounds, record->centre, record->radius))
                inside.push_back(record);
        }
        splitOccluderNode(inside, &node->children[i], depth + 1);
    }
}

// A node's own records are taken unconditionally; a child is entered when the frustum holds its
// sphere (Frustum::inside) and its corners (cornersInsideFrustum). The fourth argument is passed
// on as 0 and never read.
void LightmapGeneration::queryOccluderNode(const Frustum& frustum, OccluderNode* node,
                                           Vc71Vec<OccluderRecord*>* out, int unused)
{
    (void)unused;
    for (unsigned int i = 0; i < node->records.size(); ++i)
        out->push_back(node->records[i]);
    if (!node->children)
        return;
    for (int i = 0; i < 4; ++i)
    {
        OccluderNode* const child = &node->children[i];
        if (frustum.inside(child->centre, child->radius) && cornersInsideFrustum(child->corners, frustum))
            queryOccluderNode(frustum, child, out, 0);
    }
}

// The tree's candidates land in a function-local static list (freed at exit) that is emptied,
// not freed, on every call; each candidate whose object `out` does not hold yet is appended when
// the frustum holds its sphere and its corners.
void LightmapGeneration::occludersInFrustum(const Frustum& frustum, Vc71Vec<OccluderRecord*>* out)
{
    static Vc71Vec<OccluderRecord*> s_candidates;
    s_candidates.resize(0);
    queryOccluderNode(frustum, m_occluderTree, &s_candidates, 0);
    for (unsigned int i = 0; i < s_candidates.size(); ++i)
    {
        OccluderRecord* const candidate = s_candidates[i];
        bool listed = false;
        for (unsigned int j = 0; j < out->size(); ++j)
        {
            if (candidate->object == (*out)[j]->object)
            {
                listed = true;
                break;
            }
        }
        if (listed)
            continue;
        if (frustum.inside(candidate->centre, candidate->radius)
            && cornersInsideFrustum(candidate->corners, frustum))
            out->push_back(candidate);
    }
}

// Every record goes into a TU-local ordered map keyed by its squared distance to
// `position` less its squared radius (floored at 0); the map is then destroyed and nothing is
// written back, so the occluder list keeps its full length (shipped).
void LightmapGeneration::sortOccluders(Vc71Vec<OccluderRecord*>* records, const Vec3& position,
                                       int maxCount)
{
    (void)maxCount;
    std::multimap<float, OccluderRecord*> sorted;
    for (unsigned int i = 0; i < records->size(); ++i)
    {
        OccluderRecord* const record = (*records)[i];
        const float dx = record->position.x - position.x;
        const float dy = record->position.y - position.y;
        const float dz = record->position.z - position.z;
        float distance = (dz * dz + dy * dy + dx * dx) - record->radius * record->radius;
        if (distance < 0.0f)
            distance = 0.0f;
        sorted.insert(std::make_pair(distance, record));
    }
}

// The root is replaced (a failed allocation publishes null); with records, its box
// starts at +-9999999 and grows over every record sphere's two extreme corners, then the root is
// split over m_occluderList (the argument only feeds the box).
void LightmapGeneration::buildOccluderTree(Vc71Vec<OccluderRecord*>* records)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(OccluderNode));
    m_occluderTree = storage ? new (storage) OccluderNode() : 0;
    if (!records->data() || records->size() == 0)
        return;
    OccluderNode* const root = m_occluderTree;
    root->bounds.min.x = 9999999.0f;
    root->bounds.min.y = 9999999.0f;
    root->bounds.min.z = 9999999.0f;
    root->bounds.max.x = -9999999.0f;
    root->bounds.max.y = -9999999.0f;
    root->bounds.max.z = -9999999.0f;
    root->bounds.dirty = 1;
    for (unsigned int i = 0; i < records->size(); ++i)
    {
        const OccluderRecord* const record = (*records)[i];
        const float radius = record->radius;
        const Vec3 low = { record->centre.x - radius, record->centre.y - radius,
                           record->centre.z - radius };
        expandBox(m_occluderTree->bounds, low);
        const Vec3 high = { radius + record->centre.x, radius + record->centre.y,
                            radius + record->centre.z };
        expandBox(m_occluderTree->bounds, high);
    }
    splitOccluderNode(m_occluderList, m_occluderTree, 0);
}

void LightmapGeneration::addOccluder(world::IObject* object, Vc71Vec<OccluderRecord>* records)
{
    if (!object)
        return;
    const unsigned int flags = object->getFlags();
    if (!(flags & world::IObject::OFIsAlive) || (flags & world::IObject::OFHasMobilePhysics))
        return;
    world::IObject* const root = static_cast<world::IObject*>(staticMeshRootParent(object));
    if (root && (root->getFlags() & world::IObject::OFHasMobilePhysics))
        return;
    OccluderRecord record;
    if (fillObjectRecord(object, &record))
        records->push_back(record);
}

// Roads and meshes flagged doNotGenerateLightmaps cast no shadow; the previous tree is
// not released (the trace only rebuilds a missing one).
void LightmapGeneration::rebuildOccluders()
{
    m_occluders.clear();
    m_occluderList.clear();
    for (RegisteredObjectWalk walk; !walk.done(); walk.next())
    {
        world::IObject* const object = walk.object();
        if (!object)
            continue;
        void* const geometry = world::object_detail::LayoutAccess::geometry(object);
        if (!geometry)
            continue;
        IGeometryTemplate* const tmpl = geometryGetTemplate(geometry);
        if (tmpl->getClassId() == kStaticMeshTemplateClassId)
        {
            if (static_cast<StaticMeshTemplate*>(tmpl)->m_isRoad)
                continue;
            if (tmpl->getDoNotGenerateLightmaps())                          // +0x64
                continue;
        }
        addOccluder(object, &m_occluders);
    }
    for (unsigned int i = 0; i < m_occluders.size(); ++i)
        m_occluderList.push_back(&m_occluders[i]);
    buildOccluderTree(&m_occluderList);
}


// Scene lights from the light manager (getLightsByMask 0x1F15): enabled lights whose
// root is enabled. A directional light becomes a reversed-direction view of 500 m (the template
// called "sunlight" takes the sun overrides that are not -1); point and spot lights touching
// `bounds` (its y range widened to -9999..9999) become local lights. Then the GI lights: a top light
// for one GI light or useGITopLight, and a ring of numberOfGILights around the sky light's azimuth.
// The scene lights are appended through the published generator's last light (lastLight).
void LightmapGeneration::gatherLights(TerrainAabb* bounds)
{
    vc71::vector_layout lights;
    lights.first = 0;
    lights.last = 0;
    lights.end = 0;
    g_LightManager->getLightsByMask(lights, 0x1F15u, 0u);                  // +0x14
    for (world::LightSource** it = reinterpret_cast<world::LightSource**>(lights.first);
         it != reinterpret_cast<world::LightSource**>(lights.last); ++it)
    {
        world::LightSource* const light = *it;
        const world::LightSourceTemplate* const tmpl = lightSourceTemplatePointer(light);
        if (lightSourceFlags(light) & world::IObject::OFIsDisabled)
            continue;
        if (staticMeshRootParent(light)
            && (static_cast<world::IObject*>(staticMeshRootParent(light))->getFlags()
                & world::IObject::OFIsDisabled))
            continue;
        if (tmpl->getLightTypeEnum() == 2 || tmpl->getLightTypeEnum() == 1)
        {
            if (bounds)
            {
                TerrainAabb box = *bounds;
                box.min.y = -9999.0f;
                box.max.y = 9999.0f;
                const float range = light->getAttenuationRange2();
                if (!aabbSphereOverlap(box, light->getAbsolutePosition(), range))
                    continue;
            }
            m_lights.push_back(LightInfo());
            LightInfo* const info = lastLight();
            const Vec3& colour = light->getColor();
            info->isLocal = 1;
            info->source = light;
            info->colorRed = static_cast<int>(static_cast<double>(colour.x) + 0.5);
            const Mat4& transform = light->getAbsoluteTransformation();
            info->position.x = transform.m[12];
            info->position.y = transform.m[13];
            info->position.z = transform.m[14];
            info->radius = light->getAttenuationRange2();
            info->selfShadowing = 1;
            info->fov = light->getFov();
            info->objectShadows = tmpl->getObjectShadows();
            info->shadowIntensity = tmpl->getShadowIntensity();
            info->intensity = tmpl->getIntensity();
            info->viewDistance = 500.0f;
            info->spotSource = light;
            info->softness = tmpl->getSoftness();
        }
        else if (tmpl->getLightTypeEnum() == 0)
        {
            pushDefaultLight();
            LightInfo* const info = lastLight();
            info->source = light;
            const Vec3& direction = light->getDirection();
            const float x = direction.x * -1.0f;
            const float y = direction.y * -1.0f;
            const float z = direction.z * -1.0f;
            info->direction.z = z;
            info->direction.x = x;
            info->direction.y = y;
            info->selfShadowing = 1;
            info->fov = light->getFov();
            info->objectShadows = tmpl->getObjectShadows();
            info->shadowIntensity = tmpl->getShadowIntensity();
            info->intensity = tmpl->getIntensity();
            info->viewDistance = 500.0f;
            info->softness = tmpl->getSoftness();
            const vc71::string sunlight("sunlight");
            const bool isSun = _stricmp(fromHostStdString(tmpl->getName()).c_str(),
                                        sunlight.c_str()) == 0;
            if (isSun)
            {
                info->viewDistance = 500.0f;
                if (!isUnset(m_sunLightFov))
                    info->fov = m_sunLightFov;
                if (!isUnset(m_sunLightIntensity))
                    info->intensity = m_sunLightIntensity;
                if (!isUnset(m_sunLightShadowIntensity))
                    info->shadowIntensity = m_sunLightShadowIntensity;
            }
        }
    }

    if (m_numberOfGILights != 0)
    {
        if (m_numberOfGILights == 1 || m_useGITopLight)
        {
            m_lights.push_back(LightInfo());
            LightInfo* const top = lastLight();
            top->direction.x = 0.0f;
            top->direction.y = 1.0f;
            top->direction.z = 0.0f;
            top->selfShadowing = 1;
            top->fov = isUnset(m_giLightsFov) ? 100.0f : m_giLightsFov;
            top->intensity = m_giTopLightIntensity;
            if (!isUnset(m_giLightsShadowIntensity))
                top->shadowIntensity = m_giLightsShadowIntensity;
            top->shadowIntensity = 1.0f;                // overwrites the value just stored (shipped)
            top->objectShadows = 1;
            top->viewDistance = m_giLightsViewDistance;
            top->softness = 0.0f;
            top->isGI = 1;
        }
        if (m_numberOfGILights > 1)
        {
            const float step = 360.0f / static_cast<float>(m_numberOfGILights + 1);
            const float start = std::fmod(sunAzimuth() * 57.295776f + step, 360.0f);
            for (unsigned int i = 0; i < m_numberOfGILights; ++i)
            {
                m_lights.push_back(LightInfo());
                LightInfo* const gi = lastLight();
                const float angle =
                    std::fmod(static_cast<float>(i) * step + start, 360.0f) * 0.017453292f;
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                gi->direction.x = c * m_giLightsBaseDir.x - s * m_giLightsBaseDir.z;
                gi->direction.y = m_giLightsBaseDir.y;
                gi->direction.z = s * m_giLightsBaseDir.x + c * m_giLightsBaseDir.z;
                gi->selfShadowing = 1;
                gi->fov = isUnset(m_giLightsFov) ? 100.0f : m_giLightsFov;
                gi->intensity = isUnset(m_giLightsIntensity) ? 1.0f : m_giLightsIntensity;
                gi->shadowIntensity = isUnset(m_giLightsShadowIntensity) ? 1.0f
                                                                         : m_giLightsShadowIntensity;
                gi->objectShadows = 1;
                gi->viewDistance = m_giLightsViewDistance;
                gi->softness = 0.0f;
                gi->isGI = 1;
            }
        }
    }
    if (lights.first)
        ::renddx9abi::engineFree(lights.first);
}


// Without a log path every record is traced. Before a record (`starting`) the log is
// scanned for its line (template name and truncated position): "done!" answers false (skip) unless
// an ASSERT line came since the last status line; "failed!" or a pending ASSERT reopen it for
// tracing - and close the scan's file twice (shipped). Then "Rendering object <name>" or the
// record's done!/failed! line is appended; the append's fopen is not checked (shipped).
bool LightmapGeneration::updateResumeLog(OccluderRecord* record, bool succeeded, bool starting)
{
    if (m_logPath.empty())
        return true;
    vc71::string name("");
    if (record && record->object)
        name = fromHostStdString(objectTemplatePointer(record->object)->getName()).c_str();  // +0x28

    if (starting)
    {
        FILE* const scan = std::fopen(m_logPath.c_str(), "rb");
        if (scan)
        {
            bool asserted = false;
            // [CHOSEN, not recovered] the image leaves the three numbers unset before the first
            // line; zero stands in for that indeterminate value.
            int x = 0;
            int y = 0;
            int z = 0;
            char line[256];
            while (std::fgets(line, 256, scan))
            {
                if (std::strncmp(line, "ASSERT", 6) == 0)
                    asserted = true;
                char logged[128];
                char status[128];
                logged[0] = 0;
                status[0] = 0;
                std::sscanf(line, "\"%[^\"]\" %d %d %d %s", logged, &x, &y, &z, status);
                if (x == static_cast<int>(record->position.x)
                    && y == static_cast<int>(record->position.y)
                    && z == static_cast<int>(record->position.z)
                    && std::strcmp(logged, name.c_str()) == 0)
                {
                    if (std::strcmp(status, "failed!") == 0)
                    {
                        std::fclose(scan);
                        break;
                    }
                    if (std::strcmp(status, "done!") == 0)
                    {
                        if (asserted)
                        {
                            std::fclose(scan);
                            break;
                        }
                        std::fclose(scan);
                        return false;
                    }
                }
                if (std::strlen(status) != 0)
                    asserted = false;
            }
            std::fclose(scan);                  // a second close after the two breaks (shipped)
        }
    }

    FILE* const log = std::fopen(m_logPath.c_str(), "ab");
    std::fseek(log, 0, SEEK_END);
    if (starting)
        std::fprintf(log, "Rendering object %s\n", name.c_str());
    else if (succeeded)
        std::fprintf(log, "\"%s\" %d %d %d done!\n", name.c_str(),
                     static_cast<int>(record->position.x), static_cast<int>(record->position.y),
                     static_cast<int>(record->position.z));
    else
        std::fprintf(log, "\"%s\" %d %d %d failed!\n", name.c_str(),
                     static_cast<int>(record->position.x), static_cast<int>(record->position.y),
                     static_cast<int>(record->position.z));
    std::fclose(log);
    return true;
}


bool LightmapGeneration::traceRecord(OccluderRecord* record, int objectIndex, int objectCount,
                                     int block)
{
    if (record)
    {
        IGeometryTemplate* const tmpl = geometryGetTemplate(record->geometry);
        if (tmpl->getClassId() == kStaticMeshTemplateClassId)
        {
            const StaticMeshTemplate* const mesh = static_cast<const StaticMeshTemplate*>(tmpl);
            if (mesh->m_isRoad)
                return true;
            if (mesh->m_isVegetation)
                return true;
        }
    }
    if (m_enableIntensityLightmaps)
        return traceIntensity(record, objectIndex, objectCount, block);
    return traceColour(record, objectIndex, objectCount, block);
}

bool LightmapGeneration::traceTerrain(int patch)
{
    if (m_enableIntensityLightmaps)
        return traceIntensity(0, -1, -1, patch);
    return traceColour(0, -1, -1, patch);
}

// Geoms (+0x74 count, +0x6C select) and their lods (+0x80 count, +0x78 select) are
// walked from the last; a lightmapOverride template skips the (geom, lod) pairs whose lightmap
// size is 0.
bool LightmapGeneration::traceObjectRecord(OccluderRecord* record, int objectIndex,
                                           int objectCount, int block)
{
    void* const geometry = world::object_detail::LayoutAccess::geometry(record->object);
    int geom = geometryGetGeomCount(geometry);
    IGeometryTemplate* const tmpl = geometryGetTemplate(geometry);
    if (!updateResumeLog(record, true, true))
        return true;
    for (--geom; geom >= 0; --geom)
    {
        geometrySetGeom(geometry, geom);
        for (int lod = geometryGetLodCount(geometry) - 1; lod >= 0; --lod)
        {
            geometrySetLod(geometry, lod);
            if (tmpl && tmpl->getLightmapOverride() && tmpl->getLightmapSize(geom, lod) == 0)
                continue;
            if (!traceRecord(record, objectIndex, objectCount, block))
            {
                updateResumeLog(record, false, false);
                return false;
            }
        }
    }
    updateResumeLog(record, true, false);
    return true;
}

// The registered objects are copied, the ones without a sampleable geometry removed,
// and the rest traced in key order with their index and the remaining count; a callback abort
// stops after the current object. Always true.
bool LightmapGeneration::traceAllObjects()
{
    g_traceAborted = false;
    std::map<unsigned int, world::IObject*> objects;
    copyRegisteredObjects(objects);
    OccluderRecord record;
    for (std::map<unsigned int, world::IObject*>::iterator it = objects.begin(); it != objects.end(); )
    {
        if (!fillObjectRecord(it->second, &record))
        {
            it = objects.erase(it);
            continue;
        }
        IBase* const geometry =
            static_cast<IBase*>(world::object_detail::LayoutAccess::geometry(record.object));
        if (!geometry || !geometry->queryInterface(kIIDLightSampleable))
        {
            it = objects.erase(it);
            continue;
        }
        ILightSampleable* const sampleable =
            static_cast<ILightSampleable*>(geometry->queryInterface(kIIDLightSampleable));
        if (!sampleable || !sampleable->isLightSampleable())
        {
            it = objects.erase(it);
            continue;
        }
        ++it;
    }
    g_traceObjectIndex = 0;
    g_traceObjectCount = static_cast<int>(objects.size());
    for (std::map<unsigned int, world::IObject*>::iterator it = objects.begin();
         it != objects.end(); ++it)
    {
        if (fillObjectRecord(it->second, &record))
            traceObjectRecord(&record, g_traceObjectIndex, g_traceObjectCount, -1);
        ++g_traceObjectIndex;
        if (g_traceAborted)
            break;
    }
    return true;
}

bool LightmapGeneration::traceActiveObjectProgress(int index, int count)
{
    OccluderRecord record;
    g_traceObjectCount = 0;
    if (!fillObjectRecord(hostObjectManager()->getActiveObject(), &record))  // +0x90
        return false;
    return traceObjectRecord(&record, index, count, -1);
}

// The object the name hash finds (ObjectManager +0x24).
bool LightmapGeneration::traceObject(const LightmapGenerationString& name)
{
    OccluderRecord record;
    g_traceObjectCount = 0;
    world::IObject* const object = hostObjectManager()->getObjectFromNameHash(hostStdString(name));
    if (!fillObjectRecord(object, &record))
        return false;
    return traceObjectRecord(&record, -1, -1, -1);
}

bool LightmapGeneration::traceActiveObject()
{
    return traceActiveObjectProgress(-1, -1);
}

// The colour trace (enableIntensityLightmaps 0). `record` null traces the terrain (block -1: every
// block from m_firstBlock - 1, or the camera's block with onlyNearest); otherwise the record's
// geometry in its current (geom, lod). A missing terrain or ITerrainEditable fails it. Per batch,
// every (light, sample) pair renders the occluders seen from the sample into its tile of the
// 1024 x 1024 target, which starts at the sky colour (a GI light's tile at the GI colour); the
// target is then drawn through QuadGeom's TexturedQuad into the second target. Local lights read
// their tiles back from the second target, directional and GI lights from the first. Each sample
// starts at the ambient colour: a light with a positive intensity adds each channel's tile mean
// times (1 - ambient) and its phong/attenuation term, any other light subtracts (1 - tile mean)
// times its shadow intensity, clamped; the clamped channels are stored as the sampler's RGB.
// Unlike the intensity trace it builds no back-face bits or addLight table, skips no NaN sample
// and reads no generationMethod bits or water depth. Answers destroyLightSampler's result.
// [RETIRE-WHEN: RW-lightmap-colour-trace] The body is recovered but has never run; retire this
// marker once a colour-mode terrain trace in the editor flavour matches a retail-editor golden.
bool LightmapGeneration::traceColour(OccluderRecord* record, int objectIndex, int objectCount,
                                     int block)
{
    g_traceAborted = false;
    IBase* const terrain = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("Terrain"));                               // ClassManager +0x48
    if (!terrain)
        return false;
    ITerrainEditable* const terrainEditable =
        static_cast<ITerrainEditable*>(terrain->queryInterface(kIID_ITerrainEditable));
    if (!terrainEditable)
        return false;

    ILightSampleable* sampleable;
    if (record)
    {
        world::IObject* const object = record->object;
        object->getAbsoluteTransformation();                                // +0x80, discarded
        object->updateGeometry();                                           // +0xB4
        object->handleVisualUpdate(0.0f, 0.0f);                             // +0xA4
        IBase* const geometry =
            static_cast<IBase*>(world::object_detail::LayoutAccess::geometry(object));
        if (!geometry)
            return false;
        sampleable = static_cast<ILightSampleable*>(geometry->queryInterface(kIIDLightSampleable));
    }
    else
    {
        sampleable = static_cast<ILightSampleable*>(terrain->queryInterface(kIIDLightSampleable));
    }
    if (!sampleable)
        return false;
    if (!sampleable->isLightSampleable())
        return false;
    ILightSampling* const sampler = sampleable->createLightSampler(m_resolutionModifier);
    if (!sampler)
        return false;

    unsigned int viewportX;
    unsigned int viewportY;
    unsigned int viewportWidth;
    unsigned int viewportHeight;
    float viewportMinZ;
    float viewportMaxZ;
    getViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ, viewportMaxZ);
    if (!m_occluderTree)
        rebuildOccluders();

    // The batch arrays: three times one target's worth of samples, and their per-light results.
    const unsigned int capacity =
        0x100000u / (static_cast<unsigned int>(m_blockSize) * static_cast<unsigned int>(m_blockSize)) + 1;
    LightSample* samples = 0;
    if (void* const storage = ::renddx9abi::engineAlloc(144 * capacity))
    {
        samples = static_cast<LightSample*>(storage);
        for (unsigned int i = 0; i < 3 * capacity; ++i)
            new (&samples[i]) LightSample();
    }
    float* const results = static_cast<float*>(::renddx9abi::engineAlloc(12 * capacity));
    const unsigned int tileSize = static_cast<unsigned int>(m_blockSize);
    const unsigned int tileArea = tileSize * tileSize;
    const float pixelScale = 1.0f / (static_cast<float>(tileArea) * 255.0f);
    const unsigned int tilesPerRow = kTargetSize / tileSize;
    dx9rend::IRenderView* const savedView = renderer()->getRenderView();          // +0x48
    dx9rend::IRenderView* const savedViewDup = renderer()->getRenderViewDup();    // +0x50
    unsigned int firstBlock = static_cast<unsigned int>(m_firstBlock - 1);
    unsigned int occluderPeak = 0;
    Vc71Vec<OccluderRecord*> occluders;
    unsigned int totalSamples = 0;
    const Vec3& cameraPosition = *reinterpret_cast<const Vec3*>(&savedView->getViewMatrixPtr()->m[12]);
    if (block != -1)
    {
        firstBlock = static_cast<unsigned int>(block);
        totalSamples = sampler->getBlock(firstBlock).sampleCount;
    }
    else if (m_onlyNearest)
    {
        Vec3 relative;
        if (record)
        {
            relative.x = cameraPosition.x - record->position.x;
            relative.y = cameraPosition.y - record->position.y;
            relative.z = cameraPosition.z - record->position.z;
        }
        firstBlock = nearestBlock(record ? relative : cameraPosition, sampler);
        totalSamples = sampler->getBlock(firstBlock).sampleCount;
    }
    else
    {
        for (unsigned int b = firstBlock; b < sampler->getBlockCount(); ++b)
            totalSamples += sampler->getBlock(b).sampleCount;
    }
    unsigned int remainingTotal = totalSamples;
    const float startTime = static_cast<float>(g_system->getWallClockTime());   // System +0x18
    const bool worldSpace = !record || mat4ApproxEqual(record->transform, Mat4::getIdentity());

    IDirect3DDevice9* const device = renderDevice();
    IDirect3DSurface9* savedDepth;
    device->GetDepthStencilSurface(&savedDepth);                            // never released (shipped)
    unsigned char* const stats = static_cast<unsigned char*>(renderer()->getRenderStats());   // +0x6C
    std::memset(stats + 0x17C, 0, 0xA4);
    TerrainAabb lightBounds;
    lightBounds.min.x = 0.0f; lightBounds.min.y = 0.0f; lightBounds.min.z = 0.0f;
    lightBounds.max.x = 0.0f; lightBounds.max.y = 0.0f; lightBounds.max.z = 0.0f;
    lightBounds.cornerLength = 0.0f;
    lightBounds.radius = 0.0f;
    lightBounds.dirty = 1;
    lightBounds.extent.x = 0.0f; lightBounds.extent.y = 0.0f; lightBounds.extent.z = 0.0f;
    lightBounds.centre.x = 0.0f; lightBounds.centre.y = 0.0f; lightBounds.centre.z = 0.0f;

    if (firstBlock < sampler->getBlockCount())
    {
        do
        {
            const LightSampleBlock blockInfo = sampler->getBlock(firstBlock);
            const unsigned int persistentLights = m_lights.size();
            lightBounds = blockInfo.bounds;
            if (record)
            {
                lightBounds.min = cornersMinimum(record->corners);
                lightBounds.max = cornersMaximum(record->corners);
            }
            gatherLights(&lightBounds);

            // A render view per light: its fov, the camera near plane, its view distance, square.
            for (unsigned int l = 0; l < m_lights.size(); ++l)
            {
                LightInfo& light = m_lights[l];
                light.renderView = static_cast<dx9rend::IRenderView*>(
                    ::dice::hfe::classManager->createInstance(kCIDRenderView, kIIDRenderView, 0));   // +0x30
                light.renderView->setFov(0.017453292f * light.fov);
                light.renderView->setNearPlane(m_cameraNearPlane);
                light.renderView->setFarPlane(light.viewDistance);
                light.renderView->setAspect(1.0f);
            }

            unsigned int remainingInBlock = blockInfo.sampleCount;
            if (!m_lights.data() || m_lights.size() == 0)
                goto nextBlock;                     // no light: the next block, untrimmed (shipped)
            if (blockInfo.sampleCount != 0)
            {
                const unsigned int viewsPerTarget = tilesPerRow * tilesPerRow;
                do
                {
                    const unsigned int count = sampler->getSamples(
                        firstBlock, viewsPerTarget / m_lights.size(), samples);
                    if (count == 0 || g_traceAborted)
                        break;
                    (void)g_system->getWallClockTime();
                    device->BeginScene();
                    lightManagerSetRenderTargetTexture(0, m_targetTexture);
                    lightManagerSetDepthStencil(m_depthStencil);
                    setViewport(0, 0, kTargetSize, kTargetSize, viewportMinZ, viewportMaxZ);
                    dx9rend::stateManagerClear(7, opaqueClearColour(m_skyColor), 1.0f, 1);
                    dx9rend::rendererStatsBeginFrame();
                    std::memset(stats + 0x17C, 0, 0xA4);
                    bool rendered = false;
                    bool directionalSeen = false;
                    for (unsigned int s = 0; s < count; ++s)
                    {
                        samples[s].done = 0;
                        samples[s].directlyLit = 0;
                    }

                    unsigned int slotBase = 0;
                    for (LightInfo* light = m_lights.data(); light != m_lights.data() + m_lights.size();
                         ++light, slotBase += count)
                    {
                        if (!light->isLocal)
                        {
                            // Directional and GI lights look along their direction from the origin;
                            // straight up gets a fixed basis. The sample position is set per tile.
                            Mat4 view = Mat4::getIdentity();
                            const Vec3& d = light->direction;
                            if (std::fabs(d.x) <= FLT_EPSILON && std::fabs(d.y - 1.0f) <= FLT_EPSILON
                                && std::fabs(d.z) <= FLT_EPSILON)
                            {
                                view.m[9] = 1.0f;
                                view.m[8] = 0.0f;
                                view.m[4] = 1.0f;
                                view.m[10] = 0.0f;
                                view.m[2] = 1.0f;
                                view.m[5] = 0.0f;
                                view.m[6] = 0.0f;
                                view.m[0] = 0.0f;
                                view.m[1] = 0.0f;
                            }
                            else
                            {
                                const Vec3 up = { 0.0f, 1.0f, 0.0f };
                                lookAt(view, Vec3::getZero(), light->direction, up);
                            }
                            light->renderView->setViewMatrix(view.m);               // +0x60
                            directionalSeen = true;
                        }

                        for (unsigned int s = 0; s < count; ++s)
                        {
                            LightSample sample = samples[s];
                            const float offset = m_sampleOffset;
                            sample.position.x = sample.normal.x * offset + sample.position.x;
                            sample.position.y = sample.normal.y * offset + sample.position.y;
                            sample.position.z = sample.normal.z * offset + sample.position.z;
                            if (!worldSpace)
                            {
                                const float* const m = record->transform.m;
                                const Vec3 p = sample.position;
                                sample.position.x = ((p.z * m[8] + p.y * m[4]) + p.x * m[0]) + m[12];
                                sample.position.y = ((p.z * m[9] + p.y * m[5]) + p.x * m[1]) + m[13];
                                sample.position.z = ((p.z * m[10] + p.y * m[6]) + p.x * m[2]) + m[14];
                            }
                            if (m_shadowsNearCamera >= 0.0f)
                            {
                                const float dx = cameraPosition.x - sample.position.x;
                                const float dy = cameraPosition.y - sample.position.y;
                                const float dz = cameraPosition.z - sample.position.z;
                                const float limit = m_shadowsNearCamera;
                                if (limit * limit < (dz * dz + dy * dy) + dx * dx)
                                {
                                    // Past the shadow range the sample keeps the ambient colour.
                                    samples[s].result[0] = m_ambientColor.x;
                                    samples[s].result[1] = m_ambientColor.y;
                                    samples[s].result[2] = m_ambientColor.z;
                                    samples[s].done = 1;
                                    samples[s].directlyLit = 0;
                                    continue;
                                }
                            }
                            if (!worldSpace)
                            {
                                const float* const m = record->transform.m;
                                const Vec3 n = sample.normal;
                                sample.normal.x = (n.y * m[4] + n.z * m[8]) + n.x * m[0];
                                sample.normal.y = (n.y * m[5] + n.z * m[9]) + n.x * m[1];
                                sample.normal.z = (n.y * m[6] + n.z * m[10]) + n.x * m[2];
                            }
                            const unsigned int slot = s + slotBase;
                            const float facingLight = (sample.normal.y * light->direction.y
                                                       + sample.normal.z * light->direction.z)
                                                      + sample.normal.x * light->direction.x;
                            if (m_noPhongAngle)
                                results[slot] = facingLight > 0.1f ? 1.0f : 0.0f;
                            else
                                results[slot] = clampUnit(responseCurve(m_phongAngleMode, facingLight));

                            // The sample's tile of the target.
                            rendered = true;
                            setViewport((slot % tilesPerRow) * tileSize, (slot / tilesPerRow) * tileSize,
                                        tileSize, tileSize, viewportMinZ, viewportMaxZ);
                            Mat4 view = *light->renderView->getViewMatrixPtr();      // +0x64
                            if (light->isLocal)
                            {
                                const float dx = light->position.x - sample.position.x;
                                const float dy = light->position.y - sample.position.y;
                                const float dz = light->position.z - sample.position.z;
                                const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
                                const float t = distance / light->radius;
                                const float attenuation = clampUnit(1.0f - t * t);
                                const float inverse = 1.0f / distance;
                                const float toLightX = dx * inverse;
                                const float toLightY = dy * inverse;
                                const float toLightZ = dz * inverse;
                                if (m_noPhongAngle)
                                    results[slot] = attenuation;
                                else
                                    results[slot] = clampUnit(responseCurve(
                                        m_phongAngleMode,
                                        (toLightY * sample.normal.y + toLightZ * sample.normal.z)
                                            + toLightX * sample.normal.x)) * attenuation;
                                const Vec3 target = { toLightX + sample.position.x,
                                                      toLightY + sample.position.y,
                                                      toLightZ + sample.position.z };
                                const Vec3 up = { 0.0f, 1.0f, 0.0f };
                                lookAt(view, sample.position, target, up);
                            }
                            else
                            {
                                if (light->isGI)
                                    dx9rend::stateManagerClear(1, opaqueClearColour(m_giLightColor), 1.0f, 1);
                                view.m[12] = sample.position.x;
                                view.m[13] = sample.position.y;
                                view.m[14] = sample.position.z;
                            }
                            light->renderView->setViewMatrix(view.m);
                            renderer()->setRenderView(light->renderView);           // +0x44
                            renderer()->setRenderViewDup(light->renderView);        // +0x4C

                            if (light->objectShadows)
                            {
                                occluders.resize(0);
                                occludersInFrustum(*light->renderView->getFrustumPtr(), &occluders);
                                if (occluders.size() > static_cast<unsigned int>(light->maxOccluders))
                                    sortOccluders(&occluders, sample.position, light->maxOccluders);
                                for (unsigned int o = 0; o < occluders.size(); ++o)
                                {
                                    OccluderRecord* const occluder = occluders[o];
                                    world::IObject* const object = occluder->object;
                                    if ((record && record->geometry == occluder->geometry)
                                        || !occluder->geometry)
                                        continue;
                                    object->getAbsoluteTransformation();            // discarded
                                    object->updateGeometry();
                                    geometryDrawSilhouette(occluder->geometry, light->renderView, 0);
                                }
                                if (occluders.data() && occluders.size() > occluderPeak)
                                    occluderPeak = occluders.size();
                            }
                            if (m_renderTerrain)
                                terrainEditable->renderForLightmapGeneration();   // ITerrainEditable +0xB0
                            if (light->selfShadowing && record)
                                geometryDrawSilhouette(record->geometry, light->renderView, 0);

                            if (!light->isLocal || !light->source)
                                continue;

                            // The local light's marker: its sphere through LightGeom into the tile.
                            dx9rend::stateManagerPush();
                            Mat4 worldViewProjection = Mat4::getIdentity();
                            Mat4 worldView = Mat4::getIdentity();
                            const Mat4 inverseView = *light->renderView->getInverseViewMatrixPtr();
                            mat4Mult(worldView, light->spotSource->getAbsoluteTransformation(),
                                     inverseView);
                            const world::LightSourceTemplate* const tmpl =
                                lightSourceTemplatePointer(light->source);
                            if (tmpl->getLightTypeEnum() == 1)
                                dx9rend::effectSetTechniqueByName(m_lightEffect,
                                                                  vc71::string("Pointlight"));
                            else if (tmpl->getLightTypeEnum() == 2)
                            {
                                float coneAngle = light->source->getConeAngleRads2();   // +0x1C0
                                Mat4 basis = light->source->getAbsoluteTransformation();
                                const Mat4 copy = basis;
                                mat4Transpose(basis, copy);
                                const Vec3& direction = light->source->getAbsoluteDirection();  // +0x180
                                const float* const m = basis.m;
                                Vec3 spotDirection;
                                spotDirection.x = (m[0] * direction.x + m[8] * direction.z)
                                                  + m[4] * direction.y;
                                spotDirection.y = (m[1] * direction.x + m[9] * direction.z)
                                                  + m[5] * direction.y;
                                spotDirection.z = (m[2] * direction.x + m[10] * direction.z)
                                                  + m[6] * direction.y;
                                dx9rend::effectSetTechniqueByName(m_lightEffect,
                                                                  vc71::string("Spotlight"));
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("WorldView"))->setValue(&worldView, 0);
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("SpotDir"))->setValue(&spotDirection, 0);
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("ConeAngle"))->setValue(&coneAngle, 0);
                            }
                            // The scale replaces the WorldView diagonal (shipped).
                            worldView.m[0] = light->source->getScale().x;           // +0x1D4
                            worldView.m[5] = light->source->getScale().y;
                            worldView.m[10] = light->source->getScale().z;
                            mat4Mult(worldViewProjection, worldView,
                                     *light->renderView->getProjectionMatrixPtr());  // +0x6C
                            dx9rend::effectGetShaderParameterBorrowedTyped(
                                m_lightEffect, vc71::string("WorldViewProj"))
                                ->setValue(&worldViewProjection, 0);
                            const Vec3& colour = light->source->getColor();
                            const float lightColour[4] = { colour.x, colour.y, colour.z, 1.0f };
                            dx9rend::effectGetShaderParameterBorrowedTyped(
                                m_lightEffect, vc71::string("LightColor"))->setValue(lightColour, 0);
                            dx9rend::stateManagerSetStreamSource(0, m_lightMeshVertices, 0, 12);
                            dx9rend::stateManagerSetIndices(m_lightMeshIndices);
                            dx9rend::effectBegin(m_lightEffect);
                            dx9rend::effectBeginPass(m_lightEffect, 0);
                            dx9rend::rendererDrawIndexedPrimitive(m_lightMeshFaces, D3DPT_TRIANGLELIST,
                                                                  0, 0, m_lightMeshVertexCount, 0);
                            dx9rend::effectEnd(m_lightEffect);
                            dx9rend::stateManagerPop();
                        }
                    }

                    // The target through QuadGeom's TexturedQuad into the second target.
                    lightManagerSetRenderTargetTexture(0, m_targetTexture2);
                    setViewport(0, 0, kTargetSize, kTargetSize, viewportMinZ, viewportMaxZ);
                    lightManagerSetDepthStencil(m_depthStencil);
                    dx9rend::stateManagerClear(1, 0xFF000000u, 1.0f, 1);
                    if (m_quadVertexBuffer)
                    {
                        dx9rend::stateManagerPush();
                        dx9rend::effectSetTechniqueByName(m_quadEffect, vc71::string("TexturedQuad"));
                        dx9rend::effectGetShaderParameterBorrowedTyped(
                            m_quadEffect, vc71::string("TEXLAYER0"))->setValue(m_targetTexture, 0);
                        dx9rend::vertexBufferBindStream(m_quadVertexBuffer, 0);
                        dx9rend::effectBegin(m_quadEffect);
                        dx9rend::effectBeginPass(m_quadEffect, 0);
                        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
                        dx9rend::effectEnd(m_quadEffect);
                        dx9rend::stateManagerPop();
                    }
                    device->EndScene();
                    bool allDone = true;
                    for (unsigned int s = 0; s < count; ++s)
                        if (!samples[s].done)
                            allDone = false;
                    if (!allDone)
                    {
                        // The second target is read back once anything was drawn, the first only
                        // when a directional or GI light was among the lights.
                        dx9rend::TextureLockRecord* localLock = 0;
                        dx9rend::TextureLockRecord* directLock = 0;
                        const unsigned char* localPixels = 0;
                        const unsigned char* directPixels = 0;
                        if (rendered)
                        {
                            if (!dx9rend::textureManagerCopyRenderTarget(m_targetTexture2,
                                                                         m_sysMemTargetTexture, 0,
                                                                         0x7FFFFFFF))
                                ::renddx9abi::logf("LightmapGeneration.cpp:1836: copy fail");
                            do
                            {
                                localLock = dx9rend::textureLock(m_sysMemTargetTexture, 0, 0x7FFFFFFF);
                            }
                            while (!localLock);
                            localPixels = static_cast<const unsigned char*>(localLock->pBits);
                            if (directionalSeen)
                            {
                                if (!dx9rend::textureManagerCopyRenderTarget(m_targetTexture,
                                                                             m_sysMemTargetTexture2, 0,
                                                                             0x7FFFFFFF))
                                    ::renddx9abi::logf("LightmapGeneration.cpp:1845: copy fail");
                                do
                                {
                                    directLock = dx9rend::textureLock(m_sysMemTargetTexture2, 0,
                                                                      0x7FFFFFFF);
                                }
                                while (!directLock);
                                directPixels = static_cast<const unsigned char*>(directLock->pBits);
                            }
                        }

                        for (unsigned int s = 0; s < count; ++s)
                        {
                            LightSample& sample = samples[s];
                            if (sample.done)
                                continue;
                            sample.result[0] = m_ambientColor.x;
                            sample.result[1] = m_ambientColor.y;
                            sample.result[2] = m_ambientColor.z;
                            unsigned int slot = s;
                            for (LightInfo* light = m_lights.data();
                                 light != m_lights.data() + m_lights.size(); ++light, slot += count)
                            {
                                const unsigned char* const pixels =
                                    light->isLocal ? localPixels : directPixels;
                                unsigned int red = 0;
                                unsigned int green = 0;
                                unsigned int blue = 0;
                                if (pixels)
                                {
                                    // The tile's channel sums; opaque black adds nothing and is
                                    // skipped.
                                    const unsigned int tileX = (slot % tilesPerRow) * tileSize;
                                    unsigned int row = (slot / tilesPerRow) * tileSize;
                                    const unsigned int rowEnd = row + tileSize;
                                    const unsigned char* pixel = pixels + (row * kTargetSize + tileX) * 4;
                                    for (;;)
                                    {
                                        for (unsigned int x = tileX; ; )
                                        {
                                            unsigned int texel;
                                            std::memcpy(&texel, pixel, sizeof(texel));
                                            if (texel != 0xFF000000u)
                                            {
                                                red += pixel[2];
                                                green += pixel[1];
                                                blue += pixel[0];
                                            }
                                            if (++x == tileX + tileSize)
                                                break;
                                            pixel += 4;
                                        }
                                        pixel += 4 * (kTargetSize + 1 - tileSize);
                                        if (++row == rowEnd)
                                            break;
                                    }
                                }
                                if (light->intensity > 0.0f)
                                {
                                    const float term = (light->softness + results[slot])
                                                       / (light->softness + 1.0f) * light->intensity;
                                    if (results[slot] > m_directlyLightedThreshold
                                        && blue + green + red != 0)
                                        sample.directlyLit = 1;
                                    sample.result[0] = static_cast<float>(red) * pixelScale
                                                       * (1.0f - m_ambientColor.x) * term
                                                       + sample.result[0];
                                    sample.result[1] = static_cast<float>(green) * pixelScale
                                                       * (1.0f - m_ambientColor.y) * term
                                                       + sample.result[1];
                                    sample.result[2] = static_cast<float>(blue) * pixelScale
                                                       * (1.0f - m_ambientColor.z) * term
                                                       + sample.result[2];
                                }
                                else
                                {
                                    const float shadowRed = clampUnit(
                                        (1.0f - static_cast<float>(red) * pixelScale)
                                        * light->shadowIntensity);
                                    const float shadowGreen = clampUnit(
                                        (1.0f - static_cast<float>(green) * pixelScale)
                                        * light->shadowIntensity);
                                    const float shadowBlue = clampUnit(
                                        (1.0f - static_cast<float>(blue) * pixelScale)
                                        * light->shadowIntensity);
                                    sample.result[0] = sample.result[0] - shadowRed;
                                    sample.result[1] = sample.result[1] - shadowGreen;
                                    sample.result[2] = sample.result[2] - shadowBlue;
                                }
                            }
                            sample.result[0] = clampUnit(sample.result[0]);
                            sample.result[1] = clampUnit(sample.result[1]);
                            sample.result[2] = clampUnit(sample.result[2]);
                        }
                        if (directLock)
                            dx9rend::textureUnlock(m_sysMemTargetTexture2, directLock);
                        if (localLock)
                            dx9rend::textureUnlock(m_sysMemTargetTexture, localLock);
                    }
                    sampler->storeResults();                                  // +0x1C

                    // Progress: the callback, or (without one) the on-screen statistics.
                    const unsigned int drawCalls = *reinterpret_cast<unsigned int*>(stats + 0x1C4);
                    const unsigned int primitives = *reinterpret_cast<unsigned int*>(stats + 0x1C0);
                    remainingInBlock -= count;
                    remainingTotal -= count;
                    const unsigned int done = totalSamples - remainingTotal;
                    const float doneSamples = static_cast<float>(done);
                    const float percent = doneSamples / static_cast<float>(totalSamples) * 100.0f;
                    const float rate = static_cast<float>(
                        1.0 / (g_system->getWallClockTime() - static_cast<double>(startTime)));
                    const float remainingSeconds =
                        static_cast<float>(remainingTotal) / (rate * doneSamples);
                    if (m_progressCallback)
                    {
                        LightmapGenerationProgress progress;
                        progress.percentDone = percent;
                        vc71::string total;
                        vc71::string left;
                        vc71::string line;
                        if (objectIndex != -1 && objectCount != -1)
                        {
                            vc71::string head;
                            formatString(head, "Object %d/%d :", objectIndex + 1, objectCount);
                            formatTime(total, static_cast<unsigned int>(
                                static_cast<int>(g_system->getWallClockTime())));
                            formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                            formatString(line, "%s [%s left , total: %s]", head.c_str(), left.c_str(),
                                         total.c_str());
                        }
                        else
                        {
                            formatTime(total, static_cast<unsigned int>(
                                static_cast<int>(g_system->getWallClockTime())));
                            formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                            formatString(line, "Patch %d : [%s left , total: %s]", firstBlock + 1,
                                         left.c_str(), total.c_str());
                        }
                        progress.status = line.c_str();
                        if (!m_progressCallback(&progress, m_progressUser))
                            g_traceAborted = true;
                    }
                    else
                    {
                        renderer()->setRenderView(savedView);
                        renderer()->setRenderViewDup(savedViewDup);
                        lightManagerSetRenderTargetTexture(0, 0);
                        lightManagerSetDepthStencil(savedDepth);
                        setViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ,
                                    viewportMaxZ);
                        const unsigned int white[3] = { 255, 255, 255 };
                        renderer()->beginScene(white, 1);                            // +0xC8
                        float y = 30.0f;
                        vc71::string text;
                        formatString(text, "%.2f%%  Lights:%d", static_cast<double>(percent),
                                     m_lights.size());
                        fontPortedDrawText(m_font, 50.0f, 30.0f, text);
                        if (record)
                        {
                            const int shownCount = g_traceObjectCount ? g_traceObjectCount : 1;
                            y = 70.0f;
                            formatString(text, "Current:%s    %d / %d objects",
                                         fromHostStdString(
                                             objectTemplatePointer(record->object)->getName()).c_str(),
                                         g_traceObjectIndex + 1, shownCount);
                            fontPortedDrawText(m_font, 50.0f, 70.0f, text);
                        }
                        if (!m_onlyNearest)
                        {
                            y = y + 40.0f;
                            formatString(text, "%d / %d blocks", firstBlock + 1, sampler->getBlockCount());
                            fontPortedDrawText(m_font, 50.0f, y, text);
                        }
                        y = y + 40.0f;
                        formatString(text, "%d / %d samples", done, totalSamples);
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        formatString(text, "%d samples/sec",
                                     static_cast<int>(static_cast<float>(count) * rate));
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        vc71::string total;
                        vc71::string left;
                        formatTime(total, static_cast<unsigned int>(
                            static_cast<int>(g_system->getWallClockTime())));
                        formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                        formatString(text, "%s left , total: %s", left.c_str(), total.c_str());
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        formatString(text, "%d pixels in shadow", 0);
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        formatString(text, "DrawCalls:%d Primtives:%d", drawCalls, primitives);
                        fontPortedDrawText(m_font, 50.0f, y + 40.0f, text);
                        renderer()->endScene();                                     // +0xCC
                        renderer()->present(2);                                     // +0xD0
                    }
                }
                while (remainingInBlock > 0);
            }
            m_lights.resize(persistentLights, LightInfo());
            if (g_traceAborted || m_onlyNearest)
                break;
        nextBlock:
            ++firstBlock;
        }
        while (firstBlock < sampler->getBlockCount());
    }

    (void)g_system->getWallClockTime();
    ::renddx9abi::engineFree(samples);
    ::renddx9abi::engineFree(results);
    renderer()->setRenderView(savedView);
    renderer()->setRenderView(savedViewDup);         // the duplicate goes through +0x44 too (shipped)
    lightManagerSetRenderTargetTexture(0, 0);
    setViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ, viewportMaxZ);
    lightManagerSetDepthStencil(savedDepth);
    (void)occluderPeak;
    return sampleable->destroyLightSampler(sampler);
}

// The intensity trace. `record` null traces the terrain (block -1: every block from
// m_firstBlock - 1, or the camera's block with onlyNearest); otherwise the record's geometry in its
// current (geom, lod). Per block: the block's lights are gathered and per-triangle back-face bits
// built; per batch, every (light, sample) pair renders the occluders seen from the sample into its
// tile of the 1024 x 1024 target (the view from the sample toward the light), the target is read
// back, and each sample's channels become R = local lights, G = directional lights (sun), B = 0,
// A = GI lights, from the unshadowed share of each tile times the light's phong/attenuation term;
// the sampler stores them. Answers destroyLightSampler's result.
bool LightmapGeneration::traceIntensity(OccluderRecord* record, int objectIndex, int objectCount,
                                        int block)
{
    unsigned int method = static_cast<unsigned int>(m_generationMethod);
    g_traceAborted = false;
    m_waterSunIntensity = 0.0f;
    IBase* const terrain = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("Terrain"));                               // ClassManager +0x48
    if (!terrain)
        return false;
    ITerrainEditable* const terrainEditable =
        static_cast<ITerrainEditable*>(terrain->queryInterface(kIID_ITerrainEditable));
    world::IHeightmap* const heightmap = heightmapClusterByName()->getPrimaryHeightmap();   // +0x40

    bool waterObject = false;
    ILightSampleable* sampleable;
    if (record)
    {
        world::IObject* const object = record->object;
        object->getAbsoluteTransformation();                                // +0x80, discarded
        object->updateGeometry();                                           // +0xB4
        object->handleVisualUpdate(0.0f, 0.0f);                             // +0xA4
        IBase* const geometry =
            static_cast<IBase*>(world::object_detail::LayoutAccess::geometry(object));
        if (!geometry)
            return false;
        sampleable = static_cast<ILightSampleable*>(geometry->queryInterface(kIIDLightSampleable));
        // +0x280 of whatever geometry the object carries: StaticMesh::m_template.
        const StaticMeshTemplate* const tmpl = static_cast<const StaticMeshTemplate*>(
            static_cast<StaticMesh*>(world::object_detail::LayoutAccess::geometry(object))->m_template);
        waterObject = tmpl ? tmpl->m_isWater != 0 : false;
    }
    else
    {
        sampleable = static_cast<ILightSampleable*>(terrain->queryInterface(kIIDLightSampleable));
        if (!(method & 0x100u))
            method &= ~5u;
    }
    if (!sampleable)
        return false;
    if (!sampleable->isLightSampleable())
        return false;
    ILightSampling* const sampler = sampleable->createLightSampler(m_resolutionModifier);
    if (!sampler)
        return false;

    unsigned int viewportX;
    unsigned int viewportY;
    unsigned int viewportWidth;
    unsigned int viewportHeight;
    float viewportMinZ;
    float viewportMaxZ;
    getViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ, viewportMaxZ);
    if (!m_occluderTree)
        rebuildOccluders();

    // The batch arrays: three times one target's worth of samples, and their per-light results.
    const unsigned int capacity =
        0x100000u / (static_cast<unsigned int>(m_blockSize) * static_cast<unsigned int>(m_blockSize)) + 1;
    LightSample* samples = 0;
    if (void* const storage = ::renddx9abi::engineAlloc(144 * capacity))
    {
        samples = static_cast<LightSample*>(storage);
        for (unsigned int i = 0; i < 3 * capacity; ++i)
            new (&samples[i]) LightSample();
    }
    float* const results = static_cast<float*>(::renddx9abi::engineAlloc(12 * capacity));
    const unsigned int tileSize = static_cast<unsigned int>(m_blockSize);
    const unsigned int tileArea = tileSize * tileSize;
    const float pixelScale = 1.0f / (static_cast<float>(tileArea) * 255.0f);
    const unsigned int tilesPerRow = kTargetSize / tileSize;
    dx9rend::IRenderView* const savedView = renderer()->getRenderView();          // +0x48
    dx9rend::IRenderView* const savedViewDup = renderer()->getRenderViewDup();    // +0x50
    unsigned int firstBlock = static_cast<unsigned int>(m_firstBlock - 1);
    unsigned int occluderPeak = 0;
    Vc71Vec<OccluderRecord*> occluders;
    unsigned int totalSamples = 0;
    const Vec3& cameraPosition = *reinterpret_cast<const Vec3*>(&savedView->getViewMatrixPtr()->m[12]);
    if (block != -1)
    {
        firstBlock = static_cast<unsigned int>(block);
        totalSamples = sampler->getBlock(firstBlock).sampleCount;
    }
    else if (m_onlyNearest)
    {
        Vec3 relative;
        if (record)
        {
            relative.x = cameraPosition.x - record->position.x;
            relative.y = cameraPosition.y - record->position.y;
            relative.z = cameraPosition.z - record->position.z;
        }
        firstBlock = nearestBlock(record ? relative : cameraPosition, sampler);
        totalSamples = sampler->getBlock(firstBlock).sampleCount;
    }
    else
    {
        for (unsigned int b = firstBlock; b < sampler->getBlockCount(); ++b)
            totalSamples += sampler->getBlock(b).sampleCount;
    }
    unsigned int remainingTotal = totalSamples;
    const float startTime = static_cast<float>(g_system->getWallClockTime());   // System +0x18
    const bool worldSpace = !record || mat4ApproxEqual(record->transform, Mat4::getIdentity());

    IDirect3DDevice9* const device = renderDevice();
    IDirect3DSurface9* savedDepth;
    device->GetDepthStencilSurface(&savedDepth);                            // never released (shipped)
    unsigned char* const stats = static_cast<unsigned char*>(renderer()->getRenderStats());   // +0x6C
    std::memset(stats + 0x17C, 0, 0xA4);
    TerrainAabb lightBounds;
    lightBounds.min.x = 0.0f; lightBounds.min.y = 0.0f; lightBounds.min.z = 0.0f;
    lightBounds.max.x = 0.0f; lightBounds.max.y = 0.0f; lightBounds.max.z = 0.0f;
    lightBounds.cornerLength = 0.0f;
    lightBounds.radius = 0.0f;
    lightBounds.dirty = 1;
    lightBounds.extent.x = 0.0f; lightBounds.extent.y = 0.0f; lightBounds.extent.z = 0.0f;
    lightBounds.centre.x = 0.0f; lightBounds.centre.y = 0.0f; lightBounds.centre.z = 0.0f;
    lightManagerSetRenderTargetTexture(0, m_targetTexture);
    lightManagerSetDepthStencil(m_depthStencil);

    if (firstBlock < sampler->getBlockCount())
    {
        do
        {
            const LightSampleBlock blockInfo = sampler->getBlock(firstBlock);
            const unsigned int persistentLights = m_lights.size();
            lightBounds = blockInfo.bounds;
            if (record)
            {
                lightBounds.min = cornersMinimum(record->corners);
                lightBounds.max = cornersMaximum(record->corners);
            }
            gatherLights(&lightBounds);

            // Per light: a back-face bit per sampler triangle (the terrain has none - one clear bit),
            // and each local light's index in the sampler (the table is never freed - shipped).
            const vc71::vector_layout* const triangleVector =
                static_cast<const vc71::vector_layout*>(blockInfo.triangles);
            int triangleCount = 0;
            if (triangleVector && triangleVector->first)
                triangleCount = static_cast<int>((triangleVector->last - triangleVector->first) / 92);
            const SampleTriangle* const triangles = triangleVector
                ? reinterpret_cast<const SampleTriangle*>(triangleVector->first) : 0;
            int* lightIndex = static_cast<int*>(::renddx9abi::engineAlloc(4 * m_lights.size()));
            for (unsigned int l = 0; l < m_lights.size(); ++l, ++lightIndex)
            {
                LightInfo& light = m_lights[l];
                if (triangleCount == 0)
                {
                    light.facing.resize(1);
                    light.facing.clear(0);
                }
                else
                {
                    light.facing.resize(static_cast<unsigned int>(triangleCount));
                    for (int t = 0; t < triangleCount; ++t)
                    {
                        if (!m_enableBackfaceCulling || light.isLocal)
                        {
                            light.facing.clear(static_cast<unsigned int>(t));
                            continue;
                        }
                        const Vec3* const n = triangles[t].normal;
                        const Vec3& d = light.direction;
                        float dot0;
                        float dot1;
                        float dot2;
                        if (!worldSpace)
                        {
                            const float* const m = record->transform.m;
                            const float z0 = (n[0].x * m[2] + n[0].z * m[10]) + n[0].y * m[6];
                            const float y0 = (n[0].x * m[1] + n[0].z * m[9]) + n[0].y * m[5];
                            const float x0 = (n[0].z * m[8] + n[0].y * m[4]) + n[0].x * m[0];
                            dot0 = (x0 * d.x + y0 * d.y) + z0 * d.z;
                            const float z1 = (n[1].y * m[6] + n[1].z * m[10]) + n[1].x * m[2];
                            const float y1 = (n[1].y * m[5] + n[1].z * m[9]) + n[1].x * m[1];
                            const float x1 = (n[1].y * m[4] + n[1].z * m[8]) + n[1].x * m[0];
                            dot1 = (x1 * d.x + y1 * d.y) + z1 * d.z;
                            const float z2 = (n[2].y * m[6] + n[2].z * m[10]) + n[2].x * m[2];
                            const float y2 = (n[2].y * m[5] + n[2].z * m[9]) + n[2].x * m[1];
                            const float x2 = (n[2].y * m[4] + n[2].z * m[8]) + n[2].x * m[0];
                            dot2 = (x2 * d.x + y2 * d.y) + z2 * d.z;
                        }
                        else
                        {
                            dot0 = (n[0].y * d.y + n[0].z * d.z) + n[0].x * d.x;
                            dot1 = (n[1].y * d.y + n[1].z * d.z) + d.x * n[1].x;
                            dot2 = (n[2].y * d.y + n[2].z * d.z) + n[2].x * d.x;
                        }
                        if (dot0 <= 0.0f && dot1 <= 0.0f && dot2 <= 0.0f)
                            light.facing.set(static_cast<unsigned int>(t));
                        else
                            light.facing.clear(static_cast<unsigned int>(t));
                    }
                }
                if (light.isLocal)
                {
                    world::LightSource* const source = light.source;
                    const Mat4 transform = source->getAbsoluteTransformation();    // +0x80
                    const float range = source->getAttenuationRange1();             // +0x188
                    const Vec3 position = { transform.m[12], transform.m[13], transform.m[14] };
                    *lightIndex = sampler->addLight(source->getColor(), position, range);  // +0x198
                }
            }

            // A render view per light: its fov, the camera near plane, its view distance, square.
            for (unsigned int l = 0; l < m_lights.size(); ++l)
            {
                LightInfo& light = m_lights[l];
                light.renderView = static_cast<dx9rend::IRenderView*>(
                    ::dice::hfe::classManager->createInstance(kCIDRenderView, kIIDRenderView, 0));   // +0x30
                light.renderView->setFov(0.017453292f * light.fov);
                light.renderView->setNearPlane(m_cameraNearPlane);
                light.renderView->setFarPlane(light.viewDistance);
                light.renderView->setAspect(1.0f);
            }

            unsigned int remainingInBlock = blockInfo.sampleCount;
            if (!m_lights.data() || m_lights.size() == 0)
                goto nextBlock;                     // no light: the next block, untrimmed (shipped)
            if (blockInfo.sampleCount != 0)
            {
                const unsigned int viewsPerTarget = tilesPerRow * tilesPerRow;
                do
                {
                    const unsigned int count = sampler->getSamples(
                        firstBlock, viewsPerTarget / m_lights.size(), samples);
                    if (count == 0 || g_traceAborted)
                        break;
                    (void)g_system->getWallClockTime();
                    device->BeginScene();
                    lightManagerSetRenderTargetTexture(0, m_targetTexture);
                    lightManagerSetDepthStencil(m_depthStencil);
                    setViewport(0, 0, kTargetSize, kTargetSize, viewportMinZ, viewportMaxZ);
                    dx9rend::stateManagerClear(7, 0xFFFFFFFFu, 1.0f, 1);
                    dx9rend::rendererStatsBeginFrame();
                    std::memset(stats + 0x17C, 0, 0xA4);
                    int sunIndex = -1;
                    bool rendered = false;
                    for (unsigned int s = 0; s < count; ++s)
                    {
                        samples[s].done = 0;
                        samples[s].directlyLit = 0;
                    }

                    int lightNumber = 0;
                    for (LightInfo* light = m_lights.data(); light != m_lights.data() + m_lights.size();
                         ++light, ++lightNumber)
                    {
                        if (!light->isLocal)
                        {
                            // Directional and GI lights look along their direction from the origin;
                            // straight up gets a fixed basis. The sample position is set per tile.
                            Mat4 view = Mat4::getIdentity();
                            const Vec3& d = light->direction;
                            if (std::fabs(d.x) <= FLT_EPSILON && std::fabs(d.y - 1.0f) <= FLT_EPSILON
                                && std::fabs(d.z) <= FLT_EPSILON)
                            {
                                view.m[9] = 1.0f;
                                view.m[8] = 0.0f;
                                view.m[4] = 1.0f;
                                view.m[10] = 0.0f;
                                view.m[2] = 1.0f;
                                view.m[5] = 0.0f;
                                view.m[6] = 0.0f;
                                view.m[0] = 0.0f;
                                view.m[1] = 0.0f;
                            }
                            else
                            {
                                const Vec3 up = { 0.0f, 1.0f, 0.0f };
                                lookAt(view, Vec3::getZero(), light->direction, up);
                            }
                            light->renderView->setViewMatrix(view.m);               // +0x60
                            if (!light->isGI && !light->isLocal)
                            {
                                g_traceSunDirection = light->direction;
                                sunIndex = lightNumber;
                            }
                        }

                        for (unsigned int s = 0; s < count; ++s)
                        {
                            LightSample sample = samples[s];
                            if (_isnan(sample.position.x) || _isnan(sample.position.y)
                                || _isnan(sample.position.z) || _isnan(sample.normal.x)
                                || _isnan(sample.normal.y) || _isnan(sample.normal.z))
                                continue;
                            const unsigned int slot = count * static_cast<unsigned int>(lightNumber) + s;
                            if (sample.triangle != 0xFFFFFFFFu && light->facing.test(sample.triangle))
                            {
                                results[slot] = 0.0f;
                                continue;
                            }
                            const float offset = m_sampleOffset;
                            sample.position.x = sample.normal.x * offset + sample.position.x;
                            sample.position.y = offset * sample.normal.y + sample.position.y;
                            sample.position.z = offset * sample.normal.z + sample.position.z;
                            if (!worldSpace)
                            {
                                const float* const m = record->transform.m;
                                const Vec3 p = sample.position;
                                sample.position.x = ((p.z * m[8] + p.y * m[4]) + p.x * m[0]) + m[12];
                                sample.position.y = ((p.z * m[9] + p.y * m[5]) + p.x * m[1]) + m[13];
                                sample.position.z = ((p.z * m[10] + p.y * m[6]) + p.x * m[2]) + m[14];
                            }
                            if (m_shadowsNearCamera >= 0.0f)
                            {
                                const float dx = cameraPosition.x - sample.position.x;
                                const float dy = cameraPosition.y - sample.position.y;
                                const float dz = cameraPosition.z - sample.position.z;
                                const float limit = m_shadowsNearCamera;
                                if (limit * limit < (dx * dx + dz * dz) + dy * dy)
                                {
                                    samples[s].done = 1;
                                    samples[s].directlyLit = 0;
                                    continue;
                                }
                            }
                            if (!worldSpace)
                            {
                                const float* const m = record->transform.m;
                                const Vec3 n = sample.normal;
                                sample.normal.x = (n.z * m[8] + n.y * m[4]) + n.x * m[0];
                                sample.normal.y = (n.z * m[9] + n.y * m[5]) + n.x * m[1];
                                sample.normal.z = (n.z * m[10] + n.y * m[6]) + n.x * m[2];
                            }
                            const float facingLight = (sample.normal.z * light->direction.z
                                                       + sample.normal.y * light->direction.y)
                                                      + sample.normal.x * light->direction.x;
                            if (m_noPhongAngle)
                                results[slot] = facingLight > 0.1f ? 1.0f : 0.0f;
                            else
                                results[slot] = clampUnit(responseCurve(m_phongAngleMode, facingLight));

                            // The sample's tile of the target.
                            setViewport((slot % tilesPerRow) * tileSize, (slot / tilesPerRow) * tileSize,
                                        tileSize, tileSize, viewportMinZ, viewportMaxZ);
                            rendered = true;
                            Mat4 view = *light->renderView->getViewMatrixPtr();      // +0x64
                            if (light->isLocal)
                            {
                                const float dx = light->position.x - sample.position.x;
                                const float dy = light->position.y - sample.position.y;
                                const float dz = light->position.z - sample.position.z;
                                const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
                                const float t = distance / light->radius;
                                const float attenuation = clampUnit(1.0f - t * t);
                                const float inverse = 1.0f / distance;
                                const float toLightX = inverse * dx;
                                const float toLightY = dy * inverse;
                                const float toLightZ = inverse * dz;
                                if (m_noPhongAngle)
                                    results[slot] = attenuation;
                                else
                                    results[slot] = attenuation * clampUnit(responseCurve(
                                        m_phongAngleMode,
                                        (sample.normal.x * toLightX + toLightZ * sample.normal.z)
                                            + toLightY * sample.normal.y));
                                const Vec3 target = { sample.position.x + toLightX,
                                                      toLightY + sample.position.y,
                                                      toLightZ + sample.position.z };
                                const Vec3 up = { 0.0f, 1.0f, 0.0f };
                                lookAt(view, sample.position, target, up);
                            }
                            else
                            {
                                view.m[12] = sample.position.x;
                                view.m[13] = sample.position.y;
                                view.m[14] = sample.position.z;
                            }
                            light->renderView->setViewMatrix(view.m);
                            renderer()->setRenderView(light->renderView);           // +0x44
                            renderer()->setRenderViewDup(light->renderView);        // +0x4C

                            if (light->objectShadows)
                            {
                                occluders.resize(0);
                                occludersInFrustum(*light->renderView->getFrustumPtr(), &occluders);
                                if (occluders.size() > static_cast<unsigned int>(light->maxOccluders))
                                    sortOccluders(&occluders, sample.position, light->maxOccluders);
                                for (unsigned int o = 0; o < occluders.size(); ++o)
                                {
                                    OccluderRecord* const occluder = occluders[o];
                                    world::IObject* const object = occluder->object;
                                    if ((record && record->geometry == occluder->geometry)
                                        || !occluder->geometry)
                                        continue;
                                    object->getAbsoluteTransformation();            // discarded
                                    object->updateGeometry();
                                    geometryDrawSilhouette(occluder->geometry, light->renderView, 0);
                                }
                                if (occluders.data() && occluders.size() > occluderPeak)
                                    occluderPeak = occluders.size();
                            }
                            if (terrainEditable && !light->isGI && m_renderTerrain)
                                terrainEditable->renderForLightmapGeneration();   // ITerrainEditable +0xB0
                            if (light->selfShadowing && record)
                                geometryDrawSilhouette(record->geometry, light->renderView, 0);

                            if (!light->isLocal || !light->source)
                                continue;

                            // The local light's marker: its sphere through LightGeom into the tile.
                            dx9rend::stateManagerPush();
                            Mat4 worldViewProjection = Mat4::getIdentity();
                            Mat4 worldView = Mat4::getIdentity();
                            const Mat4 inverseView = *light->renderView->getInverseViewMatrixPtr();
                            mat4Mult(worldView, light->spotSource->getAbsoluteTransformation(),
                                     inverseView);
                            const world::LightSourceTemplate* const tmpl =
                                lightSourceTemplatePointer(light->source);
                            if (tmpl->getLightTypeEnum() == 1)
                                dx9rend::effectSetTechniqueByName(m_lightEffect,
                                                                  vc71::string("Pointlight"));
                            else if (tmpl->getLightTypeEnum() == 2)
                            {
                                float coneAngle = light->source->getConeAngleRads2();   // +0x1C0
                                Mat4 basis = light->source->getAbsoluteTransformation();
                                const Mat4 copy = basis;
                                mat4Transpose(basis, copy);
                                const Vec3& direction = light->source->getAbsoluteDirection();  // +0x180
                                const float* const m = basis.m;
                                Vec3 spotDirection;
                                spotDirection.x = (m[0] * direction.x + m[8] * direction.z)
                                                  + m[4] * direction.y;
                                spotDirection.y = (m[1] * direction.x + m[9] * direction.z)
                                                  + m[5] * direction.y;
                                spotDirection.z = (m[2] * direction.x + m[10] * direction.z)
                                                  + m[6] * direction.y;
                                dx9rend::effectSetTechniqueByName(m_lightEffect,
                                                                  vc71::string("Spotlight"));
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("WorldView"))->setValue(&worldView, 0);
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("SpotDir"))->setValue(&spotDirection, 0);
                                dx9rend::effectGetShaderParameterBorrowedTyped(
                                    m_lightEffect, vc71::string("ConeAngle"))->setValue(&coneAngle, 0);
                            }
                            // The scale replaces the WorldView diagonal (shipped).
                            worldView.m[0] = light->source->getScale().x;           // +0x1D4
                            worldView.m[5] = light->source->getScale().y;
                            worldView.m[10] = light->source->getScale().z;
                            mat4Mult(worldViewProjection, worldView,
                                     *light->renderView->getProjectionMatrixPtr());  // +0x6C
                            dx9rend::effectGetShaderParameterBorrowedTyped(
                                m_lightEffect, vc71::string("WorldViewProj"))
                                ->setValue(&worldViewProjection, 0);
                            const Vec3& colour = light->source->getColor();
                            const float lightColour[4] = { colour.x, colour.y, colour.z, 1.0f };
                            dx9rend::effectGetShaderParameterBorrowedTyped(
                                m_lightEffect, vc71::string("LightColor"))->setValue(lightColour, 0);
                            dx9rend::stateManagerSetStreamSource(0, m_lightMeshVertices, 0, 12);
                            dx9rend::stateManagerSetIndices(m_lightMeshIndices);
                            dx9rend::effectBegin(m_lightEffect);
                            dx9rend::effectBeginPass(m_lightEffect, 0);
                            dx9rend::rendererDrawIndexedPrimitive(m_lightMeshFaces, D3DPT_TRIANGLELIST,
                                                                  0, 0, m_lightMeshVertexCount, 0);
                            dx9rend::effectEnd(m_lightEffect);
                            dx9rend::stateManagerPop();
                        }
                    }

                    setViewport(0, 0, kTargetSize, kTargetSize, viewportMinZ, viewportMaxZ);
                    device->EndScene();
                    bool allDone = true;
                    for (unsigned int s = 0; s < count; ++s)
                        if (!samples[s].done)
                            allDone = false;
                    if (!allDone)
                    {
                        dx9rend::TextureLockRecord* lock = 0;
                        const unsigned char* pixels = 0;
                        if (rendered)
                        {
                            if (!dx9rend::textureManagerCopyRenderTarget(m_targetTexture,
                                                                         m_sysMemTargetTexture2, 0,
                                                                         0x7FFFFFFF))
                                ::renddx9abi::logf("LightmapGeneration.cpp:2703: copy fail");
                            do
                            {
                                lock = dx9rend::textureLock(m_sysMemTargetTexture2, 0, 0x7FFFFFFF);
                            }
                            while (!lock);
                            pixels = static_cast<const unsigned char*>(lock->pBits);
                        }

                        // Bit 0: GI darkened by the sun's term (the sun's tile at index -1 when no
                        // sun was gathered - shipped); bit 2: the sun's term thresholded at 0.1.
                        if (method & 5u)
                        {
                            for (unsigned int s = 0; s < count; ++s)
                            {
                                if (method & 1u)
                                {
                                    float* term = &results[s];
                                    for (LightInfo* light = m_lights.data();
                                         light != m_lights.data() + m_lights.size(); ++light, term += count)
                                        if (light->isGI)
                                            *term = (1.0f - results[sunIndex * static_cast<int>(count)
                                                                    + static_cast<int>(s)]) * *term;
                                }
                                if (method & 4u)
                                {
                                    float* term = &results[s];
                                    for (LightInfo* light = m_lights.data();
                                         light != m_lights.data() + m_lights.size(); ++light, term += count)
                                        if (!light->isGI && !light->isLocal)
                                            *term = *term < 0.1f ? 0.0f : 1.0f;
                                }
                            }
                        }

                        for (unsigned int s = 0; s < count; ++s)
                        {
                            LightSample& sample = samples[s];
                            if (sample.done)
                                continue;
                            float localLights = 0.0f;
                            float giLights = 0.0f;
                            float directLights = 0.0f;
                            unsigned int slot = s;
                            const unsigned int triangle = sample.triangle;
                            for (LightInfo* light = m_lights.data();
                                 light != m_lights.data() + m_lights.size(); ++light, slot += count)
                            {
                                if (triangle != 0xFFFFFFFFu && light->facing.test(triangle))
                                    continue;
                                unsigned int red = 0;
                                unsigned int green = 0;
                                unsigned int blue = 0;
                                if (pixels)
                                {
                                    const unsigned int tileX = (slot % tilesPerRow) * tileSize;
                                    unsigned int row = (slot / tilesPerRow) * tileSize;
                                    const unsigned int rowEnd = row + tileSize;
                                    const unsigned char* pixel = pixels + (row * kTargetSize + tileX) * 4;
                                    for (;;)
                                    {
                                        for (unsigned int x = tileX; ; )
                                        {
                                            red += pixel[2];
                                            green += pixel[1];
                                            blue += pixel[0];
                                            if (++x == tileX + tileSize)
                                                break;
                                            pixel += 4;
                                        }
                                        pixel += 4 * (kTargetSize + 1 - tileSize);
                                        if (++row == rowEnd)
                                            break;
                                    }
                                }
                                const unsigned int lit = blue + green + red;
                                float contribution;
                                if (light->intensity > 0.0f)
                                {
                                    const float term = (results[slot] + light->softness)
                                                       / (light->softness + 1.0f) * light->intensity;
                                    if (results[slot] > m_directlyLightedThreshold && lit != 0)
                                        sample.directlyLit = 1;
                                    contribution = term * (static_cast<float>(lit) * pixelScale)
                                                   * 0.33333334f;
                                }
                                else
                                {
                                    contribution = -clampUnit(
                                        (1.0f - static_cast<float>(lit) * pixelScale * 0.33333334f)
                                        * light->shadowIntensity);
                                }
                                if (light->isLocal)
                                    localLights += contribution;
                                else if (light->isGI)
                                    giLights += contribution;
                                else
                                    directLights += contribution;
                            }
                            sample.result[0] = clampUnit(localLights);
                            const float direct = clampUnit(directLights);
                            sample.result[1] = direct;
                            sample.result[2] = 0.0f;
                            sample.result[3] = clampUnit(giLights);
                            if (method & 0x10u)
                            {
                                const float towardSun = clampUnit(
                                    (-g_traceSunDirection.z * sample.normal.z
                                     + -g_traceSunDirection.y * sample.normal.y)
                                    + -g_traceSunDirection.x * sample.normal.x);
                                sample.result[1] = (1.0f - towardSun) * direct;
                            }
                            if (waterObject)
                            {
                                // The water depth below the sample (0..3 m -> 0..1) replaces R.
                                Vec3 normal = { 0.0f, 0.0f, 0.0f };
                                if (!worldSpace)
                                    transformPoint(record->transform, sample.position, sample.position);
                                unsigned char material;
                                const float height = heightmap->getHeightAndNormalInWorldCoords(
                                    sample.position.x, sample.position.z, normal, material);   // +0xB4
                                float depth = sample.position.y - height;
                                if (depth < 0.0f)
                                    depth = 0.0f;
                                else if (depth > 3.0f)
                                    depth = 3.0f;
                                sample.result[0] = depth * 0.33333334f;
                            }
                            if (!record)
                            {
                                const Vec3 up = { 0.0f, 1.0f, 0.0f };
                                if (vec3ApproxEqual(sample.normal, up)
                                    && sample.result[1] > m_waterSunIntensity)
                                    m_waterSunIntensity = sample.result[1];
                            }
                            if (method & 2u)
                                sample.result[3] = (1.0f - sample.result[3]) * m_skyWhite
                                                   + sample.result[3];
                        }
                        if (lock)
                            dx9rend::textureUnlock(m_sysMemTargetTexture2, lock);
                    }
                    sampler->storeResults();                                  // +0x1C

                    // Progress: the callback, or (without one) the on-screen statistics.
                    const unsigned int drawCalls = *reinterpret_cast<unsigned int*>(stats + 0x1C4);
                    const unsigned int primitives = *reinterpret_cast<unsigned int*>(stats + 0x1C0);
                    remainingInBlock -= count;
                    remainingTotal -= count;
                    const unsigned int done = totalSamples - remainingTotal;
                    const float doneSamples = static_cast<float>(done);
                    const float percent = doneSamples / static_cast<float>(totalSamples) * 100.0f;
                    const float rate = static_cast<float>(
                        1.0 / (g_system->getWallClockTime() - static_cast<double>(startTime)));
                    const float remainingSeconds =
                        static_cast<float>(remainingTotal) / (rate * doneSamples);
                    if (m_progressCallback)
                    {
                        LightmapGenerationProgress progress;
                        progress.percentDone = percent;
                        vc71::string total;
                        vc71::string left;
                        vc71::string line;
                        if (objectIndex != -1 && objectCount != -1)
                        {
                            vc71::string head;
                            formatString(head, "Object %d/%d :", objectIndex + 1, objectCount);
                            formatTime(total, static_cast<unsigned int>(
                                static_cast<int>(g_system->getWallClockTime())));
                            formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                            formatString(line, "%s [%s left , total: %s]", head.c_str(), left.c_str(),
                                         total.c_str());
                        }
                        else
                        {
                            formatTime(total, static_cast<unsigned int>(
                                static_cast<int>(g_system->getWallClockTime())));
                            formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                            formatString(line, "Patch %d : [%s left , total: %s]", firstBlock + 1,
                                         left.c_str(), total.c_str());
                        }
                        progress.status = line.c_str();
                        if (!m_progressCallback(&progress, m_progressUser))
                            g_traceAborted = true;
                    }
                    else
                    {
                        renderer()->setRenderView(savedView);
                        renderer()->setRenderViewDup(savedViewDup);
                        lightManagerSetRenderTargetTexture(0, 0);
                        lightManagerSetDepthStencil(savedDepth);
                        setViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ,
                                    viewportMaxZ);
                        const unsigned int white[3] = { 255, 255, 255 };
                        renderer()->beginScene(white, 1);                            // +0xC8
                        float y = 30.0f;
                        vc71::string text;
                        formatString(text, "%.2f%%  Lights:%d", static_cast<double>(percent),
                                     m_lights.size());
                        fontPortedDrawText(m_font, 50.0f, 30.0f, text);
                        if (record)
                        {
                            const int shownCount = g_traceObjectCount ? g_traceObjectCount : 1;
                            y = 70.0f;
                            formatString(text, "Current:%s    %d / %d objects",
                                         fromHostStdString(
                                             objectTemplatePointer(record->object)->getName()).c_str(),
                                         g_traceObjectIndex + 1, shownCount);
                            fontPortedDrawText(m_font, 50.0f, 70.0f, text);
                        }
                        if (!m_onlyNearest)
                        {
                            y = y + 40.0f;
                            formatString(text, "%d / %d blocks", firstBlock + 1, sampler->getBlockCount());
                            fontPortedDrawText(m_font, 50.0f, y, text);
                        }
                        y = y + 40.0f;
                        formatString(text, "%d / %d samples", done, totalSamples);
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        formatString(text, "%d samples/sec",
                                     static_cast<int>(static_cast<float>(count) * rate));
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        vc71::string total;
                        vc71::string left;
                        formatTime(total, static_cast<unsigned int>(
                            static_cast<int>(g_system->getWallClockTime())));
                        formatTime(left, static_cast<unsigned int>(static_cast<int>(remainingSeconds)));
                        formatString(text, "%s left , total: %s", left.c_str(), total.c_str());
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        y = y + 40.0f;
                        formatString(text, "%d pixels in shadow", 0);
                        fontPortedDrawText(m_font, 50.0f, y, text);
                        formatString(text, "DrawCalls:%d Primtives:%d", drawCalls, primitives);
                        fontPortedDrawText(m_font, 50.0f, y + 40.0f, text);
                        renderer()->endScene();                                     // +0xCC
                        renderer()->present(2);                                     // +0xD0
                    }
                }
                while (remainingInBlock > 0);
            }
            m_lights.resize(persistentLights, LightInfo());
            if (g_traceAborted || m_onlyNearest)
                break;
        nextBlock:
            ++firstBlock;
        }
        while (firstBlock < sampler->getBlockCount());
    }

    (void)g_system->getWallClockTime();
    ::renddx9abi::engineFree(samples);
    ::renddx9abi::engineFree(results);
    renderer()->setRenderView(savedView);
    renderer()->setRenderView(savedViewDup);         // the duplicate goes through +0x44 too (shipped)
    lightManagerSetRenderTargetTexture(0, 0);
    setViewport(viewportX, viewportY, viewportWidth, viewportHeight, viewportMinZ, viewportMaxZ);
    lightManagerSetDepthStencil(savedDepth);
    (void)occluderPeak;
    return sampleable->destroyLightSampler(sampler);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
