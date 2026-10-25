// Editable road construction, terrain projection and compiled output.
// Tessellation helpers are included from RoadEditableTessellate.inl.


#ifdef BF2_IMAGE_RENDDX9

#include "RoadEditable.h"
#include "RoadEditableRenderer.h"             // the queue's mode word (+0x18) draw reads
#include "RoadTemplate.h"                     // IRoadTemplate

#include "../Core/EventManager.h"             // eventManager's methods, ECEditor
#include "../GeomMisc/EditorSpline.h"         // IEditorSpline
#include "../GeomMisc/PerlinNoise.h"          // IPerlinNoise
#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"      // world::IHeightmapCluster (+0x68)
#include "../Physics/HeightmapClusterEditable.h"   // world::IHeightmapClusterEditable (+0x34)
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"           // the effect / state / draw / settings seams
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager, fileManager, eventManager
#include "../../RendDX9Abi/Vc71String.h"

#if BF2_PLUGIN_ABI_NATIVE
#include "HeightmapEventNode.h"               // the native host's EventNode shape
#endif

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sstream>
#include <string>
#include <vector>

// The D3DX entry points this TU calls - real static imports of the shipped image (d3dx9_25.dll).
// D3DX exports undecorated names; /alternatename maps this compiler's decorated references onto them
// (see Code/RendDX9Abi/D3dx9_25.def). The Vec3 parameters are D3DXVECTOR3* (three floats).
extern "C" HRESULT WINAPI D3DXComputeBoundingBox(
    const ::dice::hfe::Vec3* firstPosition, DWORD vertexCount, DWORD stride,
    ::dice::hfe::Vec3* minimum, ::dice::hfe::Vec3* maximum);
extern "C" HRESULT WINAPI D3DXComputeBoundingSphere(
    const ::dice::hfe::Vec3* firstPosition, DWORD vertexCount, DWORD stride,
    ::dice::hfe::Vec3* centre, float* radius);
extern "C" HRESULT WINAPI D3DXCreateRenderToSurface(
    IDirect3DDevice9* device, UINT width, UINT height, D3DFORMAT format, BOOL depthStencil,
    D3DFORMAT depthStencilFormat, void** renderToSurface);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    IDirect3DSurface9* destination, const PALETTEENTRY* destinationPalette,
    const RECT* destinationRect, IDirect3DSurface9* source,
    const PALETTEENTRY* sourcePalette, const RECT* sourceRect,
    DWORD filter, D3DCOLOR colourKey);
extern "C" HRESULT WINAPI D3DXSaveSurfaceToFileA(
    const char* destinationFile, unsigned int fileFormat, IDirect3DSurface9* source,
    const PALETTEENTRY* sourcePalette, const RECT* sourceRect);

#pragma comment(linker, "/alternatename:_D3DXComputeBoundingBox@20=_D3DXComputeBoundingBox")
#pragma comment(linker, "/alternatename:_D3DXComputeBoundingSphere@20=_D3DXComputeBoundingSphere")
#pragma comment(linker, "/alternatename:_D3DXCreateRenderToSurface@28=_D3DXCreateRenderToSurface")
#pragma comment(linker, "/alternatename:_D3DXLoadSurfaceFromSurface@32=_D3DXLoadSurfaceFromSurface")
#pragma comment(linker, "/alternatename:_D3DXSaveSurfaceToFileA@20=_D3DXSaveSurfaceToFileA")

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_bufferManager;       // dword_1023B978 - BufferManager.cpp
extern void* g_shaderManager;       // dword_1023B97C - ShaderManager.cpp
extern void* g_rendererBase;        // dword_1023B970 - the base renderer (RendDX9Base)
extern void* g_deviceCaps;          // dword_1023B980 - D3DCAPS9 at offset 0 (RendDX9Init.cpp)
extern void* g_raShaderConstants;   // dword_1023DDBC - RenderQueues.cpp

// BufferManager.cpp / StateManager.cpp, declared per TU as every other client does.
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const vc71::string& name, unsigned int usage);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexBufferRelease(void* wrapper);                                    // --ref, sub_10014510, free
int   bufferManagerSwapPool(int value);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferBind(void* wrapper);
void  indexBufferRelease(void* wrapper);                                     // --ref, sub_10014300, free
void  bufferManagerRemoveVertexBuffer(void* wrapper);
void  bufferManagerRemoveIndexBuffer(void* wrapper);
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);
} // namespace dx9rend

