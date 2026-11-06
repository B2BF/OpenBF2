// IPIMobileDummy. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2EEBA0..0x2EEFCE; the inline
// slots are in IPIMobileDummy.h.

#include "IPIMobileDummy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EEE80, __tcf_0 0x2EEEE0, _GLOBAL__I 0x2EEF10).
#include <iostream>

#include "Information.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

} // anonymous namespace

// ELF 0x2EEBA0 / 0x2EEC30: the speed is copied, the rotational speed zeroed, then the (empty)
// polygon update.
IPIMobileDummy::IPIMobileDummy(Information* information, Vec3& speed)
    : IPIMobile(information), m_speed(speed)
{
    m_rotationalSpeed.x = 0.0f;
    m_rotationalSpeed.y = 0.0f;
    m_rotationalSpeed.z = 0.0f;
    updateCurrentPolygon();
}

// ELF 0x2EECC0 / 0x2EECE0 / 0x2EED00: nothing of its own.
IPIMobileDummy::~IPIMobileDummy()
{
}

// ELF 0x2EED40: never a valid position; `position` is untouched.
bool IPIMobileDummy::getValidPosition(Vec3& /*position*/)
{
    return false;
}

// ELF 0x2EED50: the information's position (Information +12).
void IPIMobileDummy::getPositionEstimate(Vec3& position)
{
    position = m_information->getPosition();
}

// ELF 0x2EED90: the information's position, moved by the speed (through +7) over the time from
// now to `time`.
void IPIMobileDummy::getPositionEstimate(double time, Vec3& position)
{
    position = m_information->getPosition();
    const float dt = static_cast<float>(time - gameTime());
    const Vec3& speed = getSpeed();
    position.x += speed.x * dt;
    position.y += speed.y * dt;
    position.z += speed.z * dt;
}

// ELF 0x2EEE30: setTime(double) through the vtable with the game time.
void IPIMobileDummy::setTime()
{
    setTime(gameTime());
}

// ELF 0x2EEE70. Empty; the image keeps a 0x148-byte frame and no code (the body's locals were
// compiled out).
void IPIMobileDummy::updateCurrentPolygon()
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
