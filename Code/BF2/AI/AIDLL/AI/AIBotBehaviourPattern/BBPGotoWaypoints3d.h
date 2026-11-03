// Waypoint-following pattern of aircraft (3d navigation): flies the bot's flight waypoint list.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTS3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTS3D_H

#include "BBPGotoWaypoint.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBFC0 (7 words), operator new 0xC, package G-vehicles.
class BBPGotoWaypoint3d : public BBPGotoWaypoint
{
public:
    explicit BBPGotoWaypoint3d(int);   // 0x27F630
    virtual ~BBPGotoWaypoint3d();   // 0x280CF0

    virtual BotScript* createPlan(Bot*);   // 0x27F6A0, slot +0x08; from the base declaration
    virtual void init(Bot*);   // 0x27F670, slot +0x18; from the base declaration

    /* +0x08 */ int m_patternId;   // the constructor argument (operator new 0xC)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTS3D_H
