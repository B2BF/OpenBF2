// Bridges engine projectile events to the bots. libaidll.so (ia-32) 1.5.3153; every body carries
// its ELF address. [INFERRED] Source placement (one TU in the image, static initializer 0x383440).

// static init: ELF 0x3833B0 (__tcf_0 0x383410, _GLOBAL__I 0x383440): <iostream>'s Init object only.
#include <iostream>

#include "AICollisionHandler.h"

#include <string>

#include "../../../../Core/ClassManager.h"
#include "CommonSingletonServer.h"
#include "IBotManager.h"
#include "../AIBot/BotMain.h"                    // Bot +0x94 / +0xD0 / +0xE0
#include "../../../AIInterface/BFKnowledge/IAIObject.h"   // IAIObject +0xB8
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SquadManager.h"
#include "../AIKnowledge/FireObject.h"
#include "../AIKnowledge/Information.h"
#include "InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIUtils/AIVectorMath.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"

#include <cmath>

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {


// IAICollisionHandler
// libaidll.so 0x3E5F80 (C2) / 0x3E6020 (C1).
IAICollisionHandler::IAICollisionHandler()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iAICollisionHandler)
        classManager->registerSimpleSingleton("IAICollisionHandler", this);
}

// libaidll.so 0x3E60C0 (D2) / 0x3E6150 (D1) / 0x3E61E0 (D0).
IAICollisionHandler::~IAICollisionHandler()
{
    if (CommonSingletonServer::m_iAICollisionHandler == this)
        classManager->unregisterSimpleSingleton("IAICollisionHandler");
}

// Stateless collision adapter; construction requires only the base-class initialization.
// libaidll.so 0x382200 (C2) / 0x382240 (C1).
AICollisionHandler::AICollisionHandler()
{
}

// The vptr reset and the base call, which again is what C++ emits.
// libaidll.so 0x382280 (D2) / 0x3822C0 (D1) / 0x382300 (D0).
AICollisionHandler::~AICollisionHandler()
{
}

namespace {

inline IBFTimer* bfTimer()
{
    return static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
}

inline IAIPlayerFactory* aiPlayerFactory()
{
    return static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
}

} // namespace

// handleProjectileFire - libaidll.so (ia-32) 0x382340, 689 B (objdump listing, 2026-11-02; Ghidra
// drops `this` here, so its parameters are shifted by one). `from` is the round's start point,
// `projectile` the round, `objectId` its object id, `shooter` the AI object of the shooter's root
// and `playerId` the shooter's player id; the transformation and the team are not read. A live
// bot books a fire correction for the round (BotMain::event_firing answers whether it may and the
// record's predicted target) and hands it to the engine collision handler. A thrown or launched
// explosive without guidance stamps the grenade time. Last, the firing target's information is
// resolved to its primary object and, when the bot senses it, the physical plug-in is asked its
// strength type; the answer is discarded, as compiled.
void AICollisionHandler::handleProjectileFire(const Vec3& from, world::IObject* projectile, const Mat4&,
                                              int objectId, IAIObject* shooter, int playerId, int)
{
    if (!shooter)
        return;
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);          // +0x1C
    if (!bot || bot->isDeactivated())                                               // +0xD0
        return;
    if (projectile)
    {
        bool corrected;
        const Vec3& target = static_cast<BotMain*>(bot)->event_firing(corrected, objectId, from);   // +0x2A8
        if (corrected)
            static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler)
                ->addBotProjectile(bot->getBotId(), projectile, target);           // +0xE8, +0x3C
    }

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());   // +0xD8
    IPIArmament* const armament = equipment ? equipment->getArmament() : 0;
    const int activeWeapon = armament ? armament->getActiveWeapon() : -1;          // +0x14
    if (activeWeapon == -1 || !armament)
        return;
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(activeWeapon));   // +0x1C
    if (!weapon)
        return;
    if (weapon->getExplosionRadius() > 0.0f && weapon->getWeaponGuidanceType() == 0)
        bot->setTimeOfLastGrenadeThrown(bfTimer()->getGameTime());                  // +0xF4

    unsigned int target = bot->getFiringTarget();                                   // +0x80
    Information* targetInfo = informationManager->getInformation(target);
    if (targetInfo && !targetInfo->isPrimary())
    {
        target = targetInfo->getPrimaryObject(bot->getSide());                     // +0xE0
        targetInfo = informationManager->getInformation(target);
    }
    IPIPhysical* const physical = targetInfo ? targetInfo->getPhysical() : 0;
    SensedObject* const sensed = bot->getSensedObjectFromHandle(target);            // +0x94
    if (targetInfo && physical && sensed)
        physical->getStrengthType();                                                // +0x44
}

