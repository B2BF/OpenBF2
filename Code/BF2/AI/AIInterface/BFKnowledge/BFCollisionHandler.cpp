// [INFERRED] Source placement.

#include "BFCollisionHandler.h"

#include "../../../Game/Objects/IObject.h"       // getAbsolutePosition, the +0x6C / +0x68 slot

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"   // IAISettings::getMaxNBots - slot +0
#include "../../AIDLL/AI/Main/AICollisionHandler.h"     // IAICollisionHandler +0x14 - handleProjectileDamage
#include "IAIObject.h"
#include "IAIObjectPlugIn.h"                            // IAIObjectArmament / IAIObjectPhysical
#include "WeaponTemplateManager.h"
#include "WeaponTemplate.h"
#include "../../../Game/Objects/IObjectTemplate.h"
#include "../../../Game/Objects/IPlayer.h"
#include "../../../Game/Objects/Base/SimpleObjectTemplate.h"   // m_aiTemplate (+0x128), friended
#include "../../../Game/Objects/ObjectLayoutAccess.h"          // IObject +0x34, the AI object
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Core/Ids.h"                                 // CID_SimpleObjectTemplate
#include "../../../Game/Objects/IObjectTemplateComponent.h"    // getClassID +0x10
#include "../../../Game/Objects/Projectiles/GenericProjectile.h"   // IProjectileObject's implementor
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../AIDLL/AI/Main/AIMineManager.h"                 // minePlaced +0x18 / mineRemoved +0x14
#include <string>

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IBFCollisionHandler::IBFCollisionHandler()
{
    if (!CommonSingletonServer::m_iBFCollisionHandler)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IBFCollisionHandler"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IBFCollisionHandler::~IBFCollisionHandler()
{
    if (CommonSingletonServer::m_iBFCollisionHandler == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IBFCollisionHandler"));
        BFSingletonServer::updateStaticMethods();
    }
}

// Initializes and registers the collision adapter used by the AI.
BFCollisionHandler::BFCollisionHandler()
{
    // A diagnostic, not a guard - see the block above.
    IAISettings* const settings =
        checkPointer(static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings));

    // the bot count sizes the whole class. See the block above.
    m_count = settings->getMaxNBots();

    if (m_count == -1)
    {
        m_projectiles = 0;
    }
    else
    {
        m_projectiles = checkPointer(new BotProjectile*[m_count]);

        for (int i = 0; i < m_count; ++i)
            m_projectiles[i] = checkPointer(new BotProjectile());
    }
}

// +0, +56 and +60 are deliberately untouched - see the note in BFCollisionHandler.h. Retail
// clears +4..+55 as thirteen dword stores; the two flags, their padding and the four Vec3s.
BFCollisionHandler::BotProjectile::BotProjectile()
{
    m_active = false;
    m_passed = false;
    m_prePoint.x = m_prePoint.y = m_prePoint.z = 0.0f;
    m_postPoint.x = m_postPoint.y = m_postPoint.z = 0.0f;
    m_startPoint.x = m_startPoint.y = m_startPoint.z = 0.0f;
    m_targetPoint.x = m_targetPoint.y = m_targetPoint.z = 0.0f;
}

// [INFERRED] local helper name. The pass test compares distXZSqr's unrounded x87 result with the
// stored float: neither image stores it first. Under SSE2 a C++ call would round it to float on
// return, so the distXZSqr arithmetic is repeated here in double - equal to the PE's 53-bit x87.
// [PLATFORM] the ELF keeps a 64-bit mantissa. Strictly short of the target; equality and NaN
// count as passed.
static bool botProjectileShortOfTarget(const Vec3& start, const Vec3& position, float passDistanceSq)
{
    const double dx = static_cast<double>(start.x) - position.x;
    const double dz = static_cast<double>(start.z) - position.z;
    return dx * dx + dz * dz < passDistanceSq;
}

