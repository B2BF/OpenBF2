// Owns camera and projection state used by renderer views.
// The renderer constructs its primary view and obtains a separate secondary instance.

#ifdef BF2_IMAGE_RENDDX9

#include "IRenderView.h"
#if !defined(_MSC_VER) || !defined(_M_IX86)
#include "../Math/PortableX87.h"
#endif
#include "../Math/Frustum.h"
#include "../RendDX9/RendDX9Math.h"       // the RendDX9.dll Math copies
#include "../Platform/X87Compat.h"          // [CHOSEN, not recovered] the projection quotient
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"

#include <math.h>
#include <stddef.h>
#include <cstring>
#include <cstdlib>
#include <new>                                 // renderViewCopyConstruct's placement copy

namespace dice {
namespace hfe {
namespace Rend {

// Read from the registration RendModule::getSingletonList passes for "RenderView", and confirmed
// from the other end by Renderer::initialize's createInstance(820301, 820201, 0) and by
// queryInterface at 0x10125AB0 testing 820201.
// extern, because a namespace-scope `const` has internal linkage in C++ and RendModule.cpp needs
// to see these. Without it the two ids resolve inside this file and nowhere else, which the linker
// reports as two unresolved externals rather than as the scoping mistake it is.
extern const unsigned int IID_IRenderView = 820201u;
extern const unsigned int CID_RenderView  = 820301u;

// 0x1023B418 == 0x3C8EFA35 == 0.017453292. Pi/180 - the constructor's default field of view is
// literally written as `DEG_TO_RAD * 60.0`, so 60 degrees.
static const float RD9_DEG_TO_RAD = 0.017453292f;


// Viewport and camera state with lazily rebuilt matrix caches.
// The x86 interface has 39 slots and the object occupies 0x430 bytes.

class RenderView : public dx9rend::IRenderView
{
public:
    RenderView();

    virtual void  addRef();                                   // +0x00  (folded)
    virtual int   getRef();                                   // +0x04  (folded)
    virtual int   release();                                  // +0x08  (folded)
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C  0x10125AB0   read

    virtual void setViewport(float x, float y, float width, float height) override; // +10
    virtual float* getViewport() override;                      // +14, four floats
    virtual void setAdjustedFov(float radians) override;         // +18
    virtual void setFov(float radians) override;                 // +1C
    virtual float getFov() override;                            // +20
    virtual float getReferenceFov() override;                   // +24
    virtual float getZoom() override;                           // +28
    virtual float getInverseZoom() override;                    // +2C
    virtual void setNearPlane(float value) override;            // +30
    virtual float getNearPlane() override;                      // +34
    virtual void setFarPlane(float value) override;             // +38
    virtual float getFarPlane() override;                       // +3C
    virtual void setAspect(float value) override;               // +40
    virtual float getAspect() override;                         // +44
    virtual void setProjectionMode(unsigned int mode) override; // +48, 1 = ortho
    virtual unsigned int getProjectionMode() override;          // +4C
    virtual void setOrthoWidth(float value) override;           // +50
    virtual float getOrthoWidth() override;                     // +54
    virtual void setOrthoHeight(float value) override;          // +58
    virtual float getOrthoHeight() override;                    // +5C
    virtual void setViewMatrix(const float* matrix16) override; // +60
    virtual Mat4* getViewMatrixPtr() override;                  // +64
    virtual Mat4* getInverseViewMatrixPtr() override;           // +68
    virtual Mat4* getProjectionMatrixPtr() override;            // +6C
    virtual Mat4* getProjectionTransposePtr() override;         // +70
    virtual Frustum* getFrustumPtr() override;                 // +74
    virtual dx9rend::IRenderView* clone() override;                      // +78
    virtual void setFlag42E(bool value) override;               // +7C, purpose unresolved
    virtual bool getFlag42E() override;                        // +80
    virtual Mat4* getViewProjectionMatrixPtr() override;        // +84
    virtual Mat4* getCameraMatrixPtr() override;               // +88
    virtual Mat4* getLinearProjectionMatrixPtr() override;      // +8C
    virtual Mat4* getInverseProjectionMatrixPtr() override;     // +90
    virtual void setFlag42F(bool value) override;               // +94, purpose unresolved
    virtual bool getFlag42F() override;                        // +98

private:
    void rebuildProjection();

