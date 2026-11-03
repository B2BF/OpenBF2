// VehicleUrgencyCalculator: libaidll.so TU VehicleUrgencyCalculator.cpp (0x2265E0..0x228EF0), the
// vehicle scoring behind BBChange (which vehicle / seat a bot should take). Recovered by ai-bot
// (coordinator assignment 2026-11-01) from the ELF bodies; the strength rows are IPIUnit +8
// (getBattleStrengths), indexed by the IPIPhysical strength type (vtable +0x44).

// static init: ELF 0x228E60 (__tcf_0 0x228EC0, _GLOBAL__I 0x228EF0): <iostream>'s Init object only.
#include <iostream>

#include "VehicleUrgencyCalculator.h"

#include <cmath>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/FireObject.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../Main/AIMain.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIObjectUnit.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

const int kMaxStrengths = 64;   // [CHOSEN] stack row size (the image's frame holds 15 / 83 floats)

Information* infoOf(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

int strengthCount()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
}

// The bot's soldier's information for `side` (Bot +0xBC getPlayer, IAIPlayer +0x4C getSoldier,
// IAIObject +0xB8 getInformation).
Information* soldierInfo(Bot* bot, int side)
{
    return infoOf(bot->getPlayer()->getSoldier()->getInformation(side));
}

// The inline Vec3 normalisation: unchanged within FLT_EPSILON of unit length; false (left as is)
// within FLT_EPSILON of zero, where the image skips the camera test.
bool normalise(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-07f)
        return true;
    if (std::fabs(lengthSq) < 1.1920929e-07f)
        return false;
    const float inverse = 1.0f / std::sqrt(lengthSq);
    v.x = v.x * inverse;
    v.y = v.y * inverse;
    v.z = inverse * v.z;
    return true;
}

// The armament's weapon range envelope: max of the maximum ranges (from 50), min of the minimum
// ranges (from 0.1); `guided` when any weapon has a guidance type.
void weaponRanges(IPIArmament* armament, float& maxRange, float& minRange, bool* guided)
{
    std::vector<unsigned int>& weapons = armament->getWeapons();
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon)
            continue;
        if (guided && weapon->getWeaponGuidanceType() != 0)
            *guided = true;
        const float maxR = weapon->getMaxRange();
        const float minR = weapon->getMinRange();
        if (maxRange < maxR)
            maxRange = maxR;
        if (minR < minRange)
            minRange = minR;
    }
}

// The occupied seats' fire strength added to `strengths`: 0.4 of the primary object's row when it
// is occupied, plus `share` (0.4; 0.9 for an unarmed primary) of every occupied secondary's row.
// Inline in calculateVehicleFireUrgency and calculateFireStrength.
void addSeatStrengths(float* strengths, int n, Information* vehicle, int side, bool flag,
                      Information* current)
{
    Information* primary;
    float share = 0.4f;
    if (!vehicle->isPrimary())
    {
        primary = infoOf(vehicle->getPrimaryObject(side));
        IPIUnit* const unit = primary->getUnit();
        if ((primary != current || !flag) && unit->isOccupied())
        {
            for (int i = 0; i < n; ++i)
                strengths[i] = unit->getBattleStrengths()[i] * 0.4f + strengths[i];
        }
    }
    else
    {
        primary = vehicle;
        if (!vehicle->hasArmament())
            share = 0.9f;
    }
    int iterator;
    Information* secondary = infoOf(primary->getFirstSecondaryObject(iterator, side));
    while (secondary)
    {
        if (secondary != vehicle)
        {
            IPIUnit* const unit = secondary->getUnit();
            if ((secondary != current || !flag) && unit->isOccupied())
            {
                for (int i = 0; i < n; ++i)
                    strengths[i] = share * unit->getBattleStrengths()[i] + strengths[i];
            }
        }
        secondary = infoOf(primary->getSecondaryObject(iterator, side));
    }
}

bool closeAndShouldAvoidVehiclesImpl(Information* vehicle, int side, Bot* bot);

// [CHOSEN, not recovered] No GNU unused attribute: these external helpers also compile on MSVC.
// ELF 0x226C90 (no caller in the image): sum over six components of a*b (b where a is 0), x0.25.
float sqrDistance(const float* a, const float* b)
{
    float sum = 0.0f;
    for (int i = 0; i < 6; ++i)
        sum = sum + (a[i] != 0.0f ? a[i] * b[i] : b[i]);
    return sum * 0.25f;
}

