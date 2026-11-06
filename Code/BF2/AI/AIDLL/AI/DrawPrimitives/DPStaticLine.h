// A timed debug polyline in world space.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICLINE_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICLINE_H

#include <vector>

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x24 on ia-32 (DPManager::createStaticLine allocates 0x24).
class DPStaticLine : public DrawPrimitive
{
public:
    // A two-point line. The shipped constructor stores the end point twice (see the body).
    DPStaticLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end, float width);
    DPStaticLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points, float width);
    virtual ~DPStaticLine();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    // Copies lifetime, colour, points and width; `points` is replaced, not appended to.
    void getAttributes(float& lifeTime, unsigned int& colour, std::vector<Vec3>& points, float& width) const;
    // Replaces lifetime, colour, points and width; the creation time is kept.
    void setAttributes(float& lifeTime, unsigned int& colour, const std::vector<Vec3>& points, float& width);

protected:
    /* +0x14 */ std::vector<Vec3> m_points;
    /* +0x20 */ float m_width;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICLINE_H