    // byte_1023B400 / dword_10267D08 in the shipped image - file-scope there, so static here.
    // Every RenderView in the process shares one reference fov; see setFov.
    static bool  s_referenceFovValid;
    static float s_referenceFov;

    int   m_refCount;          // +0x04
    float m_viewportX;         // +0x08
    float m_viewportY;         // +0x0C
    float m_viewportW;         // +0x10
    float m_viewportH;         // +0x14
    // Orthographic mode uses the width/height stored at +0x34/+0x38.
    unsigned int m_projectionMode;   // +0x18  0 = perspective, 1 = orthographic
    float m_zoom;              // +0x1C  cot(fov/2) / cot(refFov/2); 1.0 until setFov runs
    float m_invZoom;           // +0x20  1/m_zoom, or 0 when m_zoom is 0
    float m_fov;               // +0x24  radians
    float m_nearPlane;         // +0x28
    float m_farPlane;          // +0x2C  measured from the near plane - see rebuildProjection
    float m_aspect;            // +0x30  BF2 stores height/width (0.75), so m00 multiplies by it
    float m_orthoWidth;        // +0x34  ctor: 1.0
    float m_orthoHeight;       // +0x38  ctor: 1.0

    // eight 4x4 matrices, AND THE count IS THE constructor'S, NOT A guess. It initialises eight
    // consecutive 16-float blocks at +0x3C, +0x7C, +0xBC, +0xFC, +0x13C, +0x17C, +0x1BC and +0x1FC,
    // each zeroed and then given a unit diagonal.
    Mat4 m_matrix[8];     // +0x3C .. +0x23B

    // Actual native math object; no byte-storage or duplicate Frustum overlay.
    Frustum m_frustum;          // +23C..423; PE10030F30/ELF82B1B40

