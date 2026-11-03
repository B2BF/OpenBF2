// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EA970 (__tcf_0 0x1EA9D0, _GLOBAL__I 0x1EAA00): <iostream>'s Init object only.
#include <iostream>

#include "BAPConSplashDamage.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EA2F0 (C1; C2 0x1EA280)
BAPConSplashDamage::BAPConSplashDamage(unsigned int object, BAPVariable<Vec3>* targetPoint,
                                       BAPVariable<Vec3>* variable7C, float radius)
{
    m_object = object;
    m_targetPoint = targetPoint;
    m_variable7C = variable7C;
    m_radiusSqr = radius * radius;
}

// ELF 0x1EAA20 (D1; D0 0x1EAA70): the Aimer and the base only.
BAPConSplashDamage::~BAPConSplashDamage()
{
}

// ELF 0x1EA360. Casts the shot along the weapon's projectile-origin row 2 out to the target's last
// seen position + 5 m, ignoring the bot's own object. True when it hits within the radius of the
// target point and no soldier of the bot's side stands within the radius of the hit (|dy| < 2.5).
// Without the target information or its sensed record: false. Every other false answer stamps
// BotMain::setTimeOfLastFailedSplashDamageAttack(now). The bot's information and its weapon are
// not null-checked.
bool BAPConSplashDamage::evaluate(Bot* bot)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    IPIArmament* armament = self->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!bapinline::info(m_object))
        return false;
    SensedObject* sensed = bot->getSensedObjectFromHandle(m_object);
    if (!sensed)
        return false;

    const float drag = weapon->getDrag();
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = weapon->getExitVelocity();
    const float gravityModifier = weapon->getGravityModifier();
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);

    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3 from = bapinline::vec3(origin.m[12], origin.m[13], origin.m[14]);
    const Vec3 d = bapinline::sub(sensed->m_lastSeenPosition, from);
    const float reach = std::sqrt(d.z * d.z + d.x * d.x + d.y * d.y) + 5.0f;
    const Vec3 to = bapinline::vec3(origin.m[8] * reach + origin.m[12], origin.m[9] * reach + origin.m[13],
                                    reach * origin.m[10] + origin.m[14]);
    Vec3 hit = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (bapinline::collideLineIgnoring(from, to, self->getAIObject(), hit)) {
        const std::vector<IAIPlayer*>& players =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
        bool friendlyInSplash = false;
        for (unsigned int i = 0; i < players.size() && !friendlyInSplash; ++i) {
            IAIPlayer* player = players[i];
            if (!player || player->getSide() != bot->getSide())
                continue;
            IAIObject* vehicle = player->getCurrentVehicle();
            if (!vehicle)
                continue;
            Information* info = bapinline::info(vehicle->getInformation(bot->getSide()));
            if (info && (info->getObjectTypeFlags() & 0x200000u) && std::fabs(info->getPosition().y - hit.y) < 2.5f) {
                const Vec3& p = info->getPosition();
                if ((hit.z - p.z) * (hit.z - p.z) + (hit.x - p.x) * (hit.x - p.x) + (hit.y - p.y) * (hit.y - p.y)
                    < m_radiusSqr)
                    friendlyInSplash = true;
            }
        }
        if (!friendlyInSplash) {
            const Vec3& t = m_targetPoint->m_value;
            const float dx = t.x - hit.x;
            const float dy = t.y - hit.y;
            const float dz = t.z - hit.z;
            if (dx * dx + dy * dy + dz * dz < m_radiusSqr)
                return true;
        }
    }
    static_cast<BotMain*>(bot)->setTimeOfLastFailedSplashDamageAttack(bapinline::gameTime());
    return false;
}

// ELF 0x1EA960
BAPConditionType BAPConSplashDamage::getConditionType() const
{
    return BAPCTSplashDamage;   // 57
}

} // namespace ai
} // namespace hfe
} // namespace dice
