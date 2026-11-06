
#include "WeaponObject.h"

#include "BFGame.h"                                          // IAIGame::getIsPrecaching (+0x68)
#include "BFTimer.h"                                         // IBFTimer::getGameTime
#include "IAIObject.h"

#include "../SingletonServer.h"                              // BFSingletonServer::m_iInformationFactory
#include "../../AIDLL/AI/AIKnowledge/BFInformationCourier.h" // IAIInformationCourier
#include "../../AIDLL/AI/AIKnowledge/InformationFactory.h"   // IInformationFactory
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"        // IAISettings
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"

#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Game/IGameLogic.h"                         // gameLogic->getTargetObject
#include "../../../Game/Objects/Base/SimpleObjectTemplate.h"
#include "../../../Game/Objects/CompTemplateBases.h"          // world::TargetSystem
#include "../../../Game/Objects/IItemObject.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/Objects/IObjectTemplate.h"
#include "../../../Game/Objects/IObjectTemplateComponent.h"
#include "../../../Game/Objects/IWeaponObject.h"
#include "../../../Game/Objects/ObjectLayoutAccess.h"
#include "../../../Game/Objects/Weapons/AmmoComponents/ReplenishingAmmoComp.h"
#include "../../../Game/Objects/Weapons/FireComponents/FireComp.h"
#include "../../../Game/Objects/Weapons/FireComponents/MultiFireComp.h"
#include "../../../Game/Objects/Weapons/GenericFireArm.h"
#include "../../../Game/Objects/Weapons/ZoomComponents/DefaultZoomComp.h"
#include "../../../Game/Objects/Weapons/ZoomComponents/ZoomComp.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAIGame* game() { return static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame); }
const IBFTimer* timer() { return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer); }
IInformationFactory* factory()
{
    return static_cast<IInformationFactory*>(BFSingletonServer::m_iInformationFactory);
}

const unsigned int kNoInformation = 0xFFFFFFFFu;

} // anonymous namespace

// the count, the template and three empty handles. The firearm
// interface (+0x08), the firing time and the object are left as the allocator returned them.
WeaponObject::WeaponObject(WeaponTemplate* weaponTemplate)
    : m_refCount(1), m_template(weaponTemplate)
{
    m_information[0] = kNoInformation;
    m_information[1] = kNoInformation;
    m_information[2] = kNoInformation;
}

// the vptr store only - the information handles are not freed.
WeaponObject::~WeaponObject()
{
}

// (, a folded twin): one count, destroyed at zero.
void WeaponObject::addRef()
{
    ++m_refCount;
}

int WeaponObject::getRef()
{
    return m_refCount;
}

int WeaponObject::release()
{
    const int refCount = --m_refCount;
    if (refCount == 0)
        delete this;
    return refCount;
}

// IBase, IWeapon and IObjectComponent answer the object itself.
void* WeaponObject::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IWeapon)
        return const_cast<WeaponObject*>(this);
    if (iid == world::IID_IObjectComponent)
        return const_cast<WeaponObject*>(this);
    return 0;
}

