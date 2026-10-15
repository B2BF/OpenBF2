// Path: [INFERRED] from the registered class; no __FILE__ or Linux witness.
// SkyDome implementation, recovered from RendDX9.dll 1.50.
// Dome drawing uses the listener subobject at +4; flare drawing uses the primary.
// [CHOSEN, not recovered] profiler-only brackets remain omitted at the marked sites.

#ifdef BF2_IMAGE_RENDDX9

#include "SkyDome.h"

#include "GeometryTemplateManager.h"
#include "MeshTemplate.h"
#include "StaticMeshTemplate.h"
#include "XpLightManager.h"
#include "../Game/Objects/Visual/LightSource.h"   // skyLightGetDirection - typed (RW-266)
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>

#include <cmath>
#include <cstddef>
#include <new>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {

// The three globals this file reaches, exactly as FSQuadDrawer.cpp declares them.
extern void* g_shaderManager;      // dword_1023B97C
extern void* g_rendererBase;       // dword_1023B970
extern void* g_raShaderConstants;  // dword_1023DDBC - RenderQueues.cpp owns it

// BufferManager.cpp. Same three declarations FSQuadDrawer.cpp carries; RendDX9Base.h publishes the
// create/release half of the vertex-declaration pair but not the bind half.
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
void vertexDeclarationBind(void* wrapper);

// StateManager.cpp / BF2Render.cpp - sub_10005820, declared the same way FSQuadDrawer.cpp does
// because RendDX9Base.h publishes the state-manager seams but not the draw call itself.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);

// QueryManager.cpp, the published half of the query surface.
void* queryManagerCreateQuery(int type);                           // sub_10004420 via the global
// The native address of the Query field at a retail x86 byte offset (the identity on x86); an
// offset that names no field stops with a [FRONTIER] diagnostic.
unsigned char* queryFieldAt(void* query, unsigned int retailOffset);

} // namespace dx9rend

