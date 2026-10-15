// Viewport overlay texture with device-reset handling; five virtual operations remain frontiers.
// [INFERRED] Source placement.
// [RETIRE-WHEN: RW-81] Confirm source placement and implement and validate the remaining overlay operations.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../BF2/Math/Vec3.h"                // Vec2 - setSize's argument
#include "../../BF2/RendDX9/RendDX9Base.h"      // textureManagerGetTexture, the effect/listener seams
#include "../../BF2/RendDX9/ShaderParameter.h"  // the TEXLAYER0 / blendalpha handles
#include "../../BF2/RendDX9/Texture.h"          // EngineTexture, textureReleaseRef
#include "../../BF2/RendMisc/IRenderView.h"     // the viewport the rebuild maps the rect through

#include <cmath>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
// The BufferManager / Effect / renderer seams. All exist in the tree (BufferManager.cpp,
// ShaderManager.cpp, StateManager.cpp) except the two marked NEW, whose bodies the dc_console report
// gives for Effect.cpp. g_shaderManager is dword_1023B97C.
extern void* g_shaderManager;
int   bufferManagerSwapPool(int value);                             // sub_100113B0 - BufferManager+0x08
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);   // sub_10011FF0
void  vertexBufferRelease(void* wrapper);                           // the inline --ref, sub_10014510, free
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);                         // sub_10014630
bool  vertexBufferUnlock(void* wrapper);                            // sub_10014690
void  vertexBufferBindStream(void* wrapper, unsigned int stream);   // sub_100146C0
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);              // sub_10005870 on dword_1023B970
void  effectBeginCurrentTechniqueStates(void* effect);              // NEW - sub_1001DB60
void  effectEndCurrentTechniqueStates(void* effect);                // NEW - sub_1001DB70
} // namespace dx9rend

namespace geom {

// createInstance(81008, 80002) in BF2Render_initializeFontAndConsoleOverlay. The classId is read as
// data at 0x1007D866 - dword_1020AEB0 == 0x13C70 == 81008.
extern const unsigned int IID_IOverlayTexture = 80002u;
extern const unsigned int CID_OverlayTexture  = 81008u;

// One vertex of the overlay's six-vertex buffer - the same 28-byte FLOAT4 position / D3DCOLOR color /
// FLOAT2 TEXCOORD0 declaration PortedFontShader.fx's "Overlay" technique annotates (the rebuild asks
// createVertexBuffer for stride 0x1C). The position is clip space, w = 1.
struct OverlayVertex
{
    float        x, y, z, w;   // +0x00
    unsigned int colour;       // +0x10  never written - see the rebuild
    float        u, v;         // +0x14
};

static_assert(sizeof(OverlayVertex) == 28, "createVertexBuffer(6, 28, ...) - the stride");


// Draws an overlay using normalized viewport position and size.
// Preserve the secondary DeviceLostResetListener base and reference-count layout.

class OverlayTexture : public IBase, public dx9rend::DeviceLostResetListener
{
public:
    OverlayTexture();
    ~OverlayTexture();                                        // not a slot

    virtual void  addRef();                                   // +0x00  0x1001A010  (++[this+8])
    virtual int   getRef();                                   // +0x04  0x10051870  ([this+8])
    virtual int   release();                                  // +0x08
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C

    // (dc_console). [INFERRED] name; `ret` with no arguments.
    virtual void draw();                                      // +0x10
    // read, left a named stop: returns the size pair {+0x14, +0x18} by value (hidden pointer, `retn 4`).
    virtual void slot_0x14();
    // (dc_console). [INFERRED] name; `retn 4`, one pointer to two floats.
    virtual void setSize(const Vec2& size);                   // +0x18
    // read, left named stops: +0x1C returns the position pair {+0x0C, +0x10} by value; +0x20 is
    // setPosition(const Vec2&) - setSize's body on +0x0C/+0x10, setting +0x56; +0x24 returns the
    // alpha (`fld [ecx+20h]`); +0x30 returns &m_textureName (`lea eax, [ecx+30h]`).
    virtual void slot_0x1C();
    virtual void slot_0x20();
    virtual void slot_0x24();
    virtual void setAlpha(float alpha);                       // +0x28  0x100880E0   read
    virtual void setTexture(const std::string& name);        // +0x2C  0x10088100   read
    virtual void slot_0x30();                                 // 0x10088150   the last slot

