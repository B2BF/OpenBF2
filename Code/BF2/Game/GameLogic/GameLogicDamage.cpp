// Damage calculation and server-side application. [INFERRED] source path.

#include "GameLogicDamage.h"

#include <cmath>
#include <cstring>
#include <map>

#include "../../Core/Ids.h"
#include "../../Core/IBase.h"
#include "../../Core/EventManager.h"                      // eventManager->postEvent(cat, id, void*, float)
#include "../../Core/StringUtil.h"
#include "../Utility/Vars.h"
#include "../IGame.h"                                     // game - exitVehicle, the two sends
#include "../IGameLogic.h"                                // gameLogic - getHitIndicationThreshold, killPlayer
#include "../Common/GameEvent.h"                          // PostRemoteEvent
#include "../GameEvents/KilledByEvent.h"
#include "../GameServer/ServerSettings.h"                 // the four friendly-fire percentages
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"
#include "../Objects/IPlayer.h"
#include "../Objects/ObjectLayoutAccess.h"                // +0x3C armor
#include "../Objects/Game/TargetObject.h"                  // designated-target player id (+0x14C)
#include "../Objects/GameLogicComp/IArmorGLComp.h"
#include "../Objects/Projectiles/GenericProjectile.h"      // typed IProjectileObject -> only implementor
#include "../Python/PythonHost.h"                         // buildArgs, pythonHost()->handleEvent
#include "../Python/PyRef.h"
#include "../../Math/Mat4.h"                              // calcLookAtMatrix
#include "../../Scene/Object/PlayerManager.h"             // playerManager
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "Player.h"                                       // dice::hfe::getPlayer; the +0x264 stamp
#include "../../AI/AIInterface/AIInstance.h"               // ai::g_aiInstance - the team-kill AI event gate
#include "../../AI/AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../AI/AIDLL/AI/Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {

