// Particle manager, instance, and template interfaces shared with world-side callers.
// Preserve virtual declaration order and value/reference semantics across the image boundary.

#ifndef GEOM_PARTICLESYSTEM_PARTICLESYSTEMMANAGER_H
#define GEOM_PARTICLESYSTEM_PARTICLESYSTEMMANAGER_H

#if defined(BF2_IMAGE_RENDDX9)
#include "../../../RendDX9Abi/HfeAbi.h"
#else
#include "../../Core/IBase.h"
#include <string>
#endif

#include "../../Core/Random.h"
#include "../../Math/Vec3.h"

#include <cstddef>
#include <stdint.h>
#include <vector>

namespace dice {
namespace hfe {
namespace world { class IEffectController; }
namespace geom {

#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString ParticleSystemName;
#else
typedef std::string ParticleSystemName;
#endif

enum GasCloudType : int;

class IParticleSystemTemplate;

// The five concrete runtime classes share this 51-entry interface. PE vtables 0x1021FE48,
// 0x10220058, 0x1021FBC0, 0x1021FF60 and 0x1021FD48 establish every position and machine
// signature. The reserved names retain exactly the observed argument widths/return
// materialisation; no neutral void() padding is used. Four semantic names are additionally pinned
// by their BF2.exe callers and the manager broadcast at PE 0x100D3300.
class IParticleSystemInstance
{
protected:
    virtual ~IParticleSystemInstance() {}                                      // +0x00 (MSVC)

public:
    virtual int reserved01() const = 0;                                       // +0x04
    virtual void reserved02() const = 0;                                      // +0x08 clear particles
    virtual void reserved03(bool firstPerson) = 0;                            // +0x0C start
    virtual void reserved04() = 0;                                            // +0x10 stop
    virtual bool reserved05() const = 0;                                      // +0x14
    virtual void setPaused(bool paused) = 0;                                  // +0x18
    virtual bool reserved07() const = 0;                                      // +0x1C
    virtual void reserved08() = 0;                                            // +0x20 detach
    virtual void reserved09(bool value) = 0;                                  // +0x24
    virtual bool reserved10() const = 0;                                      // +0x28
    // +0x2C..+0x34 carry the owner object and the parent matrix as integers; intptr_t is `int` on
    // x86 (same signature) and pointer-sized on x64.
    virtual void reserved11(intptr_t value) = 0;                              // +0x2C
    virtual intptr_t reserved12() const = 0;                                  // +0x30
    virtual void reserved13(intptr_t value) = 0;                              // +0x34
    virtual void* reserved14(const void* value) = 0;                          // +0x38
    virtual const void* reserved15() const = 0;                               // +0x3C
    virtual void reserved16(const Vec3& value) = 0;                           // +0x40
    virtual const Vec3& reserved17() const = 0;                               // +0x44
    virtual void reserved18(const void* value) = 0;                           // +0x48
    virtual void reserved19(float value) = 0;                                 // +0x4C speed
    virtual float reserved20() const = 0;                                     // +0x50
    virtual void reserved21(int first, int second) = 0;                       // +0x54
    virtual void reserved22(float value) = 0;                                 // +0x58
    virtual float reserved23() const = 0;                                     // +0x5C
    virtual void reserved24(float value) = 0;                                 // +0x60
    virtual float reserved25() const = 0;                                     // +0x64
    virtual void reserved26(float value) = 0;                                 // +0x68
    virtual float reserved27() const = 0;                                     // +0x6C
    virtual void reserved28(const Vec3& value) = 0;                           // +0x70
    virtual const Vec3& reserved29() const = 0;                               // +0x74
    virtual void reserved30(float value) = 0;                                 // +0x78
    virtual float reserved31() const = 0;                                     // +0x7C
    virtual void reserved32(float value) = 0;                                 // +0x80
    virtual float reserved33() const = 0;                                     // +0x84
    virtual int reserved34() const = 0;                                       // +0x88
    virtual IParticleSystemTemplate* getTemplate() const = 0;                 // +0x8C
    virtual void reserved36(const Vec3& value) = 0;                           // +0x90
    virtual void reserved37() = 0;                                            // +0x94
    virtual float reserved38() const = 0;                                     // +0x98
    virtual int reserved39() const = 0;                                       // +0x9C
    virtual const Vec3& getPosition() const = 0;                              // +0xA0
    virtual void reserved41(float value) = 0;                                 // +0xA4
    virtual void reserved42(bool value) = 0;                                  // +0xA8
    virtual float getGasCloudRadius() const = 0;                              // +0xAC
    virtual void setPlayer(void* player) = 0;                                 // +0xB0
    virtual void* getPlayer() const = 0;                                      // +0xB4
    virtual void setMaterial(int material) = 0;                               // +0xB8
    virtual int getMaterial() const = 0;                                     // +0xBC
    virtual void setEffectController(world::IEffectController* controller) = 0;// +0xC0
    virtual world::IEffectController* getEffectController() const = 0;         // +0xC4
    virtual bool getWaterSide() const = 0;                                   // +0xC8