    unsigned int  m_state424;  // +0x424  setViewMatrix writes 3
    unsigned int  m_dirty428;  // +0x428  setFov / setFarPlane / setAspect write 15
    unsigned char m_dirty42C;  // +0x42C  every setter writes 1
    unsigned char m_dirty42D;  // +0x42D  every setter writes 1
    unsigned char m_flag42E;   // +0x42E
    unsigned char m_flag42F;   // +0x42F
};

#if defined(_M_IX86)
static_assert(sizeof(RenderView) == 1072,
              "dx9rend_Renderer_ctor allocates 1072 for the RenderView - see 0x1002D4C0");
#endif
// Every allocation is `new RenderView` (sizeof); the published size is what callers' copy storage
// for renderViewCopyConstruct must hold (issue #7).
static_assert(sizeof(RenderView) == dx9rend::kRenderViewObjectBytes,
              "IRenderView.h publishes the RenderView object size");


// Initializes the default perspective view, eight identity matrices and the dirty state.
// PE 0x101C2B90
RenderView::RenderView()
    : m_refCount(1)
{
    m_viewportX = 0.0f;
    m_viewportY = 0.0f;
    m_viewportW = 0.0f;
    m_viewportH = 0.0f;
    m_projectionMode = 0;      // the shipped 0.0f store, bit-identical

    // THE FOV IS computed, NOT stored AS A literal. The body reads the file-scope float at
    // (0x3C8EFA35 == pi/180) and multiplies by 60.0 - so dice wrote the default as
    // sixty degrees and converted, and the stored value is 1.0471976 radians. Reproduced as the
    // multiplication rather than as the constant, because that is what the instructions say.
    m_fov       = RD9_DEG_TO_RAD * 60.0f;

    m_nearPlane = 0.0099999998f;   // the exact float the body stores, not 0.01f rounded here
    m_farPlane  = 1000.0f;
    m_aspect    = 0.75f;           // 3/4 - height over width for 4:3, matching what initialize passes

    m_zoom      = 1.0f;
    m_invZoom   = 1.0f;
    m_orthoWidth  = 1.0f;
    m_orthoHeight = 1.0f;

    // Eight identity matrices. The original writes each block's sixteen floats to 0.0 and then
    // stores 1.0 into the four diagonal slots, in that order.
    for (int m = 0; m < 8; ++m)
    {
        for (int i = 0; i < 16; ++i)
            m_matrix[m].m[i] = 0.0f;
        m_matrix[m].m[0]  = 1.0f;
        m_matrix[m].m[5]  = 1.0f;
        m_matrix[m].m[10] = 1.0f;
        m_matrix[m].m[15] = 1.0f;
    }


    m_flag42E  = 0;
    m_flag42F  = 0;
    m_state424 = 3;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;

    // Preserve the second identity initialization after the flags.
    for (int i = 0; i < 16; ++i)
        m_matrix[0].m[i] = 0.0f;
    m_matrix[0].m[0]  = 1.0f;
    m_matrix[0].m[5]  = 1.0f;
    m_matrix[0].m[10] = 1.0f;
    m_matrix[0].m[15] = 1.0f;
}

// IBase
// The first three are the same folded bodies the modules use - 0x1000CB20, 0x101A9A10, 0x10051820.

void RenderView::addRef()
{
    ++m_refCount;
}

int RenderView::getRef()
{
    return m_refCount;
}

int RenderView::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::renddx9abi::engineFree(this);
    return remaining;
}

// Two ids only, like GeometryTemplateManager's - not the module quartet's three.
void* RenderView::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IRenderView)
        return const_cast<RenderView*>(this);
    return 0;
}

// state setters

// Four stores, no return materialization and no dirty flags.
// Scratch EAX is not a boolean success result.
// PE 0x101C2A30
void RenderView::setViewport(float x, float y, float w, float h)
{
    m_viewportX = x;
    m_viewportY = y;
    m_viewportW = w;
    m_viewportH = h;
}