namespace geom {

// ModuleGeom.cpp, dword_1024B914.
extern LightManager* g_LightManager;

namespace {


// The retail bodies guard each of these blocks with a bit of one flags word and zero the block on
// first use. That is MSVC's function-local-static guard pattern, and the blocks live in .data with
// no external reader, so they are file-scope here with the same guards and the same zero fill.


// dword_1025D3D0 - the dome draw's guard word. Bit 1 guards the profiler-stat registration that is
// deliberately not reproduced (see the file header), so nothing below claims it.
unsigned int g_domeOnceFlags = 0;

// [UNRESOLVED] Zeroed once by dome drawing; no consumer is known.
Vec4 g_domeScratchUnused = { 0.0f, 0.0f, 0.0f, 0.0f };

// dword_1025D144 / 134 / 124 / 114, guard bits 4 / 8 / 0x10 / 0x20. Each is filled in x and y from
// a member and handed to a shader parameter with count 0; z and w stay at the zero the guard wrote.
Vec4 g_texOffsetConstant       = { 0.0f, 0.0f, 0.0f, 0.0f };
Vec4 g_texOffset2Constant      = { 0.0f, 0.0f, 0.0f, 0.0f };
Vec4 g_cloudLerpConstant       = { 0.0f, 0.0f, 0.0f, 0.0f };
Vec4 g_cloudsFadeOutConstant   = { 0.0f, 0.0f, 0.0f, 0.0f };

// dword_1025D644 - the flare draw's guard word, a different word from the dome's. Bit 2 guards its
// own copy of the profiler registration (not reproduced), bit 1 guards the constant below.
unsigned int g_flareOnceFlags = 0;

// dword_1025D640, guard bit 1 = 0x3FC90FDB = pi/2. [UNRESOLVED]: written once at 0x10139917 and
// never read anywhere in the image. Same treatment as g_domeScratchUnused.
float g_flareUnusedConstant = 0.0f;

// Optional flare basis switch, false by default with no runtime writer.
// [UNRESOLVED] Its original name and purpose.
const bool g_flareUseUpVectorBasis = false;

// byte_10239B50, .data, initialised to 1 and never written; read only by the flare draw's angle
// fade at 0x10139DF5. flt_10239B54, .data, -0.001f, read only by the flare draw's placement maths.
const bool  g_flareAngleFadeEnabled = true;
const float g_flareDistanceScale    = -0.001f;

// The shipped constants that are not in .data.
// dbl_1021D000 is an eight-byte constant: the flare-direction test compares against a double,
// not a float, so the type here is load-bearing.
const double kFlareDirectionEpsilon = 0.01;
const float kSkyFarPlane           = 2500.0f;   // 0x451C4000, pushed as an immediate by both draws
const float kTexOffsetWrap         = 16.0f;     // update()'s wrap threshold


// Local vector math helpers.


// sub_10077830 - scale a matrix IN place by multiplying each column by the matching component.
// It scales the translation ROW too (m[12] *= s.x and so on); the dome's world matrix relies on
// that, so do not "fix" it into a 3x3 scale.
void mat4ScaleColumns(Mat4& m, const Vec3& s)
{
    m.m[0]  = s.x * m.m[0];
    m.m[4]  = m.m[4] * s.x;
    m.m[8]  = s.x * m.m[8];
    m.m[12] = m.m[12] * s.x;
    m.m[1]  = s.y * m.m[1];
    m.m[5]  = s.y * m.m[5];
    m.m[9]  = s.y * m.m[9];
    m.m[13] = s.y * m.m[13];
    m.m[2]  = s.z * m.m[2];
    m.m[6]  = m.m[6] * s.z;
    m.m[10] = m.m[10] * s.z;
    m.m[14] = m.m[14] * s.z;
}

// Multiplies in place using the full 4x4 product, preserving perspective terms.
void mat4MultiplyInPlace(Mat4& m, const Mat4& rhs)
{
    const Mat4 lhs = m;
    m.fullMult(lhs, rhs);
}

void vec3Negate(Vec3& out, const Vec3& in)
{
    const float z = -in.z;
    const float y = -in.y;
    out.x = -in.x;
    out.y = y;
    out.z = z;
}

void vec3Scale(Vec3& out, const Vec3& in, float s)
{
    const float z = s * in.z;
    const float y = s * in.y;
    out.x = s * in.x;
    out.y = y;
    out.z = z;
}

void vec3Add(Vec3& out, const Vec3& a, const Vec3& b)
{
    const float z = b.z + a.z;
    const float y = b.y + a.y;
    out.x = b.x + a.x;
    out.y = y;
    out.z = z;
}

// sub_10077760 - `out = cross(a, b)`. `out` is the __thiscall `this`; the body never reads it.
void vec3Cross(Vec3& out, const Vec3& a, const Vec3& b)
{
    out.x = b.z * a.y - b.y * a.z;
    out.y = b.x * a.z - a.x * b.z;
    out.z = a.x * b.y - b.x * a.y;
}

// sub_100777A0 - normalise in place, but only when the squared length is not already 1 (within
// FLT_EPSILON); a degenerate vector is zeroed and -20001 returned. The callers here discard it.
int vec3Normalize(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(static_cast<double>(lengthSq) - 1.0) >= 1.1920929e-7)
    {
        if (std::fabs(static_cast<double>(lengthSq)) < 1.1920929e-7)
        {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            return -20001;
        }
        const double scale = 1.0 / std::sqrt(static_cast<double>(lengthSq));
        v.x = static_cast<float>(scale * v.x);
        v.y = static_cast<float>(scale * v.y);
        v.z = static_cast<float>(scale * v.z);
    }
    return 0;
}

// sub_1000ED40, __stdcall. The high clamp is applied first and the compare is `>=`, so a NaN
// passes through both tests and comes back unchanged. Transcribed, not tidied.
float clampFloat(float value, float low, float high)
{
    double v = value;
    if (value >= static_cast<double>(high))
        v = high;
    if (v < low)
        return low;
    return static_cast<float>(v);
}


// sub_10037EB0 on dword_1023DDBC: `return this + 268`. The RaShaderConstants record is 0xAFC bytes
// and private to RenderQueues.cpp; +0x10C is the three-float world fog colour the sky uploads as
// the xyz of its `fogColor` parameter.
const Vec3* raShaderConstantsFogColor()
{
    return reinterpret_cast<const Vec3*>(dx9rend::raShaderConstantsAt(268));
}

// dx9rend::Query is private to QueryManager.cpp; these three bodies have no declaration there.
// Fields by retail offset (QueryManager.cpp's own layout): +0x04 refcount, +0x08 IDirect3DQuery9*,
// +0x0C type, +0x10 the frame ordinal of the last accepted result, +0x14 the result itself,
// +0x18/+0x19 the two state bytes. queryFieldAt maps each onto the native field.
template <typename T>
T& queryField(void* query, unsigned int retailOffset)
{
    return *reinterpret_cast<T*>(dx9rend::queryFieldAt(query, retailOffset));
}

// sub_10004050 - read the occlusion result. Only a type-9 query with a live D3D object and the
// "issued" byte set actually calls GetData; on S_OK it stamps the renderer's frame ordinal at +0x10,
// clears "issued", and returns the new sample count. Otherwise the previous value at +0x14 is
// returned and, on a failed GetData, restored.
int queryReadOcclusionResult(void* query)
{
    IDirect3DQuery9* const d3d = queryField<IDirect3DQuery9*>(query, 0x08);
    int* const samples = &queryField<int>(query, 0x14);
    if (!d3d || queryField<int>(query, 0x0C) != 9)
        return -1;
    unsigned char& issued = queryField<unsigned char>(query, 0x19);
    if (issued)
    {
        const int previous = *samples;
        if (d3d->GetData(samples, 4, 0) == D3D_OK)
        {
            queryField<unsigned int>(query, 0x10) = dx9rend::rendererFrameOrdinal();
            const int result = *samples;
            issued = 0;
            return result;
        }
        *samples = previous;
    }
    return *samples;
}

// sub_10004120 - the frame ordinal stamped by the read above.
unsigned int queryResultFrame(void* query)
{
    return queryField<unsigned int>(query, 0x10);
}

// sub_100041A0 - the begin/end toggle. It reads the pending result first, then, if nothing is
// outstanding, issues D3DISSUE_BEGIN (2) when the toggle byte is clear and D3DISSUE_END (1) when it
// is set, flips the toggle, and marks "issued" on the END half. Two calls bracket a draw.
void queryToggleBeginEnd(void* query)
{
    queryReadOcclusionResult(query);
    unsigned char& issued = queryField<unsigned char>(query, 0x19);
    if (issued)
        return;
    IDirect3DQuery9* const d3d = queryField<IDirect3DQuery9*>(query, 0x08);
    if (!d3d)
        return;
    unsigned char& toggle = queryField<unsigned char>(query, 0x18);
    const bool wasEnd = (toggle != 0);
    d3d->Issue(static_cast<DWORD>((wasEnd ? 0 : 1) + 1));
    toggle = static_cast<unsigned char>(wasEnd ? 0 : 1);
    if (wasEnd)
        issued = 1;
}

// Returns the sky light's stored direction.
const Vec3* skyLightGetDirection(world::LightSource* light)
{
    return &light->getDirection();                                         // +0x17C
}

// The typed view of the two singletons the draws reach through the ClassManager and ModuleGeom.
IGeometryTemplateManager* templateManager()
{
    return static_cast<IGeometryTemplateManager*>(
        static_cast<void*>(g_GeometryTemplateManager));
}

// both draws look the cluster up by name every time they run, and both
// accept a null answer.
world::IHeightmapCluster* heightmapCluster()
{
    IBase* const found = classManager->getSingleton(
        renddx9abi::HostString("HeightmapCluster"));
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(found));
}

// The template chain the draws and setSkyTemplate/setFlareTemplate walk. Retail reads the offsets
// straight off the IGeometryTemplate* with NO type check, and so does this.
StaticMeshTemplate* asStaticMesh(IGeometryTemplate* t)
{
    return static_cast<StaticMeshTemplate*>(t);
}

// Binds template buffers and submits the material's indexed primitive range.
void submitIndexedGeometry(StaticMeshTemplate* template_, StaticMeshMaterial* material)
{
    dx9rend::rendererDrawIndexedPrimitive(material->m_primitiveCount,
                                          template_->m_primitiveType,
                                          0, 0,
                                          template_->m_vertexCount,
                                          0);
}

} // anonymous namespace


// Initializes sky resources in member order. Flare material and +0x180 remain untouched.