namespace geom {

extern IBase* g_RoadEditableRenderer;               // ModuleGeom.cpp - the "RoadEditableRenderer" singleton

// RoadCompiledRenderer.cpp - the singleton's depth-bias pair (+0x90 / +0x94) as the bit patterns the
// StateManager is handed. Unguarded, as RoadEditable's draw reads them (census hazard 7).
unsigned int roadCompiledRendererDepthBiasBits();
unsigned int roadCompiledRendererSlopeScaleDepthBiasBits();

// dword_1021AEB8 - 821011, read as data by getClassList (0x100C83FA).
extern const unsigned int CID_RoadEditable = 821011u;

namespace {

// dword_1021AE24 / dword_1021AE28 / dword_10208DC0 - the ids queryInterface tests after IBase's 1.
const unsigned int kIID_IRoad             = 820010u;
const unsigned int kIID_IRoadEditable     = 820011u;
const unsigned int kIID_IConsoleSaveable  = 0x3709A4FFu;
// dword_1021AE18 / dword_1021AE1C - the Terrain lookup compile makes (ITerrain, ITerrainEditable).
const unsigned int kIID_ITerrain          = 820004u;
const unsigned int kIID_ITerrainEditable  = 820005u;
// dword_10223028 - IHeightmapClusterEditable.
const unsigned int kIID_IHeightmapClusterEditable = 820003u;

// The Ra constants block's holders draw reads (the RoadCompiledRenderer.cpp offsets).
const unsigned int kRaFogRange = 0xA8;
const unsigned int kRaFogColor = 0xBC;

// The editor's event ids under ECEditor (census §4.4).
const unsigned int kEventSplineChanged         = 200;
const unsigned int kEventSplineReconstructed   = 206;
const unsigned int kEventRoadTemplateChanged   = 300;
const unsigned int kEventRoadTemplateDetached  = 301;
const unsigned int kEventRoadNoiseHasChanged   = 302;
const unsigned int kEventRoadPropertiesChanged = 303;
const unsigned int kEventBuildFinalRoad        = 304;
const unsigned int kEventBuildGhostRoad        = 305;
const unsigned int kEventHeightmapModified     = 100;

unsigned char* raConstants()
{
    return static_cast<unsigned char*>(dx9rend::g_raShaderConstants);
}

dx9rend::RendDX9Base* rendererBase()
{
    return static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
}

// sub_10001170 on dword_1023B980 - `mov eax, [ecx+5Ch]`: D3DCAPS9::MaxTextureHeight. A field of the
// image's own caps record, read at its offset (the RendDX9Init.cpp accessors' convention).
int deviceCapsMaxTextureHeight()
{
    return *reinterpret_cast<const int*>(static_cast<const unsigned char*>(dx9rend::g_deviceCaps) + 0x5C);
}

// PE 0x1000ED40 (34 B, stdcall), read in full: `fcomi high; fcmovnb` then `fcomi low; fcmovb` - the
// high bound first, so a NaN ends at the low bound. Each TU that needs it carries its own copy.
float clampRange(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

world::IHeightmapCluster* heightmapCluster()
{
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(
        ::dice::hfe::classManager->getSingleton(::renddx9abi::HostString("HeightmapCluster"))));   // +0x48
}

// ClassManager +0x4C (ELF +0x50 _ZN...14getSingletonAsERKSsj, `void* getSingletonAs(const
// std::string&, unsigned int)`), compile's 0x1013C1AB. RAW in both flavours, as
// UndergrowthSystemLoad.cpp's copy of this seam: HfeAbi.h's IClassManager - which both flavours
// dispatch through, with the HostString carrier - still declares the slot reserved_getSingletonAs().
// [CHOSEN, not recovered] seam (RW-268 / RW-275) - retire both copies by typing the slot in HfeAbi.h.
void* classManagerGetSingletonAs(const ::renddx9abi::HostString& name, unsigned int iid)
{
    typedef void* (__thiscall* GetSingletonAsFn)(void*, const ::renddx9abi::HostString&, unsigned int);
    void* const manager = ::dice::hfe::classManager;
    return reinterpret_cast<GetSingletonAsFn>((*reinterpret_cast<void***>(manager))[0x4C / 4])(
        manager, name, iid);
}

// ID3DXRenderToSurface, in d3dx9core.h's slot order (the SDK header is not part of this build).
struct RenderToSurface : public IUnknown
{
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** device) = 0;                     // +0x0C
    virtual HRESULT __stdcall GetDesc(void* description) = 0;                               // +0x10
    virtual HRESULT __stdcall BeginScene(IDirect3DSurface9* surface,
                                         const D3DVIEWPORT9* viewport) = 0;                 // +0x14
    virtual HRESULT __stdcall EndScene(DWORD mipFilter) = 0;                                // +0x18
    virtual HRESULT __stdcall OnLostDevice() = 0;                                           // +0x1C
    virtual HRESULT __stdcall OnResetDevice() = 0;                                          // +0x20
};

#if !BF2_PLUGIN_ABI_NATIVE

// Compatibility event node with the retail host's 0x40-byte layout and affected-area payload.
// [CHOSEN] Host-owned fields remain opaque; native builds use geom::HeightmapEventNode.
// RW-275: remove this copy when EventNode matches the retail host layout.

class RetailHeightmapEventNode
{
public:
    explicit RetailHeightmapEventNode(unsigned int eventType)
        : next(0)
        , pad24(0)
        , type(eventType)
        , pad2C(0)
    {
        std::memset(hostOwned, 0, sizeof(hostOwned));
        area.x = 0.0f;
        area.y = 0.0f;
        area.z = 0.0f;
        area.w = 0.0f;
    }
    virtual ~RetailHeightmapEventNode() {}
    virtual void release() { delete this; }
    virtual int  getType() const { return static_cast<int>(type); }

    void setAffectedArea(const Vec4& value) { area = value; }

    unsigned char hostOwned[0x1C];   // +0x04
    void*         next;              // +0x20
    unsigned int  pad24;             // +0x24
    unsigned int  type;              // +0x28
    unsigned int  pad2C;             // +0x2C
    Vec4          area;              // +0x30
};

static_assert(sizeof(RetailHeightmapEventNode) == 0x40, "0x10140633 allocates 0x40");
#endif

} // anonymous namespace

#if !BF2_PLUGIN_ABI_NATIVE

// RoadTemplateNameSet's destructor - the callee-side destruction of +0x84's by-value set
// (0x1013D0D0): erase(begin(), end()) (the whole range, so _Erase from the root,
// right subtree first, then the node's string, the node, and on to the left), then the
// head freed (`push [ebp+arg_8]; call sub_1005F360`).

namespace {
void eraseNameSubtree(RoadTemplateNameSetNode* node)
{
    while (!node->isNil)
    {
        eraseNameSubtree(node->right);
        RoadTemplateNameSetNode* const left = node->left;
        reinterpret_cast<vc71::string*>(static_cast<void*>(node->value))->~string();
        ::renddx9abi::engineFree(node);
        node = left;
    }
}
} // anonymous namespace

RoadTemplateNameSet::~RoadTemplateNameSet()
{
    if (!head)
        return;
    eraseNameSubtree(head->parent);
    ::renddx9abi::engineFree(head);
}
#endif


