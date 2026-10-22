// Configures player and AI soldiers, including independent first- and third-person animation rigs.
// [INFERRED] Source placement. Unknown members remain explicitly identified.

#ifndef GAME_OBJECTS_SOLDIERTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_SOLDIERTEMPLATE_H

#include "../Base/BundleTemplate.h"
#include <string>
#include "../../../Math/Vec3.h"     // m_nameTagOffset is a Vec3 - see the layout note below

namespace dice {

// dice::anim is a sibling of dice::hfe, not a child.
namespace anim { class Skeleton; }

namespace hfe {
class Player;
namespace world {

struct GameTemplateConsoleAccess;

class SoldierTemplate : public BundleTemplate /* , public ISoldierTemplate @ +380 */
{
public:
    // Soldier reads its template configuration directly.
    friend class Soldier;
    friend class ::dice::hfe::Player;
    friend struct GameTemplateConsoleAccess;     // recovered direct-field console executors

protected:
    // Original ELF field offsets are references; native STL layouts may differ.
    /* 380 */ void* pVMT_ISoldierTemplate;           // second base, RTTI-proven @ +380
    /* 384 */ int   m_refCount3;

    /* 388 */ float m_ridingWithDistMod;
    /* 392 */ Vec3  m_ridingWithOffset;              // 392..403

    // Three-component nametag offset.
    /* 404 */ Vec3  m_nameTagOffset;                 // 404..415

    /* 416 */ int   m_itemContainerType;

public:
    // ConsoleClass272's `ObjectTemplate.inventorySize` SET arm writes this field directly
    // (ELF SoldierTemplate +0x1A4, PE +0x254); there is no setter symbol. Stock soldiers author
    // the value 10 over the constructor default 6, making item slots 1..9 valid. Public solely so
    // the console binding's narrow SET helper can preserve that raw generated-executor store
    // without inventing a public setter.
    /* 420 */ int   m_inventorySize;

protected:

    // One camera position per pose; requesting an index above 3 aborts.
    /* 424 */ Vec3  m_poseCameraPosition[4];         // 424..471

    /* 472 */ float m_manDownCameraOffsetInDof;
    /* 476 */ float m_manDownCameraOffsetInUp;

    // Sprint constants. setConstants receives dissipation time before recovery time.
    /* 480 */ float m_sprintRecoverTime;
    /* 484 */ float m_sprintDissipationTime;
    /* 488 */ float m_sprintLimit;
    /* 492 */ float m_sprintLossAtJump;
    /* 496 */ float m_gasMaskSprintFactor;
    // [UNRESOLVED] 500..535 - still unaccounted for. The five above are read; the remaining 36 bytes
    // have no console binding, so this route cannot see them.
    // NOT padding - the constructor writes +0x1F4 (500). A ctor-initialised slot is a real member;
    // this block is simply unreachable by the console route because nothing here is content-settable.
    // [UNRESOLVED] how many members it holds and their names.
    /* 500 */ char  _pad1F4[36];
    // [INFERRED] the five are typed float because every one is a time/limit/factor and the
    // neighbouring recovered fields are floats. The extraction proves the offsets, not the types -
    // only `geometry1P`/`geometry3P` below had their type read directly (from std::string::assign).

    // First- and third-person rigs each hold a skeleton, animation system and geometry name.
    /* 536 */ anim::Skeleton* m_skeleton1P;
    /* 540 */ void*       m_animationSystem1P;
    /* 544 */ std::string m_geometry1P;              // was `int _dword220`
    /* 548 */ anim::Skeleton* m_skeleton3P;
    /* 552 */ void*       m_animationSystem3P;
    /* 556 */ std::string m_geometry3P;              // was `int _dword22C`
    /* 560 */ int   _dword230;                       // [UNRESOLVED]
    // Inverse root-bone bind transform cached when loading the first-person skeleton.
    /* 564 */ Mat4  m_bone0InverseTransform;         // 564..627, ctor identity

    // by initAnimation: the "fallingWeapon" object template.
    /* 628 */ IObjectTemplate* m_weapObjTemplate;
    /* 632 */ float m_ragdollLandedDelay;            // was `_float278` - named from the console
    /* 636 */ std::string m_soundFilter;

public:
    // / dedicated PE 0x592670: release both animation rigs before
    // automatic member/base destruction. The compiler-generated destructor omitted
    // this call and retained the shared 1P/3P animation-system templates across reload.
    virtual ~SoldierTemplate() override;

    // The leaf owns its reference count: ELF +0x180 / PE +0x230, seeded to one below.
    // Override the existing IBase slots; native code uses the typed member, not retail offsets.
    virtual void addRef() override;
    virtual int getRef() override;
    virtual int release() override;

