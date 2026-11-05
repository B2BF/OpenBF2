// [INFERRED] Source placement.
#include "Information.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2D9460, __tcf_0 0x2D94C0, _GLOBAL__I 0x2D94F0).
#include <iostream>

#include "IPIArmament.h"
#include "IPIControlInfo.h"
#include "IPICover.h"
#include "IPIMobile.h"
#include "IPIPhysical.h"
#include "IPIUnit.h"
#include "../Main/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"   // IAIObject, IAIObjectMobile

namespace dice { namespace hfe { namespace ai {

// the time, the owner, not booked, no plug-in. The
// +0x04..+0x14 prefix is not touched here (the subclass constructors write it).
Information::Information(IAIObject* object, double time)
    : m_time(time), m_object(object), m_isBooked(false), m_bookingPlayerId(-1),
      m_armament(0), m_controlInfo(0), m_cover(0), m_mobile(0), m_physical(0), m_unit(0)
{
}

// Each present plug-in through its deleting destructor, in this order.
Information::~Information()
{
    if (m_physical)
        delete m_physical;
    if (m_cover)
        delete m_cover;
    if (m_mobile)
        delete m_mobile;
    if (m_armament)
        delete m_armament;
    if (m_unit)
        delete m_unit;
    if (m_controlInfo)
        delete m_controlInfo;
}

// the base answers 3.
int Information::getInformationSort()
{
    return 3;
}

// The plug-in of one type is detached and destroyed; true when
// there was one. An out-of-range type answers false without touching anything.
bool Information::removePlugIn(int type)
{
    InformationPlugIn* plugIn;
    switch (type)
    {
    case 0: plugIn = m_physical;    m_physical = 0;    break;
    case 1: plugIn = m_cover;       m_cover = 0;       break;
    case 2: plugIn = m_mobile;      m_mobile = 0;      break;
    case 3: plugIn = m_unit;        m_unit = 0;        break;
    case 4: plugIn = m_armament;    m_armament = 0;    break;
    case 5: plugIn = m_controlInfo; m_controlInfo = 0; break;
    default: return false;
    }
    if (!plugIn)
        return false;
    delete plugIn;
    return true;
}

// Every present plug-in is told the time
// control info, cover, mobile, physical, unit - but the armament only when the unit does not have to
// borrow armament and the object is not man-down (IPIPhysical +23), and it goes first.
void Information::setTimeOnPlugIns(double time)
{
    if (m_armament && (!m_unit || !m_unit->mustBorrowArmament()) &&
        (!m_physical || !m_physical->getManDown()))
        m_armament->setTime(time);
    if (m_controlInfo)
        m_controlInfo->setTime(time);
    if (m_cover)
        m_cover->setTime(time);
    if (m_mobile)
        m_mobile->setTime(time);
    if (m_physical)
        m_physical->setTime(time);
    if (m_unit)
        m_unit->setTime(time);
}

// The six setters - each : first wins - a plug-in already installed
// is kept and the new one is not taken (nor freed: the caller still owns it).
bool Information::setArmament(IPIArmament* armament)
{
    if (m_armament)
        return false;
    m_armament = armament;
    return true;
}

// A borrowed armament is shared, not copied: both records then hold the
// same pointer, and the one destroyed first frees it.
void Information::borrowArmament(Information* lender)
{
    m_armament = lender->m_armament;
}

void Information::resetArmament()
{
    m_armament = 0;
}

bool Information::setControlInfo(IPIControlInfo* controlInfo)
{
    if (m_controlInfo)
        return false;
    m_controlInfo = controlInfo;
    return true;
}

bool Information::setCover(IPICover* cover)
{
    if (m_cover)
        return false;
    m_cover = cover;
    return true;
}

bool Information::setMobile(IPIMobile* mobile)
{
    if (m_mobile)
        return false;
    m_mobile = mobile;
    return true;
}

bool Information::setPhysical(IPIPhysical* physical)
{
    if (m_physical)
        return false;
    m_physical = physical;
    return true;
}

bool Information::setUnit(IPIUnit* unit)
{
    if (m_unit)
        return false;
    m_unit = unit;
    return true;
}

// forwards to the owning engine object; none tests it
const std::string& Information::getObjectTypeName() const
{
    return m_object->getObjectTypeName();                                   // +0x58
}

unsigned int Information::getObjectId() const
{
    return m_object->getObjectId();                                         // +0xE4
}

int Information::getPlayerId() const
{
    return m_object->getPlayerId();                                         // +0x148
}

bool Information::isPrimary() const
{
    return m_object->isPrimary();                                           // +0xF8
}

// the primary object is asked for twice - once for the null test
// once for the call - and only the first answer is tested.
unsigned int Information::getPrimaryObject(int side)
{
    if (!m_object->getPrimaryObject())                                     // +0xFC
        return 0xFFFFFFFFu;
    return m_object->getPrimaryObject()->getInformation(side);              // +0xB8
}

unsigned int Information::getSecondaryObject(int& iterator, int side)
{
    IAIObject* const secondary = m_object->getSecondaryObject(iterator);    // +0x114
    if (!secondary)
        return 0xFFFFFFFFu;
    return secondary->getInformation(side);
}

unsigned int Information::getFirstSecondaryObject(int& iterator, int side)
{
    IAIObject* const secondary = m_object->getFirstSecondaryObject(iterator);   // +0x11C
    if (!secondary)
        return 0xFFFFFFFFu;
    return secondary->getInformation(side);
}

int Information::getFirstSecondaryObjectIterator() const
{
    return m_object->getFirstSecondaryObjectIterator();                     // +0x124
}

const std::string& Information::getName() const
{
    return m_object->getName();                                             // +0x50
}

const std::string& Information::getAIName() const
{
    return m_object->getAIName();                                           // +0x54
}

bool Information::getObjectSpawnerHolding() const
{
    return m_object->getObjectSpawnerHolding();                             // +0x138
}

unsigned int Information::getObjectSpawner(int side) const
{
    IAIObject* const spawner = m_object->getObjectSpawner();                // +0x13C
    if (!spawner)
        return 0xFFFFFFFFu;
    return spawner->getInformation(side);
}

// This record's own pool handle: the owner's information for
// sides 1 and 2, whichever resolves to this record, else -1. Side 0 (neutral) is never asked, so
// a neutral object's record answers -1.
unsigned int Information::getHandle() const
{
    int side = 1;
    for (;;)
    {
        const unsigned int handle = m_object->getInformation(side);
        if (informationManager->getInformation(handle) == this)
            return handle;
        if (++side > 2)
            return 0xFFFFFFFFu;
    }
}

int Information::getKitType() const
{
    return m_object->getKitType();                                          // +0xF4
}

bool Information::isRootObject() const
{
    return m_object->isRootObject();                                        // +0x140
}

SoldierPose Information::getSoldierPose() const
{
    return m_object->getSoldierPose();                                      // +0xEC
}

// vehicle booking
int Information::getBookingPlayerId()
{
    return m_bookingPlayerId;
}

bool Information::isBooked()
{
    return m_isBooked;
}

// Compares the id alone - a cancelled booking (-1) is "booked by" player -1.
bool Information::isBookedBy(int playerId)
{
    return m_bookingPlayerId == playerId;
}

void Information::bookVehicle(int playerId)
{
    m_isBooked = true;
    m_bookingPlayerId = playerId;
}

void Information::cancelBookVehicle()
{
    m_isBooked = false;
    m_bookingPlayerId = -1;
}

} } }
