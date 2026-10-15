// Collects visible road chunks in template-priority and road order, draws, then clears them.
// Drawing requires terrain and the renderer's road-enable flag.


#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "RoadCompiled.h"
#include "RoadCompiledTemplate.h"
#include "QuadPatch.h"                          // TerrainCuller
#include "Terrain.h"                            // Terrain, terrainOriginScratch
#include "XpLightManager.h"                     // lightManagerSetRenderTargetTexture (sub_10005920)
#include "HemiMapManager/HemiMapDependencies.h"

#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>

#include <cmath>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void*  g_shaderManager;      // dword_1023B97C - ShaderManager.cpp
extern void*  g_raShaderConstants;  // dword_1023DDBC - RenderQueues.cpp (created at initialize)
extern IBase* g_Renderer;           // dword_1023D098 - BF2Render.cpp
}

namespace geom {

extern IBase* g_RoadCompiledRenderer;

// StaticMesh.cpp - sub_1008DD80: a Vec3 and a w packed into a shared static Vec4, its address back.
const float* staticMeshPackVec4(const float v[3], float w);

// From ModuleGeom's own getClassList and singleton table, both read:
//   sub_100518A0(821912, "dice.hfe.geom.RoadCompiledRenderer", RefFactory_geom_RoadCompiledRenderer)
//   { "RoadCompiledRenderer", &g_RoadCompiledRenderer, iid 820013, cid 821912, flags 2 }
extern const unsigned int CID_RoadCompiledRenderer = 821912u;
extern const unsigned int IID_IRoadCompiledRenderer = 820013u;

namespace {

// The Ra constants block's holders the road reads (RenderQueues.cpp owns the block; the offsets are
// the shipped holder offsets every recovered reader of dword_1023DDBC uses).
const unsigned int kRaFogRange                 = 0xA8;    // FogRange
const unsigned int kRaFogColor                 = 0xBC;    // FogColor
const unsigned int kRaAmbientIlluminationRange = 0xD0;    // AmbientIlluminationRange (sub_100380E0)
const unsigned int kRaSinglePointColor         = 0x230;   // SinglePointColor

// The Ra constants block (g_raShaderConstants) word at a retail x86 byte offset.
unsigned char* raConstantAt(unsigned int byteOffset)
{
    return dx9rend::raShaderConstantsAt(byteOffset);
}

unsigned int floatBits(float value)
{
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Renderer +0x780 - one of the renderer's path targets (+0x778..+0x7C8, released by the render-path
// deactivation), read through g_Renderer at 0x10145356 for the render-path-2 arm. Read through
// BF2Render.cpp's path-target accessor, keyed by that retail offset.
dx9rend::EngineTexture* rendererPathTarget780()
{
    return dx9rend::rendererPathTarget(0x780);
}

// (retn 8) - a row vector through a Mat4: out = in * m. Every product is formed before the
// first store, so `in` and `out` may alias (the draw walk passes the same Vec4 for both). The sums
// keep the shipped association, w first:
//     w = ((m3*x + m15*w) + m7*y) + m11*z      z = ((m2*x + m14*w) + m6*y) + m10*z
//     y = ((m1*x + m13*w) + m5*y) + m9*z       x = ((m12*w + m4*y) + m8*z) + x*m0
void transformVec4(const Mat4& m, const float* in, float* out)
{
    const float x = in[0];
    const float y = in[1];
    const float z = in[2];
    const float w = in[3];
    const float ow = ((m.m[3] * x + m.m[15] * w) + m.m[7] * y) + m.m[11] * z;
    const float oz = ((m.m[2] * x + m.m[14] * w) + m.m[6] * y) + m.m[10] * z;
    const float oy = ((m.m[1] * x + m.m[13] * w) + m.m[5] * y) + m.m[9] * z;
    const float ox = ((m.m[12] * w + m.m[4] * y) + m.m[8] * z) + x * m.m[0];
    out[0] = ox;
    out[1] = oy;
    out[2] = oz;
    out[3] = ow;
}

} // anonymous namespace

// Queues and draws compiled roads through MeshRendererCommon and RenderPathListener.
// The caller must set enabled/render-path state and bind the effect before rendering.
// The integer at +0x84 has an [UNRESOLVED] role. The camera constant stays zero;
// its intended writer remains [UNRESOLVED].
class RoadCompiledRenderer
{
public:
    RoadCompiledRenderer();
    ~RoadCompiledRenderer();