// bf2 0x83FD2F0: every record is deleted, then the array - with scalar delete although the
// constructor allocated it with new[] (a shipped defect, kept: the element type is a plain
// pointer, so no array cookie is involved). The count is read live; with no bots (-1) the loop
// does not run and the null array is deleted.
BFCollisionHandler::~BFCollisionHandler()
{
    for (int i = 0; i < m_count; ++i)
        delete m_projectiles[i];
    delete m_projectiles;
}

// bf2 0x83FD3F0: every bot's record is deleted and replaced by a fresh one (checkPointer on the
// allocation). [CHOSEN, not recovered] The ELF's inline construction clears only the +4 flag byte
// and the four Vec3s; the shared constructor also clears the +5 flag, which no path reads before
// botProjectilePassedPoint sets it.
void BFCollisionHandler::removeAllProjectiles()
{
    for (int i = 0; i < m_count; ++i)
    {
        delete m_projectiles[i];
        m_projectiles[i] = checkPointer(new BotProjectile());
    }
}

namespace {

// [INFERRED] local helper: handleCollision and handleProjectileMiss end in the same inlined block.
// A round that shows a mine icon is reported to the mine manager: radius-trigger type 2 as type 1
// with radius 9, otherwise, when the template's detonation component is the default one, as type 0
// with radius 7.5. The template's component query is not null-tested, as compiled.
void reportPlacedMine(world::IObject* object, unsigned int objectId)
{
    world::IProjectileDetonationComp* detonation = static_cast<world::IProjectileDetonationComp*>(
        object->queryComponent(world::IID_IProjectileDetonationComp,
                               world::IID_IProjectileDetonationComp));                    // +0x5C
    if (!detonation || !detonation->getShowMineIndicationIcon())                           // +0x74
        return;
    detonation = static_cast<world::IProjectileDetonationComp*>(
        object->queryComponent(world::IID_IProjectileDetonationComp,
                               world::IID_IProjectileDetonationComp));                    // asked again
    IAIMineManager* const mines = static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager);
    if (detonation->getRadiusTriggerType() == 2)                                           // +0x78
    {
        mines->minePlaced(static_cast<IAIMineManager::MineType>(1), objectId,
                          object->getAbsoluteTransformation(), 9.0f);                      // +0x18
        return;
    }
    // [CHOSEN, not recovered] const_cast: the template's queryComponent (+0x5C) is not const.
    world::IObjectTemplate* const objectTemplate =
        const_cast<world::IObjectTemplate*>(object->getTemplate());                       // +0x14
    if (objectTemplate->queryComponent(world::IID_IProjectileDetonationCompTemplate)->getClassID()
        != world::CID_DefaultDetonationCompTemplate)
        return;
    mines->minePlaced(static_cast<IAIMineManager::MineType>(0), objectId,
                      object->getAbsoluteTransformation(), 7.5f);
}

// GenericProjectile is IProjectileObject's only implementor (see world::projectileOwner in
// GenericProjectile.cpp); the typed downcast is the retail subtable dispatch minus the thunk.
inline world::GenericProjectile* asGenericProjectile(world::IProjectileObject* projectile)
{
    return static_cast<world::GenericProjectile*>(projectile);
}

inline IAIObject* aiObjectOf(world::IObject* object)
{
    return static_cast<IAIObject*>(world::object_detail::LayoutAccess::aiObject(object));   // +0x34
}

} // namespace

