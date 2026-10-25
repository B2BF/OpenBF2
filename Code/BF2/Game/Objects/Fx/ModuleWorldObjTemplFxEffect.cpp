// [INFERRED] split of attested Game/Objects/Fx/ModuleWorldObjTemplFx.cpp.
// ELF ConsoleClass474..563/572; full executors and registration metadata read.
// Client PE examples: TTL 571357, startAtCreation 571976, properties 571D02,
// emit-frequency bound 575E48, water threshold 57660D, enableParticles 57672A.
// These bindings share the existing module's list; they introduce no new module or ABI.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Ids.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"
#include "EffectBundleTemplate.h"
#include "AirDraftEffectBundleTemplate.h"
#include "WheelEffectBundleTemplate.h"
#include <cstring>
#include "Emitter.h"                         // Emitter::mDebugNoOldParticles - enableOldParticles

namespace dice { namespace hfe { namespace world {
extern bool g_effectsDisabled;
namespace {
const char* const kEffectConsoleTag = "world.objTemplFx";
EffectBundleTemplate* activeEffectBundle()
{
    return static_cast<EffectBundleTemplate*>(getActiveTemplate(CID_EffectBundleTemplate));
}
// ELF 86ACE30: preserve the repeated lookup and EffectBundle -> AirDraft -> Wheel ordering.
EffectProperties* activeEffectProperties()
{
    if (EffectBundleTemplate* value = activeEffectBundle())
        return value;
    if (AirDraftEffectBundleTemplate* value = static_cast<AirDraftEffectBundleTemplate*>(
            getActiveTemplate(CID_AirDraftEffectBundleTemplate)))
        return value;
    return static_cast<WheelEffectBundleTemplate*>(getActiveTemplate(CID_WheelEffectBundleTemplate));
}
// The air-draft bundle is the only class gated by CID_AirDraftEffectBundleTemplate; its
// engineInfluence / startDistance / maxDistance floats are the PE block behind that gate.
AirDraftEffectBundleTemplate* activeAirDraftEffectBundle()
{
    return static_cast<AirDraftEffectBundleTemplate*>(
        getActiveTemplate(CID_AirDraftEffectBundleTemplate));
}
void setAirDraftEngineInfluence(float v) { activeAirDraftEffectBundle()->m_field2C4 = v; }
void setAirDraftStartDistance(float v) { activeAirDraftEffectBundle()->m_field2C8 = v; }
void setAirDraftMaxDistance(float v) { activeAirDraftEffectBundle()->m_field2CC = v; }
void addWorkOnMaterial(int material) { activeEffectBundle()->m_vec318.push_back(material); }
void setEffectWaterHeight(float value)
{
    std::memcpy(&activeEffectBundle()->m_field33C, &value, sizeof(value));
}
float getEffectWaterHeight()
{
    float value;
    std::memcpy(&value, &activeEffectBundle()->m_field33C, sizeof(value));
    return value;
}
void setParticlesEnabled(bool enabled) { g_effectsDisabled = !enabled; }
}
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)
BF2_CONSOLE_FIELD(timeToLive, RandomDistribution, activeEffectBundle(), m_dist2E0, kEffectConsoleTag)
BF2_CONSOLE_FIELD(randomAgeFactor, RandomDistribution, activeEffectBundle(), m_dist2F4, kEffectConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addWorkOnMaterial, addWorkOnMaterial, activeEffectBundle() != 0, int, kEffectConsoleTag)
BF2_CONSOLE_FIELD(startOnEffects, int, activeEffectBundle(), m_field330, kEffectConsoleTag)
BF2_CONSOLE_FIELD(stopOnEffects, int, activeEffectBundle(), m_field334, kEffectConsoleTag)
BF2_CONSOLE_FIELD(startAtCreation, bool, activeEffectBundle(), m_byte314, kEffectConsoleTag)
BF2_CONSOLE_FIELD(speedFromPhysics, bool, activeEffectBundle(), m_byte338, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitFrequencyScaleFactorLowerBound, float, activeEffectBundle(),
    setEmitFrequencyScaleFactorLowerBound, getEmitFrequencyScaleFactorLowerBound, kEffectConsoleTag)
BF2_CONSOLE_FIELD(startLodPercentageOfLodDistance, float, activeEffectBundle(), m_field310, kEffectConsoleTag)
BF2_CONSOLE_FIELD(minDistanceUnderWaterSurface, float, activeEffectBundle(), m_field2D8, kEffectConsoleTag)
BF2_CONSOLE_FIELD(maxDistanceUnderWaterSurface, float, activeEffectBundle(), m_field2DC, kEffectConsoleTag)
BF2_CONSOLE_FIELD(lodDistance, float, activeEffectBundle(), m_field308, kEffectConsoleTag)
BF2_CONSOLE_FIELD(hasAltEffect, bool, activeEffectBundle(), m_byte339, kEffectConsoleTag)
BF2_CONSOLE_FIELD(isUnderwaterEffect, bool, activeEffectBundle(), m_byte33A, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY_FN(effectChangeWaterHeight, float, activeEffectBundle() != 0,
    getEffectWaterHeight(), setEffectWaterHeight, kEffectConsoleTag)

// objectTemplate.engineInfluence / startDistance / maxDistance - dedicated PE gate sub_512000 =
// `getActiveTemplate(0x9485 = CID_AirDraftEffectBundleTemplate) != 0`; executors 0x512400 / 0x512080
// / 0x512240 read and write the PE floats +904 / +908 / +912, which are openBF2 m_field2C4 / m_field2C8
// / m_field2CC (the constant +0xC4 delta). All three are set/get properties with m_isDynamic = 1.
BF2_CONSOLE_PROPERTY_FN(engineInfluence, float, activeAirDraftEffectBundle() != 0,
    activeAirDraftEffectBundle()->m_field2C4, setAirDraftEngineInfluence, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY_FN(startDistance, float, activeAirDraftEffectBundle() != 0,
    activeAirDraftEffectBundle()->m_field2C8, setAirDraftStartDistance, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY_FN(maxDistance, float, activeAirDraftEffectBundle() != 0,
    activeAirDraftEffectBundle()->m_field2CC, setAirDraftMaxDistance, kEffectConsoleTag)

// The generated Vec2 and bool executor streams are identical except for their two virtual
// call offsets. All 48 pairs were compared against ELF 871C2B0/871C840, including the
// separate SpeedInDof slot and AgeScaleFactor's capitalized script spelling.
BF2_CONSOLE_PROPERTY(alphaIntensityRange, Vec2, activeEffectProperties(), setAlphaIntensityRange, getAlphaIntensityRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityUsed, bool, activeEffectProperties(), setAlphaIntensityUsed, getAlphaIntensityUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromSpeed, Vec2, activeEffectProperties(), setAlphaIntensityFromSpeed, getAlphaIntensityFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromSpeedInDof, Vec2, activeEffectProperties(), setAlphaIntensityFromSpeedInDof, getAlphaIntensityFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromRotationalSpeed, Vec2, activeEffectProperties(), setAlphaIntensityFromRotationalSpeed, getAlphaIntensityFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromAcceleration, Vec2, activeEffectProperties(), setAlphaIntensityFromAcceleration, getAlphaIntensityFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromAltitude, Vec2, activeEffectProperties(), setAlphaIntensityFromAltitude, getAlphaIntensityFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(alphaIntensityFromEngine, Vec2, activeEffectProperties(), setAlphaIntensityFromEngine, getAlphaIntensityFromEngine, kEffectConsoleTag)

BF2_CONSOLE_PROPERTY(emitScaleFactorRange, Vec2, activeEffectProperties(), setEmitScaleFactorRange, getEmitScaleFactorRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorUsed, bool, activeEffectProperties(), setEmitScaleFactorUsed, getEmitScaleFactorUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromSpeed, Vec2, activeEffectProperties(), setEmitScaleFactorFromSpeed, getEmitScaleFactorFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromSpeedInDof, Vec2, activeEffectProperties(), setEmitScaleFactorFromSpeedInDof, getEmitScaleFactorFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromRotationalSpeed, Vec2, activeEffectProperties(), setEmitScaleFactorFromRotationalSpeed, getEmitScaleFactorFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromAcceleration, Vec2, activeEffectProperties(), setEmitScaleFactorFromAcceleration, getEmitScaleFactorFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromAltitude, Vec2, activeEffectProperties(), setEmitScaleFactorFromAltitude, getEmitScaleFactorFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitScaleFactorFromEngine, Vec2, activeEffectProperties(), setEmitScaleFactorFromEngine, getEmitScaleFactorFromEngine, kEffectConsoleTag)

BF2_CONSOLE_PROPERTY(emitSpeedScaleRange, Vec2, activeEffectProperties(), setEmitSpeedScaleRange, getEmitSpeedScaleRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleUsed, bool, activeEffectProperties(), setEmitSpeedScaleUsed, getEmitSpeedScaleUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromSpeed, Vec2, activeEffectProperties(), setEmitSpeedScaleFromSpeed, getEmitSpeedScaleFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromSpeedInDof, Vec2, activeEffectProperties(), setEmitSpeedScaleFromSpeedInDof, getEmitSpeedScaleFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromRotationalSpeed, Vec2, activeEffectProperties(), setEmitSpeedScaleFromRotationalSpeed, getEmitSpeedScaleFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromAcceleration, Vec2, activeEffectProperties(), setEmitSpeedScaleFromAcceleration, getEmitSpeedScaleFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromAltitude, Vec2, activeEffectProperties(), setEmitSpeedScaleFromAltitude, getEmitSpeedScaleFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedScaleFromEngine, Vec2, activeEffectProperties(), setEmitSpeedScaleFromEngine, getEmitSpeedScaleFromEngine, kEffectConsoleTag)

BF2_CONSOLE_PROPERTY(startAgeRange, Vec2, activeEffectProperties(), setStartAgeRange, getStartAgeRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeUsed, bool, activeEffectProperties(), setStartAgeUsed, getStartAgeUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromSpeed, Vec2, activeEffectProperties(), setStartAgeFromSpeed, getStartAgeFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromSpeedInDof, Vec2, activeEffectProperties(), setStartAgeFromSpeedInDof, getStartAgeFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromRotationalSpeed, Vec2, activeEffectProperties(), setStartAgeFromRotationalSpeed, getStartAgeFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromAcceleration, Vec2, activeEffectProperties(), setStartAgeFromAcceleration, getStartAgeFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromAltitude, Vec2, activeEffectProperties(), setStartAgeFromAltitude, getStartAgeFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(startAgeFromEngine, Vec2, activeEffectProperties(), setStartAgeFromEngine, getStartAgeFromEngine, kEffectConsoleTag)

BF2_CONSOLE_PROPERTY(soundIntensityRange, Vec2, activeEffectProperties(), setSoundIntensityRange, getSoundIntensityRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityUsed, bool, activeEffectProperties(), setSoundIntensityUsed, getSoundIntensityUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromSpeed, Vec2, activeEffectProperties(), setSoundIntensityFromSpeed, getSoundIntensityFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromSpeedInDof, Vec2, activeEffectProperties(), setSoundIntensityFromSpeedInDof, getSoundIntensityFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromRotationalSpeed, Vec2, activeEffectProperties(), setSoundIntensityFromRotationalSpeed, getSoundIntensityFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromAcceleration, Vec2, activeEffectProperties(), setSoundIntensityFromAcceleration, getSoundIntensityFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromAltitude, Vec2, activeEffectProperties(), setSoundIntensityFromAltitude, getSoundIntensityFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(soundIntensityFromEngine, Vec2, activeEffectProperties(), setSoundIntensityFromEngine, getSoundIntensityFromEngine, kEffectConsoleTag)

BF2_CONSOLE_PROPERTY(AgeScaleFactorRange, Vec2, activeEffectProperties(), setAgeScaleFactorRange, getAgeScaleFactorRange, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorUsed, bool, activeEffectProperties(), setAgeScaleFactorUsed, getAgeScaleFactorUsed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromSpeed, Vec2, activeEffectProperties(), setAgeScaleFactorFromSpeed, getAgeScaleFactorFromSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromSpeedInDof, Vec2, activeEffectProperties(), setAgeScaleFactorFromSpeedInDof, getAgeScaleFactorFromSpeedInDof, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromRotationalSpeed, Vec2, activeEffectProperties(), setAgeScaleFactorFromRotationalSpeed, getAgeScaleFactorFromRotationalSpeed, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromAcceleration, Vec2, activeEffectProperties(), setAgeScaleFactorFromAcceleration, getAgeScaleFactorFromAcceleration, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromAltitude, Vec2, activeEffectProperties(), setAgeScaleFactorFromAltitude, getAgeScaleFactorFromAltitude, kEffectConsoleTag)
BF2_CONSOLE_PROPERTY(AgeScaleFactorFromEngine, Vec2, activeEffectProperties(), setAgeScaleFactorFromEngine, getAgeScaleFactorFromEngine, kEffectConsoleTag)
BF2_CONSOLE_OBJECT("Particles", 2, 0)
// ELF 86AFBF6/8731110: unlike the template properties this global binding is static.
#define BF2_EFFECT_ENABLE_BINDING() \
namespace { \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl \
{ \
    BF2_CC_HEAD(enableParticles, 1, 0, 1, "bool", false) \
        m_argTypes[0] = "bool"; \
    BF2_CC_TAIL(1, kEffectConsoleTag) \
    bool isObjectActive() const override { return true; } \
    void executeObjectMethod() override \
    { \
        if (m_argCount == 1) { m_hasReturnValue = false; setParticlesEnabled(BF2_CC_ARG(bool, 0)); } \
        else if (m_argCount == 0) { m_hasReturnValue = true; m_returnValue = !g_effectsDisabled; } \
    } \
    void setArgFromString(int index, const std::string& value) override { BF2_CC_CONVERT(bool, 1) } \
    BF2_CC_RETURN_AS_STRING \
    BF2_CC_COMMON_HOOKS \
private: \
    bool m_returnValue; \
}; \
static BF2_CC_NAME BF2_CC_INSTANCE; \
}
BF2_EFFECT_ENABLE_BINDING()
#undef BF2_EFFECT_ENABLE_BINDING
// `Particles.enableOldParticles` - ELF ConsoleClass573 (registration 0x86AFAA0, executor 0x8731640) /
// BF2.exe ctor 0x576769, executor 0x576827; The exact twin of
// enableParticles above and registered right after it, under the same "Particles" object: type 1,
// argc 0..1, "bool"/"bool", not dynamic, isObjectActive `return 1`. GET answers
// !Emitter::mDebugNoOldParticles, SET stores !arg; the client Emitter spawns nothing while it is set.
// No stock content uses it.
namespace {
void setOldParticlesEnabled(bool enabled) { Emitter::mDebugNoOldParticles = !enabled; }
}
#define BF2_OLD_PARTICLES_ENABLE_BINDING() \
namespace { \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl \
{ \
    BF2_CC_HEAD(enableOldParticles, 1, 0, 1, "bool", false) \
        m_argTypes[0] = "bool"; \
    BF2_CC_TAIL(1, kEffectConsoleTag) \
    bool isObjectActive() const override { return true; } \
    void executeObjectMethod() override \
    { \
        if (m_argCount == 1) { m_hasReturnValue = false; setOldParticlesEnabled(BF2_CC_ARG(bool, 0)); } \
        else if (m_argCount == 0) { m_hasReturnValue = true; m_returnValue = !Emitter::mDebugNoOldParticles; } \
    } \
    void setArgFromString(int index, const std::string& value) override { BF2_CC_CONVERT(bool, 1) } \
    BF2_CC_RETURN_AS_STRING \
    BF2_CC_COMMON_HOOKS \
private: \
    bool m_returnValue; \
}; \
static BF2_CC_NAME BF2_CC_INSTANCE; \
}
BF2_OLD_PARTICLES_ENABLE_BINDING()
#undef BF2_OLD_PARTICLES_ENABLE_BINDING
} } } // namespace dice::hfe::world