    int   addRef()        { return ++m_refCount; }             // +0x00 (answers the count)
    int   getRef() const  { return m_refCount; }               // +0x04
    int   release();                                           // +0x08
    void* queryInterface(unsigned int iid) const;              // +0x0C
    void  setSlot84(int value) { m_int84 = value; }            // +0x10
    int   getSlot84() const    { return m_int84; }             // +0x14
    void  setEnabled(unsigned char value) { m_enabled = value; }       // +0x18
    unsigned char getEnabled() const { return m_enabled; }             // +0x1C
    std::string& setName(const std::string& value) { return m_name = value; }   // +0x20
    std::string* getName() { return &m_name; }                        // +0x24

    // the work
    void add(RoadCompiled* road);
    void onRenderPathNotification();                           // (listener +0x0C)
    void drawAll();
    void updateFrameConstants();

    unsigned char getHemiDirectFlag() const { return m_hemiDirectFlag; }
    void setHemiDirectFlag(unsigned char value) { m_hemiDirectFlag = value; }

private:
    void bindEffect();

public:
    void* const*               m_vptr;                         // +0x00
    unsigned char              m_enabled;                      // +0x04
    unsigned char              m_pad05[3];
    std::string               m_name;                         // +0x08
    void* const*               m_listenerVptr;                 // +0x24
    int                        m_renderPath;                   // +0x28
    int                        m_refCount;                     // +0x2C
    dx9rend::Effect*           m_effect;                       // +0x30
    dx9rend::ShaderParameter*  m_paramWorldViewProjection;     // +0x34  "WorldViewProjection"
    dx9rend::ShaderParameter*  m_paramLocalEye;                // +0x38  "LocalEye"
    dx9rend::ShaderParameter*  m_paramTexBlendFactor;          // +0x3C  "TexBlendFactor"
    dx9rend::ShaderParameter*  m_paramFadeOut;                 // +0x40  "FadeOut"
    dx9rend::ShaderParameter*  m_paramTexLayer3;               // +0x44  "TEXLAYER3"
    dx9rend::ShaderParameter*  m_paramTexLayer4;               // +0x48  "TEXLAYER4"
    dx9rend::ShaderParameter*  m_paramTexLayer2;               // +0x4C  "TEXLAYER2"
    dx9rend::ShaderParameter*  m_paramWorldSpaceCamPos;        // +0x50  "WorldSpaceCamPos"
    dx9rend::ShaderParameter*  m_paramScaleY;                  // +0x54  "SCALEY"
    dx9rend::ShaderParameter*  m_paramSunColor;                // +0x58  "SUNCOLOR"
    dx9rend::ShaderParameter*  m_paramGIColor;                 // +0x5C  "GICOLOR"
    dx9rend::ShaderParameter*  m_paramFogRange;                // +0x60  "fogRange"
    dx9rend::ShaderParameter*  m_paramFogColor;                // +0x64  "fogColor"
    dx9rend::ShaderParameter*  m_paramTexProjOffset;           // +0x68  "TEXPROJOFFSET"
    dx9rend::ShaderParameter*  m_paramTexProjScale;            // +0x6C  "TEXPROJSCALE"
    dx9rend::ShaderParameter*  m_paramSinglePointColor;        // +0x70  "SINGLEPOINTCOLOR"
    dx9rend::ShaderParameter*  m_paramAmbientIlluminationRange;// +0x74  "AmbientIlluminationRange"
    RoadList                   m_roads;                        // +0x78
    int                        m_int84;                        // +0x84
    unsigned char              m_hemiDirectFlag;               // +0x88
    unsigned char              m_pad89[3];
    float                      m_viewDistanceScale;            // +0x8C
    float                      m_depthBias;                    // +0x90
    float                      m_slopeScaleDepthBias;          // +0x94
    IBase*                     m_heightmapCluster;             // +0x98
    Terrain*                   m_terrain;                      // +0x9C
    float                      m_worldSpaceCamPos[4];          // +0xA0
    float                      m_fadeOut[2];                   // +0xB0

private:
    RoadCompiledRenderer(const RoadCompiledRenderer&);
    RoadCompiledRenderer& operator=(const RoadCompiledRenderer&);
};

#if defined(_M_IX86)
static_assert(offsetof(RoadCompiledRenderer, m_enabled) == 0x04, "enabled byte at +0x04");
static_assert(offsetof(RoadCompiledRenderer, m_name) == 0x08, "name at +0x08");
#endif

namespace {

// The owner of the listener subobject (retail: listener - 0x24).
RoadCompiledRenderer* fromListener(void* listener)
{
    return reinterpret_cast<RoadCompiledRenderer*>(static_cast<unsigned char*>(listener)
                                                   - offsetof(RoadCompiledRenderer, m_listenerVptr));
}


// Primary-table thunks use __fastcall with unused EDX to match the __thiscall slots.

int RD9_THUNK rcr_addRef(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->addRef();
}

int RD9_THUNK rcr_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->getRef();
}

int RD9_THUNK rcr_release(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->release();
}

void* RD9_THUNK rcr_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<RoadCompiledRenderer*>(self)->queryInterface(iid);
}

// +0x10 / +0x14 - the int at +0x84. [UNRESOLVED] role: StaticMeshRenderer's +0x10 is an empty body and
// its +0x14 answers 0; nothing recovered calls either on this singleton.
void RD9_THUNK rcr_setSlot84(void* self RD9_THUNK_EDX, int value)
{
    static_cast<RoadCompiledRenderer*>(self)->setSlot84(value);
}

int RD9_THUNK rcr_getSlot84(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->getSlot84();
}

// +0x18, the one VideoOptions::apply calls. A single byte store.
void RD9_THUNK rcr_setEnabled(void* self RD9_THUNK_EDX, int value)
{
    static_cast<RoadCompiledRenderer*>(self)->setEnabled(static_cast<unsigned char>(value));
}

unsigned char RD9_THUNK rcr_getEnabled(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->getEnabled();
}

// +0x20, 0x10121A10 - `add ecx, 8; jmp std::string::operator=(const std::string&)`: the import's
// mangled name pins the argument, and the assignment's result (the name) is what it answers.
std::string* RD9_THUNK rcr_setName(void* self RD9_THUNK_EDX, const std::string& value)
{
    return &static_cast<RoadCompiledRenderer*>(self)->setName(value);
}

std::string* RD9_THUNK rcr_getName(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiledRenderer*>(self)->getName();
}

void* const kPrimaryVtable[10] =
{
    reinterpret_cast<void*>(&rcr_addRef),          // +0x00
    reinterpret_cast<void*>(&rcr_getRef),          // +0x04
    reinterpret_cast<void*>(&rcr_release),         // +0x08
    reinterpret_cast<void*>(&rcr_queryInterface),  // +0x0C  (shared with MeshRendererCommon)
    reinterpret_cast<void*>(&rcr_setSlot84),       // +0x10
    reinterpret_cast<void*>(&rcr_getSlot84),       // +0x14
    reinterpret_cast<void*>(&rcr_setEnabled),      // +0x18
    reinterpret_cast<void*>(&rcr_getEnabled),      // +0x1C
    reinterpret_cast<void*>(&rcr_setName),         // +0x20
    reinterpret_cast<void*>(&rcr_getName)          // +0x24
};


// THE RenderPathListener table, 0x1021DABC - `this` is the +0x24 subobject. RendDX9Init.cpp dispatches
// the typed dx9rend::RenderPathListener slots onto these entries; renderFrame calls +0x0C directly.


// +0x00, 0x101C43C0 - `mov [ecx+4], arg`: the activated path into +0x28.
void RD9_THUNK rcr_onRenderPathActivated(void* self RD9_THUNK_EDX, int path)
{
    fromListener(self)->m_renderPath = path;
}

void RD9_THUNK rcr_onRenderPathDeactivated(void* RD9_THUNK_EDX) { }            // +0x04  0x10037D40 `ret`
void RD9_THUNK rcr_onRenderPathChanged(void* RD9_THUNK_EDX, int) { }           // +0x08  0x100E13B0 `retn 4`

// +0x0C, (`retn 4`) - the per-frame call. The argument (renderFrame passes 0) is not read.
void RD9_THUNK rcr_onRenderPathNotification(void* self RD9_THUNK_EDX, int)
{
    fromListener(self)->onRenderPathNotification();
}

// +0x10, 0x101A9A10 - `mov eax, [ecx+4]`.
int RD9_THUNK rcr_getCurrentRenderPath(void* self RD9_THUNK_EDX)
{
    return fromListener(self)->m_renderPath;
}

// +0x14, (`retn 4`) - the name, constructed into the caller's hidden return slot.
std::string* RD9_THUNK rcr_getRenderPathListenerName(void* RD9_THUNK_EDX, std::string* out)
{
    new (out) std::string("RoadCompiledRenderer");
    return out;
}

void* const kListenerVtable[6] =
{
    reinterpret_cast<void*>(&rcr_onRenderPathActivated),       // +0x00
    reinterpret_cast<void*>(&rcr_onRenderPathDeactivated),     // +0x04
    reinterpret_cast<void*>(&rcr_onRenderPathChanged),         // +0x08
    reinterpret_cast<void*>(&rcr_onRenderPathNotification),    // +0x0C
    reinterpret_cast<void*>(&rcr_getCurrentRenderPath),        // +0x10
    reinterpret_cast<void*>(&rcr_getRenderPathListenerName)    // +0x14
};

} // anonymous namespace


