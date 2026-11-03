
#include "InformationReal.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2DCB90, __tcf_0 0x2DCBF0, _GLOBAL__I 0x2DCC20).
#include <iostream>

#include <cmath>

#include "IPIPhysical.h"
#include "IPIUnit.h"
#include "InformationManager.h"
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Mat4.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

// The base's time is the given time minus twice the allowed
// time difference (exact in both images: the doubling of a float is exact). Then the prefix: the
// template's types, the allowed time difference, and the basic temperature twice (+0x10 and +0x14).
InformationReal::InformationReal(IAIObject* object, unsigned int types, float basicTemp,
                                 double time, float allowedTimeDiff)
    : Information(object, time - (allowedTimeDiff + allowedTimeDiff))
{
    m_allowedTimeDiff = allowedTimeDiff;
    m_strength = basicTemp;
    m_basicTemp = basicTemp;
    m_objectTypeFlags = types;
}

// the vptr and the base destructor - which destroys the plug-ins.
InformationReal::~InformationReal()
{
}

// a real information is sort 0 (see Information.h for what
// BFInformationCourier::isRealInformation makes of that).
int InformationReal::getInformationSort()
{
    return 0;
}

// A primary object first passes the time down to
// every secondary object's information on its own side (the owner's secondary list, each resolved
// through the pool); then a unit with a physical plug-in refreshes that plug-in and recomputes the
// unit's strength modifier from the health; then every plug-in gets the time. Always true.
bool InformationReal::setTime(double time)
{
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();                                             // +17
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->setTime(time);                                           // +3
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
    if (m_unit)
    {
        IPIPhysical* const physical = m_physical;
        if (physical)
        {
            physical->setTime(time);                                            // +4
            m_unit->calculateStrMod(physical->getHealth());                     // +5
        }
    }
    setTimeOnPlugIns(time);
    return true;
}

// the float is ignored; the virtual above answers.
bool InformationReal::setTime(double time, float /*f*/)
{
    return setTime(time);
}

// always "now".
double InformationReal::getTimeStamp() const
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

// - empty: the truth needs no contact bookkeeping.
void InformationReal::addContact()
{
}

void InformationReal::removeContact()
{
}

void InformationReal::removeAllContacts()
{
}

bool InformationReal::hasContact() const
{
    return true;
}

// full security, whenever.
float InformationReal::getSecurity(double /*time*/)
{
    return 1.0f;
}

float InformationReal::getSecurity()
{
    return 1.0f;
}

// the engine object's position (IAIObject +0xD4). [CHOSEN] cast - see the
// header.
Vec3& InformationReal::getPosition()
{
    return const_cast<Vec3&>(m_object->getPosition());
}

// the height above the ground (AerialHeightMap) at the object.
float InformationReal::getAltitude()
{
    return AerialHeightMap::m_instance->getAltitude(m_object->getPosition());
}

// Returns the minimum sampled height above ground along direction.
// Samples the start, then skips the first tenth-step; a zero direction samples only the start.
float InformationReal::getAltitude(const Vec3& direction)
{
    Vec3 probe;
    probe.x = 0.0f;
    probe.y = 0.0f;
    probe.z = 0.0f;
    const float length = static_cast<float>(std::sqrt(
        static_cast<double>(direction.x) * direction.x +
        static_cast<double>(direction.y) * direction.y +
        static_cast<double>(direction.z) * direction.z));
    probe = m_object->getPosition();
    float lowest = AerialHeightMap::m_instance->getAltitude(probe);

    const float stepX = static_cast<float>(direction.x * 0.1);
    const float stepY = static_cast<float>(direction.y * 0.1);
    const float stepZ = static_cast<float>(direction.z * 0.1);
    const double stepLength = length * 0.1;
    const float step = static_cast<float>(stepLength);
    float travelled = step;
    probe.x = static_cast<float>(static_cast<double>(stepX) + probe.x);
    probe.y = static_cast<float>(static_cast<double>(stepY) + probe.y);
    probe.z = static_cast<float>(static_cast<double>(stepZ) + probe.z);
    if (stepLength < length)
    {
        double next;
        do
        {
            probe.x = static_cast<float>(static_cast<double>(stepX) + probe.x);
            probe.y = static_cast<float>(static_cast<double>(stepY) + probe.y);
            probe.z = static_cast<float>(static_cast<double>(stepZ) + probe.z);
            const float altitude = AerialHeightMap::m_instance->getAltitude(probe);
            if (lowest >= altitude)
                lowest = altitude;
            next = static_cast<double>(travelled) + step;
            travelled = static_cast<float>(next);
        }
        while (next < length);
    }
    return lowest;
}

// the engine object's transformation (IAIObject +0xDC). [CHOSEN] cast.
Mat4& InformationReal::getTransformation()
{
    return const_cast<Mat4&>(m_object->getTransformation());
}

// the transformation's third row - the forward vector.
Vec3 InformationReal::getFaceing()
{
    const Mat4& transformation = m_object->getTransformation();
    Vec3 facing;
    facing.x = transformation.m[8];
    facing.y = transformation.m[9];
    facing.z = transformation.m[10];
    return facing;
}

// (IAIObject +0x12C).
int InformationReal::getSide() const
{
    return m_object->getSide();
}

} // namespace ai
} // namespace hfe
} // namespace dice