SkyDome::SkyDome()
    : m_renderPath(0)
    , m_refCount(1)
    , m_domeScale(100000.0f)
    , m_domeRotation(0.0f)
    , m_hasCloudLayer(0)
    , m_hasCloudLayer2(0)
    , m_cloudLayerCount(0)
    , m_skyTemplatePtr(0)
    , m_flareTemplatePtr(0)
    , m_cloudTemplatePtr(0)
    , m_skyTextureTex(0)
    , m_cloudTextureTex(0)
    , m_cloudTexture2Tex(0)
    , m_effect(0)
    , m_vertexDeclClouds(0)
    , m_vertexDeclNoClouds(0)
    , m_time(0.0f)
    , m_skyMaterial(0)
    , m_dword164(0)
    , m_flareQuery(0)
    , m_flareFadeAdd(0.0f)
    , m_flareFadeMul(1.0f)
    , m_flareAngleFadeAdd(-0.9f)
    , m_flareAngleFadeMul(10.0f)
    , m_flareTextureTex(0)
    , m_flareOcclusionReference(1)
    , m_flareOcclusionVisible(1)
    , m_flareOcclusionPending(1)
    , m_hasFlare(0)
    , m_flareOcclusionFrame(0)
    , m_flareOcclusionResultFrame(0)
    , m_flareOcclusionInterval(32)
    , m_lightingBlend(0.0f)
{
    // The retail x86 layout, checked here because the members are private and the class is
    // complete only inside its own bodies. Every access is by member name, so x64 keeps its own
    // offsets.
#if defined(_M_IX86)
    static_assert(offsetof(SkyDome, m_renderPath) == 0x008, "listener path word at +0x08");
    static_assert(offsetof(SkyDome, m_refCount) == 0x00C, "refcount at +0x0C");
    static_assert(offsetof(SkyDome, m_skyTemplate) == 0x024, "skyTemplate string at +0x24");
#endif

    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
    m_scrollDirection.x = 0.0f;
    m_scrollDirection.y = 0.0f;
    m_scrollDirection2.x = 0.0f;
    m_scrollDirection2.y = 0.0f;
    m_texOffset.x = 0.0f;
    m_texOffset.y = 0.0f;
    m_texOffset2.x = 0.0f;
    m_texOffset2.y = 0.0f;
    m_fadeCloudsDistances.x = 1000.0f;
    m_fadeCloudsDistances.y = 1100.0f;
    m_cloudLerpFactors.x = 0.5f;
    m_cloudLerpFactors.y = 0.5f;
    m_flareDirection.x = 0.0f;
    m_flareDirection.y = 0.0f;
    m_flareDirection.z = 0.0f;
    m_lightingColor.x = 1.0f;
    m_lightingColor.y = 1.0f;
    m_lightingColor.z = 1.0f;

    // the effect, then its twelve parameters in this exact order. Every one of them
    // takes the caller-facing extra reference sub_1001E420 adds, which is what the destructor
    // releases. No null guard on the effect: the parameter lookups dereference it directly.
    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "mods/xpack/shaders/SkyDome"));
    m_paramTexLayer0 = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER0"));
    m_paramTexLayer1 = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER1"));
    m_paramTexLayer2 = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER2"));
    m_paramTexOffset = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXOFFSET"));
    m_paramTexOffset2 = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXOFFSET2"));
    m_paramCloudsFadeOutDist =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("CLOUDSFADEOUTDIST"));
    m_paramCloudLerpFactors =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("CLOUDLERPFACTORS"));
    m_paramWorldViewProjection =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WorldViewProjection"));
    m_paramFlareParams =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("FLAREPARAMS"));
    m_paramFogColor = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("fogColor"));
    m_paramLightingBlend =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("LIGHTINGBLEND"));
    m_paramLightingColor =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("LIGHTINGCOLOR"));

    // register before the first technique selection.
    dx9rend::registerRenderPathListener(
        static_cast<dx9rend::RenderPathListener*>(this));

    // a direct call (MSVC devirtualises inside a constructor). Two cloud layers is the
    // constructed default, which with both cloud flags still false selects SkyDomeNV3xNoClouds.
    setCloudLayerCount(2);

    // the two vertex declarations. The D3DDECL_END element is built once
    // and pushed into both lists, exactly as shipped.
    D3DVERTEXELEMENT9 element;
    D3DVERTEXELEMENT9 endElement;
    endElement.Stream = 0xFF;
    endElement.Offset = 0;
    endElement.Type = D3DDECLTYPE_UNUSED;
    endElement.Method = 0;
    endElement.Usage = 0;
    endElement.UsageIndex = 0;

    std::vector<D3DVERTEXELEMENT9> cloudElements;
    element.Stream = 0;
    element.Offset = 0;
    element.Type = D3DDECLTYPE_FLOAT3;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_POSITION;
    element.UsageIndex = 0;
    cloudElements.push_back(element);
    element.Stream = 0;
    element.Offset = 12;
    element.Type = D3DDECLTYPE_FLOAT2;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_NORMAL;
    element.UsageIndex = 0;
    cloudElements.push_back(element);
    element.Stream = 0;
    element.Offset = 24;
    element.Type = D3DDECLTYPE_FLOAT2;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_TEXCOORD;
    element.UsageIndex = 0;
    cloudElements.push_back(element);
    element.Stream = 1;
    element.Offset = 0;
    element.Type = D3DDECLTYPE_FLOAT2;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_TEXCOORD;
    element.UsageIndex = 1;
    cloudElements.push_back(element);
    cloudElements.push_back(endElement);
    m_vertexDeclClouds = dx9rend::vertexDeclarationCreate(&cloudElements[0]);

    std::vector<D3DVERTEXELEMENT9> plainElements;
    element.Stream = 0;
    element.Offset = 0;
    element.Type = D3DDECLTYPE_FLOAT3;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_POSITION;
    element.UsageIndex = 0;
    plainElements.push_back(element);
    element.Stream = 0;
    element.Offset = 12;
    element.Type = D3DDECLTYPE_FLOAT2;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_NORMAL;
    element.UsageIndex = 0;
    plainElements.push_back(element);
    element.Stream = 0;
    element.Offset = 24;
    element.Type = D3DDECLTYPE_FLOAT2;
    element.Method = 0;
    element.Usage = D3DDECLUSAGE_TEXCOORD;
    element.UsageIndex = 0;
    plainElements.push_back(element);
    plainElements.push_back(endElement);
    m_vertexDeclNoClouds = dx9rend::vertexDeclarationCreate(&plainElements[0]);
}


// destruction - 0x10138530. The release order is the shipped one and it is NOT the reverse of
// construction: the two lighting parameters go first, then the occlusion query, then four textures
// in the order sky, flare, cloud2, cloud, then the remaining ten parameters in a scrambled order,
// then the effect, then the three templates (flare, sky, cloud), then the two vertex declarations,
// then the listener unregister, then the strings. Transcribed exactly.

