// Client texture and canvas interface to RendDX9. [INFERRED] Interface name and header placement.
// Vector-taking methods require the caller and renderer to share one STL (the rebuilt client and
// its plugins do).
#ifndef GEOM_MENUREND_IMEMETEXTURE_H
#define GEOM_MENUREND_IMEMETEXTURE_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "../../Core/IBase.h"
#include <utility>
#include <vector>
namespace dice { namespace meme { struct Quad; } }
namespace dice { namespace hfe { namespace geom {
class IMemeTexture : public IBase
{
public:
    virtual void setTexture(const char* name, const char* prefix, int flags) = 0; // +10
    virtual bool createTexture(unsigned int width, unsigned int height) = 0; // +14
    virtual bool drawUV(meme::Quad*, float u0, float v0, float u1, float v1) = 0; // +18
    virtual bool drawOther(IMemeTexture*, meme::Quad*) = 0; // +1C
    virtual const char* getName() const = 0;       // +20
    virtual void setUserObject(void*) = 0;        // +24
    virtual void* getUserObject() const = 0;      // +28
    virtual float getUvWidthCopy() const = 0;     // +2C
    virtual float getUvHeightCopy() const = 0;    // +30
    virtual int getPixelWidth() const = 0;       // +34
    virtual int getPixelHeight() const = 0;      // +38
    virtual int getPageWidth() const = 0;        // +3C
    virtual int getPageHeight() const = 0;       // +40
    virtual unsigned char setByte66(unsigned char) = 0; // +44
    virtual unsigned char getByte66() const = 0;  // +48
    virtual float getUOffset() const = 0;        // +4C
    virtual float getVOffset() const = 0;        // +50
    virtual float getUvWidth() const = 0;        // +54
    virtual float getUvHeight() const = 0;       // +58
    virtual void releaseTexture() = 0;          // +5C
    virtual void unload() = 0;                  // +60
    // The shipped body at +64 is dice's own unsupported-operation diagnostic
    // (MemeTexture.cpp:291, "hmm") and returns a truthy integer.
    virtual int unsupportedOperation() = 0;     // +64 [INFERRED] name

    // Canvas points are pairs of floats. The point list crosses the boundary by value, as a
    // std::vector the client and the renderer share (issue #7); the callee owns and frees its copy.
    typedef std::pair<float, float> CanvasPoint;
    typedef std::vector<CanvasPoint> CanvasPointList;

    virtual void plotOutline(CanvasPointList points,
                             unsigned char color) = 0; // +68
    virtual void fillCanvas(unsigned char color) = 0;  // +6C
    virtual void plotAndFill(CanvasPointList points,
                             unsigned char color) = 0; // +70 [INFERRED] name
    virtual void beginShape(unsigned char clearValue) = 0; // +74
    virtual void applyCanvas(short mode) = 0;               // +78
};
} } }
#endif
#endif
