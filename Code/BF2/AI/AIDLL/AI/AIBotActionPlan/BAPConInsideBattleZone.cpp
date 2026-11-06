// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1E7360 (__tcf_0 0x1E73C0, _GLOBAL__I 0x1E73F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConInsideBattleZone.h"

#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIBot/BotMain.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E72D0 (C1; C2 0x1E72A0)
BAPConInsideBattleZone::BAPConInsideBattleZone(float distance)
{
    m_distance = distance;
}

// ELF 0x1E7410 (D0 0x1E7430): base only.
BAPConInsideBattleZone::~BAPConInsideBattleZone()
{
}

// ELF 0x1E7300
bool BAPConInsideBattleZone::evaluate(Bot* bot)
{
    return static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->isInsideCombatArea(bot->getPlayer());
}

// ELF 0x1E7350
BAPConditionType BAPConInsideBattleZone::getConditionType() const
{
    return BAPCTInsideBattleZone;
}

} // namespace ai
} // namespace hfe
} // namespace dice
