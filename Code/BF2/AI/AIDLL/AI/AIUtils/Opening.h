// Opening visibility queries. [INFERRED] Source placement; no construction path is emitted.

#ifndef AI_AIDLL_AI_AIUTILS_OPENING_H
#define AI_AIDLL_AI_AIUTILS_OPENING_H

#include "../../../../Math/Vec3.h"

#include <cstddef>
#include <cstdint>
#include <list>

namespace dice {
namespace hfe {

class IPlanePolygon;

namespace ai {

class Opening
{
public:
    bool spot(Vec2& origin, Vec2& towards, std::list<Opening*>* path, std::list<Opening*>* openings,
              IPlanePolygon& polygon, Vec3& targetPosition);
    // Returns a newly allocated list owned by the caller; recursive intermediate lists leak as shipped.
    std::list<Vec2>* spotAll(Vec2& origin, Vec2& first, Vec2& second, std::list<Opening*>* path,
                            std::list<Opening*>* openings, IPlanePolygon& polygon, Vec3& targetPosition);
    bool isInside(Vec2& origin, Vec2& towards, Vec2& clipped);

    // [CHOSEN, not recovered] Unaccessed storage under AGENTS.md's member exception.
    // No vtable/constructor/writer exists in either ELF. Only spot 0x367AA0 / amd-64
    // 0x39EF90, spotAll 0x367EE0 / 0x39F1F0 and isInside 0x368510 / 0x39EDC0
    // access this type; their object reads start at endpoints +0x04/+0x0C and position +0x1C.
    // Their outputs and list nodes are distinct from the object prefix. Neighbouring Vec2
    // floats establish four-byte field alignment; no source-level prefix type is attested.
    alignas(float) unsigned char m_unaccessedPrefix[4];
#if INTPTR_MAX == INT64_MAX
    // [PLATFORM] [UNRESOLVED] amd-64's unaccessed prefix is sixteen bytes, not four.
    // [CHOSEN, not recovered] Extra native storage preserves its observed field offsets;
    // it does not guess a container, pointer or original member type/alignment.
    unsigned char m_nativeUnresolvedPrefix[12];
#endif
    /* +0x04 */ Vec2 m_first;  // [INFERRED] endpoint names; amd-64 +0x10
    /* +0x0C */ Vec2 m_second; // amd-64 +0x18
    // [CHOSEN, not recovered] The eight bytes between endpoints and position are also unaccessed.
    /* +0x14 */ unsigned char m_unaccessedBetween[8]; // amd-64 +0x20
    /* +0x1C */ Vec3 m_position; // [INFERRED] name; amd-64 +0x28
};

#if INTPTR_MAX == INT32_MAX
static_assert(offsetof(Opening, m_first) == 0x04, "Opening ia-32 first endpoint");
static_assert(offsetof(Opening, m_second) == 0x0C, "Opening ia-32 second endpoint");
static_assert(offsetof(Opening, m_position) == 0x1C, "Opening ia-32 position");
#elif INTPTR_MAX == INT64_MAX
static_assert(offsetof(Opening, m_first) == 0x10, "Opening native first endpoint");
static_assert(offsetof(Opening, m_second) == 0x18, "Opening native second endpoint");
static_assert(offsetof(Opening, m_position) == 0x28, "Opening native position");
#endif

} // namespace ai
} // namespace hfe
} // namespace dice

#endif
