// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F6920 (__tcf_0 0x1F6980, _GLOBAL__I 0x1F69B0): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWChange.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F5A80 (C1; C2 0x1F59E0): BAPInfoWrapper(statement, patternId), +0x18 handle, +0x1C player id,
// then Information::bookVehicle(player id) on the handle's information (no null test), child +4 = this.
BAPIWChange::BAPIWChange(unsigned int target, BAPStatement* statement, int patternId, int playerId)
    : BAPInfoWrapper(statement, patternId)
{
    m_target = target;
    m_playerId = playerId;
    bapinline::info(target)->bookVehicle(playerId);
    m_statement->mpOwner = this;
}

// ELF 0x1F5FC0 (D1; D2 0x1F5B20, D0 0x1F6470). Cancels the booking; then, when the plan's bot
// (IBotManager::getBotFromPlayerId, +0x1C, no null test) now sits in the target: "requested" when the
// target's primary or one of its secondary seats has a player (getPlayerId >= 0) who sent message
// type 1 (receivedMessageExists) and the bot's primary is that primary; "seatsFree" starts as
// "the bot is not in the primary seat, the primary's unit is occupied, same primary" and stays set
// while no secondary seat is booked. The seat loop leaves as soon as requested && !seatsFree.
// Afterwards: seatsFree -> enqueueRadioMessage(11, 7, 3, 1.0f, -1), else requested -> (8, 7, 3, 1.0f, -1).
// Finally the child is deleted (vfn +8) and the base destructor runs.
BAPIWChange::~BAPIWChange()
{
    Information* target = bapinline::info(m_target);
    if (target && target->isBooked())
        target->cancelBookVehicle();

    Bot* bot = IBotManager::instance->getBotFromPlayerId(m_playerId);
    CommunicationAgent* communication = bot->getCommunicationAgent();
    Information* equipment = bapinline::info(bot->getCurrentEquipment());
    if (target && equipment && m_target == bot->getCurrentEquipment()) {
        bool requested = false;
        Information* primary = bapinline::info(target->getPrimaryObject(bot->getSide()));
        Information* equipmentPrimary = bapinline::info(equipment->getPrimaryObject(bot->getSide()));
        int playerId = primary->getPlayerId();
        if (playerId >= 0 && communication->receivedMessageExists(static_cast<AIRadioMessageType>(1), playerId)
            && equipmentPrimary == primary)
            requested = true;
        bool seatsFree = false;
        if (!equipment->isPrimary() && primary->getUnit()->isOccupied() && equipmentPrimary == primary)
            seatsFree = true;

        int iterator;
        Information* seat = bapinline::info(primary->getFirstSecondaryObject(iterator, bot->getSide()));
        while (seat) {
            playerId = seat->getPlayerId();
            if (playerId >= 0 && communication->receivedMessageExists(static_cast<AIRadioMessageType>(1), playerId)
                && equipmentPrimary == primary)
                requested = true;
            seatsFree = seatsFree && !seat->isBooked();
            if (requested && !seatsFree)
                break;
            seat = bapinline::info(primary->getSecondaryObject(iterator, bot->getSide()));
        }

        if (seatsFree)
            communication->enqueueRadioMessage(static_cast<AIRadioMessageType>(11), static_cast<AIRadioChannelType>(7),
                                               3, 1.0f, -1);
        else if (requested)
            communication->enqueueRadioMessage(static_cast<AIRadioMessageType>(8), static_cast<AIRadioChannelType>(7),
                                               3, 1.0f, -1);
    }

    if (m_statement)
        delete m_statement;
}

// ELF 0x1F69D0: "Change" in infoTypes.
int BAPIWChange::getInfoType() const
{
    return 8;
}

} // namespace ai
} // namespace hfe
} // namespace dice
