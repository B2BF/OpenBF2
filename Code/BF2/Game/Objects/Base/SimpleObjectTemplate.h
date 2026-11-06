// Geometry, material, and physics configuration above ObjectTemplate.
// ISimpleObjectTemplate is a secondary base; documented binary offsets vary by platform.
// checkPhysicsTypes uses the first set bit in 3..6 and retains the old type when none is set.

#ifndef GAME_OBJECTS_SIMPLEOBJECTTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_SIMPLEOBJECTTEMPLATE_H

#include <string>
#include <utility>
#include <vector>

#include "../ObjectTemplate.h"
#include "../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai { class AIObjectUnit; class WeaponObject; class BFCollisionHandler; class BFObjectEnvironment; }   // field readers, friended below
namespace world {

class ArtilleryHandler;
class PreDestinedFollowComp;
struct BaseTemplateConsoleAccess;

void turnOffChildFloaters(IObject* object);

// The 14-slot secondary interface is complete in both ports: the IBase quartet followed by these
// ten operations. No virtual destructor is present; lifetime is controlled through release().
class ISimpleObjectTemplate : public IBase
{
public:
    virtual void setGeometry(const std::string&) = 0;
    virtual const std::string& getGeometry() = 0;
    virtual void mapMaterial(unsigned int, const std::string&, unsigned int) = 0;
    virtual std::vector<std::pair<std::string, unsigned int> >* getMaterialVector() = 0;
    virtual void setCollisionMesh(const std::string&) = 0;
    virtual const std::string& getCollisionMesh() const = 0;
    virtual void setRelativePositionOffset(const Vec3&) = 0;
    virtual Vec3 getRelativePositionOffset() = 0;
    virtual void setHasMobilePhysics(bool) = 0;
    virtual bool getHasMobilePhysics() const = 0;

protected:
    ~ISimpleObjectTemplate() {}
};

// Shared authored geometry and physics properties. Component binders require collision
// physics and keep separate node/response ownership sequences.
class SimpleObjectTemplate : public ObjectTemplate, public ISimpleObjectTemplate
{
public:
    SimpleObjectTemplate();
    // Preserve the primary virtual order and separate ISimpleObjectTemplate interface.
    // createObject is the factory inherited from ObjectTemplate.

    // overrides of ObjectTemplate / IBase slots (positions fixed by the base)
    // ObjectTemplate does NOT override the IBase quartet; this class does.
    virtual void  addRef();                                              // +0x00
    virtual int   getRef();                                              // +0x04
    virtual int   release();                                             // +0x08
    virtual void* queryInterface(unsigned int iid) const;                // +0x0C
    virtual unsigned int getClassID() const;                             // +0x10
    virtual ~SimpleObjectTemplate();                                     // +0x14 +0x18

    virtual bool  writeBinary(io::IStream*) const;                       // +0x20
    virtual bool  readBinary(io::IStream*);                              // +0x24
    virtual void* queryImplementation(unsigned int classId);             // +0x50
    virtual IObject* createObject() const;                               // +0x54
    // Serializes template configuration using symbolic physics-type names.
    virtual bool  makeScript(io::IStream*);                              // +0xD4

    // Only the SET halves are overridden. removePhysicsNodeComponent (+0x84) and
    // removeResponsePhysicsComponent (+0x8C) stay with ObjectTemplate — an asymmetry visible in the
    // vtable and easy to lose when writing these by hand.
    virtual void setPhysicsComponent(IObject*) const;                    // +0x78
    virtual void removePhysicsComponent(IObject*) const;                 // +0x7C
    // Attach physics only when collision-physics flag 4 is set.
    virtual void setPhysicsNodeComponent(IObject*) const;                // +0x80
    virtual void setResponsePhysicsComponent(IObject*) const;            // +0x88

private:
    // [CHOSEN, not recovered] Private helpers partition the original physics-attachment methods without changing virtual dispatch.
    void buildPhysicsNode(IObject*) const;
    void buildResponsePhysics(IObject*) const;
    void buildMobilePhysicsNode(IObject*) const;

public:
    // Dedicated builds return null; client builds construct the named sound.
    // [CHOSEN] untyped return representation.
    void* createSoundFromTemplate(const std::string& name, IObject* owner) const;
private:
    void buildMobileResponsePhysics(IObject*) const;

public:
    virtual void setCollisionMesh(const std::string&);                   // +0x98
    virtual const std::string& getCollisionMesh() const;                 // +0x9C
    virtual const Vec3& getAnchor() const;                               // +0xA0
    virtual void  setAnchor(const Vec3&);                                // +0xA4
    virtual const Vec3& getAnchorOffset() const;                         // +0xA8
    virtual void  setAnchorOffset(const Vec3&);                          // +0xAC
    virtual Vec3  getAnchorPos() const;                                  // +0xB0
    virtual std::vector<std::pair<std::string, unsigned int> >* getMaterialVector(); // +0xBC
    virtual void  setMaterial(int);                                      // +0xC4
    virtual int   getMaterial() const;                                   // +0xC8


