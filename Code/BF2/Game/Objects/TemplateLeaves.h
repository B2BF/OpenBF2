// Template subclasses that override inherited virtuals without adding slots. [INFERRED] Source placement.
// GroundEffectBundle cannot instantiate an object. Emitter creation returns null in dedicated mode.
// EnvMap refuses binary serialization; specialized precache and physics overrides remain class-specific.

#ifndef GAME_OBJECTS_TEMPLATELEAVES_H     // [INFERRED] guard style
#define GAME_OBJECTS_TEMPLATELEAVES_H

// Aggregate template includes.
#include "AnimatedBundleTemplate.h"
#include "AntennaObjectTemplate.h"
#include "DestroyableObjectTemplate.h"
#include "ForceObjectTemplate.h"
#include "FreeCameraTemplate.h"
#include "GrapplingHookRopeTemplate.h"
#include "OverheadCameraTemplate.h"
#include "RemoteControlledObjectTemplate.h"
#include "SupplyObjectTemplate.h"
#include "TargetObjectTemplate.h"
#include "Game/Trigger.h" // TriggerTemplate and Trigger.
#include "Game/AmbientEffectArea.h" // AmbientEffectAreaTemplate and AmbientEffectArea.
#include "Game/SupplyDepot.h"

#include "Base/BundleTemplate.h"
#include "Base/SimpleObjectTemplate.h"
#include "PlayerControlObjectTemplate.h"
#include <list>
#include "../../Core/Random.h"
#include "../../Core/OverTimeDistribution.h"

namespace dice {
namespace anim { class IAnimationSystemTemplate; }
namespace hfe {
namespace world {

class Soldier;

// The five every leaf overrides. Signatures come from the bases, which derived them separately.
#define BF2_TEMPLATE_LEAF(cls)                                      \
    virtual unsigned int getClassID() const;                        \
    virtual ~cls();                                                 \
    virtual void* queryImplementation(unsigned int classId);        \
    virtual IObject* createObject() const

// Leaves overriding IBase also own a separate reference count after their base layout.
#define BF2_TEMPLATE_LEAF_IBASE                                     \
    virtual void  addRef();                                         \
    virtual int   getRef();                                         \
    virtual int   release();                                        \
    virtual void* queryInterface(unsigned int) const;               \
    int m_refCount

// SimpleObjectTemplate leaves share the 69-slot interface.

class DestroyableWindowTemplate : public SimpleObjectTemplate
{ public: DestroyableWindowTemplate();
          BF2_TEMPLATE_LEAF(DestroyableWindowTemplate); BF2_TEMPLATE_LEAF_IBASE;
          virtual bool makeScript(io::IStream*);
          int _dword168;                                           // [UNRESOLVED name], ctor 0
          int _dword16C; };                                        // [UNRESOLVED name], ctor 1

// Creates emitters on clients and returns null on dedicated servers.
class EmitterTemplate : public SimpleObjectTemplate
{ public: EmitterTemplate();
          BF2_TEMPLATE_LEAF(EmitterTemplate);
          virtual void preCache() const;                             // +0x6C  -  unique
          virtual bool makeScript(io::IStream*);

          void setStartProbability(float);
          void updateNumAlternateTemplates();

