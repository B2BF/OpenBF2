// Per-bot mine, tool and movement parameters for defusing plans.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPDEFUSEMINES_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPDEFUSEMINES_H

#include "../../../../Math/Vec3.h"
#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBB80 (6 words), operator new 0x1C, package F-infantry.
class BBPDefuseMines : public BBPPattern
{
public:
    explicit BBPDefuseMines(int);   // 0x2414A0
    virtual ~BBPDefuseMines();   // 0x241A50

    virtual BotScript* createPlan(Bot*);   // 0x241C00, slot +0x08; from the base declaration

    // 0x241B80: void (stores only).
    void init(unsigned int, int, Bot*, Vec3, Vec3);

private:
    // Constructor 0x240F80/0x2414A0, destructor 0x2419C0/0x241A50/0x241AE0; per-bot arrays are
    // new [getMaxNBots] (IAISettings slot 0); init 0x241B80 writes them by Bot::getBotId.
    /* +0x08 */ int m_unknown8;              // creating pattern id
    /* +0x0C */ unsigned int* m_mine;        // -1; mine id [INFERRED] name
    /* +0x10 */ int* m_weaponIndex;          // -1; first weapon of type 1 [INFERRED] name
    /* +0x14 */ Vec3* m_minePosition;        // zeroed (Vec3 array constructor loop)
    /* +0x18 */ Vec3* m_targetPosition;      // zeroed
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPDEFUSEMINES_H