    // Declaration order matters under the Itanium ABI: these two are the first appended primary
    // slots (+0xE8/+0xEC). MSVC emits their implementations only in the secondary interface table.
    virtual void setHasMobilePhysics(bool);                             // +0xE8
    virtual bool getHasMobilePhysics() const;                           // +0xEC

    virtual void  setGeometry(const std::string&);                       // +0xF0
    // [INFERRED] return; 13 B is the trivial-getter shape, and the caller-visible type is a string.
    // NON-const, unlike getCollisionMesh above — read from the mangled name, not assumed.
    virtual const std::string& getGeometry();                            // +0xF4

    virtual void  setGeometryPart(int);                                  // +0xF8
    virtual void  setCollisionPart(int);                                 // +0xFC

    // Exact container semantics recovered: indexed (name,id) pairs; in-range remaps change only id.
    virtual void  mapMaterial(unsigned int, const std::string&, unsigned int);  // +0x100

    virtual void  setRelativePositionOffset(const Vec3&);                // +0x104
    // NON-const and BY value in both ports; both bodies copy into a hidden sret buffer.
    virtual Vec3 getRelativePositionOffset();                            // +0x108

    // Base no-op; destroyable-window templates override it.
    virtual void  setDestroyableWindowsTemplate(const std::string&);     // +0x10C

    // Creates and returns a sound template marked with flag 0x400.
    virtual ObjectTemplate* addSoundTemplate(const std::string&);        // +0x110

    // "S_" + getName() + "_" + suffix - the bundle-sound naming rule. 0x81F2770, 117 B, const.
    std::string makeSoundTemplateName(const std::string& suffix) const;

    // Shared interface for geometry, materials, collision, position offset and mobile-physics settings.

    // Mobile-physics accessors update the shared flag byte through the virtual interface.
    void createComponent(const std::string& name);
    // Instantiates a temporary component to find its interface ID, removes that slot, then releases it.
    // Unknown names are ignored.
    void deleteComponent(const std::string& name);

    void checkPhysicsTypes();            // resolves bits 3..6 into m_physicsType
    void CheckForSpecialMaterials();

    // Reads or sets tree collision diameter. The setter clamps negative values to zero.
    float getTreeCollisionDiameter() const { return m_treeCollisionDiameter; }
    void  setTreeCollisionDiameter(float value)
    {
        m_treeCollisionDiameter = (value >= 0.0f) ? value : 0.0f;
    }

protected:
    friend struct BaseTemplateConsoleAccess;      // recovered direct-field console executors
    friend class SimpleObject;
    friend class ai::BFObjectEnvironment;        // the spawner query reads m_aiTemplate directly
    friend class ai::AIObjectUnit;               // m_aiTemplate (+296) of a soldier's KIT - the
                                                // AIObjectUnit reads the kit template directly.
    friend class ai::BFCollisionHandler;         // m_aiTemplate (+296) - handleProjectileDamage's
                                                // AI weapon template lookup
    friend class ai::WeaponObject;               // a projectile's m_drag (+64), m_mass (+92) and
                                                // WeaponObject reads gravity configuration directly.
    friend class GenericFireArm;                 // m_aiTemplate (+296) - GenericFireArm::init
                                                // looks its AI weapon template up by that name
    friend class ArtilleryHandler;               // projectile gravity modifier in ballistic solve
    friend class PreDestinedFollowComp;           // the shipped debug ballistic trace reads it
    friend class PhysicsNode;                   // m_floaterMod (+0x88), in updateRotationalPhysics'
                                                // under-water damping branch
    friend void turnOffChildFloaters(IObject* object);
    friend float getMassMod(IObject* other, IObject* self);   // m_mass (+92) - GameLogicCollision.cpp
    friend void setChildPartGeometries(IObject*, geom::IGeometry*);
    friend void setChildPartCollisionMeshes(const IObjectTemplate*, IObject*,
                                            const std::string&);
    // layout, offsets 48..355. ObjectTemplate occupies 0..47.
    // +48 is the real ISimpleObjectTemplate base subobject (its vptr is compiler-generated).
    /*  52 */ int          m_refCount;

    // geometry
    // defaults from SimpleObjectTemplate::SimpleObjectTemplate (0x81F0300, 599 bytes), which writes
    // these literally. -1 is a sentinel meaning "no part selected", NOT an index - a rebuild that
    // zero-initialises would silently select part 0 for every object that never sets geometryPart.
public:
    // Console getters access these fields directly; setters use the virtual methods.
    /*  56 */ int          m_geometryPart;             // = -1  (sentinel)
    /*  60 */ int          m_collisionPart;            // = 0
protected:

