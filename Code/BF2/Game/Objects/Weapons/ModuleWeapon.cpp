// Registers firearm and weapon-component console properties in startup declaration order.
// projectileTemplate stores the name and clears the cached pointer, allowing later precaching to reserve projectile IDs.
// Original translation-unit placement is [INFERRED].

#include <cstdlib>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

#include "../FireCompTemplate.h"
#include "../ExplosivesFireCompTemplate.h"
#include "../GenericFireArmTemplate.h"
#include "../SingleFireCompTemplate.h"
#include "../ThrownFireCompTemplate.h"
#include "../AmmoCompTemplate.h"
#include "FireComponents/FireComp.h"
#include "FireComponents/LPFireComp.h"
#include "FireComponents/MultiFireComp.h"
#include "AmmoComponents/ReplenishingAmmoComp.h"
#include "AnimationComponents/DefaultAnimationComp.h"
#include "DeviationComponents/TargetDeviationComp.h"
#include "DeviationComponents/SimpleDeviationComp.h"
#include "DeviationComponents/SoldierDeviationComp.h"
#include "RecoilComponents/SoldierBasedRecoilComp.h"
#include "RecoilComponents/WeaponBasedRecoilComp.h"
#include "SoundComponents/DefaultSoundComp.h"
#include "TargetComponents/LPTargetComp.h"
#include "TargetComponents/MultiFireArmTargetComp.h"
#include "TargetComponents/SimpleTargetComp.h"
#include "ZoomComponents/DefaultZoomComp.h"
#include "ZoomComponents/ToggleCameraComp.h"
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"     // world::getActiveTemplate

namespace dice {
namespace hfe {
namespace world {

// /0x8580020 and 0x85803F0/0x8580450.  These enum stream pairs are part of this
// weapons-vocabulary TU in retail.  The readers intentionally leave an unknown token unchanged
// and accept a one-character numeric spelling by round-tripping it through the matching writer.
std::ostream& operator<<(std::ostream& stream, WeaponTrigger trigger)
{
    switch (trigger)
    {
    case WTPostFire:     stream << "WTPostFire";     break;
    case WTFireComplete: stream << "WTFireComplete"; break;
    case WTReload:       stream << "WTReload";       break;
    default:                                            break;
    }
    return stream;
}

std::istream& operator>>(std::istream& stream, WeaponTrigger& trigger)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        std::ostringstream normalized;
        normalized << static_cast<WeaponTrigger>(std::atoi(token.c_str()));
        token = normalized.str();
    }

    if (equalsIgnoreCase(token, "WTPostFire"))
        trigger = WTPostFire;
    else if (equalsIgnoreCase(token, "WTReload"))
        trigger = WTReload;
    else if (equalsIgnoreCase(token, "WTFireComplete"))
        trigger = WTFireComplete;
    return stream;
}

std::ostream& operator<<(std::ostream& stream, WeaponAction action)
{
    switch (action)
    {
    case WAShow: stream << "WAShow"; break;
    case WAHide: stream << "WAHide"; break;
    case WADrop: stream << "WADrop"; break;
    default:                         break;
    }
    return stream;
}

std::istream& operator>>(std::istream& stream, WeaponAction& action)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        std::ostringstream normalized;
        normalized << static_cast<WeaponAction>(std::atoi(token.c_str()));
        token = normalized.str();
    }

    if (equalsIgnoreCase(token, "WAShow"))
        action = WAShow;
    else if (equalsIgnoreCase(token, "WAHide"))
        action = WAHide;
    else if (equalsIgnoreCase(token, "WADrop"))
        action = WADrop;
    return stream;
}

// /0x8580D30. As with the weapon action/trigger readers above, retail accepts a
// one-character numeric spelling by normalising it through the matching writer first; an unknown
// token leaves the destination unchanged.
std::ostream& operator<<(std::ostream& stream, ReplenishingType type)
{
    switch (type)
    {
    case RTHeal: stream << "RTHeal"; break;
    case RTAmmo: stream << "RTAmmo"; break;
    default:                         break;
    }
    return stream;
}

std::istream& operator>>(std::istream& stream, ReplenishingType& type)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        std::ostringstream normalized;
        normalized << static_cast<ReplenishingType>(std::atoi(token.c_str()));
        token = normalized.str();
    }

    if (equalsIgnoreCase(token, "RTHeal"))
        type = RTHeal;
    else if (equalsIgnoreCase(token, "RTAmmo"))
        type = RTAmmo;
    return stream;
}

namespace {

// Weapon bindings join the shared BF object-template list; no separate weapons module registers a list.
// Keep this tag equal to the one registered by GameLogicObjTemplBFModule.
const char* const kWeaponConsoleTag = "world.objTemplGame";

GenericFireArmTemplate* activeFireArm()
{
    return static_cast<GenericFireArmTemplate*>(getActiveTemplate(CID_GenericFireArmTemplate));
}

IItemTemplate* activeFireArmItemTemplate()
{
    // The PE executors explicitly adjust to the secondary IItemTemplate face. The ELF emits the
    // equivalent calls through GenericFireArmTemplate's appended primary override slots; this
    // source spelling preserves the PE dispatch while reaching the same four retail bodies.
    return static_cast<IItemTemplate*>(activeFireArm());
}

FireCompTemplate* activeFireCompTemplate()
{
    return static_cast<FireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_FireCompTemplate));
}

SingleFireCompTemplate* activeSingleFireCompTemplate()
{
    return static_cast<SingleFireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_SingleFireCompTemplate));
}

ThrownFireCompTemplate* activeThrownFireCompTemplate()
{
    return static_cast<ThrownFireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_ThrownFireCompTemplate));
}