// handleCollision - bf2 0x83FD5C0, 1118 B (objdump, 2026-11-01). GameLogicCollision's object hit:
// `projectile` hit `hitObject` at `at`. A round fired from a vehicle with an AI object tells the AI
// collision handler about the victim (victimUpdate +0x1C, the victim's player id from the nearest
// player-control object up the hit object's chain) and, when that answers true, the shooter
// (shooterUpdate +0x20); then the indirect hit (+0x2C) is reported with the victim's player id
// (-1 without one). The mine block follows for every hit. Without a hit object the indirect hit
// is reported with a null player, AI object and projectile: the next two slots then dereference
// the null player, as compiled (GameLogicCollision never passes a null hit object).
void BFCollisionHandler::handleCollision(world::IObject* projectile, world::IObject* hitObject, const Vec3& at)
{
    if (!hitObject)
    {
        world::IProjectileObject* const proj = projectile
            ? static_cast<world::IProjectileObject*>(projectile->queryInterface(world::IID_IProjectileObject))
            : 0;
        if (!proj)
            return;
        world::IPlayer* const player = asGenericProjectile(proj)->getPlayer();                 // +0x54
        if (!player)
            return;
        world::IObject* const vehicle = player->getVehicle();                                  // +0x48
        if (!vehicle || !aiObjectOf(vehicle))
            return;
        handleIndirectProjectileHit(0, 0, static_cast<world::IProjectileObject*>(0), projectile, -1);   // +0x2C
        return;
    }
    if (!projectile)
        return;

    world::IProjectileObject* const proj =
        static_cast<world::IProjectileObject*>(projectile->queryInterface(world::IID_IProjectileObject));
    world::IPlayer* const player = proj ? asGenericProjectile(proj)->getPlayer() : 0;
    world::IObject* const vehicle = player ? player->getVehicle() : 0;
    IAIObject* const shooter = vehicle ? aiObjectOf(vehicle) : 0;
    if (shooter)
    {
        int victimPlayerId = -1;
        IAIObject* const victim = aiObjectOf(world::getRootParent(hitObject));
        if (victim)
        {
            world::IPlayerControlObject* pco = 0;
            for (world::IObject* o = hitObject; o; o = o->getParent())
            {
                pco = static_cast<world::IPlayerControlObject*>(
                    o->queryInterface(world::IID_IPlayerControlObject));
                if (pco)
                    break;
            }
            world::IPlayer* const victimPlayer = pco ? pco->getOccupingPlayer() : 0;           // +0x10
            if (victimPlayer)
            {
                victimPlayerId = victimPlayer->getId();                                        // +0x54
                const Vec3& position = projectile->getAbsolutePosition();                      // +0x6C
                IAICollisionHandler* const ai =
                    checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler));
                if (ai->victimUpdate(victimPlayerId, projectile->getObjectId(), position, victim, shooter))
                    static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler)
                        ->shooterUpdate(asGenericProjectile(proj)->getPlayer()->getId(),
                                        projectile->getObjectId(), hitObject, victim, at);
            }
        }
        handleIndirectProjectileHit(player, shooter, proj, projectile, victimPlayerId);         // +0x2C
    }
    reportPlacedMine(projectile, projectile->getObjectId());
}

// handleProjectileTimeout - bf2 0x83FDA30, 483 B. GenericProjectile's reset and detonate: the
// owner's AI hears the round's object id, the owner's id, the round's position and whether its
// template carries a sticky-collision component (+0x24). A round that shows a mine icon is
// removed from the mine manager (+0x14).
// [CHOSEN, not recovered] the return value: the image sets none; both callers discard it.
int BFCollisionHandler::handleProjectileTimeout(world::IProjectileObject* projectile, world::IObject* owner)
{
    world::IPlayer* const player = asGenericProjectile(projectile)->getPlayer();               // +0x54
    if (player)
    {
        const int playerId = player->getId();
        const unsigned int objectId = owner->getObjectId();
        bool sticky = false;
        const world::IObjectTemplate* const objectTemplate = owner->getTemplate();
        if (objectTemplate && objectTemplate->getComponentFromClassID(world::CID_StickyCollisionCompTemplate))   // +0x60
            sticky = true;
        const Vec3& position = owner->getAbsolutePosition();
        checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
            ->handleProjectileTimeout(static_cast<int>(objectId), playerId, position, sticky);  // +0x24
    }
    world::IProjectileDetonationComp* const detonation = static_cast<world::IProjectileDetonationComp*>(
        owner->queryComponent(world::IID_IProjectileDetonationComp, world::IID_IProjectileDetonationComp));
    if (detonation && detonation->getShowMineIndicationIcon())
        static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager)->mineRemoved(owner->getObjectId());
    return 0;
}