namespace world {

namespace {
// the TU's one tunable (static initialiser 0x80E1790). It lives in this TU, not
// GameLogicExplosion's (whose header lists it "alongside" - the initialiser says otherwise).
int* const g_showDamageIndicatorForAll = Vars::getInt("show-damage-ndicator-for-all", 1);   // sic

// a debug byte read by calcDamage, ArmorGLComp::damage and GenericFireArm, written
// by nothing in this image. [CHOSEN name]
const bool g_damageDebugLocalPlayer = false;

IArmorGLComp* findArmor(IObject* start, IObject*& owner)
{
    for (IObject* o = start; o; o = o->getParent())
        if (IObjectComponent* c = object_detail::LayoutAccess::armorComponent(o))
        {
            owner = o;
            return static_cast<IArmorGLComp*>(c);
        }
    owner = 0;
    return 0;
}
IPlayerControlObject* findPco(IObject* start)
{
    for (IObject* o = start; o; o = o->getParent())
        if (void* p = o->queryInterface(IID_IPlayerControlObject))
            return static_cast<IPlayerControlObject*>(p);
    return 0;
}

// Typed laser-assist access without a partial projectile-interface vtable.
// Reads the designated target and its replicated designating-player ID.
IObject* projectileObjectGetDesignatedTarget(void* projectileObject)
{
    IProjectileObject* const projectile = static_cast<IProjectileObject*>(projectileObject);
    return static_cast<GenericProjectile*>(projectile)->getTargetObject();
}

int designatedTargetGetPlayerId(IObject* target)
{
    return static_cast<TargetObject*>(target)->getTargetPlayerId();
}
} // namespace

void fillKilledByData(KilledByData& data, IPlayer* killer, IPlayer* victim, IObject* weapon,
                      IObject* vehicle, IArmorGLComp* armor, int material, bool vehicleKill)
{
    data.victimPlayerId = static_cast<unsigned char>(victim->getId());            // +0x54
    data.field04 = static_cast<unsigned short>(material);
    if (killer)
    {
        data.killerPlayerId = static_cast<unsigned char>(killer->getId());
        data.isKillerPlayerAlive = true;
    }
    else
        data.isKillerPlayerAlive = false;
    if (!vehicleKill && weapon)
        data.someObjTemplateId = weapon->getTemplate()->getId();                  // +0x0C
    if (vehicle)
    {
        IPlayerControlObject* killerPco = 0;
        if (killer)
            killerPco = findPco(killer->getVehicle());                            // +0x48
        if (killerPco && killerPco->isChildSoldierActive())                       // +0x140
            data.vehicleTemplateId = killer->getDefaultVehicle()->getTemplate()->getId();   // +0x08
        else
            data.vehicleTemplateId = getRootParent(vehicle)->getTemplate()->getId();
    }
    if (armor)
    {
        data.lastHitBoneIndex = static_cast<unsigned char>(armor->getLastHitBoneIndex());   // +0xB8
        data.lastHitDirection = armor->getLastHitDirection();                     // +0xC0
    }
}

float GameLogicDamage::calcDamage(IObject* object, float damage, int team, bool splash,
                                  bool noFriendlyFireMod)
{
    if (g_damageDebugLocalPlayer)
    {
        const std::list<IPlayer*>& players = playerManager->getPlayers();        // +0x3C
        for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        {
            IPlayer* p = getPlayer(*it);
            if (p->getDefaultVehicle() == object                                   // +0xA4
                || (*it)->getVehicle() == object                                   // +0x48
                || getRootParent((*it)->getVehicle()) == object)
            {
                if (*it == playerManager->getLocalHumanPlayer())                  // +0x30
                    return 0.0f;
            }
        }
    }

    IObject* root = getRootParent(object);
    IPlayerControlObject* pco = findPco(object);
    float mod = 1.0f;
    if (pco && (!pco->getVehicleMod(static_cast<VehicleMod>(4))                  // +0xE4
                || !pco->getVehicleMod(static_cast<VehicleMod>(5))))
    {
        if (pco->getVehicleMod(static_cast<VehicleMod>(4)))
            mod = 0.9f;
        else
            mod = pco->getVehicleMod(static_cast<VehicleMod>(5)) ? 1.1f : 1.0f;
    }

    bool soldierLike = false;
    const IObjectTemplate* rootTemplate = root->getTemplate();
    if (rootTemplate
        && (rootTemplate->getClassID() == CID_SoldierTemplate
            || root->getTemplate()->getClassID() == CID_LadderContainerTemplate))
        soldierLike = true;

    if (team <= 0 || noFriendlyFireMod || !pco)
        return damage * mod;

    const bool sameTeam = pco->getTeam() == team;                                // +0x98
    int percent;
    if (splash)
    {
        if (soldierLike && sameTeam)
            percent = serverSettings->m_soldierSplashFriendlyFire;                // +0x88
        else if (!soldierLike && sameTeam)
            percent = serverSettings->m_vehicleSplashFriendlyFire;                // +0x8C
        else
            return damage * mod;
    }
    else
    {
        if (soldierLike && sameTeam)
            percent = serverSettings->m_soldierFriendlyFire;                      // +0x80
        else if (!soldierLike && sameTeam)
            percent = serverSettings->m_vehicleFriendlyFire;                      // +0x84
        else
            return damage * mod;
    }
    damage = static_cast<float>(percent) / 100.0f * damage;
    return damage * mod;
}

// serverGiveDamage - 0x80DFFF0, 5,396 B. See the header block for the shape; the comments below
// mark the places where the raw overrode the decompile.
void GameLogicDamage::serverGiveDamage(IObject* object, float damage, IPlayer* attacker,
                                       std::list<WeakPtr<IPlayer> >* assistList, IObject* sourceObject,
                                       int team, IObject* weapon, IObject* vehicle, Vec3 position,
                                       Vec3 direction, int material, bool lastDamageType, bool /*unused*/,
                                       bool canTK, bool vehicleKill)
{
    IObject* owner = 0;
    IArmorGLComp* armor = findArmor(object, owner);
    if (!owner || !armor || armor->isInvunerable() || owner->isObjectDestroyed())   // +0x108, slot 15
        return;

    // the team filter and the last-hit record
    IPlayer* credited = attacker;
    bool stampLastHit = true;
    if (team > 0)
    {
        if (!attacker)
            stampLastHit = false;
        else if (attacker->getTeam() != team)                                     // +0xE8
        {
            attacker = 0;
            credited = 0;
        }
    }
    if (stampLastHit && credited)
    {
        armor->setLastHitPlayer(credited);                                        // +0xCC
        armor->setLastHitWeapon(weapon);                                          // +0xDC
        armor->setLastHitVehicle(vehicle);                                        // +0xE4
        armor->setLastHitCanTK(canTK);                                            // +0xD4
        armor->setLastDamageType(lastDamageType ? 1 : 0);                         // +0xEC
    }

    IObject* root = getRootParent(owner);
    const Vec3& rootPos = root->getAbsolutePosition();                            // +0x6C
    Vec3 hitPlace;
    hitPlace.x = position.x - rootPos.x;
    hitPlace.y = position.y - rootPos.y;
    hitPlace.z = position.z - rootPos.z;
    armor->setLastHitPlace(hitPlace);                                             // +0xC4
    armor->setLastHitDirection(direction, false);                                 // +0xBC
    if (!const_cast<IObjectTemplate*>(owner->getTemplate())->queryImplementation(CID_SimpleObjectTemplate))
        return;

    // the victims
    bool teamKill = false;
    IPlayer* seatPlayer = 0;
    bool fullDestroy = false;
    std::list<IPlayer*> victims;
    std::map<IPlayer*, int> assists;
    bool dies = false;
    bool wreckDies = false;
    const bool wasDamaged = armor->getStatus() != 0;                              // +0x5C
    bool soldier = owner->getTemplate()->getClassID() == CID_SoldierTemplate;

    IPlayerControlObject* pco = findPco(owner);
    if (pco)
    {
        std::map<int, IPlayerControlObject*>* pcos = pco->getPcos();              // +0x60
        IPlayer* occupant = pco->getOccupingPlayer();                             // +0x10
        if (occupant)
        {
            if (*g_showDamageIndicatorForAll || soldier)
            {
                const float norm = damage / armor->getMaxHitPoints();             // +0x84
                armor->accumulateDamageNorm(norm);                                // +0x110
                if (!attacker || attacker == occupant || attacker->getTeam() != occupant->getTeam() || canTK)
                {
                    const float accumulated = armor->getAccumulatedDamageNorm();  // +0x114
                    if (accumulated > gameLogic->getHitIndicationThreshold())     // +0x284
                    {
                        armor->resetAccumulatedDamageNorm();                      // +0x118
                        // [PLATFORM] ELF +0x1E0 / PE +0x1DC. The ignored float return is
                        // intentional: both retail callers discard it with `fstp st`.
                        occupant->updateHitFromDirection(hitPlace);
                        if (occupant->getId() == 255)
                            eventManager->postEvent(ECHud, 52, 0, 0.0f);          // +0x14
                    }
                }
                else
                    armor->resetAccumulatedDamageNorm();
                if (soldier)
                {
                    fullDestroy = occupant->getDefaultVehicle() != occupant->getVehicle();   // +0xA4, +0x48
                    soldier = !fullDestroy;
                }
            }
        }

        if (!soldier)
        {
            const float hitPoints = armor->getHitPoints();                        // +0x3C
            if (hitPoints - damage > 0.001f)
                fullDestroy = false;
            else
                dies = true;
        }
        else if (armor->getStatus() != 1)
        {
            if (armor->getHitPoints() - damage <= 0.0f)
                dies = true;
        }
        if (soldier)
        {
            const float after = armor->getHitPoints() - damage;
            if (-armor->getMaxWreckHitPoints() >= after)                          // +0x88
                wreckDies = true;
        }

        for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
        {
            seatPlayer = getPlayer(it->second->getOccupingPlayer());              // +0x10
            if (!seatPlayer)
                continue;

            if (!dies)
            {
                if (wreckDies)
                    victims.push_back(seatPlayer);
            }
            else if (seatPlayer->getIsAlive() != true)                            // +0x6C
                game->exitVehicle(seatPlayer, true, true);                        // +0xA8
            else if (!attacker)
            {
                seatPlayer->getIsManDown();                                       // +0x100 - result unused
                victims.push_back(seatPlayer);
            }
            else if (seatPlayer->getIsManDown())
                victims.push_back(seatPlayer);
            else
            {
                if (attacker->getTeam() == seatPlayer->getTeam())
                {
                    teamKill = true;
                    if (canTK && attacker != seatPlayer)
                    {
                        // raw 0x80E11EB: `mov [ecx+264h], eax` - Player +0x264 <- killerId + 256
                        static_cast<Player*>(seatPlayer)->m_field264 = attacker->getId() + 256;
                        if (ai::g_aiInstance)
                        {
                            int ids[3] = { attacker->getId(), seatPlayer->getId(), 0 };
                            eventManager->postEvent(ECAI, 11, ids, 0.0f);         // raw 0x80E1255
                        }
                    }
                }
                else
                {
                    if (assistList)
                    {
                        for (std::list<WeakPtr<IPlayer> >::iterator a = assistList->begin();
                             a != assistList->end(); ++a)
                        {
                            if (IPlayer* p = a->get())
                            {
                                const int value = a == assistList->begin() ? 3 : 2;
                                assists.insert(std::make_pair(p, value));
                            }
                        }
                    }
                    if (sourceObject)
                    {
                        if (void* projectile = sourceObject->queryInterface(IID_IProjectileObject))
                        {
                            if (IObject* target = projectileObjectGetDesignatedTarget(projectile))   // +0xC8
                            {
                                const Vec3& a = owner->getAbsolutePosition();
                                const Vec3& b = target->getAbsolutePosition();
                                const float dy = a.y - b.y, dx = a.x - b.x, dz = a.z - b.z;
                                if (sqrtf(dy * dy + dx * dx + dz * dz) < 10.0f)
                                {
                                    IPlayer* designator =
                                        playerManager->getPlayerFromId(designatedTargetGetPlayerId(target));   // +0x20
                                    if (designator && designator != attacker)
                                        assists.insert(std::make_pair(designator, 1));
                                }
                            }
                        }
                    }
                }

                // Selects at most one assist; each ledger entry replaces the candidate, even when skipped later.
                std::list<DamageFromPlayer>* ledger = armor->getDamageFromPlayers();   // +0xF4
                std::map<int, float> byId;
                float best = 0.0f;
                IPlayer* candidate = 0;
                for (std::list<DamageFromPlayer>::iterator r = ledger->begin(); r != ledger->end(); ++r)
                {
                    candidate = r->player.get();
                    if (!candidate || candidate == attacker)
                        continue;
                    if (candidate->getTeam() != seatPlayer->getTeam())
                    {
                        std::map<int, float>::iterator f = byId.find(candidate->getId());
                        if (f == byId.end())
                        {
                            byId.insert(std::make_pair(candidate->getId(), r->amount));
                            if (r->amount > best)
                                best = r->amount;
                        }
                        else
                        {
                            f->second = r->amount + f->second;
                            if (candidate->getId() == f->first)
                                best = f->second;
                            else if (f->second > best)
                            {
                                best = f->second;
                                candidate = playerManager->getPlayerFromId(f->first);
                            }
                        }
                    }
                }
                armor->getTeamDamagePlayers()->clear();                           // +0xFC
                ledger->clear();
                if (candidate)
                {
                    const float norm = best / armor->getMaxHitPoints();
                    if (norm >= 0.5f)
                        assists.insert(std::make_pair(candidate, 2));
                }
                victims.push_back(seatPlayer);
            }

            // The vestigial client block - see the header. Virtual calls kept, results dropped.
            {
                Mat4 lookAt = Mat4::getIdentity();
                owner->getAbsoluteTransformation();                               // +0x84
                owner->getAbsoluteTransformation();
                owner->getAbsoluteTransformation();
                Vec3 up = { 0.0f, 1.0f, 0.0f };
                calcLookAtMatrix(lookAt, owner->getAbsolutePosition(), position, up);
                playerManager->getLocalHumanPlayer();                             // +0x30
                if (attacker && seatPlayer && attacker != seatPlayer)
                {
                    seatPlayer->getTeam();
                    attacker->getTeam();
                }
            }
        }
    }

    // the AI collision handler
    ai::IBFCollisionHandler* collisionHandler;
    if (attacker && weapon &&
        (collisionHandler = static_cast<ai::IBFCollisionHandler*>(
            ai::CommonSingletonServer::m_iBFCollisionHandler)) && seatPlayer)
    {
        Vec3 pos = position;
        const unsigned int weaponId = weapon->getObjectId();                       // +0x10
        // ..539 / bf2_w32ded.exe 0x58763F..684: the registered IBFCollisionHandler, the same
        // variable (w32ded 0x8B7030, BF2.exe 0xA38688) every other engine report reads. Vec3 is
        // BY value (mangling and PE's twelve stack bytes); GCC passes its copy
        // indirectly. The typed call preserves the native compiler's ABI.
        collisionHandler->handleProjectileDamage(owner, damage, seatPlayer->getId(), weaponId, pos);
    }

    // a vehicle that just died: Python event 24
    if (dies && !wasDamaged)
    {
        if (owner->queryInterface(IID_IPlayerControlObject))
        {
            python::PyRef args = python::buildArgs(owner, attacker);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(24), args.get());
        }
    }