// Initializes resources, binds the shader, then registers the render-path listener.
// The enabled flag and render path remain uninitialized; bindEffect fills the deferred handles.

RoadCompiledRenderer::RoadCompiledRenderer()
    : m_vptr(kPrimaryVtable)
    , m_listenerVptr(kListenerVtable)
    , m_refCount(1)
    , m_effect(0)
    , m_paramWorldViewProjection(0)
    , m_paramLocalEye(0)
    , m_paramTexBlendFactor(0)
    , m_paramFadeOut(0)
    , m_paramTexLayer3(0)
    , m_paramTexLayer4(0)
    , m_paramTexLayer2(0)
    , m_paramWorldSpaceCamPos(0)
    , m_paramFogRange(0)
    , m_paramFogColor(0)
    , m_paramTexProjOffset(0)
    , m_paramTexProjScale(0)
    , m_paramAmbientIlluminationRange(0)
    , m_int84(1)
    , m_hemiDirectFlag(0)
    , m_viewDistanceScale(1.3f)
    , m_depthBias(-1.0e-05f)
    , m_slopeScaleDepthBias(-1.0e-05f)
    , m_heightmapCluster(0)
    , m_terrain(0)
{
    m_worldSpaceCamPos[0] = 0.0f;
    m_worldSpaceCamPos[1] = 0.0f;
    m_worldSpaceCamPos[2] = 0.0f;
    m_worldSpaceCamPos[3] = 0.0f;
    m_fadeOut[0] = 0.0f;
    m_fadeOut[1] = 0.0f;

    bindEffect();
    dx9rend::registerRenderPathListener(static_cast<void*>(&m_listenerVptr));   // this + 0x24
}


