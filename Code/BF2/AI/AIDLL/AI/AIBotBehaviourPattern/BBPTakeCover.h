// Per-bot cover positions and fixed-point/object threat targets.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER_H

#include "../../../../Math/Vec3.h"
#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC7E0 (6 words), operator new 0x20, package H-deferred.
class BBPTakeCover : public BBPPattern
{
public:
    explicit BBPTakeCover(int);   // 0x29D910
    virtual ~BBPTakeCover();   // 0x29DFD0

    virtual BotScript* createPlan(Bot*);   // 0x29E330, slot +0x08; from the base declaration

    void init(Bot*, const Vec3&, const Vec3&, unsigned int);   // 0x29E120; [INFERRED] ELF void
    void init(Bot*, unsigned int, const Vec3&, unsigned int);   // 0x29E1E0; [INFERRED] ELF void

private:
    // [INFERRED] names; constructor and init stores establish the ia-32 layout.
    /* +0x08 */ int m_patternId;
    /* +0x0C */ unsigned int* m_coverObjects;
    /* +0x10 */ Vec3* m_coverPositions;
    /* +0x14 */ Vec3* m_threatPositions;
    /* +0x18 */ unsigned int* m_threatObjects;
    /* +0x1C */ bool* m_useObjects;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER_H
