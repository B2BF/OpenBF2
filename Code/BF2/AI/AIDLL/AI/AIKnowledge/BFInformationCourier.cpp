// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BFInformationCourier operations.

#include "BFInformationCourier.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2E6040, __tcf_0 0x2E60A0, _GLOBAL__I 0x2E60D0).
#include <iostream>

#include <cfloat>
#include <cmath>
#include <string>

#include "Information.h"
#include "InformationManager.h"
#include "IPIMobile.h"
#include "WeaponInfo.h"
#include "WeaponInfoManager.h"
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../../../../Core/ClassManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../AIInterface/BFKnowledge/InformationType.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

namespace {

InformationManager::Pool* pool()
{
    return informationManager->getPool();
}

} // anonymous namespace

// IAIInformationCourier
IAIInformationCourier::IAIInformationCourier()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iaiInformationCourier)
        classManager->registerSimpleSingleton("IAIInformationCourier", this);
}

IAIInformationCourier::~IAIInformationCourier()
{
    if (CommonSingletonServer::m_iaiInformationCourier == this)
        classManager->unregisterSimpleSingleton("IAIInformationCourier");
}

// sizeof == 0x44: no saved object, and three empty position
// records {-1, -1, (0, 0, 0)} - the same sixteen stores in both images.
BFInformationCourier::BFInformationCourier()
    : m_savedObject(0)
{
    for (int i = 0; i < 3; ++i)
    {
        m_saved[i].polygon = 0xFFFFFFFFu;
        m_saved[i].quadTree = 0xFFFFFFFFu;
        m_saved[i].position.x = 0.0f;
        m_saved[i].position.y = 0.0f;
        m_saved[i].position.z = 0.0f;
    }
}

// - the vptr reset and the base call, nothing else. The records own nothing.
BFInformationCourier::~BFInformationCourier()
{
}

// the information pool
// the record behind a handle, or null for a stale / zero one.
Information* BFInformationCourier::getInformation(unsigned int handle) const
{
    return informationManager->getInformation(handle);
}

// `getInformationSort != 0`, and a real
// record's sort is 0 (Known 1, Dummy 2): so this answers false for an InformationReal and true for a
// Known or Dummy one - the name reads backwards. Both images; the engine's one caller
// (AIObjectReal's side change) discards the answer.
bool BFInformationCourier::isRealInformation(unsigned int handle) const
{
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return false;
    return information->getInformationSort() != 0;                              // +2
}

// the slot is released first (generation bumped, index queued)
// then the record is destroyed - which takes its plug-ins with it.
void BFInformationCourier::freeInformation(unsigned int handle)
{
    Information* const information = informationManager->getInformation(handle);
    pool()->release(handle);
    if (information)
        delete information;
}

// the old record is destroyed, then the new one stored - when the
// handle is still valid. A stale handle destroys nothing and stores nothing, and the new record
// is then owned by nobody.
void BFInformationCourier::setInformation(unsigned int handle, Information* information)
{
    Information* const previous = informationManager->getInformation(handle);
    if (previous)
        delete previous;
    pool()->set(handle, information);
}

// a fresh handle, or 0 with the pool full.
unsigned int BFInformationCourier::registrateInformation(Information* information)
{
    return pool()->acquire(information);
}

// the weapon twins - WeaponInfoManager
// The records are destroyed through WeaponInfo's first slot (ELF at vptr+4, PE the scalar
// deleting destructor at vptr+0). Real records are now produced by InformationFactory; Known
// records remain a named frontier.
WeaponInfo* BFInformationCourier::getWeaponInfo(unsigned int handle) const
{
    return WeaponInfoManager::m_instance->getInformation(handle);
}

// released first, destroyed after, as above.
void BFInformationCourier::freeWeaponInfo(unsigned int handle)
{
    WeaponInfo* const weaponInfo = WeaponInfoManager::m_instance->getInformation(handle);
    WeaponInfoManager::m_instance->freeInformation(handle);
    if (weaponInfo)
        delete weaponInfo;
}

void BFInformationCourier::setWeaponInfo(unsigned int handle, WeaponInfo* weaponInfo)
{
    WeaponInfo* const previous = WeaponInfoManager::m_instance->getInformation(handle);
    if (previous)
        delete previous;
    WeaponInfoManager::m_instance->setInformation(handle, weaponInfo);
}

unsigned int BFInformationCourier::registrateWeaponInfo(WeaponInfo* weaponInfo)
{
    return WeaponInfoManager::m_instance->registerInformation(weaponInfo);
}

// armament sharing
// the second handle's record borrows the first's armament - both
// must resolve. (Information::borrowArmament shares the pointer; nothing is copied.)
void BFInformationCourier::borrowArmament(unsigned int lender, unsigned int borrower)
{
    Information* const lending = informationManager->getInformation(lender);
    Information* const borrowing = informationManager->getInformation(borrower);
    if (lending && borrowing)
        borrowing->borrowArmament(lending);
}