// handleIndirectProjectileHit - libaidll.so 0x382600, 52 B: the bots near the round's flight line
// are told about it. The team, object id and last argument are not read.
// [CHOSEN, not recovered] the return value: the image returns whatever the void helper left in
// EAX; the only caller (BFCollisionHandler, bf2 0x83FDDC0) passes it on and its callers discard it.
int AICollisionHandler::handleIndirectProjectileHit(int, IAIObject* shooter, int, const Vec3& position, int)
{
    updateBotsOnProjectile(shooter, position);
    return 0;
}

// handleIndirectProjectileHit - libaidll.so 0x382640, 52 B: as above, without the object id.
// [CHOSEN, not recovered] the return value, as above (caller bf2 0x83FDC70).
int AICollisionHandler::handleIndirectProjectileHit(int, IAIObject* shooter, const Vec3& position, int)
{
    updateBotsOnProjectile(shooter, position);
    return 0;
}

// handleProjectileDamage - libaidll.so (ia-32) 0x392680, 128 B (read with rea, 2026-11-01). Only a
// shooting bot that is not deactivated reacts: it looks the victim up among its sensed objects by
// the victim's information handle for the bot's side (result discarded, as compiled). The
// strength and weapon index are not read.
// [CHOSEN, not recovered] the return value: the image sets none (EAX keeps the last call's result)
// and the only caller, BFCollisionHandler::handleProjectileDamage (bf2 0x83FE410), discards it.
int AICollisionHandler::handleProjectileDamage(IAIObject* shooter, IAIObject* victim, const int&, int)
{
    Bot* const bot = IBotManager::instance->getBot(shooter);                // +0x24
    if (bot && !bot->isDeactivated())                                       // +0xD0
        bot->getSensedObjectFromHandle(victim->getInformation(bot->getSide()));   // +0x94 / +0xB8 / +0xE0
    return 0;
}

// handleProjectileMiss - libaidll.so 0x382710, 136 B. GameLogicCollision's terrain hit through
// BFCollisionHandler (bf2 0x83FE6A0): a live shooting bot records the miss of the round
// (event_shotMissed +0x174), then every bot near the flight line is told about it.
// [CHOSEN, not recovered] the return value: EAX is the void helper's leftover; the caller discards it.
int AICollisionHandler::handleProjectileMiss(IAIObject* shooter, int playerId, int objectId, const Vec3& position)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);          // +0x1C
    if (bot && !bot->isDeactivated())                                               // +0xD0
        bot->event_shotMissed(objectId, position);                                  // +0x174
    updateBotsOnProjectile(shooter, position);
    return 0;
}