// ELF 0x226CF0 (no caller in the image): 1 / the (larger, when `both`) strength, capped at 10.
float vulnerability(const float* strengths, int type, bool both, int other)
{
    float s = strengths[type];
    if (both && !(strengths[other] <= strengths[type]))
        s = strengths[other];
    if (s == 0.0f)
        return 10.0f;
    const float inverse = 1.0f / s;
    return 10.0f < inverse ? 10.0f : inverse;
}

// ELF 0x226D60 (no caller in the image): the squared strongest seat strength against an enemy
// type with weight > 0, minus the enemy's strength against the vehicle (and its exposed soldier).
float calculateVehicleFireUrgency(Bot* bot, Information* vehicle,
                                                          const float* enemyStrengths, const float* weights,
                                                          int, bool flag, Information* current, int side)
{
    const int n = strengthCount();
    IPIUnit* const unit = vehicle->getUnit();
    const int ownType = vehicle->getPhysical()->getStrengthType();
    const bool exposed = unit->hasExposedSoldier();
    if (closeAndShouldAvoidVehiclesImpl(vehicle, side, bot))
        return 0.0f;
    int soldierType = 0;
    if (exposed)
        soldierType = soldierInfo(bot, side)->getPhysical()->getStrengthType();
    const bool usable = vehicle != current || !flag;
    float strengths[kMaxStrengths];
    for (int i = 0; i < n; ++i)
        strengths[i] = usable ? unit->getBattleStrengths()[i] : 0.0f;
    addSeatStrengths(strengths, n, vehicle, side, flag, current);
    float best = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (0.0f < weights[i])
        {
            const float sq = strengths[i] * strengths[i];
            if (best < sq)
                best = sq;
        }
    }
    if (!exposed)
        return best - enemyStrengths[ownType];
    const float threat = enemyStrengths[soldierType] <= enemyStrengths[ownType] ? enemyStrengths[ownType]
                                                                               : enemyStrengths[soldierType];
    return best - threat;
}

// ELF 0x227150. Vehicles the bot should avoid are worth nothing. A primary mobile vehicle: max
// speed x5. A seat of a mobile primary: an empty primary is worth x4 faded over the 15 s after a
// player of this side last possessed it (0 otherwise); a primary a bot drives x2.5 when that bot
// is ordered elsewhere than this one (SAI +0xB8 getBotOrderedPos), else x4.
float calculateVehicleMoveUrgency(Information* vehicle, int side, Bot* bot, bool flag, Information* current)
{
    if (closeAndShouldAvoidVehiclesImpl(vehicle, side, bot))
        return 0.0f;
    const unsigned int botId = bot->getBotId();
    if (!vehicle->isPrimary())
    {
        Information* const primary = infoOf(vehicle->getPrimaryObject(side));
        IPIMobile* const mobile = primary->getMobile();
        if (mobile && (primary != current || !flag))
        {
            IPIUnit* const unit = primary->getUnit();
            if (!unit->isOccupied())
            {
                float urgency = 0.0f;
                if (primary->getSide() == side)
                {
                    const double now =
                        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
                    const double since = now - unit->getLastPlayerPossesionTime();
                    if (since < 15.0)
                        urgency = (1.0f - static_cast<float>(since / 15.0)) * (mobile->getMaxSpeed() * 4.0f);
                }
                return urgency;
            }
            Bot* const driver = IBotManager::instance->getBot(primary->getAIObject());
            SAI* const sai = IAIMain::m_instance->getSAI(side);
            if (driver)
            {
                const int mine = sai->getBotOrderedPos(botId);
                if (mine != sai->getBotOrderedPos(driver->getBotId()))
                    return mobile->getMaxSpeed() * 2.5f;
            }
            return mobile->getMaxSpeed() * 4.0f;
        }
    }
    else if (vehicle->getMobile())
    {
        return vehicle->getMobile()->getMaxSpeed() * 5.0f;
    }
    return 0.0f;
}

