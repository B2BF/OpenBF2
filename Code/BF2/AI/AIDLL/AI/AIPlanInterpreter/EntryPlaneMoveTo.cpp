// EntryPlaneMoveTo (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2C0E80 (__tcf_0 0x2C0EE0, _GLOBAL__I 0x2C0F10): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneMoveTo.h"

#include "PlaneControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C0BE0 (C1; C2 0x2C0BC0).
EntryPlaneMoveTo::EntryPlaneMoveTo()
{
}

// ELF 0x2C0C20 (D1; D2 0x2C0C00, D0 0x2C0C40).
EntryPlaneMoveTo::~EntryPlaneMoveTo()
{
}

// ELF 0x2C0F30.
int EntryPlaneMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2C0C80.
bool EntryPlaneMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3d* const moveTo = static_cast<BAPAMoveTo3d*>(script);
    Mat4 unused = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    (void)unused;
    Vec3 point = {0.0f, 0.0f, 0.0f};
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        // shipped defect: the completed statement still drives the plane below.
    }
    else
        done = false;
    point = *moveTo->getFirstPoint();
    ok = PlaneControl::towardsPoint(bot, info, point, 1.0f, moveTo->m_forward,
                                   moveTo->m_right, moveTo->m_up, moveTo->m_distance, bot->getInput());
    const Mat4& transform = info->getTransformation();
    moveTo->m_forward = {transform.m[8], transform.m[9], transform.m[10]};
    moveTo->m_up = {transform.m[4], transform.m[5], transform.m[6]};
    moveTo->m_right = {transform.m[0], transform.m[1], transform.m[2]};
    moveTo->m_int4C = transform.m[13];
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
