// A timed set of debug triangles given as a vertex list and per-vertex colours.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICTRIANGLE_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICTRIANGLE_H

#include <vector>

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x30 on ia-32 (DPManager::createStaticTriangle allocates 0x30).
class DPStaticTriangle : public DrawPrimitive
{
public:
    // The primitive colour is the first entry of `colours`, which must not be empty.
    DPStaticTriangle(float lifeTime, const std::vector<Vec3>& vertices,
                     const std::vector<unsigned int>& colours, bool filled);
    virtual ~DPStaticTriangle();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

    // Copies lifetime, vertices, colours and the fill flag; the vectors are replaced.
    void getAttributes(float& lifeTime, std::vector<Vec3>& vertices,
                       std::vector<unsigned int>& colours, bool& filled) const;
    // Replaces lifetime, vertices, colours and the fill flag; the primitive colour is kept.
    void setAttributes(float& lifeTime, const std::vector<Vec3>& vertices,
                       const std::vector<unsigned int>& colours, bool& filled);

protected:
    /* +0x14 */ std::vector<Vec3> m_vertices;
    /* +0x20 */ std::vector<unsigned int> m_colours;
    /* +0x2C */ bool m_filled;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICTRIANGLE_H
