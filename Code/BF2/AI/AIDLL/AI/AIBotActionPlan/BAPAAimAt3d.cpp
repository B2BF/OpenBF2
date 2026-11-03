// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1CE1D0 (__tcf_0 0x1CE230, _GLOBAL__I 0x1CE260): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAt3d.h"

#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The weapon transform's third and fourth rows (+0xEC forward, +0xFC position), passed by address.
// [CHOSEN, not recovered] spelling of the image's `this + 0xEC` / `this + 0xFC`.
const Vec3& forwardOf(const Mat4& transform)
{
    return *reinterpret_cast<const Vec3*>(&transform.m[8]);
}

const Vec3& positionOf(const Mat4& transform)
{
    return *reinterpret_cast<const Vec3*>(&transform.m[12]);
}

void addTo(Vec3& v, const Vec3& add)
{
    v.x = add.x + v.x;
    v.y = add.y + v.y;
    v.z = add.z + v.z;
}

} // namespace

// ELF 0x1CDE70 (C1; C2 0x1CDC90). m_condition, m_impactTime and the subclass members are left
// to the subclass constructors, as compiled.
BAPAAimAt3d::BAPAAimAt3d()
{
    m_offset = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_vec24 = bapinline::vec3(1.0f, 0.0f, 0.0f);
    m_vec30 = bapinline::vec3(0.0f, 0.0f, 1.0f);
    m_vec3C = bapinline::vec3(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
        m_vecs48[i] = bapinline::vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
        m_vecs78[i] = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_vecA8 = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_lastAimVec = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_predictedPosition = bapinline::vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 16; ++i)
        m_aimerTransform.m[i] = 0.0f;
    m_aimerTransform.m[0] = m_aimerTransform.m[5] = m_aimerTransform.m[10] = m_aimerTransform.m[15] = 1.0f;
    m_aimVecValid = false;
    m_int1C4 = 0;
    for (int i = 0; i < 4; ++i)
        m_floats1AC[i] = 0.0f;
    m_vec1C8 = bapinline::vec3(0.0f, 0.0f, 1.0f);
}

// ELF 0x1CE360 (weak; D0 0x1CE3C0): the Aimer member, then BAPStatement's destructor.
BAPAAimAt3d::~BAPAAimAt3d()
{
}

// ELF 0x1CDC70. Only an interpreter entry (EntryHeliAimAt, ...) can execute an aim: on its own
// the statement fails at once.
bool BAPAAimAt3d::execute(Bot*, bool& ok, bool& finished)
{
    ok = false;
    finished = true;
    return false;
}

// ELF 0x1CE280 (weak)
BAPStatementType BAPAAimAt3d::getId() const
{
    return BAPSTAimAt;
}

// ELF 0x1CE290 (weak)
bool BAPAAimAt3d::hasDefault()
{
    return false;
}

// ELF 0x1CE2A0 (weak)
const Mat4& BAPAAimAt3d::getAimerTransform() const
{
    return m_aimerTransform;
}

// ELF 0x1CE2B0 (weak)
bool BAPAAimAt3d::isAimVecValid() const
{
    return m_aimVecValid;
}

// ELF 0x1CE2C0 (weak)
bool BAPAAimAt3d::getLastAimVec(Vec3& aimVec)
{
    aimVec = m_lastAimVec;
    return m_aimVecValid;
}

// ELF 0x1CE300 (weak)
bool BAPAAimAt3d::getPredictedPosition(Vec3& position) const
{
    position = m_predictedPosition;
    return m_aimVecValid;
}

// ELF 0x1CE340 (weak)
bool BAPAAimAt3d::getImpactTime(double& time) const
{
    time = m_impactTime;
    return m_aimVecValid;
}

// ELF 0x1CE050. The impact point `time` seconds after firing along the weapon's forward axis, in
// world space.
bool BAPAAimAt3d::getImpactPosition(Vec3& position, double time) const
{
    const bool result = m_aimer.getImpactPosition(position, forwardOf(m_aimerTransform), static_cast<float>(time));
    addTo(position, positionOf(m_aimerTransform));
    return result;
}

// ELF 0x1CE0D0. The point of the forward-axis trajectory nearest the target, in world space.
bool BAPAAimAt3d::getNearestImpactPosition(Vec3& position, double& time) const
{
    const bool result = m_aimer.getNearestImpactPoint(forwardOf(m_aimerTransform), position, time);
    addTo(position, positionOf(m_aimerTransform));
    return result;
}

// ELF 0x1CE150. The target's predicted position `time` seconds from now, in world space.
void BAPAAimAt3d::getPredictedPosition(double time, Vec3& position) const
{
    m_aimer.getPredictedTargetPosition(bapinline::gameTime() + time, position);
    addTo(position, positionOf(m_aimerTransform));
}

} // namespace ai
} // namespace hfe
} // namespace dice
