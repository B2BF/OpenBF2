// Spline control point and its shared sphere visualization. [INFERRED] header placement.
// The 64-byte record includes position, picking radius and [INFERRED] trigger properties.
// The +0x3C byte is [UNRESOLVED] and neither constructor initializes it.
// Every construction/copy references shared mesh/effect resources; destruction releases them.
// Only the mesh pointer is nulled on final release; the next construction replaces the others.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOMMISC_SPLINECONTROLPOINT_H
#define BF2_GEOMMISC_SPLINECONTROLPOINT_H

#include "../Math/Vec3.h"
#include <string>

#include <stddef.h>

namespace dice {
namespace hfe {
namespace geom {

class SplineControlPoint
{
public:
    explicit SplineControlPoint(const Vec3& position);
    SplineControlPoint(const SplineControlPoint& other);
    ~SplineControlPoint();

    void draw();

    Vec3          m_position;      // +0x00
    float         m_radius;        // +0x0C
    int           m_type;          // +0x10
    float         m_speed;         // +0x14  [INFERRED name]
    int           m_direction;     // +0x18  [INFERRED name]
    std::string  m_sound;         // +0x1C  [INFERRED name]
    float         m_delay;         // +0x38  [INFERRED name]
    unsigned char m_flag3C;        // +0x3C  [UNRESOLVED] - never written by a constructor
    unsigned char m_pad3D[3];

private:
    // Not used on any recovered path (the vector only copy-constructs and destroys its elements).
    SplineControlPoint& operator=(const SplineControlPoint&);
};

#if defined(_M_IX86)
static_assert(offsetof(SplineControlPoint, m_radius) == 0x0C, "radius at +0x0C");
static_assert(offsetof(SplineControlPoint, m_type) == 0x10, "type at +0x10");
static_assert(offsetof(SplineControlPoint, m_speed) == 0x14, "+0x14 float");
static_assert(offsetof(SplineControlPoint, m_direction) == 0x18, "+0x18 int");
static_assert(offsetof(SplineControlPoint, m_sound) == 0x1C, "+0x1C string");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_SPLINECONTROLPOINT_H
#endif // BF2_IMAGE_RENDDX9
