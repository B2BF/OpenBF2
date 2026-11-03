// [TEMP] [CHOSEN, not recovered] package D's stand-ins (since ai-bot eeb9049 they forward to the
// recovered callees; call sites may call them directly and this file go) for calls whose callee is not declared in
// this tree yet. Each wrapper names the image's call (ELF address of the callee) and stops by name,
// so D's recovered bodies stay complete and read like the image; when the owner's declaration
// lands, the wrapper body becomes the real call (or the call site calls it directly).
// [RETIRE-WHEN: ai-w1-d-pending-calls] every callee below is declared and linked (coord/ai-requests.md).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBPENDINGCALLS_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBPENDINGCALLS_H

#include "../AIBotActionPlan/BAPStatement.h"
#include "../Main/AIDllFrontier.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WPCloseTo.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class BotScript;
class CommunicationAgent;
class SquadMember;
class WaypointList;
struct AIRadioMessageInfo;
enum AIRadioMessageType : int;

namespace bbpending {

// --- ai-bot -------------------------------------------------------------------------------
inline bool isAtFormationSlot(SquadMember* member)             // SquadMember::isAtFormationSlot 0x1B5540
{
    return member->isAtFormationSlot();
}


// WPCloseTo +0x10: the handle of the object to close in on (getId() 0), read inline by
// BBPGotoWaypointSoldier::createPlan 0x284EEC. WPCloseTo is not declared in this tree.
inline unsigned int closeToTarget(WaypointList* waypoints)
{
    return static_cast<WPCloseTo*>(waypoints)->getTarget();
}

// CommunicationAgent::generalConfirmReceivedMessage(AIRadioMessageType)
inline bool generalConfirmReceivedMessage(CommunicationAgent* agent, AIRadioMessageType type)
{
    return agent->generalConfirmReceivedMessage(type);
}

// CommunicationAgent::confirmReceivedMessage(const AIRadioMessageInfo&)
inline bool confirmReceivedMessage(CommunicationAgent* agent, const AIRadioMessageInfo& info)
{
    return agent->confirmReceivedMessage(info);
}

// CommunicationAgent +0x5C (m_int92, private), read inline by BBIdle::waitForPickup 0x21584E.
inline int communicationAgentInt92(CommunicationAgent* agent)
{
    return agent->getInt92();
}

} // namespace bbpending
} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBPENDINGCALLS_H
