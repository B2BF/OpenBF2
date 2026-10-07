// Vehicle template console bindings for drivetrain, rotation and suspension properties.
// [CHOSEN, not recovered] Macros replace generated property classes while preserving their public behavior.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"     // world::getActiveTemplate
#include "../EngineTemplate.h"
#include "../TemplateLeaves.h"                        // SpringTemplate - the suspension pair
#include "../RotationalBundleLeaves.h"                // WingTemplate
#include "../LandingGearTemplate.h"
#include "../ForceObjectTemplate.h"
#include "../../../IO/Input/Input.h"                  // io::PlayerInputMap stream conversion
#include "../../../Core/Ids.h"                            // CID_EngineTemplate

namespace dice {
namespace hfe {
namespace world {

// Zero-layout bridge for engine properties whose original generated executors read and write the
// template fields directly rather than calling public accessor slots.
struct VehicleTemplateConsoleAccess
{
    // newCar2: ELF ConsoleClass112..133 (122/128 are unused), +0x620..0x66C;
    // PE executors 0x521A30..0x523BB0, +0x6D0..0x71C. Direct stores,
    // no clamps; useClutchedTorque is the one bool among nineteen floats.
    static float getNewCarMinRpm(EngineTemplate* value) { return value->m_newCar2MinRpm; }
    static void setNewCarMinRpm(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2MinRpm = v; }
    static float getNewCarMaxRpm(EngineTemplate* value) { return value->m_newCar2MaxRpm; }
    static void setNewCarMaxRpm(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2MaxRpm = v; }
    static bool getNewCarUseClutchedTorque(EngineTemplate* value) { return value->m_newCar2UseClutchedTorque; }
    static void setNewCarUseClutchedTorque(bool v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2UseClutchedTorque = v; }
    static float getNewCarWheelInertia(EngineTemplate* value) { return value->m_newCar2WheelInertia; }
    static void setNewCarWheelInertia(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelInertia = v; }
    static float getNewCarEngineFeedbackMod(EngineTemplate* value) { return value->m_newCar2EngineFeedbackMod; }
    static void setNewCarEngineFeedbackMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2EngineFeedbackMod = v; }
    static float getNewCarBrakeTorque(EngineTemplate* value) { return value->m_newCar2BrakeTorque; }
    static void setNewCarBrakeTorque(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2BrakeTorque = v; }
    static float getNewCarEngineBrakeTorque(EngineTemplate* value) { return value->m_newCar2EngineBrakeTorque; }
    static void setNewCarEngineBrakeTorque(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2EngineBrakeTorque = v; }
    static float getNewCarFrictionTorque(EngineTemplate* value) { return value->m_newCar2FrictionTorque; }
    static void setNewCarFrictionTorque(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2FrictionTorque = v; }
    static float getNewCarFrictionMod(EngineTemplate* value) { return value->m_newCar2FrictionMod; }
    static void setNewCarFrictionMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2FrictionMod = v; }
    static float getNewCarSlopeTorqueMod(EngineTemplate* value) { return value->m_newCar2SlopeTorqueMod; }
    static void setNewCarSlopeTorqueMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2SlopeTorqueMod = v; }
    static float getNewCarWheelLongFrictionMod(EngineTemplate* value) { return value->m_newCar2WheelLongFrictionMod; }
    static void setNewCarWheelLongFrictionMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLongFrictionMod = v; }
    static float getNewCarWheelLongDriveFrictionMod(EngineTemplate* value) { return value->m_newCar2WheelLongDriveFrictionMod; }
    static void setNewCarWheelLongDriveFrictionMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLongDriveFrictionMod = v; }
    static float getNewCarWheelLongPeakAt(EngineTemplate* value) { return value->m_newCar2WheelLongPeakAt; }
    static void setNewCarWheelLongPeakAt(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLongPeakAt = v; }
    static float getNewCarWheelLongMinDynamicFriction(EngineTemplate* value) { return value->m_newCar2WheelLongMinDynamicFriction; }
    static void setNewCarWheelLongMinDynamicFriction(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLongMinDynamicFriction = v; }
    static float getNewCarWheelLongScale(EngineTemplate* value) { return value->m_newCar2WheelLongScale; }
    static void setNewCarWheelLongScale(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLongScale = v; }
    static float getNewCarWheelLatFrictionMod(EngineTemplate* value) { return value->m_newCar2WheelLatFrictionMod; }
    static void setNewCarWheelLatFrictionMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLatFrictionMod = v; }
    static float getNewCarWheelLatDriveFrictionMod(EngineTemplate* value) { return value->m_newCar2WheelLatDriveFrictionMod; }
    static void setNewCarWheelLatDriveFrictionMod(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLatDriveFrictionMod = v; }
    static float getNewCarWheelLatPeakAt(EngineTemplate* value) { return value->m_newCar2WheelLatPeakAt; }
    static void setNewCarWheelLatPeakAt(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLatPeakAt = v; }
    static float getNewCarWheelLatMinDynamicFriction(EngineTemplate* value) { return value->m_newCar2WheelLatMinDynamicFriction; }
    static void setNewCarWheelLatMinDynamicFriction(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLatMinDynamicFriction = v; }
    static float getNewCarWheelLatScale(EngineTemplate* value) { return value->m_newCar2WheelLatScale; }
    static void setNewCarWheelLatScale(float v) { static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_newCar2WheelLatScale = v; }
    // Stock supply_parachute authors these ForceObject settings. The generated executors are
    // direct float/byte reads and writes (ELF ConsoleClass205/207/208/209; PE 0x528150..0x528820).
    static ForceObjectTemplate* activeForce()
    {
        return static_cast<ForceObjectTemplate*>(getActiveTemplate(CID_ForceObjectTemplate));
    }
    static float getConstantForce() { return activeForce()->constantForce; }
    static void setConstantForce(float value) { activeForce()->constantForce = value; }
    static io::PlayerInputMap getForceInput() { return static_cast<io::PlayerInputMap>(activeForce()->input); }
    static void setForceInput(io::PlayerInputMap value) { activeForce()->input = value; }
    static float getForce() { return activeForce()->force; }
    static void setForce(float value) { activeForce()->force = value; }
    static bool getUseAbsoluteDirection() { return activeForce()->useAbsoluteDirection; }
    static void setUseAbsoluteDirection(bool value) { activeForce()->useAbsoluteDirection = value; }
    static bool getOnlyWhenOccupied() { return activeForce()->onlyWhenOccupied; }
    static void setOnlyWhenOccupied(bool value) { activeForce()->onlyWhenOccupied = value; }
    static float getForceStartDelay() { return activeForce()->startDelay; }
    static void setStartDelay(float value)
    {
        WingTemplate* wing = static_cast<WingTemplate*>(getActiveTemplate(CID_WingTemplate));
        if (wing)
            wing->m_startDelay = value;
        else
            activeForce()->startDelay = value;
    }

    // Aircraft console properties.
    static float get_noPropellerEffectAtSpeed(EngineTemplate* value) { return value->m_noPropellerEffectAtSpeed; }
    static void set_noPropellerEffectAtSpeed(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_noPropellerEffectAtSpeed = input;
    }
    static float get_attackSpeed(EngineTemplate* value) { return value->m_attackSpeed; }
    static void set_attackSpeed(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_attackSpeed = input;
    }
    static float get_horizontalDampAngle(EngineTemplate* value) { return value->m_horizontalDampAngle; }
    static void set_horizontalDampAngle(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_horizontalDampAngle = input;
    }
    static float get_horizontalDampAngleFactor(EngineTemplate* value) { return value->m_horizontalDampAngleFactor; }
    static void set_horizontalDampAngleFactor(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_horizontalDampAngleFactor = input;
    }
    static bool get_constantRpm(EngineTemplate* value) { return value->m_constantRpm; }
    static void set_constantRpm(bool input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_constantRpm = input;
    }
    static float get_spinWhenNoEngineInAir(EngineTemplate* value) { return value->m_spinWhenNoEngineInAir; }
    static void set_spinWhenNoEngineInAir(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_spinWhenNoEngineInAir = input;
    }
    static float get_dampHorizontalVel(EngineTemplate* value) { return value->m_dampHorizontalVel; }
    static void set_dampHorizontalVel(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_dampHorizontalVel = input;
    }
    static float get_dampHorizontalVelFactor(EngineTemplate* value) { return value->m_dampHorizontalVelFactor; }
    static void set_dampHorizontalVelFactor(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_dampHorizontalVelFactor = input;
    }
    static bool get_decreaseAngleToZero(EngineTemplate* value) { return value->m_decreaseAngleToZero; }
    static void set_decreaseAngleToZero(bool input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_decreaseAngleToZero = input;
    }
    static float get_decreaseAngleToZeroVerticalVel(EngineTemplate* value) { return value->m_decreaseAngleToZeroVerticalVel; }
    static void set_decreaseAngleToZeroVerticalVel(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_decreaseAngleToZeroVerticalVel = input;
    }
    static float get_decreaseAngleToZeroSpeed(EngineTemplate* value) { return value->m_decreaseAngleToZeroSpeed; }
    static void set_decreaseAngleToZeroSpeed(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_decreaseAngleToZeroSpeed = input;
    }
    static float get_airFlowAffect(EngineTemplate* value) { return value->m_airFlowAffect; }
    static void set_airFlowAffect(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_airFlowAffect = input;
    }
    static float get_noEffectAtPerpSpeed(EngineTemplate* value) { return value->m_noEffectAtPerpSpeed; }
    static void set_noEffectAtPerpSpeed(float input)
    {
        static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate))->m_noEffectAtPerpSpeed = input;
    }
    static io::PlayerInputMap getRemoteEngineInput(EngineTemplate* value)
    {
        return value->m_remoteEngineInput;
    }
    static void setRemoteEngineInput(EngineTemplate* value, io::PlayerInputMap input)
    {
        value->m_remoteEngineInput = input;
    }
    static bool getCanReverse(EngineTemplate* value)
    {
        return value->m_canReverse;
    }
    static void setCanReverse(EngineTemplate* value, bool canReverse)
    {
        value->m_canReverse = canReverse;
    }
    static float getGearUp(EngineTemplate* value) { return value->m_gearUp; }
    static void setGearUp(EngineTemplate* value, float gearUp) { value->m_gearUp = gearUp; }
    static float getGearDown(EngineTemplate* value) { return value->m_gearDown; }
    static void setGearDown(EngineTemplate* value, float gearDown) { value->m_gearDown = gearDown; }
    static float getGearChangeTime(EngineTemplate* value) { return value->m_gearChangeTime; }
    static void setGearChangeTime(EngineTemplate* value, float gearChangeTime)
    {
        if (gearChangeTime >= 100.0f)
            gearChangeTime = 100.0f;
        if (gearChangeTime < 0.1f)
            gearChangeTime = 0.1f;
        value->m_gearChangeTime = gearChangeTime;
    }
    static float getTrackTurnAcceleration(EngineTemplate* value)
    {
        return value->m_trackTurnAcceleration;
    }
    static void setTrackTurnAcceleration(EngineTemplate* value, float acceleration)
    {
        value->m_trackTurnAcceleration = acceleration;
    }
    static float getTrackTurnSpeed(EngineTemplate* value) { return value->m_trackTurnSpeed; }
    static void setTrackTurnSpeed(EngineTemplate* value, float speed)
    {
        value->m_trackTurnSpeed = speed;
    }
};

