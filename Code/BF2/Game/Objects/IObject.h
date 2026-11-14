// Path: [INFERRED] from dice::hfe::world -> Game/Objects/.
// Runtime object interface and state shared by gameplay, physics and replication.
// The PE/ELF x86 base is 0x80 bytes; native ports use their actual pointer layout.

#ifndef GAME_OBJECTS_IOBJECT_H     // [INFERRED] guard style
#define GAME_OBJECTS_IOBJECT_H

#include <string>
#include <vector>       // getChilds' out-vector

#include "../../Core/IPersistant.h"
#include "../../Core/WeakPtr.h"
#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include "IObjectComponent.h"   // IObjectComponent was declared here; now fully recovered there

namespace dice {
namespace hfe {
struct LocalBoundingBox;
namespace io { class INetworkable; class PlayerInput; }
namespace geom { class IGeometry; }
namespace hud { class BF2HudManager; class HudVehicleInformation; class HudAmmo; class HudMessages; }
class DistributedLightRenderer;
class BF2FrameInterpolator;         // Game/Main/BF2FrameInterpolator.h - reads +0x08/+0x38 directly
namespace world {

class IPlayerControlObject;   // findOwningPlayerControlObject's return

// Object is defined in Object.h; pointer-only users need only this declaration.
class Object;
class SimpleObject;
class CullerObject;
class CollisionFaceInfo;
class IObject;
class IObjectTemplate;
class IPlayer;
enum UpdateFreqencyType : int;
enum ObjectLodType : int;

namespace object_detail { struct LayoutAccess; }

// Symbol-attested Bundle.cpp helpers.  Their friendship preserves the shipped direct field tests
// at IObject +0x38/+0x40 without inventing public geometry/collision accessors.
void setChildPartGeometries(IObject*, geom::IGeometry*);
void setChildPartCollisionMeshes(const IObjectTemplate*, IObject*, const std::string&);

// Runtime object base: 0x80 bytes on x86. Derived constructors initialize the remaining fields.
// m_networkable is non-owning; preserve direct cached-component reads.
// Keep all 90 logical virtual slots in order. getWeakPtr and getArmorInfo return by value.
class IObject : public IPersistant
{
public:
    // complete 90-slot interface table.  The first four entries come from IBase.  Slot numbers
    // below are logical source slots; GCC emits two destructor entries while MSVC emits one.
    virtual unsigned int getClassID() const = 0;                         // slot 4
    virtual ~IObject();                                                  // slots 5/6 (ELF)
    virtual WeakPtr<IObject> getWeakPtr();                               // slot 7
    virtual void init() = 0;                                             // slot 8
    virtual void disableCachedObject();                                  // slot 9
    virtual void enableCachedObject();                                   // slot 10
    virtual unsigned int getObjectCacheID() const;                       // slot 11
    virtual void setObjectCacheID(unsigned int);                         // slot 12
    virtual void destroy(bool force) = 0;                                // slot 13
    // Set useForceDestroy around the destroy(true) dispatch, then clear it.
    virtual void forceDestroy();                                         // slot 14
    virtual bool isObjectDestroyed() = 0;                                // slot 15
    virtual void setObjectInDestroyList(bool) = 0;                       // slot 16
    virtual bool getObjectInDestroyList() const = 0;                     // slot 17
    virtual void setGridCullerObject(CullerObject*);                     // slot 18
    virtual CullerObject* getGridCullerObject() const;                   // slot 19
    virtual void setLatencyCullerObject(CullerObject*);                  // slot 20
    virtual CullerObject* getLatencyCullerObject() const;                // slot 21
    // Passing null detaches the component.
    virtual bool setComponent(unsigned int classId, IBase*) = 0;         // slot 22
    virtual IBase* queryComponent(unsigned int classId,
                                  unsigned int iid) = 0;                 // slot 23
    virtual void updateFlags(unsigned int set,
                             unsigned int clear) = 0;                    // slot 24
    virtual const std::string& getName() const = 0;                      // slot 25
    virtual void setName(const std::string&) = 0;                        // slot 26
    virtual const Vec3& getAbsolutePosition() const = 0;                 // slot 27
    virtual void setAbsolutePosition(const Vec3&) = 0;                   // slot 28
    virtual const Vec3& getAbsolutePositionSecondary() const = 0;        // slot 29
    virtual void setAbsolutePositionSecondary(const Vec3&) = 0;         // slot 30
    virtual const Vec3& getRotationSecondary() = 0;                      // slot 31
    virtual void setRotationSecondary(const Vec3&) = 0;                 // slot 32
    virtual const Mat4& getAbsoluteTransformation() const = 0;           // slot 33
    virtual const Mat4& getInvAbsoluteTransformation() const = 0;        // slot 34
    virtual void setAbsoluteTransformation(const Mat4&) = 0;            // slot 35
    virtual const Mat4& getVisualAbsoluteTransformation() const = 0;     // slot 36
    virtual float getVisualBoundingRadius() const = 0;                   // slot 37
    virtual float getPhysicBoundingRadius() const = 0;                   // slot 38
    virtual float getBoundingRadiusModifier() const = 0;                 // slot 39
    virtual LocalBoundingBox* getFullBoundingBox() = 0;                  // slot 40
    virtual LocalBoundingBox* getMineBoundingBox() = 0;                  // slot 41
    virtual void handleVisualUpdate(float, float) = 0;                   // slot 42
    virtual void handleFrameUpdate(float) = 0;                           // slot 43
    virtual void handleUpdate(float, unsigned int) = 0;                  // slot 44
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&,
                                 const Vec3&, int, int,
                                 CollisionFaceInfo*, CollisionFaceInfo*) = 0; // slot 45
    virtual void updateGeometry() = 0;                                   // slot 46
    virtual int getUpdateFrequency() const = 0;                          // slot 47
    virtual UpdateFreqencyType getUpdateFrequencyType() const = 0;       // slot 48
    virtual void setUpdateFrequencyType(UpdateFreqencyType) = 0;         // slot 49
    virtual int getGeometryPart() const;                                 // slot 50
    virtual bool hasCollisionLod(int) const;                             // slot 51
    virtual int getMaterial() const = 0;                                // slot 52
    virtual void SetActiveLod(ObjectLodType);                            // slot 53
    virtual io::INetworkable* getNetworkable() const;                   // slot 54
    // The concrete renderer-decal interface name remains unresolved; both ports prove this is
    // not an IObject*.  Keep the pointer ABI honest until that supplied-renderer boundary lands.
    virtual void* getDecalObject() const;                               // slot 55
    virtual const Vec3& getRelativePosition() const = 0;                // slot 56
    virtual void setRelativePosition(const Vec3&) = 0;                  // slot 57
    virtual const Mat4& getRelativeTransformation() const = 0;          // slot 58
    virtual void setRelativeTransformation(const Mat4&) = 0;            // slot 59
    virtual IObject* getChild() const = 0;                              // slot 60
    virtual void addChild(IObject*) = 0;                                // slot 61
    virtual void removeChild(IObject*) = 0;                             // slot 62
    virtual void resetCachedRootParents() = 0;                          // slot 63
    virtual void handlePlayerInput(IPlayer*, const io::PlayerInput&,
                                   float, unsigned int) = 0;             // slot 64
    virtual void handleMessage(TemplateMessage, IPlayer*, bool) = 0;     // slot 65
    virtual char getInputId() const = 0;                                // slot 66
    virtual void setInputId(char) = 0;                                  // slot 67
    virtual char getChildId() const = 0;                                // slot 68
    virtual void setChildId(char) = 0;                                  // slot 69
    virtual unsigned char getAlpha() = 0;                               // slot 70
    virtual unsigned char getLayer() = 0;                               // slot 71
    virtual void setLayer(unsigned char) = 0;                            // slot 72
    virtual unsigned char getGroup() = 0;                               // slot 73
    virtual void setGroup(unsigned char) = 0;                            // slot 74
    virtual void setIsOvergrowth(bool) = 0;                              // slot 75
    virtual bool getIsOvergrowth() = 0;                                 // slot 76
    virtual void setNotInAI(bool) = 0;                                  // slot 77
    virtual bool getNotInAI() const = 0;                                // slot 78
    virtual void setLightSourceMask(int) = 0;                            // slot 79
    virtual int getLightSourceMask() const = 0;                          // slot 80
    virtual bool getDisableChildren() = 0;                               // slot 81
    virtual void setDisableChildren(bool) = 0;                           // slot 82
    virtual std::string getArmorInfo();                                  // slot 83
    virtual bool isCreatedByLocalPlayer() const;                         // slot 84
    virtual void internal_assignNewObjectID();                           // slot 85
    virtual void destroyObject();                                        // slot 86
    virtual void addSibling(IObject*) = 0;                               // slot 87
    virtual void removeSibling(IObject*) = 0;                            // slot 88
    virtual void setParent(IObject*) = 0;                                // slot 89

