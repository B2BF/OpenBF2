// Grappling-rope settings and seat animation. Unnamed tuning values retain neutral indices.
// The stored Player pointer is live runtime state. [INFERRED] Source placement.

#ifndef GAME_OBJECTS_GRAPPLINGHOOKROPETEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_GRAPPLINGHOOKROPETEMPLATE_H

#include "Base/SimpleObjectTemplate.h"

#include <string>

namespace dice {
namespace anim { class IAnimationSystemTemplate; }
namespace hfe {
namespace world {

class GrapplingHookRope;
class GrapplingHookRopeContainer;
class IPlayer;
class PlayerControlObject;
struct GameTemplateConsoleAccess;                    // ModuleWorldObjTemplGame.cpp rope console bridge

// The trailing template word is intentionally left uninitialized and copied into the live rope during initialization.
class GrapplingHookRopeTemplate : public SimpleObjectTemplate
{
public:
    GrapplingHookRopeTemplate();

    // Inherits makeScript from SimpleObjectTemplate; do not add an override here.
    virtual void  addRef();                          // `++[this+0x164]`, read directly
    virtual int   getRef();                          // `return [this+0x164]`
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;   // byte-identical to the base's 56 B
    virtual unsigned int getClassID() const;
    virtual ~GrapplingHookRopeTemplate();            // (D1) - see the note at the dtor body
    virtual void* queryImplementation(unsigned int); // flat 3-cid, no base call
    virtual IObject* createObject() const;           // new(0x210) GrapplingHookRope

    // Resolves or renames the seat-animation template through its manager. The destructor releases it.
    // Replacing a nonnull template with an empty name releases it without clearing the stored pointer.
    void setSeatAnimationSystem(const std::string& name);
    const std::string& getSeatAnimationSystem();

    // (528 B), PE 0x578F90 (372 B).  FireComp reaches this only for the
    // GrapplingHookRope projectile class-id arm.
    GrapplingHookRope* createGrapplingHookObject(IPlayer*, Mat4&, float);

protected:
    friend class GrapplingHookRope;
    friend class GrapplingHookRopeContainer;
    friend class PlayerControlObject;
    friend struct GameTemplateConsoleAccess;      // recovered ObjectTemplate.* rope-console executors

    // layout, offsets 356..467. SimpleObjectTemplate occupies 0..355.
    // Every default below is from the full ctor read.
    /* 356 */ int   m_refCount2;                     // ctor 1 - and addRef/getRef both read it
    /* 360 */ anim::IAnimationSystemTemplate* m_seatAnimationSystem; // ctor 0; manager-owned ref
    /* 364 */ std::string m_animationName;           // retyped int -> std::string on the ctor's
                                                     // _S_empty_rep construction AND the dtor's
                                                     // refcount decrement. Ctor: empty.
                                                     // [UNRESOLVED] which name it holds - no
                                                     // accessor reads it, and the seat animation
                                                     // system uses +0x168 instead.

    // Runtime state in a template - see the header block.
    /* 368 */ IPlayer* m_player;                     // createGrapplingHookObject's player; ctor 0

    /* 372 */ float _float174;                       // [UNRESOLVED] name; ctor 14.0f
    /* 376 */ int   _dword178;                       // [UNRESOLVED] name; ctor 28 (0x1C - stored as
                                                     // a plain integer immediate, and 0x1C read
                                                     // as a float would be a denormal, so int)

    // 380..463: the rope simulation constants, in shipped order. The names are the retail
    // ObjectTemplate console properties, recovered from the dedicated PE: the template ctor
    // sub_578E10 writes the block at +0x240..+0x294 and each generated console executor writes its
    // field at PE +0x244..+0x290 and +0x294 (PE offset = openBF2 offset + 0xC4). The leading +0x240
    // field is the `objectTemplate.timeout` property (executor 0x4BA3C0), kept under the consumer's
    // descriptive name.
    /* 380 */ float m_ropeUnusedLifetime;            // PE +0x240, ctor 40.0; console ObjectTemplate.timeout
    /* 384 */ float m_throwStrength;                 // PE +0x244  ObjectTemplate.throwStrength
    /* 388 */ float m_upThrowBoost;                  // PE +0x248  ObjectTemplate.upThrowBoost
    /* 392 */ float m_degradeThrowStrength;          // PE +0x24C  ObjectTemplate.degradeThrowStrength
    /* 396 */ float m_throwStrength2;                // PE +0x250  ObjectTemplate.throwStrength2
    /* 400 */ float m_upThrowBoost2;                 // PE +0x254  ObjectTemplate.upThrowBoost2
    /* 404 */ float m_degradeThrowStrength2;         // PE +0x258  ObjectTemplate.degradeThrowStrength2
    /* 408 */ float m_dropStrength;                  // PE +0x25C  ObjectTemplate.dropStrength
    /* 412 */ float m_upDropBoost;                   // PE +0x260  ObjectTemplate.upDropBoost
    /* 416 */ float m_degradeDropStrength;           // PE +0x264  ObjectTemplate.degradeDropStrength
    /* 420 */ float m_linkAttachMovement;            // PE +0x268  ObjectTemplate.linkAttachMovement
    /* 424 */ float m_linkAttachTime;                // PE +0x26C  ObjectTemplate.linkAttachTime
    /* 428 */ float m_dismountCheckOffset;           // PE +0x270  ObjectTemplate.dismountCheckOffset
    /* 432 */ float m_minYNormal;                    // PE +0x274  ObjectTemplate.minYNormal
    /* 436 */ float m_attachClimberRadius;           // PE +0x278  ObjectTemplate.attachClimberRadius
    /* 440 */ float m_climbingSpeed;                 // PE +0x27C  ObjectTemplate.climbingSpeed
    /* 444 */ float m_elasticity;                    // PE +0x280  ObjectTemplate.elasticity
    /* 448 */ float m_airFriction;                   // PE +0x284  ObjectTemplate.airFriction
    /* 452 */ float m_climbableAngle;                // PE +0x288  ObjectTemplate.climbableAngle
    /* 456 */ float m_dismountAngle;                 // PE +0x28C  ObjectTemplate.dismountAngle
    /* 460 */ float m_climbersAttachForce;           // PE +0x290  ObjectTemplate.climbersAttachForce
    /* 464 */ float m_awakeTime;                     // PE +0x294  ObjectTemplate.awakeTime; ctor leaves it unwritten
                                                     // uninitialized; initialize reads it
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GRAPPLINGHOOKROPETEMPLATE_H
