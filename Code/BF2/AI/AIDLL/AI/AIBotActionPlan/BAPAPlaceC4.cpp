// C4 action: places or removes the bot's C4 (AI/AIDLL/AI/AIBotActionPlan/BAPAPlaceC4.cpp in the
// image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF
// address.
//
// static init: ELF 0x1DE280 (__tcf_0 0x1DE2E0, _GLOBAL__I 0x1DE310): the <iostream> guard object only.

#include "BAPAPlaceC4.h"

#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../../Math/Vec3.h"

#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DE050 (C1; C2 0x1DDFF0): the inline BAPStatement constructor (+0x04, +0x08 zeroed), then
// +0x14 type, +0x18 target, +0x1C place. The image leaves +0x10 (pattern id) unwritten; the tree's
// BAPStatement() stores 0 there [CHOSEN, not recovered].
BAPAPlaceC4::BAPAPlaceC4(int placementType, AIStrategicObject* target, bool place)
{
    m_placementType = placementType;
    m_target = target;
    m_place = place;
}

// ELF 0x1DE0F0 (D1; D2 0x1DE0B0, D0 0x1DE130): base destructors only.
BAPAPlaceC4::~BAPAPlaceC4()
{
}

// ELF 0x1DE180: place: Bot +0x14C placeC4(type, position of the bot's equipment, target) unless
// +0x150 isC4Placed; remove: +0x154 removeC4 when placed. Both flags set, returns false. The
// equipment information is not null-checked, as compiled.
bool BAPAPlaceC4::execute(Bot* bot, bool& ok, bool& failed)
{
    if (m_place)
    {
        if (!bot->isC4Placed())
        {
            // 0x1DE1D7
            Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
            const Vec3 position = info->getPosition();
            bot->placeC4(m_placementType, position, m_target);
        }
    }
    else if (bot->isC4Placed())
    {
        bot->removeC4();   // 0x1DE267
    }
    ok = true;
    failed = true;
    return false;
}

// ELF 0x1DE340
BAPStatementType BAPAPlaceC4::getId() const
{
    return BAPSTPlaceC4;   // 22
}

// ELF 0x1DE330
bool BAPAPlaceC4::hasDefault()
{
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