// the shader binding. The effect through the ShaderManager (sub_10022EF0), its technique,
// then seventeen handles through Effect::getShaderParameter (sub_1001E420, each with the caller-facing
// extra reference the destructor releases), in this exact order - WorldSpaceCamPos is fetched after
// fogColor although its slot is +0x50.
// No null test anywhere: a failed effect load faults in the technique selection, as shipped.

void RoadCompiledRenderer::bindEffect()
{
    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/shaders/RoadCompiled"));
    dx9rend::effectSetTechniqueByName(m_effect, std::string("roadcompiledFull"));

    m_paramWorldViewProjection =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WorldViewProjection"));
    m_paramLocalEye       = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("LocalEye"));
    m_paramTexBlendFactor = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TexBlendFactor"));
    m_paramFadeOut        = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("FadeOut"));
    m_paramTexLayer3      = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER3"));
    m_paramTexLayer4      = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER4"));
    m_paramTexLayer2      = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXLAYER2"));
    m_paramScaleY         = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("SCALEY"));
    m_paramSunColor       = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("SUNCOLOR"));
    m_paramGIColor        = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("GICOLOR"));
    m_paramFogRange       = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("fogRange"));
    m_paramFogColor       = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("fogColor"));
    m_paramWorldSpaceCamPos =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WorldSpaceCamPos"));
    m_paramTexProjOffset  = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXPROJOFFSET"));
    m_paramTexProjScale   = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXPROJSCALE"));
    m_paramSinglePointColor =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("SINGLEPOINTCOLOR"));
    m_paramAmbientIlluminationRange =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("AmbientIlluminationRange"));
}


