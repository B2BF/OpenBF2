// Waypoint-following pattern of boats: follows the bot's waypoint list, including beach landings.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSBOAT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSBOAT_H

#include "BBPGotoWaypoint.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC040 (7 words), operator new 0xC, package G-vehicles.
// [UNRESOLVED] data members: the owning package names them from the constructors.
class BBPGotoWaypointBoat : public BBPGotoWaypoint
{
public:
    explicit BBPGotoWaypointBoat(int);   // 0x280DB0
    virtual ~BBPGotoWaypointBoat();   // 0x280E30

    virtual BotScript* createPlan(Bot*);   // 0x280EE0, slot +0x08; from the base declaration
    virtual void init(Bot*);   // 0x280EB0, slot +0x18; from the base declaration

    /* +0x08 */ int m_patternId;   // the constructor argument (operator new 0xC)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSBOAT_H