// ELF 0x2273B0 (1814 instructions). The fire value of taking `vehicle`:
// * a static emplacement without guided or anti-aircraft weapons is worth nothing beyond 12 m / 2 m
//   of height of the current equipment;
// * with sensed objects: the best sided target (static: within weapon range, not an indirect sighting
//   younger than 20 s) the seat out-guns the current equipment against, s^2 minus the target's
//   strength against the vehicle; x4 unless the vehicle should be avoided; no target engaged in a
//   driven vehicle falls back to the raw value;
// * nothing sensed, undriven: the best enemy object in weapon range (validity test as above); no
//   enemy at all: 0 unless no neighbouring area of the bot's ordered area is in camera reach;
// * the raw value (fallback): the best weighted row strength minus the threat.
float calculateFireStrength(Bot* bot, Information* vehicle, int side, const float* enemyStrengths,
                            const float* weights, bool flag, Information* current)
{
    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    const int n = strengthCount();
    float fireThreat = 0.0f;
    bool engaged = false;
    const Vec3& position = vehicle->getPosition();
    float maxRange = 50.0f;
    IPIUnit* const unit = vehicle->getUnit();
    float minRange = 0.1f;
    IPIPhysical* const physical = vehicle->getPhysical();
    IPIControlInfo* const control = vehicle->getControlInfo();
    IPIArmament* armament = vehicle->getArmament();

    bool skipProximity = false;
    if (armament)
    {
        const int antiAircraft = armament->isAntiAircraft();
        bool guided = false;
        weaponRanges(armament, maxRange, minRange, &guided);
        skipProximity = guided || antiAircraft == 1;
    }
    if (!skipProximity && vehicle->isPrimary() && !vehicle->hasMobile())
    {
        const Vec3& here = current->getPosition();
        const float dx = position.x - here.x;
        const float dy = position.y - here.y;
        const float dz = position.z - here.z;
        if (144.0f < dx * dx + dy * dy + dz * dz
            || 2.0f < std::fabs(current->getPosition().y - position.y))
            return 0.0f;
    }

    const bool usable = vehicle != current || !flag;
    IPIMobile* driven;          // -0x1D8: the mobile of the vehicle / a manned primary
    bool noMobile;              // -0x1F1
    if (!vehicle->isPrimary())
    {
        Information* const primary = infoOf(vehicle->getPrimaryObject(bot->getSide()));
        if (!primary->getUnit() || current == primary || !primary->getUnit()->isOccupied())
        {
            noMobile = true;
            driven = 0;
        }
        else
        {
            driven = primary->getMobile();
            noMobile = false;
        }
    }
    else
    {
        driven = vehicle->getMobile();
        noMobile = driven == 0;
    }
    const int ownType = physical->getStrengthType();
    const bool exposed = unit->hasExposedSoldier();
    int soldierType = 0;
    if (exposed)
        soldierType = soldierInfo(bot, side)->getPhysical()->getStrengthType();

    // A seat that must borrow the soldier's weapons scores with the soldier's row and ranges.
    IAIObject* const object = vehicle->getAIObject();
    IPIUnit* strengthUnit = unit;
    if (usable && object->getUnit()->mustBorrowArmament(object))
    {
        Information* const soldier = soldierInfo(bot, side);
        maxRange = 50.0f;
        strengthUnit = soldier->getUnit();
        minRange = 0.1f;
        armament = soldier->getArmament();
        if (armament)
            weaponRanges(armament, maxRange, minRange, 0);
    }
    float strengths[kMaxStrengths];
    for (int i = 0; i < n; ++i)
        strengths[i] = usable ? strengthUnit->getBattleStrengths()[i] : 0.0f;
    addSeatStrengths(strengths, n, vehicle, side, flag, current);

    if (sensed.empty())
    {
        if (driven == 0)
        {
            float reach = 0.0f;
            if (!armament && !object->getUnit()->mustBorrowArmament(object))
                return 0.0f;
            if (!armament)
                armament = soldierInfo(bot, side)->getArmament();
            const std::vector<unsigned int> weapons = armament->getWeapons();
            for (unsigned int i = 0; i < weapons.size(); ++i)
            {
                if (weapons[i] == 0xFFFFFFFFu)
                    continue;
                WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
                if (!weapon)
                    continue;
                const float r = weapon->getMaxRange();
                if (reach < r)
                    reach = r;
            }
            std::vector<unsigned int> objects;
            static_cast<const IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
                ->getEnemyObjects(objects, bot, reach);
            float value = 0.0f;
            float best = 0.0f;
            if (!objects.empty())
            {
                for (unsigned int k = 0; k < objects.size(); ++k)
                {
                    Information* const target = infoOf(objects[k]);
                    if (!target)
                        continue;
                    const Vec3& p = target->getPosition();
                    const float dx = p.x - position.x;
                    const float dy = p.y - position.y;
                    const float dz = p.z - position.z;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (!(distance <= maxRange && minRange <= distance))
                        continue;
                    IPIUnit* const targetUnit = target->getUnit();
                    IPIPhysical* const targetPhysical =
                        target->isPrimary() ? target->getPhysical()
                                            : infoOf(target->getPrimaryObject(side))->getPhysical();
                    if (!targetUnit || !targetPhysical)
                        continue;
                    bool valid = false;
                    Vec3 direction;
                    direction.x = target->getPosition().x - position.x;
                    direction.y = target->getPosition().y - position.y;
                    direction.z = target->getPosition().z - position.z;
                    if (normalise(direction) && control->validateCameraDirection(direction))
                        valid = true;
                    // 0x227D1D..0x227D5E: the same row entry compared with itself - taken unless NaN.
                    const float s = strengths[targetPhysical->getStrengthType()];
                    if ((valid && s > strengths[targetPhysical->getStrengthType()])
                        || s == strengths[targetPhysical->getStrengthType()])
                    {
                        const float* const theirs = targetUnit->getBattleStrengths();
                        if (!exposed)
                            value = s * s - theirs[ownType];
                        else
                            value = s * s - (theirs[soldierType] <= theirs[ownType] ? theirs[ownType]
                                                                                    : theirs[soldierType]);
                    }
                    if (best < value)
                        best = value;
                }
                return best;
            }
            // No enemy in reach: the bot's ordered area (SAI +0xB8; looked up with -1 only when the
            // current one, +0xB4, is -1 too).
            SAI* const sai = IAIMain::m_instance->getSAI(side);
            const int ordered = sai->getBotOrderedPos(bot->getBotId());
            if (ordered != -1 || sai->getBotCurrentPos(bot->getBotId()) == -1)
            {
                AIStrategicObject* const area = AIStrategicObjectManager::getInstance()->getStrategicObject(ordered);
                if (area)
                {
                    const Vec2 from = area->getPosition(side);
                    const std::vector<AIStrategicObject*>& neighbours =
                        static_cast<AIStrategicArea*>(area)->m_neighbours;   // +0x11C
                    for (unsigned int k = 0; k < neighbours.size(); ++k)
                    {
                        AIStrategicObject* const neighbour = neighbours[k];
                        if (static_cast<int>(neighbour->getStatus(side)) == 0)
                            continue;
                        const Vec2& to = neighbour->getPosition(side);
                        Vec2 offset;
                        offset.x = to.x - from.x;
                        offset.y = to.y - from.y;
                        Vec3 direction;
                        direction.x = 0.0f;
                        direction.y = 0.0f;
                        direction.z = 0.0f;
                        vec2ToVec3XZ(offset, direction);
                        if (normalise(direction) && control->validateCameraDirection(direction))
                            noMobile = false;
                    }
                }
            }
            if (!noMobile)
                return 0.0f;
        }
    }
    else
    {
        bool valid = true;
        float best = 0.0f;
        IPIUnit* const currentUnit = current->getUnit();
        const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
        {
            Information* const target = infoOf(it->first);
            const SensedObject* const record = it->second;
            if (!target || (record->m_flags & SensedObject::SENSE_SIDED) == 0)
                continue;
            if (!vehicle->hasMobile() && armament)
            {
                const Vec3& p = target->getPosition();
                const float dx = p.x - position.x;
                const float dy = p.y - position.y;
                const float dz = p.z - position.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (maxRange < distance || distance < minRange
                    || ((record->m_flags & SensedObject::SENSE_INDIRECT) && now < record->m_firstSeenTime + 20.0))
                    continue;
            }
            IPIUnit* const targetUnit = target->getUnit();
            IPIPhysical* const targetPhysical =
                target->isPrimary() ? target->getPhysical() : infoOf(target->getPrimaryObject(side))->getPhysical();
            if (!targetUnit || !targetPhysical)
                continue;
            if (driven == 0)
            {
                valid = false;
                Vec3 direction;
                direction.x = target->getPosition().x - position.x;
                direction.y = target->getPosition().y - position.y;
                direction.z = target->getPosition().z - position.z;
                if (normalise(direction) && control->validateCameraDirection(direction))
                {
                    noMobile = false;
                    valid = true;
                }
            }
            const int type = targetPhysical->getStrengthType();
            const float s = strengths[type];
            float value = 0.0f;
            if (valid && currentUnit->getBattleStrengths()[type] <= unit->getBattleStrengths()[type])
            {
                engaged = true;
                const float* const theirs = targetUnit->getBattleStrengths();
                if (!exposed)
                    value = s * s - theirs[ownType];
                else
                    value = s * s - (theirs[soldierType] <= theirs[ownType] ? theirs[ownType] : theirs[soldierType]);
            }
            if (value <= best)
                value = best;
            best = value;
        }

        // The strongest incoming fire against the vehicle's type, over a copy of the list (the image's
        // sensed-object find and the two side reads per entry have no effect).
        const std::list<std::pair<FireObject*, unsigned int> > fire = *bot->getIncommingFire();
        for (std::list<std::pair<FireObject*, unsigned int> >::const_iterator it = fire.begin(); it != fire.end(); ++it)
        {
            const float strength = it->first->getBattleStrength(ownType);
            if (fireThreat < strength)
                fireThreat = strength;
        }
        if (driven != 0 || !noMobile)
        {
            if (!(!engaged && driven != 0))
            {
                if (!closeAndShouldAvoidVehiclesImpl(vehicle, side, bot))
                    return best * 4.0f;
                return 0.0f;
            }
            // not engaged in a driven vehicle: the raw value below
        }
        else
        {
            return 0.0f;
        }
    }

    // The raw fire value: the best weighted strength (x6 for weights <= 0.5, else x3 x weight; a
    // running maximum) against an enemy type with weight > 0, minus the threat to the vehicle.
    if ((!vehicle->isPrimary() || vehicle->hasMobile()) && !closeAndShouldAvoidVehiclesImpl(vehicle, side, bot))
    {
        float best = 0.0f;
        float running = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float w = weights[i];
            float v = w <= 0.5f ? strengths[i] * 6.0f : strengths[i] * 3.0f * w;
            if (v <= running)
                v = running;
            if (0.0f < w && best < v)
                best = v;
            running = v;
        }
        if (best <= 0.0f)
            return running;
        float threat;
        if (!exposed)
            threat = fireThreat + enemyStrengths[ownType];
        else
            threat = (enemyStrengths[soldierType] <= enemyStrengths[ownType] ? enemyStrengths[ownType]
                                                                             : enemyStrengths[soldierType])
                     + fireThreat;
        return best - threat;
    }
    return 0.0f;
}