namespace {

// Stands in for the original's file-scope list (ELF 0x8B11904 - the vehicle module's own).
const char* const kObjTemplVehicleConsoleTag = "world.objTemplVehicle";

// The active-template gate every engine binding shares: the active template, but only if it IS-A
// EngineTemplate (queryImplementation's chain answers for the exact class here - EngineTemplate
// has no template subclasses in the shipped hierarchy).
EngineTemplate* activeEngine()
{
    return static_cast<EngineTemplate*>(getActiveTemplate(CID_EngineTemplate));
}

float objTemplGetGearUp()
{
    return VehicleTemplateConsoleAccess::getGearUp(activeEngine());
}
void objTemplSetGearUp(float value)
{
    VehicleTemplateConsoleAccess::setGearUp(activeEngine(), value);
}
float objTemplGetGearDown()
{
    return VehicleTemplateConsoleAccess::getGearDown(activeEngine());
}
void objTemplSetGearDown(float value)
{
    VehicleTemplateConsoleAccess::setGearDown(activeEngine(), value);
}
float objTemplGetGearChangeTime()
{
    return VehicleTemplateConsoleAccess::getGearChangeTime(activeEngine());
}
void objTemplSetGearChangeTime(float value)
{
    VehicleTemplateConsoleAccess::setGearChangeTime(activeEngine(), value);
}
float objTemplGetTrackTurnAcceleration()
{
    return VehicleTemplateConsoleAccess::getTrackTurnAcceleration(activeEngine());
}
void objTemplSetTrackTurnAcceleration(float value)
{
    VehicleTemplateConsoleAccess::setTrackTurnAcceleration(activeEngine(), value);
}
float objTemplGetTrackTurnSpeed()
{
    return VehicleTemplateConsoleAccess::getTrackTurnSpeed(activeEngine());
}
void objTemplSetTrackTurnSpeed(float value)
{
    VehicleTemplateConsoleAccess::setTrackTurnSpeed(activeEngine(), value);
}

// The same gate for the suspension pair below - SpringTemplate has no template subclasses either.
SpringTemplate* activeSpring()
{
    return static_cast<SpringTemplate*>(getActiveTemplate(CID_SpringTemplate));
}

WingTemplate* activeWing()
{
    return static_cast<WingTemplate*>(getActiveTemplate(CID_WingTemplate));
}

LandingGearTemplate* activeLandingGear()
{
    return static_cast<LandingGearTemplate*>(getActiveTemplate(CID_LandingGearTemplate));
}

// The Rotor rows' gate (ConsoleClass171..176 isObjectActive: getActiveTemplate(CID_RotorTemplate)).
RotorTemplate* activeRotor()
{
    return static_cast<RotorTemplate*>(getActiveTemplate(CID_RotorTemplate));
}

// ConsoleClass172 `tolerance` (ELF exec 0x87A3360) - NOT a plain field row: the SET stores half
// the argument (`fmul 0.5`) and the GET hands back twice the field (`fadd st, st`), so the
// template keeps the half-width of the hysteresis band that Rotor::handleVisualUpdate uses.
float getRotorTolerance()
{
    const float half = activeRotor()->m_tolerance;
    return half + half;
}
void setRotorTolerance(float value)
{
    activeRotor()->m_tolerance = value * 0.5f;
}

} // namespace