ExplosivesFireCompTemplate* activeExplosivesFireCompTemplate()
{
    return static_cast<ExplosivesFireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_ExplosivesFireCompTemplate));
}

MultiFireCompTemplate* activeMultiFireCompTemplate()
{
    return static_cast<MultiFireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_MultiFireCompTemplate));
}

LPFireCompTemplate* activeLPFireCompTemplate()
{
    return static_cast<LPFireCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponFireCompTemplate, CID_LPFireCompTemplate));
}

AmmoCompTemplate* activeAmmoCompTemplate()
{
    return static_cast<AmmoCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponAmmoCompTemplate, CID_AmmoCompTemplate));
}

ReplenishingAmmoCompTemplate* activeReplenishingAmmoCompTemplate()
{
    return static_cast<ReplenishingAmmoCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponAmmoCompTemplate,
                                   CID_ReplenishingAmmoCompTemplate));
}

ZoomCompTemplate* activeZoomCompTemplate()
{
    return static_cast<ZoomCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponZoomCompTemplate, CID_ZoomCompTemplate));
}

DefaultZoomCompTemplate* activeDefaultZoomCompTemplate()
{
    return static_cast<DefaultZoomCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponZoomCompTemplate, CID_DefaultZoomCompTemplate));
}

ToggleCameraCompTemplate* activeToggleCameraCompTemplate()
{
    return static_cast<ToggleCameraCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponZoomCompTemplate,
                                   CID_ToggleCameraCompTemplate));
}

DefaultSoundCompTemplate* activeDefaultSoundCompTemplate()
{
    return static_cast<DefaultSoundCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponSoundCompTemplate,
                                   CID_DefaultSoundCompTemplate));
}

AnimationCompTemplate* activeAnimationCompTemplate()
{
    return static_cast<AnimationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponAnimationCompTemplate,
                                   CID_AnimationCompTemplate));
}

DefaultAnimationCompTemplate* activeDefaultAnimationCompTemplate()
{
    return static_cast<DefaultAnimationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponAnimationCompTemplate,
                                   CID_DefaultAnimationCompTemplate));
}

WeaponBasedRecoilCompTemplate* activeWeaponBasedRecoilCompTemplate()
{
    return static_cast<WeaponBasedRecoilCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponRecoilCompTemplate,
                                   CID_WeaponBasedRecoilCompTemplate));
}

SoldierBasedRecoilCompTemplate* activeSoldierBasedRecoilCompTemplate()
{
    return static_cast<SoldierBasedRecoilCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponRecoilCompTemplate,
                                   CID_SoldierBasedRecoilCompTemplate));
}

DeviationCompTemplate* activeDeviationCompTemplate()
{
    return static_cast<DeviationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponDeviationCompTemplate,
                                   CID_DeviationCompTemplate));
}

TargetDeviationCompTemplate* activeTargetDeviationCompTemplate()
{
    return static_cast<TargetDeviationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponDeviationCompTemplate,
                                   CID_TargetDeviationCompTemplate));
}

SimpleDeviationCompTemplate* activeSimpleDeviationCompTemplate()
{
    return static_cast<SimpleDeviationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponDeviationCompTemplate,
                                   CID_SimpleDeviationCompTemplate));
}

SoldierDeviationCompTemplate* activeSoldierDeviationCompTemplate()
{
    return static_cast<SoldierDeviationCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponDeviationCompTemplate,
                                   CID_SoldierDeviationCompTemplate));
}

TargetCompTemplate* activeTargetCompTemplate()
{
    return static_cast<TargetCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponTargetCompTemplate,
                                   CID_TargetCompTemplate));
}

MultiFireArmTargetCompTemplate* activeMultiFireArmTargetCompTemplate()
{
    return static_cast<MultiFireArmTargetCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponTargetCompTemplate,
                                   CID_MultiFireArmTargetCompTemplate));
}

LPTargetCompTemplate* activeLPTargetCompTemplate()
{
    return static_cast<LPTargetCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponTargetCompTemplate,
                                   CID_LPTargetCompTemplate));
}

SimpleTargetCompTemplate* activeSimpleTargetCompTemplate()
{
    return static_cast<SimpleTargetCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponTargetCompTemplate,
                                   CID_SimpleTargetCompTemplate));
}

void objTemplDeviationSetFireDev(float maximum, float add, float subtract)
{
    activeDeviationCompTemplate()->setFireDev(maximum, add, subtract);
}

void objTemplFireAddFireRate(int fireRate)
{
    activeFireCompTemplate()->addFireRate(fireRate);
}

void objTemplFireAddTimeEvent(WeaponTrigger trigger, WeaponAction action, float seconds)
{
    activeSingleFireCompTemplate()->addWeaponTimeEvent(trigger, action, seconds);
}

void objTemplZoomAddZoomFactor(float factor)
{
    activeDefaultZoomCompTemplate()->addZoomFactor(factor);
}

void objTemplTargetSetOnLockChangeToWeapon(bool enabled, std::string weaponName)
{
    activeTargetCompTemplate()->setOnLockChangeToWeapon(enabled, weaponName);
}

void objTemplMultiFireAddBarrelName(std::string name)
{
    activeMultiFireCompTemplate()->addBarrelName(name);
}

void objTemplMultiFireRemoveBarrelName(std::string name)
{
    activeMultiFireCompTemplate()->removeBarrelName(name);
}

bool objTemplMultiFireIsBarrel(std::string name)
{
    return activeMultiFireCompTemplate()->isBarrel(name);
}

void objTemplSoldierDeviationSetTurnDev(float maximum, float pitch, float yaw, float subtract)
{
    activeSoldierDeviationCompTemplate()->setTurnDev(maximum, pitch, yaw, subtract);
}