    // the kills
    for (std::list<IPlayer*>::iterator v = victims.begin(); v != victims.end(); ++v)
    {
        IPlayer* victim = *v;
        if (!victim->getIsManDown())                                              // +0x100
        {
            if (teamKill && !canTK)
                continue;
            if (attacker && attacker->getScoreData())                             // +0x174 - twice, unused
                attacker->getScoreData();
            IObject* killerWeapon = (vehicle && vehicleKill) ? 0 : weapon;
            {
                python::PyRef args = python::buildArgs(victim, attacker, killerWeapon, assists, vehicle);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(1), args.get());
            }
            KilledByData data;
            std::memset(&data, 0, sizeof(data));
            fillKilledByData(data, attacker, victim, weapon, vehicle, armor, material, vehicleKill);
            KilledByEvent* event = new KilledByEvent(data);
            game->sendGameEventToAllConnected(event);                             // +0x94
            eventManager->postEvent(ECGameLogic, 3, &data, 0.0f);                 // raw 0x80E093A..
            for (std::map<IPlayer*, int>::iterator a = assists.begin(); a != assists.end(); ++a)
            {
                IPlayer* player = a->first;
                int score = a->second;
                PostRemoteEvent* remote = new PostRemoteEvent(static_cast<EventCategory>(0), 9,
                                                              player->getId(), &score, 4, 0.0f);
                game->sendGameEventIfConnected(player->getId(), remote);          // +0x98
            }
        }
        if (!soldier || wreckDies)
            gameLogic->killPlayer(victim, true, attacker == 0);                   // +0x6C
    }

    // the armor takes it
    IPlayer* perpetrator = 0;
    if (attacker)
    {
        IObject* dummy = 0;
        IArmorGLComp* attackerArmor = findArmor(attacker->getVehicle(), dummy);   // +0x48
        perpetrator = attackerArmor == armor ? 0 : attacker;
    }
    armor->damage(damage, perpetrator);                                           // +0x40
    if (fullDestroy)
        armor->fullDestroy();                                                     // +0x100
}

} // namespace world
} // namespace hfe
} // namespace dice
