// InformationKnown. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2DB4B0..0x2DC5F5
// (Ghidra + 0x10000); vtable 0x4BE100 has the same 18 slots as InformationReal's.

#include "InformationKnown.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2DC4D0, __tcf_0 0x2DC530, _GLOBAL__I 0x2DC560).
#include <iostream>

#include <cmath>

#include "BFInformationEnvironment.h"                                   // IAIInformationEnvironmentModifiers
#include "IPIMobile.h"
#include "IPIPhysical.h"
#include "IPIUnit.h"
#include "../Main/InformationManager.h"
#include "../AIUtils/CustomCurve.h"                              // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAITerrain* terrain()
{
    return static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
}

double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

} // anonymous namespace

// 0x2DB4B0. The base time is -1 (never updated); the last update is the given time. The cached
// position starts at zero and the transformation at identity, then both take the object's
// current values; the object's side is cached, contact count 0.
InformationKnown::InformationKnown(IAIObject* object, unsigned int types, double time,
                                   float degeneration, float basicTemp, float allowedTimeDiff,
                                   int ownerSide)
    : Information(object, -1.0)
{
    m_lastUpdate = time;
    m_degeneration = degeneration;
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
    for (int i = 0; i < 16; ++i)
        m_lastTransformation.m[i] = 0.0f;
    m_lastTransformation.m[0] = 1.0f;
    m_lastTransformation.m[5] = 1.0f;
    m_lastTransformation.m[10] = 1.0f;
    m_lastTransformation.m[15] = 1.0f;
    m_ownerSide = ownerSide;
    m_contacts = 0;
    m_allowedTimeDiff = allowedTimeDiff;
    m_strength = basicTemp;
    m_basicTemp = basicTemp;
    m_objectTypeFlags = types;
    m_side = m_object->getSide();                                           // +0x12C
    storeLastSeen();
}

// 0x2DB7F0 / 0x2DB870: the base destroys the plug-ins.
InformationKnown::~InformationKnown()
{
}

void InformationKnown::storeLastSeen()
{
    m_lastPosition = m_object->getPosition();                               // +0xD4
    m_lastTransformation = m_object->getTransformation();                   // +0xDC
}

// 0x2DC580: a known information is sort 1.
int InformationKnown::getInformationSort()
{
    return 1;
}

// 0x2DB9B0: setTime(double) through the vtable with the game time.
bool InformationKnown::setTime()
{
    return setTime(gameTime());                                             // +3
}

// 0x2DBA00. Updates are throttled: unless the time is past the last full update plus the
// allowed difference, only the timestamp moves and the answer is false. Otherwise: without
// contact the last-seen state is refreshed and the mobile plug-in is told contact was lost;
// the mobile plug-in gets the time; a primary object passes the time to its secondary
// objects' information (looked up for this record's side); the side is re-read; a unit
// recomputes its strength from the physical plug-in's health (the physical plug-in is not
// null-tested here, unlike InformationReal); all plug-ins get the time; then a primary object
// re-registers its position with the information environment for the owning side.
bool InformationKnown::setTime(double time)
{
    if (!(time > m_time + m_allowedTimeDiff))
    {
        m_lastUpdate = time;
        return false;
    }
    if (!m_contacts)
    {
        storeLastSeen();
        if (m_mobile)
            m_mobile->setLostContact();                                     // +6
    }
    if (m_mobile)
        m_mobile->setTime(time);                                            // +4
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();                                         // +17
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->setTime(time);                                       // +3
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
    m_side = m_object->getSide();                                           // +0x12C
    if (m_unit)
    {
        m_physical->setTime(time);                                          // +4
        m_unit->calculateStrMod(m_physical->getHealth());                   // +5
    }
    setTimeOnPlugIns(time);
    m_time = time;
    if (isPrimary())
    {
        const unsigned int handle = m_object->getInformation(m_ownerSide);  // +0xB8
        static_cast<IAIInformationEnvironmentModifiers*>(
            CommonSingletonServer::m_iaiInformationEnvironmentModifiers)
            ->addAndUpdatePosition(m_ownerSide, m_unit != 0, handle);      // +0xC
    }
    m_lastUpdate = time;
    return true;
}

// 0x2DBD20. Raises the record's security to `security` (clamped to [0,1]) if it is lower:
// a full update first, then the timestamp is backdated so getSecurity answers about that
// value, and setTime(double) runs again with the base time reset (its answer is returned).
// Retail backdates by 1 / (security * degeneration), not by the SCurve inverse. A clamped
// security of exactly zero cannot pass the test (security is never below 0) - the branch that
// stores 0.0 as the timestamp is kept as written.
bool InformationKnown::setTime(double time, float security)
{
    float target = 0.0f;
    if (!(0.0f > security))
    {
        target = security;
        if (security > 1.0f)
            target = 1.0f;
    }
    if (!(getSecurity() < target))                                          // +11
        return false;
    setTime(time);                                                          // +3
    if (target == 0.0f)
    {
        m_lastUpdate = target;
        return true;
    }
    const double backdated = time - 1.0 / (static_cast<double>(target) * m_degeneration);
    m_time = -1.0;
    return setTime(backdated);                                              // +3
}

// 0x2DC5D0.
double InformationKnown::getTimeStamp() const
{
    return m_lastUpdate;
}