    // The DeviceLostResetListener at +0x04 (vtable 0x1020B110). .
    virtual void onDeviceLost();
    virtual void onDeviceReset();
    virtual void onDeviceDestroy();

private:
    // sub_100882D0 (813 B, `retn 4`) - (re)build the six vertices. [INFERRED] name. Returns false only
    // when the lock fails.
    bool rebuild(bool force);

    int                       m_refCount;       // +0x08  ctor: 1
    float                     m_rectX;          // +0x0C  ctor: 0    - position, viewport fraction
    float                     m_rectY;          // +0x10  ctor: 0
    float                     m_rectW;          // +0x14  ctor: 1.0  - size, viewport fraction
    float                     m_rectH;          // +0x18  ctor: 1.0
    unsigned int              m_unknown1C;      // +0x1C  ctor: 2 - no recovered body reads it
    float                     m_alpha;          // +0x20  ctor: 0.5 - setAlpha writes this
    void*                     m_vertexBuffer;   // +0x24  ctor: 0, built by the rebuild
    void*                     m_texture;        // +0x28  ctor: 0, setTexture writes this (EngineTexture*)
    dx9rend::Effect*          m_effect;         // +0x2C  the PortedFontShader effect
    std::string              m_textureName;    // +0x30  setTexture compares and assigns this
    dx9rend::ShaderParameter* m_paramTexLayer0; // +0x4C  the "TEXLAYER0" handle
    dx9rend::ShaderParameter* m_paramBlendAlpha;// +0x50  the "BLENDALPHA" handle
    unsigned char             m_alphaDirty;     // +0x54  ctor: 1, setAlpha sets it; no reader found
    unsigned char             m_sizeDirty;      // +0x55  ctor: 1, setSize sets it, the rebuild clears it
    unsigned char             m_positionDirty;  // +0x56  ctor: 1, setPosition sets it, the rebuild clears it
    unsigned char             m_dirty57;        // +0x57  ctor: 1
    unsigned char             m_textureDirty;   // +0x58  ctor: 1, setTexture sets it
    unsigned char             m_pad59[3];
    unsigned int              m_cachedX;        // +0x5C  the viewport the vertex buffer was built for
    unsigned int              m_cachedY;        // +0x60
    unsigned int              m_cachedW;        // +0x64
    unsigned int              m_cachedH;        // +0x68
};




// Creates the overlay buffer and shader parameters, retains the texture, and registers for device resets.
// Requires valid renderer services and effect parameters; buffer rebuild failure is not propagated.

OverlayTexture::OverlayTexture()
    : m_refCount(1)
    , m_rectX(0.0f)
    , m_rectY(0.0f)
    , m_rectW(1.0f)
    , m_rectH(1.0f)
    , m_unknown1C(2)
    , m_alpha(0.5f)
    , m_vertexBuffer(0)
    , m_texture(0)
    , m_effect(0)
    , m_paramTexLayer0(0)
    , m_paramBlendAlpha(0)
    , m_alphaDirty(1)
    , m_sizeDirty(1)
    , m_positionDirty(1)
    , m_dirty57(1)
    , m_textureDirty(1)
    , m_cachedX(0)
    , m_cachedY(0)
    , m_cachedW(0)
    , m_cachedH(0)
{
    m_pad59[0] = 0; m_pad59[1] = 0; m_pad59[2] = 0;

    rebuild(false);

    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/Shaders/PortedFontShader"));

    m_paramTexLayer0 =
        dx9rend::effectGetShaderParameterBorrowedTyped(m_effect, std::string("TEXLAYER0"));
    m_paramTexLayer0->addRef();

    m_paramBlendAlpha =
        dx9rend::effectGetShaderParameterBorrowedTyped(m_effect, std::string("BLENDALPHA"));
    m_paramBlendAlpha->addRef();

    dx9rend::registerDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this), 4);
}


// Releases the buffer, shader parameters, effect and texture, then unregisters the device listener.
// The renderer must remain alive through destruction.

OverlayTexture::~OverlayTexture()
{
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_paramTexLayer0)
        m_paramTexLayer0->release();
    if (m_paramBlendAlpha)
        m_paramBlendAlpha->release();
    if (m_effect)
        dx9rend::effectRelease(m_effect);

    dx9rend::EngineTexture* texture = static_cast<dx9rend::EngineTexture*>(m_texture);
    dx9rend::textureReleaseRef(texture);                    // tests the pointer itself, as retail does

    dx9rend::unregisterDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this));
}

