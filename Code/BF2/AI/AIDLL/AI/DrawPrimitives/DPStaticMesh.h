// A timed debug mesh over caller-owned vertex and index arrays.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICMESH_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICMESH_H

#include "DrawPrimitive.h"
#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// sizeof == 0x2C on ia-32 (DPManager::createStaticMesh allocates 0x2C).
class DPStaticMesh : public DrawPrimitive
{
public:
    // Keeps `vertices` and `indices` by pointer (not owned) and copies `count` colours into an
    // owned array. A null `colours` gives the primitive colour 0xFFFFFFFF and random per-entry
    // colours: one high-bit base (& 0xE0E0E0E0) plus a fresh low-bit term (& 0x1F1F1F1F) each.
    DPStaticMesh(float lifeTime, unsigned int* colours, Vec3* vertices, unsigned int* indices,
                 int count, bool filled);
    // As above; the polygon pointer is accepted and ignored.
    DPStaticMesh(float lifeTime, unsigned int* colours, Polygon* polygons, Vec3* vertices,
                 unsigned int* indices, int count, bool filled);
    virtual ~DPStaticMesh();

    // Always draws and returns true.
    virtual bool draw(IDebugRenderer* renderer);

protected:
    /* +0x14 */ Vec3* m_vertices;
    /* +0x18 */ unsigned int* m_indices;
    /* +0x1C */ int m_count;
    /* +0x20 */ bool m_filled;
    /* +0x24 */ unsigned int* m_colours;
    // [INFERRED] name. Always false here; when set, the destructor also frees the two arrays.
    /* +0x28 */ bool m_ownsArrays;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPSTATICMESH_H
