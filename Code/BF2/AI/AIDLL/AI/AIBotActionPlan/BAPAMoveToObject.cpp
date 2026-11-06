// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DAA60 (__tcf_0 0x1DAAC0, _GLOBAL__I 0x1DAAF0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveToObject.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIKnowledge/Information.h"
#include "../Main/IBotManager.h"

#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DA690 (C1; C2 0x1DA5D0)
BAPAMoveToObject::BAPAMoveToObject(Bot* bot, unsigned int object, float distance, float maxSpeed,
                                   float maxArriveSpeed, BAPCondition* endCondition,
                                   unsigned long long controls, int patternId)
    : BAPAMoveTo(bot)
{
    m_object = object;
    m_distance = distance;
    m_distanceSqr = distance * distance;
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_controls = controls;
    m_patternId = patternId;
    m_endCondition = endCondition;
    if (endCondition)
        endCondition->setOwner(this);
}

// ELF 0x1DAB50 (D0 0x1DABB0, thunk 0x1DAC00): base only.
BAPAMoveToObject::~BAPAMoveToObject()
{
}

// ELF 0x1DA7D0: runs only through the move entries.
bool BAPAMoveToObject::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1DAB10
unsigned long long BAPAMoveToObject::getControls()
{
    return m_controls;
}

// ELF 0x1DAB20
BAPStatementType BAPAMoveToObject::getId() const
{
    return BAPSTMoveToObject;   // 9
}

// ELF 0x1DAB30
bool BAPAMoveToObject::hasDefault()
{
    return false;
}

// ELF 0x1DA9A0: the id is printed as a number here (this TU has no operator<< for the enum).
void BAPAMoveToObject::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    out << static_cast<int>(getId()) << "(" << static_cast<double>(m_distance) << ")" << std::endl;
}

// ELF 0x1DA750: end condition or within m_distance; then BotNavigation::clearProgress.
bool BAPAMoveToObject::isFinished(Bot* bot)
{
    if ((!m_endCondition || !m_endCondition->evaluate(bot)) && m_distanceSqr <= getSimpleTargetDistanceSqr(bot))
        return false;
    bot->getBotNavigation()->clearProgress();
    return true;
}

// ELF 0x1DAB40
unsigned int BAPAMoveToObject::getObject()
{
    return m_object;
}

// ELF 0x1DA7F0
float BAPAMoveToObject::getSimpleTargetDistance(Bot* bot)
{
    return std::sqrt(BAPAMoveToObject::getSimpleTargetDistanceSqr(bot));   // inline in the image (no virtual call)
}

// ELF 0x1DA8E0: neither information is null-checked.
float BAPAMoveToObject::getSimpleTargetDistanceSqr(Bot* bot)
{
    const Vec3& a = bapinline::info(bot->getCurrentEquipment())->getPosition();
    const Vec3& b = bapinline::info(m_object)->getPosition();
    const double dz = static_cast<double>(a.z) - b.z;
    const double dx = static_cast<double>(a.x) - b.x;
    const double dy = static_cast<double>(a.y) - b.y;
    return static_cast<float>(dz * dz + dx * dx + dy * dy);
}

} // namespace ai
} // namespace hfe
} // namespace dice
