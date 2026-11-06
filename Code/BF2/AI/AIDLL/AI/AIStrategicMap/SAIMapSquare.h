// One square of the strategic grid map: its temperature tree. Evidence: libaidll.so (ia-32)
// 0x3663F0..0x366458; sizeof 40 (SAIMap allocates count * 0x28 bytes, no array cookie).

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPSQUARE_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPSQUARE_H

#include "TemperatureTree.h"

namespace dice {
namespace hfe {
namespace ai {

class SAIMapSquare
{
public:
    SAIMapSquare();

    // The image answers the square's own address; the reference spelling is [INFERRED].
    const TemperatureTree& getTemperatureTree() const;

    /* +0x00 */ TemperatureTree m_temperatureTree;   // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPSQUARE_H