// the destructor body. Each handle released through its +0x08 when present, in the
// shipped (non-declaration) order, with the effect released between fogColor and texprojoffset; then
// the listener unregistered (RendDX9_unregisterRenderPathListener, this + 0x24), and the members: the
// road list (0x1000EAD0), then the name.

RoadCompiledRenderer::~RoadCompiledRenderer()
{
    if (m_paramTexLayer2)                m_paramTexLayer2->release();                 // +0x4C
    if (m_paramTexLayer4)                m_paramTexLayer4->release();                 // +0x48
    if (m_paramTexLayer3)                m_paramTexLayer3->release();                 // +0x44
    if (m_paramFadeOut)                  m_paramFadeOut->release();                   // +0x40
    if (m_paramTexBlendFactor)           m_paramTexBlendFactor->release();            // +0x3C
    if (m_paramWorldViewProjection)      m_paramWorldViewProjection->release();       // +0x34
    if (m_paramLocalEye)                 m_paramLocalEye->release();                  // +0x38
    if (m_paramWorldSpaceCamPos)         m_paramWorldSpaceCamPos->release();          // +0x50
    if (m_paramScaleY)                   m_paramScaleY->release();                    // +0x54
    if (m_paramSunColor)                 m_paramSunColor->release();                  // +0x58
    if (m_paramGIColor)                  m_paramGIColor->release();                   // +0x5C
    if (m_paramFogRange)                 m_paramFogRange->release();                  // +0x60
    if (m_paramFogColor)                 m_paramFogColor->release();                  // +0x64
    if (m_effect)                        dx9rend::effectRelease(m_effect);            // +0x30
    if (m_paramTexProjOffset)            m_paramTexProjOffset->release();             // +0x68
    if (m_paramTexProjScale)             m_paramTexProjScale->release();              // +0x6C
    if (m_paramSinglePointColor)         m_paramSinglePointColor->release();          // +0x70
    if (m_paramAmbientIlluminationRange) m_paramAmbientIlluminationRange->release();  // +0x74

    dx9rend::unregisterRenderPathListener(static_cast<void*>(&m_listenerVptr));
}

// at zero the destructor body runs and the block goes back to the engine heap.
int RoadCompiledRenderer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~RoadCompiledRenderer();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// 0x10145610, a comdat-folded body shared with MeshRendererCommon: iid 1 and iid 820013 answer `this`.
void* RoadCompiledRenderer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IRoadCompiledRenderer)
        return const_cast<RoadCompiledRenderer*>(this);
    return 0;
}


// the sorted insert RoadCompiled::addToRenderQueue calls. The road goes in front of the
// first queued road whose template priority (+0x6C) is greater, or - under the same template - whose
// order (+0x190) is greater; otherwise at the end. Both compares are signed (`jl`). A different
// template of equal priority does not stop the walk, so equal-priority templates interleave in
// arrival order.

