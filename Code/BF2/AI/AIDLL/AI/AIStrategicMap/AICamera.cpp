// Camera access for the AI. Evidence: libaidll.so (ia-32) 0x366B20..0x366C9F.

#include "AICamera.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x366BF0, __tcf_0 0x366C50, _GLOBAL__I 0x366C80).
#include <iostream>

#include "../../../../Game/Objects/ICameraObject.h"
#include "../../../../Game/Objects/IPlayer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x366B20. IPlayer::getCamera (+0x180), then ICameraObject::getTransformation(0.0f) (+0x10).
bool AICamera::GetCameraTrans(const world::IPlayer& player, Mat4& transform)
{
    world::ICameraObject* const camera = player.getCamera();
    if (!camera)
        return false;
    transform = camera->getTransformation(0.0f);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
