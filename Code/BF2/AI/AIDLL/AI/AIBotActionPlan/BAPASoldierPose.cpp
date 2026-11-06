// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPASoldierPose.h"

#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {
// `+0x08 = 0; +0x0C = 0x60` in every constructor: (1 << PILie) | (1 << PICrouch).
const unsigned long long kSoldierPoseControls = 0x6000000000ULL;
}

// ELF 0x1E0740 (C1): one pose for all three cases, fire threshold 2, control threshold 0,
// no object, 5 s between changes; no break condition.
BAPASoldierPose::BAPASoldierPose(SoldierPose pose, BAPCondition* run, int tag, bool flag)
    : BAPAComponentSoldierPose(pose, pose, 2.0f, pose, 0.0f, 0xFFFFFFFFu, 5.0f)
{
    m_breakCondition = 0;
    m_runCondition = run;
    m_flag = flag;
    run->setOwner(this);
    m_controls = kSoldierPoseControls;
    m_patternId = tag;
}

// ELF 0x1E0900 (C1): as above with a followed object.
BAPASoldierPose::BAPASoldierPose(SoldierPose pose, BAPCondition* run, int tag, unsigned int object, bool flag)
    : BAPAComponentSoldierPose(pose, pose, 2.0f, pose, 0.0f, object, 5.0f)
{
    m_breakCondition = 0;
    m_runCondition = run;
    m_flag = flag;
    run->setOwner(this);
    m_controls = kSoldierPoseControls;
    m_patternId = tag;
}

// ELF 0x1E0AD0 (C1): run and break conditions (both owners set unconditionally).
BAPASoldierPose::BAPASoldierPose(SoldierPose pose, BAPCondition* run, BAPCondition* brk, int tag, bool flag)
    : BAPAComponentSoldierPose(pose, pose, 2.0f, pose, 0.0f, 0xFFFFFFFFu, 5.0f)
{
    m_runCondition = run;
    m_breakCondition = brk;
    m_flag = flag;
    run->setOwner(this);
    m_breakCondition->setOwner(this);
    m_controls = kSoldierPoseControls;
    m_patternId = tag;
}

// ELF 0x1E0CC0 (C1): the full chooser; the int fire threshold is converted to float.
BAPASoldierPose::BAPASoldierPose(SoldierPose normal, SoldierPose incommingFire, int fireObjectsThreshold,
                                 SoldierPose danger, float controlThreshold, BAPCondition* run,
                                 BAPCondition* brk, int tag, bool flag)
    : BAPAComponentSoldierPose(normal, incommingFire, static_cast<float>(fireObjectsThreshold), danger,
                               controlThreshold, 0xFFFFFFFFu, 5.0f)
{
    m_runCondition = run;
    m_breakCondition = brk;
    m_flag = flag;
    if (run)
        run->setOwner(this);
    if (m_breakCondition)
        m_breakCondition->setOwner(this);
    m_controls = kSoldierPoseControls;
    m_patternId = tag;
}

// ELF 0x1E0E50 (D1; D0 0x1E0EE0; thunk D0 0x1E1000 through the +0x14 base): deletes both conditions.
BAPASoldierPose::~BAPASoldierPose()
{
    if (m_runCondition)
        delete m_runCondition;
    if (m_breakCondition)
        delete m_breakCondition;
}

// ELF 0x1E0FA0: runs only through EntrySoldierPose.
bool BAPASoldierPose::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1E0F80
BAPStatementType BAPASoldierPose::getId() const
{
    return BAPSTSoldierPose;   // 26
}

// ELF 0x1E0F90
bool BAPASoldierPose::hasDefault()
{
    return false;
}

// ELF 0x1E0FC0
BAPCondition* BAPASoldierPose::getRunCondition()
{
    return m_runCondition;
}

// ELF 0x1E0FD0
BAPCondition* BAPASoldierPose::getBreakCondition()
{
    return m_breakCondition;
}

} // namespace ai
} // namespace hfe
} // namespace dice