    // Inline/non-virtual field views used throughout the original runtime.  No getTemplate symbol
    // exists in either binary; callers compile this direct +0x14 load.
    const IObjectTemplate* getTemplate() const { return m_template; }
    // [INFERRED] inline name for the cached +0x34 load in AIPlayer::getCurrentVehicle:
    // /0x83FC870, dedicated PE 0x753059/0x753073. No virtual call or query.
    void* getAIComponent() const { return m_aiObject; }
    unsigned int getObjectId() const { return m_objectId; }
    unsigned int getFlags() const { return m_flags; }
    IObject* getParent() const { return m_parent; }
    IObject* getSibling() const { return m_sibling; }
    // NetworkManager::updateStateMasks tests it from outside the hierarchy
    // (the scopable include rule), and unlike Bundle/SimpleObject it is not a derived class.
    IObjectComponent* getArmorComponent() const { return m_armorComponent; }
    // Cached IVehicleHudComp interface pointer; no further pointer adjustment is needed.
    IObjectComponent* getVehicleHudComponent() const { return m_vehicleHudComponent; }
    // [INFERRED] inline spelling for the shipped direct +0x74 read. The cached component and its
    // placement are attested by Object::setComponent and the map painter; this avoids an ABI-bound
    // raw offset while retaining the original non-virtual access.
    IObjectComponent* getStrategicObjectComponent() const
    { return m_strategicObjectComponent; }