// ELF 0x2265E0. A ground vehicle (primary not a soldier, not flags 0x10 / 0x20) while the bot's
// squad holds a takeable area whose order position is outside its control point radius: avoided
// when the bot already uses it (same primary) within 25 m of the order position (unless the bot
// is in an armed primary and the side's SAI has owned control points, +0x74), or when the vehicle
// or the squad is within 70 m of the control point.
bool closeAndShouldAvoidVehiclesImpl(Information* vehicle, int side, Bot* bot)
{
    bool avoid = false;
    Information* const primary = infoOf(vehicle->getPrimaryObject(side));
    const unsigned int flags = primary->getObjectTypeFlags();
    if ((flags & 0x200000u) || (flags & 0x10u) || (flags & 0x20u))
        return false;
    IPIMobile* const mobile = primary->getMobile();
    AIStrategicArea* const area = bot->getSquadMember()->getSquad()->getSquadStrategicAreaAssignment();
    if (!area || !area->isAreaTakeable() || !mobile)
        return false;
    if (!area->isOrderPosOutsideCpRadius(mobile->getVehicleNumber(), side))
        return false;
    Vec3 squad;
    squad.x = 0.0f;
    squad.y = 0.0f;
    squad.z = 0.0f;
    if (!bot->getSquadMember()->getPosition(squad))
        return false;
    Information* const equipment = infoOf(bot->getCurrentEquipment());
    if (equipment->getPrimaryObject(side) == primary->getPrimaryObject(side))
    {
        const Vec3 order = area->getOrderPos(mobile->getVehicleNumber(), side);
        const float dx = squad.x - order.x;
        const float dy = squad.y - order.y;
        const float dz = squad.z - order.z;
        if (dx * dx + dy * dy + dz * dz < 625.0f)
        {
            avoid = true;
            if (equipment->isPrimary() && equipment->hasArmament()
                && IAIMain::m_instance->getSAI(side)->haveAnyOwnedCps(side))
                avoid = false;
        }
    }
    else
    {
        Vec3 centre;
        centre.x = 0.0f;
        centre.y = 0.0f;
        centre.z = 0.0f;
        float radius = 1.0f;
        bool active;
        static_cast<const IAIGame*>(CommonSingletonServer::m_iaiGame)
            ->getControlPointInfo(area->m_controlPointId, centre, radius, active);
        const Vec3& p = primary->getPosition();
        if ((p.z - centre.z) * (p.z - centre.z) + (p.x - centre.x) * (p.x - centre.x)
                    + (p.y - centre.y) * (p.y - centre.y) < 4900.0f
            || (centre.x - squad.x) * (centre.x - squad.x) + (centre.y - squad.y) * (centre.y - squad.y)
                    + (centre.z - squad.z) * (centre.z - squad.z) < 4900.0f)
            avoid = true;
    }
    return avoid;
}

} // namespace