// Initializes a ghost road, registers its terrain-event handler, and acquires effect handles.
// The triangle count and vector allocator word remain uninitialized until geometry is built.
RoadEditable::RoadEditable()
    : m_refCount(1)
    , m_template(0)
    , m_spline(0)
    , m_edge0(0)
    , m_edge1(0)
    , m_noise(0)
    , m_vertexCount(0)
    , m_vertexBuffer(0)
    , m_indexBuffer(0)
    , m_texture0(0)
    , m_texture1(0)
    , m_paramFogRange(0)
    , m_paramFogColor(0)
    , m_radius(0.0f)
    , m_ghost(1)
    , m_priority(0)
{
    m_centre.x = 0.0f;
    m_centre.y = 0.0f;
    m_centre.z = 0.0f;
    std::memset(&m_bounds, 0, sizeof(m_bounds));
    m_bounds.dirty = 1;
    m_padF1[0] = m_padF1[1] = m_padF1[2] = 0;
    m_fadeDistAtStartLeft.x = 0.0f;
    m_fadeDistAtStartLeft.y = 3.0f;
    m_fadeDistAtStartRight.x = 0.0f;
    m_fadeDistAtStartRight.y = 3.0f;
    m_fadeDistAtEndLeft.x = 0.0f;
    m_fadeDistAtEndLeft.y = 3.0f;
    m_fadeDistAtEndRight.x = 0.0f;
    m_fadeDistAtEndRight.y = 3.0f;

    ::dice::hfe::eventManager->registerEventHandler(ECEditor, static_cast<IEventListener*>(this), 0);

    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/Road"));
    m_paramWorldViewProjection =
        dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("WorldViewProjection"));
    m_paramViewPos = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("ViewPos"));
    m_paramTexLayer0 = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("TEXLAYER0"));
    m_paramTexLayer1 = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("TEXLAYER1"));
    m_paramBlendFactor = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("BlendFactor"));
    m_paramMaterial = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("Material"));
    m_paramFogRange = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("fogRange"));
    m_paramFogColor = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("fogColor"));

    Vec2 zero;
    zero.x = 0.0f;
    zero.y = 0.0f;
    m_textureScaleOffsets.push_back(zero);
    zero.x = 0.0f;
    zero.y = 0.0f;
    m_textureScaleOffsets.push_back(zero);
}

// The vptrs restored; RoadEditableRenderer +0x14 remove(this) and EventManager +0x30
// unregister (category 8) - neither singleton tested; the handles BlendFactor, Material, TEXLAYER1,
// TEXLAYER0, ViewPos, WorldViewProjection and the effect released (+0x08, no null tests); the two edge
// objects released when set; both textures, the vertex buffer and the index buffer through the inline
// owner decrement (the buffers are NOT removed from the BufferManager's tracking sets); fogRange and
// fogColor released only when set. Then the members: the two polygon lists, the name, the scale
// offsets.
RoadEditable::~RoadEditable()
{
    static_cast<IRoadEditableRenderer*>(static_cast<void*>(g_RoadEditableRenderer))->remove(this);  // +0x14
    ::dice::hfe::eventManager->unregisterEventHandler(ECEditor, static_cast<IEventListener*>(this));
    m_paramBlendFactor->release();
    m_paramMaterial->release();
    m_paramTexLayer1->release();
    m_paramTexLayer0->release();
    m_paramViewPos->release();
    m_paramWorldViewProjection->release();
    dx9rend::effectRelease(m_effect);
    if (m_edge0)
        m_edge0->release();
    if (m_edge1)
        m_edge1->release();
    dx9rend::textureReleaseRef(m_texture0);
    dx9rend::textureReleaseRef(m_texture1);
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour: the buffers leave the buffer manager too (the shipped body releases them
    // only, leaving both in the manager for good); see invalidate.
    releaseBuffersForEditor();
#else
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);
#endif
    if (m_paramFogRange)
        m_paramFogRange->release();
    if (m_paramFogColor)
        m_paramFogColor->release();
}


// IBase.

void RoadEditable::addRef()
{
    ++m_refCount;                                                            // (shared)
}

int RoadEditable::getRef()
{
    return m_refCount;                                                       // (shared)
}

// 0x10143460: `--ref`; at zero the destructor body (called directly) and the engine heap.
int RoadEditable::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~RoadEditable();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// 0x10143270: 1, 820010 and 820011 answer `this`; 0x3709A4FF the IConsoleSaveable subobject (`add
// eax, 4` behind a null test); anything else 0.
void* RoadEditable::queryInterface(unsigned int iid) const
{
    RoadEditable* const self = const_cast<RoadEditable*>(this);
    if (iid == RD9_IID_IBASE || iid == kIID_IRoad || iid == kIID_IRoadEditable)
        return static_cast<IRoadEditable*>(self);
    if (iid == kIID_IConsoleSaveable)
        return static_cast< ::dice::hfe::io::IConsoleSaveable*>(self);
    return 0;
}


// Draws the road and paired edge objects, restoring depth bias afterward.
// In road-mask mode, only material IDs 1..255 draw. The queue, compiled renderer,
// and template must exist whenever vertices are present; no null guards are applied.
void RoadEditable::draw()
{
    if (!m_vertexCount)
        return;
    const RoadEditableRenderer* const queue =
        static_cast<const RoadEditableRenderer*>(static_cast<IRoadEditableRenderer*>(
            static_cast<void*>(g_RoadEditableRenderer)));
    if (queue->m_mode)
    {
        if (m_template->getMaterial() > 255)                                 // +0x54
            return;
        if (m_template->getMaterial() < 1)
            return;
    }

    dx9rend::stateManagerPush();
    dx9rend::stateManagerSetRenderState(195u, roadCompiledRendererDepthBiasBits());
    dx9rend::stateManagerSetRenderState(175u, roadCompiledRendererSlopeScaleDepthBiasBits());

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();     // renderer +0x48
    Mat4 worldViewProjection = Mat4::getIdentity();
    const Mat4* const projection = view->getProjectionMatrixPtr();           // +0x6C
    const Mat4* const inverseView = view->getInverseViewMatrixPtr();         // +0x68
    worldViewProjection.fullMult(*inverseView, *projection);
    m_paramWorldViewProjection->setValue(&worldViewProjection, 0);           // +0x18
    m_paramViewPos->setValue(&view->getInverseViewMatrixPtr()->m[12], 0);
    m_paramTexLayer0->setValue(m_texture0, 0);
    m_paramTexLayer1->setValue(m_texture1, 0);
    m_paramBlendFactor->setFloat(m_template->getBlendFactor());              // +0x24 / +0x3C
    m_paramMaterial->setFloat(static_cast<float>(m_template->getMaterial())
                              * 0.0039215689f);                              // flt_101E2B98 (1/255)
    m_paramFogRange->setValue(raConstants() + kRaFogRange, 0);
    m_paramFogColor->setValue(raConstants() + kRaFogColor, 0);

    dx9rend::effectSetTechniqueByName(m_effect, vc71::string("roadeditable"));
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, static_cast<unsigned int>(queue->m_mode));
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    if (m_indexBuffer)
    {
        dx9rend::indexBufferBind(m_indexBuffer);
        dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(m_triangleCount), 4u, 0, 0u,
                                              static_cast<unsigned int>(m_vertexCount), 0u);   // list
    }
    else
    {
        dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(m_triangleCount), 5u, 0u);    // strip
    }
    dx9rend::effectEnd(m_effect);

    if (m_edge0 && m_edge1)
    {
        m_edge0->draw();                                                     // +0x18
        m_edge1->draw();
    }
    dx9rend::stateManagerSetRenderState(195u, 0u);
    dx9rend::stateManagerSetRenderState(175u, 0u);
    dx9rend::stateManagerPop();
}

