// Terrain-aware spline, ribbon renderer and segment/sample iterator for editable roads.
// [INFERRED] header placement. Keep the 1.50 virtual order: the 2006 editor interface differs.
// Control points are 64-byte records. Edits mark the ribbon dirty for rebuilding on draw.
// The reader of the second dirty byte (+0x31) remains [UNRESOLVED].

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOMMISC_EDITORSPLINE_H
#define BF2_GEOMMISC_EDITORSPLINE_H

#include "IEditorSpline.h"               // IEditorSpline (table 0x1020CE70), shared with the editor
#include "SplineControlPoint.h"
#include "../Geom/UndergrowthSystem.h"      // Vc71Vec<T>
#include "../Math/Vec3.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"     // renddx9abi::HostString
#include "../../RendDX9Abi/Vc71String.h"

#include <stddef.h>

namespace dice {
namespace hfe {

namespace dx9rend { class Effect; class ShaderParameter; }

namespace geom {


class EditorSpline : public IEditorSpline
{
public:
    explicit EditorSpline(int order);
    ~EditorSpline();                                                            // 0x1009EEE0's body

    // IBase
    virtual void  addRef();                                                     // +0x00
    virtual int   getRef();                                                     // +0x04
    virtual int   release();                                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;                       // +0x0C

    // IEditorSpline
    virtual IEditorSpline* makeOffsetCopy(float offset);
    virtual void slot_0x14();
    virtual void draw();
    virtual void setLineWidth(float width);
    virtual float getLineWidth();
    virtual void setStepSize(float step);
    virtual float getStepSize();
    virtual void setColor(const Vec4& color);
    virtual const Vec4& getColor();
    virtual void setSnapFlags(int flags);
    virtual int getSnapFlags();
    virtual void pickAndAddControlPoint(const Vec2& screen);
    virtual void addControlPoint(const Vec3& position);
    virtual void clearControlPoints();
    virtual void pickAndMoveControlPoint(int index, const Vec2& screen);
    virtual void setControlPointPosition(int index, const Vec3& position);
    virtual SplineControlPoint* getControlPoint(int index);
    virtual int getControlPointCount();
    virtual int findNearestControlPoint(const Vec2& screen);
    virtual void setAllControlPointTypes(bool visible);
    virtual void setControlPointType(int index, int type);
    virtual int getControlPointType(int index);
    virtual void resetControlPointTypes();
    virtual bool nextSegment();
    virtual bool nextSample(Vec3* position, Vec3* normal);
    virtual float getControlPointSpeed(int index);
    virtual int getControlPointDirection(int index);
    virtual ::renddx9abi::HostString getControlPointSound(int index);
    virtual float getControlPointDelay(int index);
    virtual ::renddx9abi::HostString getName();
    virtual void setControlPointSpeed(int index, float speed);
    virtual void setLastControlPointSpeed(float speed);
    virtual void setControlPointDelay(int index, float delay);
    virtual void setLastControlPointDelay(float delay);
    virtual void setControlPointDirection(int index, int direction);
    virtual void setLastControlPointDirection(int direction);
    virtual void setControlPointSound(int index, ::renddx9abi::HostString sound);
    virtual void setLastControlPointSound(::renddx9abi::HostString sound);
    virtual void setControlPointFlag(int index, bool flag);
    virtual void setLastControlPointFlag(bool flag);
    virtual void* destroy(unsigned int flags);
    virtual void setOrder(int order);
    virtual int getOrder();

    // The TU's own member (non-virtual in the image)
    void rebuild();

    // `first ? (last - first) >> 6 : 0` - the count every body open-codes (signed `sar`).
    int pointCount() const;
    // `first + index * 64` with no range check (shipped; a negative index lands before the block).
    SplineControlPoint& pointAt(int index);

    // Data, in shipped order
    int                          m_refCount;                  // +0x04
    float                        m_stepSize;                  // +0x08
    int                          m_order;                     // +0x0C
    Vc71Vec<SplineControlPoint>  m_points;                    // +0x10
    int                          m_snapFlags;                 // +0x20
    dx9rend::Effect*             m_effect;                    // +0x24
    dx9rend::ShaderParameter*    m_paramWorldViewProjection;  // +0x28
    dx9rend::ShaderParameter*    m_paramDiffuseColor;         // +0x2C
    unsigned char                m_dirty;                     // +0x30
    unsigned char                m_rebuildBuffer;             // +0x31  [UNRESOLVED name]
    unsigned char                m_pad32[2];
    void*                        m_vertexBuffer;              // +0x34
    unsigned int                 m_vertexCount;               // +0x38  not written by the ctor
    Vec3                         m_up;                        // +0x3C
    float                        m_lineWidth;                 // +0x48
    Vec4                         m_color;                     // +0x4C
    int                          m_iteratorState;             // +0x5C
    Vec3                         m_segmentPoint[4];           // +0x60 p0, +0x6C p1, +0x78 p2, +0x84 p3
    int                          m_segment;                   // +0x90  not written by the ctor
    float                        m_t;                         // +0x94  not written by the ctor
    int                          m_samples;                   // +0x98  not written by the ctor
    Vec3                         m_lastSample;                // +0x9C
    Vec3                         m_previousSample;            // +0xA8
    int                          m_endState;                  // +0xB4
    vc71::string                 m_name;                      // +0xB8

private:
    EditorSpline(const EditorSpline&);
    EditorSpline& operator=(const EditorSpline&);
};

#if defined(_M_IX86)
static_assert(offsetof(EditorSpline, m_stepSize) == 0x08, "step at +0x08");
static_assert(offsetof(EditorSpline, m_points) == 0x10, "control points at +0x10");
static_assert(offsetof(EditorSpline, m_snapFlags) == 0x20, "snap flags at +0x20");
static_assert(offsetof(EditorSpline, m_effect) == 0x24, "effect at +0x24");
static_assert(offsetof(EditorSpline, m_dirty) == 0x30, "dirty byte at +0x30");
static_assert(offsetof(EditorSpline, m_vertexBuffer) == 0x34, "vertex buffer at +0x34");
static_assert(offsetof(EditorSpline, m_up) == 0x3C, "up at +0x3C");
static_assert(offsetof(EditorSpline, m_lineWidth) == 0x48, "width at +0x48");
static_assert(offsetof(EditorSpline, m_color) == 0x4C, "colour at +0x4C");
static_assert(offsetof(EditorSpline, m_iteratorState) == 0x5C, "iterator state at +0x5C");
static_assert(offsetof(EditorSpline, m_segmentPoint) == 0x60, "segment points at +0x60");
static_assert(offsetof(EditorSpline, m_segment) == 0x90, "segment index at +0x90");
static_assert(offsetof(EditorSpline, m_lastSample) == 0x9C, "last sample at +0x9C");
static_assert(offsetof(EditorSpline, m_previousSample) == 0xA8, "previous sample at +0xA8");
static_assert(offsetof(EditorSpline, m_endState) == 0xB4, "end state at +0xB4");
static_assert(offsetof(EditorSpline, m_name) == 0xB8, "name at +0xB8");
static_assert(sizeof(EditorSpline) == 0xD4, "the factory 0x1007CEA0 allocates 0xD4");
#endif

// RefFactory 0x1007CEA0 - published by ModuleGeomMisc::getClassList.
IBase* __fastcall createEditorSpline(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_EDITORSPLINE_H
#endif // BF2_IMAGE_RENDDX9