// Binds this weapon component to its engine firearm during initialization.
void WeaponObject::init(world::IObject* object)
{
    m_weaponObject = checkPointer(static_cast<world::IWeaponObject*>(
        object->queryInterface(world::IID_IWeaponObject)));
    if (m_template->m_initialized || game()->getIsPrecaching())                     // +0x68
        return;

    const world::SimpleObjectTemplate* projectileSimpleTemplate = 0;
    if (world::IObjectTemplate* const projectileTemplate = m_weaponObject->getProjectileTemplate())
        projectileSimpleTemplate = static_cast<const world::SimpleObjectTemplate*>(
            projectileTemplate->queryImplementation(world::CID_SimpleObjectTemplate));   // +0x50
    world::IObjectTemplateComponent* const ammoTemplate =
        const_cast<world::IObjectTemplate*>(object->getTemplate())
            ->queryComponent(world::IID_IWeaponAmmoCompTemplate);                    // +0x5C
    world::IObjectTemplate* const projectileTemplate = m_weaponObject->getProjectileTemplate();

    int weaponType = WTDefault;
    float replenishRadius = 0.0f;
    if (ammoTemplate && ammoTemplate->getClassID() == world::CID_ReplenishingAmmoCompTemplate)
    {
        const world::ReplenishingAmmoCompTemplate* const replenishing =
            static_cast<const world::ReplenishingAmmoCompTemplate*>(ammoTemplate);
        weaponType = (replenishing->getReplenishingType() != world::RTHeal) + 1;
        replenishRadius = replenishing->getAbilityRadius();
    }
    if (projectileTemplate)
    {
        if (projectileTemplate->getComponentFromClassID(world::CID_ResurrectCollisionCompTemplate))
            weaponType = WTResurrect;                                                   // +0x60
        else if (projectileTemplate->getComponentFromClassID(world::CID_StickyCollisionCompTemplate))
            weaponType = WTC4;
    }

    float gravityModifier = 1.0f;
    float drag;
    if (projectileSimpleTemplate)
    {
        gravityModifier = projectileSimpleTemplate->m_gravityModifier;
        const float mass = projectileSimpleTemplate->m_mass;
        drag = projectileSimpleTemplate->m_drag;
        switch (projectileSimpleTemplate->m_physicsType)
        {
        case 0:
            break;
        case 1:
        case 2:
            drag = static_cast<float>(static_cast<double>(drag) / mass
                                      * static_cast<double>(0.031415928f));
            break;
        case 3:
        case 4:
            if (drag != 0.0f)
                drag = 0.3f;
            break;
        default:
            Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/WeaponObject.cpp", 189, "")
                << "Unknown physics on projectile " << projectileSimpleTemplate->getName();   // +0x2C
            break;
        }
    }
    else
    {
        drag = 0.0f;
    }

    const int maxAmmoInMag = m_weaponObject->getAmmoInMag(0);                           // +0x94
    const int maxAmmo = m_weaponObject->getTotalAmmo();                                 // +0x84
    const float exitVelocity = m_weaponObject->getVelocity();                           // +0x18C
    int projectileMaterial = -1;
    if (projectileSimpleTemplate)
        projectileMaterial = projectileSimpleTemplate->getMaterial();                   // +0xC8
    const io::PlayerInputMap fireInput = m_weaponObject->getFireInput();                // +0x38
    int itemIndex = -1;
    if (world::IItemObject* const item =
            static_cast<world::IItemObject*>(object->queryInterface(world::IID_IItemObject)))
        itemIndex = item->getItemIndex();                                               // +0x30
    const WeaponGuidingSystem guidingSystem = targetSystemToWeaponGuidingSystem(
        static_cast<world::TargetSystem>(m_weaponObject->getTargetSystemType()));       // +0x130

    world::IWeaponZoomComp* const zoom = static_cast<world::IWeaponZoomComp*>(
        object->queryComponent(world::IID_IWeaponZoomComp, world::IID_IWeaponZoomComp));
    io::PlayerInputMap zoomInput = io::PINone;
    if (zoom && zoom->canZoom())
    {
        world::IObjectTemplateComponent* const zoomTemplate =
            const_cast<world::IObjectTemplate*>(object->getTemplate())
                ->queryComponent(world::IID_IWeaponZoomCompTemplate);
        if (zoomTemplate->getClassID() == world::CID_DefaultZoomCompTemplate &&
            static_cast<world::DefaultZoomCompTemplate*>(zoomTemplate)->getZoomLod())
            zoomInput = zoom->getInput();
    }
    world::IWeaponFireComp* const fire = static_cast<world::IWeaponFireComp*>(
        object->queryComponent(world::IID_IWeaponFireComp, world::IID_IWeaponFireComp));
    io::PlayerInputMap altFireInput = io::PINone;
    if (fire)
        altFireInput = fire->getAltFireInput();

    m_template->init(drag, gravityModifier, exitVelocity, projectileMaterial, maxAmmoInMag, maxAmmo,
                     fireInput, zoomInput, altFireInput, itemIndex,
                     static_cast<WeaponType>(weaponType), replenishRadius, guidingSystem);
}

// TSHeatSeeking..TSTVGuided map one for one; TSParabellic and anything
// else is "no guiding system".
WeaponGuidingSystem WeaponObject::targetSystemToWeaponGuidingSystem(world::TargetSystem system) const
{
    switch (system)
    {
    case world::TSHeatSeeking: return WGSHeatSeeking;
    case world::TSLaserGuided: return WGSLaserGuided;
    case world::TSWireGuided:  return WGSWireGuided;
    case world::TSTVGuided:    return WGSTVGuided;
    default:                   return WGSNone;
    }
}

void WeaponObject::setObject(world::IObject* object)
{
    m_object = object;
}