void RoadEditable::updateChunkVisibility(const Frustum*) {}                 // 0x1013B2E0 `retn 4`
bool RoadEditable::anyChunkVisible() { return true; }                       // 0x1013B2F0 `mov al, 1`


// IRoadEditable - the accessors.


// the template is stored first, unconditionally; with one, both textures are released
// (the inline owner decrement) and reloaded from its two texture files (TextureManager by name,
// sub_10019260, each file copied into the by-value string argument). Then invalidate (+0x78).
void RoadEditable::setTemplate(IRoadTemplate* roadTemplate)
{
    m_template = roadTemplate;
    if (roadTemplate)
    {
        dx9rend::textureReleaseRef(m_texture0);
        dx9rend::textureReleaseRef(m_texture1);
        m_texture0 = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(
            ::renddx9abi::toVc71String(roadTemplate->getTextureFile(0))));   // +0x14
        m_texture1 = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(
            ::renddx9abi::toVc71String(roadTemplate->getTextureFile(1))));
    }
    invalidate();
}

IRoadTemplate* RoadEditable::getTemplate()        { return m_template; }

// stored with no reference taken, then invalidate (+0x78).
void RoadEditable::setSpline(IEditorSpline* spline)
{
    m_spline = spline;
    invalidate();
}

IEditorSpline* RoadEditable::getSpline()          { return m_spline; }

void RoadEditable::setNoise(IPerlinNoise* noise)
{
    m_noise = noise;
    invalidate();
}

IPerlinNoise* RoadEditable::getNoise()            { return m_noise; }

// y through the FPU, x as a dword, into element `layer` (unchecked; no invalidate).
void RoadEditable::setTextureScaleOffset(int layer, const Vec2& offset)
{
    Vec2& element = m_textureScaleOffsets[static_cast<unsigned int>(layer)];
    element.y = offset.y;
    element.x = offset.x;
}

// the element's address (unchecked).
const Vec2& RoadEditable::getTextureScaleOffset(int layer)
{
    return m_textureScaleOffsets[static_cast<unsigned int>(layer)];
}

void RoadEditable::setPriority(int priority)      { m_priority = priority; }  // (shared)
int  RoadEditable::getPriority()                  { return m_priority; }

// the pair's two dwords stored, then invalidate;
// the getters 0x101432C0 / D0 / E0 / F0 answer the pair's address.
void RoadEditable::setFadeDistAtStartLeft(Vec2 distance)
{
    m_fadeDistAtStartLeft = distance;
    invalidate();
}

const Vec2& RoadEditable::getFadeDistAtStartLeft()    { return m_fadeDistAtStartLeft; }

void RoadEditable::setFadeDistAtStartRight(Vec2 distance)
{
    m_fadeDistAtStartRight = distance;
    invalidate();
}

const Vec2& RoadEditable::getFadeDistAtStartRight()   { return m_fadeDistAtStartRight; }

void RoadEditable::setFadeDistAtEndLeft(Vec2 distance)
{
    m_fadeDistAtEndLeft = distance;
    invalidate();
}

const Vec2& RoadEditable::getFadeDistAtEndLeft()      { return m_fadeDistAtEndLeft; }

void RoadEditable::setFadeDistAtEndRight(Vec2 distance)
{
    m_fadeDistAtEndRight = distance;
    invalidate();
}

const Vec2& RoadEditable::getFadeDistAtEndRight()     { return m_fadeDistAtEndRight; }

// 0x10143300 - `add ecx, 0BCh; jmp std::string::operator=`.
void RoadEditable::setName(const ::renddx9abi::HostString& name)
{
    m_name = name;
}

// a copy of the name into the caller's return slot.
::renddx9abi::HostString RoadEditable::getName()
{
    return m_name;
}

// only a change does anything: the byte stored, then buildGeometry (+0x88) with the
// template's width and max error(0) - the error asked for first - and the new state.
void RoadEditable::setGhost(bool ghost)
{
    if (static_cast<unsigned char>(ghost) == m_ghost)
        return;
    IRoadTemplate* const roadTemplate = m_template;
    m_ghost = ghost ? 1 : 0;
    const float maxError = roadTemplate->getMaxTesselatedError(0);           // +0x34
    const float width = roadTemplate->getWidth();                            // +0x2C
    buildGeometry(width, maxError, ghost);
}

bool RoadEditable::getGhost()                     { return m_ghost != 0; }

// 0x1013BB10 - "\Roads\" + getName() (+0x68, asked for first) + "_compiled.mesh", each joint a
// temporary string. Relative: compile prefixes the level path.
::renddx9abi::HostString RoadEditable::getCompiledMeshPath()
{
    const ::renddx9abi::HostString name = getName();
    ::renddx9abi::HostString path("\\Roads\\");
    path += name;
    path += "_compiled.mesh";
    return path;
}