    // Nonvirtual ownership/field bridges: no new slot or layout. PE 100D3F30 deletes through
    // +0x00; BF2.exe EffectBundle writes the common FOV override byte at instance +0x08.
    void destroy() { delete this; }
    void setFovOverride(bool enabled) { reinterpret_cast<unsigned char*>(this)[8] = enabled; }
};

// Common descriptor surface: the shortest concrete descriptor (type 3) has 99 PE entries, so
// every one of these first 89 positions exists on all five types. Names and parameter types are
// attested by the symbolized ELF ParticleSystemTemplateBase forwarders; the PE slot is one entry
// earlier because Itanium emits two destructor entries and MSVC emits one.
class IParticleSystemTemplate
{
protected:
    virtual ~IParticleSystemTemplate() {}                                      // +0x000 (MSVC)

public:
    virtual const ParticleSystemName& getName() const = 0;                    // +0x004
    virtual void refreshDerivedState() = 0;                                   // +0x008
    virtual int getSystemType() const = 0;                                    // +0x00C
    virtual void setEmitPrio(int value) = 0;                                  // +0x010
    virtual int getEmitPrio() const = 0;                                      // +0x014
    virtual void setLodDistance(int value) = 0;                               // +0x018
    virtual int getLodDistance() const = 0;                                   // +0x01C
    virtual void setQualityLevel(int value) = 0;                              // +0x020
    virtual int getQualityLevel() const = 0;                                  // +0x024
    virtual void setEmitRadius(Vec3 value) = 0;                               // +0x028
    virtual Vec3 getEmitRadius() const = 0;                                   // +0x02C
    virtual void setRadialDirection(float value) = 0;                         // +0x030
    virtual float getRadialDirection() const = 0;                             // +0x034
    virtual void setRandomRadialDirection(float value) = 0;                   // +0x038
    virtual float getRandomRadialDirection() const = 0;                       // +0x03C
    virtual void setEmitterType(int value) = 0;                               // +0x040
    virtual void setEmitRangeX(const RandomDistribution& value) = 0;          // +0x044
    virtual const RandomDistribution& getEmitRangeX() const = 0;              // +0x048
    virtual void setEmitRangeY(const RandomDistribution& value) = 0;          // +0x04C
    virtual const RandomDistribution& getEmitRangeY() const = 0;              // +0x050
    virtual void setEmitRangeZ(const RandomDistribution& value) = 0;          // +0x054
    virtual const RandomDistribution& getEmitRangeZ() const = 0;              // +0x058
    virtual void setEmitFrequency(float value) = 0;                           // +0x05C
    virtual float getEmitFrequency() const = 0;                               // +0x060
    virtual void setEmitSpeed(float value) = 0;                               // +0x064
    virtual float getEmitSpeed() const = 0;                                   // +0x068
    virtual void setPreWarmTime(float value) = 0;                             // +0x06C
    virtual float getPreWarmTime() const = 0;                                 // +0x070
    virtual void setEmitTime(float value) = 0;                                // +0x074
    virtual float getEmitTime() const = 0;                                    // +0x078
    virtual void setIsLooping(bool value) = 0;                                // +0x07C
    virtual bool getIsLooping() const = 0;                                    // +0x080
    virtual void setEmitDelay(float value) = 0;                               // +0x084
    virtual float getEmitDelay() const = 0;                                   // +0x088
    virtual void setRandomDirectionAngle(const Vec3& value) = 0;              // +0x08C
    virtual Vec3 getRandomDirectionAngle() const = 0;                         // +0x090
    virtual void setEmitDirection(const Vec3& value) = 0;                     // +0x094
    virtual Vec3 getEmitDirection() const = 0;                                // +0x098
    virtual void setRotationGraph(const Vec4& value) = 0;                     // +0x09C
    virtual const Vec4& getRotationGraph() const = 0;                         // +0x0A0
    virtual void setAlignRotationToSpeed(bool value) = 0;                     // +0x0A4
    virtual bool getAlignRotationToSpeed() const = 0;                         // +0x0A8
    virtual void setRandomTimeToLive(float value) = 0;                        // +0x0AC
    virtual float getRandomTimeToLive() const = 0;                            // +0x0B0
    virtual void setTimeToLive(float value) = 0;                              // +0x0B4
    virtual float getTimeToLive() const = 0;                                  // +0x0B8
    virtual void setGravity(float value) = 0;                                 // +0x0BC
    virtual float getGravity() const = 0;                                     // +0x0C0
    virtual void setAirResistance(float value) = 0;                           // +0x0C4
    virtual float getAirResistance() const = 0;                               // +0x0C8
    virtual void setRandomSpeed(float value) = 0;                             // +0x0CC
    virtual float getRandomSpeed() const = 0;                                 // +0x0D0
    virtual void setGravityGraph(const Vec4& value) = 0;                      // +0x0D4
    virtual const Vec4& getGravityGraph() const = 0;                          // +0x0D8
    virtual void setAirResistanceGraph(const Vec4& value) = 0;                // +0x0DC
    virtual const Vec4& getAirResistanceGraph() const = 0;                    // +0x0E0
    virtual void setEmitSpeedGraph(const Vec4& value) = 0;                    // +0x0E4
    virtual const Vec4& getEmitSpeedGraph() const = 0;                        // +0x0E8
    virtual void setTransparencyGraph(const Vec4& value) = 0;                 // +0x0EC
    virtual const Vec4& getTransparencyGraph() const = 0;                     // +0x0F0
    virtual void setWindAffectionFactor(float value) = 0;                     // +0x0F4
    virtual float getWindAffectionFactor() const = 0;                         // +0x0F8
    virtual void setCollisionEnable(bool value) = 0;                          // +0x0FC
    virtual bool getCollisionEnable() const = 0;                              // +0x100
    virtual void setCollisionSpeedAlongNormal(float value) = 0;               // +0x104
    virtual float getCollisionSpeedAlongNormal() const = 0;                   // +0x108
    virtual void setCollisionSpeedAlongPlane(float value) = 0;                // +0x10C
    virtual float getCollisionSpeedAlongPlane() const = 0;                    // +0x110
    virtual void setIsLocalSystem(bool value) = 0;                            // +0x114
    virtual bool getIsLocalSystem() const = 0;                                // +0x118
    virtual void setForceToWaterSurface(bool value) = 0;                      // +0x11C
    virtual bool getForceToWaterSurface() const = 0;                          // +0x120
    virtual void setWaterSurfaceOffset(float value) = 0;                      // +0x124
    virtual float getWaterSurfaceOffset() const = 0;                          // +0x128
    virtual void setGasCloudType(GasCloudType value) = 0;                     // +0x12C
    virtual GasCloudType getGasCloudType() const = 0;                         // +0x130
    virtual void setBlurryVisionTime(float value) = 0;                        // +0x134
    virtual float getBlurryVisionTime() const = 0;                            // +0x138
    virtual void setBlurryVisionHoldTime(float value) = 0;                    // +0x13C
    virtual float getBlurryVisionHoldTime() const = 0;                        // +0x140
    virtual void setMinGasCloudRadius(float value) = 0;                       // +0x144
    virtual float getMinGasCloudRadius() const = 0;                           // +0x148
    virtual void setMaxGasCloudRadius(float value) = 0;                       // +0x14C
    virtual float getMaxGasCloudRadius() const = 0;                           // +0x150
    virtual void setGasCloudRadiusTime(float value) = 0;                      // +0x154
    virtual float getGasCloudRadiusTime() const = 0;                          // +0x158
    virtual void setGasCloudDamage(float value) = 0;                          // +0x15C
    virtual float getGasCloudDamage() const = 0;                              // +0x160

