// Bridge-repair plan parameters and generation, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPBRIDGEREPAIR_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPBRIDGEREPAIR_H

#include "../../../../Math/Vec3.h"
#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BB8A0 (6 words), operator new 0x24, package F-infantry.
class BBPBridgeRepair : public BBPPattern
{
public:
    explicit BBPBridgeRepair(int);   // 0x22EC30
    virtual ~BBPBridgeRepair();   // 0x22F450

    virtual BotScript* createPlan(Bot*);   // 0x22F6E0, slot +0x08; from the base declaration

    // 0x22F5E0: void (stores only, %eax is left as the last getBotId result).
    void init(unsigned int, int, Vec3, Bot*, bool, Vec3, Vec3);

private:
    // Constructor 0x22E4D0/0x22EC30, destructor 0x22F390/0x22F450/0x22F510; per-bot arrays are
    // new [getMaxNBots] (IAISettings slot 0); init 0x22F5E0 writes them by Bot::getBotId.
    /* +0x08 */ int m_unknown8;              // pattern id carried by the generated plan
    /* +0x0C */ unsigned int* m_segment;     // -1; bridge segment Information handle [INFERRED] name
    /* +0x10 */ int* m_weaponIndex;          // -1; first weapon of type 1 [INFERRED] name
    /* +0x14 */ Vec3* m_direction;           // zeroed (Vec3 array constructor loop)
    /* +0x18 */ bool* m_nearRequest;         // false; segment within 50 m of the squad repair request
    /* +0x1C */ Vec3* m_standPosition;       // zeroed
    /* +0x20 */ Vec3* m_segmentEnd;          // zeroed
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPBRIDGEREPAIR_H