// Releases buffers and rebuilds when a spline and template exist.
// The vertex pointer is left stale and buildGeometry releases it again; preserve this legacy
// behavior. Missing inputs also leave the old vertex count and pointer available to draw.
void RoadEditable::invalidate()
{
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour: both buffers are released, taken out of the buffer manager and forgotten,
    // with the counts draw reads, as buildGeometry does with live buffers. The shipped body keeps the released vertex
    // buffer's pointer, so the next rebuild releases it again and the buffer manager writes into the freed wrapper -
    // which retail's Memory.dll heap absorbed and the editor's CRT heap does not - and leaves every index buffer in the
    // manager. A road whose spline or template is missing then draws nothing instead of the freed buffer. The game
    // flavours keep the shipped body.
    releaseBuffersForEditor();
#else
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);
    m_indexBuffer = 0;
#endif
    if (m_spline && m_template)
    {
        const bool ghost = m_ghost != 0;
        IRoadTemplate* const roadTemplate = m_template;
        const float maxError = roadTemplate->getMaxTesselatedError(0);
        const float width = roadTemplate->getWidth();
        buildGeometry(width, maxError, ghost);
    }
}


#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// [CHOSEN, not recovered] editor flavour (see invalidate): each buffer released and removed from the buffer manager,
// in the pool buildGeometry made it in.
void RoadEditable::releaseBuffersForEditor()
{
    const int savedPool = dx9rend::bufferManagerSwapPool(1);
    if (m_vertexBuffer)
    {
        dx9rend::vertexBufferRelease(m_vertexBuffer);
        dx9rend::bufferManagerRemoveVertexBuffer(m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    if (m_indexBuffer)
    {
        dx9rend::indexBufferRelease(m_indexBuffer);
        dx9rend::bufferManagerRemoveIndexBuffer(m_indexBuffer);
        m_indexBuffer = 0;
    }
    dx9rend::bufferManagerSwapPool(savedPool);
    m_vertexCount = 0;
    m_triangleCount = 0;
}
#endif


// Rebuilds road buffers and bounds from at least four control points.
// Uses template width/error rather than the arguments. Empty triangle output leaves buffers alone.

void RoadEditable::buildGeometry(float /*width - not read*/, float /*maxError - not read*/, bool ghost)
{
    if (m_spline->getControlPointCount() < 4)                                // +0x54
        return;
    std::vector<RoadEditableVertex> vertices;
    std::vector<int> indices;
    IRoadTemplate* const roadTemplate = m_template;
    const float maxError = roadTemplate->getMaxTesselatedError(0);
    const float templateWidth = roadTemplate->getWidth();
    const int triangles = tessellate(vertices, indices, templateWidth, maxError, ghost);
    m_triangleCount = triangles;
    m_vertexCount = static_cast<int>(vertices.size());
    if (!triangles)
        return;

    const int savedPool = dx9rend::bufferManagerSwapPool(1);
    if (m_vertexBuffer)
    {
        dx9rend::vertexBufferRelease(m_vertexBuffer);
        dx9rend::bufferManagerRemoveVertexBuffer(m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    m_vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager,
                                                 static_cast<unsigned int>(m_vertexCount), 32u,
                                                 vc71::string("RoadEditable"), 0u);
    float* destination = static_cast<float*>(dx9rend::vertexBufferLock(m_vertexBuffer, 0u, 0u, 0u));
    for (int i = 0; i < m_vertexCount; ++i)
    {
        const RoadEditableVertex& source = vertices[static_cast<unsigned int>(i)];
        destination[0] = source.position.x;
        destination[1] = source.position.y;
        destination[2] = source.position.z;
        destination[3] = source.uv0.x;
        destination[4] = source.uv0.y;
        destination[5] = source.uv1.x;
        destination[6] = source.uv1.y;
        destination[7] = source.alpha;
        destination += 8;
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);

    if (m_indexBuffer)
    {
        dx9rend::indexBufferRelease(m_indexBuffer);
        dx9rend::bufferManagerRemoveIndexBuffer(m_indexBuffer);
        m_indexBuffer = 0;
    }
    if (!indices.empty())
    {
        const unsigned int count = static_cast<unsigned int>(indices.size());
        m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(count, 0u, 101u);   // D3DFMT_INDEX16
        unsigned short* const out16 =
            static_cast<unsigned short*>(dx9rend::indexBufferLock(m_indexBuffer, 0u, 0u, 0u));
        for (unsigned int i = 0; i < count; ++i)
            out16[i] = static_cast<unsigned short>(indices[i]);
        dx9rend::indexBufferUnlock(m_indexBuffer);
    }

    Vec3 minimum;
    Vec3 maximum;
    minimum.x = minimum.y = minimum.z = 0.0f;
    maximum.x = maximum.y = maximum.z = 0.0f;
    D3DXComputeBoundingBox(&vertices[0].position, static_cast<DWORD>(vertices.size()), 64u,
                           &minimum, &maximum);
    m_bounds.min = minimum;
    m_bounds.max = maximum;
    m_bounds.dirty = 1;
    D3DXComputeBoundingSphere(&vertices[0].position, static_cast<DWORD>(vertices.size()), 64u,
                              &m_centre, &m_radius);
    dx9rend::bufferManagerSwapPool(savedPool);
}


// Appends this road to CompiledRoads.con and writes its mesh; returns false when empty.
// The caller must truncate the .con first and provide terrain and writable output paths.
// The version-1.4 mesh omits the chunk table expected by the reader. Ghost state is restored.
// [CHOSEN] Host float formatting may use two-digit rather than three-digit exponents.

bool RoadEditable::compile(const ::renddx9abi::HostString& levelPath)
{
    if (!m_vertexCount)
        return false;
    if (m_ghost)
    {
        IRoadTemplate* const roadTemplate = m_template;
        const float maxError = roadTemplate->getMaxTesselatedError(0);
        const float width = roadTemplate->getWidth();
        buildGeometry(width, maxError, false);
    }

    vc71::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", savePath);   // settings +0x40
    ::renddx9abi::HostString conPath = ::renddx9abi::makeHostString(savePath);
    conPath += "\\";
    conPath += levelPath;
    conPath += "\\CompiledRoads.con";

    ::renddx9abi::HostString meshPath(levelPath);
    meshPath += getCompiledMeshPath();

    ::renddx9abi::HostStream* const con = ::dice::hfe::fileManager->open(
        conPath, static_cast< ::renddx9abi::HostFileMode>(3),
        static_cast< ::renddx9abi::HostFileAccess>(2), 0);                   // FileManager +0x18
    static_cast< ::renddx9abi::HostNonlinearStream*>(con)->seek(
        static_cast< ::renddx9abi::HostSeekOrigin>(2), 0);                   // +0x30, the end

    std::ostringstream text;
    const ::renddx9abi::HostString templateName = getTemplate()->getName();   // +0x20, then RT+0x5C
    text << "object.create ";
    text.write(templateName.c_str(), static_cast<std::streamsize>(templateName.size()));
    text << std::endl;
    text << "object.geometry.loadMesh ";
    text.write(meshPath.c_str(), static_cast<std::streamsize>(meshPath.size()));
    text << std::endl;
    text << "object.absoluteposition " << m_centre.x << '/' << m_centre.y << '/' << m_centre.z
         << std::endl;
    text << std::endl;
    const std::string written = text.str();
    con->write(written.c_str(), static_cast<unsigned int>(written.size()));   // +0x14
    con->release();                                                          // +0x08

    ::renddx9abi::HostStream* const mesh = ::dice::hfe::fileManager->open(
        meshPath, static_cast< ::renddx9abi::HostFileMode>(0),
        static_cast< ::renddx9abi::HostFileAccess>(2), 0);
    const unsigned int version = 0x10004u;                                   // minor 4, major 1
    mesh->write(&version, 4);
    mesh->write(&m_centre, 12);
    mesh->write(&m_radius, 4);
    mesh->write(&m_bounds.min, 12);
    mesh->write(&m_bounds.max, 12);
    mesh->write(&m_priority, 4);
    mesh->write(&m_vertexCount, 4);

    IBase* const terrain = static_cast<IBase*>(classManagerGetSingletonAs(
        ::renddx9abi::HostString("Terrain"), kIID_ITerrain));
    terrain->queryInterface(kIID_ITerrainEditable);                          // the answer is unused

    const unsigned char* source =
        static_cast<const unsigned char*>(dx9rend::vertexBufferLock(m_vertexBuffer, 0u, 0u, 0u));
    for (int i = 0; i < m_vertexCount; ++i)
    {
        const float* const record = reinterpret_cast<const float*>(source);
        RoadEditableVertex vertex;
        vertex.position.x = record[0] - m_centre.x;
        vertex.position.y = record[1] - m_centre.y;
        vertex.position.z = record[2] - m_centre.z;
        vertex.uv0.x = record[3];
        vertex.uv0.y = record[4];
        vertex.uv1.x = record[5];
        vertex.uv1.y = record[6];
        vertex.alpha = record[7];
        source += 32;
        mesh->write(&vertex.position, 12);
        mesh->write(&vertex.uv0, 8);
        mesh->write(&vertex.uv1, 8);
        mesh->write(&vertex.alpha, 4);
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);

    if (m_indexBuffer)
    {
        const int indexCount = m_triangleCount * 3;
        mesh->write(&indexCount, 4);
        const unsigned short* const indices16 =
            static_cast<const unsigned short*>(dx9rend::indexBufferLock(m_indexBuffer, 0u, 0u, 0u));
        for (int i = 0; i < indexCount; ++i)
            mesh->write(&indices16[i], 2);
        dx9rend::indexBufferUnlock(m_indexBuffer);
    }
    else
    {
        ::renddx9abi::logf("RoadEditable.cpp:2548: TBM: The new roads need Idx Buffer!");
    }
    mesh->release();

    if (m_ghost)
    {
        IRoadTemplate* const roadTemplate = m_template;
        const float maxError = roadTemplate->getMaxTesselatedError(0);
        const float width = roadTemplate->getWidth();
        buildGeometry(width, maxError, true);
    }
    return true;
}

// (`retn 10h`) - a stub: the by-value set is destroyed (the callee's job) and the answer
// is true. The path is not read.
bool RoadEditable::compileTemplates(const ::renddx9abi::HostString&, RoadTemplateNameSet)
{
    return true;
}

// Projects the road footprint into the editable terrain and posts its changed rectangle.
// Requires a valid spline and editable HeightmapCluster; fewer than four points or no template
// produces no geometry. The spline count is queried before the null check.
// The float target writes a debug DDS without updating heights. Target allocation failure
// preserves the existing held buffers/open state bracket, and pixel traversal ignores pitch.
// The editor flavour takes the 8-bit target, sizes it to the footprint, retries a smaller one when
// the copy back fails, cleans up when none works, and reads at the pitch (see the body).
void RoadEditable::projectToTerrain()
{
    if (m_spline->getControlPointCount() < 4 || !m_spline || !m_template)
        return;
    world::IHeightmapCluster* const cluster = heightmapCluster();
    world::IHeightmapClusterEditable* const editable = static_cast<world::IHeightmapClusterEditable*>(
        cluster->queryInterface(kIID_IHeightmapClusterEditable));            // +0x0C

    Vec3 minimum;
    minimum.x = FLT_MAX;
    minimum.y = FLT_MAX;
    minimum.z = FLT_MAX;
    Vec3 maximum;
    maximum.x = -FLT_MAX;
    maximum.y = -FLT_MAX;
    maximum.z = -FLT_MAX;
    std::vector<RoadEditableVertex> vertices;
    float footprint = m_template->getWidth() + 2.0f;                         // flt_101E0550
    if (m_noise)
    {
        const Vec2& high = m_noise->getOutputRange();                        // +0x44
        const Vec2& low = m_noise->getOutputRange();
        footprint = (high.y - low.x) + footprint;
    }
    std::vector<int> indices;
    const float maxError = m_template->getMaxTesselatedError(0);
    const int triangles = tessellate(vertices, indices, footprint, maxError, true);
    const int vertexCount = static_cast<int>(vertices.size());
    if (!triangles)
        return;

    dx9rend::stateManagerPush();
    void* const positions = dx9rend::createVertexBuffer(dx9rend::g_bufferManager,
                                                        static_cast<unsigned int>(vertexCount), 12u,
                                                        vc71::string("RoadEditable"), 8u);
    float* out = static_cast<float*>(dx9rend::vertexBufferLock(positions, 0u, 0u, 0u));
    for (int i = 0; i < vertexCount; ++i)
    {
        const Vec3& p = vertices[static_cast<unsigned int>(i)].position;
        if (!(minimum.x < p.x))
            minimum.x = p.x;
        if (!(minimum.y < p.y))
            minimum.y = p.y;
        if (!(minimum.z < p.z))
            minimum.z = p.z;
        if (maximum.x < p.x)
            maximum.x = p.x;
        if (maximum.y < p.y)
            maximum.y = p.y;
        if (maximum.z < p.z)
            maximum.z = p.z;
        out[0] = p.x;
        out[1] = p.y;
        out[2] = p.z;
        out += 3;
    }
    dx9rend::vertexBufferUnlock(positions);

    void* footprintIndices = 0;
    if (!indices.empty())
    {
        const unsigned int count = static_cast<unsigned int>(indices.size());
        footprintIndices = dx9rend::bufferManagerCreateIndexBuffer(count, 0u, 101u);
        unsigned short* const out16 =
            static_cast<unsigned short*>(dx9rend::indexBufferLock(footprintIndices, 0u, 0u, 0u));
        for (unsigned int i = 0; i < count; ++i)
            out16[i] = static_cast<unsigned short>(indices[i]);
        dx9rend::indexBufferUnlock(footprintIndices);
    }

    IDirect3DDevice9* const device = rendererBase()->device;                 // renderer +0x0C
    int size = deviceCapsMaxTextureHeight();
    if (size <= 1024)
        size = 1024;
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour: the target sized to the footprint - the smallest power of two from 1024
    // holding two texels per metre of its larger side, at most the card's limit - not the card's limit itself. The heights
    // are read back at one texel per metre, and the card's limit (8192 or more) asks the 32-bit editor for a 256 MB copy.
    {
        const float extent = maximum.x - minimum.x > maximum.z - minimum.z ? maximum.x - minimum.x : maximum.z - minimum.z;
        int wanted = 1024;
        while (wanted < size && static_cast<float>(wanted) < 2.0f * extent)
            wanted *= 2;
        size = wanted;
    }
#endif
    size = size * 2;
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour: always the 8-bit target, as the 2006 editor hard-wired it. The shipped
    // body takes the float target wherever the device renders G32R32F - every current card - and on that path writes
    // no height at all (it saves a debug c:\tbmTest.dds instead). The game flavours keep the shipped choice.
    const bool floatTarget = false;
#else
    const bool floatTarget = dx9rend::deviceCapsSupportsTextureFormat(115u); // sub_100013D0, G32R32F
#endif
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    bool projected = false;
#endif
    for (;;)
    {
        size >>= 1;
        if (size < 128)
            break;
        const D3DFORMAT format = static_cast<D3DFORMAT>(floatTarget ? 115 : 21);
        IDirect3DSurface9* rendering = 0;
        if (device->CreateOffscreenPlainSurface(static_cast<UINT>(size), static_cast<UINT>(size), format,
                                                D3DPOOL_SYSTEMMEM, &rendering, 0) != 0)
            continue;
        RenderToSurface* target = 0;
        if (D3DXCreateRenderToSurface(device, static_cast<UINT>(size), static_cast<UINT>(size), format,
                                      FALSE, D3DFMT_UNKNOWN, reinterpret_cast<void**>(&target)) != 0)
        {
            rendering->Release();
            continue;
        }
        if (target->BeginScene(rendering, 0) != 0)
        {
            rendering->Release();
            target->Release();
            continue;
        }

        device->Clear(0, 0, D3DCLEAR_TARGET, 0xFFFF0000u, 0.0f, 0);
        float constant0[4];
        float constant1[4];
        float constant2[4];
        const float spanX = maximum.x - minimum.x;
        const float spanZ = maximum.z - minimum.z;
        constant0[0] = minimum.x;
        constant0[1] = minimum.z;
        constant0[2] = 0.1f;
        constant0[3] = 2.0f;
        constant1[2] = 0.0f;
        constant1[3] = 1.0f;
        constant2[0] = minimum.y;
        constant2[2] = 0.0f;
        constant2[3] = 1.0f;
        constant1[0] = 2.0f / spanX;
        constant1[1] = 2.0f / spanZ;
        const float spanY = maximum.y - minimum.y;
        constant2[1] = 1.0f / spanY;

        dx9rend::effectSetTechniqueByName(m_effect, vc71::string("projectroad"));
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        device->SetVertexShaderConstantF(0, constant0, 1);                   // +0x178
        device->SetVertexShaderConstantF(1, constant1, 1);
        device->SetVertexShaderConstantF(2, constant2, 1);
        dx9rend::vertexBufferBindStream(positions, 0);
        if (footprintIndices)
        {
            dx9rend::indexBufferBind(footprintIndices);
            dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(triangles), 4u, 0, 0u,
                                                  static_cast<unsigned int>(vertexCount), 0u);
        }
        else
        {
            dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(triangles), 5u, 0u);
        }
        dx9rend::effectEnd(m_effect);
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
        // [CHOSEN, not recovered] editor flavour: the shipped body ignores EndScene, whose copy into the system-memory
        // surface can fail (E_OUTOFMEMORY in the 32-bit editor); the surface then stays zero, every texel reads as the
        // footprint at its lowest point and the whole rectangle is flattened. A failed copy tries the next size down,
        // the footprint buffers kept.
        if (target->EndScene(0) != 0)
        {
            rendering->Release();
            target->Release();
            continue;
        }
#else
        target->EndScene(0);
#endif

        dx9rend::vertexBufferRelease(positions);
        dx9rend::bufferManagerRemoveVertexBuffer(positions);
        if (footprintIndices)
        {
            dx9rend::indexBufferRelease(footprintIndices);
            dx9rend::bufferManagerRemoveIndexBuffer(footprintIndices);
        }
        if (floatTarget)
            D3DXSaveSurfaceToFileA("c:\\tbmTest.dds", 4u, rendering, 0, 0);   // D3DXIFF_DDS

        const int x0 = static_cast<int>(std::floor(static_cast<double>(minimum.x)));
        const int z0 = static_cast<int>(std::floor(static_cast<double>(minimum.z)));
        const int x1 = static_cast<int>(std::ceil(static_cast<double>(maximum.x)));
        const int z1 = static_cast<int>(std::ceil(static_cast<double>(maximum.z)));
        int rows = z1 - z0;
        const int columns = x1 - x0;
        IDirect3DSurface9* heights = 0;
        device->CreateOffscreenPlainSurface(static_cast<UINT>(columns), static_cast<UINT>(rows), format,
                                            D3DPOOL_SYSTEMMEM, &heights, 0);  // the answer is not tested
        D3DXLoadSurfaceFromSurface(heights, 0, 0, rendering, 0, 0, 2u, 0);   // D3DX_FILTER_POINT
        rendering->Release();
        target->Release();
        D3DLOCKED_RECT locked;
        heights->LockRect(&locked, 0, D3DLOCK_READONLY);
        const unsigned char* const bits = static_cast<const unsigned char*>(locked.pBits);
        if (rows > 0)
        {
            int rowStart = 0;
            int z = z1;
            do
            {
                for (int i = 0; i < columns; ++i)
                {
                    if (floatTarget)
                        continue;
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
                    // [CHOSEN, not recovered] editor flavour: the row at the surface's pitch (the shipped body steps
                    // by the width, which misreads a padded surface).
                    const unsigned char* const texel =
                        bits + static_cast<size_t>(locked.Pitch) * static_cast<size_t>(rowStart / columns) + 4 * i;
#else
                    const unsigned char* const texel = bits + 4 * (i + rowStart);
#endif
                    if (texel[3] != 0)
                        continue;
                    float height = static_cast<float>(static_cast<int>(texel[1])) * 0.0039215689f
                                       * spanY + minimum.y - 0.1f;           // flt_101E2B98, flt_101DD25C
                    height = clampRange(height, 0.0f, 65535.0f);
                    editable->setHeightInWorldCoords(static_cast<float>(x0 + i), static_cast<float>(z),
                                                     height);                // +0x34
                }
                rowStart += columns;
                --z;
                --rows;
            }
            while (rows != 0);
        }
        heights->UnlockRect();
        heights->Release();
        dx9rend::stateManagerPop();

        Vec4 area;
        area.x = static_cast<float>(x0);
        area.y = static_cast<float>(z0);
        area.z = static_cast<float>(x1);
        area.w = static_cast<float>(z1);
#if BF2_PLUGIN_ABI_NATIVE
        HeightmapEventNode* const node = new HeightmapEventNode(kEventHeightmapModified);
        node->setAffectedArea(area);
        ::dice::hfe::eventManager->postEvent(ECEditor, node, 0.0f);          // +0x14
#else
        RetailHeightmapEventNode* const node = new RetailHeightmapEventNode(kEventHeightmapModified);
        node->setAffectedArea(area);
        ::dice::hfe::eventManager->postEvent(ECEditor, reinterpret_cast<EventNode*>(node), 0.0f);
#endif
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
        projected = true;
#endif
        break;
    }
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour: when no size worked, the footprint buffers are released and the state
    // bracket closed (the shipped body leaves both).
    if (!projected)
    {
        dx9rend::vertexBufferRelease(positions);
        dx9rend::bufferManagerRemoveVertexBuffer(positions);
        if (footprintIndices)
        {
            dx9rend::indexBufferRelease(footprintIndices);
            dx9rend::bufferManagerRemoveIndexBuffer(footprintIndices);
        }
        dx9rend::stateManagerPop();
    }
#endif
}


// io::IConsoleSaveable and IEventListener.


// 0x1013B4B0 - `xor al, al; retn 4`: a road writes no script of its own (the LevelEditor writes
// Editor\Roads.con).
bool RoadEditable::makeScript(::dice::hfe::io::IStream*)
{
    return false;
}

// (`this` = the +0x08 subobject, `retn 10h`) - the editor's category-8 events, each for
// one sender (the user-data word): 200 the road's spline -> invalidate; 206 the spline ->
// projectToTerrain, then invalidate; 300 the template -> invalidate; 301 the template ->
// setTemplate(0); 302 / 303 this road -> invalidate; 304 / 305 this road -> setGhost(false / true).
// Every reaction is a virtual call through the primary table.
void RoadEditable::handleEvent(EventCategory, unsigned int id, EventNode*, void* data)
{
    switch (id)
    {
    case kEventSplineChanged:
        if (data == static_cast<void*>(m_spline))
            invalidate();                                                    // +0x78
        break;
    case kEventSplineReconstructed:
        if (data == static_cast<void*>(m_spline))
        {
            projectToTerrain();                                              // +0x7C
            invalidate();
        }
        break;
    case kEventRoadTemplateChanged:
        if (data == static_cast<void*>(m_template))
            invalidate();
        break;
    case kEventRoadTemplateDetached:
        if (data == static_cast<void*>(m_template))
            setTemplate(0);                                                  // +0x1C
        break;
    case kEventRoadNoiseHasChanged:
    case kEventRoadPropertiesChanged:
        if (data == static_cast<void*>(static_cast<IRoadEditable*>(this)))
            invalidate();
        break;
    case kEventBuildFinalRoad:
        if (data == static_cast<void*>(static_cast<IRoadEditable*>(this)))
            setGhost(false);                                                 // +0x6C
        break;
    case kEventBuildGhostRoad:
        if (data == static_cast<void*>(static_cast<IRoadEditable*>(this)))
            setGhost(true);
        break;
    default:
        break;
    }
}


// RefFactory refuse aggregation, 0x118 bytes, construct, QI with NO null test on the
// object (shipped), release on refusal. (The native flavour's object is smaller - its HostString is
// the host's std::string - and still fits the shipped block.)

IBase* __fastcall createRoadEditable(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    static_assert(sizeof(RoadEditable) <= 0x118, "the shipped 0x118-byte block must hold the object");
    void* const storage = ::renddx9abi::engineAlloc(0x118);
    RoadEditable* self = 0;
    if (storage)
        self = new (storage) RoadEditable();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [eax+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#include "RoadEditableTessellate.inl"

#endif // BF2_IMAGE_RENDDX9
