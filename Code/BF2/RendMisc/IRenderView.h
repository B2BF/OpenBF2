// Path: [INFERRED] beside the RendMisc RenderView producer.
// 1.50 RendDX9 table1021C968: 39 entries, no virtual destructor. The interface
// namespace is corroborated by the dedicated ELF's dx9rend::IID_IRenderView
// and the older renderer's IRenderView RTTI. Method names, pointer/reference
// spelling and accessor constness remain [INFERRED]; the x86 argument/return
// carriers and contiguous order are read from the producer and client callers.
// This declaration does not publish a new factory.
#ifndef RENDMISC_IRENDERVIEW_H
#define RENDMISC_IRENDERVIEW_H

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/HfeAbi.h"
#else
#include "../Core/IBase.h"
#endif
#include "../Math/Mat4.h"

namespace dice { namespace hfe {
class Frustum;
namespace dx9rend {

class IRenderView : public IBase
{
public:
    virtual void setViewport(float x, float y, float width, float height) = 0; // +10
    virtual float* getViewport() = 0;                      // +14, four floats
    virtual void setAdjustedFov(float radians) = 0;         // +18
    virtual void setFov(float radians) = 0;                 // +1C
    virtual float getFov() = 0;                            // +20
    virtual float getReferenceFov() = 0;                   // +24
    virtual float getZoom() = 0;                           // +28
    virtual float getInverseZoom() = 0;                    // +2C
    virtual void setNearPlane(float value) = 0;            // +30
    virtual float getNearPlane() = 0;                      // +34
    virtual void setFarPlane(float value) = 0;             // +38
    virtual float getFarPlane() = 0;                       // +3C
    virtual void setAspect(float value) = 0;               // +40
    virtual float getAspect() = 0;                         // +44
    virtual void setProjectionMode(unsigned int mode) = 0; // +48, 1 = ortho
    virtual unsigned int getProjectionMode() = 0;          // +4C
    virtual void setOrthoWidth(float value) = 0;           // +50
    virtual float getOrthoWidth() = 0;                     // +54
    virtual void setOrthoHeight(float value) = 0;          // +58
    virtual float getOrthoHeight() = 0;                    // +5C
    virtual void setViewMatrix(const float* matrix16) = 0; // +60
    virtual Mat4* getViewMatrixPtr() = 0;                  // +64
    virtual Mat4* getInverseViewMatrixPtr() = 0;           // +68
    virtual Mat4* getProjectionMatrixPtr() = 0;            // +6C
    virtual Mat4* getProjectionTransposePtr() = 0;         // +70
    virtual Frustum* getFrustumPtr() = 0;                 // +74
    virtual IRenderView* clone() = 0;                      // +78
    virtual void setFlag42E(bool value) = 0;               // +7C, purpose unresolved
    virtual bool getFlag42E() = 0;                        // +80
    virtual Mat4* getViewProjectionMatrixPtr() = 0;        // +84
    virtual Mat4* getCameraMatrixPtr() = 0;               // +88
    virtual Mat4* getLinearProjectionMatrixPtr() = 0;      // +8C
    virtual Mat4* getInverseProjectionMatrixPtr() = 0;     // +90
    virtual void setFlag42F(bool value) = 0;               // +94, purpose unresolved
    virtual bool getFlag42F() = 0;                        // +98
};

// [CHOSEN, not recovered] The size of the concrete RenderView (RendMisc/RenderView.cpp), for callers
// that copy-construct one into their own pointer-aligned storage through renderViewCopyConstruct.
// Retail x86: 1072 bytes (0x430); x64 adds the wider vptr and its alignment (issue #7).
const unsigned int kRenderViewObjectBytes = sizeof(void*) == 4 ? 1072u : 1080u;

} } }
#endif