void OverlayTexture::addRef() { ++m_refCount; }
int  OverlayTexture::getRef() { return m_refCount; }

// release - 0x10088830: `--[this+8]`; at zero the destructor 0x10088170 and then sub_1005F360 (the
// engine free); returns the decremented count either way.
int OverlayTexture::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~OverlayTexture();
        ::renddx9abi::engineFree(this);                                      // sub_1005F360
    }
    return remaining;
}

void* OverlayTexture::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IOverlayTexture)
        return const_cast<OverlayTexture*>(this);
    return 0;
}


// Rebuilds the overlay quad when forced, dirty or the viewport changes.
// Returns false if the vertex-buffer lock fails. Cache and dirty state are updated
// Before locking, so a failed lock can leave stale vertices accepted on the next call.
// Vertex colour is intentionally unwritten; the shader uses texture colour and tfactor.

bool OverlayTexture::rebuild(bool force)
{
    if (!m_vertexBuffer)
    {
        const int savedPool = dx9rend::bufferManagerSwapPool(0);            // D3DPOOL_DEFAULT
        m_vertexBuffer =
            dx9rend::bufferManagerCreateVertexBuffer(6, 28, 0x208, "OverlayTexture");
        dx9rend::bufferManagerSwapPool(savedPool);
    }

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
    const unsigned int width  = static_cast<unsigned int>(static_cast<int>(view->getViewport()[2]));
    const unsigned int height = static_cast<unsigned int>(static_cast<int>(view->getViewport()[3]));
    const unsigned int left   = static_cast<unsigned int>(static_cast<int>(view->getViewport()[0]));
    const unsigned int top    = static_cast<unsigned int>(static_cast<int>(view->getViewport()[1]));

    if (!force && !m_sizeDirty && !m_positionDirty &&
        left == m_cachedX && top == m_cachedY && width == m_cachedW && height == m_cachedH)
        return true;

    m_cachedX = left;
    m_cachedY = top;
    m_cachedW = width;
    m_cachedH = height;
    m_sizeDirty = 0;
    m_positionDirty = 0;

    const float inverseWidth = 1.0f / static_cast<float>(width);
    const float clipScaleX = inverseWidth + inverseWidth;
    const float inverseHeight = 1.0f / static_cast<float>(height);
    const float clipScaleY = inverseHeight + inverseHeight;
    const float edgeRight  = static_cast<float>(left + width);
    const float edgeLeft   = static_cast<float>(left);
    const float x0 = ((1.0f - m_rectX) * edgeLeft + edgeRight * m_rectX) * clipScaleX - 1.0f;
    const float edgeBottom = static_cast<float>(top + height);
    const float edgeTop    = static_cast<float>(top);
    const float y0 = -(((1.0f - m_rectY) * edgeTop + edgeBottom * m_rectY) * clipScaleY - 1.0f);
    const float right = m_rectW + m_rectX;
    const float x1 = ((1.0f - right) * edgeLeft + right * edgeRight) * clipScaleX - 1.0f;
    const float bottom = m_rectH + m_rectY;
    const float y1 = -(((1.0f - bottom) * edgeTop + bottom * edgeBottom) * clipScaleY - 1.0f);

    OverlayVertex* const vertex = static_cast<OverlayVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, D3DLOCK_DISCARD));
    if (!vertex)
        return false;

    // Positions, then texture coordinates - the order the shipped stores run in. `colour` is
    // deliberately left alone (see above).
    vertex[0].x = x0; vertex[0].y = y0; vertex[0].z = 0.0f; vertex[0].w = 1.0f;
    vertex[1].x = x1; vertex[1].y = y0; vertex[1].z = 0.0f; vertex[1].w = 1.0f;
    vertex[2].x = x1; vertex[2].y = y1; vertex[2].z = 0.0f; vertex[2].w = 1.0f;
    vertex[3].x = x1; vertex[3].y = y1; vertex[3].z = 0.0f; vertex[3].w = 1.0f;
    vertex[4].x = x0; vertex[4].y = y1; vertex[4].z = 0.0f; vertex[4].w = 1.0f;
    vertex[5].x = x0; vertex[5].y = y0; vertex[5].z = 0.0f; vertex[5].w = 1.0f;
    vertex[0].u = 0.0f; vertex[0].v = 0.0f;
    vertex[1].u = 1.0f; vertex[1].v = 0.0f;
    vertex[2].u = 1.0f; vertex[2].v = 1.0f;
    vertex[3].u = 1.0f; vertex[3].v = 1.0f;
    vertex[4].u = 0.0f; vertex[4].v = 1.0f;
    vertex[5].u = 0.0f; vertex[5].v = 0.0f;

    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    return true;
}


