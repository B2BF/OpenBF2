// Road template interface (IID 820008, class 821008): the surface a road is built and compiled with.
// [CHOSEN, not recovered] shared header usable by the renderer and editor images; the slot order is
// the 1.50 table's. Indexed slots do not check bounds: texture layers 0 and 1, lod 0.
#ifndef BF2_GEOM_IROADTEMPLATE_H
#define BF2_GEOM_IROADTEMPLATE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"                     // Vec2

#if defined(BF2_IMAGE_RENDDX9)
#include "../../RendDX9Abi/PluginAbi.h"       // renddx9abi::HostString
#else
#include <string>
#endif

namespace dice {
namespace hfe {
namespace geom {

// The string type every host-callable slot carries.
#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString RoadTemplateString;
#else
typedef std::string RoadTemplateString;
#endif

// The 25 slots after the IBase quartet. Names follow the retail LevelEditor's RoadTemplate rows where
// one exists; the rest are [INFERRED].
class IRoadTemplate : public IBase
{
public:
    virtual void setTextureFile(int index, const RoadTemplateString& file) = 0;   // +0x10
    // The template's own copy; valid while the template lives and the file is not set again.
    virtual const RoadTemplateString& getTextureFile(int index) = 0;            // +0x14
    virtual void setTextureScale(int index, const Vec2& scale) = 0;             // +0x18
    virtual const Vec2& getTextureScale(int index) = 0;                         // +0x1C
    virtual void setMirror(unsigned int index, bool mirror) = 0;                // +0x20
    virtual bool getMirror(unsigned int index) = 0;                             // +0x24
    virtual void setWidth(float width) = 0;                                     // +0x28
    virtual float getWidth() = 0;                                               // +0x2C
    virtual void setMaxTesselatedError(int lod, float error) = 0;               // +0x30
    virtual float getMaxTesselatedError(int lod) = 0;                           // +0x34
    virtual void setBlendFactor(float factor) = 0;                              // +0x38
    virtual float getBlendFactor() = 0;                                         // +0x3C
    virtual void setFadeOffDistance(const Vec2& distance) = 0;                  // +0x40
    virtual const Vec2& getFadeOffDistance() = 0;                               // +0x44
    virtual void setPriority(int priority) = 0;                                 // +0x48
    virtual int getPriority() = 0;                                              // +0x4C
    // A material id of the material manager.
    virtual void setMaterial(int material) = 0;                                 // +0x50
    virtual int getMaterial() = 0;                                              // +0x54
    virtual void setName(const RoadTemplateString& name) = 0;                   // +0x58
    virtual RoadTemplateString getName() = 0;                                   // +0x5C
    // Writes <path>\<name>_compiled.con and <path>\<name>_compiled.dat through the file manager.
    // An unopenable .con path is a null dereference (shipped).
    virtual bool compile(const RoadTemplateString& path) = 0;                   // +0x60
    virtual void setUnknown5C(int value) = 0;                                   // +0x64 [UNRESOLVED]
    virtual int getUnknown5C() = 0;                                             // +0x68 [UNRESOLVED]
    virtual void setUnknown60(float value) = 0;                                 // +0x6C [UNRESOLVED]
    virtual float getUnknown60() = 0;                                           // +0x70 [UNRESOLVED]
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_IROADTEMPLATE_H