void BFInformationCourier::resetArmament(unsigned int handle)
{
    Information* const information = informationManager->getInformation(handle);
    if (information)
        information->resetArmament();
}

// Saves a mobile object's current navigation polygon for restoration after a side change.
void BFInformationCourier::saveCurrentPolygon(IAIObject* object)
{
    if (!object)
    {
        m_savedObject = 0;
        return;
    }
    int side = object->getSide();                                              // +0x12C
    if (!side)
        side = 1;
    Information* const information =
        informationManager->getInformation(object->getInformation(side));      // +0xB8
    if (!information)
    {
        m_savedObject = 0;
        return;
    }
    IPIMobile* const mobile = information->getMobile();
    if (!mobile)
    {
        m_savedObject = 0;
        return;
    }

    m_savedObject = object;
    m_saved[0] = mobile->m_lastValidInfantryPos;
    if (m_saved[0].polygon == static_cast<unsigned int>(-1))
    {
        m_savedObject = 0;
        return;
    }
    if (mobile->m_useFirstVehicleValidPos)
        m_saved[1] = mobile->m_lastValidVehiclePosA;
    else
        m_saved[1] = mobile->m_lastValidVehiclePosB;
    m_saved[2] = mobile->m_lastValidVehiclePosB;
}

// Restores a saved object's three
// records after a side change, or queries a new infantry polygon and, only when that succeeds, a
// vehicle polygon. Each successful record is published to the own-side mobile and the optional
// opposite-side mobile. The second query's map is the shipped Boolean `(traceMapIndex == 0)`.
void BFInformationCourier::updateCurrentPolygon(IAIObject* object)
{
    const Vec3& position = object->getPosition();
    if (object->isOfType(ITAir)
        && AerialHeightMap::m_instance->getAltitude(position) > 10.0f)
        return;

    int side = object->getSide();
    if (!side)
        side = 1;
    Information* const information =
        informationManager->getInformation(object->getInformation(side));
    Information* const oppositeInformation = informationManager->getInformation(
        object->getInformation((side == 1) + 1));

    if (object == m_savedObject)
    {
        if (information)
        {
            information->getMobile()->setLastValidPositions(m_saved[0], m_saved[1], m_saved[2]);
            if (oppositeInformation)
                oppositeInformation->getMobile()->setLastValidPositions(m_saved[0], m_saved[1],
                                                                         m_saved[2]);
        }
        return;
    }

    m_savedObject = 0;
    if (!information)
        return;
    if (std::fabs(0.0f - position.x) <= FLT_EPSILON
        && std::fabs(0.0f - position.y) <= FLT_EPSILON
        && std::fabs(0.0f - position.z) <= FLT_EPSILON)
        return;

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    IPIMobile* const mobile = information->getMobile();
    IPIMobile* const oppositeMobile =
        oppositeInformation ? oppositeInformation->getMobile() : 0;
    const int equipmentType = object->getUnit()->getEquipmentType();
    const int traceMapIndex = pathfinding->getTraceMapIndex();

    IPIMobile::ValidPos infantryPosition;
    infantryPosition.polygon = static_cast<unsigned int>(-1);
    infantryPosition.quadTree = static_cast<unsigned int>(-1);
    infantryPosition.position.x = 0.0f;
    infantryPosition.position.y = 0.0f;
    infantryPosition.position.z = 0.0f;
    if (pathfinding->getPolygon(
            position, traceMapIndex, infantryPosition.polygon, infantryPosition.quadTree,
            mobile->m_lastValidInfantryPos.polygon, mobile->m_lastValidInfantryPos.quadTree,
            equipmentType))
    {
        infantryPosition.position = position;
        mobile->setLastValidInfantryPos(infantryPosition);
        if (oppositeMobile)
            oppositeMobile->setLastValidInfantryPos(infantryPosition);

        const IPIMobile::ValidPos& previousVehiclePosition = mobile->m_useFirstVehicleValidPos
            ? mobile->m_lastValidVehiclePosA
            : mobile->m_lastValidVehiclePosB;
        IPIMobile::ValidPos vehiclePosition;
        vehiclePosition.polygon = static_cast<unsigned int>(-1);
        vehiclePosition.quadTree = static_cast<unsigned int>(-1);
        vehiclePosition.position.x = 0.0f;
        vehiclePosition.position.y = 0.0f;
        vehiclePosition.position.z = 0.0f;
        if (pathfinding->getPolygon(
                position, traceMapIndex == 0, vehiclePosition.polygon, vehiclePosition.quadTree,
                previousVehiclePosition.polygon, previousVehiclePosition.quadTree, equipmentType))
        {
            vehiclePosition.position = position;
            mobile->setLastValidVehiclePos(vehiclePosition);
            if (oppositeMobile)
                oppositeMobile->setLastValidVehiclePos(vehiclePosition);
        }
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
