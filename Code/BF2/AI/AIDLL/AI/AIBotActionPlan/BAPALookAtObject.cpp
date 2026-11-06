// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D3580 (__tcf_0 0x1D35E0, _GLOBAL__I 0x1D3610): <iostream>'s Init object only.
#include <iostream>

#include "BAPALookAtObject.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D31A0 (C1; C2 0x1D3120)
BAPALookAtObject::BAPALookAtObject(Bot* bot, unsigned int target, float lookSpeed, BAPCondition* endCondition,
                                   unsigned long long controls, int tag, BAPVariable<Vec3>* targetPosition,
                                   float heightOffset)
    : BAPALook(lookSpeed, endCondition, controls, tag)
{
    m_bot = bot;
    m_target = target;
    m_targetPosition = targetPosition;
    m_heightOffset = heightOffset;
    m_endCondition->setOwner(this);
}

// ELF 0x1D3270 (D1; D2 0x1D3220, D0 0x1D32C0): deletes the end condition.
BAPALookAtObject::~BAPALookAtObject()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1D3320: from the camera position of the bot's current vehicle/soldier to the target
// (or the override variable's point), raised by m_heightOffset, normalised. No target
// information: ok = false and (1, 0, 0).
Vec3 BAPALookAtObject::getLookDirection(bool& ok)
{
    Information* self = bapinline::info(m_bot->getCurrentEquipment());
    Information* target = bapinline::info(m_target);
    ok = target != 0;
    if (!ok)
        return bapinline::vec3(1.0f, 0.0f, 0.0f);

    // The camera transform is fetched before the target point (0x1D33D9; the bot's equipment
    // information is not null-checked). Its row 3 (+0x30) is the camera position.
    const Mat4& camera = self->getControlInfo()->getCameraTransformation();
    Vec3 p;
    if (m_targetPosition)
        p = m_targetPosition->m_value;
    else
        p = target->getPosition();
    Vec3 dir = bapinline::vec3(p.x - camera.m[12], (m_heightOffset + p.y) - camera.m[13], p.z - camera.m[14]);
    bapinline::normalize(dir);
    return dir;
}

} // namespace ai
} // namespace hfe
} // namespace dice
