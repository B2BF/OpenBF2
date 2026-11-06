// A timed debug circle in world space.
// [INFERRED] Source placement and header name (the TU is attested as DPStaticCirlce.cpp).

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICCIRCLE_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICCIRCLE_H

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x38 (DPManager::createStaticCircle allocates 0x38). The renderer receives the normal
// as drawCircle's first vector and the position as its second.
class DPStaticCircle : public DrawPrimitive
{
public:
    DPStaticCircle(float lifeTime, unsigned int colour, const Vec3& position, float radius,
                   const Vec3& normal, bool filled, int segments);
    virtual ~DPStaticCircle();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    // Copies every attribute except the creation time.
    void getAttributes(float& lifeTime, unsigned int& colour, Vec3& position, float& radius,
                       Vec3& normal, bool& filled, int& segments) const;
    // Replaces every attribute except the creation time.
    void setAttributes(float lifeTime, unsigned int colour, Vec3 position, float radius,
                       const Vec3& normal, bool filled, int segments);

protected:
    /* +0x14 */ Vec3 m_position;
    /* +0x20 */ float m_radius;
    /* +0x24 */ Vec3 m_normal;
    /* +0x30 */ bool m_filled;
    /* +0x34 */ int m_segments;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICCIRCLE_H