// Sets FOV in radians and zoom relative to the process's first FOV.
// Recomputes zoom only on change, but always stores FOV and dirties dependent caches.
// PE 0x101C2500
void RenderView::setFov(float fovRadians)
{
#if defined(_WIN32)
    if (!s_referenceFovValid)
    {
        s_referenceFov      = fovRadians;
        s_referenceFovValid = true;
    }

    if (m_fov != fovRadians)
    {
        const float halfScale = 0.5f;
        const float reference = s_referenceFov;
        float referenceHalf;
        float* zoomOut = &m_zoom;
        float* inverseOut = &m_invZoom;
        // PE101C252D..258B uses x87 fcos/fsin and retains the quotient while
        // storing zoom and computing its reciprocal. CRT overloads introduce
        // different rounding (including a non-unit zoom at the reference FOV).
#if defined(_M_IX86)
        __asm {
            fld fovRadians
            fmul halfScale
            fld reference
            fmul halfScale
            fstp referenceHalf
            fld st(0)
            fcos
            fxch st(1)
            fsin
            fdivp st(1), st(0)
            fld referenceHalf
            fcos
            fld referenceHalf
            fsin
            fdivp st(1), st(0)
            fdivp st(1), st(0)
            mov edx, zoomOut
            fst dword ptr [edx]
            fldz
            fld st(1)
            fucompp
            fnstsw ax
            test ah, 44h
            jnp zeroZoom
            fld1
            fdiv st(0), st(1)
            mov edx, inverseOut
            fstp dword ptr [edx]
            jmp zoomDone
        zeroZoom:
            mov edx, inverseOut
            mov dword ptr [edx], 0
        zoomDone:
            fstp st(0)
        }
#else
        // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). Same order: the half
        // fov stays unrounded, the half reference is stored as a float, both cotangents use the
        // double cos/sin, and the reciprocal divides the unrounded quotient (issue #7).
        const double halfFov = static_cast<double>(fovRadians) * static_cast<double>(halfScale);
        referenceHalf = static_cast<float>(static_cast<double>(reference) *
                                           static_cast<double>(halfScale));
        const double cotFov = ::cos(halfFov) / ::sin(halfFov);
        const double cotReference = ::cos(static_cast<double>(referenceHalf)) /
                                    ::sin(static_cast<double>(referenceHalf));
        const double zoom = cotFov / cotReference;
        *zoomOut = static_cast<float>(zoom);
        // fucompp + `test ah, 44h; jnp`: only an ordered zero stores the integer 0; an unordered
        // (NaN) quotient takes the reciprocal arm.
        if (zoom == 0.0)
            *inverseOut = 0.0f;
        else
            *inverseOut = static_cast<float>(1.0 / zoom);
#endif
    }

    m_fov      = fovRadians;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    if (!s_referenceFovValid)
    {
        s_referenceFov      = fovRadians;
        s_referenceFovValid = true;
    }

    if (m_fov != fovRadians)
    {
        const float halfScale = 0.5f;
        const float reference = s_referenceFov;
        float referenceHalf;
        float* zoomOut = &m_zoom;
        float* inverseOut = &m_invZoom;
        // PE101C252D..258B uses x87 fcos/fsin and retains the quotient while
        // storing zoom and computing its reciprocal. CRT overloads introduce
        // different rounding (including a non-unit zoom at the reference FOV).
        // [CHOSEN, not recovered] Preserve x87 retained cotangent quotient, zero and unordered inverse.
        bf2x87::zoom(fovRadians, halfScale, reference, *zoomOut, *inverseOut);
    }

    m_fov      = fovRadians;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;
#endif
}


// PE 0x101C2640
void RenderView::setFarPlane(float f)
{
    m_farPlane = f;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;
}


// PE 0x101C2680
void RenderView::setAspect(float a)
{
    m_aspect   = a;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;
}

// 0x101C2A50. `lea ecx, [esi+3Ch]` names the destination: matrix[0]. sub_10030E80 is a plain
// sixteen-float copy - a Mat4 assignment, nothing more.

// IT sets m_state424 TO 3, NOT m_dirty428 TO 15. Every other setter above touches the other pair.
// Read, not tidied.
// PE 0x101C2A50
void RenderView::setViewMatrix(const float* m16)
{
    for (int i = 0; i < 16; ++i)
        m_matrix[0].m[i] = m16[i];

    m_state424 = 3;
    m_dirty42C = 1;
    m_dirty42D = 1;
}

