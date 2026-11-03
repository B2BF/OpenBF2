// Camera-frustum condition recovered from libaidll.so.
// static init: ELF 0x1EF010 (__tcf_0 0x1EF070, _GLOBAL__I 0x1EF0A0): <iostream>'s Init object only.
#include <iostream>
#include "BAPConObjectInsideFrustum.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EED40 (C1; C2 0x1EECE0)
BAPConObjectInsideFrustum::BAPConObjectInsideFrustum(unsigned int object, TransformType type)
    : m_transformType(type), m_frustum(), m_initialize(true)
{
    m_object = object;
}

// ELF 0x1EEDC0 (D1; D2 0x1EEDA0, D0 0x1EEDE0)
BAPConObjectInsideFrustum::~BAPConObjectInsideFrustum()
{
}

// ELF 0x1EEE20
bool BAPConObjectInsideFrustum::evaluate(Bot* bot)
{
    if (!bot)
        return false;
    Information* object = bapinline::info(m_object);
    if (!object)
        return false;
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self || m_transformType != 0)
        return false;
    if (m_initialize) {
        m_initialize = false;
        const float farDistance = bot->getPlayer()->getViewDistance();
        if (self->getObjectTypeFlags() & 0x400000u) {
            const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            m_frustum.setPerspective(1.570796371f, 1.0f, 0.01f, farDistance, camera);
        } else {
            const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            m_frustum.setPerspective(2.094395161f, 1.0f, 0.01f, farDistance, camera);
        }
    }
    IPIPhysical* physical = object->getPhysical();
    if (physical) {
        const float radius = physical->getRadius();
        return m_frustum.inside(object->getPosition(), radius);
    }
    return m_frustum.inside(object->getPosition(), 1.0f);
}

// ELF 0x1EF000
BAPConditionType BAPConObjectInsideFrustum::getConditionType() const
{
    return BAPCTObjectInsideFrustum;
}

} // namespace ai
} // namespace hfe
} // namespace dice