void objTemplSoldierDeviationSetSpeedDev(float maximum, float forward, float strafe, float subtract)
{
    activeSoldierDeviationCompTemplate()->setSpeedDev(maximum, forward, strafe, subtract);
}

void objTemplSoldierDeviationSetMiscDev(float maximum, float jump, float subtract)
{
    activeSoldierDeviationCompTemplate()->setMiscDev(maximum, jump, subtract);
}

// The SET arm, as the retail exec has it: assign the authored name, null the resolved cache.
void objTemplProjectileTemplateSet(const std::string& templateName)
{
    GenericFireArmTemplate* const fireArm = activeFireArm();
    fireArm->m_projectileTemplateName = templateName;
    fireArm->m_projectileTemplate = 0;
}

// ConsoleClass324, 0x85C3E80 - the exact mirror of the primary above, on +476/+484.
void objTemplSecondaryProjectileTemplateSet(const std::string& templateName)
{
    GenericFireArmTemplate* const fireArm = activeFireArm();
    fireArm->m_secondaryProjectileTemplateName = templateName;
    fireArm->m_secondaryProjectileTemplate = 0;
}

// the ten raw-field SET arms
// Each generated executor stores straight into the field off getActiveTemplate(...) with no setter
// symbol in between; the offset each one writes is in the comment. See GenericFireArmTemplate.h's
// public-island block for the whole table.
void objTemplVelocitySet(float v)               { activeFireArm()->m_velocity = v; }               // +456
void objTemplKeepProjectilesSet(float v)        { activeFireArm()->m_keepProjectiles = v; }        // +488
void objTemplHeatAddWhenFireSet(float v)        { activeFireArm()->m_heatAddWhenFire = v; }        // +432
void objTemplCoolDownPerSecSet(float v)         { activeFireArm()->m_coolDownPerSec = v; }         // +436
void objTemplInheritHeatFromPrimarySet(bool v)  { activeFireArm()->m_inheritHeatFromPrimary = v; } // +444
void objTemplIsSelectableSet(bool v)            { activeFireArm()->m_isSelectable = v; }           // +492
void objTemplForceSustainedFireSet(bool v)      { activeFireArm()->m_canInternalHandleMessage = v; } // +493
void objTemplHideFirstPersonSet(bool v)         { activeFireArm()->m_setHideFirstPerson = v; }     // +424
void objTemplBlockInputOnRotBundleSet(const std::string& v)
                                                { activeFireArm()->m_blockInputOnRotBundle = v; }  // +500

// the two that route through a real method
void objTemplDelayToUseSet(float v)      { activeFireArm()->setDelayToUse(v); }        // also caches x30
void objTemplOverheatPenaltySet(float v) { activeFireArm()->setOverheatPenalty(v); }   // clamped at 10

} // namespace

// The retail defaults for this property block: ("ObjectTemplate", access 2, category 0x10).
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 16)