    // physics: drag
    /*  64 */ float        m_drag;
    /*  68 */ Math::Vec3   m_dragOffset;                 // 68,72,76
    /*  80 */ Math::Vec3   m_dragModifier;               // 80,84,88

    /*  92 */ float        m_mass;
    /*  96 */ Math::Vec3   m_centerOfMassOffset;         // 96 + the 8-byte gap at 100
    /* 108 */ Math::Vec3   m_centerOfCollisionOffset;    // 108,112,116
    /* 120 */ Math::Vec3   m_inertiaModifier;            // 120,124,128
    /* 132 */ float        m_gravityModifier;
    /* 136 */ float        m_floaterMod;

    /* 140 */ Math::Vec3   m_unknown8C;                  // [UNRESOLVED] name; ctor writes (0,0,0)
    /* 152 */ Math::Vec3   m_unknown98;                  // [UNRESOLVED] name; ctor writes (1,1,1)

    // collision
    /* 164 */ float        m_treeCollisionDiameter;
    /* 168 */ int          m_otherCollisionLod;
    // Physics flags: sleeping 0x01, mobile 0x02, collision 0x04, point 0x08, rotational point 0x10, response 0x20.
    // Type setters recompute physics type; sleeping and collision setters leave it unchanged.
public:
    // Console bindings read/write physics flags and type directly.
    /* 172 */ unsigned char m_physicsFlags;
protected:
    /* 173 */ char         _padAD[3];
public:
    /* 176 */ int          m_physicsType;                // console type: PhysicsType
protected:
    /* 180 */ char         _padB4[4];                    // [UNRESOLVED]
    /* 184 */ unsigned int m_grip;                       // console type: PhysicsGripFlags
    /* 188 */ int          m_physicsFrequency;           // was `_dwordBC` [UNRESOLVED]
    /* 192 */ unsigned int m_collisionGroups;            // console type: U32

    // materials
    /* 196 */ int          m_material;
    /* 200 */ unsigned char m_isPortalPassing;           // was `_byteC8` [UNRESOLVED]
    /* 201 */ char         _padC9[3];
    /* 204 */ Math::Vec3   m_portalPassingPosition;      // passed by const reference to PortalManager

    // getMaterialVector (ObjectTemplate vtable +0xBC) returns this exact vector object. The pair
    // element type is named by mapMaterial's instantiated std::pair/std::vector helper symbols.
    /* 216 */ std::vector<std::pair<std::string, unsigned int> > m_materialVector;

    // animated UVs
    /* 228 */ bool         m_animatedUVTranslation;
    /* 229 */ char         _padE5[3];
    /* 232 */ Math::Vec2   m_animatedUVTranslationSpeed; // 232,236
    /* 240 */ Math::Vec2   m_animatedUVTranslationMax;   // 240,244
    /* 248 */ Math::Vec2   m_animatedUVTranslationSize;  // 248,252
    /* 256 */ int          m_animatedUVTranslationIndex;
    /* 260 */ bool         m_animatedUVTranslationReverse;
    /* 261 */ bool         m_animatedUVRotation;
    /* 262 */ char         _pad106[2];
    /* 264 */ Math::Vec2   m_animatedUVRotationScale;    // 264,268
    /* 272 */ int          m_animatedUVRotationIndex;
    /* 276 */ float        m_animatedUVRotationRadius;
    /* 280 */ bool         m_animatedUVRotationReverse;
    /* 281 */ bool         m_rotateAsAnimatedUV;
    /* 282 */ char         _pad11A[2];
    /* 284 */ std::string  m_rotateAsAnimatedUVObject;   // console type: std::string
    /* 288 */ bool         m_rotateAsAnimatedUVReverse;
    /* 289 */ char         _pad121[3];

    // resources
    /* 292 */ std::string  m_geometry;
    /* 296 */ std::string  m_aiTemplate;
    /* 300 */ std::string  m_collisionMesh;
    /* 304 */ std::string  m_unknown130;                 // [UNRESOLVED] name; ctor/dtor prove string

    // placement
    /* 308 */ Math::Vec3   m_relativePositionOffset;     // 308,312,316
    /* 320 */ float        m_handleCollisionSpeed;
    /* 324 */ Math::Vec3   m_anchor;                     // 324 + the 8-byte gap at 328
    /* 336 */ Math::Vec3   m_anchorOffset;               // 336 + the 12-byte gap at 340 (see note)
    /* 348 */ char         _pad15C[4];                   // [UNRESOLVED] remainder of that gap

    /* 352 */ bool         _byte160;                     // [UNRESOLVED] name; role resolved: the
                                                         // "special materials checked" latch. ctor
                                                         // = false; createObject (74 B) runs
                                                         // CheckForSpecialMaterials (146 B) while
                                                         // false; that sets it true (its last
                                                         // store). Nothing else touches it.
    /* 353 */ char         _pad161[3];                   // no ctor writes; padding in both ports
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_SIMPLEOBJECTTEMPLATE_H