// handleProjectileFire - bf2 0x83FDF80, 501 B. `object` is the fired round (GenericFireArm /
// MultiFireComp pass it as the second argument of the short form); `projectile` is not read. A
// player in a vehicle with an AI object reports the shot to the AI collision handler (+0x08: the
// start point, the round, its transformation and object id, the AI object of the shooter's root,
// the player's id and team) and marks the vehicle's last fired weapon (armament +0x28).
// [CHOSEN, not recovered] the return value: the image sets none; the caller discards it.
int BFCollisionHandler::handleProjectileFire(const Vec3& from, world::IProjectileObject* /*projectile*/,
                                             world::IObject* object, world::IPlayer* player)
{
    world::IObject* const vehicle = player->getVehicle();                                      // +0x48
    IAIObject* const shooter = vehicle ? aiObjectOf(vehicle) : 0;
    if (!shooter)
        return 0;
    world::IObject* const shooterObject = shooter->getObject();                                // +0x20
    const int team = player->getTeam();                                                        // +0xE8
    const int playerId = player->getId();                                                      // +0x54
    IAIObject* const root = aiObjectOf(world::getRootParent(shooterObject));
    const unsigned int objectId = object->getObjectId();
    const Mat4& transformation = object->getAbsoluteTransformation();                          // +0x84
    checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
        ->handleProjectileFire(from, object, transformation, static_cast<int>(objectId), root,
                               playerId, team);                                                // +0x08
    if (IAIObjectArmament* const armament = shooter->getArmament())                            // +0x84
        armament->setLastFiredWeapon();                                                        // +0x28
    return 0;
}

// handleProjectileFire - bf2 0x83FDF10, 106 B. A round with an owner is reported with its start
// point (IProjectileObject +0x6C) through the long form (+0x14).
void BFCollisionHandler::handleProjectileFire(world::IProjectileObject* projectile, world::IObject* object)
{
    if (!projectile)
        return;
    world::IPlayer* const player = asGenericProjectile(projectile)->getPlayer();               // +0x54
    if (!player)
        return;
    const Vec3 from = asGenericProjectile(projectile)->getStartPos();                          // +0x6C
    handleProjectileFire(from, projectile, object, player);                                    // +0x14
}

// handleProjectileDamage - ELF 0x83FE410, 651 B (read with objdump/rea, 2026-11-01). Called by
// GameLogicDamage::serverGiveDamage after the damage is applied, result unused. `id` is the weapon's
// template id, `type` the player id: the AI weapon template is looked up by the weapon template's
// m_aiTemplate name (a copied string, as compiled), then the shooter is that player's current
// vehicle's AI object and the victim the target's; the strength row is the weapon template's
// m_strengths for the victim's combat-strength type. Every lookup that fails returns silently.
// The handler goes through checkPointer (a null one is logged, then called, as shipped).
// `amount` and `at` are not read.
void BFCollisionHandler::handleProjectileDamage(world::IObject* target, float /*amount*/, int type,
                                                unsigned int id, Vec3 /*at*/)
{
    if (id == static_cast<unsigned int>(-1))
        return;
    world::IObjectTemplate* const objectTemplate = world::objectTemplateManager->getTemplate(id);   // +0x60
    if (!objectTemplate)
        return;
    world::SimpleObjectTemplate* const simple = static_cast<world::SimpleObjectTemplate*>(
        objectTemplate->queryImplementation(world::CID_SimpleObjectTemplate));                  // +0x50
    if (!simple)
        return;
    WeaponTemplate* weapon;
    {
        const std::string aiTemplate(simple->m_aiTemplate);                                     // +0x128
        weapon = WeaponTemplateManager::m_instance->getTemplate(aiTemplate);
    }
    if (!weapon)
        return;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(type);                 // +0x20
    if (!player)
        return;
    world::IObject* const vehicle = player->getVehicle();                                       // +0x48
    if (!vehicle)
        return;
    IAIObject* const shooter = static_cast<IAIObject*>(
        world::object_detail::LayoutAccess::aiObject(vehicle));                                 // +0x34
    if (!shooter)
        return;
    IAIObjectArmament* const armament = shooter->getArmament();                                 // +0x84
    if (!armament)
        return;
    IAIObject* const victim = static_cast<IAIObject*>(
        world::object_detail::LayoutAccess::aiObject(target));                                  // +0x34
    if (!victim)
        return;
    IAIObjectPhysical* const physical = victim->getPhysical();                                  // +0x94
    if (!physical)
        return;
    const int weaponIndex = armament->getWeaponIdFromActivationId(weapon->m_weaponActivate,    // +0x34
                                                                  weapon->m_weaponFire);
    if (weaponIndex == -1)
        return;
    const int strength = weapon->m_strengths[physical->getStrType()];                          // +0x4C
    checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
        ->handleProjectileDamage(shooter, victim, strength, weaponIndex);                       // +0x14
}


