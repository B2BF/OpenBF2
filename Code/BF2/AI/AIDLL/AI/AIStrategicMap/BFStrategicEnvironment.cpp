// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BFStrategicEnvironment operations.

#include "BFStrategicEnvironment.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF 0x2E8D50,
// __tcf_0 0x2E8DB0, _GLOBAL__I 0x2E8DE0).
#include <iostream>

#include <string>

#include "../../../../Core/ClassManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"                             // IAIMain::m_instance->getSAI
#include "../AIBot/BotMain.h"                           // Bot::getSide / getBotId
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IAIStrategicEnvironment
IAIStrategicEnvironment::IAIStrategicEnvironment()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!AISingletonServer::m_iaiStrategicEnvironment)
        classManager->registerSimpleSingleton("IAIStrategicEnvironment", this);
}

IAIStrategicEnvironment::~IAIStrategicEnvironment()
{
    if (AISingletonServer::m_iaiStrategicEnvironment == this)
        classManager->unregisterSimpleSingleton("IAIStrategicEnvironment");
}

// Stateless adapter; construction requires only the base-class initialization.
BFStrategicEnvironment::BFStrategicEnvironment()
{
}

// The vptr reset and the base call, which again is what C++ emits.
BFStrategicEnvironment::~BFStrategicEnvironment()
{
}

// ELF 0x2E8860 / 0x2E88D0: the bot's side SAI rows (ISAI +0x30 / +0x34, by bot id).
float* BFStrategicEnvironment::getEnemyStrengths(Bot* bot) const
{
    const int id = bot->getBotId();
    return IAIMain::m_instance->getSAI(bot->getSide())->getEnemyStrengths(id);
}

float* BFStrategicEnvironment::getEnemyTypes(Bot* bot) const
{
    const int id = bot->getBotId();
    return IAIMain::m_instance->getSAI(bot->getSide())->getEnemyTypes(id);
}

// The Bot* overloads: the bot's current strategic object (ISAI::getBotCurrentPos, +0xB4) through
// the (object, side) overload of this vtable. The (object, side) overloads read the object's
// per-side float (+0xA0/+0xB0/+0xB8/+0xC0/+0xC8 + 4 side), or 0.0f (security: 1.0f) without one.
float BFStrategicEnvironment::getTemperature(Bot* bot)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    return getTemperature(IAIMain::m_instance->getSAI(side)->getBotCurrentPos(id), side);
}

float BFStrategicEnvironment::getTemperature(int object, int side)
{
    AIStrategicObject* const o = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return o ? o->m_f164[side - 1] : 0.0f;
}

float BFStrategicEnvironment::getSecurity(Bot* bot)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    return getSecurity(IAIMain::m_instance->getSAI(side)->getBotCurrentPos(id), side);
}

float BFStrategicEnvironment::getSecurity(int object, int side)
{
    AIStrategicObject* const o = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return o ? o->m_needCurveInput[side - 1] : 1.0f;
}

float BFStrategicEnvironment::getControl(Bot* bot)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    return getControl(IAIMain::m_instance->getSAI(side)->getBotCurrentPos(id), side);
}

float BFStrategicEnvironment::getControl(int object, int side)
{
    AIStrategicObject* const o = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return o ? o->m_friendlyRatio[side - 1] : 0.0f;
}

float BFStrategicEnvironment::getFriendlyBattleStr(Bot* bot)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    return getFriendlyBattleStr(IAIMain::m_instance->getSAI(side)->getBotCurrentPos(id), side);
}

float BFStrategicEnvironment::getFriendlyBattleStr(int object, int side)
{
    AIStrategicObject* const o = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return o ? o->m_friendlyBattleStrength[side - 1] : 0.0f;
}

float BFStrategicEnvironment::getHostileBattleStr(Bot* bot)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    return getHostileBattleStr(IAIMain::m_instance->getSAI(side)->getBotCurrentPos(id), side);
}

float BFStrategicEnvironment::getHostileBattleStr(int object, int side)
{
    AIStrategicObject* const o = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return o ? o->m_enemyBattleStrength[side - 1] : 0.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
