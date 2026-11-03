// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F0D70 (__tcf_0 0x1F0DD0, _GLOBAL__I 0x1F0E00): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectValidAiming.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F0510 (C1; C2 0x1F0480): the bot's own equipment shoots.
BAPConObjectValidAiming::BAPConObjectValidAiming(unsigned int object, BAPVariable<bool>* aimValid,
                                                 BAPVariable<Vec3>* aimDirection, bool trackStaticAttack)
{
    m_shooter = 0xFFFFFFFFu;
    m_object = object;
    m_aimValid = aimValid;
    m_aimDirection = aimDirection;
    m_trackStaticAttack = trackStaticAttack;
}

// ELF 0x1F05F0 (C1; C2 0x1F0580)
BAPConObjectValidAiming::BAPConObjectValidAiming(unsigned int object, unsigned int shooter, BAPVariable<bool>* aimValid,
                                                 BAPVariable<Vec3>* aimDirection, bool trackStaticAttack)
{
    m_object = object;
    m_shooter = shooter;
    m_aimValid = aimValid;
    m_aimDirection = aimDirection;
    m_trackStaticAttack = trackStaticAttack;
}

// ELF 0x1F0E20 (D1; D0 0x1F0E70): the Aimer and the base only.
BAPConObjectValidAiming::~BAPConObjectValidAiming()
{
}

// ELF 0x1F0660. The aim must be solvable (internalAiming) and the camera must be able to turn to
// the direction (IPIControlInfo::validateCameraDirection on m_shooter's, or the bot's, control
// info; not null-checked). Retail quirk: with the Aimer path internalAiming leaves `dir` at zero,
// so the camera check sees (0, 0, 0). With m_trackStaticAttack a failed check stamps the failed
// static attack time and clears both artillery flags (unless the aim failed too and the last
// failed sky attack is at most 2.5 s old: false without stamping), or, while the bot wants to
// simulate artillery, the failed sky attack time.
bool BAPConObjectValidAiming::evaluate(Bot* bot)
{
    Vec3 dir = bapinline::vec3(0.0f, 0.0f, 0.0f);
    const bool aimed = internalAiming(bot, dir);
    bool valid = false;
    if (aimed) {
        const unsigned int handle = m_shooter == 0xFFFFFFFFu ? bot->getCurrentEquipment() : m_shooter;
        valid = bapinline::info(handle)->getControlInfo()->validateCameraDirection(dir);
    }
    if (m_trackStaticAttack) {
        BotMain* main = static_cast<BotMain*>(bot);
        const double now = bapinline::gameTime();
        if (!main->getBotWantsToSimulateArtillery()) {
            if (!valid) {
                if (!aimed && now - main->getTimeOfLastFailedStaticSkyAttack() <= 2.5)
                    return false;
                main->setTimeOfLastFailedStaticAttack(now);
                main->setBotWantsToSimulateArtillery(false);
                main->setBotIsSimulatingArtillery(false);
            }
        } else if (!valid) {
            main->setTimeOfLastFailedStaticSkyAttack(now);
        }
    }
    return valid;
}

// ELF 0x1F07E0. With both variables: dir = the direction variable, answer = the valid variable.
// Otherwise solves the Aimer for the active weapon of the bot's current vehicle/soldier against
// the target (its position when not sensed or sensed indirectly (flag 8), else the sensed hit
// point), relative to the weapon's absolute transformation, 0.1 s ahead; `dir` is not written
// (the solution goes to a local). No information, control info, armament, weapon or target: false.
bool BAPConObjectValidAiming::internalAiming(Bot* bot, Vec3& dir)
{
    if (m_aimValid && m_aimDirection) {
        dir = m_aimDirection->m_value;
        return m_aimValid->m_value;
    }
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self || !self->getControlInfo() || !self->getArmament())
        return false;
    IPIArmament* armament = self->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    Information* target = bapinline::info(m_object);
    if (!target)
        return false;
    IPIMobile* ownMobile = bapinline::info(self->getPrimaryObject(bot->getSide()))->getMobile();   // not null-checked

    const float drag = weapon->getDrag();
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = weapon->getExitVelocity();
    const float gravityModifier = weapon->getGravityModifier();
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);

    SensedObject* sensed = bot->getSensedObjectFromHandle(m_object);
    Vec3 point;
    if (!sensed || (sensed->m_flags & 8u)) {
        point = target->getPosition();
    } else {
        const Mat4& t = target->getTransformation();
        const Vec3& l = sensed->m_seenPosition;
        point.x = t.m[8] * l.z + t.m[0] * l.x + t.m[4] * l.y + t.m[12];
        point.y = t.m[1] * l.x + t.m[5] * l.y + t.m[9] * l.z + t.m[13];
        point.z = l.x * t.m[2] + l.y * t.m[6] + l.z * t.m[10] + t.m[14];
    }
    IPIMobile* targetMobile = bapinline::info(target->getPrimaryObject(bot->getSide()))->getMobile();   // not null-checked
    const Mat4& weaponTransform = weapon->getAbsoluteTransformation();
    const Vec3 rel = bapinline::vec3(point.x - weaponTransform.m[12], point.y - weaponTransform.m[13],
                                     point.z - weaponTransform.m[14]);
    Vec3 targetVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    Vec3 ownVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (ownMobile)
        ownVelocity = ownMobile->getSpeed();
    if (targetMobile)
        targetVelocity = targetMobile->getSpeed();
    m_aimer.setTargetData(rel, targetVelocity, ownVelocity, &weapon->getAbsoluteTransformation(), bapinline::gameTime());
    Vec3 solution = bapinline::vec3(0.0f, 0.0f, 0.0f);
    Vec3 solutionTarget = bapinline::vec3(0.0f, 0.0f, 0.0f);
    const bool indirect = weapon->getIndirect();
    return m_aimer.getFiringDirection(bapinline::gameTime() + 0.1, 0.5f, indirect, solution, solutionTarget, 0);
}

// ELF 0x1F0D60
BAPConditionType BAPConObjectValidAiming::getConditionType() const
{
    return BAPCTObjectValidAiming;   // 32
}

} // namespace ai
} // namespace hfe
} // namespace dice