void RoadCompiledRenderer::add(RoadCompiled* road)
{
    RoadCompiledTemplate* const tmpl = road->m_template;
    RoadList::iterator where = m_roads.begin();
    if (where != m_roads.end())
    {
        const int priority = tmpl->m_priority;
        do
        {
            RoadCompiled* const queued = *where;
            RoadCompiledTemplate* const queuedTemplate = queued->m_template;
            if (priority < queuedTemplate->m_priority)
                break;
            if (tmpl == queuedTemplate && road->m_order < queued->m_order)
                break;
            ++where;
        }
        while (where != m_roads.end());
    }
    m_roads.insert(where, road);                           // 0x1013B1C0 + _Incsize + the link
}


// Draws queued roads to the current render-path targets, then drains the queue.
// A disabled renderer drops the queue without drawing.

void RoadCompiledRenderer::onRenderPathNotification()
{
    if (m_roads.empty())
        return;
    if (!m_enabled)
    {
        m_roads.clear();
        return;
    }

    if (m_renderPath == 3)
    {
        m_paramTexLayer2->setValue(dx9rend::rendererGetHemiScratchTexture(), 0);
    }
    else if (m_renderPath == 2)
    {
        dx9rend::stateManagerSetRenderTarget(1, 0);
        dx9rend::stateManagerSetRenderTarget(2, 0);
        dx9rend::stateManagerSetRenderTarget(3, 0);
        lightManagerSetRenderTargetTexture(0, rendererPathTarget780());
    }

    drawAll();
}


// Draws queued roads with the duplicate render view, then clears the queue.
// Without terrain, returns before clearing. Distance culling uses the file-space origin;
// template changes update blending even when both templates use the same textures.

void RoadCompiledRenderer::drawAll()
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();                // +0x50
    const Mat4* const inverseView = view->getInverseViewMatrixPtr();                         // +0x68
    const Mat4* const projection  = view->getProjectionMatrixPtr();                          // +0x6C

    Mat4 viewProjection;
    viewProjection.fullMult(*inverseView, *projection);

    Mat4 cameraWorld = *inverseView;
    {
        const Mat4 copy(cameraWorld);
        fullInvert(copy, cameraWorld);
    }

    const float* const packed = staticMeshPackVec4(&view->getViewMatrixPtr()->m[12], 1.0f);  // +0x64
    Vec3 camera;
    camera.x = packed[0];
    camera.y = packed[1];
    camera.z = packed[2];

    m_terrain = terrain();                                                                   // +0x9C
    if (!g_Terrain)
        return;
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor-only region (agents.md, Editor): the editor's TerrainEditable has no
    // culler (Geom/TerrainEditable.cpp, region 2), whose view distance this walk and updateFrameConstants read
    // unguarded, as shipped; no compiled road is drawn over it (RoadCompiled::updateChunkVisibility queues none).
    if (!m_terrain->m_culler)
    {
        m_roads.clear();
        return;
    }
#endif

    dx9rend::stateManagerPush();
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, m_renderPath == 2 ? 1u : 0u);
    updateFrameConstants();

    RoadCompiledTemplate* last = 0;
    for (RoadList::iterator node = m_roads.begin(); node != m_roads.end(); ++node)
    {
        RoadCompiled* const road = *node;
        RoadCompiledTemplate* const tmpl = road->m_template;

        terrainOriginScratch(camera);                                                        // inlined sub_100E11B0

        const float dx = road->m_origin.x - camera.x;
        const float dy = road->m_origin.y - camera.y;
        const float dz = road->m_origin.z - camera.z;
        const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
        const float limit = static_cast<float>(m_terrain->m_culler->param18) * m_viewDistanceScale
                          + road->m_radius;
        if (limit < distance)
            continue;

        if (!last || last != tmpl)
        {
            m_paramTexBlendFactor->setFloat(tmpl->m_blendFactor);                           // +0x24
            if (!last)
            {
                m_paramTexLayer3->setValue(tmpl->m_texture0, 0);                             // +0x18
                m_paramTexLayer4->setValue(tmpl->m_texture1, 0);
            }
            else
            {
                if (last->m_texture0 != tmpl->m_texture0)
                    m_paramTexLayer3->setValue(tmpl->m_texture0, 0);
                if (last->m_texture1 != tmpl->m_texture1)
                    m_paramTexLayer4->setValue(tmpl->m_texture1, 0);
            }
            last = tmpl;
        }

        Mat4 worldViewProjection;
        worldViewProjection.fullMult(road->m_transform, viewProjection);
        m_paramWorldViewProjection->setValue(&worldViewProjection, 0);

        Mat4 inverseTransform = road->m_transform;
        {
            const Mat4 copy(inverseTransform);
            fullInvert(copy, inverseTransform);
        }

        float eye[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        transformVec4(cameraWorld, eye, eye);
        transformVec4(inverseTransform, eye, eye);
        m_paramLocalEye->setValue(eye, 0);

        road->draw();
    }

    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
    m_roads.clear();                                                                         // inline 0x1013B180
}


