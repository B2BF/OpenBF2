// Recovered from libaidll.so 1.5.3153 (ia-32); objdump listings with Ghidra cross-checks, ELF
// addresses on every body. Static init: ELF 0x1DFB40 (the <iostream> guard object only).

#include "BAPARunOverObject.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/CarControl.h"
#include "../Main/IAIPathfinding.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
// static init: ELF 0x1DFB40 (__tcf_0 0x1DFBA0, _GLOBAL__I 0x1DFBD0): <iostream>'s Init object only.
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DF470 (C1; C2 0x1DF410). BAPStatement() (controls 0), then the members.
BAPARunOverObject::BAPARunOverObject(unsigned int object, unsigned long long controls, int patternId)
{
    m_object = object;
    m_controls18 = controls;
    m_patternId = patternId;
}

// ELF 0x1DFBF0 (D1; D0 0x1DFC30): base only.
BAPARunOverObject::~BAPARunOverObject()
{
}

// ELF 0x1DF4D0 (listing; Ghidra cross-check). Without the bot's equipment it returns false and
// leaves ok = true, done = false. Without the target's information, or when IAIPathfinding +0x8C
// finds no clear line from the bot's last valid position to the target's (for the bot's vehicle
// number), it is done. Otherwise the intercept point is the target's position advanced by its
// velocity over the bot's travel time, refined once; the car steers at it with speed 50 through
// CarControl::controlTowardsDirection, the angle measured from the bot's normalised XZ facing.
// Shipped quirks kept: the target's mobile part is used unchecked, the bot's speed is not tested
// against zero, the trace's hit point is unused.
bool BAPARunOverObject::execute(Bot* bot, bool& ok, bool& done)
{
    ok = true;
    done = false;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return false;
    IPIMobile* const mobile = equipment->getMobile();
    IPIPhysical* const physical = equipment->getPhysical();
    Information* const target = informationManager->getInformation(m_object);
    if (!target)
    {
        done = true;
        return true;
    }
    IPIMobile* const targetMobile = target->getMobile();
    const IPIMobile::ValidPos& targetValid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
    const IPIMobile::ValidPos& ownValid = mobile->getLastValidPosition();
    Vec3 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    hit.z = 0.0f;
    unsigned int polygon;
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    if (!IAIPathfinding::instance->trace(ownValid.polygon, ownValid.quadTree, mobile->getVehicleNumber(),
                                         ownValid.position, targetValid.position, hit, polygon, equipmentType,
                                         -1, 0.0f, 5, 0))
    {
        done = true;
        return true;
    }

    const Vec3& own = equipment->getPosition();
    const Vec3& at = target->getPosition();
    const float dx = at.x - own.x;
    const float dy = at.y - own.y;
    const float dz = at.z - own.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float speed = mobile->getSpeedMagnitude();
    const float time = distance / speed;
    const Vec3& velocity = targetMobile->getSpeed();
    Vec3 predicted;
    predicted.x = at.x + time * velocity.x;
    predicted.y = at.y + time * velocity.y;
    predicted.z = at.z + time * velocity.z;
    const float px = predicted.x - own.x;
    const float py = predicted.y - own.y;
    const float pz = predicted.z - own.z;
    const float refinedTime = std::sqrt(pz * pz + px * px + py * py) / speed;
    const Vec3& velocity2 = targetMobile->getSpeed();
    Vec3 intercept;
    intercept.x = refinedTime * velocity2.x + at.x;
    intercept.y = refinedTime * velocity2.y + at.y;
    intercept.z = refinedTime * velocity2.z + at.z;
    Vec3 toIntercept;
    toIntercept.x = intercept.x - own.x;
    toIntercept.y = intercept.y - own.y;
    toIntercept.z = intercept.z - own.z;
    Vec2 direction;
    direction.x = 0.0f;
    direction.y = 0.0f;
    vec3ToVec2XZ(toIntercept, direction);

    Vec2 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    Vec2 facing;
    facing.x = 0.0f;
    facing.y = 0.0f;
    vec3ToVec2XZ(equipment->getFaceing(), facing);
    bapinline::normalize(facing);
    getNormal(facing, normal);
    const float angle = getSignedDiffAngleToDirWithNormal(facing, normal, direction);
    ok = CarControl::controlTowardsDirection(equipment, equipment->getControlInfo(), mobile, physical, bot->getInput(),
                                             50.0f, 1.0f, angle, 1.0f, 1.0f) != 0;
    return true;
}

// ELF 0x1DFB30
BAPStatementType BAPARunOverObject::getId() const
{
    return BAPSTRunOverObject;
}

// ELF 0x1DFB20
bool BAPARunOverObject::hasDefault()
{
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