    // Nonvirtual ownership bridge; preserves the existing virtual layout.
    void destroy() { delete this; }
};

// Shared sprite/mesh descriptor extension, PE slots 89..113. The one unnamed Vec3 setter at 105
// is instruction-proven (all relevant PE descriptors take one Vec3 pointer there); its source
// spelling has not been recovered, so only the name stays reserved.
class IParticleSpriteSystemTemplate : public IParticleSystemTemplate
{
public:
    virtual void setParticleMaxSize(float value) = 0;                         // +0x164
    virtual float getParticleMaxSize() const = 0;                             // +0x168
    virtual void setRandomSize(float value) = 0;                              // +0x16C
    virtual float getRandomSize() const = 0;                                  // +0x170
    virtual void setRandomIntensity(float value) = 0;                         // +0x174
    virtual float getRandomIntensity() const = 0;                             // +0x178
    virtual void setSizeGraph(const Vec4& value) = 0;                         // +0x17C
    virtual const Vec4& getSizeGraph() const = 0;                             // +0x180
    virtual void setAlphaCull(float value) = 0;                               // +0x184
    virtual float getAlphaCull() const = 0;                                   // +0x188
    virtual void setLightAffectionFactor(float value) = 0;                    // +0x18C
    virtual float getLightAffectionFactor() const = 0;                        // +0x190
    virtual void setColor1(const Vec3& value) = 0;                            // +0x194
    virtual Vec3 getColor1() const = 0;                                       // +0x198
    virtual void setColor2(const Vec3& value) = 0;                            // +0x19C
    virtual Vec3 getColor2() const = 0;                                       // +0x1A0
    virtual void reserved105(const Vec3& value) = 0;                          // +0x1A4
    virtual void setColorBlendGraph(const Vec4& value) = 0;                   // +0x1A8
    virtual const Vec4& getColorBlendGraph() const = 0;                       // +0x1AC
    virtual void setLightMapIntensityOffset(float value) = 0;                 // +0x1B0
    virtual float getLightMapIntensityOffset() const = 0;                     // +0x1B4
    virtual void setDirectionalScale(float value) = 0;                        // +0x1B8
    virtual float getDirectionalScale() const = 0;                            // +0x1BC
    virtual void setIsStaticSystem(bool value) = 0;                           // +0x1C0
    virtual bool getIsStaticSystem() const = 0;                               // +0x1C4
};

class IParticleSystemTemplateType0 : public IParticleSpriteSystemTemplate
{
public:
    virtual void setParticleType(int value) = 0;                              // +0x1C8
    virtual const int& getParticleTypeRef() const = 0;                        // +0x1CC
    virtual void setDrawOrder(int value) = 0;                                 // +0x1D0
    // Returns draw order by value.
    virtual int getDrawOrder() const = 0;                                     // +0x1D4