world::IObject* WeaponObject::getObject() const
{
    return m_object;
}

// the AI weapon has nothing to tick.
bool WeaponObject::update()
{
    return true;
}

// the side's handle, or a new one (the virtual (int, int) overload).
unsigned int WeaponObject::getInformation(int side, int ownSide)
{
    const unsigned int handle = m_information[side];
    if (handle == kNoInformation)
        return createInformation(side, ownSide);
    return handle;
}

// stamped with the game time - which the three-argument overload ignores.
unsigned int WeaponObject::createInformation(int side, int ownSide)
{
    return createInformation(timer()->getGameTime(), side, ownSide);
}

// keep a handle the factory still knows; otherwise ask it for weapon
// information on it ("own side" when the two sides agree) and store what it answers.
unsigned int WeaponObject::createInformation(double /*time*/, int side, int ownSide)
{
    const unsigned int handle = m_information[side];
    if (checkIfInfoExists(handle))
        return handle;
    const unsigned int created = static_cast<unsigned int>(
        factory()->requestWeaponInfo(handle, this, side == ownSide));
    m_information[side] = created;
    return created;
}

// free the side's weapon information if the courier still has it.
void WeaponObject::deleteInformation(int side)
{
    if (m_information[side] != kNoInformation)
    {
        IAIInformationCourier* const courier =
            static_cast<IAIInformationCourier*>(CommonSingletonServer::m_iaiInformationCourier);
        if (courier->getWeaponInfo(m_information[side]))
            courier->freeWeaponInfo(m_information[side]);
        m_information[side] = kNoInformation;
    }
}

// sides 0..getNSides inclusive, through the virtual.
void WeaponObject::deleteAllInformation()
{
    const int sides = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1;
    for (int side = 0; side < sides; ++side)
        deleteInformation(side);
}

// the firearm's absolute transformation, or retail's fatal Debug(Error)
// "Cant get object" (line 478) and the identity.
const Mat4& WeaponObject::getAbsoluteTransformation() const
{
    world::IObject* const object =
        static_cast<world::IObject*>(m_weaponObject->queryInterface(world::IID_IObject));
    if (object)
        return object->getAbsoluteTransformation();
    Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/WeaponObject.cpp", 478, "") << "Cant get object";
    return Mat4::getIdentity();
}

// the translation row of the virtual above.
const Vec3& WeaponObject::getPosition() const
{
    return *reinterpret_cast<const Vec3*>(&getAbsoluteTransformation().m[12]);
}

// A GenericFireArm whose fire component
// template is a MultiFireCompTemplate aims from the multi-barrel component's medium origin;
// everything else asks the firearm. The fire component itself is not null-tested (a null one
// reaches getMediumProjectileOrigin with a null object).
const Mat4& WeaponObject::getProjectileOrigin()
{
    world::IObjectTemplate* const objectTemplate =
        const_cast<world::IObjectTemplate*>(m_object->getTemplate());
    if (!objectTemplate->queryImplementation(world::CID_GenericFireArmTemplate))
        return m_weaponObject->getProjectileOrigin();                                  // +0x6C
    world::IObjectTemplateComponent* const fireTemplate =
        objectTemplate->queryComponent(world::IID_IWeaponFireCompTemplate);
    if (!fireTemplate->queryImplementation(world::CID_MultiFireCompTemplate))           // +0x28
        return m_weaponObject->getProjectileOrigin();
    world::IWeaponFireComp* const fire = static_cast<world::IWeaponFireComp*>(
        m_object->queryComponent(world::IID_IWeaponFireComp, world::IID_IWeaponFireComp));
    return static_cast<world::MultiFireComp*>(fire)->getMediumProjectileOrigin();
}

