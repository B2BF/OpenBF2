// EntryTrigger (attested TU `EntryTrigger.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier entry for BAPATrigger (statement type 11). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2C7270 (__tcf_0 0x2C72D0, _GLOBAL__I 0x2C7300): <iostream>'s Init object only.
#include <iostream>

#include "EntryTrigger.h"

#include "InfanteryControl.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

EntryTrigger::EntryTrigger()
{
}

EntryTrigger::~EntryTrigger()
{
}

int EntryTrigger::getTargetId()
{
    return 11;
}

namespace {

inline Information* lookup(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

} // namespace

// ELF 0x2C6900. Presses the statement's input once the start condition holds and releases it on the
// stop condition (or right away for a one-shot trigger: m_flag false finishes on the press). For
// weapon-select inputs it finishes as soon as that weapon is active. For PIFire/PIAltFire it first
// puts the soldier into the weapon's firing pose (crouching against a lying target closer than
// `range`) and tunes the weapon's bot skill: 1 for thrown or guided weapons, otherwise
// 1 - (1 - skill) * {0.5 direct fire, 0.8 explosive from a vehicle, 0.4 explosive on foot}, with
// direct fire at an occupied human vehicle left at the raw skill; the scale is 3 against an
// object flagged 0x10 by a secondary non-0x80 object (the image's flag tests on Information +4).
bool EntryTrigger::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    static float range = 1.0f;   // ELF 0x4BDAA4 (.data), compared with a squared XZ distance

    BAPATrigger* trigger = static_cast<BAPATrigger*>(script);
    BotMain* botMain = static_cast<BotMain*>(bot);
    int type = trigger->m_input;
    Information* info = lookup(bot->getCurrentEquipment());
    IPIArmament* armament = info->getArmament();
    WeaponInfo* weapon = 0;
    if (armament != 0 && armament->getActiveWeapon() != -1)
        weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));

    const io::PlayerInput* input = bot->getInput();
    bool moving = false;
    if (infantry_detail::inputValue(input, io::PIThrottle) != 0.0f
        || infantry_detail::inputValue(input, io::PIYaw) != 0.0f
        || infantry_detail::inputValue(input, io::PISprint) != 0.0f)
        moving = true;
    bool isFire = false;
    if (type == io::PIFire || type == io::PIAltFire)
        isFire = true;
    bool allowPress = true;
    if (isFire && botMain->botInPoseTransition())
        allowPress = false;

    if (weapon != 0)
    {
        if (static_cast<unsigned int>(type - io::PIWeaponSelect1) < 8)
        {
            if (weapon->getWeaponActivate() == type)
            {
                ok = true;
                done = true;
                return true;
            }
            if (trigger->m_started)
            {
                ok = true;
                done = false;
                return true;
            }
        }
        else if (weapon->getWeaponZoom() == type)
        {
            if (moving)
                return true;
        }
        else if (isFire)
        {
            if (!moving && weapon->getWeaponType() != 4)
            {
                SoldierPose pose = SPStanding;
                if (info->isOfType(ITSoldier))
                {
                    pose = weapon->getFiringPose();
                    if (armament->getActiveWeapon() == 0
                        || (bot->getFiringTarget() != static_cast<unsigned int>(-1) && weapon->getWeaponType() == 3))
                    {
                        Vec2 own;
                        own.x = own.y = 0.0f;
                        Vec2 other;
                        other.x = other.y = 0.0f;
                        vec3ToVec2XZ(info->getPosition(), own);
                        Information* target = lookup(bot->getFiringTarget());
                        if (target != 0)
                        {
                            vec3ToVec2XZ(target->getPosition(), other);
                            const float dx = other.x - own.x;
                            const float dy = other.y - own.y;
                            if (dy * dy + dx * dx < range && target->getSoldierPose() == SPLying)
                                pose = SPCrouching;
                        }
                    }
                }
                const SoldierPose current = info->getSoldierPose();
                if (current != SPSwimming && current != pose)
                    bot->requestSoldierPose(pose, false);
            }

            Information* target = lookup(bot->getFiringTarget());
            float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
            bool humanVehicle = false;
            if (target != 0 && target->getAIObject() != 0
                && IBotManager::instance->getBot(target->getAIObject()) == 0
                && target->getUnit() != 0 && target->getUnit()->isOccupied())
                humanVehicle = true;
            if (weapon->getWeaponZoom() != io::PINone && bot->getSpawnKit() == 1)
                weapon->getMaxRange();   // retail: the result is dropped
            if (!weapon->getIsThrown())
            {
                if (weapon->getExplosionRadius() == 0.0f)
                {
                    if (!humanVehicle)
                        skill = 1.0f - (1.0f - skill) * 0.5f;
                }
                else if (!info->isOfType(ITSoldier))
                {
                    skill = 1.0f - (1.0f - skill) * 0.8f;
                }
                else
                {
                    skill = 1.0f - (1.0f - skill) * 0.4f;
                }
            }
            else
            {
                skill = 1.0f;
            }
            if (weapon->getWeaponGuidanceType() != 0)
                skill = 1.0f;
            float scale = 1.0f;
            bool apply = true;
            if (!info->isOfType(ITAir))
            {
                if (target == 0)
                    apply = false;
                else if (target->isOfType(ITAir) && !info->isPrimary()
                         && !info->isOfType(ITNaval))
                    scale = 3.0f;
            }
            if (apply)
                weapon->setBotSkill(skill, bot->getFiringTargetTime(), scale);
        }
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now < trigger->m_endTime)
        return true;
    io::PlayerInput* writable = bot->getInput();
    if (trigger->m_stopCondition != 0 && trigger->m_stopCondition->evaluate(bot))
    {
        // inlined setInput in the image
        writable->setInput(static_cast<io::PlayerInputMap>(type), 0.0f);
        trigger->m_started = false;
        ok = true;
        done = true;
        return true;
    }
    if (!trigger->m_started || !trigger->m_flag)
    {
        if (allowPress && trigger->m_startCondition->evaluate(bot))
        {
            writable->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 1.0f);
            trigger->m_started = true;
            ok = true;
            done = !trigger->m_flag;
            return true;
        }
        writable->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 0.0f);
        trigger->m_started = false;
        ok = true;
        done = false;
        return true;
    }
    writable->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 0.0f);
    trigger->m_started = false;
    ok = true;
    done = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