    // Authoring tail: ELF world forwarders 8213200..8213880; PE descriptor
    // 1021F300 slots118..151. The Vec2 pair's semantic name remains unresolved.
    virtual void reserved118(const Vec2& value) = 0; // +0x1D8
    virtual Vec2 reserved119() const = 0; // +0x1DC
    virtual void setRandomRotationSpeed(float value) = 0; // +0x1E0
    virtual float getRandomRotationSpeed() const = 0; // +0x1E4
    virtual void setRotationSpeed(float value) = 0; // +0x1E8
    virtual float getRotationSpeed() const = 0; // +0x1EC
    virtual void setPivotOffset(const Vec2& value) = 0; // +0x1F0
    virtual Vec2 getPivotOffset() const = 0; // +0x1F4
    virtual void setSelfShadowIntensity(float value) = 0; // +0x1F8
    virtual float getSelfShadowIntensity() const = 0; // +0x1FC
    virtual void setRandomRotation(float value) = 0; // +0x200
    virtual float getRandomRotation() const = 0; // +0x204
    virtual void setTextureName(const ParticleSystemName& value) = 0; // +0x208
    virtual const ParticleSystemName& getTextureName() const = 0; // +0x20C
    virtual void setAnimationPlayOnce(bool value) = 0; // +0x210
    virtual bool getAnimationPlayOnce() const = 0; // +0x214
    virtual void setAnimationEnable(bool value) = 0; // +0x218
    virtual bool getAnimationEnable() const = 0; // +0x21C
    virtual void setAnimationRandomizedStartFrame(bool value) = 0; // +0x220
    virtual bool getAnimationRandomizedStartFrame() const = 0; // +0x224
    virtual void setAnimationSpeed(float value) = 0; // +0x228
    virtual float getAnimationSpeed() const = 0; // +0x22C
    virtual void setAnimationFrameWidth(unsigned int value) = 0; // +0x230
    virtual unsigned int getAnimationFrameWidth() const = 0; // +0x234
    virtual void setAnimationFrameHeight(unsigned int value) = 0; // +0x238
    virtual unsigned int getAnimationFrameHeight() const = 0; // +0x23C
    virtual void setAnimationFrameWidthRelative(float value) = 0; // +0x240
    virtual float getAnimationFrameWidthRelative() const = 0; // +0x244
    virtual void setAnimationFrameHeightRelative(float value) = 0; // +0x248
    virtual float getAnimationFrameHeightRelative() const = 0; // +0x24C
    virtual void setAnimationFrameCount(unsigned int value) = 0; // +0x250
    virtual unsigned int getAnimationFrameCount() const = 0; // +0x254
    virtual void setAnimationFrameCountX(unsigned int value) = 0; // +0x258
    virtual unsigned int getAnimationFrameCountX() const = 0; // +0x25C
};

// NSA authoring tail: ELF 8205760..8205E10, PE descriptor1021F5B8 slots114..147.
// The ordering differs from type0 (random rotation precedes pivot/self-shadow).
class IParticleSystemTemplateType4 : public IParticleSpriteSystemTemplate
{
public:
    virtual void reserved114(const Vec2& value) = 0; // +0x1C8
    virtual Vec2 reserved115() const = 0; // +0x1CC
    virtual void setRandomRotationSpeed(float value) = 0; // +0x1D0
    virtual float getRandomRotationSpeed() const = 0; // +0x1D4
    virtual void setRotationSpeed(float value) = 0; // +0x1D8
    virtual float getRotationSpeed() const = 0; // +0x1DC
    virtual void setRandomRotation(float value) = 0; // +0x1E0
    virtual float getRandomRotation() const = 0; // +0x1E4
    virtual void setPivotOffset(const Vec2& value) = 0; // +0x1E8
    virtual Vec2 getPivotOffset() const = 0; // +0x1EC
    virtual void setSelfShadowIntensity(float value) = 0; // +0x1F0
    virtual float getSelfShadowIntensity() const = 0; // +0x1F4
    virtual void setTextureName(const ParticleSystemName& value) = 0; // +0x1F8
    virtual const ParticleSystemName& getTextureName() const = 0; // +0x1FC
    virtual void setAnimationPlayOnce(bool value) = 0; // +0x200
    virtual bool getAnimationPlayOnce() const = 0; // +0x204
    virtual void setAnimationEnable(bool value) = 0; // +0x208
    virtual bool getAnimationEnable() const = 0; // +0x20C
    virtual void setAnimationRandomizedStartFrame(bool value) = 0; // +0x210
    virtual bool getAnimationRandomizedStartFrame() const = 0; // +0x214
    virtual void setAnimationSpeed(float value) = 0; // +0x218
    virtual float getAnimationSpeed() const = 0; // +0x21C
    virtual void setAnimationFrameWidth(unsigned int value) = 0; // +0x220
    virtual unsigned int getAnimationFrameWidth() const = 0; // +0x224
    virtual void setAnimationFrameHeight(unsigned int value) = 0; // +0x228
    virtual unsigned int getAnimationFrameHeight() const = 0; // +0x22C
    virtual void setAnimationFrameWidthRelative(float value) = 0; // +0x230
    virtual float getAnimationFrameWidthRelative() const = 0; // +0x234
    virtual void setAnimationFrameHeightRelative(float value) = 0; // +0x238
    virtual float getAnimationFrameHeightRelative() const = 0; // +0x23C
    virtual void setAnimationFrameCount(unsigned int value) = 0; // +0x240
    virtual unsigned int getAnimationFrameCount() const = 0; // +0x244
    virtual void setAnimationFrameCountX(unsigned int value) = 0; // +0x248
    virtual unsigned int getAnimationFrameCountX() const = 0; // +0x24C
};

// Trail descriptor tail, PE10220308 slots114..145; symbolized world forwarding
// ELF8214190..82147C0 supplies names/types. The initial Vec2 pair remains unnamed.
class IParticleSystemTemplateType2 : public IParticleSpriteSystemTemplate
{
public:
    virtual void reserved114(const Vec2& value) = 0; // +0x1C8
    virtual Vec2 reserved115() const = 0; // +0x1CC
    virtual void setTextureName(const ParticleSystemName& value) = 0; // +0x1D0
    virtual const ParticleSystemName& getTextureName() const = 0; // +0x1D4
    virtual void setAnimationEnable(bool value) = 0; // +0x1D8
    virtual bool getAnimationEnable() const = 0; // +0x1DC
    virtual void setAnimationSpeed(float value) = 0; // +0x1E0
    virtual float getAnimationSpeed() const = 0; // +0x1E4
    virtual void setAnimationFrameWidth(unsigned int value) = 0; // +0x1E8
    virtual unsigned int getAnimationFrameWidth() const = 0; // +0x1EC
    virtual void setAnimationFrameHeight(unsigned int value) = 0; // +0x1F0
    virtual unsigned int getAnimationFrameHeight() const = 0; // +0x1F4
    virtual void setAnimationFrameWidthRelative(float value) = 0; // +0x1F8
    virtual float getAnimationFrameWidthRelative() const = 0; // +0x1FC
    virtual void setAnimationFrameHeightRelative(float value) = 0; // +0x200
    virtual float getAnimationFrameHeightRelative() const = 0; // +0x204
    virtual void setAnimationFrameCount(unsigned int value) = 0; // +0x208
    virtual unsigned int getAnimationFrameCount() const = 0; // +0x20C
    virtual void setAnimationFrameCountX(unsigned int value) = 0; // +0x210
    virtual unsigned int getAnimationFrameCountX() const = 0; // +0x214
    virtual void setFadeInFactor(float value) = 0; // +0x218
    virtual float getFadeInFactor() const = 0; // +0x21C
    virtual void setFadeOutFactor(float value) = 0; // +0x220
    virtual float getFadeOutFactor() const = 0; // +0x224
    virtual void setTextureTileFactor(float value) = 0; // +0x228
    virtual float getTextureTileFactor() const = 0; // +0x22C
    virtual void setTextureOffsetVelocity(float value) = 0; // +0x230
    virtual float getTextureOffsetVelocity() const = 0; // +0x234
    virtual void setNumPlanes(unsigned int value) = 0; // +0x238
    virtual unsigned int getNumPlanes() const = 0; // +0x23C
    virtual void setMinTrailVelocity(float value) = 0; // +0x240
    virtual float getMinTrailVelocity() const = 0; // +0x244
};

class IParticleSystemTemplateType1 : public IParticleSpriteSystemTemplate
{
public:
    virtual void setGeometry(const ParticleSystemName& value) = 0;             // +0x1C8
    virtual const ParticleSystemName& getGeometry() const = 0;                 // +0x1CC
    virtual void setDrawOrder(int value) = 0;                                 // +0x1D0
    virtual int getDrawOrder() const = 0;                                     // +0x1D4
    virtual void setWriteZ(bool value) = 0;                                   // +0x1D8
    virtual bool getWriteZ() const = 0;                                       // +0x1DC
    virtual void setBlendMode(int value) = 0;                                 // +0x1E0
    // Vec3 setters take values, and getters return values through the ABI's hidden result
    // pointer. Keep value semantics on both MSVC and Itanium; do not use references.
    virtual int getBlendMode() const = 0;                                     // +0x1E4
    virtual void setRandomRotation(Vec3 value) = 0;                           // +0x1E8
    virtual Vec3 getRandomRotation() const = 0;                               // +0x1EC
    virtual void setRandomRotationSpeed(Vec3 value) = 0;                      // +0x1F0
    virtual Vec3 getRandomRotationSpeed() const = 0;                          // +0x1F4
    virtual void setRotationSpeed(Vec3 value) = 0;                            // +0x1F8
    virtual Vec3 getRotationSpeed() const = 0;                                // +0x1FC
};

// Type-3 emitter interface derives directly from the base template, not the sprite interface.
// Its extension slots are independent of same-numbered sprite slots.
class IParticleSystemEmitterTemplate : public IParticleSystemTemplate
{
public:
    virtual void setParticleSystemTemplate(const ParticleSystemName& value) = 0;  // +0x164
    virtual const ParticleSystemName& getParticleSystemTemplate() const = 0;      // +0x168
    virtual void setRandomRotation(Vec3 value) = 0;                           // +0x16C
    virtual Vec3 getRandomRotation() const = 0;                               // +0x170
    virtual void setRandomRotationSpeed(Vec3 value) = 0;                      // +0x174
    virtual Vec3 getRandomRotationSpeed() const = 0;                          // +0x178
    virtual void setRotationSpeed(Vec3 value) = 0;                            // +0x17C
    virtual Vec3 getRotationSpeed() const = 0;                                // +0x180
    virtual void setSpeedFromParticle(float value) = 0;                       // +0x184
    virtual float getSpeedFromParticle() const = 0;                           // +0x188
};

// The manager's active-instance vector, returned by reference at +0x38. BF2.exe
// (Game/GameLogic/Player.cpp) reads it directly: the client and its renderer share one STL
// (issue #7).
typedef std::vector<IParticleSystemInstance*> ParticleSystemInstanceList;

// +0x3C consumes exactly 60 bytes. Names for the four pool counts and final two renderer knobs are
// unresolved; their widths, offsets, and the float fields are recovered from 0x100D37A0.
struct ParticleSystemSettings
{
    /* +0x00 */ float distanceFactor;
    /* +0x04 */ int   poolCount04;
    /* +0x08 */ int   poolCount08;
    /* +0x0C */ int   poolCount0C;
    /* +0x10 */ float tierThresholds[3];
    /* +0x1C */ float lodScales[3];
    /* +0x28 */ float nearRatio;
    /* +0x2C */ float farRatio;
    /* +0x30 */ int   poolCount30;
    /* +0x34 */ int   shaderQuality;
    /* +0x38 */ int   qualityLevel;
};

static_assert(sizeof(ParticleSystemSettings) == 60,
              "ParticleSystemManager::applySettings consumes a 60-byte profile");
static_assert(offsetof(ParticleSystemSettings, tierThresholds) == 0x10,
              "particle tier thresholds moved");
static_assert(offsetof(ParticleSystemSettings, lodScales) == 0x1C,
              "particle LOD scales moved");
static_assert(offsetof(ParticleSystemSettings, nearRatio) == 0x28,
              "particle near ratio moved");
static_assert(offsetof(ParticleSystemSettings, farRatio) == 0x2C,
              "particle far ratio moved");
static_assert(offsetof(ParticleSystemSettings, shaderQuality) == 0x34,
              "particle shader-quality field moved");
static_assert(offsetof(ParticleSystemSettings, qualityLevel) == 0x38,
              "particle quality-level field moved");


// Complete manager interface, RendDX9.dll vtable 0x10219978 (18 entries / 0x48 bytes).
// Unknown source spellings stay reserved; their signatures and effects are nevertheless recovered.
// The destructor is last, which is why all operational offsets agree between the PE and ELF.

class IParticleSystemManager : public IBase
{
public:
    // +0x00..+0x0C: IBase quartet.