// the template's fields and the firearm's live state.
int WeaponObject::getWeaponActivate() const          { return m_template->m_weaponActivate; }
int WeaponObject::getWeaponFire() const              { return m_template->m_weaponFire; }
int WeaponObject::getWeaponZoom() const              { return m_template->m_weaponZoom; }
int WeaponObject::getWeaponAltFire() const           { return m_template->m_altFire; }
bool WeaponObject::isZoomed() const                  { return m_weaponObject->getZoom() != 0; }
int WeaponObject::getWeaponState() const             { return m_weaponObject->getWeaponState(); }
float WeaponObject::getDrag() const                  { return m_template->m_drag; }
float WeaponObject::getGravityModifier() const       { return m_template->m_gravityModifier; }
float WeaponObject::getExitVelocity() const          { return m_template->m_exitVelocity; }
float WeaponObject::getMinRange() const              { return m_template->m_minRange; }
float WeaponObject::getMaxRange() const              { return m_template->m_maxRange; }
float WeaponObject::getOptimalRangePercentage() const{ return m_template->m_optimalRangePercentage; }
SoldierPose WeaponObject::getFiringPose() const      { return m_template->m_firingPose; }
float WeaponObject::getAllowedDeviation() const      { return m_template->m_allowedDeviation; }
bool WeaponObject::getFiresThroughTransparent() const{ return m_template->m_firesThroughTransparent; }
const int* WeaponObject::getStrengths() const        { return m_template->m_strengths; }
int WeaponObject::getStrength(int index) const       { return m_template->m_strengths[index]; }
float WeaponObject::getExplosionRadius() const       { return m_template->m_explosionRadius; }
bool WeaponObject::getIsThrown() const               { return m_template->getIsThrown(); }
bool WeaponObject::useAimerOnly() const              { return m_template->m_useAimerOnly; }
int WeaponObject::getFireRate() const                { return m_template->m_fireRate; }
int WeaponObject::getAmmo() const                    { return m_weaponObject->getTotalAmmo(); }
int WeaponObject::getAmmoInMag(int mag) const
{
    return m_weaponObject->getAmmoInMag(static_cast<unsigned int>(mag));
}
int WeaponObject::getMaxAmmo() const                 { return m_template->m_maxAmmo; }
int WeaponObject::getMaxAmmoInMag() const            { return m_template->m_maxAmmoInMag; }
bool WeaponObject::hasAutoReload() const             { return m_weaponObject->getAutoReload(); }
float WeaponObject::getReloadTime() const            { return m_weaponObject->getReloadTime(); }
int WeaponObject::getWeaponType() const              { return m_template->m_weaponType; }
int WeaponObject::getWeaponGuidingSystem() const     { return m_template->m_weaponGuidingSystem; }
void WeaponObject::setTimeOfFiring(double time)      { m_timeOfFiring = time; }
double WeaponObject::getTimeOfFiring() const         { return m_timeOfFiring; }
bool WeaponObject::getIndirect() const               { return m_template->m_indirect; }
float WeaponObject::getHeat() const                  { return m_weaponObject->getHeat(); }
bool WeaponObject::hasHeat() const                   { return m_weaponObject->getHasHeat(); }
bool WeaponObject::usesHeat() const                  { return m_weaponObject->getUsesHeat(); }
bool WeaponObject::isReadyToFire() const             { return m_weaponObject->isReadyToUse(); }
float WeaponObject::getSoundSphereRadius() const     { return m_template->m_soundSphereRadius; }
int WeaponObject::getProjectileMaterial() const      { return m_template->m_projectileMaterial; }
world::IObject* WeaponObject::getProjectile() const  { return m_weaponObject->getProjectile(); }
bool WeaponObject::hasTargetLocked() const           { return m_weaponObject->hasTargetLock(); }

// every current magazine's ammunition, summed.
int WeaponObject::getCurrentAmmo() const
{
    int ammo = 0;
    const int mags = m_weaponObject->getCurrentNrOfMags();                             // +0xA0
    for (int mag = 0; mag < mags; ++mag)
        ammo += m_weaponObject->getAmmoInMag(static_cast<unsigned int>(mag));
    return ammo;
}

// (ELF only - the PE strips it, nothing calls it): the current magazine count
// despite the name.
int WeaponObject::getCurrentMagSize() const
{
    return m_weaponObject->getCurrentNrOfMags();
}

// -1 while the firearm reports negative ammunition, else its magazines.
int WeaponObject::getNumberOfMags() const
{
    if (m_weaponObject->getTotalAmmo() >= 0)
        return m_weaponObject->getMaxNrOfMags();                                       // +0x98
    return -1;
}

// 0x8422D60 / PE 0x73DB20: fire rate 2 [UNRESOLVED enumerator; the name says burst].
bool WeaponObject::getBurst() const
{
    return static_cast<int>(m_weaponObject->getFireRate()) == 2;
}

