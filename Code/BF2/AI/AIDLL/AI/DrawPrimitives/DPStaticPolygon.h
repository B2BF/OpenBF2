// A timed debug polygon, fan-triangulated at construction.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICPOLYGON_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICPOLYGON_H

#include <vector>

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x30 on ia-32 (DPManager::createStaticPolygon allocates 0x30).
class DPStaticPolygon : public DrawPrimitive
{
public:
    // Splits the outline into the fan (v0, vi, vi+1); `colours` runs parallel to `vertices` and
    // its first entry becomes the primitive colour. Needs at least one vertex and one colour.
    DPStaticPolygon(float lifeTime, const std::vector<Vec3>& vertices,
                    const std::vector<unsigned int>& colours, bool filled);
    virtual ~DPStaticPolygon();

    // Draws every fan triangle and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    void getAttributes(float& lifeTime, bool& filled) const;
    // Replaces the lifetime and the fill flag; the triangles are kept.
    void setAttributes(float& lifeTime, bool& filled);

protected:
    /* +0x14 */ std::vector<std::vector<Vec3> > m_triangles;
    /* +0x20 */ std::vector<std::vector<unsigned int> > m_colours;
    /* +0x2C */ bool m_filled;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICPOLYGON_H