    // +0x10, PE 0x100D2E30. Calls +0x30, then reacquires the WindManager singleton. The exact
    // source name remains [UNRESOLVED]. postLoadLevel is the separate direct BF2 0x100D2FD0 body,
    // named by the Editor caller, which reacquires HeightmapCluster at manager +0x8C14.
    virtual void reserved_reacquireWindManager() = 0;

    virtual IParticleSystemInstance* createInstance(
        IParticleSystemTemplate* systemTemplate) = 0;                         // +0x14
    virtual void removeInstance(IParticleSystemInstance* instance) = 0;       // +0x18
    virtual IParticleSystemTemplate* createParticleSystem(
        int type, const ParticleSystemName& name) = 0;                        // +0x1C
    virtual void removeParticleSystem(IParticleSystemTemplate* system) = 0;   // +0x20
    // +0x24 releases/stops all live instances. Exact source spelling is [UNRESOLVED]; do not
    // confuse it with the Editor-profiled private deferred-delete drain named RemoveInstances.
    virtual void reserved_releaseAllInstances() = 0;                          // +0x24
    virtual void update(float deltaTime) = 0;                                 // +0x28
    virtual void onPathDraw(bool firstPerson, bool secondPass) = 0;           // +0x2C

    // +0x30, resetStats(). The BF2 build is an exact no-argument ret; the Editor counterpart and
    // console registration establish the spelling and reset purpose.
    virtual void resetStats() = 0;

