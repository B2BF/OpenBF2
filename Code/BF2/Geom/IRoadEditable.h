// Road interfaces shared by the renderer and editor images (IRoad 820010, IRoadEditable 820011, class
// 821011). [CHOSEN, not recovered] header organisation; the slot order is the 1.50 table's.
// Cross-image strings use RoadEditableString. Fade setters take Vec2 by value and getters return borrowed
// references. String getters return by value; the template filter owns its by-value name set.
#ifndef BF2_GEOM_IROADEDITABLE_H
#define BF2_GEOM_IROADEDITABLE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"                     // Vec2

#if defined(BF2_IMAGE_RENDDX9)
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"       // renddx9abi::HostString
#else
#include <string>
#endif

#include <set>

namespace dice {
namespace hfe {

class Frustum;

namespace geom {

class IEditorSpline;
class IPerlinNoise;
class IRoadTemplate;

// The string type every host-callable slot carries.
#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString RoadEditableString;
#else
typedef std::string RoadEditableString;
#endif


// Name set passed by value to the road-template filter.
// [CHOSEN, not recovered] Compatibility renderer builds use the VC7.1 12-byte header and 44-byte
// nodes. The caller supplies the copy; the callee destroys it. Native images use std::set.

typedef std::set<RoadEditableString> RoadTemplateNameSet;


// IRoad (IID 820010) - the three slots after the IBase quartet both road classes share.

class IRoad : public IBase
{
public:
    virtual void draw() = 0;                                                    // +0x10
    virtual void updateChunkVisibility(const Frustum* frustum) = 0;             // +0x14
    virtual bool anyChunkVisible() = 0;                                         // +0x18
};


// IRoadEditable (IID 820011) - table 0x1021D918 after IRoad, in order. Names: census §2.3 and the
// retail LevelEditor's Road.* rows where one exists; the rest [INFERRED] from the bodies.

class IRoadEditable : public IRoad
{
public:
    virtual void setTemplate(IRoadTemplate* roadTemplate) = 0;                  // +0x1C
    virtual IRoadTemplate* getTemplate() = 0;                                   // +0x20
    virtual void setSpline(IEditorSpline* spline) = 0;                          // +0x24
    virtual IEditorSpline* getSpline() = 0;                                     // +0x28
    virtual void setNoise(IPerlinNoise* noise) = 0;                             // +0x2C
    virtual IPerlinNoise* getNoise() = 0;                                       // +0x30
    virtual void setTextureScaleOffset(int layer, const Vec2& offset) = 0;      // +0x34
    virtual const Vec2& getTextureScaleOffset(int layer) = 0;                   // +0x38
    virtual void setPriority(int priority) = 0;                                 // +0x3C (ICF)
    virtual int getPriority() = 0;                                              // +0x40
    virtual void setFadeDistAtStartLeft(Vec2 distance) = 0;                     // +0x44
    virtual const Vec2& getFadeDistAtStartLeft() = 0;                           // +0x48
    virtual void setFadeDistAtStartRight(Vec2 distance) = 0;                    // +0x4C
    virtual const Vec2& getFadeDistAtStartRight() = 0;                          // +0x50
    virtual void setFadeDistAtEndLeft(Vec2 distance) = 0;                       // +0x54
    virtual const Vec2& getFadeDistAtEndLeft() = 0;                             // +0x58
    virtual void setFadeDistAtEndRight(Vec2 distance) = 0;                      // +0x5C
    virtual const Vec2& getFadeDistAtEndRight() = 0;                            // +0x60
    virtual void setName(const RoadEditableString& name) = 0;             // +0x64
    virtual RoadEditableString getName() = 0;                             // +0x68
    virtual void setGhost(bool ghost) = 0;                                      // +0x6C
    virtual bool getGhost() = 0;                                                // +0x70
    virtual RoadEditableString getCompiledMeshPath() = 0;                 // +0x74
    virtual void invalidate() = 0;                                              // +0x78
    virtual void projectToTerrain() = 0;                                        // +0x7C
    virtual bool compile(const RoadEditableString& levelPath) = 0;        // +0x80
    virtual bool compileTemplates(const RoadEditableString& path,
                                  RoadTemplateNameSet templates) = 0;           // +0x84
    virtual void buildGeometry(float width, float maxError, bool ghost) = 0;    // +0x88
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_IROADEDITABLE_H