// 0x2DBDE0. Counts the contact, tells the mobile plug-in, and passes the contact on to a
// primary object's secondary objects.
void InformationKnown::addContact()
{
    ++m_contacts;
    if (m_mobile)
        m_mobile->setHasContact();                                          // +5
    if (!isPrimary())
        return;
    int iterator = -1;
    const int side = getSide();                                             // +17
    Information* secondary =
        informationManager->getInformation(getFirstSecondaryObject(iterator, side));
    while (secondary)
    {
        secondary->addContact();                                            // +6
        secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
    }
}

// 0x2DBEF0. When the last contact goes, the last-seen state is frozen and the mobile plug-in
// told; a primary object passes the removal on to its secondary objects either way.
void InformationKnown::removeContact()
{
    if (m_contacts > 0)
    {
        if (--m_contacts == 0)
        {
            storeLastSeen();
            if (m_mobile)
                m_mobile->setLostContact();                                 // +6
        }
    }
    if (!isPrimary())
        return;
    int iterator = -1;
    const int side = getSide();                                             // +17
    Information* secondary =
        informationManager->getInformation(getFirstSecondaryObject(iterator, side));
    while (secondary)
    {
        secondary->removeContact();                                         // +7
        secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
    }
}

// 0x2DC0D0. As removeContact, dropping every contact at once.
void InformationKnown::removeAllContacts()
{
    if (m_contacts > 0)
    {
        m_contacts = 0;
        storeLastSeen();
        if (m_mobile)
            m_mobile->setLostContact();                                     // +6
    }
    if (!isPrimary())
        return;
    int iterator = -1;
    const int side = getSide();                                             // +17
    Information* secondary =
        informationManager->getInformation(getFirstSecondaryObject(iterator, side));
    while (secondary)
    {
        secondary->removeAllContacts();                                     // +8
        secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
    }
}

// 0x2DC5E0.
bool InformationKnown::hasContact() const
{
    return m_contacts > 0;
}

// 0x2DB970: one minus the SCurve of the age over the degeneration.
float InformationKnown::getSecurity(double time)
{
    const float age = static_cast<float>(time - m_lastUpdate);
    return 1.0f - SCurve::calculate(age / m_degeneration);
}

// 0x2DB910: full while in contact, else the decay at the current game time.
float InformationKnown::getSecurity()
{
    if (m_contacts > 0)
        return 1.0f;
    return getSecurity(gameTime());                                         // +10
}

// 0x2DB8B0: live while in contact, else the last seen position.
Vec3& InformationKnown::getPosition()
{
    if (m_contacts)
        return const_cast<Vec3&>(m_object->getPosition());                  // +0xD4
    return m_lastPosition;
}

// 0x2DC470: the terrain's altitude (IAITerrain +0xC) at getPosition's answer, inlined.
float InformationKnown::getAltitude()
{
    if (m_contacts)
        return terrain()->getAltitude(m_object->getPosition());
    return terrain()->getAltitude(m_lastPosition);
}

// 0x2DC2B0. The lowest terrain altitude along `direction` from the current position, in
// tenth steps; like InformationReal's the first tenth step is skipped, but the samples come
// from IAITerrain::getAltitude, not the aerial height map.
float InformationKnown::getAltitude(const Vec3& direction)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    Vec3 probe;
    if (m_contacts)
        probe = m_object->getPosition();
    else
        probe = m_lastPosition;
    float lowest = terrain()->getAltitude(probe);

    const double tenth = 1.0f / 10.0f;
    const float stepX = static_cast<float>(direction.x * tenth);
    const float stepY = static_cast<float>(direction.y * tenth);
    const float stepZ = static_cast<float>(direction.z * tenth);
    const double firstStep = length / 10.0f;
    const float step = static_cast<float>(firstStep);
    float travelled = step;
    probe.x = static_cast<float>(direction.x * tenth + probe.x);
    probe.y = static_cast<float>(direction.y * tenth + probe.y);
    probe.z = static_cast<float>(direction.z * tenth + probe.z);
    if (length > firstStep)
    {
        double next;
        do
        {
            probe.x = stepX + probe.x;
            probe.y = stepY + probe.y;
            probe.z = stepZ + probe.z;
            const float altitude = terrain()->getAltitude(probe);
            if (!(altitude > lowest))
                lowest = altitude;
            next = static_cast<double>(travelled) + step;
            travelled = static_cast<float>(next);
        }
        while (length > next);
    }
    return lowest;
}

// 0x2DB8E0: live while in contact, else the last seen transformation.
Mat4& InformationKnown::getTransformation()
{
    if (m_contacts)
        return const_cast<Mat4&>(m_object->getTransformation());            // +0xDC
    return m_lastTransformation;
}

// 0x2DC590: the stored transformation's third row, even while in contact.
Vec3 InformationKnown::getFaceing()
{
    Vec3 facing;
    facing.x = m_lastTransformation.m[8];
    facing.y = m_lastTransformation.m[9];
    facing.z = m_lastTransformation.m[10];
    return facing;
}

// 0x2DC5C0.
int InformationKnown::getSide() const
{
    return m_side;
}

// 0x2DC440 / 0x2DC450.
int InformationKnown::getOwnerSide()
{
    return m_ownerSide;
}

void InformationKnown::setOwnerSide(int side)
{
    m_ownerSide = side;
}

} // namespace ai
} // namespace hfe
} // namespace dice