    static bool useForceDestroy;

    // Object flags; meanings come from recovered writers/readers, spellings are [INFERRED].
    // The mapping is partial.
    enum ObjectFlags
    {
        OFNone              = 0u,
        // Disabling sets this bit; enabling clears it. It also gates bits 11 and 13.
        OFIsDisabled        = 1u <<  0,
        // Gates culler admission to the render list; clearing it does not stop simulation
        // or replication. Soldier and destruction paths also write this bit.
        OFIsVisible         = 1u <<  1,
        // Named by setHasMobilePhysics/getHasMobilePhysics; distinct from isDynamic.
        // Soldiers, FreeCamera and SpriteParticle can set it without a physics node.
        OFHasMobilePhysics  = 1u <<  3,
        // The inherited half of the pair above: stamped on a child by Object::addChild when the
        // parent carries bit 3, and propagated by updateFlags via uf_childrenHelper (always with
        // mask 0x10, never 0x8). Its clear-propagation is gated by bit 3 being clear.
        OFParentHasMobilePhysics = 1u << 4,
        // Parked in ObjectCache: disableCachedObject sets it, enableCachedObject clears it.
        // Cached objects skip ghosting and client database pushes; saved flags are in m_cacheFlags.
        OFIsInObjectCache   = 1u <<  5,
        // ObjectManager::addObject/removeObject on the ResponsePhysicsManager singleton
        // (0x8B27848), then `anonymous namespace'::setResponseCacheToDirty.
        OFHasCollision      = 1u <<  8,
        // Live marker and double-destroy guard, cleared by internalDestroyObject.
        // World queries require it. isObjectDestroyed reads a separate byte at +0x125.
        OFIsAlive           = 1u <<  9,
        // ObjectManager +0x100 addObjectToGrid / +0x104 removeObjectFromGrid.
        // root objects only - the whole arm is under `if (!m_parent)`.
        OFHasGrid           = 1u << 10,
        // ObjectManager +0xF0 addObjectToUpdate / +0xF4 removeObjectFromUpdate.
        OFHasUpdates        = 1u << 11,
        // ObjectManager +0xF8 addObjectToFrameUpdate / +0xFC removeObjectFromFrameUpdate.
        OFHasFrameUpdates   = 1u << 13,
        // ObjectManager +0xE8 addObjectToNameHash / +0xEC removeObjectFromNameHash.
        OFHasNameHash       = 1u << 14,
        // `anonymous namespace'::setTransformationIsDirty. Carries a quirk on the clear path -
        // see the census note.
        OFHasDirtyTransform = 1u << 15,
        // attested - set and cleared by Object::setParent.
        OFIsRootParent      = 1u << 24
    };

