// Fixed-point line-of-fire condition recovered from libaidll.so.
// static init: ELF 0x1E7EE0 (__tcf_0 0x1E7F40, _GLOBAL__I 0x1E7F70): <iostream>'s Init object only.
#include <iostream>
#include "BAPConLineOfFire.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Core/Debug.h"
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E79B0 (C1; C2 0x1E7940)
BAPConLineOfFire::BAPConLineOfFire(const Vec3& target, bool flag)
    : m_target(target), m_useCamera(true), m_flag15(flag)
{
}

// ELF 0x1E7A90 (C1; C2 0x1E7A20)
BAPConLineOfFire::BAPConLineOfFire(const Vec3& target, bool flag, SoldierPose pose)
    : m_target(target), m_useCamera(false), m_flag15(flag), m_pose(pose)
{
}

// ELF 0x1E7B20 (D1; D2 0x1E7B00, D0 0x1E7B40)
BAPConLineOfFire::~BAPConLineOfFire()
{
}

// ELF 0x1E7B80
bool BAPConLineOfFire::evaluate(Bot* bot)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self)
        return false;
    Vec3 from = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (m_useCamera) {
        const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
        from = bapinline::vec3(camera.m[12], camera.m[13], camera.m[14]);
    } else {
        const Vec3 local = self->getAIObject()->getSoldierPoseCameraPosition(m_pose);
        const Mat4& transform = self->getTransformation();
        from.x = local.x * transform.m[0] + local.y * transform.m[4] + local.z * transform.m[8] + transform.m[12];
        from.y = local.x * transform.m[1] + local.y * transform.m[5] + local.z * transform.m[9] + transform.m[13];
        from.z = local.x * transform.m[2] + local.y * transform.m[6] + local.z * transform.m[10] + transform.m[14];
    }
    std::vector<IAIObject*> ignore;
    ignore.push_back(self->getAIObject());
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotActionPlan/BAPConLineOfFire.cpp", 58, "")
        << "Does not work with transparancy.";
    bool hit;
    return !static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->collideLineWithWorld(from, m_target, ignore, bot->getLodLevel() != 0, 0.75f, false, hit);
}

// ELF 0x1E7ED0
BAPConditionType BAPConLineOfFire::getConditionType() const
{
    return BAPCTLineOfFire;
}

} // namespace ai
} // namespace hfe
} // namespace dice