          // Names from the retail script writer; offsets here refer to the ELF. PE strings
          // have a different size, so callers must use these members rather than raw offsets.
          bool m_looping;                      // ELF +0x161 / PE +0x210 (ELF reuses base tail)
          bool m_addEmitterSpeed;              // ELF +0x162 / PE +0x211
          float m_emitterSpeedScale;           // +0x164
          bool m_startAtCreation;              // +0x168
          float m_startProbability;            // +0x16C
          float m_intensityAtSpeed;            // +0x170
          float m_lodDistance;                 // +0x174
          RandomDistribution m_delay;          // +0x178
          RandomDistribution m_timeToLive;     // +0x18C
          RandomDistribution m_intensity;      // +0x1A0
          OverTimeDistribution m_intensityOverTime; // +0x1B4
          std::string m_template;              // +0x360 PE +0x410
          std::string m_alternateTemplates[5]; // +0x364 PE +0x42C
          IObjectTemplate* m_alternateTemplateCache[5]; // +0x378 PE +0x4B8
          bool m_useCameraOrientation;         // +0x38C
          RandomDistribution m_relativePosition[3]; // +0x390: dof, up, right
          RandomDistribution m_positionalSpeed[3];  // +0x3CC: dof, up, right
          RandomDistribution m_rotationalSpeed[3];  // +0x408: dof, up, right
          RandomDistribution m_startRotation;  // +0x444
          RandomDistribution m_zRotation;      // +0x458
          float m_zRotationSnap;               // +0x46C
          bool m_hasOverDamage;                // +0x470
          bool m_noPhysics;                    // +0x471
          bool m_addChild;                     // +0x472
          bool m_showInFirstPerson;            // +0x473
          bool m_showInThirdPerson;            // +0x474
          bool m_moveToWaterSurface;           // +0x475
          bool m_unknown476;                   // +0x476 [UNRESOLVED name], ctor false
          int m_numAlternateTemplates;         // +0x478
          bool m_randomizeGeomPart;            // +0x47C
          int m_geometryPartId;                // +0x480
};

class EntryPointTemplate : public SimpleObjectTemplate
{ public: BF2_TEMPLATE_LEAF(EntryPointTemplate);
          EntryPointTemplate();                                      // m_entryRadius = 3.0f
          virtual bool makeScript(io::IStream*);
          float getEntryRadius() const { return m_entryRadius; }
          float m_entryRadius; };                                    // +0x164 (ObjectTemplate.setEntryRadius)

//  The only class overriding writeBinary/readBinary  -  with 7-byte bodies that return false.
// It declines binary serialisation. Also overrides templateInitialize.
class EnvMapTemplate : public SimpleObjectTemplate
{ public: BF2_TEMPLATE_LEAF(EnvMapTemplate); BF2_TEMPLATE_LEAF_IBASE;
          EnvMapTemplate();                                         // ELF8201D60 / clientPE64E720
          virtual void templateInitialize();                         // +0x1C
          virtual bool writeBinary(io::IStream*) const;              // +0x20  -  returns false
          virtual bool readBinary(io::IStream*);                     // +0x24  -  returns false
          virtual bool makeScript(io::IStream*);
          void* m_envMap; };                                        // ELF+168 / clientPE+214, borrowed renderer template

// createObject returns null; this template cannot instantiate an object.
class GroundEffectBundleTemplate : public SimpleObjectTemplate
{ public: BF2_TEMPLATE_LEAF(GroundEffectBundleTemplate);
          GroundEffectBundleTemplate();
          virtual void templateInitialize();                         // +0x1C  -  empty
          virtual bool makeScript(io::IStream*);                     // ELF8202E70 / dedicated PE5FE6E0
          float m_maxTriggerDistance; };                            // ELF+164; ctor3.0, writer-named

class HookLinkTemplate : public SimpleObjectTemplate
{ public: HookLinkTemplate();                                        // refcount + flag 0x40
          BF2_TEMPLATE_LEAF(HookLinkTemplate); BF2_TEMPLATE_LEAF_IBASE; };

class LadderTemplate : public SimpleObjectTemplate                   // ELF 0x16C / PE 0x218
{
public:
    BF2_TEMPLATE_LEAF(LadderTemplate);
    BF2_TEMPLATE_LEAF_IBASE;

    void setSeatAnimationSystem(const std::string& name);
    const std::string& getSeatAnimationSystem();

private:
    friend class Ladder;
    friend class LadderContainer;
    friend class PlayerControlObject;

    // ELF +0x168 / PE +0x214. The default member initializer is intentionally inline: retail emits
    // no standalone LadderTemplate constructor symbol, while its factory still zeroes this word.
    anim::IAnimationSystemTemplate* m_seatAnimationSystem = 0;
};

class RopeLinkTemplate : public SimpleObjectTemplate
{ public: RopeLinkTemplate();                                        // refcount + flag 0x40
          BF2_TEMPLATE_LEAF(RopeLinkTemplate); BF2_TEMPLATE_LEAF_IBASE; };

// SupplyDepotTemplate and its product are recovered in Game/SupplyDepot.h.

class ZiplineRopeTemplate : public SimpleObjectTemplate
{ public: ZiplineRopeTemplate();                                     // refcount + flag 0x40
          BF2_TEMPLATE_LEAF(ZiplineRopeTemplate); BF2_TEMPLATE_LEAF_IBASE;
          // ELF +0x168/+0x16C; PE +0x214/+0x230.  The constructor and destructor identify the
          // first member as std::string on both ABIs; no recovered accessor names its contents.
          std::string m_ropeName;                                    // [UNRESOLVED name], ctor ""
          float       m_lengthScale;                                 // [CHOSEN name], ctor 1.0f
};

// Zipline entryRadius is compared directly with squared distance; its default reach is sqrt(3) world units.
// EntryPoint's similarly named radius uses ordinary distance.
class ZiplineTemplate : public SimpleObjectTemplate
{ public: ZiplineTemplate();                                         // refcount + flag 0x40
          BF2_TEMPLATE_LEAF(ZiplineTemplate); BF2_TEMPLATE_LEAF_IBASE;