// GenericFireArmTemplate's item identity properties. The generated executors are attested in the
// PE at 0x489AF0 (itemIndex) and the adjacent itemType class; the ELF accessors are
// ..0x8256D40. These are load-bearing for Kit::init: without itemIndex registration every
// firearm retains -1, so the retail duplicate-index assertion sees the second weapon as a duplicate.
BF2_CONSOLE_PROPERTY(itemIndex, int, activeFireArmItemTemplate(),
                     setItemIndex, getItemIndex, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY(itemType, int, activeFireArmItemTemplate(),
                     setItemType, getItemType, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(projectileTemplate, std::string,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_projectileTemplateName,
                        objTemplProjectileTemplateSet, kWeaponConsoleTag)

// Firearm template properties. delayToUse and overheatPenalty call setters with side effects;
// the remaining rows directly update their authored values.
BF2_CONSOLE_PROPERTY_FN(velocity, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_velocity,
                        objTemplVelocitySet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(secondaryProjectileTemplate, std::string,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_secondaryProjectileTemplateName,
                        objTemplSecondaryProjectileTemplateSet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(keepProjectiles, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_keepProjectiles,
                        objTemplKeepProjectilesSet, kWeaponConsoleTag)

// The GET arm reads the float at +412 directly; only the SET arm goes through the method that
// also refreshes the +416 tick cache. That asymmetry is the executor's, not ours.
BF2_CONSOLE_PROPERTY_FN(delayToUse, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_delayToUse,
                        objTemplDelayToUseSet, kWeaponConsoleTag)

// the overheat model. All four were dead, which is why GenericFireArm::postFire's heat
// arithmetic has never had an effect: heatAddWhenFire stayed 0.0f on every weapon.
BF2_CONSOLE_PROPERTY_FN(heatAddWhenFire, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_heatAddWhenFire,
                        objTemplHeatAddWhenFireSet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(coolDownPerSec, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_coolDownPerSec,
                        objTemplCoolDownPerSecSet, kWeaponConsoleTag)

// Same GET/SET asymmetry as delayToUse: the read is the raw field, the write is clamped at 10.
BF2_CONSOLE_PROPERTY_FN(overheatPenalty, float,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_overheatPenalty,
                        objTemplOverheatPenaltySet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(inheritHeatFromPrimary, bool,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_inheritHeatFromPrimary,
                        objTemplInheritHeatFromPrimarySet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(isSelectable, bool,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_isSelectable,
                        objTemplIsSelectableSet, kWeaponConsoleTag)

// The .con spelling and the member name differ - see the header's note on this field.
BF2_CONSOLE_PROPERTY_FN(forceSustainedFire, bool,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_canInternalHandleMessage,
                        objTemplForceSustainedFireSet, kWeaponConsoleTag)

// registered AS `hideFirstPerson`, which is the bare property string at 0x88FEE16 - NOT the
// `setHideFirstPerson` spelling makeScript writes. Registering the parser's name is what makes a
// .con file load; retail's save side emits a line its own parser rejects. No stock file writes it.
BF2_CONSOLE_PROPERTY_FN(hideFirstPerson, bool,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_setHideFirstPerson,
                        objTemplHideFirstPersonSet, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY_FN(blockInputOnRotBundle, std::string,
                        getActiveTemplate(CID_GenericFireArmTemplate) != 0,
                        activeFireArm()->m_blockInputOnRotBundle,
                        objTemplBlockInputOnRotBundleSet, kWeaponConsoleTag)

// Common fire-component template surface.  The PE generated executors are 0x48BA40..0x48D040;
// all gate through getActiveTemplateComponent(50417, 38100).  `addFireRate` is exactly an `int`
// method argument in the generated constructor (0x48C7E0), not an unresolved FireRate parser.
BF2_CONSOLE_PROPERTY(fire.roundsPerMinute, int, activeFireCompTemplate(),
                     setRoundsPerMinute, getRoundsPerMinute, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.fireStartDelay, RandomDistribution, activeFireCompTemplate(),
                     setFireStartDelay, getFireStartDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.fireLaunchDelay, float, activeFireCompTemplate(),
                     setFireLaunchDelay, getFireLaunchDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.fireInput, io::PlayerInputMap, activeFireCompTemplate(),
                     setFireInput, getFireInput, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.altFireInput, io::PlayerInputMap, activeFireCompTemplate(),
                     setAltFireInput, getAltFireInput, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.ignoreFireButton, io::PlayerInputMap, activeFireCompTemplate(),
                     setIgnoreFireButton, getIgnoreFireButton, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.batchSize, int, activeFireCompTemplate(),
                     setBatchSize, getBatchSize, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.burstSize, int, activeFireCompTemplate(),
                     setBurstSize, getBurstSize, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(fire.addFireRate, objTemplFireAddFireRate,
                            activeFireCompTemplate() != 0, int, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.projectileStartPosition, Vec3, activeFireCompTemplate(),
                     setProjectileStartPosition, getProjectileStartPosition, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.fireInCameraDof, bool, activeFireCompTemplate(),
                     setFireInCameraDof, getFireInCameraDof, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.fireInCameraDofLocked, bool, activeFireCompTemplate(),
                     setFireInCameraDofLocked, getFireInCameraDofLocked, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.maxProjectilesInWorld, int, activeFireCompTemplate(),
                     setMaxProjectilesInWorld, getMaxProjectilesInWorld, kWeaponConsoleTag)

// `fire.addTimeEvent` is the one recovered three-argument active method in this slice.  Spell the
// local generated class out because the shared macro family currently has active forms only up to
// arity two.  Constructor metadata and executor are PE 0x48D0A0 / 0x48D230; the type spellings are
// exactly WeaponTrigger, WeaponAction, float and the active concrete CID is 38107.
#define BF2_WEAPON_METHOD_3_ACTIVE(fnName, target, activeExpr, A0, A1, A2, listTag)                \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                     \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 0, 3, 3, "void", true)                                                    \
        m_argTypes[0] = #A0;                                                                       \
        m_argTypes[1] = #A1;                                                                       \
        m_argTypes[2] = #A2;                                                                       \
    BF2_CC_TAIL(3, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 3) return;                                                               \
        m_hasReturnValue = false;                                                                  \
        target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1), BF2_CC_ARG(A2, 2));                          \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                            \
    {                                                                                              \
        BF2_CC_CONVERT(A0, 1)                                                                      \
        BF2_CC_CONVERT(A1, 2)                                                                      \
        BF2_CC_CONVERT(A2, 3)                                                                      \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                               \
}

BF2_WEAPON_METHOD_3_ACTIVE(fire.addTimeEvent, objTemplFireAddTimeEvent,
                           activeSingleFireCompTemplate() != 0,
                           WeaponTrigger, WeaponAction, float, kWeaponConsoleTag)

#undef BF2_WEAPON_METHOD_3_ACTIVE

// SingleFireCompTemplate surface.  These bindings all gate CID 38107 in the PE executors
// (0x48D4B0..0x48E2B0); the five delay properties call the named time-event wrappers while the
// four scalar properties directly address the recovered leaf fields.
BF2_CONSOLE_PROPERTY(fire.hideWeaponAfterFiringDelay, float, activeSingleFireCompTemplate(),
                     setHideWeaponAfterFiringDelay, getHideWeaponAfterFiringDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.dropWeaponAfterFiringDelay, float, activeSingleFireCompTemplate(),
                     setDropWeaponAfterFiringDelay, getDropWeaponAfterFiringDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.showWeaponAfterReloadDelay, float, activeSingleFireCompTemplate(),
                     setShowWeaponAfterReloadDelay, getShowWeaponAfterReloadDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.dropWeaponAfterReloadDelay, float, activeSingleFireCompTemplate(),
                     setDropWeaponAfterReloadDelay, getDropWeaponAfterReloadDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.hideWeaponAfterReloadDelay, float, activeSingleFireCompTemplate(),
                     setHideWeaponAfterReloadDelay, getHideWeaponAfterReloadDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.dropLod, int, activeSingleFireCompTemplate(),
                     setDropLod, getDropLod, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.hideLod, int, activeSingleFireCompTemplate(),
                     setHideLod, getHideLod, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.busyUntilButtonRelease, bool, activeSingleFireCompTemplate(),
                     setBusyUntilButtonRelease, getBusyUntilButtonRelease, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.onlyFireWhenProne, bool, activeSingleFireCompTemplate(),
                     setOnlyFireWhenProne, getOnlyFireWhenProne, kWeaponConsoleTag)

// MultiFireCompTemplate and LPFireCompTemplate.  The first three entries are genuine methods:
// add/remove mutate the authored barrel-name list and isBarrel returns a bool.  `radioMessageDelay`
// belongs to LPFire, despite sitting beside MultiFire's registrations in the retail weapons TU.
BF2_CONSOLE_METHOD_1_ACTIVE(fire.addBarrelName, objTemplMultiFireAddBarrelName,
                            activeMultiFireCompTemplate() != 0,
                            std::string, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(fire.removeBarrelName, objTemplMultiFireRemoveBarrelName,
                            activeMultiFireCompTemplate() != 0,
                            std::string, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(fire.isBarrel, objTemplMultiFireIsBarrel, bool,
                                activeMultiFireCompTemplate() != 0,
                                std::string, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.useDummyProjectiles, bool, activeMultiFireCompTemplate(),
                     setUseDummyProjectiles, getUseDummyProjectiles, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.radioMessageDelay, float, activeLPFireCompTemplate(),
                     setRadioMessageDelay, getRadioMessageDelay, kWeaponConsoleTag)

// ThrownFireCompTemplate.  The property constructors at PE 0x48EC60..0x48F600 spell all scalar
// arguments `float`, except the exact `Vec3` spelling for projectileStartRotation.  Their
// executors gate CID 38110.  In particular, projectileStartRotation writes the leaf's own Vec3 at
// PE +0xA0; it is not a guessed alias of FireCompTemplate's earlier rotation member.
BF2_CONSOLE_PROPERTY(fire.fireLaunchDelaySoft, float, activeThrownFireCompTemplate(),
                     setFireLaunchDelaySoft, getFireLaunchDelaySoft, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.timeToReachMaxStrengthSlow, float, activeThrownFireCompTemplate(),
                     setTimeToReachMaxStrengthSlow, getTimeToReachMaxStrengthSlow,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.pullbackTime, float, activeThrownFireCompTemplate(),
                     setPullbackTime, getPullbackTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.showWeaponAfterReloadDelay, float, activeThrownFireCompTemplate(),
                     setShowWeaponAfterReloadDelay, getShowWeaponAfterReloadDelay,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.projectileStartRotation, Vec3, activeThrownFireCompTemplate(),
                     setProjectileStartRotation, getProjectileStartRotation, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.randomRotationSpeed, float, activeThrownFireCompTemplate(),
                     setRandomRotationSpeed, getRandomRotationSpeed, kWeaponConsoleTag)

// ExplosivesFireCompTemplate. PE constructors 0x48F7C0/0x48F9A0/0x48FB70/0x48FDC0 in .CRT
// declaration order and their
// generated executors gate CID 38111; detonatorObject is the recovered direct std::string field
// binding while the three times use their cache-updating named setters.
BF2_CONSOLE_PROPERTY(fire.toggleWeaponTime, float, activeExplosivesFireCompTemplate(),
                     setToggleWeaponTime, getToggleWeaponTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.triggerChargesTime, float, activeExplosivesFireCompTemplate(),
                     setTriggerChargesTime, getTriggerChargesTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.detonatorObject, std::string, activeExplosivesFireCompTemplate(),
                     setDetonatorObject, getDetonatorObject, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(fire.showWeaponAfterReloadDelay, float, activeExplosivesFireCompTemplate(),
                     setShowWeaponAfterReloadDelay, getShowWeaponAfterReloadDelay,
                     kWeaponConsoleTag)

// AmmoCompTemplate's fourteen common properties.  The generated executors gate the abstract base
// implementation CID so both concrete ammo leaves inherit this surface.  Duration/fraction fields
// use the recovered setters: they preserve the clamped `changeMagAt` and the truncated 30 Hz caches.
BF2_CONSOLE_PROPERTY(ammo.autoReload, bool, activeAmmoCompTemplate(),
                     setAutoReload, getAutoReload, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.instantReloadOnEnable, bool, activeAmmoCompTemplate(),
                     setInstantReloadOnEnable, getInstantReloadOnEnable, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.ammoType, int, activeAmmoCompTemplate(),
                     setAmmoType, getAmmoType, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.magSize, int, activeAmmoCompTemplate(),
                     setMagSize, getMagSize, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.nrOfMags, int, activeAmmoCompTemplate(),
                     setNrOfMags, getNrOfMags, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.reloadAmount, int, activeAmmoCompTemplate(),
                     setReloadAmount, getReloadAmount, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.reloadTime, float, activeAmmoCompTemplate(),
                     setReloadTime, getReloadTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.firstShotExtraTime, float, activeAmmoCompTemplate(),
                     setExtraReloadTimeFirst, getFirstShotExtraTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.lastShotExtraTime, float, activeAmmoCompTemplate(),
                     setExtraReloadTimeLast, getLastShotExtraTime, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.changeMagAt, float, activeAmmoCompTemplate(),
                     setChangeMagAt, getChangeMagAt, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.magLinkWeapon, std::string, activeAmmoCompTemplate(),
                     setMagLinkWeapon, getMagLinkWeapon, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.minimumTimeUntilReload, float, activeAmmoCompTemplate(),
                     setMinimumTimeToReload, getMinimumTimeUntilReload, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.toggleWhenNoAmmo, bool, activeAmmoCompTemplate(),
                     setToggleWhenNoAmmo, getToggleWhenNoAmmo, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.reloadWithoutPlayer, bool, activeAmmoCompTemplate(),
                     setReloadWithoutPlayer, getReloadWithoutPlayer, kWeaponConsoleTag)

// ReplenishingAmmoCompTemplate's seven leaf properties.  abilityDrain/abilityStrength use their
// cache-updating setters; replenishingType is the recovered RTHeal/RTAmmo enum parser above.
BF2_CONSOLE_PROPERTY(ammo.abilityDrain, float, activeReplenishingAmmoCompTemplate(),
                     setAbilityDrain, getAbilityDrain, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.abilityCost, float, activeReplenishingAmmoCompTemplate(),
                     setAbilityCost, getAbilityCost, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.abilityMaterial, int, activeReplenishingAmmoCompTemplate(),
                     setAbilityMaterial, getAbilityMaterial, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.abilityRadius, float, activeReplenishingAmmoCompTemplate(),
                     setAbilityRadius, getAbilityRadius, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.abilityStrength, float, activeReplenishingAmmoCompTemplate(),
                     setAbilityStrength, getAbilityStrength, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.replenishingType, ReplenishingType, activeReplenishingAmmoCompTemplate(),
                     setReplenishingType, getReplenishingType, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(ammo.onlyActiveWhileFiring, bool, activeReplenishingAmmoCompTemplate(),
                     setOnlyActiveWhileFiring, getOnlyActiveWhileFiring, kWeaponConsoleTag)

// Common deviation surface. The generated executors resolve the base implementation through
// IID 50428/CID 38102, so all three leaves inherit it. Registration order is the PE's .CRT order;
// each grouped method preserves retail's non-obvious `maximum`-first argument order.
BF2_CONSOLE_METHOD_3(deviation.setFireDev, objTemplDeviationSetFireDev,
                     activeDeviationCompTemplate() != 0,
                     float, float, float, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.minDev, float, activeDeviationCompTemplate(),
                     setMinDev, getMinDev, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.subProjectileDev, float, activeDeviationCompTemplate(),
                     setSubProjectileDev, getSubProjectileDev, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.fireDevMax, float, activeDeviationCompTemplate(),
                     setFireDevMax, getFireDevMax, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.fireDevAdd, float, activeDeviationCompTemplate(),
                     setFireDevAdd, getFireDevAdd, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.fireDevSub, float, activeDeviationCompTemplate(),
                     setFireDevSub, getFireDevSub, kWeaponConsoleTag)

// TargetDeviation's one leaf field. Retail gates it through the same deviation IID and concrete
// CID 38126, then addresses the template's +0x20 radius.
BF2_CONSOLE_PROPERTY(deviation.radius, float, activeTargetDeviationCompTemplate(),
                     setRadius, getRadius, kWeaponConsoleTag)

// SoldierDeviationCompTemplate. The four-argument setters store maximum last in memory (at the
// highest field offset); the three-argument misc setter follows the same maximum-first contract.
BF2_CONSOLE_METHOD_4(deviation.setTurnDev, objTemplSoldierDeviationSetTurnDev,
                     activeSoldierDeviationCompTemplate() != 0,
                     float, float, float, float, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_4(deviation.setSpeedDev, objTemplSoldierDeviationSetSpeedDev,
                     activeSoldierDeviationCompTemplate() != 0,
                     float, float, float, float, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_3(deviation.setMiscDev, objTemplSoldierDeviationSetMiscDev,
                     activeSoldierDeviationCompTemplate() != 0,
                     float, float, float, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.turnDevAddPitch, float, activeSoldierDeviationCompTemplate(),
                     setTurnDevAddPitch, getTurnDevAddPitch, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.turnDevAddYaw, float, activeSoldierDeviationCompTemplate(),
                     setTurnDevAddYaw, getTurnDevAddYaw, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.turnDevSub, float, activeSoldierDeviationCompTemplate(),
                     setTurnDevSub, getTurnDevSub, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.turnDevMax, float, activeSoldierDeviationCompTemplate(),
                     setTurnDevMax, getTurnDevMax, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.speedDevAddForward, float, activeSoldierDeviationCompTemplate(),
                     setSpeedDevAddForward, getSpeedDevAddForward, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.speedDevAddStrafe, float, activeSoldierDeviationCompTemplate(),
                     setSpeedDevAddStrafe, getSpeedDevAddStrafe, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.speedDevSub, float, activeSoldierDeviationCompTemplate(),
                     setSpeedDevSub, getSpeedDevSub, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.speedDevMax, float, activeSoldierDeviationCompTemplate(),
                     setSpeedDevMax, getSpeedDevMax, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.miscDevAddJump, float, activeSoldierDeviationCompTemplate(),
                     setMiscDevAddJump, getMiscDevAddJump, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.miscDevSub, float, activeSoldierDeviationCompTemplate(),
                     setMiscDevSub, getMiscDevSub, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.miscDevMax, float, activeSoldierDeviationCompTemplate(),
                     setMiscDevMax, getMiscDevMax, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.devModStand, float, activeSoldierDeviationCompTemplate(),
                     setDevModStand, getDevModStand, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.devModCrouch, float, activeSoldierDeviationCompTemplate(),
                     setDevModCrouch, getDevModCrouch, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(deviation.devModLie, float, activeSoldierDeviationCompTemplate(),
                     setDevModLie, getDevModLie, kWeaponConsoleTag)
// [PLATFORM] Win32 registers devModZoom. The Linux ELF's corresponding string is the shorter
// deviation.devMod; this dedicated-server rebuild follows the PE console ABI.
BF2_CONSOLE_PROPERTY(deviation.devModZoom, float, activeSoldierDeviationCompTemplate(),
                     setDevModZoom, getDevModZoom, kWeaponConsoleTag)

// DefaultZoomCompTemplate.  PE generated executors gate the base two properties through
// IID 50440/CID 38104 and the seven leaf properties through IID 50440/CID 38117.
// The spelling mismatch is shipped: the standalone command registered here is `zoomInput`, while
// ZoomCompTemplate::makeScript emits `cameraInput`. Preserve both rather than normalising them.
BF2_CONSOLE_PROPERTY(zoom.zoomInput, io::PlayerInputMap, activeZoomCompTemplate(),
                     setCameraInput, getCameraInput, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.onlyZoomWhenProne, bool, activeZoomCompTemplate(),
                     setOnlyZoomWhenProne, getOnlyZoomWhenProne, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(zoom.addZoomFactor, objTemplZoomAddZoomFactor,
                            activeDefaultZoomCompTemplate() != 0, float, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.zoomDelay, float, activeDefaultZoomCompTemplate(),
                     setZoomDelay, getZoomDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.zoomLod, int, activeDefaultZoomCompTemplate(),
                     setZoomLod, getZoomLod, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.changeFovDelay, float, activeDefaultZoomCompTemplate(),
                     setChangeFovDelay, getChangeFovDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.zoomOutAfterFire, bool, activeDefaultZoomCompTemplate(),
                     setZoomOutAfterFire, getZoomOutAfterFire, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.mouseSpeedMod, float, activeDefaultZoomCompTemplate(),
                     setMouseSpeedMod, getMouseSpeedMod, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.disableMuzzleWhenZoomed, bool, activeDefaultZoomCompTemplate(),
                     setDisableMuzzleWhenZoomed, getDisableMuzzleWhenZoomed, kWeaponConsoleTag)

// ToggleCameraCompTemplate's six camera-switch properties (IID 50440/CID 38118).
// [CHOSEN, not recovered] The shipped metadata registers both view modes as CameraViewMode, but the
// accessors store them as int (and the sibling cameraId commands are int). int is kept so each
// command binds the accessor it calls; the enum's identifiers remain unresolved.
BF2_CONSOLE_PROPERTY(zoom.changeCameraId, int, activeToggleCameraCompTemplate(),
                     setChangeCameraId, getChangeCameraId, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.changeCameraViewMode, int, activeToggleCameraCompTemplate(),
                     setChangeCameraViewMode, getChangeCameraViewMode, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.startCameraId, int, activeToggleCameraCompTemplate(),
                     setStartCameraId, getStartCameraId, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.startCameraViewMode, int, activeToggleCameraCompTemplate(),
                     setStartCameraViewMode, getStartCameraViewMode, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.useProjectileCamera, bool, activeToggleCameraCompTemplate(),
                     setUseProjectileCamera, getUseProjectileCamera, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(zoom.allowUntoggledFire, bool, activeToggleCameraCompTemplate(),
                     setAllowUntoggledFire, getAllowUntoggledFire, kWeaponConsoleTag)

// SoldierBasedRecoilCompTemplate's eight properties come first in the retail registration order.
// The following six WeaponBased properties resolve the same recoil-template secondary identity
// (IID 50424), then require their respective concrete CID. Cache-updating setters retain the
// derived periods and recoil graph consumed by the runtimes.
BF2_CONSOLE_PROPERTY(recoil.hasRecoilForce, bool, activeSoldierBasedRecoilCompTemplate(),
                     setHasRecoilForce, getHasRecoilForce, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.goBackOnRecoil, bool, activeSoldierBasedRecoilCompTemplate(),
                     setGoBackOnRecoil, getGoBackOnRecoil, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilForceUp, RandomDistribution,
                     activeSoldierBasedRecoilCompTemplate(),
                     setRecoilForceUp, getRecoilForceUp, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilForceLeftRight, RandomDistribution,
                     activeSoldierBasedRecoilCompTemplate(),
                     setRecoilForceLeftRight, getRecoilForceLeftRight, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.zoomModifier, float, activeSoldierBasedRecoilCompTemplate(),
                     setZoomModifier, getZoomModifier, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilGraphFrameCount, int,
                     activeSoldierBasedRecoilCompTemplate(),
                     setRecoilGraphNumFrames, getRecoilGraphFrameCount, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilGraphExponent, float,
                     activeSoldierBasedRecoilCompTemplate(),
                     setRecoilGraphExponent, getRecoilGraphExponent, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilGraphTotalMovement, float,
                     activeSoldierBasedRecoilCompTemplate(),
                     setRecoilGraphTotalMove, getRecoilGraphTotalMovement, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY(recoil.recoilSize, float, activeWeaponBasedRecoilCompTemplate(),
                     setRecoilSize, getRecoilSize, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilSpeed, float, activeWeaponBasedRecoilCompTemplate(),
                     setRecoilSpeed, getRecoilSpeed, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilSpeedGB, float, activeWeaponBasedRecoilCompTemplate(),
                     setRecoilSpeedGB, getRecoilSpeedGB, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.recoilForce, float, activeWeaponBasedRecoilCompTemplate(),
                     setRecoilForce, getRecoilForce, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.cameraRecoilSpeed, float, activeWeaponBasedRecoilCompTemplate(),
                     setCameraRecoilSpeed, getCameraRecoilSpeed, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(recoil.cameraRecoilSize, float, activeWeaponBasedRecoilCompTemplate(),
                     setCameraRecoilSize, getCameraRecoilSize, kWeaponConsoleTag)

// AnimationCompTemplate and DefaultAnimationCompTemplate.  The generated PE constructors occur
// in this order (property strings 0x79A9E4..0x79ABC4); the base command resolves IID 50436/CID
// 38105 and the three leaf commands require concrete CID 38120.
BF2_CONSOLE_PROPERTY(animation.animateOnZoom, bool, activeAnimationCompTemplate(),
                     setAnimateOnZoom, getAnimateOnZoom, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(animation.useShiftAnimation, bool, activeDefaultAnimationCompTemplate(),
                     setUseShiftAnimation, getUseShiftAnimation, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(animation.shiftDelay, float, activeDefaultAnimationCompTemplate(),
                     setShiftDelay, getShiftDelay, kWeaponConsoleTag)
// [CHOSEN, not recovered] The shipped metadata registers loopingFire as float, but the accessor is
// a bool; bool is kept so the command binds the accessor it calls.
BF2_CONSOLE_PROPERTY(animation.loopingFire, bool, activeDefaultAnimationCompTemplate(),
                     setLoopingFire, getLoopingFire, kWeaponConsoleTag)

// Target component family in retail registration order. The base PE executors resolve
// IID 50444/CID 38106; the three leaf groups require their concrete CIDs. The two-argument
// onLockChangeToWeapon command is the shipped bool/string method rather than a scalar property.
BF2_CONSOLE_PROPERTY(target.targetInput, io::PlayerInputMap, activeTargetCompTemplate(),
                     setTargetInput, getTargetInput, kWeaponConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(target.onLockChangeToWeapon,
                            objTemplTargetSetOnLockChangeToWeapon,
                            activeTargetCompTemplate() != 0,
                            bool, std::string, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.targetSystem, TargetSystem, activeTargetCompTemplate(),
                     setTargetSystem, getTargetSystem, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.maxDistance, float, activeTargetCompTemplate(),
                     setMaxDistance, getMaxDistance, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.setTargetAtMaxDistance, bool, activeTargetCompTemplate(),
                     setSetTargetAtMaxDistance, getSetTargetAtMaxDistance, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.connectionEffect, std::string, activeTargetCompTemplate(),
                     setConnectionEffect, getConnectionEffect, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.connectionEffectOnlyWhenZoomed, bool, activeTargetCompTemplate(),
                     setConnectionEffectOnlyWhenZoomed, getConnectionEffectOnlyWhenZoomed,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.useCameraDirectionToTarget, bool, activeTargetCompTemplate(),
                     setUseCameraDirectionToTarget, getUseCameraDirectionToTarget,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.sendVehicleLockWarning, bool, activeTargetCompTemplate(),
                     setSendVehicleLockWarning, getSendVehicleLockWarning, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY(target.lockDelay, float, activeMultiFireArmTargetCompTemplate(),
                     setLockDelay, getLockDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.loseLockDelay, float, activeMultiFireArmTargetCompTemplate(),
                     setLoseLockDelay, getLoseLockDelay, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.lockAngle, float, activeMultiFireArmTargetCompTemplate(),
                     setLockAngle, getLockAngle, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY(target.targetObjectTemplate, std::string, activeLPTargetCompTemplate(),
                     setTargetObjectTemplate, getTargetObjectTemplateName, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.updateInterval, float, activeLPTargetCompTemplate(),
                     setUpdateInterval, getUpdateInterval, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.disableFireWhenActiveMissile, bool, activeLPTargetCompTemplate(),
                     setDisableFireWhenActiveMissile, getDisableFireWhenActiveMissile,
                     kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.disableGuidanceOnReload, bool, activeLPTargetCompTemplate(),
                     setDisableGuidanceOnReload, getDisableGuidanceOnReload, kWeaponConsoleTag)

BF2_CONSOLE_PROPERTY(target.useDynamicVelocity, bool, activeSimpleTargetCompTemplate(),
                     setUseDynamicVelocity, getUseDynamicVelocity, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.minVelocity, float, activeSimpleTargetCompTemplate(),
                     setMinVelocity, getMinVelocity, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.maxVelocity, float, activeSimpleTargetCompTemplate(),
                     setMaxVelocity, getMaxVelocity, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(target.velocityModifier, float, activeSimpleTargetCompTemplate(),
                     setVelocityModifier, getVelocityModifier, kWeaponConsoleTag)

// DefaultSoundCompTemplate. The generated PE executors gate both properties through
// IID 50432/CID 38119 and address the template's own +0x0C/+0x10 fields through these accessors.
BF2_CONSOLE_PROPERTY(sound.noisy, bool, activeDefaultSoundCompTemplate(),
                     setNoisy, getNoisy, kWeaponConsoleTag)
BF2_CONSOLE_PROPERTY(sound.maxSoundsPerBurst, int, activeDefaultSoundCompTemplate(),
                     setMaxSoundsPerBurst, getMaxSoundsPerBurst, kWeaponConsoleTag)

// Class-gated firearm properties for equipment type, deployment and HUD behavior.
BF2_CONSOLE_FIELD(weaponType,          int,   activeFireArm(), m_weaponType,          kWeaponConsoleTag)
BF2_CONSOLE_FIELD(isGasMask,           bool,  activeFireArm(), m_isGasMask,           kWeaponConsoleTag)
BF2_CONSOLE_FIELD(isNightVision,       bool,  activeFireArm(), m_isNightVision,       kWeaponConsoleTag)
BF2_CONSOLE_FIELD(deployAnimationTime, float, activeFireArm(), m_deployAnimationTime, kWeaponConsoleTag)
BF2_CONSOLE_FIELD(canFireGuiIndex,     int,   activeFireArm(), m_canFireGuiIndex,     kWeaponConsoleTag)
BF2_CONSOLE_FIELD(cannotFireGuiIndex,  int,   activeFireArm(), m_cannotFireGuiIndex,  kWeaponConsoleTag)
BF2_CONSOLE_FIELD(cannotFireTimer,     float, activeFireArm(), m_cannotFireTimer,     kWeaponConsoleTag)
BF2_CONSOLE_FIELD(crosshairFireTime,   float, activeFireArm(), m_crosshairFireTime,   kWeaponConsoleTag)

} // namespace world
} // namespace hfe
} // namespace dice