SkyDome::~SkyDome()
{
    if (m_paramLightingColor)
        m_paramLightingColor->release();
    if (m_paramLightingBlend)
        m_paramLightingBlend->release();

    if (m_flareQuery)
    {
        // The Query has no release slot - QueryManager.cpp's three-slot table is {getRef, nullsub,
        // addRef}. Retail open-codes `if (!--refcount) { dtor; engineFree; }` here (sub_10003FA0 is
        // that destructor: it releases the IDirect3DQuery9 and drops the manager's counter).
        int* const refCount = &queryField<int>(m_flareQuery, 0x04);
        if (--*refCount == 0)
        {
            IDirect3DQuery9** const d3d = &queryField<IDirect3DQuery9*>(m_flareQuery, 0x08);
            if (*d3d)
            {
                (*d3d)->Release();
                *d3d = 0;
            }
            ::renddx9abi::engineFree(m_flareQuery);
        }
    }

    if (m_skyTextureTex)
        dx9rend::textureReleaseRef(m_skyTextureTex);
    if (m_flareTextureTex)
        dx9rend::textureReleaseRef(m_flareTextureTex);
    if (m_cloudTexture2Tex)
        dx9rend::textureReleaseRef(m_cloudTexture2Tex);
    if (m_cloudTextureTex)
        dx9rend::textureReleaseRef(m_cloudTextureTex);

    if (m_paramWorldViewProjection)
        m_paramWorldViewProjection->release();
    if (m_paramFogColor)
        m_paramFogColor->release();
    if (m_paramFlareParams)
        m_paramFlareParams->release();
    if (m_paramCloudsFadeOutDist)
        m_paramCloudsFadeOutDist->release();
    if (m_paramCloudLerpFactors)
        m_paramCloudLerpFactors->release();
    if (m_paramTexOffset2)
        m_paramTexOffset2->release();
    if (m_paramTexOffset)
        m_paramTexOffset->release();
    if (m_paramTexLayer2)
        m_paramTexLayer2->release();
    if (m_paramTexLayer1)
        m_paramTexLayer1->release();
    if (m_paramTexLayer0)
        m_paramTexLayer0->release();

    if (m_effect)
        dx9rend::effectRelease(m_effect);

    if (m_flareTemplatePtr)
        m_flareTemplatePtr->release();
    if (m_skyTemplatePtr)
        m_skyTemplatePtr->release();
    if (m_cloudTemplatePtr)
        m_cloudTemplatePtr->release();

    if (m_vertexDeclClouds)
        dx9rend::vertexDeclarationRelease(m_vertexDeclClouds);
    if (m_vertexDeclNoClouds)
        dx9rend::vertexDeclarationRelease(m_vertexDeclNoClouds);

    dx9rend::unregisterRenderPathListener(
        static_cast<dx9rend::RenderPathListener*>(this));

    // The seven embedded strings are destroyed by the compiler in reverse declaration order, which
    // is the order 0x1013A00C..0x1013A05B emits: +0x184, +0xB8, +0x9C, +0x80, +0x5C, +0x40, +0x24.
}


// THE IBase quartet

void SkyDome::addRef()                       // ICF_incrementDwordAt0xC, 0x10132630
{
    ++m_refCount;
}

int SkyDome::getRef()                        // ICF_getDwordAt0xC, 0x10051710
{
    return m_refCount;
}

int SkyDome::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~SkyDome();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* SkyDome::queryInterface(unsigned int iid) const
{
    if (iid != 1)
        return iid != 0xB801DCFBu ? 0 : const_cast<SkyDome*>(this);
    return const_cast<SkyDome*>(this);
}


// update - 0x10138790, primary +0x10. Renderer::renderFrame's frame-update call.

// m_time is accumulated and never read by anything in the image; the two texture offsets scroll and
// wrap. THE wrap IS NOT PER-component: either component reaching 16 subtracts 16 from both, so a
// pair with very different speeds drifts. Shipped, reproduced.

void SkyDome::update(float deltaTime)
{
    m_time = deltaTime + m_time;

    const float dy = deltaTime * m_scrollDirection.y;
    m_texOffset.x = deltaTime * m_scrollDirection.x + m_texOffset.x;
    m_texOffset.y = dy + m_texOffset.y;
    if (m_texOffset.x >= kTexOffsetWrap || m_texOffset.y >= kTexOffsetWrap)
    {
        m_texOffset.x = m_texOffset.x - kTexOffsetWrap;
        m_texOffset.y = m_texOffset.y - kTexOffsetWrap;
    }

    const float dy2 = deltaTime * m_scrollDirection2.y;
    m_texOffset2.x = deltaTime * m_scrollDirection2.x + m_texOffset2.x;
    m_texOffset2.y = dy2 + m_texOffset2.y;
    if (m_texOffset2.x >= kTexOffsetWrap || m_texOffset2.y >= kTexOffsetWrap)
    {
        m_texOffset2.x = m_texOffset2.x - kTexOffsetWrap;
        m_texOffset2.y = m_texOffset2.y - kTexOffsetWrap;
    }
}


void SkyDome::setPosition(const Vec3& position)
{
    m_position = position;
}

const Vec3& SkyDome::getPosition() const
{
    return m_position;
}

void SkyDome::setDomeScale(float scale)
{
    m_domeScale = scale;
}

float SkyDome::getDomeScale() const
{
    return m_domeScale;
}

void SkyDome::setDomeRotation(float degrees)
{
    m_domeRotation = degrees;
}

float SkyDome::getDomeRotation() const
{
    return m_domeRotation;
}

void SkyDome::setLightingBlend(float value)
{
    m_lightingBlend = value;
}

float SkyDome::getLightingBlend() const
{
    return m_lightingBlend;
}

void SkyDome::setLightingColor(const Vec3& color)
{
    m_lightingColor = color;
}

const Vec3& SkyDome::getLightingColor() const
{
    return m_lightingColor;
}

void SkyDome::setScrollDirection(const Vec2& direction)
{
    m_scrollDirection = direction;
}

const Vec2& SkyDome::getScrollDirection() const
{
    return m_scrollDirection;
}

void SkyDome::setScrollDirection2(const Vec2& direction)
{
    m_scrollDirection2 = direction;
}

const Vec2& SkyDome::getScrollDirection2() const
{
    return m_scrollDirection2;
}

void SkyDome::setFadeCloudsDistances(const Vec2& distances)
{
    m_fadeCloudsDistances = distances;
}

const Vec2& SkyDome::getFadeCloudsDistances() const
{
    return m_fadeCloudsDistances;
}

void SkyDome::setCloudLerpFactors(const Vec2& factors)
{
    m_cloudLerpFactors = factors;
}