          void setZiplineSoldierHeight(float h);

          /* 0x168 */ std::string _string168;      // ctor = the empty rep [UNRESOLVED name]
          /* 0x16C */ float m_searchRadius;        // [CHOSEN name] ctor 50.0f - the grid query
                                                   // radius checkForEntry sweeps
          /* 0x170 */ float m_entryRadius;         // ELF-named, ctor 3.0f - see the above
          /* 0x174 */ float m_collisionStartRadius;// ELF-named, ctor 2.0f
          /* 0x178 */ float m_ziplineSoldierHeight;// console-named, ctor 1.4899999f
          /* 0x17C */ float _float17C;             // ctor 1000.0f; console ObjectTemplate.Timeout
                                                   // (PE +0x240, executor 0x4B9750); Zipline::
                                                   // updateTimeOut compares the live timer to it
          /* 0x180 */ float m_creationHeightOffset;// [CHOSEN name] ctor 0.5f; both placement
                                                   // helpers add it to their vertical origin
          /* 0x184 */ float m_minZiplineLength;    // [CHOSEN name] ctor 3.0f - checkForEntry squares
                                                   // this before comparing, so it IS a length
          /* 0x188 */ IPlayer* m_player;           // ctor 0; createZiplineObject stores its player

          // The Special Forces placement spine. The two trailing arguments on the collision
          // helpers are present in both mangled names/call sites even though this retail body does
          // not consume them directly.
          bool createZiplineObject(IPlayer*, Mat4&, IObject*);
          bool checkForRopeCreation(IPlayer*, const Mat4&, const Vec3&, Vec3&,
                                    bool, IObject*) const;
          bool findRopeCollisionPoint(IPlayer*, const Vec3&, const Vec3&, Vec3&,
                                      bool, IObject*) const;
          bool checkForVerticalObstruction(Soldier*) const;

