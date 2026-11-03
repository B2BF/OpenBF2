// [CHOSEN, not recovered] package C (ai-w1-c-actions): the inline helpers the BAP actions and
// conditions spell out in place. libaidll.so has no symbol for any of them; each one is the exact
// expansion seen in the bodies that cite it (ELF addresses there). Header-only, no build-list entry.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACTIONINLINE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACTIONINLINE_H

#include "../../../../Math/Vec3.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../AIKnowledge/InformationManager.h"
#include "../Main/AIDllFrontier.h"
#include "BAPCondition.h"
#include "BAPTypes.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../AIUtils/IgnoreListPredicate.h"
#include "../Main/AIStreamOperators.h"

#include <cmath>
#include <iosfwd>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class BAPCondition;
class IAIObject;

namespace bapinline {

// informationManager->m_pool->get(handle): `h & 0xFFFF` one-based slot, `h >> 16` generation
// (e.g. BAPALookAtObject::getLookDirection 0x1D3320, BAPAMergeSoldierInputs::execute 0x1D3F00).
inline Information* info(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

// `(mappingFlags >> i) & 1 ? inputs[i] : 0.0f`: an unmapped input reads as zero
// (BAPAMergeSoldierInputs::execute 0x1D3FF0: bit 13 then +0x34; bit 3 then +0x0C).
inline float mappedInput(const io::PlayerInput* in, io::PlayerInputMap i)
{
    return ((in->mappingFlags >> static_cast<unsigned int>(i)) & 1ULL) ? in->inputs[i] : 0.0f;
}

inline Vec3 vec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

inline Vec3 sub(const Vec3& a, const Vec3& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 add(const Vec3& a, const Vec3& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float lengthSqr(const Vec3& a) { return dot(a, a); }

// The engine's inline Vec3 normalise: left alone within FLT_EPSILON of unit length, zeroed when
// the squared length is below FLT_EPSILON, else scaled by 1/sqrt (0x1D3495..0x1D351A).
inline void normalize(Vec3& v)
{
    const float lsq = lengthSqr(v);
    if (std::fabs(1.0f - lsq) < 1.1920929e-07f)
        return;
    if (std::fabs(lsq) < 1.1920929e-07f) {
        v = vec3(0.0f, 0.0f, 0.0f);
        return;
    }
    const float inv = 1.0f / std::sqrt(lsq);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
}

// The Vec2 variant: zeroed below FLT_EPSILON^2 (1.4210855e-14), BAPAMergeSoldierInputs 0x1D3F00.
inline void normalize(Vec2& v)
{
    const float lsq = v.x * v.x + v.y * v.y;
    if (std::fabs(lsq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lsq) < 1.4210855e-14f) {
        v.x = 0.0f;
        v.y = 0.0f;
        return;
    }
    const float inv = 1.0f / std::sqrt(lsq);
    v.x *= inv;
    v.y *= inv;
}

// CommonSingletonServer::m_iBFTimer->vfn+0x08 / +0x0C (BAPATriggerContinously ctor 0x1E1BDF).
inline double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

inline double timeIncrease()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
}

// operator<<(std::ostream&, const BAPCondition&) (ELF 0x1F80D0, A-core's BAPCondition.cpp).
inline std::ostream& streamCondition(std::ostream& out, const BAPCondition* condition)
{
    return out << *condition;
}

// [FRONTIER] io::operator<<(std::ostream&, PlayerInputMap) is the engine's (IO/Input/InputMapStream.cpp,
// a bf2 export the shipped libaidll imports through its PLT); that TU is not in the bf2_aidll
// source list (coordinator decision requested), so the debug text stops here.
inline std::ostream& streamInput(std::ostream& out, io::PlayerInputMap)
{
    BF2_AI_FRONTIER("dice::hfe::io::operator<<(std::ostream&, PlayerInputMap) (engine export, not linked into bf2_aidll)");
    return out;
}

// operator<<(std::ostream&, BAPStatementType) (ELF 0x3C7D90) and operator<<(std::ostream&,
// BAPConditionType) (0x3C7AE0), ai-main's Main/AIStreamOperators.cpp.
inline std::ostream& streamType(std::ostream& out, BAPStatementType type)
{
    return out << type;
}

inline std::ostream& streamType(std::ostream& out, BAPConditionType type)
{
    return out << type;
}

// `std::vector<IAIObject*> ignore; ignore.push_back(object); IgnoreListPredicate pred(ignore);
// m_iaiObjectEnvironment->collideLineWithWorld(from, to, &pred, &hit)` (slot +0x44), spelled out
// in BAPConSplashDamage::evaluate (push_back 0x1EA618, ctor 0x1EA627), BAPConOkToThrowGrenade::
// evaluate (two traces; the second reuses the predicate after `ignore.resize(0);
// ignore.push_back(target)`, the same one-element list) and BAPWrapperAiming::getGroundLevelAtPos
// 0x1F8CF0. `hit` is the caller's zeroed output.
inline bool collideLineIgnoring(const Vec3& from, const Vec3& to, IAIObject* object, Vec3& hit)
{
    std::vector<IAIObject*> ignore;
    ignore.push_back(object);
    IgnoreListPredicate predicate(ignore);
    return static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->collideLineWithWorld(from, to, &predicate, &hit);
}

} // namespace bapinline
} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACTIONINLINE_H
