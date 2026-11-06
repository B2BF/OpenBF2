// EntryTriggerContinously (attested TU `EntryTriggerContinously.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier entry for BAPATriggerContinously (statement type 12). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2C7CD0 (__tcf_0 0x2C7D30, _GLOBAL__I 0x2C7D60): <iostream>'s Init object only.
#include <iostream>

#include "EntryTriggerContinously.h"

#include "InfanteryControl.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
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

EntryTriggerContinously::EntryTriggerContinously()
{
}

EntryTriggerContinously::~EntryTriggerContinously()
{
}

int EntryTriggerContinously::getTargetId()
{
    return 12;
}

namespace {

inline Information* lookup(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

} // namespace

// ELF 0x2C73F0. Holds the statement's input while the start condition holds (or, once started,
// until the stop condition holds) and finishes on the abort condition. For PIFire/PIAltFire it puts
// the soldier into the weapon's firing pose (not for kit 4's weapon 5 on foot) and tunes the
// weapon's bot skill as EntryTrigger does, without the thrown-weapon case. Fails (ok false, done)
// for a fire input when the bot has weapons but no active one.
bool EntryTriggerContinously::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPATriggerContinously* trigger = static_cast<BAPATriggerContinously*>(script);
    BotMain* botMain = static_cast<BotMain*>(bot);
    Information* info = lookup(bot->getCurrentEquipment());
    IPIArmament* armament = info->getArmament();
    WeaponInfo* weapon = 0;
    if (armament != 0 && armament->getActiveWeapon() != -1)
        weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));

    const int type = trigger->m_input;
    bool isFire = false;
    if (type == io::PIFire || type == io::PIAltFire)
        isFire = true;
    bool allowPress = true;
    if (isFire)
    {
        allowPress = !botMain->botInPoseTransition();
        if (weapon == 0)
        {
            if (armament != 0)
            {
                ok = false;
                done = true;
                return true;
            }
        }
        else
        {
            const io::PlayerInput* input = bot->getInput();
            if (infantry_detail::inputValue(input, io::PIThrottle) == 0.0f
                && infantry_detail::inputValue(input, io::PIYaw) == 0.0f
                && infantry_detail::inputValue(input, io::PISprint) == 0.0f
                && !(bot->getSpawnKit() == 4 && info->isOfType(ITSoldier)
                     && armament->getActiveWeapon() == 5))
            {
                SoldierPose pose = SPStanding;
                if (info->isOfType(ITSoldier))
                    pose = weapon->getFiringPose();
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
            weapon->getExplosionRadius();   // retail: called twice, the first result dropped
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
    if (trigger->m_abortCondition != 0 && trigger->m_abortCondition->evaluate(bot))
    {
        ok = true;
        done = true;
        return true;
    }
    io::PlayerInput* writable = bot->getInput();
    if (allowPress)
    {
        if (trigger->m_startCondition->evaluate(bot))
        {
            // inlined setInput in the image
            writable->setInput(static_cast<io::PlayerInputMap>(type), 1.0f);
            trigger->m_started = true;
            ok = true;
            done = false;
            return true;
        }
        if (trigger->m_stopCondition != 0 && trigger->m_started && !trigger->m_stopCondition->evaluate(bot))
        {
            writable->setInput(static_cast<io::PlayerInputMap>(type), 1.0f);
            ok = true;
            done = false;
            return true;
        }
    }
    // inlined setInput in the image
    writable->setInput(static_cast<io::PlayerInputMap>(type), 0.0f);
    trigger->m_started = false;
    ok = true;
    done = false;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
