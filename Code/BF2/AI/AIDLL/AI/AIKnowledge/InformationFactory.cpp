// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining InformationFactory operations.

#include "InformationFactory.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2DB350, __tcf_0 0x2DB3B0, _GLOBAL__I 0x2DB3E0).
#include <iostream>

#include <string>

#include "Information.h"
#include "../Main/InformationManager.h"
#include "InformationKnown.h"
#include "InformationReal.h"
#include "IPIArmamentKnown.h"
#include "IPIArmamentReal.h"
#include "IPIControlInfo.h"
#include "IPICover.h"
#include "IPIMobileKnown.h"
#include "IPIMobileReal.h"
#include "IPIPhysicalKnown.h"
#include "IPIPhysicalReal.h"
#include "IPIUnit.h"
#include "WeaponInfoKnown.h"
#include "WeaponInfoReal.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Core/ClassManager.h"
#include "../../../../Core/Ids.h"                       // IID_IBase
#include "../Main/CommonSingletonServer.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IInformationFactory
IInformationFactory::IInformationFactory()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iInformationFactory)
        classManager->registerSimpleSingleton("IInformationFactory", this);
}

IInformationFactory::~IInformationFactory()
{
    if (CommonSingletonServer::m_iInformationFactory == this)
        classManager->unregisterSimpleSingleton("IInformationFactory");
}

// Initializes the reference-counted information factory.
InformationFactory::InformationFactory()
{
}

InformationFactory::~InformationFactory()
{
}

// IBase
void InformationFactory::addRef()
{
    ++m_refCount;
}

int InformationFactory::getRef()
{
    return m_refCount;
}

// the decremented count is stored and returned; at zero the object
// deletes itself first (ELF at +0x14, PE the scalar deleting destructor at +0x10).
int InformationFactory::release()
{
    const int refCount = --m_refCount;
    if (!refCount)
        delete this;
    return refCount;
}

void* InformationFactory::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInformationFactory)
        return const_cast<InformationFactory*>(this);
    return 0;
}

// the information requests
// A fresh InformationReal (ELF
// PE ) registered under `handle` - , as every engine caller passes, means "a new
// handle"; see registerInformation for the rest. The registration goes through the vtable (ELF
// +0x38, PE +0x34), which the unqualified call reproduces.
unsigned int InformationFactory::requestInformationReal(unsigned int handle, IAIObject* object,
                                                        unsigned int types, float basicTemp,
                                                        double time, float allowedTimeDiff) const
{
    return registerInformation(handle, checkPointer(new InformationReal(object, types, basicTemp,
                                                                        time, allowedTimeDiff)));
}

// The same with an InformationKnown
// (ELF 0x2DA1B0: `(object, types, time, degeneration, basicTemp, allowedTimeDiff, ownerSide)`
// passed through in order), registered through the same vtable slot.
unsigned int InformationFactory::requestInformationKnown(unsigned int handle, IAIObject* object,
                                                         unsigned int types, double time,
                                                         float degeneration, float basicTemp,
                                                         float allowedTimeDiff, int ownerSide) const
{
    return registerInformation(handle, checkPointer(new InformationKnown(
                                           object, types, time, degeneration, basicTemp,
                                           allowedTimeDiff, ownerSide)));
}

// the plug-in halves
// the 4-argument slot with the handle twice, through the vtable
// (ELF +0x24, PE +0x1C).
bool InformationFactory::requestInformationPlugin(unsigned int handle, int type, bool real) const
{
    return requestInformationPlugin(handle, handle, type, real);
}

// Creates and attaches the requested information plug-in to the source record.
// Returns whether creation succeeded; unsupported concrete types stop by name.
bool InformationFactory::requestInformationPlugin(unsigned int sourceHandle,
                                                  unsigned int targetHandle, int type,
                                                  bool real) const
{
    Information* const source = informationManager->getInformation(sourceHandle);
    Information* const target = informationManager->getInformation(targetHandle);
    switch (type)
    {
    case 0:     // armament - IPIArmamentReal (ELF / PE ) or IPIArmamentKnown (32 / 36)
        if (real)
        {
            target->setArmament(checkPointer(new IPIArmamentReal(source)));
            return true;
        }
        target->setArmament(checkPointer(new IPIArmamentKnown(source)));
        return true;

    case 1:     // control info - , built inline in both images, the same for real and known
        target->setControlInfo(checkPointer(new IPIControlInfo(source)));
        return true;

    case 2:     // cover - IPICover , the same for real and known
        target->setCover(checkPointer(new IPICover(source)));
        return true;

    case 3:     // mobile - IPIMobileReal or IPIMobileKnown (ELF / PE )
        if (real)
            target->setMobile(checkPointer(new IPIMobileReal(source)));
        else
            target->setMobile(checkPointer(new IPIMobileKnown(source)));
        return true;

    case 4:     // physical - IPIPhysicalReal or IPIPhysicalKnown
        if (real)
        {
            target->setPhysical(checkPointer(new IPIPhysicalReal(source)));
            return true;
        }
        target->setPhysical(checkPointer(new IPIPhysicalKnown(source)));
        return true;

    case 5:     // unit - IPIUnit , the same for real and known; `real` is ignored
        target->setUnit(checkPointer(new IPIUnit(source)));
        return true;

    default:
        return false;
    }
}

// Only the armament half (type 0)
// can be updated: any other type, or a stale handle, answers false; otherwise the record's armament
// plug-in re-reads its weapons (IPIArmament +0x30 updateWeapons - pure in the base, ELF vtable
// IPIArmamentReal's forwards to updateWeaponsInternal(true)) and the answer is true.
// The armament plug-in itself is not null-tested. Both images.
bool InformationFactory::updateInformationPlugin(unsigned int handle, int type) const
{
    if (type)
        return false;
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return false;
    information->getArmament()->updateWeapons();
    return true;
}

// A WeaponInfoReal for the owning
// side, else a WeaponInfoKnown (ELF / PE ), each built on the IWeapon and registered through
// the vtable (registerWeapon, ELF +0x3C / PE +0x38).
unsigned int InformationFactory::requestWeaponInfo(unsigned int handle, IWeapon* weapon,
                                                   bool real) const
{
    if (real)
        return registerWeapon(handle, checkPointer(new WeaponInfoReal(weapon)));
    return registerWeapon(handle, checkPointer(new WeaponInfoKnown(weapon)));
}

// lookups
bool InformationFactory::checkIfInfoExists(unsigned int handle) const
{
    return informationManager->getInformation(handle) != 0;
}

bool InformationFactory::checkIfWeaponInfoExists(unsigned int handle) const
{
    return WeaponInfoManager::m_instance->getInformation(handle) != 0;
}

// asks for a new handle: the
// pool's next free one, or 0 when it is full (the record is then owned by nobody). Any other handle
// is overwritten in place when it is still valid - without destroying the record it held (the
// courier's setInformation destroys it; this does not) - and is returned whether or not it was.
unsigned int InformationFactory::registerInformation(unsigned int handle,
                                                     Information* information) const
{
    if (handle == 0xFFFFFFFFu)
        return informationManager->getPool()->acquire(information);
    informationManager->getPool()->set(handle, information);
    return handle;
}

// the same over WeaponInfoManager.
unsigned int InformationFactory::registerWeapon(unsigned int handle, WeaponInfo* weaponInfo) const
{
    if (handle == 0xFFFFFFFFu)
        return WeaponInfoManager::m_instance->registerInformation(weaponInfo);
    WeaponInfoManager::m_instance->setInformation(handle, weaponInfo);
    return handle;
}

} // namespace ai
} // namespace hfe
} // namespace dice
