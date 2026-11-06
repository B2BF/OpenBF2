// IPIArmamentDummy. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2EC850..0x2ECD44; the
// inline slots are in IPIArmamentDummy.h.

#include "IPIArmamentDummy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2ECC70, __tcf_0 0x2ECCD0, _GLOBAL__I 0x2ECD00).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2EC850 / 0x2EC950. The base gets no information. The member is zeroed (the shipped Mat4
// member construction) and then copied.
IPIArmamentDummy::IPIArmamentDummy(Mat4& transformation)
    : IPIArmament(0)
{
    for (int i = 0; i < 16; ++i)
        m_transformation.m[i] = 0.0f;
    m_transformation = transformation;
}

// ELF 0x2ECA50 / 0x2ECAB0 / 0x2ECB10: nothing of its own; the base frees the weapon list.
IPIArmamentDummy::~IPIArmamentDummy()
{
}

// ELF 0x2ECB80: the transformation's third row; always true.
bool IPIArmamentDummy::getDirection(Bot* /*bot*/, Vec3& direction)
{
    direction.x = m_transformation.m[8];
    direction.y = m_transformation.m[9];
    direction.z = m_transformation.m[10];
    return true;
}

// ELF 0x2ECBB0. Despite the name it STORES the given matrix into the member (the argument is
// only read); always true. Shipped behaviour.
bool IPIArmamentDummy::getTransformation(Bot* /*bot*/, Mat4& transformation)
{
    m_transformation = transformation;
    return true;
}

// ELF 0x2ECC40: empty.
void IPIArmamentDummy::setTime()
{
}

// ELF 0x2ECC50: empty.
void IPIArmamentDummy::setTime(float /*time*/)
{
}

// ELF 0x2ECC60: never anti-aircraft.
int IPIArmamentDummy::isAntiAircraft() const
{
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