          // Finds a zipline the player can grab and updates the attachment.
          // [INFERRED] Return type is void; callers inspect the soldier's parent afterward.
          void checkForEntry(IPlayer* player) const;
};

class DropVehicle;

// ClassFactory allocates 0x168 in the ELF and 0x214 in the PE. The old manifest's "30-byte"
// reading was the value stored in this one added field, not the allocation size.
class DropVehicleTemplate : public SimpleObjectTemplate              // ELF 0x168 / PE 0x214
{
public:
    DropVehicleTemplate();
    BF2_TEMPLATE_LEAF(DropVehicleTemplate);

private:
    friend class DropVehicle;
    friend struct GameTemplateConsoleAccess;     // recovered ObjectTemplate.noPassengerTimeToLive
    /* ELF +0x164 / PE +0x210 */ int m_destroyVehicleTime;   // console `noPassengerTimeToLive`
};

// BundleTemplate leaves share the 80-slot interface.

class AreaObjectTemplate : public BundleTemplate
{ public: AreaObjectTemplate();
          BF2_TEMPLATE_LEAF(AreaObjectTemplate);
          virtual bool makeScript(io::IStream*);
          void addLinePoint(const Vec2&);
          void setTriggerRadius(float);
          float getTriggerRadius();
          std::vector<Vec2> m_linePoints;       // ELF +0x17C / PE +0x230
          float m_triggerRadius;               // ELF +0x188 / PE +0x23C; ctor leaves unwritten
          LocalBoundingBox m_areaBounds;        // ELF +0x18C / PE +0x240
};

// Window-bundle settings, including respawn delay consumed directly by GameLogic.
class DestroyableWindowsBundleTemplate : public BundleTemplate       // (0x1C0)
{ public: DestroyableWindowsBundleTemplate();                        // read in full
          BF2_TEMPLATE_LEAF(DestroyableWindowsBundleTemplate); BF2_TEMPLATE_LEAF_IBASE;   // refcount +0x17C
          virtual bool makeScript(io::IStream*);
          IObjectTemplate* getEffectTemplate(const LocalBoundingBox&);
          /* 0x180 */ float       respawnDelay;              // ctor -1.0f - console `respawnDelay`
          /* 0x184 */ bool        clientHitDetection;        // ctor true  - console `clientHitDetection`
          /* 0x185 */ char        _pad185[3];
          /* 0x188 */ int         numWindows;                // ctor 0     - console `setNumWindows`
          /* 0x18C */ Math::Vec3  windowSizeVector;          // ctor = the TU's default (0.2, 1.0, 1.8)
                                                             // (a static Vec3 at ds:0x8AACF58) -
                                                             // console `windowSizeVector`
          /* 0x198 */ std::string effectTemplateBaseName;    // ctor "e_destroyableWindow_"
          /* 0x19C */ IObjectTemplate* m_effectTemplates[9];  // non-owning cache, width * 3 + height
};

class DynamicBundleTemplate : public BundleTemplate
{ public: DynamicBundleTemplate();                                   // physics-flag bits 1|2
          BF2_TEMPLATE_LEAF(DynamicBundleTemplate);
          virtual void templateInitialize();                         // +0x1C  -  empty
          virtual bool makeScript(io::IStream*);
          /* 0x17C */ float respawnDelay;                            // ctor -1.0f - console
                                                                     // Console and GameLogic access respawnDelay directly.
};

// Inherits makeScript.
class ObstacleTemplate : public BundleTemplate
{ public: ObstacleTemplate();
          BF2_TEMPLATE_LEAF(ObstacleTemplate);
          float m_unknown17C;                   // ELF +0x17C / PE +0x22C; ctor 5, name unresolved
};

// Builds the PhysicsSpring node through setPhysicsNodeComponent.
class SpringTemplate : public BundleTemplate
{ public: BF2_TEMPLATE_LEAF(SpringTemplate);
          SpringTemplate();                                          // (the six defaults below)
          virtual void templateInitialize();                         // +0x1C > WheelSound::initTemplates
          virtual void setPhysicsNodeComponent(IObject*) const;      // +0x80 new(0x258) PhysicsSpring
          virtual bool makeScript(io::IStream*);
          float getStrength() const { return m_strength; }           // (+0x198)
          float getDamping() const { return m_damping; }             // (+0x194)
          // Non-virtual console setters for spring strength and damping.
          void  setStrength(float v) { m_strength = v; }             // (+0x198)
          void  setDamping(float v)  { m_damping = v; }              // (+0x194)
          // Spring fixation and response; rotateUV scrolls texture instead of rotating the mesh.
          /* 0x17C */ Vec3  m_positionalFixation;                    // ctor 0
          /* 0x188 */ Vec3  m_axisFixation;                          // ctor (0,1,0)
          /* 0x194 */ float m_damping;                               // ctor 0.5 - PhysicsSpring::updatePhysics' damping term
          /* 0x198 */ float m_strength;                              // ctor 1.0 - PhysicsSpring::calcDefaultPosition's strength
          /* 0x19C */ bool  m_rotateUV;                              // ctor false
          /* 0x19D */ bool  m_fixAxisSpring; };                      // ctor false


class TriggerableTargetTemplate : public BundleTemplate
{ public: BF2_TEMPLATE_LEAF(TriggerableTargetTemplate); BF2_TEMPLATE_LEAF_IBASE;
          TriggerableTargetTemplate();                             // ELF81C8240
          virtual bool makeScript(io::IStream*);
          int m_unresolved180;                                     // ctor -1, no recovered reader
          unsigned int m_targetType;                               // +184, console name "type"
          float m_movementSpeed;                                   // +188, ctor1
          int m_movementType;                                      // +18C, 0 translation /1 rotation
          bool m_clockwise;                                        // +190, ctorfalse
          float m_delayBeforeStart;                                // +194, ctor0
          float m_delayBeforeCycle;                                // +198, ctor0
          float m_delayBeforeEnd;                                  // +19C, ctor0
          int m_cycles;                                            // +1A0, ctor1
          std::string m_unresolved1A4; };                          // ctor empty

class TriggerableTemplate : public BundleTemplate
{ public: BF2_TEMPLATE_LEAF(TriggerableTemplate); BF2_TEMPLATE_LEAF_IBASE;
          TriggerableTemplate();                                   // ELF81C5CE0 / PE5464D0
          virtual bool makeScript(io::IStream*);
          void setChance(float);
          void addTriggerableTarget(std::string);
          // Native list replaces the ELF +180 / PE +230 list. Remaining names unresolved.
          std::list<std::string> m_targetNames;
          int m_types;                                             // ELF +184, ctor 3
          float m_radius;                                          // +188, ctor 5
          int m_triggerId;                                         // +18C, ctor -1
          float m_float190, m_float194, m_float198;                  // ctor 100/10/1
          bool m_bool19C, m_bool19D, m_bool19E; };                   // ctor false x3

//  No makeScript override.
class UAVVehicleTemplate : public BundleTemplate
{ public: UAVVehicleTemplate();                                      // flag 0x40 only
          BF2_TEMPLATE_LEAF(UAVVehicleTemplate); };

// Rotor settings control the blade/blur swap threshold and hysteresis, per-geometry spin rate and rotation axis.
class RotorTemplate : public BundleTemplate                          // / PE 0x25C
{ public: BF2_TEMPLATE_LEAF(RotorTemplate);
          RotorTemplate();                                           // (the six defaults below)
          virtual bool makeScript(io::IStream*);
          // , non-virtual (`_ZN...13RotorTemplate15setRotationAxleEi`) - the
          // rotationAxle console row's setter. Ignores anything above 2 by an unsigned compare
          // (`cmp edx, 2 / ja`), so a negative axis is dropped too.
          void setRotationAxle(int axis);
          /* 0x17C */ float m_changeLodAt;                           // ctor 0.2   - the engine input at which the blur geom takes over
          /* 0x180 */ float m_tolerance;                             // ctor 0.025 - the hysteresis either side of it
          /* 0x184 */ std::string m_engineName;                      // ctor "" - findEngine's key [CHOSEN name] (Rotor::getEngine)
          /* 0x188 */ float m_rotationSpeedMod;                      // ctor 1.0   - spin scalar while geom 0 (the blades) is up
          /* 0x18C */ float m_rotationSpeedModBlur;                  // ctor 0.1   - spin scalar while geom 1 (the blur) is up
          /* 0x190 */ int   m_rotationAxle; };                       // ctor 2 - which Vec3 component of Rotor::m_rotation spins

// Player-control template subclasses retain the base virtual slots and add their own reference counters.
class LadderContainerTemplate : public PlayerControlObjectTemplate   // ELF 0x30C / PE 0x408
{ public: BF2_TEMPLATE_LEAF(LadderContainerTemplate); BF2_TEMPLATE_LEAF_IBASE;
          LadderContainerTemplate(); };

// Parachute template adds no fields; construction enables collision-group bit 8.
class ParachuteTemplate : public PlayerControlObjectTemplate         // ctor 76 B
{ public: BF2_TEMPLATE_LEAF(ParachuteTemplate);
          ParachuteTemplate();
          virtual bool makeScript(io::IStream*); };


// The LadderContainer's twin: same 0x30C shape, same refcount at +0x308 seeded to 1, same
// updateFlags(0x40) - and unlike the Ladder it re-declares the quartet, which is what named that
// field. createObject is recovered at ELF 0x81D7000 / PE 0x57F000 and news the complete
// ZiplineContainer (0x450 ELF / 0x4A0 PE).
class ZiplineContainerTemplate : public PlayerControlObjectTemplate  // 0x30C; ctor 103 B
{ public: BF2_TEMPLATE_LEAF(ZiplineContainerTemplate); BF2_TEMPLATE_LEAF_IBASE;
          ZiplineContainerTemplate(); };

// ELF 0x310 / PE 0x3BC. The constructor writes the usual leaf refcount at +0x308, a 1.5f
// Grapple-specific scalar at +0x30C, and updateFlags(0x40, 0).
class GrapplingHookRopeContainerTemplate : public PlayerControlObjectTemplate
{
public:
    BF2_TEMPLATE_LEAF(GrapplingHookRopeContainerTemplate);
    BF2_TEMPLATE_LEAF_IBASE;
    GrapplingHookRopeContainerTemplate();

private:
    friend class GrapplingHookRopeContainer;
    /* ELF +0x30C */ float m_float30C;                // ctor 1.5 [UNRESOLVED name]
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_TEMPLATELEAVES_H