// ELF 0x2265E0.
bool closeAndShouldAvoidVehicles(Information* vehicle, int side, Bot* bot)
{
    return closeAndShouldAvoidVehiclesImpl(vehicle, side, bot);
}

// ELF 0x2269E0 (static double allowedTimeDiff = 15.0 at 0x4BB688). Nothing for a vehicle booked by
// another player, more than 8 m above ground (unless flag 0x10 or the current one), occupied (with
// `flag`) or not team-locked to the side. Otherwise fire + move value (+ 4 x the strength when
// above 0.01), scaled by health when it is at least the bail value (x0.3 for the current primary;
// soldiers always), and faded over 15 s after the side last possessed it. The two floats are unread.
float calculateVehicleUrgency(Bot* bot, Information* vehicle, const float* strengths, const float* weights,
                              bool flag, Information* current, float, float, int side)
{
    static double allowedTimeDiff = 15.0;

    IPIPhysical* const physical = vehicle->getPhysical();
    IPIUnit* const unit = vehicle->getUnit();
    if (vehicle->isBooked() && !vehicle->isBookedBy(bot->getPlayerId()))
        return 0.0f;
    if (vehicle != current && (vehicle->getObjectTypeFlags() & 0x10u) == 0)
    {
        if (8.0f < static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                       ->getAltitude(vehicle->getPosition()))
            return 0.0f;
    }
    if ((flag && unit->isOccupied()) || !unit->isTeamLocked(side))
        return 0.0f;
    const float fire = calculateFireStrength(bot, vehicle, side, strengths, weights, flag, current);
    const float move = calculateVehicleMoveUrgency(vehicle, side, bot, flag, current);
    float urgency = move + fire;
    if (0.01f < urgency)
        urgency = vehicle->getStrength() * 4.0f + urgency;
    const float health = physical->getHealth();
    float bail = IBotManager::instance->getVehicleBailValue();
    if ((vehicle->getObjectTypeFlags() & 0x200000u) == 0 && vehicle->isPrimary()
        && vehicle->getHandle() == current->getHandle())
        bail = bail * 0.3f;
    if (!(bail <= health) && (vehicle->getObjectTypeFlags() & 0x200000u) == 0)
        return 0.0f;
    urgency = urgency * health;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (vehicle->getSide() == side)
    {
        const double since = now - unit->getLastPlayerPossesionTime();
        if (since < allowedTimeDiff)
            urgency = static_cast<float>(1.0 / allowedTimeDiff * since) * urgency;
    }
    return urgency;
}

} // namespace ai
} // namespace hfe
} // namespace dice
