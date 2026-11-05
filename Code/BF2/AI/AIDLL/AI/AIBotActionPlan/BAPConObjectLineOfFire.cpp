// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EF940 (__tcf_0 0x1EF9A0, _GLOBAL__I 0x1EF9D0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectLineOfFire.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Function statics of hasFriendlySoldiersInLineOfFire (ELF .data 0x4B9784 / 0x4B9788 / 0x4B978C).
const float allowedDistanceShort = 0.5f;
const float allowedDistanceFar = 1.5f;
const float minDistForSideStep = 5.0f;

} // namespace

// ELF 0x1EF100 (C1; C2 0x1EF0C0)
BAPConObjectLineOfFire::BAPConObjectLineOfFire(unsigned int object, bool flag)
{
    m_anyPose = true;
    m_object = object;
    m_flag0D = flag;
}

// ELF 0x1EF180 (C1; C2 0x1EF140)
BAPConObjectLineOfFire::BAPConObjectLineOfFire(unsigned int object, bool flag, SoldierPose pose)
{
    m_anyPose = false;
    m_object = object;
    m_flag0D = flag;
    m_pose = pose;
}

// ELF 0x1EF8D0 (D1; D0 0x1EF8F0): base only.
BAPConObjectLineOfFire::~BAPConObjectLineOfFire()
{
}

// ELF 0x1EF1C0. True when the bot senses the target directly (record without flag 8); a weapon
// that cannot fire through transparent objects also needs flag 0x40 on the record. A soldier
// additionally needs no friendly soldier in the line of fire. No information for the bot or the
// target: false.
bool BAPConObjectLineOfFire::evaluate(Bot* bot)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    Information* target = bapinline::info(m_object);
    if (!self || !target)
        return false;
    SensedObject* sensed = bot->getSensedObjectFromHandle(m_object);
    bool result = sensed && !(sensed->m_flags & 8u);
    IPIArmament* armament = self->getArmament();
    if (armament && armament->getActiveWeapon() != -1) {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
        if (weapon && sensed && !weapon->getFiresThroughTransparent()) {
            const bool clear = !(sensed->m_flags & 8u) && !(sensed->m_flags & 0x40u);
            result = result && !clear;
        }
    }
    if (result && self->isOfType(ITSoldier))
        result = !hasFriendlySoldiersInLineOfFire(bot, self, target);
    return result;
}

// ELF 0x1EF360. Friendly soldiers (same side, not the bot, not the target) between the weapon's
// projectile origin and the target, within a corridor widening from allowedDistanceShort at the
// muzzle to allowedDistanceFar at 50 m (support kit (spawn kit 1) with a range above 100 m and a
// weapon zoom other than 0x40: allowedDistanceShort throughout). A friend closer than
// minDistForSideStep in the bot's facing (XZ dot > 0.9) stamps BotMain::setFriendsInLineOfFireTime
// (now) and answers at once; otherwise the stamp is cleared (0.0) after the scan. The bot's weapon
// is not null-checked.
bool BAPConObjectLineOfFire::hasFriendlySoldiersInLineOfFire(Bot* bot, Information* self, Information* target)
{
    BotMain* main = static_cast<BotMain*>(bot);
    IPIArmament* armament = self->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3& tp = target->getPosition();
    Vec3 dir = bapinline::vec3(tp.x - origin.m[12], tp.y - origin.m[13], tp.z - origin.m[14]);
    const float distance = std::sqrt(dir.z * dir.z + dir.x * dir.x + dir.y * dir.y);
    if (0.0001f < distance) {
        const float inv = 1.0f / distance;
        dir.x *= inv;
        dir.y *= inv;
        dir.z = inv * dir.z;
    }

    bool found = false;
    const std::vector<IAIPlayer*>& players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
    for (unsigned int i = 0; i < players.size(); ++i) {
        IAIPlayer* player = players[i];
        if (!player || player->getSide() != bot->getSide())
            continue;
        IAIObject* vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* info = bapinline::info(vehicle->getInformation(bot->getSide()));
        if (info == self || info == target || !info || !info->isOfType(ITSoldier))
            continue;
        const Vec3& p = info->getPosition();
        const Vec3 rel = bapinline::vec3(p.x - origin.m[12], p.y - origin.m[13], p.z - origin.m[14]);
        const float along = dir.z * rel.z + dir.y * rel.y + dir.x * rel.x;
        if (distance < along)
            continue;
        const Vec3 perp = bapinline::vec3(rel.x - dir.x * along, rel.y - along * dir.y, rel.z - dir.z * along);
        const float allowed = (along / 50.0f) * (allowedDistanceFar - allowedDistanceShort) + allowedDistanceShort;
        float limit = allowed;
        if (weapon->getWeaponZoom() != 0x40 && bot->getSpawnKit() == 1) {
            limit = allowedDistanceShort;
            if (weapon->getMaxRange() <= 100.0f)
                limit = allowed;
        }
        if (!(perp.z * perp.z + perp.y * perp.y + perp.x * perp.x < limit * limit))
            continue;
        found = true;
        if (!(along < minDistForSideStep))
            continue;
        Vec2 dir2;
        dir2.x = 0.0f;
        dir2.y = 0.0f;
        vec3ToVec2XZ(dir, dir2);
        bapinline::normalize(dir2);
        const Vec3 facing = self->getFaceing();
        Vec2 face2;
        face2.x = 0.0f;
        face2.y = 0.0f;
        vec3ToVec2XZ(facing, face2);
        if (0.9f < dir2.x * face2.x + dir2.y * face2.y) {
            main->setFriendsInLineOfFireTime(bapinline::gameTime());
            return true;
        }
    }
    main->setFriendsInLineOfFireTime(0.0);
    return found;
}

// ELF 0x1EF930
BAPConditionType BAPConObjectLineOfFire::getConditionType() const
{
    return BAPCTObjectLineOfFire;   // 26
}

} // namespace ai
} // namespace hfe
} // namespace dice