// Updates road fade, lighting, fog, projection and depth-bias constants.
// Requires terrain, its culler, renderer targets and Ra constants. Projection offset is
// cached from the first target size, even if later frame dimensions change.

void RoadCompiledRenderer::updateFrameConstants()
{
    if (!m_heightmapCluster)
        m_heightmapCluster = ::dice::hfe::classManager->getSingleton(
            ::renddx9abi::HostString("HeightmapCluster"));                                 // +0x48

    const TerrainCuller* const culler = m_terrain->m_culler;
    const float viewDistance = static_cast<float>(culler->param18) * m_viewDistanceScale;
    const float fadeStart = viewDistance * culler->lodFactor20;
    const float scale = 1.0f / (viewDistance - fadeStart);
    m_fadeOut[0] = scale;
    m_fadeOut[1] = scale * fadeStart;
    m_paramFadeOut->setValue(m_fadeOut, 0);

    m_paramScaleY->setFloat(m_terrain->getPrimaryWorldScale()->y);                          // +0x38

    {
        Vec3 raw;
        const float* const packed = staticMeshPackVec4(&m_terrain->getSunColorRaw(&raw)->x, 1.0f);   // +0x78
        const float sun[4] = { packed[0], packed[1], packed[2], packed[3] };
        m_paramSunColor->setValue(sun, 0);
    }
    {
        Vec3 raw;
        const float* const packed = staticMeshPackVec4(&m_terrain->getGIColorRaw(&raw)->x, 1.0f);    // +0x84
        const float gi[4] = { packed[0], packed[1], packed[2], packed[3] };
        m_paramGIColor->setValue(gi, 0);
    }

    m_paramFogRange->setValue(raConstantAt(kRaFogRange), 0);
    m_paramFogColor->setValue(raConstantAt(kRaFogColor), 0);
    m_paramWorldSpaceCamPos->setValue(m_worldSpaceCamPos, 0);

    dx9rend::EngineTexture* const hemi = dx9rend::rendererGetHemiScratchTexture();          // g_Renderer +0x774
    float halfTexel[2];
    halfTexel[0] = 0.5f / static_cast<float>(static_cast<unsigned short>(hemi->width));
    halfTexel[1] = 0.5f / static_cast<float>(static_cast<unsigned short>(hemi->height));

    static unsigned int s_texProjOnce = 0;          // dword_1025DF6C
    static float s_texProjScale[4];                 // dword_1025DF5C
    static float s_texProjOffset[4];                // flt_1025DF4C
    if ((s_texProjOnce & 1u) == 0)
    {
        s_texProjOnce |= 1u;
        s_texProjScale[0] = 0.5f;
        s_texProjScale[1] = -0.5f;
        s_texProjScale[2] = 1.0f;
        s_texProjScale[3] = 1.0f;
    }
    if ((s_texProjOnce & 2u) == 0)
    {
        s_texProjOnce |= 2u;
        s_texProjOffset[2] = 0.0f;
        s_texProjOffset[3] = 0.0f;
        s_texProjOffset[0] = halfTexel[0] + 0.5f;
        s_texProjOffset[1] = halfTexel[1] + 0.5f;
    }
    m_paramTexProjScale->setValue(s_texProjScale, 0);
    m_paramTexProjOffset->setValue(s_texProjOffset, 0);

    m_paramSinglePointColor->setValue(raConstantAt(kRaSinglePointColor), 0);
    m_paramTexLayer2->setValue(dx9rend::rendererGetHemiScratchTexture(), 0);

    {
        const float* const holder = reinterpret_cast<const float*>(
            raConstantAt(kRaAmbientIlluminationRange));
        const float range[4] = { holder[0], holder[1], holder[2], holder[3] };
        m_paramAmbientIlluminationRange->setValue(range, 0);
    }

    dx9rend::stateManagerSetRenderState(195u, floatBits(m_depthBias));                       // D3DRS_DEPTHBIAS
    dx9rend::stateManagerSetRenderState(175u, floatBits(m_slopeScaleDepthBias));             // D3DRS_SLOPESCALEDEPTHBIAS
}


