// A row-major grid of strategic statuses, and the status values used by scripts. Unknown input
// maps to Unknown; writing an out-of-range value also produces Unknown, so such values do not
// round-trip. Evidence: libaidll.so (ia-32) 0x366000..0x3663E8 (AIStrategicMap/StatusMap.cpp);
// sizeof 12, the same shape as ScalarMap. Member names [INFERRED]. [INFERRED] header placement.

#ifndef AI_AIDLL_AI_AISTRATEGIC_STATUSMAP_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGIC_STATUSMAP_H

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class StatusMap
{
public:
    // Unknown is the parser fallback for unrecognized input, including tokens never spelled "Unknown".
    enum Status
    {
        Owned   = 0,
        Hostile = 1,
        Neutral = 2,
        Unknown = 3
    };

    // Allocates width * height uninitialised statuses (checkPointer reports a failed allocation).
    StatusMap(int width, int height);
    ~StatusMap();

    // Appends the index of every Owned square, row by row.
    void getOwnedSquares(std::vector<int>& squares) const;

    /* +0x0 */ int m_width;
    /* +0x4 */ int m_height;
    /* +0x8 */ Status* m_statuses;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_STATUSMAP_H