    // Initializes soldier inventory, movement, camera and animation defaults.
    SoldierTemplate()
        : m_refCount3(1)
        , m_ridingWithDistMod(1.0f)
        , m_itemContainerType(-1)
        , m_inventorySize(6)
        , m_manDownCameraOffsetInDof(0.06f)
        , m_manDownCameraOffsetInUp(0.05f)
        , m_sprintRecoverTime(30.0f)
        , m_sprintDissipationTime(20.0f)
        , m_sprintLimit(0.2f)
        , m_sprintLossAtJump(0.0f)
        , m_gasMaskSprintFactor(1.5f)
        , m_skeleton1P(0)
        , m_animationSystem1P(0)
        , m_skeleton3P(0)
        , m_animationSystem3P(0)
        , m_weapObjTemplate(0)
        , m_ragdollLandedDelay(4.0f)
    {
        m_ridingWithOffset.x = 0.0f; m_ridingWithOffset.y = 0.0f; m_ridingWithOffset.z = 0.0f;
        // THE name TAG floats 0.7 above THE soldier'S origin by default.
        m_nameTagOffset.x = 0.0f; m_nameTagOffset.y = 0.7f; m_nameTagOffset.z = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            m_poseCameraPosition[i].x = 0.0f;
            m_poseCameraPosition[i].y = 0.0f;
            m_poseCameraPosition[i].z = 0.0f;
        }
        // Only the fourth pose starts with a vertical camera offset.
        m_poseCameraPosition[3].y = 0.3f;

        // the bind-pose inverse starts as the identity - sixteen dwords zeroed
        // and the four diagonal elements set. skeleton1P() overwrites it once a rig is loaded.
        for (int i = 0; i < 16; ++i)
            m_bone0InverseTransform.m[i] = 0.0f;
        m_bone0InverseTransform.m[0]  = 1.0f;
        m_bone0InverseTransform.m[5]  = 1.0f;
        m_bone0InverseTransform.m[10] = 1.0f;
        m_bone0InverseTransform.m[15] = 1.0f;

        initAnimation();
    }

    // the accessors Soldier forwards to. Each is [CHOSEN] in name only - the offsets and the
    // forwarding are attested by the Soldier bodies that read them.
    const Vec3& getNameTagOffset() const      { return m_nameTagOffset; }
    const Vec3& getRidingWithOffset() const   { return m_ridingWithOffset; }
    float getRidingWithDistMod() const        { return m_ridingWithDistMod; }
    int   getItemContainerType() const        { return m_itemContainerType; }
    int   getInventorySize() const            { return m_inventorySize; }   // `mov eax,[eax+1A4h]`
    const std::string& getSoundFilter() const { return m_soundFilter; }
    void setSoundFilter(const std::string& f) { m_soundFilter = f; }

    // The setter does not validate pose; callers must supply an index in 0..3.
    // stores the Vec3 and then forces x and z back to zero, so only the authored height
    // survives. Content such as `poseCameraPosition 0 0/0.47/0.1` therefore yields (0, 0.47, 0).
    void poseCameraPosition(int pose, const Vec3& p)
    {
        m_poseCameraPosition[pose].x = 0.0f;
        m_poseCameraPosition[pose].y = p.y;
        m_poseCameraPosition[pose].z = 0.0f;
    }
    const Vec3& getPoseCameraPosition(int pose) const { return m_poseCameraPosition[pose]; }

    // the two animation rigs. All four content setters and their console registrations are
    // live; Soldier::initAnimation clones these template rigs into each Soldier. The third-person
    // skeleton also receives the authored hit capsules through setSkeletonCollisionBone below.
    anim::Skeleton* getSkeleton1P() const { return m_skeleton1P; }
    anim::Skeleton* getSkeleton3P() const { return m_skeleton3P; }
    float getSprintRecoverTime() const     { return m_sprintRecoverTime; }      // +0x1E0
    float getSprintDissipationTime() const { return m_sprintDissipationTime; }  // +0x1E4
    float getSprintLimit() const           { return m_sprintLimit; }            // +0x1E8

    void* getAnimationSystem1P() const   { return m_animationSystem1P; }
    void* getAnimationSystem3P() const   { return m_animationSystem3P; }

    // Loads a skeleton by name; the first-person loader also caches the root bind inverse.
    // A failed parse stores the returned blank skeleton rather than null.
    void skeleton1P(const std::string& name);
    void skeleton3P(const std::string& name);

    // Stock soldier tweaks use this to author the animated hit capsules on skeleton3P. The body
    // deliberately does nothing when that skeleton has not been loaded yet.
    void setSkeletonCollisionBone(const std::string& name, int material, const Vec3& offset,
                                  float radius, float length);

    // Both are a single call into the animation-system manager singleton at
    // ds:0x8AB0F88 - the same one AnimatedBundleTemplate uses - and a store.
    void animationSystem1P(const std::string& name);
    void animationSystem3P(const std::string& name);

    // note THE names have NO `set` prefix and they are setters - the console
    // property system binds them by these names.
    void initAnimation();
    void releaseAnimation();

    // Exposes the soldier-template implementation to console property dispatch.
    virtual void* queryImplementation(unsigned int classId);

    // Returns the soldier template class id used by gameplay and serialization.
    virtual unsigned int getClassID() const;                           // +0x10

    virtual IObject* createObject() const;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_SOLDIERTEMPLATE_H