// The share `fraction` of a full load (magazine size x
// magazines), truncated; nothing happens for 0. The PE multiplies in 53 bits (fild / fmul) and
// asks for the magazine size first; the ELF rounds the product to float (cvtsi2ss) and asks for the
// magazine count first ([PLATFORM]). Any non-zero amount - a negative one included - is reloaded.
bool WeaponObject::refillAmmo(float fraction)
{
    const int magSize = m_weaponObject->getMagSize();                                  // +0xA8
    const int mags = m_weaponObject->getMaxNrOfMags();                                 // +0x98
    const int amount = static_cast<int>(static_cast<double>(magSize * mags) * fraction);
    if (amount == 0)
        return false;
    m_weaponObject->reloadAmmo(static_cast<unsigned int>(amount), false);              // +0x7C
    return true;
}

// retail's fatal Debug(Error) "Make weapon use fake
// fire." (lines 402 / 410), then false - the AI never got fake fire.
bool WeaponObject::setFakeFire(bool /*fake*/)
{
    Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/WeaponObject.cpp", 402, "")
        << "Make weapon use fake fire.";
    return false;
}

bool WeaponObject::getFakeFire() const
{
    Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/WeaponObject.cpp", 410, "")
        << "Make weapon use fake fire.";
    return false;
}

// calculateDeviation - , (ELF only; no caller in either image, the PE strips it).
// (1 - skill) * 0.25 + max(0, correctionTime * (1 - skill) - elapsed) * correctionRate, scaled by
// the template's deviation; `elapsed` is the game time since `since`, rounded to float.
float WeaponObject::calculateDeviation(float skill, double since)
{
    const float unskilled = static_cast<float>(1.0 - static_cast<double>(skill));
    const float elapsed = static_cast<float>(timer()->getGameTime() - since);
    double remaining = static_cast<double>(m_template->m_deviationCorrectionTime) * unskilled - elapsed;
    if (remaining <= 0.0)
        remaining = 0.0;
    return static_cast<float>((static_cast<double>(unskilled) * 0.25 +
                               remaining * m_template->m_deviationCorrectionRate) *
                              m_template->m_deviation);
}

// (inlined in the PE): does the factory still know this weapon information handle?
bool WeaponObject::checkIfInfoExists(unsigned int handle)
{
    return factory()->checkIfWeaponInfoExists(handle);                                 // +0x34
}

// only the two replenishing types (through the virtual) have a radius.
float WeaponObject::getReplenishRadius() const
{
    const int type = getWeaponType();
    if (type == WTHeal || type == WTAmmo)
        return m_template->m_replenishRadius;
    return 0.0f;
}

// The deviation a bot's weapon gets: the time since
// `since` in correction units (capped at 1; a NaN passes), then, in the PE's order
// ((((1 - t) + 1) * 0.5) * (1 - skill)) * deviation * scale
// (the ELF computes ((1 - t) * 0.5 + 0.5) * ((1 - skill) * deviation * scale) - [PLATFORM]).
void WeaponObject::setBotSkill(float skill, double since, float scale)
{
    double t = (timer()->getGameTime() - since) * static_cast<double>(m_template->m_deviationCorrectionRate);
    if (t > 1.0)
        t = 1.0;
    const float deviation = static_cast<float>(
        ((((1.0 - t) + 1.0) * 0.5) * (1.0 - static_cast<double>(skill))) *
        static_cast<double>(m_template->m_deviation) * static_cast<double>(scale));
    m_weaponObject->setAIDeviation(deviation);                                         // +0xD0
}

// lock the firearm onto the object behind a target id.
bool WeaponObject::setTarget(unsigned int targetId)
{
    if (!m_weaponObject->queryInterface(world::IID_IObject))
        return false;
    world::IObject* const target = gameLogic->getTargetObject(targetId);               // +0x1B8
    if (!target)
        return false;
    static_cast<world::GenericFireArm*>(m_weaponObject)->setTargetObject(target);
    return true;
}

// the locked target's root object's AI object (IObject +0x34), or null.
IAIObject* WeaponObject::getLockedTarget() const
{
    world::IObject* const target = m_weaponObject->getTargetObject();                  // +0x120
    if (!target)
        return 0;
    return static_cast<IAIObject*>(
        world::object_detail::LayoutAccess::aiObject(world::getRootParent(target)));
}

// the firearm's template name. The object query is not null-tested.
const std::string& WeaponObject::getName() const
{
    return static_cast<world::IObject*>(m_weaponObject->queryInterface(world::IID_IObject))
        ->getTemplate()->getName();
}

} // namespace ai
} // namespace hfe
} // namespace dice