    virtual void togglePause() = 0;                                           // +0x34
    // Machine-level proof is EAX=this+0x10. Pointer versus reference is not encoded by either
    // caller ABI, so this declaration conservatively exposes the proven pointer value.
    virtual ParticleSystemInstanceList* getInstances() = 0;                   // +0x38
    // PE proves a pointer to the 60-byte record; a source-level reference cannot be distinguished
    // from that machine ABI, so keep the declaration at the directly evidenced pointer shape.
    virtual void applySettings(const ParticleSystemSettings* settings) = 0;   // +0x3C

    // +0x40, resize the internal vector at +0x7CB0 to zero. Source name unresolved.
    virtual void reserved_clearInternalVector() = 0;

protected:
    virtual ~IParticleSystemManager() {}                                      // +0x44 (MSVC)
};

inline IParticleSystemManager* asParticleSystemManager(IBase* base)
{
    return static_cast<IParticleSystemManager*>(base);
}

// Direct concrete-manager calls at PE 0x100D2E80 / 0x100D2EA0.  They are deliberately free
// seams rather than invented virtual slots: both retail callers load the singleton into ECX and
// call the concrete functions directly.  The source-level names are [INFERRED] from their paired
// renderer-counter effects.
void particleSystemManagerBeginFrame(IParticleSystemManager* manager);
void particleSystemManagerEndFrame(IParticleSystemManager* manager);

// Direct ResourceManager lifecycle calls, PE 0x100D38D0 / 0x100D2FD0. The preLoadLevel
// spelling is [INFERRED] from the Editor ResourceManager caller; no virtual slots are added.
void particleSystemManagerPreLoadLevel(IParticleSystemManager* manager);
void particleSystemManagerPostLoadLevel(IParticleSystemManager* manager);
// Releases level instances/descriptors, queues, registries, renderer buffers, pools, and atlas.
void particleSystemManagerUnloadLevel(IParticleSystemManager* manager);

// The HeightmapCluster singleton postLoadLevel stored in the manager (retail +0x8C14); borrowed,
// possibly null. No null guard on `manager`.
IBase* particleSystemManagerHeightmapCluster(IBase* manager);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_PARTICLESYSTEM_PARTICLESYSTEMMANAGER_H