const Vec2& SkyDome::getCloudLerpFactors() const
{
    return m_cloudLerpFactors;
}


// Selects the first geometry/LOD/material of a loaded sky template.
// Those containers and the material shader must exist; no guards are applied.
void SkyDome::setSkyTemplate(const std::string& name)
{
    m_skyTemplate = name;
    if (m_skyTemplatePtr)
        m_skyTemplatePtr->release();
    m_skyTemplatePtr = templateManager()->getTemplate(::renddx9abi::toHostString(m_skyTemplate));
    if (m_skyTemplatePtr)
    {
        m_skyTemplatePtr->addRef();
        if (!static_cast<MeshTemplate*>(m_skyTemplatePtr)->m_loaded)
            m_skyTemplatePtr->load();

        // Retail re-walks the geom/lod/material chain from m_skyTemplatePtr three times here
        // (0x10138C2A, 0x10138C4F, 0x10138C74) rather than caching it. The walk is pure reads, so
        // hoisting it changes nothing observable - including which dereference faults first.
        StaticMeshMaterial* const material = staticMeshFirstMaterial(asStaticMesh(m_skyTemplatePtr));
        if (!m_skyTextureTex)
        {
            // ShaderMaterial::textures[1] at record +0x2C - slot 0 is never filled, so the first
            // map of a mesh material lands in slot 1. The +0x08 entry of the shared three-slot
            // refcount table is addRef, not release.
            m_skyTextureTex = static_cast<dx9rend::EngineTexture*>(material->m_shaderMaterial->textures[1]);
            dx9rend::textureAddRef(m_skyTextureTex);
        }
        if (!m_cloudTextureTex)
        {
            m_cloudTextureTex = static_cast<dx9rend::EngineTexture*>(material->m_shaderMaterial->textures[2]);
            dx9rend::textureAddRef(m_cloudTextureTex);
        }
        m_skyMaterial = staticMeshFirstMaterial(asStaticMesh(m_skyTemplatePtr));
    }
    // outside the `if`: a cloud texture picked up above (or set earlier) switches the cloud layer
    // on through the virtual slot, which re-selects the technique.
    if (m_cloudTextureTex)
        setHasCloudLayer(true);
}

// setFlareTemplate - 0x10138B50, primary +0x84. Same shape as setSkyTemplate minus the texture
// harvesting, plus the flare-dirty reset before the lookup.
void SkyDome::setFlareTemplate(const std::string& name)
{
    m_flareTemplate = name;
    if (m_flareTemplatePtr)
        m_flareTemplatePtr->release();
    m_flareOcclusionReference = 1;
    m_flareTemplatePtr = templateManager()->getTemplate(::renddx9abi::toHostString(m_flareTemplate));
    if (m_flareTemplatePtr)
    {
        m_flareTemplatePtr->addRef();
        if (!static_cast<MeshTemplate*>(m_flareTemplatePtr)->m_loaded)
            m_flareTemplatePtr->load();
        m_flareMaterial = staticMeshFirstMaterial(asStaticMesh(m_flareTemplatePtr));
    }
}

const std::string& SkyDome::getSkyTemplate() const
{
    return m_skyTemplate;
}

const std::string& SkyDome::getFlareTemplate() const
{
    return m_flareTemplate;
}

// setCloudTemplate - 0x10138420, primary +0x4C.

// The shipped body ignores its argument and assigns the
// current skyTemplate string instead. `Skydome.cloudTemplate whatever` therefore resolves the SKY
// template again. Reproduced; `name` is deliberately unused.
void SkyDome::setCloudTemplate(const std::string& name)
{
    (void)name;
    m_cloudTemplate = m_skyTemplate;
    if (m_cloudTemplatePtr)
        m_cloudTemplatePtr->release();
    m_cloudTemplatePtr = templateManager()->getTemplate(::renddx9abi::toHostString(m_cloudTemplate));
    if (m_cloudTemplatePtr)
    {
        m_cloudTemplatePtr->addRef();
        if (!static_cast<MeshTemplate*>(m_cloudTemplatePtr)->m_loaded)
            m_cloudTemplatePtr->load();
    }
}

const std::string& SkyDome::getCloudTemplate() const
{
    return m_cloudTemplate;
}


// Texture setters release the previous texture, save the name, then load a replacement.
// The loader modifies its own string copy.

void SkyDome::setSkyTexture(const std::string& name)
{
    if (m_skyTextureTex)
        dx9rend::textureReleaseRef(m_skyTextureTex);
    m_skyTexture = name;
    m_skyTextureTex =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(m_skyTexture));
}

const std::string& SkyDome::getSkyTexture() const
{
    return m_skyTexture;
}

void SkyDome::setCloudTexture(const std::string& name)
{
    if (m_cloudTextureTex)
        dx9rend::textureReleaseRef(m_cloudTextureTex);
    m_cloudTexture = name;
    m_cloudTextureTex =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(m_cloudTexture));
}

const std::string& SkyDome::getCloudTexture() const
{
    return m_cloudTexture;
}

void SkyDome::setCloudTexture2(const std::string& name)
{
    if (m_cloudTexture2Tex)
        dx9rend::textureReleaseRef(m_cloudTexture2Tex);
    m_cloudTexture2 = name;
    m_cloudTexture2Tex =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(m_cloudTexture2));
}

const std::string& SkyDome::getCloudTexture2() const
{
    return m_cloudTexture2;
}

// setFlareTexture - 0x10138920, primary +0x8C. The only texture setter with a shape of its own:
// an empty name simply clears m_hasFlare and does nothing else (the old wrapper is still released
// first), and a non-empty name adopts the stock "flaresprite" geometry when no flare template has
// been chosen yet - through the virtual +0x84 slot, not the member function.
void SkyDome::setFlareTexture(const std::string& name)
{
    if (m_flareTextureTex)
        dx9rend::textureReleaseRef(m_flareTextureTex);

    if (name.size() != 0)
    {
        if (!m_flareTemplatePtr)
            setFlareTemplate(std::string("flaresprite"));
        m_flareOcclusionReference = 1;
        m_flareTextureName = name;
        m_flareTextureTex = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(m_flareTextureName));
        if (m_flareTextureTex)
            m_hasFlare = 1;
    }
    else
    {
        m_hasFlare = 0;
    }
}

// getFlareTexture - 0x101382D0, primary +0x90. See the header: the read has a side effect.
const std::string& SkyDome::getFlareTexture()
{
    m_flareOcclusionReference = 1;
    return m_flareTextureName;
}