// Establish this translation unit's ObjectTemplate physics-property defaults before registering bindings.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 1) // fullAccess, COCPhysics

// Engine-type constants must be registered before integer property parsing.
BF2_CONSOLE_PROPERTY(engineType, int, activeEngine(), setEngineType, getEngineType, kObjTemplVehicleConsoleTag)

// Propeller rotation axis (EngineTemplate, dedicated PE executor writes the raw field via the
// template's accessor pair) and the spring axis-fixation flag.
BF2_CONSOLE_PROPERTY(propRotAxis, int, activeEngine(), setPropRotAxis, getPropRotAxis,
                     kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(fixAxisSpring, bool, activeSpring(), m_fixAxisSpring, kObjTemplVehicleConsoleTag)

// The gear block - the three named dwords of the six-dword gap between the two
// OverTimeDistributions (EngineTemplate.h +0x3C8/+0x3CC/+0x3D0, names proven by the engine
// objects that consume them: getGearRatio walks curveB over g / numberOfGears, getCurrentRatio =
// 3.5 * differential / gearRatio, getTorque = curveA(x) * torque). Plain field stores - the
// retail 17-byte setters are one-dword writes, the RotationalBundle precedent.
BF2_CONSOLE_FIELD(numberOfGears, int,   activeEngine(), m_numberOfGears, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(differential,  float, activeEngine(), m_differential,  kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(torque,        float, activeEngine(), m_torque,        kObjTemplVehicleConsoleTag)

// executor PE 0x51D350: five float arguments.
BF2_CONSOLE_METHOD_5(setGearRatios, activeEngine()->setGearRatios, activeEngine() != 0,
                     float, float, float, float, float, kObjTemplVehicleConsoleTag)

// Keep the authored drivetrain, tyre and braking parameters instead of constructor defaults.
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.minRpm", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarMinRpm(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarMinRpm, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.maxRpm", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarMaxRpm(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarMaxRpm, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.useClutchedTorque", bool, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarUseClutchedTorque(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarUseClutchedTorque, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelInertia", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelInertia(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelInertia, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.engineFeedbackMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarEngineFeedbackMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarEngineFeedbackMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.brakeTorque", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarBrakeTorque(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarBrakeTorque, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.engineBrakeTorque", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarEngineBrakeTorque(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarEngineBrakeTorque, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.frictionTorque", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarFrictionTorque(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarFrictionTorque, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.frictionMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarFrictionMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarFrictionMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.slopeTorqueMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarSlopeTorqueMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarSlopeTorqueMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLongFrictionMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLongFrictionMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLongFrictionMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLongDriveFrictionMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLongDriveFrictionMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLongDriveFrictionMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLongPeakAt", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLongPeakAt(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLongPeakAt, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLongMinDynamicFriction", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLongMinDynamicFriction(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLongMinDynamicFriction, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLongScale", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLongScale(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLongScale, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLatFrictionMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLatFrictionMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLatFrictionMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLatDriveFrictionMod", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLatDriveFrictionMod(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLatDriveFrictionMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLatPeakAt", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLatPeakAt(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLatPeakAt, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLatMinDynamicFriction", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLatMinDynamicFriction(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLatMinDynamicFriction, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN_NAMED("newCar2.wheelLatScale", float, activeEngine() != 0,
    VehicleTemplateConsoleAccess::getNewCarWheelLatScale(activeEngine()),
    VehicleTemplateConsoleAccess::setNewCarWheelLatScale, kObjTemplVehicleConsoleTag)

// The retail static ObjectTemplate console block stores these three floats directly. They are
// authored on essentially every stock vehicle. `setGear*` content spellings resolve to the
// registered prefix-less names below through the console's shipped set-prefix synthesis.
BF2_CONSOLE_PROPERTY_FN(gearUp, float, activeEngine() != 0,
                        objTemplGetGearUp(), objTemplSetGearUp, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(gearDown, float, activeEngine() != 0,
                        objTemplGetGearDown(), objTemplSetGearDown, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(gearChangeTime, float, activeEngine() != 0,
                        objTemplGetGearChangeTime(), objTemplSetGearChangeTime,
                        kObjTemplVehicleConsoleTag)

// Engine input bindings. accumulate takes a float but stores a Boolean.
namespace {
io::PlayerInputMap objTemplGetRemoteEngineInput()
{
    return VehicleTemplateConsoleAccess::getRemoteEngineInput(activeEngine());
}
void objTemplSetRemoteEngineInput(io::PlayerInputMap input)
{
    VehicleTemplateConsoleAccess::setRemoteEngineInput(activeEngine(), input);
}
bool objTemplGetCanReverse()
{
    return VehicleTemplateConsoleAccess::getCanReverse(activeEngine());
}
void objTemplSetCanReverse(bool canReverse)
{
    VehicleTemplateConsoleAccess::setCanReverse(activeEngine(), canReverse);
}
} // namespace

BF2_CONSOLE_PROPERTY(accumulate, float, activeEngine(), setAccumulate, getAccumulate, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(canReverse, bool, activeEngine() != 0, objTemplGetCanReverse(), objTemplSetCanReverse, kObjTemplVehicleConsoleTag)

// The tracked-vehicle pair follows canReverse in the retail table. Tanks replace the ctor defaults
// (120 / 1.5) with acceleration 30 and speed 0.75..0.85.
BF2_CONSOLE_PROPERTY_FN(trackTurnAcceleration, float, activeEngine() != 0,
                        objTemplGetTrackTurnAcceleration(), objTemplSetTrackTurnAcceleration,
                        kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(trackTurnSpeed, float, activeEngine() != 0,
                        objTemplGetTrackTurnSpeed(), objTemplSetTrackTurnSpeed,
                        kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(remoteEngineInput, io::PlayerInputMap, activeEngine() != 0,
                        objTemplGetRemoteEngineInput(), objTemplSetRemoteEngineInput,
                        kObjTemplVehicleConsoleTag)

// Aircraft propulsion/control properties. The existing field/virtual split is preserved;
// authored damping, lift and gear values must not silently fall back to constructor defaults.
// ELF ConsoleClass77..80; PE 0x51E1D0/0x51E390/0x51E550/0x51E710.
BF2_CONSOLE_PROPERTY(pureRotational, bool, activeEngine(), setPureRotational, getPureRotational, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY(horizontalSpeedMagnifier, float, activeEngine(), setHorizontalSpeedMagnifier, getHorizontalSpeedMagnifier, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY(defaultAngleOfAttack, float, activeEngine(), setDefaultAngleOfAttack, getDefaultAngleOfAttack, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY(maxAngleOfAttack, float, activeEngine(), setMaxAngleOfAttack, getMaxAngleOfAttack, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(noPropellerEffectAtSpeed, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_noPropellerEffectAtSpeed(activeEngine()),
                        VehicleTemplateConsoleAccess::set_noPropellerEffectAtSpeed, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(attackSpeed, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_attackSpeed(activeEngine()),
                        VehicleTemplateConsoleAccess::set_attackSpeed, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(horizontalDampAngle, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_horizontalDampAngle(activeEngine()),
                        VehicleTemplateConsoleAccess::set_horizontalDampAngle, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(horizontalDampAngleFactor, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_horizontalDampAngleFactor(activeEngine()),
                        VehicleTemplateConsoleAccess::set_horizontalDampAngleFactor, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(constantRpm, bool, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_constantRpm(activeEngine()),
                        VehicleTemplateConsoleAccess::set_constantRpm, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(spinWhenNoEngineInAir, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_spinWhenNoEngineInAir(activeEngine()),
                        VehicleTemplateConsoleAccess::set_spinWhenNoEngineInAir, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(dampHorizontalVel, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_dampHorizontalVel(activeEngine()),
                        VehicleTemplateConsoleAccess::set_dampHorizontalVel, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(dampHorizontalVelFactor, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_dampHorizontalVelFactor(activeEngine()),
                        VehicleTemplateConsoleAccess::set_dampHorizontalVelFactor, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(decreaseAngleToZero, bool, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_decreaseAngleToZero(activeEngine()),
                        VehicleTemplateConsoleAccess::set_decreaseAngleToZero, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(decreaseAngleToZeroVerticalVel, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_decreaseAngleToZeroVerticalVel(activeEngine()),
                        VehicleTemplateConsoleAccess::set_decreaseAngleToZeroVerticalVel, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(decreaseAngleToZeroSpeed, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_decreaseAngleToZeroSpeed(activeEngine()),
                        VehicleTemplateConsoleAccess::set_decreaseAngleToZeroSpeed, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(airFlowAffect, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_airFlowAffect(activeEngine()),
                        VehicleTemplateConsoleAccess::set_airFlowAffect, kObjTemplVehicleConsoleTag)

BF2_CONSOLE_PROPERTY_FN(noEffectAtPerpSpeed, float, activeEngine() != 0,
                        VehicleTemplateConsoleAccess::get_noEffectAtPerpSpeed(activeEngine()),
                        VehicleTemplateConsoleAccess::set_noEffectAtPerpSpeed, kObjTemplVehicleConsoleTag)

// LandingGear ConsoleClass159..165: six floats and one bool, all direct stores.
// ..0x87A2320; PE 0x5252E0..0x525D60. Delay declaration order differs
// from field order: down is ELF +0x228 / PE +0x2D8; up is +0x224 / +0x2D4.
BF2_CONSOLE_FIELD(gearDownHeight, float, activeLandingGear(), m_gearDownHeight, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(gearUpHeight, float, activeLandingGear(), m_gearUpHeight, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(gearDownSpeed, float, activeLandingGear(), m_gearDownSpeed, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(gearUpSpeed, float, activeLandingGear(), m_gearUpSpeed, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(gearDownDelay, float, activeLandingGear(), m_gearDownDelay, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(gearUpDelay, float, activeLandingGear(), m_gearUpDelay, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(invertGear, bool, activeLandingGear(), m_invertGear, kObjTemplVehicleConsoleTag)
// ELF ConsoleClass166 0x87A2890 / PE 0x525F10; registered as debugForceGearUp.
// Unlike the seven instance properties this writes the shared LandingGearTemplate flag.
BF2_CONSOLE_FIELD(debugForceGearUp, bool, activeLandingGear(), m_forceGearUp, kObjTemplVehicleConsoleTag)

// Rotor properties select its engine, blade/disc threshold and rotation axis and rates.
BF2_CONSOLE_FIELD(changeLodAt, float, activeRotor(), m_changeLodAt, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(tolerance, float, activeRotor() != 0, getRotorTolerance(),
                        setRotorTolerance, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(engineName, std::string, activeRotor(), m_engineName, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(rotationSpeedMod, float, activeRotor(), m_rotationSpeedMod, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(rotationSpeedModBlur, float, activeRotor(), m_rotationSpeedModBlur, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(rotationAxle, int, activeRotor() != 0, activeRotor()->m_rotationAxle,
                        activeRotor()->setRotationAxle, kObjTemplVehicleConsoleTag)

// Wing ConsoleClass191..198, in retail declaration order. setRegulateToLift also
// enables the regulator; setWingToRegulatorRatio does not. The Vec3 remains three
// signed floats, using the shared slash-delimited stream conversion.

BF2_CONSOLE_FIELD(pitchOffset, float, activeWing(), m_pitchOffset, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(positionOffset, Vec3, activeWing(), m_positionOffset, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(wingLift, float, activeWing(), m_wingLift, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(flapLift, float, activeWing(), m_flapLift, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(liftRegulated, bool, activeWing(), m_liftRegulated, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(regulateToLift, float, activeWing() != 0, activeWing()->m_regulateToLift,
                        activeWing()->setRegulateToLift, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(wingToRegulatorRatio, float, activeWing() != 0, activeWing()->m_wingToRegulatorRatio,
                        activeWing()->setWingToRegulatorRatio, kObjTemplVehicleConsoleTag)
// Retail has separate Wing and ForceObject executors under the same console name. The shared
// console reduction combines their active-template gates without changing either field's meaning.
BF2_CONSOLE_PROPERTY_FN(startDelay, float,
                        activeWing() != 0 || VehicleTemplateConsoleAccess::activeForce() != 0,
                        activeWing() ? activeWing()->m_startDelay : VehicleTemplateConsoleAccess::getForceStartDelay(),
                        VehicleTemplateConsoleAccess::setStartDelay, kObjTemplVehicleConsoleTag)
// Force input and magnitude: ELF ConsoleClass202/203 (0x87A8410/0x87A8970),
// /0x527D20. Required by the stock parachute's throttle-driven force children.
BF2_CONSOLE_PROPERTY_FN(input, io::PlayerInputMap, VehicleTemplateConsoleAccess::activeForce() != 0,
                        VehicleTemplateConsoleAccess::getForceInput(),
                        VehicleTemplateConsoleAccess::setForceInput, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(force, float, VehicleTemplateConsoleAccess::activeForce() != 0,
                        VehicleTemplateConsoleAccess::getForce(),
                        VehicleTemplateConsoleAccess::setForce, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(constantForce, float, VehicleTemplateConsoleAccess::activeForce() != 0,
                        VehicleTemplateConsoleAccess::getConstantForce(),
                        VehicleTemplateConsoleAccess::setConstantForce, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(useAbsoluteDirection, bool, VehicleTemplateConsoleAccess::activeForce() != 0,
                        VehicleTemplateConsoleAccess::getUseAbsoluteDirection(),
                        VehicleTemplateConsoleAccess::setUseAbsoluteDirection, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY_FN(onlyWhenOccupied, bool, VehicleTemplateConsoleAccess::activeForce() != 0,
                        VehicleTemplateConsoleAccess::getOnlyWhenOccupied(),
                        VehicleTemplateConsoleAccess::setOnlyWhenOccupied, kObjTemplVehicleConsoleTag)

// Suspension strength and damping properties. The console accepts set-prefixed aliases.
BF2_CONSOLE_PROPERTY(strength, float, activeSpring(), setStrength, getStrength, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_PROPERTY(damping,  float, activeSpring(), setDamping,  getDamping,  kObjTemplVehicleConsoleTag)

// FloatingBundle ConsoleClass148..151: direct float stores, with no range clamp or side effect.

// The stock RIB and amphibious hulls author the lift endpoints through the set-prefix aliases.
static FloatingBundleTemplate* activeFloater()
{
    return static_cast<FloatingBundleTemplate*>(getActiveTemplate(CID_FloatingBundleTemplate));
}
BF2_CONSOLE_FIELD(waterHeight, float, activeFloater(), m_waterHeight, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(hullHeight, float, activeFloater(), m_hullHeight, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(floatMaxLift, float, activeFloater(), m_floatMaxLift, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(floatMinLift, float, activeFloater(), m_floatMinLift, kObjTemplVehicleConsoleTag)
// ELF ConsoleClass152/153 0x879F820/0x879FD80, PE 0x524F40/0x525100.
// This float dragModifier belongs to FloatingBundle, distinct from SimpleObject's Vec3.
BF2_CONSOLE_FIELD(dragModifier, float, activeFloater(), m_dragModifier, kObjTemplVehicleConsoleTag)
BF2_CONSOLE_FIELD(sinkingSpeedMod, float, activeFloater(), m_sinkingSpeedMod, kObjTemplVehicleConsoleTag)

// Handed to the console registry by GameLogicObjTemplVehicleModule::init, the way the original
// registers 0x8B11904.
ConsoleObjectList& objTemplVehicleConsoleObjects()
{
    return consoleBindingList(kObjTemplVehicleConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
