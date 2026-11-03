// Ballistic aiming for one weapon at one target: given the shooter-relative target state and the
// projectile's speed, drag and gravity, finds the firing direction by iterating the elevation.
// Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIBot/Aimer.cpp (FILE symbol beside
// 0x1C8EA0, after CommunicationAgent.cpp); constructors 0x1C7120 / 0x1C71E0, destructors
// 0x1C72A0 / 0x1C72B0 (empty, not virtual). sizeof 0x6C: BAPAAimAt embeds it at +0xA4 and its next
// member is at +0x110 (BAPAAimAt::BAPAAimAt 0x1CD5E0).
//
// The fields are public: the aim actions write the projectile parameters in place before
// setTargetData (BAPAAimAtObject::getAimVec 0x1CEA00: +0 = drag, +4 = 1 / drag when the drag is
// not 0, +0xC = speed, +8 = physics gravity * the weapon's gravity modifier). Names are
// [INFERRED] from those uses unless noted.

#ifndef AI_AIDLL_AI_AIBOT_AIMER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_AIMER_H

#include "../../../../Math/Vec3.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

class Aimer
{
public:
    Aimer();                                                     // ELF 0x1C7120
    ~Aimer();                                                    // ELF 0x1C72A0 (empty)

    // ELF 0x1C72C0. relativePos = target - shooter, targetVelocity, shooterVelocity, the shooter's
    // transform (may be 0) and the time the data was sampled. Always returns true.
    bool setTargetData(const Vec3& relativePos, const Vec3& targetVelocity, const Vec3& shooterVelocity,
                       const Mat4* transform, double time);
    // ELF 0x1C74C0: time a projectile fired at `speed` needs to cover `distance` with drag.
    float getImpactTime(float distance, float speed);
    // ELF 0x1C75D0: integrates the projectile (drag, gravity) for `time`; `out` is the displacement.
    void getPredictedImpactPosition(Vec3 velocity, float time, Vec3& out) const;
    // ELF 0x1C7970: the analytic (drag-free) elevation for a static target.
    bool getStaticFiringDirection(Vec3& direction, Vec3& target) const;
    // ELF 0x1C7B80: apex of a throw in `direction`; false when the throw does not rise.
    bool getTurnPointForGrenade(const Vec3& direction, Vec3& point, float& time) const;
    // ELF 0x1C7C40: iterates the elevation until the trajectory meets the predicted target.
    bool getFiringDirection(double time, float accuracy, bool highArc, Vec3& direction, Vec3& target,
                            double* impactTime);
    // ELF 0x1C8770: the target relative position at `time`.
    void getPredictedTargetPosition(double time, Vec3& out) const;
    // ELF 0x1C8850: where a shot in `direction` is after `time` (false beyond 100 s).
    bool getImpactPosition(Vec3& out, const Vec3& direction, float time) const;
    // ELF 0x1C8900: closest approach of a shot in `direction` to the moving target.
    bool getNearestImpactPoint(const Vec3& direction, Vec3& out, double& time) const;
    // ELF 0x1C8E90: +0x58.
    double getVerticalExtreme() const;

    /* +0x00 */ float  m_drag;                  // ctor 0
    /* +0x04 */ float  m_inverseDrag;           // 1 / drag, written by the aim actions only
    /* +0x08 */ float  m_gravity;               // ctor 0; y acceleration
    /* +0x0C */ float  m_speed;                 // ctor 1.0f; projectile speed (0 = no projectile)
    /* +0x10 */ double m_time;                  // setTargetData's time
    /* +0x18 */ Vec3   m_relativePos;           // ctor (1, 0, 0)
    /* +0x24 */ Vec3   m_relativeDir;           // normalised m_relativePos; ctor (1, 0, 0)
    /* +0x30 */ Vec3   m_targetVelocity;
    /* +0x3C */ Vec3   m_shooterVelocity;
    /* +0x48 */ Vec3   m_forward;               // transform * (0, 0, 1); ctor (0, 0, 1)
    /* +0x54 */ int    m_hasTargetData;         // ctor 0, setTargetData 1
    /* +0x58 */ double m_verticalExtreme;       // half the last impact time (getFiringDirection)
    /* +0x60 */ float  m_angle;                 // last elevation; ctor 666.0f = none yet
    /* +0x64 */ float  m_angleStep;             // ctor 0.0174533f (one degree)
    /* +0x68 */ bool   m_grenade;               // ctor false; fixed fuse time in the prediction
    /* +0x69 */ bool   m_flag69;                // [UNRESOLVED] name; ctor false; with m_grenade
                                                // unset, the grenade aim point is pulled in
    /* +0x6A */ char   m_pad6A[2];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_AIMER_H
