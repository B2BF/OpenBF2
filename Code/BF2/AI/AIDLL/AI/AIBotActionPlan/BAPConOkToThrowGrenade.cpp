// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E88E0 (__tcf_0 0x1E8940, _GLOBAL__I 0x1E8970): <iostream>'s Init object only.
#include <iostream>

#include "BAPConOkToThrowGrenade.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
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

// ELF 0x1E7FF0 (C1; C2 0x1E7F90)
BAPConOkToThrowGrenade::BAPConOkToThrowGrenade(unsigned int object, BAPVariable<Vec3>* targetPoint,
                                               BAPVariable<Vec3>* throwDirection)
{
    m_object = object;
    m_targetPoint = targetPoint;
    m_throwDirection = throwDirection;
}

// ELF 0x1E8990 (D1; D0 0x1E89E0): the Aimer and the base only.
BAPConOkToThrowGrenade::~BAPConOkToThrowGrenade()
{
}

// ELF 0x1E8050. A soldier waits 10 s after a failed static attack; no friendly soldier may stand
// within 7 m of the target point (|dy| < 2.5). The throw is traced from the projectile origin to
// the apex (Aimer::getTurnPointForGrenade, only while the bot simulates artillery; otherwise three
// units along the throw direction), ignoring the bot's object; without a hit on that leg, from the
// apex to the target point, ignoring the target's object (no hit there: ok). A hit 3.5 m or more
// from the target point fails and stamps the failed static (sky) attack time. The bot's
// information and weapon are not null-checked; without the target information: false.
bool BAPConOkToThrowGrenade::evaluate(Bot* bot)
{
    BotMain* main = static_cast<BotMain*>(bot);
    Information* self = bapinline::info(bot->getCurrentEquipment());
    IPIArmament* armament = self->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    Information* target = bapinline::info(m_object);
    if (!target)
        return false;
    const double now = bapinline::gameTime();
    if (now - main->getTimeOfLastFailedStaticAttack() < 10.0 && self->isOfType(ITSoldier))
        return false;

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
        if (info && info->isOfType(ITSoldier)
            && std::fabs(info->getPosition().y - m_targetPoint->m_value.y) < 2.5f) {
            const Vec3 d = bapinline::sub(m_targetPoint->m_value, info->getPosition());
            if (d.x * d.x + d.y * d.y + d.z * d.z < 49.0f)
                return false;
        }
    }

    bot->getSensedObjectFromHandle(m_object);   // answer dropped (0x1E8340)
    const float drag = weapon->getDrag();
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = weapon->getExitVelocity();
    const float gravityModifier = weapon->getGravityModifier();
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);
    const Mat4& origin = weapon->getProjectileOrigin();

    float apexTime = 0.0f;
    Vec3 apex = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!main->getBotWantsToSimulateArtillery()
        || !m_aimer.getTurnPointForGrenade(m_throwDirection->m_value, apex, apexTime)) {
        const Vec3& d = m_throwDirection->m_value;
        apex = bapinline::vec3(d.x * 3.0f, d.y * 3.0f, d.z * 3.0f);
    } else {
        apex.y = static_cast<float>((static_cast<double>(weapon->getExitVelocity()) * m_throwDirection->m_value.y * apexTime
                                     + apex.y) * 0.5);
    }
    const Vec3 from = bapinline::vec3(origin.m[12], origin.m[13], origin.m[14]);
    apex = bapinline::vec3(origin.m[12] + apex.x, origin.m[13] + apex.y, origin.m[14] + apex.z);

    Vec3 hit = bapinline::vec3(0.0f, 0.0f, 0.0f);
    bool ok = true;
    if (bapinline::collideLineIgnoring(from, apex, self->getAIObject(), hit)) {
        ok = false;
    } else if (bapinline::collideLineIgnoring(apex, m_targetPoint->m_value, target->getAIObject(), hit)) {
        ok = false;
    }
    if (!ok) {
        const Vec3 miss = bapinline::sub(m_targetPoint->m_value, hit);
        if (12.25f <= miss.x * miss.x + miss.y * miss.y + miss.z * miss.z) {
            if (!main->getBotWantsToSimulateArtillery()) {
                main->setTimeOfLastFailedStaticAttack(now);
            } else {
                main->setTimeOfLastFailedStaticSkyAttack(now);
                main->setBotIsSimulatingArtillery(false);
            }
            return false;
        }
    }
    if (main->getBotWantsToSimulateArtillery() && !weapon->getIsThrown())
        main->setBotIsSimulatingArtillery(true);
    return true;
}

// ELF 0x1E88D0
BAPConditionType BAPConOkToThrowGrenade::getConditionType() const
{
    return BAPCTOkToThrowGrenade;   // 56
}

} // namespace ai
} // namespace hfe
} // namespace dice
