// Decal, emitter and particle template console bindings.
// Source path is attested by the ELF anonymous namespace.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Ids.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"
#include "../TemplateLeaves.h"
#include "DecalTemplate.h"                                // the Decal block's accessor receivers

namespace dice { namespace hfe { namespace world {
// [CHOSEN, not recovered] Console metadata alias for the numeric material/paint flags.
typedef unsigned int uint;
namespace {
const char* const kObjTemplFxConsoleTag = "world.objTemplFx";
EmitterTemplate* activeEmitter()
{
    return static_cast<EmitterTemplate*>(getActiveTemplate(CID_EmitterTemplate));
}
void setEmitterStartProbability(float value) { activeEmitter()->setStartProbability(value); }
void setEmitterZRotationSnap(float value)
{
    activeEmitter()->m_zRotationSnap = value > 1.0f ? value : 1.0f;
}
template<int Index> void setAlternateTemplate(const std::string& value)
{
    EmitterTemplate* emitter = activeEmitter();
    emitter->m_alternateTemplates[Index] = value;
    emitter->updateNumAlternateTemplates();
}
}

// ELF defaults at 0x86C55B9 / BF2.exe 0x84F7E6: ObjectTemplate, full access, category zero - one
// SetConsoleClassDefaults for every block of this TU (the Decal block right below, the Emitter
// block after it, then the Particle block).
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)

// Class-gated decal bindings: properties get/set, while relative frame-size methods require one argument.
// Preserve int metadata over unsigned frame accessors and Vec3ColorFloat conversion.
// Animation reads require a renderer template; dedicated setters safely see a null handle.
namespace {
DecalTemplate* activeDecal()
{
    return static_cast<DecalTemplate*>(getActiveTemplate(CID_DecalTemplate));
}
// ConsoleClass102 / 103: straight to the accessor on a freshly fetched template.
void decalSetAnimationFrameWidthRelative(float width)
{
    activeDecal()->setAnimationFrameWidthRelative(width);
}
void decalSetAnimationFrameHeightRelative(float height)
{
    activeDecal()->setAnimationFrameHeightRelative(height);
}
} // namespace

// [CHOSEN, not recovered] Retail's console metadata spelling for the Decal `color` row - the same
// local-typedef device as ModuleWorldObjTemplBase.cpp's Vec3ColorFloatIntensity. The type behind
// the name is a Vec3 (see the evidence above; DecalTemplate::setColor takes `const Vec3&`, ELF
// _ZN...8setColorERKNS0_4Vec3E).
typedef Math::Vec3 Vec3ColorFloat;

BF2_CONSOLE_PROPERTY(isStatic, bool, activeDecal(), setIsStatic, getIsStatic, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(usedOnTransparentMaterials, bool, activeDecal(), setUsedOnTransparentMaterials, getUsedOnTransparentMaterials, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(decalTextureName, std::string, activeDecal(), setDecalTextureName, getDecalTextureName, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(randomColorIntensity, float, activeDecal(), setRandomColorIntensity, getRandomColorIntensity, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(color, Vec3ColorFloat, activeDecal(), setColor, getColor, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(randomSize, float, activeDecal(), setRandomSize, getRandomSize, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(size, float, activeDecal(), setSize, getSize, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotation, float, activeDecal(), setRandomRotation, getRandomRotation, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(rotation, float, activeDecal(), setRotation, getRotation, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationEnable, bool, activeDecal(), setAnimationEnable, getAnimationEnable, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationSpeed, float, activeDecal(), setAnimationSpeed, getAnimationSpeed, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameWidth, int, activeDecal(), setAnimationFrameWidth, getAnimationFrameWidth, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameHeight, int, activeDecal(), setAnimationFrameHeight, getAnimationFrameHeight, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameWidthRelative, decalSetAnimationFrameWidthRelative,
    activeDecal() != 0, float, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameHeightRelative, decalSetAnimationFrameHeightRelative,
    activeDecal() != 0, float, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationRandomizedStartFrame, bool, activeDecal(), setAnimationRandomizedStartFrame, getAnimationRandomizedStartFrame, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCount, int, activeDecal(), setAnimationFrameCount, getAnimationFrameCount, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCountX, int, activeDecal(), setAnimationFrameCountX, getAnimationFrameCountX, kObjTemplFxConsoleTag)

// the Emitter block
// Every Emitter row below is a dynamic, zero-or-one-argument property. Distribution fields are
// assigned whole; these console executors deliberately do not invoke the separate clipping setter
// methods.
BF2_CONSOLE_FIELD(useCameraOrientation, bool, activeEmitter(), m_useCameraOrientation, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(looping, bool, activeEmitter(), m_looping, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(startAtCreation, bool, activeEmitter(), m_startAtCreation, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(intensityAtSpeed, float, activeEmitter(), m_intensityAtSpeed, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(addEmitterSpeed, bool, activeEmitter(), m_addEmitterSpeed, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(emitterSpeedScale, float, activeEmitter(), m_emitterSpeedScale, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(startProbability, float, activeEmitter() != 0,
    activeEmitter()->m_startProbability, setEmitterStartProbability, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(lodDistance, float, activeEmitter(), m_lodDistance, kObjTemplFxConsoleTag)
namespace {
// [CHOSEN] class spelling, recovered ConsoleClass124 behavior. The generic conversion macro
// discards a parsed value on failbit; this retail parser consumes EOF with get() after its last
// key, so its converter must keep the direct static assignment at ELF 0x86D2CF0 / PE 0x4FB2E0.
class EmitterCurveConsole : public ConsoleObjectBaseImpl
{
public:
    EmitterCurveConsole()
    {
        m_objectName = consoleDefaultName();
        m_functionName = "intensityOverTime";
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_type = 1; m_minNoArgs = 0; m_maxNoArgs = 1;
        m_returnType = "OverTimeDistribution";
        m_argTypes[0] = "OverTimeDistribution";
        m_argNeedsConversion[0] = -1;
        m_isDynamic = true; m_hasReturnValue = false;
        consoleBindingList(kObjTemplFxConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return activeEmitter() != 0; }
    void executeObjectMethod() override
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            activeEmitter()->m_intensityOverTime = BF2_CC_ARG(OverTimeDistribution, 0);
        }
        else if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = activeEmitter()->m_intensityOverTime;
        }
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1)
        {
            static OverTimeDistribution distribution;
            std::istringstream input(value);
            input >> distribution;
            m_args[0] = reinterpret_cast<std::string*>(&distribution);
        }
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    OverTimeDistribution m_returnValue;
};
EmitterCurveConsole emitterCurveConsole;
}
BF2_CONSOLE_FIELD(delay, RandomDistribution, activeEmitter(), m_delay, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(timeToLive, RandomDistribution, activeEmitter(), m_timeToLive, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(intensity, RandomDistribution, activeEmitter(), m_intensity, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(relativePositionInDof, RandomDistribution, activeEmitter(), m_relativePosition[0], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(relativePositionInUp, RandomDistribution, activeEmitter(), m_relativePosition[1], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(relativePositionInRight, RandomDistribution, activeEmitter(), m_relativePosition[2], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(positionalSpeedInDof, RandomDistribution, activeEmitter(), m_positionalSpeed[0], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(positionalSpeedInUp, RandomDistribution, activeEmitter(), m_positionalSpeed[1], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(positionalSpeedInRight, RandomDistribution, activeEmitter(), m_positionalSpeed[2], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(rotationalSpeedInDof, RandomDistribution, activeEmitter(), m_rotationalSpeed[0], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(rotationalSpeedInUp, RandomDistribution, activeEmitter(), m_rotationalSpeed[1], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(rotationalSpeedInRight, RandomDistribution, activeEmitter(), m_rotationalSpeed[2], kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(startRotation, RandomDistribution, activeEmitter(), m_startRotation, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(zRotation, RandomDistribution, activeEmitter(), m_zRotation, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(zRotationSnap, float, activeEmitter() != 0,
    activeEmitter()->m_zRotationSnap, setEmitterZRotationSnap, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(hasOverDamage, bool, activeEmitter(), m_hasOverDamage, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(noPhysics, bool, activeEmitter(), m_noPhysics, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(showInFirstPerson, bool, activeEmitter(), m_showInFirstPerson, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(showInThirdPerson, bool, activeEmitter(), m_showInThirdPerson, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(addChild, bool, activeEmitter(), m_addChild, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(moveToWaterSurface, bool, activeEmitter(), m_moveToWaterSurface, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(template, std::string, activeEmitter(), m_template, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(alternateTemplate0, std::string, activeEmitter() != 0,
    activeEmitter()->m_alternateTemplates[0], setAlternateTemplate<0>, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(alternateTemplate1, std::string, activeEmitter() != 0,
    activeEmitter()->m_alternateTemplates[1], setAlternateTemplate<1>, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(alternateTemplate2, std::string, activeEmitter() != 0,
    activeEmitter()->m_alternateTemplates[2], setAlternateTemplate<2>, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(alternateTemplate3, std::string, activeEmitter() != 0,
    activeEmitter()->m_alternateTemplates[3], setAlternateTemplate<3>, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(alternateTemplate4, std::string, activeEmitter() != 0,
    activeEmitter()->m_alternateTemplates[4], setAlternateTemplate<4>, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(randomizeGeomPart, bool, activeEmitter(), m_randomizeGeomPart, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(useGeomPart, int, activeEmitter(), m_geometryPartId, kObjTemplFxConsoleTag)

ConsoleObjectList& objTemplFxConsoleObjects()
{
    return consoleBindingList(kObjTemplFxConsoleTag);
}
} } } // namespace dice::hfe::world

// Particle template properties preserve direct-field, accessor and helper-setter behavior.
// Method rows have exact argument counts and nondynamic metadata; the receiver must be a ParticleTemplate.
#include "ParticleTemplate.h"
#include "../../../Core/Types.h"                          // U32 - the retail metadata spelling
#include "../../../Physics/PhysicsType.h"                 // physicsType's enum and stream pair

namespace dice { namespace hfe { namespace world {
namespace {
ParticleTemplate* activeParticle()
{
    return static_cast<ParticleTemplate*>(getActiveTemplate(CID_ParticleTemplate));
}
// geometryPart / collisionPart (ConsoleClass167 / 170): SET through the template's own virtual
// setter (ELF +0xF0 / +0xF4, PE +0xE0 / +0xE4), GET a raw read of +0x5C / +0x60.
void particleGeometryPartSet(int value) { activeParticle()->setGeometryPart(value); }
void particleCollisionPartSet(int value) { activeParticle()->setCollisionPart(value); }
// has{Response,Point,RotationalPoint}Physics (ConsoleClass174 / 175 / 176): the byte, then
// checkPhysicsTypes (ELF 0x820C1D0, PE 0x64AA20) - on the same template, fetched once.
void particleHasResponsePhysicsSet(bool value)
{
    ParticleTemplate* const particle = activeParticle();
    particle->m_hasResponsePhysics = value;
    particle->checkPhysicsTypes();
}
void particleHasPointPhysicsSet(bool value)
{
    ParticleTemplate* const particle = activeParticle();
    particle->m_hasPointPhysics = value;
    particle->checkPhysicsTypes();
}
void particleHasRotationalPointPhysicsSet(bool value)
{
    ParticleTemplate* const particle = activeParticle();
    particle->m_hasRotationalPointPhysics = value;
    particle->checkPhysicsTypes();
}
// physicsType (ConsoleClass178, ELF 0x86E2060 / PE 0x565586): the template is fetched first, then
// the value is stored only for 0..4 (ELF `cmp esi, 4; ja`, PE `jl` / `cmp edi, 5; jge`) - anything
// else is dropped without a word.
void particlePhysicsTypeSet(PhysicsType value)
{
    ParticleTemplate* const particle = activeParticle();
    if (static_cast<unsigned int>(value) <= 4u)
        particle->m_physicsType = static_cast<int>(value);
}
// addToCollisionGroup (ConsoleClass179, ELF 0x86E2420 / PE 0x565780): OR, never a store.
void particleAddToCollisionGroup(U32 value) { activeParticle()->m_collisionGroups |= value; }
// isSleeping (ConsoleClass192, ELF 0x86E65F0 / PE 0x56677D): bit 0 only.
void particleIsSleepingSet(bool value) { activeParticle()->m_isSleeping = value ? 1 : 0; }
// mapMaterial (ConsoleClass206, ELF 0x86EB830 / PE 0x567859): argc exactly 3, straight to the
// template's own mapMaterial (ELF +0xF8 / PE +0xE8), which overwrites both name and value.
void particleMapMaterial(unsigned int index, const std::string& material, unsigned int flags)
{
    activeParticle()->mapMaterial(index, material, flags);
}

// The four curve rows (ConsoleClass202..205) - the same generated class as this TU's Emitter curve
// (ConsoleClass124 / EmitterCurveConsole above) and read the same way: each setArgFromString
// (ELF 0x86E97F0 / 0x86E9F00 / 0x86EA610 / 0x86EAD20) parses into its OWN guarded function-local
// OverTimeDistribution(1.0f) with the inlined operator>> and plants its address with no failbit
// test, and each executor copies the 0x1AC block whole in both arms. One class template per
// member, so every row keeps its own static exactly as the four generated classes do.
template <OverTimeDistribution ParticleTemplate::*Member>
class ParticleCurveConsole : public ConsoleObjectBaseImpl
{
public:
    explicit ParticleCurveConsole(const char* name)
    {
        m_objectName = consoleDefaultName();
        m_functionName = name;
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_type = 1; m_minNoArgs = 0; m_maxNoArgs = 1;
        m_returnType = "OverTimeDistribution";
        m_argTypes[0] = "OverTimeDistribution";
        m_argNeedsConversion[0] = -1;
        m_isDynamic = true; m_hasReturnValue = false;
        consoleBindingList(kObjTemplFxConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return activeParticle() != 0; }
    void executeObjectMethod() override
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            activeParticle()->*Member = BF2_CC_ARG(OverTimeDistribution, 0);
        }
        else if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = activeParticle()->*Member;
        }
    }
    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1)
        {
            static OverTimeDistribution distribution;
            std::istringstream input(value);
            input >> distribution;
            m_args[0] = reinterpret_cast<std::string*>(&distribution);
        }
    }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    OverTimeDistribution m_returnValue;
};
} // namespace

// the 37 rows, in the ELF line order (166..206) = the retail registration order
BF2_CONSOLE_PROPERTY(geometry, std::string, activeParticle(), setGeometry, getGeometry, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(geometryPart, int, activeParticle() != 0,
    activeParticle()->m_geometryPart, particleGeometryPartSet, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(usedOnTransparentSurface, bool, activeParticle(), m_usedOnTransparentSurface, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(collisionMesh, std::string, activeParticle(), setCollisionMesh, getCollisionMesh, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(collisionPart, int, activeParticle() != 0,
    activeParticle()->m_collisionPart, particleCollisionPartSet, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(collisionEffectName, std::string, activeParticle(), m_collisionEffectName, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(hasMobilePhysics, bool, activeParticle(), m_hasMobilePhysics, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(hasCollisionPhysics, bool, activeParticle(), m_hasCollisionPhysics, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(hasResponsePhysics, particleHasResponsePhysicsSet,
    activeParticle() != 0, bool, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(hasPointPhysics, particleHasPointPhysicsSet,
    activeParticle() != 0, bool, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(hasRotationalPointPhysics, particleHasRotationalPointPhysicsSet,
    activeParticle() != 0, bool, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(physicsType, PhysicsType, activeParticle() != 0,
    static_cast<PhysicsType>(activeParticle()->m_physicsType), particlePhysicsTypeSet, kObjTemplFxConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(addToCollisionGroup, particleAddToCollisionGroup,
    activeParticle() != 0, U32, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(collisionGroups, U32, activeParticle(), m_collisionGroups, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(timeToLive, RandomDistribution, activeParticle(), m_timeToLive, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(size, RandomDistribution, activeParticle(), m_size, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(gravityModifier, RandomDistribution, activeParticle(), m_gravityModifier, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(drag, RandomDistribution, activeParticle(), m_drag, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(dragOffset, Vec3, activeParticle(), m_dragOffset, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(mass, float, activeParticle(), m_mass, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(centerOfMassOffset, Vec3, activeParticle(), m_centerOfMassOffset, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(centerOfCollisionOffset, Vec3, activeParticle(), m_centerOfCollisionOffset, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(inertiaModifier, Vec3, activeParticle(), m_inertiaModifier, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY_FN(isSleeping, bool, activeParticle() != 0,
    (activeParticle()->m_isSleeping & 1) != 0, particleIsSleepingSet, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(floaterMod, float, activeParticle(), m_floaterMod, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(otherCollisionLod, int, activeParticle(), m_otherCollisionLod, kObjTemplFxConsoleTag)
// Registered as "Transparency" - capital T - in both images (BF2.exe metadata; the ELF row writes
// +0xDC). Console names compare case-insensitively, so stock `ObjectTemplate.transparency` lines
// (444 of them) land here.
BF2_CONSOLE_FIELD(Transparency, RandomDistribution, activeParticle(), m_transparency, kObjTemplFxConsoleTag)
BF2_CONSOLE_PROPERTY(transparencyGraph, Vec4, activeParticle(), setTransparencyGraph, getTransparencyGraph, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(sizeModifierX, float, activeParticle(), m_sizeModifier.x, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(sizeModifierY, float, activeParticle(), m_sizeModifier.y, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(sizeModifierZ, float, activeParticle(), m_sizeModifier.z, kObjTemplFxConsoleTag)
BF2_CONSOLE_FIELD(sizeModifier, Vec3, activeParticle(), m_sizeModifier, kObjTemplFxConsoleTag)
namespace {
ParticleCurveConsole<&ParticleTemplate::m_sizeOverTime> particleSizeOverTimeConsole("sizeOverTime");
ParticleCurveConsole<&ParticleTemplate::m_gravityModifierOverTime>
    particleGravityModifierOverTimeConsole("gravityModifierOverTime");
ParticleCurveConsole<&ParticleTemplate::m_dragOverTime> particleDragOverTimeConsole("dragOverTime");
ParticleCurveConsole<&ParticleTemplate::m_alphaOverTime> particleAlphaOverTimeConsole("alphaOverTime");
} // namespace
// The retail metadata spells both numeric argTypes "uint"; the local alias above registers it.
BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(mapMaterial, particleMapMaterial, activeParticle() != 0,
    uint, std::string, uint, kObjTemplFxConsoleTag)
} } } // namespace dice::hfe::world
