// Move statement base with soldier-pose and obstacle-avoidance state.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO_H

#include "../../../../Math/Vec3.h"
#include "BAPAComponentSoldierPose.h"
#include "BAPAction.h"
#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class Information;

// ELF: vtable 0x4B7F40 (40 words), package C-actions.
// Size 0xB4. Move statement base: the move entries call getTargetDir/update and read the public
// fields below inline. The BAPAComponentSoldierPose part sits at +0x14 (vtable 0x4B7FA8).
class BAPAMoveTo : public BAPAction, public BAPAComponentSoldierPose
{
public:
    explicit BAPAMoveTo(Bot*);   // 0x1D5310
    virtual ~BAPAMoveTo();   // 0x1D5550

    virtual unsigned long long getControls();   // 0x1D5650, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D5710, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D5720, slot +0x14; from the base declaration
    virtual bool getIsSprinting();   // 0x1D5700, slot +0x20; [INFERRED] ELF bool
    virtual bool getIsMovingAway();   // 0x1D6880, slot +0x24
    virtual Vec2& getCurrentPoint();   // 0x1D6890, slot +0x28 (+0x6C)
    virtual int update(Vec2&, bool&, bool&, float);   // 0x1D68A0, slot +0x2C; ELF bool (`al`), kept int: F's BAPAMoveToDirect overrides it as int
    virtual bool isFinished(Bot*);   // 0x1D68B0, slot +0x30
    virtual bool getSecondPoint(Vec2&);   // 0x1D68F0, slot +0x34
    virtual bool getLargestAngleDiff(float&);   // 0x1D6900, slot +0x38
    virtual float getTargetDistance();   // 0x1D6910, slot +0x3C
    virtual unsigned int getObject();   // 0x1D6930, slot +0x40 (information handle, -1 = none)
    virtual float getSimpleTargetDistance(Bot*);   // 0x1D5760, slot +0x44
    virtual float getSimpleTargetDistanceSqr(Bot*);   // 0x1D5810, slot +0x48
    virtual int usesPathfinding() = 0;   // no body, slot +0x4C (ELF bool; kept int for F/H overrides)
    virtual int withinDistance() = 0;   // no body, slot +0x50 (ELF bool; kept int for F overrides)
    virtual bool isStuck();   // 0x1D6940, slot +0x54

    void setIsSprinting(Bot*, bool, const Vec3&, unsigned int);   // 0x1D5660
    Vec3& getCoordinate();   // 0x1D5730 (+0x44)
    float getMaxSpeed();   // 0x1D5740
    float getMaxArriveSpeed();   // 0x1D5750
    bool getTargetDir(Bot*, Vec2, Vec2&, Information*, bool);   // 0x1D58B0

    // Read and written inline by the move entries and the subclasses. [INFERRED] names.
    /* +0x40 */ bool m_isSprinting;
    /* +0x44 */ Vec3 m_coordinate;          // the goal
    /* +0x50 */ float m_maxSpeed;
    /* +0x54 */ float m_maxArriveSpeed;
    /* +0x58 */ BAPCondition* m_endCondition;   // owned; isFinished evaluates it
    /* +0x5C */ bool m_pathValid;           // initPathfinding's answer (BAPAMoveToFinding)
    // BoatControl::speedControl (ELF 0x2AA759, 0x2AA802..0x2AA814) reads/writes these samples.
    /* +0x60 */ float m_previousSpeed;      // = 0
    /* +0x64 */ Vec2 m_previousHeading;     // = (1, 0)
    /* +0x6C */ Vec2 m_currentPoint;        // = 0; updatePath's next point
    /* +0x74 */ Vec2 m_vec74;               // = 0; [UNRESOLVED]
    /* +0x7C */ int m_unwritten7C;          // [UNRESOLVED] not written by the constructor
    // Obstacle avoidance state of getTargetDir / updateInfantryMoveToPosWithObstacles.
    /* +0x80 */ unsigned int m_obstacle;    // information handle, -1 = none
    /* +0x84 */ Vec2 m_avoidPoint;
    /* +0x8C */ float m_obstacleRadius;
    /* +0x90 */ float m_avoidDistance;
    /* +0x94 */ Vec2 m_obstaclePosition;
    /* +0x9C */ bool m_avoiding;
    /* +0xA0 */ double m_nextDirTime;       // = -1000.0
    /* +0xA8 */ Vec2 m_cachedDir;
    /* +0xB0 */ bool m_cachedResult;

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO_H