// victimUpdate - libaidll.so 0x3827A0, 966 B (objdump listing, Ghidra cross-check). Called by
// BFCollisionHandler::handleCollision (bf2 0x83FD5C0) when a round hits a player's object; it
// answers 1 when the victim is a live bot that was told about the shot.
// First the squads: when the shooter's information (as seen by the victim's side) and the
// victim's current vehicle are on different sides and the shooter does not carry C4, the
// victim's squad learns it was fired at; a human occupying the shooter's unit is booked the
// other way round. Then a live victim bot that is not hit by C4 receives the shot as incoming
// fire: a FireObject from the shooter's position to the hit point with the unit's battle
// strengths, direct, decay 1. The victim-side information is not null-tested before setTime,
// as compiled. `objectId` is not read.
int AICollisionHandler::victimUpdate(int victimPlayerId, int, const Vec3& position, IAIObject* victim,
                                     IAIObject* shooter)
{
    IAIPlayer* const victimPlayer = aiPlayerFactory()->getPlayerFromId(victimPlayerId);   // +0x20
    if (victimPlayer && shooter)
    {
        const unsigned int shooterHandle =
            shooter->forceGetInformation(victimPlayer->getSide(), true);            // +0x6C, +0xB4
        Information* const shooterInfo = informationManager->getInformation(shooterHandle);
        IAIObject* const vehicle = shooterInfo ? victimPlayer->getCurrentVehicle() : 0;   // +0x54
        if (vehicle)
        {
            Information* const vehicleInfo =
                informationManager->getInformation(vehicle->getInformation(victimPlayer->getSide()));   // +0xB8
            if (!isC4(shooter) && vehicleInfo && vehicleInfo->getSide() != shooterInfo->getSide())   // +0x44
            {
                SquadManager* const squads = SquadManager::m_instance;
                squads->shotsFired(victimPlayerId, victimPlayer->getSide(), shooterHandle);
                if (IPIUnit* const unit = shooterInfo->getUnit())
                {
                    const int occupant = unit->getOccupyingPlayerId(shooterInfo->getSide());
                    if (occupant != -1)
                    {
                        IAIPlayer* const occupantPlayer = aiPlayerFactory()->getPlayerFromId(occupant);
                        if (occupantPlayer && !occupantPlayer->getIsAIPlayer())     // +0x88
                            squads->shotsFired(occupant, shooterInfo->getSide(),
                                               victim->forceGetInformation(shooterInfo->getSide(), true));
                    }
                }
            }
        }
    }

    Bot* const bot = IBotManager::instance->getBotFromPlayerId(victimPlayerId);    // +0x1C
    if (!bot || bot->isDeactivated() || isC4(shooter))
        return 0;
    const unsigned int handle = shooter->forceGetInformation(bot->getSide(), true);
    Information* const info = informationManager->getInformation(handle);
    info->setTime(bfTimer()->getGameTime());                                        // +0x0C
    IPIUnit* const unit = info->getUnit();
    if (!unit)
        return 0;
    FireObject* const fire = new FireObject(shooter->getPosition(), position, unit->getBattleStrengths(),
                                            true, bfTimer()->getGameTime(), 1.0f);  // +0xD4
    bot->event_projectile(fire, handle);                                            // +0x16C
    return 1;
}

// shooterUpdate - libaidll.so 0x382B70, 168 B. After victimUpdate answered 1: a live shooting bot
// counts one more hit in its sensed record of the victim (+0x48) and reports the hit of the round.
// [CHOSEN, not recovered] the return value: the image sets none; the caller discards it.
int AICollisionHandler::shooterUpdate(int playerId, int objectId, world::IObject* hitObject, IAIObject* victim,
                                      const Vec3& position)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);          // +0x1C
    if (!bot || bot->isDeactivated())
        return 0;
    if (SensedObject* const sensed = bot->getSensedObjectFromHandle(victim->getInformation(bot->getSide())))
        ++sensed->m_fireRating;
    bot->event_shotHit(objectId, hitObject, position);                              // +0x170
    return 0;
}

// handleProjectileTimeout - libaidll.so 0x382C20, 223 B. A round's reset or detonation through
// BFCollisionHandler (bf2 0x83FDA30): a live owning bot drops its fire correction for the round
// (event_shotTimeOut +0x2A4), the bots near the line from the owner's current vehicle are told,
// and a sticky round of a live bot that has C4 placed removes the C4.
// [CHOSEN, not recovered] the return value: the image sets none; both callers discard it.
int AICollisionHandler::handleProjectileTimeout(int objectId, int playerId, const Vec3& position, bool sticky)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);          // +0x1C
    bool liveBot = false;
    if (bot && !bot->isDeactivated())
    {
        liveBot = true;
        static_cast<BotMain*>(bot)->event_shotTimeOut(objectId);                    // +0x2A4
    }
    IAIPlayer* const player = aiPlayerFactory()->getPlayerFromId(playerId);         // +0x20
    if (player)
        updateBotsOnProjectile(player->getCurrentVehicle(), position);              // +0x54
    if (liveBot && player && sticky && bot->isC4Placed())                           // +0x150
        bot->removeC4();                                                            // +0x154
    return 0;
}