    // reserved - updateFlags aborts when any of these appear in its SET mask (0x830A28x).
    static const unsigned int OF_RESERVED_MASK = 0x303C0000u;

protected:
    IObject();

    unsigned int m_flags;                       // +0x04
    Mat4* m_interpolationTransformation;         // +0x08 owned previous-frame transform
    IObject* m_cachedRootParent;                 // +0x0C lazily populated root-parent cache
    unsigned int m_objectId;                     // +0x10
    const IObjectTemplate* m_template;            // +0x14 non-owning
    unsigned int m_unknown18;                    // +0x18 [UNRESOLVED]
    CullerObject* m_gridCullerObject;             // +0x1C non-owning
    CullerObject* m_latencyCullerObject;          // +0x20 non-owning
    unsigned int m_objectCacheId;                 // +0x24
    IObject* m_parent;                            // +0x28 non-owning
    IObject* m_sibling;                           // +0x2C non-owning sibling link
    IObject* m_child;                             // +0x30 non-owning first-child link
    // Object::setComponent caches queryInterface(classId) results in these fields.
    // Setting or clearing geometry/collision dirties the bound radius; response physics
    // dirties the response cache. These side effects apply to both set and clear.
    void* m_aiObject;                             // +0x34 <- queryInterface(IID_IAIObject)
    void* m_geometry;                             // +0x38 <- IID_IGeometry; also dirties bound radius
    IObjectComponent* m_armorComponent;           // +0x3C <- IID_IArmorGLComp, non-owning. It is an
                                                  // IArmorGLComp* - SupplyObject::update calls
                                                  // isDestroyed() (slot 21) straight through it.
    void* m_collisionMesh;                        // +0x40 <- IID_ICollisionMesh; dirties bound radius
    void* m_physicsNode;                          // +0x44 <- queryInterface(IID_IPhysicsNode)
    void* m_responsePhysics;                      // +0x48 <- IID_IResponsePhysics; also dirties the
                                                  // response cache, on set AND on clear
    io::INetworkable* m_networkable;              // +0x4C non-owning
    IObjectComponent* m_postProcessComponent;     // +0x50
    IObjectComponent* m_vehicleHudComponent;      // +0x54
    IObjectComponent* m_helpHudComponent;         // +0x58
    IObjectComponent* m_weaponHudComponent;       // +0x5C
    IObjectComponent* m_abilityHudComponent;      // +0x60
    IObjectComponent* m_abilityGLComponent;       // +0x64
    IObjectComponent* m_radioGLComponent;         // +0x68
    IObjectComponent* m_warningHudComponent;      // +0x6C
    IObjectComponent* m_windAffector;             // +0x70
    IObjectComponent* m_strategicObjectComponent; // +0x74
    bool m_createdOnLoad;                         // +0x78
    bool m_gridRefreshQueued;                     // +0x79
    WeakPtr<IObject>::Ctrl* m_weakControl;         // +0x7C

