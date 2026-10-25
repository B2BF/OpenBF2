// [INFERRED] split of attested Game/Objects/Fx/ModuleWorldObjTemplFx.cpp.
// Full ELF ConsoleClass246..448 executor/selection streams read and cross-checked with the client PE.
// This is the bounded stock-soldier swimming property surface, shared with ordinary sprites.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Ids.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"
#include "ParticleSystemTemplates.h"

namespace dice { namespace hfe { namespace world {
namespace {
const char* const kParticleConsoleTag = "world.objTemplFx";
SpriteParticleSystemTemplate* activeSpriteParticle()
{
    return static_cast<SpriteParticleSystemTemplate*>(getActiveTemplate(CID_SpriteParticleSystemTemplate));
}
NonScreenAlignedParticleSystemTemplate* activeNsaParticle()
{
    return static_cast<NonScreenAlignedParticleSystemTemplate*>(getActiveTemplate(CID_NonScreenAlignedParticleSystemTemplate));
}
// ELF 86ACD60 / client PE 55F24C: preserve the lookup order, including the emitter leaf.
ParticleSystemTemplateBase* activeParticleBase()
{
    if (IObjectTemplate* value = getActiveTemplate(CID_SpriteParticleSystemTemplate))
        return static_cast<ParticleSystemTemplateBase*>(value);
    if (IObjectTemplate* value = getActiveTemplate(CID_MeshParticleSystemTemplate))
        return static_cast<ParticleSystemTemplateBase*>(value);
    if (IObjectTemplate* value = getActiveTemplate(CID_ParticleSystemEmitterTemplate))
        return static_cast<ParticleSystemTemplateBase*>(value);
    if (IObjectTemplate* value = getActiveTemplate(CID_TrailSystemTemplate))
        return static_cast<ParticleSystemTemplateBase*>(value);
    return activeNsaParticle();
}
// ELF 86ACDD0 / client PE 55F298: the sprite-base selector excludes ParticleSystemEmitter.
SpriteParticleSystemTemplateBase* activeParticleSpriteBase()
{
    if (IObjectTemplate* value = getActiveTemplate(CID_SpriteParticleSystemTemplate))
        return static_cast<SpriteParticleSystemTemplateBase*>(value);
    if (IObjectTemplate* value = getActiveTemplate(CID_MeshParticleSystemTemplate))
        return static_cast<SpriteParticleSystemTemplateBase*>(value);
    if (IObjectTemplate* value = getActiveTemplate(CID_TrailSystemTemplate))
        return static_cast<SpriteParticleSystemTemplateBase*>(value);
    return activeNsaParticle();
}
}

// [CHOSEN, not recovered] Console metadata alias for a three-float colour; the generated colour
// executors move three dwords (the Math type of the same name holds four floats).
typedef Math::Vec3 Vec3ColorFloatIntensity;

namespace {
void spriteSetAnimationFrameWidthRelative(float value)
{
    activeSpriteParticle()->setAnimationFrameWidthRelative(value);
}
void spriteSetAnimationFrameHeightRelative(float value)
{
    activeSpriteParticle()->setAnimationFrameHeightRelative(value);
}
void nsaSetAnimationFrameWidthRelative(float value)
{
    activeNsaParticle()->setAnimationFrameWidthRelative(value);
}
void nsaSetAnimationFrameHeightRelative(float value)
{
    activeNsaParticle()->setAnimationFrameHeightRelative(value);
}
} // namespace

BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)
BF2_CONSOLE_FIELD(showInFirstPerson, bool, activeParticleBase(), m_byte40, kParticleConsoleTag)
BF2_CONSOLE_FIELD(showInThirdPerson, bool, activeParticleBase(), m_byte41, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitterType, std::string, activeParticleBase(), setEmitterType, getEmitterType, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitPrio, std::string, activeParticleBase(), setEmitPrio, getEmitPrio, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(LodDistance, std::string, activeParticleBase(), setLodDistance, getLodDistance, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(qualityLevel, std::string, activeParticleBase(), setQualityLevel, getQualityLevel, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(isLocalSystem, bool, activeParticleBase(), setIsLocalSystem, getIsLocalSystem, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(radialDirection, float, activeParticleBase(), setRadialDirection, getRadialDirection, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRadialDirection, float, activeParticleBase(), setRandomRadialDirection, getRandomRadialDirection, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitRadius, Vec3, activeParticleBase(), setEmitRadius, getEmitRadius, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitRangeX, RandomDistribution, activeParticleBase(), setEmitRangeX, getEmitRangeX, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitRangeY, RandomDistribution, activeParticleBase(), setEmitRangeY, getEmitRangeY, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitRangeZ, RandomDistribution, activeParticleBase(), setEmitRangeZ, getEmitRangeZ, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(timeToLive, float, activeParticleBase(), setTimeToLive, getTimeToLive, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomTimeToLive, float, activeParticleBase(), setRandomTimeToLive, getRandomTimeToLive, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(windAffectionFactor, float, activeParticleBase(), setWindAffectionFactor, getWindAffectionFactor, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(rotationGraph, Vec4, activeParticleBase(), setRotationGraph, getRotationGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(gravityGraph, Vec4, activeParticleBase(), setGravityGraph, getGravityGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(airResistanceGraph, Vec4, activeParticleBase(), setAirResistanceGraph, getAirResistanceGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeedGraph, Vec4, activeParticleBase(), setEmitSpeedGraph, getEmitSpeedGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(gravity, float, activeParticleBase(), setGravity, getGravity, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(airResistance, float, activeParticleBase(), setAirResistance, getAirResistance, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomSpeed, float, activeParticleBase(), setRandomSpeed, getRandomSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomDirectionAngle, Vec3, activeParticleBase(), setRandomDirectionAngle, getRandomDirectionAngle, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(collisionEnable, bool, activeParticleBase(), setCollisionEnable, getCollisionEnable, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(forceToWaterSurface, bool, activeParticleBase(), setForceToWaterSurface, getForceToWaterSurface, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(waterSurfaceOffset, float, activeParticleBase(), setWaterSurfaceOffset, getWaterSurfaceOffset, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(collisionSpeedAlongNormal, float, activeParticleBase(), setCollisionSpeedAlongNormal, getCollisionSpeedAlongNormal, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(collisionSpeedAlongPlane, float, activeParticleBase(), setCollisionSpeedAlongPlane, getCollisionSpeedAlongPlane, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitFrequency, float, activeParticleBase(), setEmitFrequency, getEmitFrequency, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitSpeed, float, activeParticleBase(), setEmitSpeed, getEmitSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitTime, float, activeParticleBase(), setEmitTime, getEmitTime, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(isLooping, bool, activeParticleBase(), setIsLooping, getIsLooping, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitDelay, float, activeParticleBase(), setEmitDelay, getEmitDelay, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(alignRotationToSpeed, bool, activeParticleBase(), setAlignRotationToSpeed, getAlignRotationToSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(emitDirection, Vec3, activeParticleBase(), setEmitDirection, getEmitDirection, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(preWarmTime, float, activeParticleBase(), setPreWarmTime, getPreWarmTime, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(transparencyGraph, Vec4, activeParticleSpriteBase(), setTransparencyGraph, getTransparencyGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(lightMapIntensityOffset, float, activeParticleSpriteBase(), setLightMapIntensityOffset, getLightMapIntensityOffset, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(particleMaxSize, float, activeParticleSpriteBase(), setParticleMaxSize, getParticleMaxSize, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(color1, Vec3ColorFloatIntensity, activeParticleSpriteBase(), setColor1, getColor1, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(color2, Vec3ColorFloatIntensity, activeParticleSpriteBase(), setColor2, getColor2, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(colorBlendGraph, Vec4, activeParticleSpriteBase(), setColorBlendGraph, getColorBlendGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(sizeGraph, Vec4, activeParticleSpriteBase(), setSizeGraph, getSizeGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomSize, float, activeParticleSpriteBase(), setRandomSize, getRandomSize, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomIntensity, float, activeParticleSpriteBase(), setRandomIntensity, getRandomIntensity, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(alphaCull, float, activeParticleSpriteBase(), setAlphaCull, getAlphaCull, kParticleConsoleTag)

// Ordinary sprites have their own retail overloads and descriptor-tail slots.
// ELF ConsoleClass312/313 (86F8C60/86F9310); client PE 56A591/56A70E.
// Stock muzzle flashes author Additive here; the template's constructor default is Sorted.
BF2_CONSOLE_PROPERTY(particleType, std::string, activeSpriteParticle(), setParticleType, getParticleType, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(drawOrder, std::string, activeSpriteParticle(), setDrawOrder, getDrawOrder, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(textureName, std::string, activeSpriteParticle(), setTextureName, getTextureName, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(rotationSpeed, float, activeSpriteParticle(), setRotationSpeed, getRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotationSpeed, float, activeSpriteParticle(), setRandomRotationSpeed, getRandomRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotation, float, activeSpriteParticle(), setRandomRotation, getRandomRotation, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(pivotOffset, Vec2, activeSpriteParticle(), setPivotOffset, getPivotOffset, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(selfShadowIntensity, float, activeSpriteParticle(), setSelfShadowIntensity, getSelfShadowIntensity, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationEnable, bool, activeSpriteParticle(), setAnimationEnable, getAnimationEnable, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationPlayOnce, bool, activeSpriteParticle(), setAnimationPlayOnce, getAnimationPlayOnce, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationRandomizedStartFrame, bool, activeSpriteParticle(), setAnimationRandomizedStartFrame, getAnimationRandomizedStartFrame, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationSpeed, float, activeSpriteParticle(), setAnimationSpeed, getAnimationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCount, int, activeSpriteParticle(), setAnimationFrameCount, getAnimationFrameCount, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCountX, int, activeSpriteParticle(), setAnimationFrameCountX, getAnimationFrameCountX, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameWidthRelative, spriteSetAnimationFrameWidthRelative, activeSpriteParticle() != 0, float, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameHeightRelative, spriteSetAnimationFrameHeightRelative, activeSpriteParticle() != 0, float, kParticleConsoleTag)

// Non-screen-aligned sprites have their own retail overloads and descriptor-tail slots.
BF2_CONSOLE_PROPERTY(textureName, std::string, activeNsaParticle(), setTextureName, getTextureName, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(rotationSpeed, float, activeNsaParticle(), setRotationSpeed, getRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotationSpeed, float, activeNsaParticle(), setRandomRotationSpeed, getRandomRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotation, float, activeNsaParticle(), setRandomRotation, getRandomRotation, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(pivotOffset, Vec2, activeNsaParticle(), setPivotOffset, getPivotOffset, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(selfShadowIntensity, float, activeNsaParticle(), setSelfShadowIntensity, getSelfShadowIntensity, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationEnable, bool, activeNsaParticle(), setAnimationEnable, getAnimationEnable, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationPlayOnce, bool, activeNsaParticle(), setAnimationPlayOnce, getAnimationPlayOnce, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationRandomizedStartFrame, bool, activeNsaParticle(), setAnimationRandomizedStartFrame, getAnimationRandomizedStartFrame, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationSpeed, float, activeNsaParticle(), setAnimationSpeed, getAnimationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCount, int, activeNsaParticle(), setAnimationFrameCount, getAnimationFrameCount, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCountX, int, activeNsaParticle(), setAnimationFrameCountX, getAnimationFrameCountX, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameWidthRelative, nsaSetAnimationFrameWidthRelative, activeNsaParticle() != 0, float, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameHeightRelative, nsaSetAnimationFrameHeightRelative, activeNsaParticle() != 0, float, kParticleConsoleTag)

namespace {
TrailSystemTemplate* activeTrailParticle()
{
    return static_cast<TrailSystemTemplate*>(getActiveTemplate(CID_TrailSystemTemplate));
}
void trailSetAnimationFrameWidthRelative(float value)
{
    activeTrailParticle()->setAnimationFrameWidthRelative(value);
}
void trailSetAnimationFrameHeightRelative(float value)
{
    activeTrailParticle()->setAnimationFrameHeightRelative(value);
}
}

// Trail overloads: ELF ConsoleClass316/317/319/320/338/339/342..345/371..373.
// PE names and signatures preserve stock textureName and the two setAnimationFrame... commands.
BF2_CONSOLE_PROPERTY(textureName, std::string, activeTrailParticle(), setTextureName, getTextureName, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationEnable, bool, activeTrailParticle(), setAnimationEnable, getAnimationEnable, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationSpeed, float, activeTrailParticle(), setAnimationSpeed, getAnimationSpeed, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameWidthRelative, trailSetAnimationFrameWidthRelative, activeTrailParticle() != 0, float, kParticleConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setAnimationFrameHeightRelative, trailSetAnimationFrameHeightRelative, activeTrailParticle() != 0, float, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCount, int, activeTrailParticle(), setAnimationFrameCount, getAnimationFrameCount, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(animationFrameCountX, int, activeTrailParticle(), setAnimationFrameCountX, getAnimationFrameCountX, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(fadeInFactor, float, activeTrailParticle(), setFadeInFactor, getFadeInFactor, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(fadeOutFactor, float, activeTrailParticle(), setFadeOutFactor, getFadeOutFactor, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(textureTileFactor, float, activeTrailParticle(), setTextureTileFactor, getTextureTileFactor, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(textureOffsetVelocity, float, activeTrailParticle(), setTextureOffsetVelocity, getTextureOffsetVelocity, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(numPlanes, int, activeTrailParticle(), setNumPlanes, getNumPlanes, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(minTrailVelocity, float, activeTrailParticle(), setMinTrailVelocity, getMinTrailVelocity, kParticleConsoleTag)

// Mesh-particle and emitter properties use class-specific active-template checks.
namespace {
MeshParticleSystemTemplate* activeMeshParticle()
{
    return static_cast<MeshParticleSystemTemplate*>(getActiveTemplate(CID_MeshParticleSystemTemplate));
}
ParticleSystemEmitterTemplate* activeParticleEmitter()
{
    return static_cast<ParticleSystemEmitterTemplate*>(getActiveTemplate(CID_ParticleSystemEmitterTemplate));
}
}

// Mesh overloads: ELF ConsoleClass309 blendMode (+0x12C/+0x130), 310 drawOrder (+0x138/+0x13C),
// 311 writeZ, 321 geometry (+0x144/+0x148), 387 directionalScale, 390 randomRotationSpeed
// (+0x154/+0x158), 391 rotationSpeed (+0x15C/+0x160), 398 randomRotation (+0x14C/+0x150).
BF2_CONSOLE_PROPERTY(blendMode, std::string, activeMeshParticle(), setBlendMode, getBlendMode, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(drawOrder, std::string, activeMeshParticle(), setDrawOrder, getDrawOrder, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(writeZ, bool, activeMeshParticle(), setWriteZ, getWriteZ, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(geometry, std::string, activeMeshParticle(), setGeometry, getGeometry, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(directionalScale, float, activeMeshParticle(), setDirectionalScale, getDirectionalScale, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotationSpeed, Vec3, activeMeshParticle(), setRandomRotationSpeed, getRandomRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(rotationSpeed, Vec3, activeMeshParticle(), setRotationSpeed, getRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotation, Vec3, activeMeshParticle(), setRandomRotation, getRandomRotation, kParticleConsoleTag)

// Emitter overloads: ELF ConsoleClass322 particleSystemTemplate (+0x124/+0x128), 370
// transparencyGraph (the base's setter - the sprite-base selector above excludes emitters), 393
// randomRotationSpeed (+0x134/+0x138), 394 rotationSpeed (+0x13C/+0x140), 399 randomRotation
// (+0x12C/+0x130), 417 speedFromParticle (+0x144/+0x148).
BF2_CONSOLE_PROPERTY(particleSystemTemplate, std::string, activeParticleEmitter(), setParticleSystemTemplate, getParticleSystemTemplate, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(transparencyGraph, Vec4, activeParticleEmitter(), setTransparencyGraph, getTransparencyGraph, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotationSpeed, Vec3, activeParticleEmitter(), setRandomRotationSpeed, getRandomRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(rotationSpeed, Vec3, activeParticleEmitter(), setRotationSpeed, getRotationSpeed, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(randomRotation, Vec3, activeParticleEmitter(), setRandomRotation, getRandomRotation, kParticleConsoleTag)
BF2_CONSOLE_PROPERTY(speedFromParticle, float, activeParticleEmitter(), setSpeedFromParticle, getSpeedFromParticle, kParticleConsoleTag)

} } } // namespace dice::hfe::world
