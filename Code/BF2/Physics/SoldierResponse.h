// Infantry collision, contact response and floating state.
// [INFERRED] header beside the __FILE__-attested Physics/SoldierResponse.cpp.
// Terrain contact uses five foot points; floating uses separate enter/leave thresholds.
// m_frictionFlags is a bit field, not a Boolean; preserve its retained bit 6.

#ifndef PHYSICS_SOLDIERRESPONSE_H     // [INFERRED] guard style
#define PHYSICS_SOLDIERRESPONSE_H

#include "PointResponsePhysics.h"

namespace dice {
namespace hfe {

class SoldierNetworkable;      // the wire wrapper - a friend below; see Networkables.h
class SoldierSound;            // reads the retail ground-contact byte in its movement-sound tick

namespace world {

class SoldierResponsePhysics : public PointResponsePhysics
{
public:
    SoldierResponsePhysics();
    virtual ~SoldierResponsePhysics();

    virtual void  addRef();                          // the +0x90 counter
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;

    virtual void  reset();                           // 122 B - see the header
    virtual unsigned int getClassID() const;         // CID_SoldierResponsePhysics
    virtual void  checkObjectVsObject(IObject*, IObject*, float, bool);
    virtual void  checkVsTerrain(float dt);          // -> internal_checkVsTerrain(dt, 1.0f)
    virtual void  impulseOn(const Vec3&, const Vec3&, const Vec3&, float, int, int);   // the redirect
    virtual void  solveImpulse(IPhysicsNode*);
    virtual void  addFriction(IPhysicsNode*);
    virtual void  setCollisionChecked(unsigned int); // +0xE8
    virtual unsigned int getCollisionChecked() const;
    virtual bool  getIsIntersectingTerrain(Vec3&) const;   // +0xEC / +0xF8
    virtual unsigned char getPermanentGrip() const;  // +0xE1
    virtual void  setPermanentGrip(unsigned char);
    virtual void  setSurfacePositionalSpeed(const Vec3&);  // +0xAC
    virtual const Vec3& getSurfacePositionalSpeed() const;
    virtual const Vec3& getPositionalAdjusts() const;      // +0x94
    virtual bool  poseChangeTest(IObject*, IObject*, float);
    virtual bool  testIfValidTransformation(const Mat4&, bool);

    // Non-virtual.
    void checkIfCrushedByVehicle();
    // Returns the pose's collision height and writes sample count and step size.
    float getSoldierHeight(int* countOut, float* stepOut);
    // static. Itanium does not mark static-ness in the mangling, so this read as a member with a
    // `this` for a long time. Its prologue immediately does [this+0x44] - IObject::m_physicsNode -
    // and solveImpulse's call site passes getObject()'s result in the first slot with only four
    // slots written. Both facts only fit a static member.
    static void checkSoldierVsSoldier(IObject*, IObject*, float, float);
    void hitByMovingObject(const Vec3&);
    // returns float (the deepest penetration) - solveImpulse folds it into a running minimum.
    float checkSoldierVsMesh(IObject*, IObject*, float, float, bool);
    // static too. The ELF prologue reads the first stack argument's IObject +0x44/+0x3C fields,
    // while both callers pass exactly (soldier, projectile, dt); there is no hidden response this.
    static void checkSoldierVsPoint(IObject*, IObject*, float);
    // Returns the deepest negative penetration among five foot points, or zero when none penetrate.
    float internal_checkVsTerrain(float dt, float scale);
    // (speed, normal, depth, ownMaterial, otherMaterial, isFirstPoint) - the last is unread.
    void internalImpulseOn(const Vec3&, const Vec3&, float, int, int, bool);
    bool expandSoldierCollision(const Vec3&, const Vec3&, float, IObject*, float);   // 215 B - the dispatch
    bool soldierPoseChangeCheckVsSoldier(const Vec3&, const Vec3&, float, IObject*, float);   // true
    bool soldierPoseChangeCheckVsMesh(const Vec3&, const Vec3&, float, IObject*, float);

protected:
    // Soldier networking directly snapshots and applies the response state byte.
    friend class dice::hfe::SoldierNetworkable;
    friend class dice::hfe::SoldierSound;
    // Soldier input and collision animation directly read the ground/water bytes and feet
    // normal (retail +0xFA/+0xFB/+0xC8). Named access preserves those reads on native x64.
    friend class Soldier;
    // GenericFireArm::checkAreaToFireZipline samples +0xFA then +0xF9 directly in both ports
    // (ELF 0x8254940 / PE 0x5847C0). Narrow friend preserves that raw response-physics gate.
    friend class GenericFireArm;
    // SoldierDeviationComp::updateDeviation reads the +0xFA ground-contact byte directly for an
    // unparented soldier; the friend keeps that read on the named member.
    friend class SoldierDeviationComp;

    // Layout 0x90..0xFB, every default from the 241-byte ctor.
    /* +0x90 */ int     m_refCount;                  // 1
    // The tinfo's names for +0x94 / +0xA0 / +0xB8 are kept so the IDB and this header still read
    // the same, but internalImpulseOn says what they hold - and they are ResponsePhysics's
    // m_positionalAdjusts / m_speedAdjusts / m_averageSpeed under other names:
    /* +0x94 */ Vec3    m_positionalAdjusts1;        // 0 - setAdjustVector(normal * -depth)
    /* +0xA0 */ Vec3    m_positionalAdjusts2;        // 0 - setAdjustVector(normal part of -speed)
    /* +0xAC */ Vec3    m_surfacePositionalSpeed;    // 0
    /* +0xB8 */ Vec3    m_positionalAdjusts3;        // 0 - addAdjustVector mean of the contact speeds
    /* +0xC4 */ Vec3    m_feetContactNormal;         // 0; reset -> (0, cvar, 0) - [CHOSEN] name
    /* +0xD0 */ int     m_contactCount;              // 0, zeroed by reset - addAdjustVector's count
    /* +0xD4 */ float   m_averageFriction;           // 1.0f - mean of the two materials; 0 for 7001
    /* +0xD8 */ float   m_averageElasticity;         // 0 a float, not the int this said until
    /* +0xDC */ float   m_averageResistance;         // 0
    // not a bool: internalImpulseOn preserves bit 6 (`grip | (flags & 0x40)`).
    /* +0xE0 */ unsigned char m_frictionFlags;       // 0 (the tinfo's name; ResponsePhysics: m_grip)
    /* +0xE1 */ unsigned char m_permanentGrip;       // 1
    /* +0xE4 */ int     m_dwordE4;                   // 0 [UNRESOLVED]
    /* +0xE8 */ unsigned int m_collisionChecked2;    // 0 - this class's own counter
    /* +0xEC */ Vec3    m_intersectingTerrainPoint;  // 0 - the frame's mean terrain contact point
    /* +0xF8 */ bool    m_isIntersectingTerrain;     // false
    /* +0xF9 */ bool    m_byteF9;                    // 0 - reset: = m_byteFA (last frame's)
    /* +0xFA */ bool    m_byteFA;                    // 0 - reset: = 0; set when point 0 improves
                                                     //   m_feetContactNormal ("has ground under foot")
    /* +0xFB */ bool    m_isFloating;                // 0 - the start-float / stop-float trigger
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // PHYSICS_SOLDIERRESPONSE_H