// Complete leaf reads are retained in hud_render_view_20260906/renderer_table.json.
float* RenderView::getViewport() { return &m_viewportX; }
// PE 0x101C24B0
void RenderView::setAdjustedFov(float radians)
{
    // [CHOSEN, not recovered] Extend the wide-screen cap to 2560:1080 (issue #33),
    // following marekzajac97/BF2-HUD-fix. Keep the existing narrow-screen behavior.
    constexpr float wideAspect = 2560.0f / 1080.0f;
#if defined(_WIN32)
    float scale;
    if (m_aspect <= 1.0f / wideAspect)
        scale = wideAspect;
    else if (m_aspect >= 0.80000001f)
        scale = 1.3333334f;
    else {
        setFov(radians);
        return;
    }
    const float aspect = m_aspect;
    // The two products remain in x87 until the by-value float argument store.
#if defined(_M_IX86)
    __asm {
        fld radians
        fmul aspect
        fmul scale
        fstp radians
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates) (issue #7).
    radians = static_cast<float>(static_cast<double>(radians) * static_cast<double>(aspect) *
                                 static_cast<double>(scale));
#endif
    setFov(radians);
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    float scale;
    if (m_aspect <= 1.0f / wideAspect)
        scale = wideAspect;
    else if (m_aspect >= 0.80000001f)
        scale = 1.3333334f;
    else {
        setFov(radians);
        return;
    }
    const float aspect = m_aspect;
    // The two products remain in x87 until the by-value float argument store.
    // [CHOSEN, not recovered] Preserve x87 retained FOV products.
    radians = bf2x87::store(bf2x87::mul(bf2x87::mul(bf2x87::wide(radians), bf2x87::wide(aspect)), bf2x87::wide(scale)));
    setFov(radians);
#endif
}
float RenderView::getFov() { return m_fov; }
float RenderView::getReferenceFov() { return s_referenceFov; }
float RenderView::getZoom() { return m_zoom; }
float RenderView::getInverseZoom() { return m_invZoom; }
// PE 0x101C2600
void RenderView::setNearPlane(float value)
{
    m_nearPlane = value;
    m_dirty428 = 15;
    m_dirty42C = 1;
    m_dirty42D = 1;
}
float RenderView::getNearPlane() { return m_nearPlane; }
float RenderView::getFarPlane() { return m_farPlane; }
float RenderView::getAspect() { return m_aspect; }
// PE 0x101C26C0
void RenderView::setProjectionMode(unsigned int mode)
{
    if (m_projectionMode != mode) {
        m_projectionMode = mode;
        m_dirty428 = 15;
        m_dirty42C = 1;
        m_dirty42D = 1;
    }
}
unsigned int RenderView::getProjectionMode() { return m_projectionMode; }
// PE 0x101C2700
void RenderView::setOrthoWidth(float value)
{
    if (m_orthoWidth != value) {
        m_orthoWidth = value;
        m_dirty428 = 15;
        m_dirty42C = 1;
        m_dirty42D = 1;
    }
}
float RenderView::getOrthoWidth() { return m_orthoWidth; }
// PE 0x101C2750
void RenderView::setOrthoHeight(float value)
{
    if (m_orthoHeight != value) {
        m_orthoHeight = value;
        m_dirty428 = 15;
        m_dirty42C = 1;
        m_dirty42D = 1;
    }
}
float RenderView::getOrthoHeight() { return m_orthoHeight; }
// PE 0x101C29F0
void RenderView::setFlag42E(bool value) { m_flag42E = value; }
bool RenderView::getFlag42E() { return m_flag42E != 0; }
// PE 0x101C2A10
void RenderView::setFlag42F(bool value) { m_flag42F = value; }
bool RenderView::getFlag42F() { return m_flag42F != 0; }

// +0x68, PE 0x101C27B0. Bit zero of +0x424 says the inverse of the view matrix is stale.
// The shipped body hands the four rows at +0x3C to PE 0x100780D0, RendDX9's rigid-transform
// inverse (transposed rotation, translation -(row . position)), stores the result at +0x7C, clears
// only that bit, and returns the cached matrix.
Mat4* RenderView::getInverseViewMatrixPtr()
{
    if ((m_state424 & 1u) != 0)
    {
        const float* const view = m_matrix[0].m;
        rendmath::setInverseFromRows(m_matrix[1], *reinterpret_cast<const Vec3*>(&view[0]),
                                     *reinterpret_cast<const Vec3*>(&view[4]),
                                     *reinterpret_cast<const Vec3*>(&view[8]),
                                     *reinterpret_cast<const Vec3*>(&view[12]));
        m_state424 &= ~1u;
    }
    return &m_matrix[1];
}

// PE 0x101C2A90. Shares dirty bit 2 with the linear projection cache.
Mat4* RenderView::getProjectionTransposePtr()
{
    rebuildProjection();
    if ((m_dirty428 & 4u) != 0)
    {
        m_matrix[6] = m_matrix[4];
        const Mat4 source = m_matrix[6];
        rendmath::setTranspose(m_matrix[6], source);           // PE 0x10077910
        m_dirty428 &= ~4u;
    }
    return &m_matrix[6];
}

// PE 0x101C2F80. Refreshes the source frustum before copying its dirty flags.
dx9rend::IRenderView* RenderView::clone()
{
    RenderView* const view = new (std::nothrow) RenderView();
    // shipped defect: allocation failure is not checked before the first store.
    view->m_viewportX = m_viewportX;
    view->m_viewportY = m_viewportY;
    view->m_viewportW = m_viewportW;
    view->m_viewportH = m_viewportH;
    view->m_projectionMode = m_projectionMode;
    view->m_fov = m_fov;
    view->m_nearPlane = m_nearPlane;
    view->m_farPlane = m_farPlane;
    view->m_aspect = m_aspect;
    view->m_orthoWidth = m_orthoWidth;
    view->m_orthoHeight = m_orthoHeight;
    view->m_matrix[0] = m_matrix[0];
    view->m_matrix[1] = m_matrix[1];
    view->m_matrix[2] = m_matrix[2];
    view->m_matrix[4] = m_matrix[4];
    view->m_matrix[5] = m_matrix[5];
    view->m_matrix[7] = m_matrix[7];
    view->m_matrix[6] = m_matrix[6];
    rendmath::frustumAssign(view->m_frustum, *getFrustumPtr());
    view->m_state424 = m_state424;
    view->m_dirty428 = m_dirty428;
    view->m_dirty42C = m_dirty42C;
    view->m_dirty42D = m_dirty42D;
    view->m_flag42E = m_flag42E;
    // shipped defect: zoom, inverse zoom, matrix[3] and flag42F keep constructor values,
    // even when the copied dirty flags say that matrix[3] is current.
    return view;
}
// Returns cached world-to-view times projection, rebuilding dirty inputs through virtual calls.
// Projection is fetched before inverse view.
// PE 0x101C2810
Mat4* RenderView::getViewProjectionMatrixPtr()
{
    if (m_dirty42C)
    {
        // Fetch projection first: both accessors lazily rebuild and clear their own dirty bits.
        Mat4* const projection  = getProjectionMatrixPtr();        // vslot +0x6C
        Mat4* const inverseView = getInverseViewMatrixPtr();       // vslot +0x68

        // sub_10077A30 with the destination in ecx - Mat4::fullMult, the full 4x4 product.
        m_matrix[3].fullMult(*inverseView, *projection);

        m_dirty42C = 0;
    }

    return &m_matrix[3];                                           // +0xFC
}

// PE 0x101C27E0. Caches the general inverse of the inverse view through PE 0x10078270.
// Dirty bit 1 is independent of the inverse-view cache's bit 0.
Mat4* RenderView::getCameraMatrixPtr()
{
    if ((m_state424 & 2u) != 0)
    {
        // Preserve the two-inversion round trip and its rounding rather than copying the
        // original view matrix. This cache owns dirty bit 1 independently of inverse view.
        fullInvert(*getInverseViewMatrixPtr(), m_matrix[2]);       // vslot +0x68, then sub_10078270

        m_state424 &= ~2u;
    }

    return &m_matrix[2];                                           // +0xBC
}


// Returns the cached linear-depth projection: z' = z - near and w = z.
// The legacy dirty-mask behavior tests bit 2 but clears bit 1, so once dirtied this
// getter recomputes on every call and also affects another cache's dirty state.

// PE 0x101C2AF0
Mat4* RenderView::getLinearProjectionMatrixPtr()
{
    rebuildProjection();

    if ((m_dirty428 & 4u) != 0)
    {
        m_matrix[5] = m_matrix[4];                         // sub_10030E80, the sixteen-float copy
        m_matrix[5].m[10] = 1.0f;                          // +0x1A4
        m_matrix[5].m[14] = -m_nearPlane;                  // +0x1B4 from +0x28

        m_dirty428 &= ~2u;                                 // bit 1, see above
    }

    return &m_matrix[5];                                   // +0x17C
}


// +0x90, PE 0x101C2B40. The inverse projection, cached in m_matrix[7]; D3DXMatrixInverse is the
// d3dx9_25 import, and the shipped call passes no determinant out.

// Rebuilds the inverse projection when dirty, retaining the copy before the inverse overwrites it.

// PE 0x101C2B40
Mat4* RenderView::getInverseProjectionMatrixPtr()
{
    rebuildProjection();

    if ((m_dirty428 & 8u) != 0)
    {
        m_matrix[7] = m_matrix[4];                         // sub_10030E80 - dead, see above
        ::D3DXMatrixInverse(reinterpret_cast<D3DXMATRIX*>(&m_matrix[7]), 0,
                            reinterpret_cast<const D3DXMATRIX*>(&m_matrix[4]));
        m_dirty428 &= ~8u;                                 // this one does clear its own bit
    }

    return &m_matrix[7];                                   // +0x1FC
}

// The shared Frustum has the same 488-byte native layout in both renderer ABI
// flavors. PE1007A460 is RendDX9's copy of Frustum::setPerspective (rendmath::
// frustumSetPerspective) and PE1007A7F0 its Frustum::setOrtho (rendmath::frustumSetOrtho).
static_assert(sizeof(Frustum) == 488, "shared native renderer Frustum");
static_assert(sizeof(Mat4) == 64, "shared native renderer matrix");


// the matrix getters,

// +0x64, one instruction: the view matrix at +0x3C.
Mat4* RenderView::getViewMatrixPtr()
{
    return &m_matrix[0];
}

// +0x6C, lazily rebuild, then hand back m_matrix[4] at +0x13C.
// PE 0x101C2A80
Mat4* RenderView::getProjectionMatrixPtr()
{
    rebuildProjection();
    return &m_matrix[4];
}

// Rebuilds projection when dirty. Far distance is measured from near, and aspect is height/width.
// Viewport-only changes do not dirty projection.
// PE 0x101C28D0
void RenderView::rebuildProjection()
{
    if ((m_dirty428 & 1u) == 0)
        return;

    if (m_projectionMode == 1)
    {
        // the ortho arm: sub_101C28D0 calls the d3dx9_25 import.
        ::D3DXMatrixOrthoLH(reinterpret_cast<D3DXMATRIX*>(&m_matrix[4]), m_orthoWidth,
                            m_orthoHeight, m_nearPlane, m_nearPlane + m_farPlane);
    }
    else
    {
        const double cotHalf = 1.0 / ::tan(static_cast<double>(m_fov) * 0.5);
        const float  m00     = static_cast<float>(cotHalf * m_aspect);
        // (near + far) / far stays on the x87 stack until the store (0x101C293A..0x101C29A3).
#if defined(__APPLE__)
        // [CHOSEN, not recovered] the extended sum and quotient.
        const float  q       = bf2x87::store(bf2x87::div(
            bf2x87::add(bf2x87::wide(m_nearPlane), bf2x87::wide(m_farPlane)), bf2x87::wide(m_farPlane)));
#else
        const float  q       = static_cast<float>(
            (static_cast<double>(m_nearPlane) + m_farPlane) / m_farPlane);
#endif

        float* m = m_matrix[4].m;
        for (int i = 0; i < 16; ++i)
            m[i] = 0.0f;
        m[0]  = m00;
        m[5]  = static_cast<float>(cotHalf);
        m[10] = q;
        m[11] = 1.0f;
        m[14] = -(q * m_nearPlane);
        m[15] = 0.0f;
    }

    m_dirty428 &= ~1u;
}

// +0x74, the
// lazy frustum. Rebuild only when the setters have marked +0x42D; the perspective arm passes
// far AS near+far (the same measured-from-near convention as the projection builder); the ortho
// arm is RendDX9's Frustum::setOrtho copy (rendmath::frustumSetOrtho).
// PE 0x101C2850
Frustum* RenderView::getFrustumPtr()
{
    Frustum* f = &m_frustum;
    if (m_dirty42D)
    {
        if (m_projectionMode == 1)
        {
            rendmath::frustumSetOrtho(*f, m_orthoWidth, m_orthoHeight, m_nearPlane,
                                      m_nearPlane + m_farPlane, m_matrix[0]);
        }
        else
            rendmath::frustumSetPerspective(*f, m_fov, m_aspect, m_nearPlane,
                                            m_nearPlane + m_farPlane, m_matrix[0]);
        m_dirty42D = 0;
    }
    return f;
}

// The seam the geometry-streaming scan uses to build its forward-expanded stack frustum through
// the same perspective builder (sub_1002DF20's sub_1007A460 call on a local).
void renderViewBuildFrustum(void* frustumStorage488, float fov, float aspect,
                            float nearZ, float farZ, const float* viewMatrix)
{
    Mat4 matrix;
    std::memcpy(matrix.m, viewMatrix, sizeof(matrix));
    rendmath::frustumSetPerspective(*static_cast<Frustum*>(frustumStorage488), fov, aspect, nearZ,
                                    farZ, matrix);                     // PE 0x1007A460
}


// The reference fov and its one-shot guard - byte_1023B400 and dword_10267D08 in the shipped image.
// File-scope, not per-view, exactly as the binary has them: every RenderView in the process shares
// one reference.
bool  RenderView::s_referenceFovValid = false;
float RenderView::s_referenceFov      = 0.0f;

// The factory RendModule registers. Same shape and same __fastcall convention as the other two -
// see ClassRegInfo in HfeAbi.h.
IBase* __fastcall createRenderView(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    RenderView* self = new RenderView();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


// Two free functions for dx9rend::Renderer, which creates and drives its own RenderView directly
// rather than through the registry.

// The concrete class remains local to this translation unit. Existing renderer callers retain
// these wrappers; new shared consumers use the evidence-backed IRenderView interface.
// [RETIRE-WHEN: RW-53]

IBase* createRenderViewDirect()
{
    // bf_malloc(1072) + Rend_RenderView_ctor, which is what operator new + the constructor is here.
    return new RenderView();
}

void renderViewSetViewMatrix(RenderView* view, const float* m16)
{
    view->setViewMatrix(m16);
}

void renderViewSetAspect(RenderView* view, float a)
{
    view->setAspect(a);
}

void renderViewSetViewport(RenderView* view, float x, float y, float w, float h)
{
    view->setViewport(x, y, w, h);
}

void renderViewSetFov(RenderView* view, float fovRadians)
{
    view->setFov(fovRadians);
}

void renderViewSetFarPlane(RenderView* view, float f)
{
    view->setFarPlane(f);
}

// PE 0x10125960: RenderView's implicit (compiler-generated) copy constructor, which the placement
// copy below instantiates; the shipped callers are DecalManager::collectDecals (0x10124586) and
// onRenderPathNotification (0x101245CE). It sets the vptr (table 0x1021C968) and copies every
// member in order, m_refCount included: +0x04..+0x38 by value, the eight matrices through Mat4's
// copy (0x10030DD0), m_frustum through Frustum's copy (0x100F58A0), then +0x424 / +0x428 and the
// four bytes +0x42C..+0x42F.
// [CHOSEN, not recovered] Constructs a member-wise RenderView copy in caller storage.
// This adapter is needed while the concrete class remains private to this translation unit.
dx9rend::IRenderView* renderViewCopyConstruct(void* storage, const dx9rend::IRenderView& source)
{
    return new (storage) RenderView(static_cast<const RenderView&>(source));
}

} // namespace Rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