// handleExplosionDamage - bf2 0x83FE180, 651 B: GameLogicExplosion's queued damage. It compiles to
// the same instructions as handleProjectileDamage (register choice in the checkPointer diagnostic
// aside); [CHOSEN, not recovered] the body is shared through a non-virtual call.
void BFCollisionHandler::handleExplosionDamage(world::IObject* target, float amount, int type,
                                               unsigned int id, Vec3 at)
{
    BFCollisionHandler::handleProjectileDamage(target, amount, type, id, at);
}

// handleIndirectProjectileHit - bf2 0x83FDC70, 325 B: the player's team, then the AI collision
// handler's short form (+0x10). Returns its answer.
int BFCollisionHandler::handleIndirectProjectileHit(world::IPlayer* player, IAIObject* object, const Vec3& at, int a)
{
    const int team = player->getTeam();                                                        // +0xE8
    return checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
        ->handleIndirectProjectileHit(team, object, at, a);                                    // +0x10
}

// handleIndirectProjectileHit - bf2 0x83FDDC0, 332 B: the player's team, then the long form (+0x0C).
int BFCollisionHandler::handleIndirectProjectileHit(world::IPlayer* player, IAIObject* object, int a, const Vec3& at, int b)
{
    const int team = player->getTeam();                                                        // +0xE8
    return checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
        ->handleIndirectProjectileHit(team, object, a, at, b);                                 // +0x0C
}

// handleIndirectProjectileHit - bf2 0x83FDC20, 76 B: the round's object id and position, through
// the (int, Vec3) form (+0x28). `projectile` is not read.
int BFCollisionHandler::handleIndirectProjectileHit(world::IPlayer* player, IAIObject* object,
                                                    world::IProjectileObject* /*projectile*/,
                                                    world::IObject* weapon, int a)
{
    const Vec3& position = weapon->getAbsolutePosition();                                      // +0x6C
    return handleIndirectProjectileHit(player, object, static_cast<int>(weapon->getObjectId()),
                                       position, a);                                           // +0x28
}

// handleProjectileMiss - bf2 0x83FE6A0, 586 B. GameLogicCollision's terrain hit: `playerId` is the
// round's owner (-1 without one), `objectId` the round's object id. The owner's vehicle's AI
// object reports the miss (+0x18); the mine block follows with that id.
// [CHOSEN, not recovered] the return value: the image sets none; the caller discards it.
int BFCollisionHandler::handleProjectileMiss(int playerId, world::IObject* object, int objectId, const Vec3& at)
{
    if (world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId))       // +0x20
    {
        world::IObject* const vehicle = player->getVehicle();                                  // +0x48
        IAIObject* const shooter = vehicle ? aiObjectOf(vehicle) : 0;
        if (shooter)
            checkPointer(static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler))
                ->handleProjectileMiss(shooter, playerId, objectId, at);                       // +0x18
    }
    reportPlacedMine(object, static_cast<unsigned int>(objectId));
    return 0;
}

