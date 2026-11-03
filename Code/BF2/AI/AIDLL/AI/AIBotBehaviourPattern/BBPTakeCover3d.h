// Take-cover pattern of aircraft (3d navigation): flies 500 m straight ahead while pressing input 47.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER3D_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC860 (6 words), package H-deferred. BotBehaviourManager's create function
// (0x3D7F80) allocates 0x18 bytes; no body of this class touches +0x0C..+0x17. [UNRESOLVED]
class BBPTakeCover3d : public BBPPattern
{
public:
    explicit BBPTakeCover3d(int);   // 0x29FD20
    virtual ~BBPTakeCover3d();   // 0x29FDA0

    virtual BotScript* createPlan(Bot*);   // 0x29FE50, slot +0x08; from the base declaration

    void init(Bot*);   // 0x29FE20; ELF void (no return value materialized)

protected:
    /* +0x08 */ int m_patternId;   // the constructor argument
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPTAKECOVER3D_H