// setHasCloudLayer - 0x10138380, primary +0x3C. Clearing layer 1 also clears layer 2, and both
// arms re-apply the current layer count so the technique follows. Both inner calls go through the
// vtable in the shipped body.
void SkyDome::setHasCloudLayer(bool value)
{
    m_hasCloudLayer = static_cast<unsigned char>(value);
    if (!value)
        setHasCloudLayer2(false);
    setCloudLayerCount(m_cloudLayerCount);
}

bool SkyDome::getHasCloudLayer() const
{
    return m_hasCloudLayer != 0;
}

// setHasCloudLayer2 - 0x101383C0, primary +0x44. Layer 2 cannot be enabled without layer 1.
void SkyDome::setHasCloudLayer2(bool value)
{
    if (value && getHasCloudLayer())
        m_hasCloudLayer2 = 1;
    else
        m_hasCloudLayer2 = 0;
    setCloudLayerCount(m_cloudLayerCount);
}

bool SkyDome::getHasCloudLayer2() const
{
    return m_hasCloudLayer2 != 0;
}

// setCloudLayerCount - 0x101382F0, primary +0x94. Stores the count and picks the technique. Note
// there is NO getter slot for this field; the count is only observable through the technique.
// The repeated getHasCloudLayer() dispatch in the second test is the shipped shape.
void SkyDome::setCloudLayerCount(int count)
{
    m_cloudLayerCount = count;
    if (getHasCloudLayer() && getHasCloudLayer2() && m_cloudLayerCount > 1)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xDualClouds"));
        return;
    }
    if (!getHasCloudLayer() || !m_cloudLayerCount)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xNoClouds"));
        return;
    }
    dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3x"));
}


// dx9rend::RenderPathListener - secondary table 0x1021D390

void SkyDome::onRenderPathActivated(int path)
{
    m_renderPath = path;
}

void SkyDome::onRenderPathDeactivated()                  // nullsub_15
{
}

void SkyDome::onRenderPathChanged(int path)              // nullsub_53
{
    (void)path;
}

int SkyDome::getCurrentRenderPath() const                // 0x101A9A10 on the subobject
{
    return m_renderPath;
}

std::string SkyDome::getRenderPathListenerName() const
{
    return std::string("SkyDome");
}


// Draws the dome and clouds using the camera rotation and a temporary far plane of 2500.
// Dome scaling also scales its translation before projection.

#pragma warning(push)
// C4700/C4701 are correct here and are silenced only because the defect they report is the shipped
// behaviour this body exists to reproduce. See the waterLevel note inside.
#pragma warning(disable : 4700 4701)

void SkyDome::onRenderPathNotification(int unused)
{
    (void)unused;

    if (!m_skyTemplatePtr)
        return;

    // the once-only sub_10047F20("SkyDome") stat
    // registration (guard bit 1 of g_domeOnceFlags) and the sub_10046EE0/sub_10046EF0 bracket on
    // unk_1025D168. Stat-only scaffolding; not reproduced. See the file header.

    m_paramLightingBlend->setFloat(m_lightingBlend);
    m_paramLightingColor->setValue(&m_lightingColor, 0);

    // the fog colour, xyz from the RaShaderConstants block, w forced to 1.
    // (The shipped body also spills the blue channel into an adjacent stack slot that is later
    // reused for the scale vector; a dead store, not reproduced.)
    Vec4 fogColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vec3* const fog = raShaderConstantsFogColor();
    fogColor.x = fog->x;
    fogColor.y = fog->y;
    fogColor.z = fog->z;
    fogColor.w = 1.0f;
    m_paramFogColor->setValue(&fogColor, 0);

    // waterLevel remains uninitialized when no heightmap cluster exists.
    float waterLevel;
    world::IHeightmapCluster* const cluster = heightmapCluster();
    if (cluster)
        waterLevel = cluster->getSeaWaterLevel();

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
    const Mat4* const viewMatrix = view->getViewMatrixPtr();
    const Vec3* const cameraPosition = reinterpret_cast<const Vec3*>(&viewMatrix->m[12]);

    if (cameraPosition->y < waterLevel)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeUnderWater"));
    }
    else if (getHasCloudLayer() && getHasCloudLayer2() && m_cloudLayerCount > 1)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xDualClouds"));
    }
    else if (getHasCloudLayer() && m_cloudLayerCount)
    {
        if (m_lightingBlend > 0.0f)
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xLit"));
        else
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3x"));
    }
    else
    {
        if (m_lightingBlend > 0.0f)
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xNoCloudsLit"));
        else
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeNV3xNoClouds"));
    }

    // guard bit 2 zeroes the scratch block that is never read. See the declaration.
    if ((g_domeOnceFlags & 2) == 0)
    {
        g_domeOnceFlags |= 2u;
        g_domeScratchUnused.x = 0.0f;
        g_domeScratchUnused.y = 0.0f;
        g_domeScratchUnused.z = 0.0f;
        g_domeScratchUnused.w = 0.0f;
    }

    // a second render-view fetch; the shipped body does not reuse the one above.
    dx9rend::IRenderView* const drawView = dx9rend::rendererGetRenderView();

    Mat4 world = Mat4::getIdentity();
    setRotateYDeg(world, m_domeRotation);
    world.m[12] += m_position.x;
    world.m[13] += m_position.y;
    world.m[14] += m_position.z;

    Vec3 scale;
    scale.x = m_domeScale;
    scale.y = m_domeScale;
    scale.z = m_domeScale;
    mat4ScaleColumns(world, scale);

    const Mat4 inverseView = *drawView->getInverseViewMatrixPtr();
    const Mat4 worldBeforeRotate = world;
    world.rotMult(worldBeforeRotate, inverseView);

    const float savedFarPlane = drawView->getFarPlane();
    drawView->setFarPlane(kSkyFarPlane);
    const Mat4* const linearProjection = drawView->getLinearProjectionMatrixPtr();
    const Mat4 worldBeforeProject = world;

    // Full multiplication preserves the projection's perspective column.
    world.fullMult(worldBeforeProject, *linearProjection);
    drawView->setFarPlane(savedFarPlane);

    world.m[15] = 1.0f;
    m_paramWorldViewProjection->setValue(&world, 0);

    dx9rend::stateManagerPush();
    dx9rend::effectBegin(m_effect);

    m_paramTexLayer0->setValue(m_skyTextureTex, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclClouds);

    StaticMeshTemplate* const skyTemplate = asStaticMesh(m_skyTemplatePtr);
    dx9rend::vertexBufferBindStream(skyTemplate->m_vertexStream, 0);
    dx9rend::vertexBufferBindStream(skyTemplate->m_uvStreams[0], 1);

    if (m_hasCloudLayer && m_cloudLayerCount)
    {
        m_paramTexLayer1->setValue(m_cloudTextureTex, 0);

        if ((g_domeOnceFlags & 4) == 0)
        {
            g_domeOnceFlags |= 4u;
            g_texOffsetConstant.x = 0.0f;
            g_texOffsetConstant.y = 0.0f;
            g_texOffsetConstant.z = 0.0f;
            g_texOffsetConstant.w = 0.0f;
        }
        g_texOffsetConstant.x = m_texOffset.x;
        g_texOffsetConstant.y = m_texOffset.y;
        m_paramTexOffset->setValue(&g_texOffsetConstant, 0);

        if (m_hasCloudLayer2 && m_cloudLayerCount > 1)
        {
            m_paramTexLayer2->setValue(m_cloudTexture2Tex, 0);

            if ((g_domeOnceFlags & 8) == 0)
            {
                g_domeOnceFlags |= 8u;
                g_texOffset2Constant.x = 0.0f;
                g_texOffset2Constant.y = 0.0f;
                g_texOffset2Constant.z = 0.0f;
                g_texOffset2Constant.w = 0.0f;
            }
            g_texOffset2Constant.x = m_texOffset2.x;
            g_texOffset2Constant.y = m_texOffset2.y;
            m_paramTexOffset2->setValue(&g_texOffset2Constant, 0);

            if ((g_domeOnceFlags & 0x10) == 0)
            {
                g_domeOnceFlags |= 0x10u;
                g_cloudLerpConstant.x = 0.0f;
                g_cloudLerpConstant.y = 0.0f;
                g_cloudLerpConstant.z = 0.0f;
                g_cloudLerpConstant.w = 0.0f;
            }
            g_cloudLerpConstant.x = m_cloudLerpFactors.x;
            g_cloudLerpConstant.y = m_cloudLerpFactors.y;
            m_paramCloudLerpFactors->setValue(&g_cloudLerpConstant, 0);
        }

        if ((g_domeOnceFlags & 0x20) == 0)
        {
            g_domeOnceFlags |= 0x20u;
            g_cloudsFadeOutConstant.x = 0.0f;
            g_cloudsFadeOutConstant.y = 0.0f;
            g_cloudsFadeOutConstant.z = 0.0f;
            g_cloudsFadeOutConstant.w = 0.0f;
        }
        // x = the near fade distance, y = the span, not the far distance.
        g_cloudsFadeOutConstant.x = m_fadeCloudsDistances.x;
        g_cloudsFadeOutConstant.y = m_fadeCloudsDistances.y - m_fadeCloudsDistances.x;
        m_paramCloudsFadeOutDist->setValue(&g_cloudsFadeOutConstant, 0);
    }

    dx9rend::indexBufferBind(skyTemplate->m_indexBuffer);
    dx9rend::effectBeginPass(m_effect, 0);
    submitIndexedGeometry(skyTemplate, m_skyMaterial);
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();

    // the profiler scope close; see above.
}