// The seams the rest of the image uses.


// RoadCompiled::addToRenderQueue's insert (0x1015AE0B): the singleton is not tested.
void roadCompiledRendererAdd(RoadCompiled* road)
{
    static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))->add(road);
}

// RoadCompiled::updateChunkVisibility's `mov edx, g_RoadCompiledRenderer; fmul [edx+8Ch]` (0x1015AEA3).
float roadCompiledRendererViewDistanceScale()
{
    return static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
        ->m_viewDistanceScale;
}

// renderFrame's `g_RoadCompiledRenderer + 36`: the listener subobject whose +0x0C it calls with 0.
void* roadCompiledRendererListener()
{
    return &static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
        ->m_listenerVptr;
}

// Renderer::roadQueuePass (BF2Render.cpp, 0x1002A560) - the three things it reads off the singleton:
// the fade pair (+0xB0/+0xB4, sub_10038810) and the two bias words (+0x90/+0x94) it hands the
// StateManager as their bit patterns. Unguarded; roadQueuePass tests the singleton first.
const float* roadCompiledRendererFadeOut()
{
    return static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))->m_fadeOut;
}

unsigned int roadCompiledRendererDepthBiasBits()
{
    return floatBits(static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
                         ->m_depthBias);
}

unsigned int roadCompiledRendererSlopeScaleDepthBiasBits()
{
    return floatBits(static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
                         ->m_slopeScaleDepthBias);
}

void roadCompiledRendererSetHemiEnabled(unsigned char enabled)
{
    // Intentionally unguarded, matching HemiMapManager's +0x18 singleton dispatch.
    static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))->setEnabled(enabled);
}

unsigned char roadCompiledRendererGetHemiDirectFlag()
{
    // Capture is the one retail arm which guards the road singleton; absence records inverse=1.
    if (!g_RoadCompiledRenderer)
        return 0;
    return static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
        ->getHemiDirectFlag();
}

void roadCompiledRendererSetHemiDirectFlag(unsigned char value)
{
    // The direct +0x88 byte store is guarded in retail, unlike the +0x18 virtual call above.
    if (g_RoadCompiledRenderer)
        static_cast<RoadCompiledRenderer*>(static_cast<void*>(g_RoadCompiledRenderer))
            ->setHemiDirectFlag(value);
}

// RefFactory_geom_RoadCompiledRenderer, 0x100B0A9C: `if (outer) return 0`, allocate the object
// (retail 0xB8 bytes), construct,
// then queryInterface through the new object without testing the allocation (a failed allocation
// faults at the QI, as shipped), and release it when the interface is refused.
IBase* __fastcall createRoadCompiledRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RoadCompiledRenderer));
    RoadCompiledRenderer* self = 0;
    if (storage)
        self = new (storage) RoadCompiledRenderer();

    void* const itf = self->queryInterface(iid);                                             // [eax+0Ch]
    if (!itf)
        self->release();                                                                     // [eax+8]
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
