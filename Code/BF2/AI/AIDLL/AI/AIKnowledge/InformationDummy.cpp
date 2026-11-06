// InformationDummy. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2D9520..0x2D9F3C; the
// inline slots are in InformationDummy.h.

#include "InformationDummy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2D9D20, __tcf_0 0x2D9D80, _GLOBAL__I 0x2D9DB0).
#include <iostream>

#include <cmath>

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAITerrain* terrain()
{
    return static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
}

} // anonymous namespace

// ELF 0x2D9520 / 0x2D9680. No owner, time -1. The transformation member is zeroed (the shipped
// Mat4 member construction) and then copied; the name is assigned after the side and types.
InformationDummy::InformationDummy(Vec3& position, Mat4& transformation, int side,
                                   unsigned int types)
    : Information(0, -1.0), m_position(position)
{
    for (int i = 0; i < 16; ++i)
        m_transformation.m[i] = 0.0f;
    m_transformation = transformation;
    m_objectTypeFlags = types;
    m_side = side;
    m_name = "DummyInformation";
}

// ELF 0x2D97E0 / 0x2D98E0. Zeroed, set to identity, then the three rows are overwritten with
// (0,0,1), (0,1,0), (1,0,0) and the translation with the position. The name stays empty.
InformationDummy::InformationDummy(Vec3& position, int side, unsigned int types)
    : Information(0, -1.0), m_position(position)
{
    for (int i = 0; i < 16; ++i)
        m_transformation.m[i] = 0.0f;
    m_transformation.m[0] = 1.0f;
    m_transformation.m[5] = 1.0f;
    m_transformation.m[10] = 1.0f;
    m_transformation.m[15] = 1.0f;

    m_transformation.m[0] = 0.0f;
    m_transformation.m[1] = 0.0f;
    m_transformation.m[2] = 1.0f;
    m_transformation.m[4] = 0.0f;
    m_transformation.m[5] = 1.0f;
    m_transformation.m[6] = 0.0f;
    m_transformation.m[8] = 1.0f;
    m_transformation.m[9] = 0.0f;
    m_transformation.m[10] = 0.0f;
    m_transformation.m[12] = m_position.x;
    m_transformation.m[13] = m_position.y;
    m_transformation.m[14] = m_position.z;
    m_objectTypeFlags = types;
    m_side = side;
}

// ELF 0x2D99E0 / 0x2D9A50 / 0x2D9AC0: the name, then the base (which destroys the plug-ins).
InformationDummy::~InformationDummy()
{
}

// ELF 0x2D9CE0: the terrain's altitude at the position (IAITerrain +0xC).
float InformationDummy::getAltitude()
{
    return terrain()->getAltitude(m_position);
}

// ELF 0x2D9B30. The lowest terrain altitude sampled along `direction` in sixth steps: the start,
// then from the second step on (the first step is added twice, as InformationReal does with its
// tenth steps). A direction no longer than one step samples only the start.
float InformationDummy::getAltitude(const Vec3& direction)
{
    Vec3 probe;
    probe.x = 0.0f;
    probe.y = 0.0f;
    probe.z = 0.0f;
    Vec3 step;
    step.x = 0.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    probe = m_position;
    float lowest = terrain()->getAltitude(probe);

    const float scale = 1.0f / 6.0f;
    step.x = direction.x * scale;
    step.y = direction.y * scale;
    step.z = direction.z * scale;
    const float stepLength = length / 6.0f;
    probe.x += step.x;
    probe.y += step.y;
    probe.z += step.z;
    float travelled = stepLength;
    while (length > travelled)
    {
        probe.x += step.x;
        probe.y += step.y;
        probe.z += step.z;
        const float altitude = terrain()->getAltitude(probe);
        travelled += stepLength;
        lowest = altitude > lowest ? lowest : altitude;
    }
    return lowest;
}

// ELF 0x2D9CA0: no secondary objects; the iterator is reset to -1.
unsigned int InformationDummy::getSecondaryObject(int& iterator, int /*side*/)
{
    iterator = -1;
    return 0xFFFFFFFFu;
}

// ELF 0x2D9CC0: as getSecondaryObject.
unsigned int InformationDummy::getFirstSecondaryObject(int& iterator, int /*side*/)
{
    iterator = -1;
    return 0xFFFFFFFFu;
}

} // namespace ai
} // namespace hfe
} // namespace dice