    friend struct object_detail::LayoutAccess;
    friend class Object;
    friend class SimpleObject;
    // [INFERRED] access spelling; PE 0x74F480 reads the HUD component directly.
    friend class hud::BF2HudManager;
    friend class hud::HudVehicleInformation; // PE78B940/78B4E0: kit ability component+60
    friend class hud::HudAmmo; // PE7A5600/7A59D0/7A8460: HUD component fields+54/+60/+6C
    friend class hud::HudMessages; // PE79E1B0/79E450: cached weapon/vehicle HUD components
    // traceAllObjects (46C580) tests the flags and geometry fields; assignWorkItem reads the id.
    friend class hfe::DistributedLightRenderer;
    // Bundle::handleMessage walks the raw sibling/parent links and reads the armor field directly
    // rather than dispatching getSibling()/getParent(), exactly as both ports do.
    friend class Bundle;
    friend class DestroyableWindow;
    friend class DestroyableWindowsBundle;
    // SupplyObject::update walks the parent chain reading m_armorComponent directly, the same way
    // Bundle::handleMessage does - friendship is not inherited, so it needs its own entry.
    friend class SupplyObject;
    friend class SupplyDepot; // retail init writes createdOnLoad; parent/armor walks are native
    // Frame interpolation reads geometry and owns the interpolation-transform updates.
    friend class ::dice::hfe::BF2FrameInterpolator;
    friend void setChildPartGeometries(IObject*, geom::IGeometry*);
    friend void setChildPartCollisionMeshes(const IObjectTemplate*, IObject*,
                                            const std::string&);
};

// The derived Object class and its field layout are declared in Object.h.

// walk to the root and cache it along the path, even for const input.
// Objects with flag bit 24 set are roots and cache themselves.
IObject* getRootParent(const IObject* object);

// checkIfObjectIntersects tests transformed mesh LOD 1 against the object's LOD 2.
// checkIfObjectIntersectsSoldier walks the response-physics chain, starting at object;
// a node without response physics ends the walk. The worker sweeps a vertical sphere.
// checkIfRayHitAnyObject casts to-from, excludes the object's root, and optionally
// soldiers; plain SimpleObjects marked see-through are also excluded.
class ICollisionMesh;
bool     checkIfObjectIntersects(ICollisionMesh* mesh, const Mat4& transform, IObject* object);
bool     checkIfObjectIntersectsSoldier(IObject* object, IObject* soldier,
                                        bool extendToObjectHeight);
IObject* checkIfRayHitAnyObject(Vec3 from, Vec3 to, IObject* object, bool ignoreSoldiers);
// The terrain-capable sibling used by SupplyDropManager. The first two booleans feed
// ObjectManager::intersectLine's includeTerrain / terrainTwoSided arguments; the third controls
// the LineTestPredicator's soldier filter. The optional Vec3 receives the hit point.
bool     checkIfRayHitAnyObjectOrGround(const Vec3& from, const Vec3& to, const IObject* object,
                                        IObject*& hitObject, bool includeTerrain,
                                        bool terrainTwoSided, bool ignoreSoldiers,
                                        Vec3* hitPoint);

// test terrain and a downward object ray, with no water test.
// outNormal is written even on false; terrain supplies its slope, ray hits use up.
// [INFERRED] return type from EngineBase::handleMessage.
bool     isObjectCloseToGround(IObject* object, Vec3& outNormal, float margin);

// Collect object and descendants depth-first. stop prunes the whole subtree;
// include controls insertion without pruning. Both predicates may be null.
// [CHOSEN] parameter names; ELF 0x831B9A0.
class ObjectPredicator;
void     getChilds(IObject* object, std::vector<IObject*>& out, ObjectPredicator* include,
                   ObjectPredicator* stop);
IObject* findFirstMatchingChild(IObject* object, unsigned int classId);

// ; [INFERRED] function name, [CHOSEN] placement.
// Walk from object itself through parents; return the first IPlayerControlObject
// interface, or null. Every step is guarded; other parent-only helpers differ.
// [UNRESOLVED] whether the private Engine/CarEngine copies represent these same callers.
IPlayerControlObject* findOwningPlayerControlObject(IObject* object);

} // namespace world

// ELF manglings place both helpers in dice::hfe, outside world.
// The bool is unused in both bodies; [CHOSEN] default false preserves one-argument callers.
void enableObject(world::IObject* object, bool b = false);
void disableObject(world::IObject* object, bool b = false);

// When includePlayerControlObjects is false, traversal stops at PCO children.
void enableGhosting(world::IObject* object, bool includePlayerControlObjects);
void disableGhosting(world::IObject* object, bool includePlayerControlObjects);

} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_IOBJECT_H