// Draws the overlay with its alpha setting. Requires initialized texture and shader resources.
// A buffer rebuild may occur inside the pass; its result is ignored.

void OverlayTexture::draw()
{
    dx9rend::stateManagerPush();
    dx9rend::effectSetTechniqueByName(m_effect, std::string("Overlay"));
    m_paramTexLayer0->setValue(m_texture, 0);

    float blendAlpha[4];
    blendAlpha[3] = m_alpha;                                                 // [ebp-4]
    blendAlpha[0] = 0.0f;
    blendAlpha[1] = 0.0f;
    blendAlpha[2] = 0.0f;
    m_paramBlendAlpha->setValue(blendAlpha, 0);

    dx9rend::effectBeginCurrentTechniqueStates(m_effect);                   // "Overlay_States"
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    rebuild(false);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLELIST, 0);
    dx9rend::effectEnd(m_effect);
    dx9rend::effectEndCurrentTechniqueStates(m_effect);
    dx9rend::stateManagerPop();
}


// Updates size and marks the texture dirty when the combined difference exceeds epsilon or is NaN.

void OverlayTexture::setSize(const Vec2& size)
{
    const float kEpsilon = 1.1920929e-07f;                                   // 0x34000000 at 0x101E05A0
    if (!(std::fabs(m_rectH - size.y) + std::fabs(m_rectW - size.x) <= kEpsilon))
    {
        m_sizeDirty = 1;
        m_rectH = size.y;
        m_rectW = size.x;
    }
}


// +0x28 setAlpha, 0x100880E0. Three statements, and the guard is the whole of it: the dirty byte is
// set only when the value actually changes.

void OverlayTexture::setAlpha(float alpha)
{
    RD9_TRACE("dice::hfe::geom::OverlayTexture::setAlpha", 0x28);

    if (m_alpha != alpha)
    {
        m_alphaDirty = 1;
        m_alpha      = alpha;
    }
}


// Loads a texture when the name changes, using the manager's current class and pool.
// Replacing the texture does not release the previous reference.

void OverlayTexture::setTexture(const std::string& name)
{
    RD9_TRACE("dice::hfe::geom::OverlayTexture::setTexture", 0x2C);

    if (::strcmp(m_textureName.c_str(), name.c_str()) != 0)
    {
        m_textureName  = name;
        m_textureDirty = 1;
        m_texture      = dx9rend::textureManagerGetTexture(name);

        ::renddx9abi::logf("OverlayTexture::setTexture(\"%s\"): loaded -> 0x%p",
                           name.c_str(), m_texture);
    }
}


// BufferManager handles device loss. Reset refills the overlay buffer at priority 4, after BufferManager.

void OverlayTexture::onDeviceLost()
{
}

void OverlayTexture::onDeviceReset()
{
    rebuild(true);
}

void OverlayTexture::onDeviceDestroy()
{
}

#define RD9_OT_FRONTIER(method, slotOffset)                                     \
    void OverlayTexture::method()                                               \
    {                                                                           \
        RD9_FRONTIER("dice::hfe::geom::OverlayTexture::" #method, slotOffset);   \
    }

RD9_OT_FRONTIER(slot_0x14, 0x14)
RD9_OT_FRONTIER(slot_0x1C, 0x1C)
RD9_OT_FRONTIER(slot_0x20, 0x20)
RD9_OT_FRONTIER(slot_0x24, 0x24)
RD9_OT_FRONTIER(slot_0x30, 0x30)

#undef RD9_OT_FRONTIER

// RefFactory_geom_OverlayTexture, 0x1007C860.
IBase* __fastcall createOverlayTexture(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    OverlayTexture* self = new OverlayTexture();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

// Typed overlay adapters used by the renderer's console drawing path.
void overlayTextureSetSize(IBase* overlay, const Vec2& size)
{
    static_cast<OverlayTexture*>(overlay)->setSize(size);
}

void overlayTextureDraw(IBase* overlay)
{
    static_cast<OverlayTexture*>(overlay)->draw();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
