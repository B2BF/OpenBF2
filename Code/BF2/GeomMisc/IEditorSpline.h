// Editable spline interface (IID 80033, class 81033): the terrain-aware curve roads follow.
// [CHOSEN, not recovered] shared header usable by the renderer and editor images; the slot order is
// the 1.50 table's (the 2006 editor's table differs from +0x48 on: call slots by name). Names are
// [INFERRED] from their bodies except the two the retail Splines.con writer names (+0x20, +0x28).
#ifndef BF2_GEOMMISC_IEDITORSPLINE_H
#define BF2_GEOMMISC_IEDITORSPLINE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"                     // Vec2, Vec3, Vec4

#if defined(BF2_IMAGE_RENDDX9)
#include "../../RendDX9Abi/PluginAbi.h"       // renddx9abi::HostString
#else
#include <string>
#endif

namespace dice {
namespace hfe {
namespace geom {

// The 64-byte control-point record (GeomMisc/SplineControlPoint.h, renderer image only). Its position
// is the leading Vec3.
class SplineControlPoint;

// The string type every host-callable slot carries.
#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString EditorSplineString;
#else
typedef std::string EditorSplineString;
#endif

// Snap flags (+0x34): which positions are snapped to the terrain through the "HeightmapCluster"
// singleton - the first spline ever constructed caches it and never clears it (shipped).
enum EditorSplineSnap
{
    EditorSplineSnapAdded   = 1,              // points added by addControlPoint / pickAndAddControlPoint
    EditorSplineSnapMoved   = 2,              // points moved by the setters and pickAndMoveControlPoint
    EditorSplineSnapSamples = 4,              // every curve sample
    EditorSplineSnapEdges   = 8               // the ribbon's edges
};

// The 47 slots after the IBase quartet. Edits mark the ribbon dirty; draw rebuilds it. Screen positions
// are pixels of the renderer's render view. The "last point" setters address count - 1 unguarded.
class IEditorSpline : public IBase
{
public:
    // A new spline (through the class manager) with every point moved `offset` sideways; the side vector
    // is not renormalised on slopes, and fewer than three points read before the point block (shipped).
    virtual IEditorSpline* makeOffsetCopy(float offset) = 0;                    // +0x10
    virtual void slot_0x14() = 0;                                               // +0x14 nullsub
    virtual void draw() = 0;                                                    // +0x18
    // The ribbon's width (default 0.05, half either side) and the sampling step (default 3.0).
    virtual void setLineWidth(float width) = 0;                                 // +0x1C
    virtual float getLineWidth() = 0;                                           // +0x20
    virtual void setStepSize(float step) = 0;                                   // +0x24
    virtual float getStepSize() = 0;                                            // +0x28
    virtual void setColor(const Vec4& color) = 0;                               // +0x2C
    virtual const Vec4& getColor() = 0;                                         // +0x30
    virtual void setSnapFlags(int flags) = 0;                                   // +0x34 EditorSplineSnap bits
    virtual int getSnapFlags() = 0;                                             // +0x38
    virtual void pickAndAddControlPoint(const Vec2& screen) = 0;                // +0x3C
    virtual void addControlPoint(const Vec3& position) = 0;                     // +0x40
    virtual void clearControlPoints() = 0;                                      // +0x44
    virtual void pickAndMoveControlPoint(int index, const Vec2& screen) = 0;    // +0x48
    virtual void setControlPointPosition(int index, const Vec3& position) = 0;  // +0x4C
    // The record (no range check); its leading Vec3 is the position.
    virtual SplineControlPoint* getControlPoint(int index) = 0;                 // +0x50
    virtual int getControlPointCount() = 0;                                     // +0x54
    virtual int findNearestControlPoint(const Vec2& screen) = 0;                // +0x58
    virtual void setAllControlPointTypes(bool visible) = 0;                     // +0x5C
    virtual void setControlPointType(int index, int type) = 0;                  // +0x60
    virtual int getControlPointType(int index) = 0;                             // +0x64
    virtual void resetControlPointTypes() = 0;                                  // +0x68
    // The segment/sample iterator RoadEditable walks.
    virtual bool nextSegment() = 0;                                             // +0x6C
    virtual bool nextSample(Vec3* position, Vec3* normal) = 0;                  // +0x70
    // Trigger properties of a point (triggerable splines).
    virtual float getControlPointSpeed(int index) = 0;                          // +0x74
    virtual int getControlPointDirection(int index) = 0;                        // +0x78
    virtual EditorSplineString getControlPointSound(int index) = 0;             // +0x7C
    virtual float getControlPointDelay(int index) = 0;                          // +0x80
    virtual EditorSplineString getName() = 0;                                   // +0x84
    virtual void setControlPointSpeed(int index, float speed) = 0;              // +0x88
    virtual void setLastControlPointSpeed(float speed) = 0;                     // +0x8C
    virtual void setControlPointDelay(int index, float delay) = 0;              // +0x90
    virtual void setLastControlPointDelay(float delay) = 0;                     // +0x94
    virtual void setControlPointDirection(int index, int direction) = 0;        // +0x98
    virtual void setLastControlPointDirection(int direction) = 0;               // +0x9C
    virtual void setControlPointSound(int index, EditorSplineString sound) = 0; // +0xA0
    virtual void setLastControlPointSound(EditorSplineString sound) = 0;        // +0xA4
    virtual void setControlPointFlag(int index, bool flag) = 0;                 // +0xA8
    virtual void setLastControlPointFlag(bool flag) = 0;                        // +0xAC
    virtual void* destroy(unsigned int flags) = 0;                              // +0xB0
    virtual void setOrder(int order) = 0;                                       // +0xB4
    virtual int getOrder() = 0;                                                 // +0xB8
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_IEDITORSPLINE_H
