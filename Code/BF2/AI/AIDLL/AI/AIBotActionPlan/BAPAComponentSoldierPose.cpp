// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D1BE0 (__tcf_0 0x1D1C40, _GLOBAL__I 0x1D1C70): <iostream>'s Init object only.
#include <iostream>

#include "BAPAComponentSoldierPose.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../Main/AIRandom.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"

#include <cfloat>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D1620 (C1; C2 0x1D1500 region). The fire threshold gets the inline Mersenne-Twister
// draw: `t5 = t * 5` is stored as float, then `rnd * 2^-32 * t5 + t` in x87 precision
// (double here) and stored as float.
BAPAComponentSoldierPose::BAPAComponentSoldierPose(SoldierPose normal, SoldierPose incommingFire,
                                                   float fireObjectsThreshold, SoldierPose danger,
                                                   float controlThreshold, unsigned int object,
                                                   float timeDuration)
{
    m_normalPose = normal;
    m_incommingFirePose = incommingFire;
    const float t5 = fireObjectsThreshold * 5.0f;
    const unsigned int r = AIRandom::m_instance->next();
    m_fireObjectsThreshold = static_cast<float>(static_cast<double>(r) * static_cast<double>(2.3283064e-10f) * t5
                                                + fireObjectsThreshold);
    m_dangerPose = danger;
    m_currentPose = normal;
    // The ordered comparisons leave a NaN unchanged.
    float c = controlThreshold;
    if (-1.0f > c)
        c = -1.0f;
    else if (c > 1.0f)
        c = 1.0f;
    m_controlThreshold = c;
    m_timeDuration = timeDuration;
    m_lastChangeTime = -static_cast<double>(FLT_MAX);
    m_object = object;
}

// ELF 0x1D1760 (D1; D0 0x1D1780)
BAPAComponentSoldierPose::~BAPAComponentSoldierPose()
{
}

// ELF 0x1D17C0
void BAPAComponentSoldierPose::setNormalPose(SoldierPose pose)
{
    m_normalPose = pose;
}

// ELF 0x1D17D0
void BAPAComponentSoldierPose::setIncommingFirePose(SoldierPose pose)
{
    m_incommingFirePose = pose;
}

// ELF 0x1D17E0
void BAPAComponentSoldierPose::setFireObjectsThreshold(float threshold)
{
    m_fireObjectsThreshold = threshold;
}

// ELF 0x1D17F0
void BAPAComponentSoldierPose::setDangerPose(SoldierPose pose)
{
    m_dangerPose = pose;
}

// ELF 0x1D1800
void BAPAComponentSoldierPose::setControlThreshold(float threshold)
{
    m_controlThreshold = threshold;
}

// ELF 0x1D1810
void BAPAComponentSoldierPose::setTimeDuration(float duration)
{
    m_timeDuration = duration;
}

// Inline in both 0x1D1820 and 0x1D1940: `info->m_objectTypeFlags & 0x200000` (byte +6 bit 5),
// `m_iaiPlayerFactory->getPlayerFromId(info->getPlayerId())` (+0x20), `player->getSoldier()`
// (+0x4C) non-null, and `info->getSoldierPose() != SPStanding`.
bool BAPAComponentSoldierPose::objectForcesCrouch() const
{
    if (m_object == 0xFFFFFFFFu)
        return false;
    Information* info = bapinline::info(m_object);
    if (!info || !(info->getObjectTypeFlags() & 0x200000u))
        return false;
    IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                            ->getPlayerFromId(info->getPlayerId());
    return player && player->getSoldier() && info->getSoldierPose() != SPStanding;
}

// ELF 0x1D1820
SoldierPose BAPAComponentSoldierPose::getNormalPose()
{
    if (objectForcesCrouch())
        return SPCrouching;
    return m_normalPose;
}

// ELF 0x1D18E0
SoldierPose BAPAComponentSoldierPose::getIncommingFirePose() const
{
    return m_incommingFirePose;
}

// ELF 0x1D18F0
float BAPAComponentSoldierPose::getFireObjectsThreshold() const
{
    return m_fireObjectsThreshold;
}

// ELF 0x1D1900
SoldierPose BAPAComponentSoldierPose::getDangerPose() const
{
    return m_dangerPose;
}

// ELF 0x1D1910
float BAPAComponentSoldierPose::getControlThreshold() const
{
    return m_controlThreshold;
}

// ELF 0x1D1920
float BAPAComponentSoldierPose::getTimeDuration() const
{
    return m_timeDuration;
}

// ELF 0x1D1930
SoldierPose BAPAComponentSoldierPose::getCurrentPose() const
{
    return m_currentPose;
}

// ELF 0x1D1940. Swimming wins; otherwise the pose is re-chosen at most every m_timeDuration:
// crouch next to a non-standing followed soldier; incoming fire above the threshold picks the
// incoming-fire pose, else control below the threshold picks the danger pose, else the normal
// pose; "lying" (3) needs kit 1 or 3, otherwise crouch. Then water: deeper than 0.4 stands,
// any water turns lying into crouching.
SoldierPose BAPAComponentSoldierPose::computeCurrentPose(float incommingFire, float control, double time,
                                                         Bot* bot, const Vec3& position)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self)
        return m_currentPose;
    if (self->getSoldierPose() == SPSwimming) {
        m_currentPose = SPSwimming;
        return SPSwimming;
    }
    if (time - m_lastChangeTime < static_cast<double>(m_timeDuration))
        return m_currentPose;
    m_lastChangeTime = time;
    if (objectForcesCrouch()) {
        m_currentPose = SPCrouching;
        return SPCrouching;
    }

    SoldierPose pose;
    if (incommingFire <= m_fireObjectsThreshold)
        pose = (m_controlThreshold <= control) ? m_normalPose : m_dangerPose;
    else
        pose = m_incommingFirePose;
    if (pose == SPLying && self->getKitType() != 1 && self->getKitType() != 3)
        pose = SPCrouching;
    m_currentPose = pose;

    IAITerrain* terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const float water = terrain->getWaterLevel(position.x, position.z);
    const float depth = water - terrain->getHeight(position.x, position.z);
    if (0.4f < depth) {
        m_currentPose = SPStanding;
        return SPStanding;
    }
    if (0.0f < depth && m_currentPose == SPLying) {
        m_currentPose = SPCrouching;
        return SPCrouching;
    }
    return m_currentPose;
}

} // namespace ai
} // namespace hfe
} // namespace dice