// reset - bf2 0x83FEB80, 45 B: every bot's projectile record is removed (+0x40); the count is
// read live.
void BFCollisionHandler::reset()
{
    for (int id = 0; id < m_count; ++id)
        removeBotProjectile(id);
}

// Every live, unpassed round is sampled once per AI tick: its highest Y is kept, and it is
// either still short of the target distance (the sample becomes the pre-point) or it has passed
// (flag set, the sample becomes the post-point). Records are visited by id; the count is read
// live. No null test on the record or its object. The max-height store happens on both arms;
// an unordered height keeps the old value (ELF fcmovbe / PE ordered-less test).
void BFCollisionHandler::updateBotProjectiles()
{
    for (int id = 0; id < m_count; ++id)
    {
        BotProjectile* const record = m_projectiles[id];
        if (!record->m_active || record->m_passed)
            continue;

        const Vec3& position = record->m_object->getAbsolutePosition();
        const bool shortOfTarget =
            botProjectileShortOfTarget(record->m_startPoint, position, record->m_passDistanceSq);
        record->m_maxHeight = record->m_maxHeight < position.y ? position.y : record->m_maxHeight;
        if (shortOfTarget)
            record->m_prePoint = position;
        else
        {
            record->m_passed = true;
            record->m_postPoint = position;
        }
    }
}

// [INFERRED] void: the two
// images leak different registers. No bounds or null test. The object's position is asked
// twice - once for the start point, once for the pre-point - and the maximum height is seeded
// from the second answer's Y. The pass distance is narrowed to float on the store.
void BFCollisionHandler::addBotProjectile(int id, world::IObject* object, const Vec3& at)
{
    BotProjectile* const record = m_projectiles[id];
    record->m_active = true;
    record->m_passed = false;
    record->m_object = object;
    record->m_targetPoint = at;
    record->m_startPoint = object->getAbsolutePosition();
    record->m_prePoint = object->getAbsolutePosition();
    record->m_passDistanceSq = distXZSqr(record->m_startPoint, record->m_targetPoint);
    record->m_maxHeight = record->m_prePoint.y;
}

// Both flags cleared, the array re-read between
// them; the object pointer and all points stay as they were.
void BFCollisionHandler::removeBotProjectile(int id)
{
    m_projectiles[id]->m_active = false;
    m_projectiles[id]->m_passed = false;
}

// No bounds or active test.
bool BFCollisionHandler::botProjectilePassedPoint(int id) const
{
    return m_projectiles[id]->m_passed;
}

// Pre-point copied before post-point; the array is
// re-read for the second copy in both images.
void BFCollisionHandler::botProjectilePrePostPoints(int id, Vec3& pre, Vec3& post)
{
    pre = m_projectiles[id]->m_prePoint;
    post = m_projectiles[id]->m_postPoint;
}

// the record's address + 8.
const Vec3& BFCollisionHandler::botProjectilePrePoint(int id)
{
    return m_projectiles[id]->m_prePoint;
}

// the record's address + 0x14.
const Vec3& BFCollisionHandler::botProjectilePostPoint(int id)
{
    return m_projectiles[id]->m_postPoint;
}

// the record's address + 0x2C.
const Vec3& BFCollisionHandler::botProjectileTargetPoint(int id)
{
    return m_projectiles[id]->m_targetPoint;
}

// the float at + 0x3C, returned in st0.
float BFCollisionHandler::botProjectileMaxHeight(int id)
{
    return m_projectiles[id]->m_maxHeight;
}

} // namespace ai
} // namespace hfe
} // namespace dice
