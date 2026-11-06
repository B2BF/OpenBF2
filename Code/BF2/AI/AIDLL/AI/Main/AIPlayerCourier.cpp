
// static init: ELF 0x3B3090 (__tcf_0 0x3B30F0, _GLOBAL__I 0x3B3120): <iostream>'s Init object only.
#include <iostream>

#include "AIPlayerCourier.h"

#include <string>

#include "../../../../Core/ClassManager.h"
#include "CommonSingletonServer.h"
#include "IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IAIPlayerCourier
IAIPlayerCourier::IAIPlayerCourier()
{
    if (!classManager->getSimpleSingleton("IAIPlayerCourier"))
        classManager->registerSimpleSingleton("IAIPlayerCourier", this);
}

IAIPlayerCourier::~IAIPlayerCourier()
{
    if (classManager->getSimpleSingleton("IAIPlayerCourier") == this)
        classManager->unregisterSimpleSingleton("IAIPlayerCourier");
}

// Stateless player courier; construction requires only the base-class initialization.
AIPlayerCourier::AIPlayerCourier()
{
}

// The vptr reset and the base call, which again is what C++ emits.
AIPlayerCourier::~AIPlayerCourier()
{
}

// ELF 0x3B2E90: the bot's getChangeTarget handle (Bot +0x1BC) resolved through the information pool; the
// information's AI object answers its engine object (IObjectComponent +0x20 getObject). Null for
// a non-bot or an unknown handle.
world::IObject* AIPlayerCourier::getSelectedVehicle(int playerId) const
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    Information* const information = informationManager->getInformation(bot->getChangeTarget());
    if (!information)
        return 0;
    return information->getAIObject()->getObject();
}

// ELF 0x3B2F20: the bot's getBridgeRepairTarget handle (Bot +0x1D4) resolved through the information pool; the
// information's AI object answers its engine object (IObjectComponent +0x20 getObject). Null for
// a non-bot or an unknown handle.
world::IObject* AIPlayerCourier::getRepairTarget(int playerId) const
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    Information* const information = informationManager->getInformation(bot->getBridgeRepairTarget());
    if (!information)
        return 0;
    return information->getAIObject()->getObject();
}

// ELF 0x3B2FB0: the bot's getReviveTarget handle (Bot +0x1DC) resolved through the information pool; the
// information's AI object answers its engine object (IObjectComponent +0x20 getObject). Null for
// a non-bot or an unknown handle.
world::IObject* AIPlayerCourier::getReviveTarget(int playerId) const
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    Information* const information = informationManager->getInformation(bot->getReviveTarget());
    if (!information)
        return 0;
    return information->getAIObject()->getObject();
}

// ELF 0x3B3040: the bot's mine target (BotMain +0x1FC, past the Bot interface), or -1.
unsigned int AIPlayerCourier::getMineDefuseTarget(int playerId) const
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return static_cast<unsigned int>(-1);
    return static_cast<BotMain*>(bot)->getMineTarget();
}

} // namespace ai
} // namespace hfe
} // namespace dice