#pragma warning(pop)


// submitFlarePass - the bind-and-submit sequence retail inlines at both flare draw sites.

void SkyDome::submitFlarePass()
{
    dx9rend::stateManagerPush();
    dx9rend::effectBegin(m_effect);
    m_paramTexLayer0->setValue(m_flareTextureTex, 0);
    dx9rend::vertexDeclarationBind(m_vertexDeclNoClouds);

    StaticMeshTemplate* const flareTemplate = asStaticMesh(m_flareTemplatePtr);
    dx9rend::vertexBufferBindStream(flareTemplate->m_vertexStream, 0);
    dx9rend::indexBufferBind(flareTemplate->m_indexBuffer);
    dx9rend::effectBeginPass(m_effect, 0);
    submitIndexedGeometry(flareTemplate, m_flareMaterial);
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
}


// Mode 1 draws flare occlusion, mode 2 draws the visible flare; nonpositive modes do nothing.
// The underwater early return leaves the temporary far plane in place.

void SkyDome::drawFlare(int mode)
{
    // guard bit 1 of g_flareOnceFlags initialises the constant that is never read.
    if ((g_flareOnceFlags & 1) == 0)
    {
        g_flareOnceFlags |= 1u;
        g_flareUnusedConstant = 1.5707964f;
    }

    if (!m_hasFlare || !m_flareTemplatePtr || mode <= 0)
        return;

    Vec3 flareDirection;
    flareDirection.x = 0.0f;
    flareDirection.y = 0.0f;
    flareDirection.z = 0.0f;

    // guard bit 2 plus the sub_10046EE0 bracket on unk_1025D3D8. The
    // same stat-only scaffolding as the dome draw; not reproduced.

    if (!m_flareQuery)
        m_flareQuery = dx9rend::queryManagerCreateQuery(9);   // D3DQUERYTYPE_OCCLUSION

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();

    float waterLevel = -1000.0f;
    world::IHeightmapCluster* const cluster = heightmapCluster();
    if (cluster)
        waterLevel = cluster->getSeaWaterLevel();

    const Mat4* const cameraMatrix = view->getViewMatrixPtr();
    Vec3 cameraPosition;
    cameraPosition.x = cameraMatrix->m[12];
    cameraPosition.y = cameraMatrix->m[13];
    cameraPosition.z = cameraMatrix->m[14];
    if (cameraPosition.y < waterLevel)
        return;   // the far plane has not been touched yet, so there is nothing to restore

    // an explicitly configured flare direction wins; otherwise the sky light supplies
    // one. The test is the SUM of the three components against 0.01, evaluated z + y + x, and a
    // missing sky light leaves the direction at (0,0,0).
    if (m_flareDirection.z + m_flareDirection.y + m_flareDirection.x >= kFlareDirectionEpsilon)
    {
        Vec3 negated;
        vec3Negate(negated, m_flareDirection);
        flareDirection = negated;
    }
    else
    {
        world::LightSource* const skyLight = g_LightManager->getSkyLight();
        if (skyLight)
            flareDirection = *skyLightGetDirection(skyLight);
    }

    // the billboard basis. Rows 0/1/2 of flareWorld are the quad's axes.
    Mat4 flareWorld = Mat4::getIdentity();
    Vec3* const basisX = reinterpret_cast<Vec3*>(&flareWorld.m[0]);
    Vec3* const basisY = reinterpret_cast<Vec3*>(&flareWorld.m[4]);
    Vec3* const basisZ = reinterpret_cast<Vec3*>(&flareWorld.m[8]);

    if (g_flareUseUpVectorBasis)
    {
        // Unreachable in the shipped DLL - byte_1025D3D4 is never written. Transcribed anyway.
        basisY->x = 0.0f;
        basisY->y = 1.0f;
        basisY->z = 0.0f;
        vec3Negate(*basisZ, *reinterpret_cast<const Vec3*>(&view->getViewMatrixPtr()->m[8]));
        vec3Cross(*basisX, *basisY, *basisZ);
        vec3Cross(*basisY, *basisZ, *basisX);
    }
    else
    {
        const Mat4* const m1 = view->getViewMatrixPtr();
        basisY->x = m1->m[4];
        basisY->y = m1->m[5];
        basisY->z = m1->m[6];
        const Mat4* const m2 = view->getViewMatrixPtr();
        vec3Negate(*basisZ, *reinterpret_cast<const Vec3*>(&m2->m[8]));
        const Mat4* const m3 = view->getViewMatrixPtr();
        vec3Negate(*basisX, *reinterpret_cast<const Vec3*>(&m3->m[0]));
    }

    // push the flare out along its direction, scaled by the dome and by -0.001.
    flareDirection.x = flareDirection.x * m_domeScale;
    flareDirection.y = flareDirection.y * m_domeScale;
    flareDirection.z = flareDirection.z * m_domeScale;
    flareDirection.x = flareDirection.x * g_flareDistanceScale;
    flareDirection.y = flareDirection.y * g_flareDistanceScale;
    flareDirection.z = flareDirection.z * g_flareDistanceScale;

    const Mat4* const placementMatrix = view->getViewMatrixPtr();
    Vec3 flarePosition;
    flarePosition.x = flareDirection.x + placementMatrix->m[12];
    flarePosition.y = flareDirection.y + placementMatrix->m[13];
    flarePosition.z = flareDirection.z + placementMatrix->m[14];
    flareWorld.m[12] = flarePosition.x;
    flareWorld.m[13] = flarePosition.y;
    flareWorld.m[14] = flarePosition.z;

    // on the "check" frame of the occlusion pass the quad is parked straight in front
    // of the camera instead, along the view's own Z, so the query measures an unoccluded reference.
    if (mode == 1 && !(m_flareOcclusionFrame % m_flareOcclusionInterval))
    {
        const Mat4* const axisMatrix = view->getViewMatrixPtr();
        const Mat4* const originMatrix = view->getViewMatrixPtr();
        Vec3 scaled;
        vec3Scale(scaled, *reinterpret_cast<const Vec3*>(&axisMatrix->m[8]), m_domeScale);
        Vec3 offset;
        vec3Scale(offset, scaled, -g_flareDistanceScale);
        Vec3 placed;
        vec3Add(placed, *reinterpret_cast<const Vec3*>(&originMatrix->m[12]), offset);
        flareWorld.m[12] = placed.x;
        flareWorld.m[13] = placed.y;
        flareWorld.m[14] = placed.z;
    }

    // The far plane is forced after the view-projection multiply here, unlike the dome draw,
    // which forces it before fetching its projection matrix. Shipped order, kept.
    const Mat4 viewProjection = *view->getViewProjectionMatrixPtr();
    mat4MultiplyInPlace(flareWorld, viewProjection);
    const float savedFarPlane = view->getFarPlane();
    view->setFarPlane(kSkyFarPlane);
    m_paramWorldViewProjection->setValue(&flareWorld, 0);

    if (mode == 2)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeSunFlare"));

        Vec4 flareParams = { 0.0f, 0.0f, 0.0f, 0.0f };

        Vec3 toFlare;
        toFlare.x = flarePosition.x - cameraPosition.x;
        toFlare.y = flarePosition.y - cameraPosition.y;
        toFlare.z = flarePosition.z - cameraPosition.z;
        vec3Normalize(toFlare);

        const Mat4* const facingMatrix = view->getViewMatrixPtr();
        const float facing = toFlare.z * facingMatrix->m[10] +
                             toFlare.y * facingMatrix->m[9] +
                             toFlare.x * facingMatrix->m[8];

        if (m_flareQuery)
        {
            const int samples = queryReadOcclusionResult(m_flareQuery);
            if (queryResultFrame(m_flareQuery) <= m_flareOcclusionResultFrame)
            {
                // No new result. After ten renderer frames, force the occlusion pass to run again.
                if (m_flareOcclusionResultFrame + 10 < dx9rend::rendererFrameOrdinal())
                    m_flareOcclusionPending = 1;
            }
            else
            {
                const int frame = m_flareOcclusionFrame;
                if (frame % m_flareOcclusionInterval)
                    m_flareOcclusionVisible = samples;
                else
                    m_flareOcclusionReference = samples;
                m_flareOcclusionFrame = frame + 1;
                m_flareOcclusionPending = 1;
                m_flareOcclusionResultFrame = queryResultFrame(m_flareQuery);
            }

            // integer divide materialised on the x87 stack (fild/fidiv), so a zero reference
            // count divides by zero exactly as retail does.
            const float ratio = static_cast<float>(
                (static_cast<double>(m_flareOcclusionVisible) /
                 static_cast<double>(m_flareOcclusionReference) + m_flareFadeAdd) * m_flareFadeMul);
            flareParams.x = clampFloat(ratio, 0.0f, 1.0f);
            if (g_flareAngleFadeEnabled)
            {
                const float angle = (facing + m_flareAngleFadeAdd) * m_flareAngleFadeMul;
                flareParams.x = clampFloat(angle, 0.0f, 1.0f) * flareParams.x;
            }
        }
        else
        {
            flareParams.x = 1.0f;
        }

        m_paramFlareParams->setValue(&flareParams, 0);
        submitFlarePass();
        view->setFarPlane(savedFarPlane);
        return;
    }

    if (mode == 1 && m_flareOcclusionPending)
    {
        if (m_flareOcclusionFrame % m_flareOcclusionInterval)
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeFlareOcclude"));
        else
            dx9rend::effectSetTechniqueByName(m_effect, std::string("SkyDomeFlareOccludeCheck"));

        queryToggleBeginEnd(m_flareQuery);
        submitFlarePass();
        queryToggleBeginEnd(m_flareQuery);
        m_flareOcclusionPending = 0;

        // the profiler scope close, on this path only. Not reproduced.
    }

    view->setFarPlane(savedFarPlane);
}


// Allocation failure reaches an unguarded queryInterface call.

IBase* __fastcall createSkyDome(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(SkyDome));
    SkyDome* const self = storage ? new (storage) SkyDome() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