// checkIfGrenade - libaidll.so 0x382D00, 728 B (objdump listing, Ghidra cross-check). A thrown
// explosive that lands within 10 m of the bot (and not on it) warns the bot's navigation: always
// within 5 m, otherwise only when it lies ahead of the bot (positive dot product with the
// equipment's forward axis, the third matrix row). The warning carries the weapon's firing time
// and whether it is the flashbang ("nshgr_flashbang"). The weapon name is copied, as compiled.
void AICollisionHandler::checkIfGrenade(Bot* bot, IAIObject* projectile, const Vec3& position)
{
    Information* const projectileInfo =
        informationManager->getInformation(projectile->getInformation(projectile->getSide()));   // +0x12C, +0xB8
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!projectileInfo || !equipment)
        return;
    IPIArmament* const armament = projectileInfo->getArmament();
    if (!armament || armament->getActiveWeapon() == -1)
        return;
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon || !weapon->getIsThrown() || !(weapon->getExplosionRadius() > 0.0f))
        return;

    const std::string name = weapon->getName();
    const Vec3& botPosition = equipment->getPosition();                             // +0x30
    float dx = position.x - botPosition.x;
    float dy = position.y - botPosition.y;
    float dz = position.z - botPosition.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(distance < 10.0f) || !(distance > 1e-05f))
        return;
    const float inv = 1.0f / distance;
    dx *= inv;
    dy *= inv;
    dz = inv * dz;
    bool warn = distance < 5.0f;
    if (!warn)
    {
        const Mat4& transformation = equipment->getTransformation();                // +0x3C
        warn = dx * transformation.m[8] + dy * transformation.m[9] + dz * transformation.m[10] > 0.0f;
    }
    if (warn)
    {
        const bool flashbang = name.compare("nshgr_flashbang") == 0;
        const double time = weapon->getTimeOfFiring();                              // +0x20
        bot->getBotNavigation()->setGrenadeWarning(position, time, flashbang);      // +0xFC
    }
}

// isC4 - libaidll.so 0x382FE0, 201 B: the object's own information has an active weapon of
// weapon type 4.
bool AICollisionHandler::isC4(IAIObject* object)
{
    Information* const info = informationManager->getInformation(object->getInformation(object->getSide()));
    if (!info)
        return false;
    IPIArmament* const armament = info->getArmament();
    if (!armament)
        return false;
    if (armament->getActiveWeapon() == -1)
        return false;
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    return weapon->getWeaponType() == 4;
}

// updateBotsOnProjectile - libaidll.so 0x3830B0, 763 B (objdump listing). Every active bot that is
// neither dead nor man-down is first checked for a grenade warning. A bot of another side than
// the round's object, not facing C4, whose equipment lies within 10 m of the line from that
// object to `position` (objFireLineDist), receives the round as indirect incoming fire: a
// FireObject from the object's position to `position` with the unit's battle strengths, not
// direct, decay 0.1. The bot count is re-read every pass; neither the bot, the information
// nor its unit plug-in is null-tested, as compiled.
void AICollisionHandler::updateBotsOnProjectile(IAIObject* projectile, const Vec3& position)
{
    if (!projectile)
        return;
    const Vec3& origin = projectile->getPosition();                                 // +0xD4
    const int side = projectile->getSide();                                         // +0x12C
    IBotManager* const bots = IBotManager::instance;
    for (unsigned int i = 0; i < static_cast<unsigned int>(bots->getBotCount()); ++i)   // +0x08
    {
        Bot* const bot = bots->getBot(i);                                           // +0x14
        if (bot->isDeactivated() || bot->isDead() || bot->getManDown())            // +0xD0, +0xC4, +0x1B0
            continue;
        checkIfGrenade(bot, projectile, position);
        if (bot->getPlayer()->getSide() == side)                                   // +0xBC, +0x6C
            continue;
        if (isC4(projectile))
            continue;
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
        if (!equipment)
            continue;
        if (!(objFireLineDist(equipment->getAIObject()->getPosition(), origin, position) < 10.0f))
            continue;
        Information* const info = informationManager->getInformation(
            projectile->forceGetInformation(bot->getSide(), true));                // +0xB4
        info->setTime(bfTimer()->getGameTime());
        IPIUnit* const unit = info->getUnit();
        FireObject* const fire = new FireObject(projectile->getPosition(), position, unit->getBattleStrengths(),
                                                false, bfTimer()->getGameTime(), 0.1f);
        bot->event_projectile(fire, projectile->getInformation(bot->getSide()));   // +0x16C
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
