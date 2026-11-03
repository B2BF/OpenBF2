// Perspective/orthographic frustum with planes, corners and lazily refreshed bounds.
// [INFERRED] header placement. Perspective FOV is in radians.
// Preserve the plane/corner order used by culling. Private field names are [CHOSEN];
// M_prevCorners is a [CHOSEN name] for the cached previous corners.

#ifndef MATH_FRUSTUM_H     // [INFERRED] guard style
#define MATH_FRUSTUM_H

#include "Plane3.h"
#include "Mat4.h"
#include "LocalBoundingBox.h"

namespace dice {
namespace hfe {

enum FrustumPlane : int {};      // [UNRESOLVED] enumerators (getCornerForPlane's parameter type)

class Frustum
{
public:
    Frustum();
    Frustum& operator=(const Frustum& other);

    void setPerspective(float fov, float aspect, float nearDist, float farDist, const Mat4& m);
    void setOrtho(float, float, float, float, const Mat4& m);
    void updateSecondarySizes();
    void recalcCorners();
    void getPerspProjection(Mat4&) const {}                                      // empty
    void getOrthoProjection(Mat4&) const {}                                      // empty
    void getViewMatrix(Mat4&) const {}                                           // empty
    void expandToEncapsulate(const Frustum& other);
    void reSetup();
    void transform(const Mat4& m);
    float getFov() { return m_fov; }
    void  setFov(float fov);
    float getNearDist() const { return m_nearDist; }
    void  setNearDist(float d);
    float getFarDist() const { return m_farDist; }
    void  setFarDist(float d);
    // Returns one of four corners for a plane; invalid input emits an error.
    const Vec3& getCornerForPlane(unsigned int corner, FrustumPlane plane) const;
    bool inside(const Vec3& p) const;
    bool inside(const Vec3& p, float radius) const;
    bool inside(const Vec3& p, float radius, bool& fullyInside) const;
    // Six outside-plane bits, in the same order as m_planes.
    unsigned int getOutcode(const Vec3& p) const;

    /* +0x000 */ Plane3 m_planes[6];
    /* +0x060 */ Vec3   m_corners[8];
    /* +0x0C0 */ Vec3   m_prevCorners[8];
    /* +0x120 */ LocalBoundingBox m_boundingBox;
    /* +0x15C */ int    m_planeOutcodes[6];
    /* +0x174 */ Mat4   m_transformation;
    /* +0x1B4 */ Vec3   m_center;
    /* +0x1C0 */ float  m_boundingRadius;
    /* +0x1C4 */ float  m_boundingRadiusSq;
    /* +0x1C8 */ bool   m_cornersDirty;
    /* +0x1CC */ float  m_fov;
    /* +0x1D0 */ float  m_aspect;
    /* +0x1D4 */ float  m_dword1D4;
    /* +0x1D8 */ float  m_dword1D8;
    /* +0x1DC */ float  m_nearDist;
    /* +0x1E0 */ float  m_farDist;
    /* +0x1E4 */ int    m_type;
};

} // namespace hfe
} // namespace dice

#endif // MATH_FRUSTUM_H
