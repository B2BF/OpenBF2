// Ballistic aiming. Evidence: libaidll.so 1.5.3153 (ia-32), Aimer.cpp, 0x1C7120..0x1C8EA0
// (see Aimer.h). Read from the Ghidra decompile and checked against objdump where Ghidra hides x87
// precision (getFiringDirection's impact time is `double time + float t`, 0x1C8134..0x1C8143).

#include "Aimer.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1C8EA0, __tcf_0 0x1C8F00, _GLOBAL__I 0x1C8F30).
#include <iostream>

#include <cmath>
#include <float.h>       // _finite (MSVC)
#include <vector>

#include "../../../../Core/PlatformCompat.h"   // _finite elsewhere
#include "../../../../Math/Polynom.h"

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x4B7724 (.data, file scope in Aimer.cpp): 0.95 pi, the elevation limit. dice's spelling.
static float WIERD_ANGLE_CONSTANT = 2.984513f;

namespace {

// [CHOSEN, not recovered] the image's inlined `x*x + y*y + z*z` sqrt (with its sqrtf retry when
// the x87 fsqrt yields NaN; the retry gives the same value).
inline float length3(float x, float y, float z)
{
    return std::sqrt(x * x + y * y + z * z);
}

inline float timeStep()
{
    // IBFTimer slot +0xC (getTimeIncrease), narrowed to float as the image does.
    return static_cast<float>(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
}

} // namespace

// ELF 0x1C7120.
Aimer::Aimer()
{
    m_drag = 0.0f;
    m_gravity = 0.0f;
    m_speed = 1.0f;
    m_nSamples = 0;
    m_time = 0.0;
    m_relativePos.x = 1.0f; m_relativePos.y = 0.0f; m_relativePos.z = 0.0f;
    m_targetVelocity.x = 0.0f; m_targetVelocity.y = 0.0f; m_targetVelocity.z = 0.0f;
    m_shooterVelocity.x = 0.0f; m_shooterVelocity.y = 0.0f; m_shooterVelocity.z = 0.0f;
    m_relativeDir.x = 1.0f; m_relativeDir.y = 0.0f; m_relativeDir.z = 0.0f;
    m_forward.x = 0.0f; m_forward.y = 0.0f; m_forward.z = 1.0f;
    m_grenade = false;
    m_angle = 666.0f;
    m_flag69 = false;
    m_angleStep = 0.0174533f;
    // +0x04 and +0x58 are not written.
}

// ELF 0x1C72A0.
Aimer::~Aimer()
{
}

// ELF 0x1C72C0.
bool Aimer::setTargetData(const Vec3& relativePos, const Vec3& targetVelocity, const Vec3& shooterVelocity,
                          const Mat4* transform, double time)
{
    m_nSamples = 1;
    m_relativePos = relativePos;
    m_relativeDir = relativePos;
    const float lengthSq = m_relativeDir.x * m_relativeDir.x + m_relativeDir.y * m_relativeDir.y
                         + m_relativeDir.z * m_relativeDir.z;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) < 1.1920929e-07f)
        {
            m_relativeDir.x = 0.0f; m_relativeDir.y = 0.0f; m_relativeDir.z = 0.0f;
        }
        else
        {
            const float inv = 1.0f / std::sqrt(lengthSq);
            m_relativeDir.x *= inv; m_relativeDir.y *= inv; m_relativeDir.z *= inv;
        }
    }
    m_targetVelocity = targetVelocity;
    m_shooterVelocity = shooterVelocity;
    m_forward.x = 0.0f; m_forward.y = 0.0f; m_forward.z = 1.0f;
    m_time = time;
    if (transform)
    {
        // (0, 0, 1) rotated by the transform's upper 3x3 (row vector times matrix).
        const float* m = transform->m;
        const float fx = m_forward.x, fy = m_forward.y, fz = m_forward.z;
        m_forward.x = fx * m[0] + fy * m[4] + fz * m[8];
        m_forward.y = fx * m[1] + fy * m[5] + fz * m[9];
        m_forward.z = fx * m[2] + fy * m[6] + fz * m[10];
    }
    return true;
}

// ELF 0x1C74C0. The squared-speed drag applies when the INITIAL speed is above 200.
float Aimer::getImpactTime(float distance, float speed)
{
    const float dt = timeStep();
    float time = 0.0f;
    float v = speed;
    while (distance > 0.0f)
    {
        float dragTerm = v;
        if (speed > 200.0f)
            dragTerm = v * v;
        float next = v - dragTerm * m_drag * dt;
        if (next < 0.0f)
            next = 0.0f;
        const float step = (v + next) * 0.5f * dt;
        if (step <= 0.0f || 1000.0f * step < distance)
            return 1000.0f;
        time += (distance < step) ? (distance / step) * dt : dt;
        distance -= step;
        v = next;
    }
    return time;
}

// ELF 0x1C75D0. Trapezoidal integration in timer steps; the drag is linear in the velocity up to
// an initial speed of 200 and quadratic (current |v|) above.
void Aimer::getPredictedImpactPosition(Vec3 velocity, float time, Vec3& out) const
{
    const float speed = length3(velocity.x, velocity.y, velocity.z);
    out.x = 0.0f; out.y = 0.0f; out.z = 0.0f;
    if (speed == 0.0f)
        return;
    float dt = timeStep();
    for (; time > 0.0f; time -= dt)
    {
        if (time < dt)
            dt = time;
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;
        if (m_drag != 0.0f)
        {
            if (speed <= 200.0f)
            {
                dx = velocity.x * m_drag; dy = velocity.y * m_drag; dz = velocity.z * m_drag;
            }
            else
            {
                const float v = length3(velocity.x, velocity.y, velocity.z);
                dx = velocity.x * v * m_drag; dy = velocity.y * v * m_drag; dz = v * velocity.z * m_drag;
            }
            dx *= dt; dy *= dt; dz *= dt;
        }
        const float nx = (velocity.x - dx) + dt * 0.0f;
        const float ny = (velocity.y - dy) + m_gravity * dt;
        const float nz = (velocity.z - dz) + dt * 0.0f;
        out.x = (velocity.x + nx) * 0.5f * dt + out.x;
        out.y = (velocity.y + ny) * 0.5f * dt + out.y;
        out.z = (velocity.z + nz) * 0.5f * dt + out.z;
        velocity.x = nx; velocity.y = ny; velocity.z = nz;
    }
}

// ELF 0x1C7970. Drag-free ballistic elevation: the larger root of the trajectory equation.
bool Aimer::getStaticFiringDirection(Vec3& direction, Vec3& target) const
{
    target = m_relativePos;
    const float horizontal = std::sqrt(m_relativePos.x * m_relativePos.x + m_relativePos.z * m_relativePos.z);
    const float g = -m_gravity;
    const float speedSq = m_speed * m_speed;
    const float b = -horizontal;
    const float a = (horizontal * horizontal * g) / (speedSq + speedSq);
    const float disc = b * b - a * 4.0f * (a + m_relativePos.y);
    if (disc < 0.0f)
        return false;
    const float root = std::sqrt(disc);
    const float first = std::atan((root - b) / (a + a));
    float angle = std::atan((-b - root) / (a + a));
    if (angle <= first)
        angle = first;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    direction.x = m_relativePos.x * (c / horizontal);
    direction.y = s;
    direction.z = (c / horizontal) * m_relativePos.z;
    return true;
}

// ELF 0x1C7B80.
bool Aimer::getTurnPointForGrenade(const Vec3& direction, Vec3& point, float& time) const
{
    const float up = direction.y;
    if (up > 0.0f)
    {
        const float t = (up * m_speed) / -m_gravity;
        time = t;
        point.x = direction.x * t * m_speed;
        point.z = direction.z * t * m_speed;
        point.y = up * m_speed * 0.5f * t;
    }
    return up > 0.0f;
}

// ELF 0x1C7C40. `accuracy` sets the iteration count (accuracy * 8 + 2); `highArc` starts from
// 3/8 pi (or halfway to 0.48 pi from the previous angle) and steps downwards. A zero speed (no
// projectile) aims 20 m above the target. The step (+0x64) is kept between calls; a search that
// did not bracket the target doubles it (at most 0.017) and fails once it is that large.
bool Aimer::getFiringDirection(double time, float accuracy, bool highArc, Vec3& direction, Vec3& target,
                               double* impactTime)
{
    const bool noSpeed = m_speed == 0.0f;
    float relY = m_relativePos.y;
    if (noSpeed)
        relY += 20.0f;
    const float distance = length3(m_relativePos.x, relY, m_relativePos.z);
    const float closing = (m_speed - (m_targetVelocity.y * m_relativeDir.y + m_targetVelocity.x * m_relativeDir.x
                                      + m_targetVelocity.z * m_relativeDir.z))
                        + m_relativeDir.x * m_shooterVelocity.x + m_relativeDir.y * m_shooterVelocity.y
                        + m_relativeDir.z * m_shooterVelocity.z;
    if (closing < 1.0f)
        return false;
    const float firstEstimate = distance / closing;
    if (firstEstimate < 0.0f)
        return false;
    double impactAt = time + firstEstimate;
    getPredictedTargetPosition(impactAt, target);
    float horizontal = std::sqrt(target.z * target.z + target.x * target.x);
    if (horizontal == 0.0f)
        return false;
    float height = target.y;
    if (noSpeed)
        height += 20.0f;

    float angle;
    if (!highArc || m_angle >= 0.7853982f)
    {
        if (!noSpeed)
        {
            angle = m_angle;
            if (angle != 666.0f)
                goto haveAngle;
        }
        angle = std::atan2(height, horizontal);
    }
    else
    {
        angle = 1.1780972f;
    }
haveAngle:
    {
        const float step = m_angleStep;
        const float sign = step > 0.0f ? 1.0f : (step >= 0.0f ? 0.0f : -1.0f);
        float magnitude = std::fabs(step);
        if (magnitude < 0.0001f)
            magnitude = 0.0001f;
        m_angleStep = magnitude * sign;
    }
    if (highArc && !noSpeed)
    {
        angle = (angle + 1.5079645f) * 0.5f;
        if (m_angleStep > 0.0f)
            m_angleStep = -m_angleStep;
    }

    bool converged = false;
    int direction0 = 0;                  // first step direction: +1 up, -1 down
    const float drag = m_drag;
    const unsigned int iterations = static_cast<unsigned int>(accuracy * 8.0f + 2.0f);
    unsigned int iteration = 0;
    unsigned int convergedCount = 0;
    while (convergedCount < iterations)
    {
        // horizontal unit direction to the predicted target
        float hx = target.x, hy = 0.0f, hz = target.z;
        const float hSq = hx * hx + 0.0f + hz * hz;
        if (std::fabs(hSq - 1.0f) >= 1.1920929e-07f)
        {
            if (std::fabs(hSq) < 1.1920929e-07f)
            {
                hx = 0.0f; hz = 0.0f;
            }
            else
            {
                const float inv = 1.0f / std::sqrt(hSq);
                hy = inv * 0.0f; hx = target.x * inv; hz = inv * target.z;
            }
        }
        float horizontalSpeed = m_shooterVelocity.z * hz + m_shooterVelocity.x * hx + m_shooterVelocity.y * hy
                              + std::cos(angle) * m_speed;
        float verticalSpeed = std::sin(angle) * m_speed + m_shooterVelocity.y;
        if (horizontalSpeed <= 0.1f)
            return false;
        float t = drag != 0.0f ? getImpactTime(horizontal, horizontalSpeed) : horizontal / horizontalSpeed;
        if (t > 60.0f)
        {
            angle = std::atan2(height, horizontal);
            m_angle = angle;
            horizontalSpeed = m_shooterVelocity.z * hz + m_shooterVelocity.x * hx + m_shooterVelocity.y * hy
                            + std::cos(angle) * m_speed;
            verticalSpeed = std::sin(angle) * m_speed + m_shooterVelocity.y;
            getPredictedTargetPosition(time + firstEstimate, target);
            const float h = std::sqrt(target.z * target.z + target.x * target.x);
            t = drag != 0.0f ? getImpactTime(h, horizontalSpeed) : h / horizontalSpeed;
        }
        if (t > 100.0f)
            return false;
        Vec3 velocity;
        velocity.x = horizontalSpeed; velocity.y = verticalSpeed; velocity.z = 0.0f;
        Vec3 impact;
        impact.x = 0.0f; impact.y = 0.0f; impact.z = 0.0f;
        getPredictedImpactPosition(velocity, t, impact);
        m_verticalExtreme = t * 0.5f;
        impactAt = time + t;
        getPredictedTargetPosition(impactAt, target);
        horizontal = std::sqrt(target.z * target.z + target.x * target.x);
        height = target.y;
        if (noSpeed)
            height += 20.0f;
        if (m_grenade && !m_flag69)
            height -= 1.3f;
        if (!_finite(static_cast<double>(horizontal)) || !_finite(static_cast<double>(height)))
            return false;
        if (noSpeed)
        {
            converged = true;
            break;
        }
        if (iteration == iterations - 1)
            break;
        if (height <= impact.y)
        {
            if (angle <= -WIERD_ANGLE_CONSTANT)
                return false;
            if (direction0 == 0)
                direction0 = -1;
            else if (direction0 > 0)
                converged = true;
            if (converged)
                m_angleStep *= 0.5f;
            const float next = angle - m_angleStep;
            angle = -WIERD_ANGLE_CONSTANT;
            if (-WIERD_ANGLE_CONSTANT <= next)
                angle = next;
        }
        else
        {
            if (WIERD_ANGLE_CONSTANT <= angle)
                return false;
            if (direction0 == 0)
                direction0 = 1;
            else if (direction0 < 0)
                converged = true;
            if (converged)
                m_angleStep *= 0.5f;
            angle += m_angleStep;
            if (WIERD_ANGLE_CONSTANT < angle)
                angle = WIERD_ANGLE_CONSTANT;
        }
        ++iteration;
        if (iteration > 0x400)
            break;
        if (converged)
            ++convergedCount;
    }
    m_angle = angle;
    if (!converged)
    {
        const float step = m_angleStep;
        float doubled = step + step;
        if (0.017f < step + step)
            doubled = 0.017f;
        m_angleStep = doubled;
        if (std::fabs(step) >= 0.017f)
            return false;
    }
    const float c = std::cos(angle) / horizontal;
    direction.x = target.x * c;
    direction.y = std::sin(angle);
    direction.z = c * target.z;
    const float len = length3(direction.x, direction.y, direction.z);
    if (len < 0.95f)
        return false;
    const float inv = 1.0f / len;
    direction.x *= inv; direction.y *= inv; direction.z = inv * direction.z;
    if (impactTime)
        *impactTime = impactAt;
    return true;
}

// ELF 0x1C8770. A grenade uses its fixed fuse time; a grenade without +0x69 aims 10% short.
void Aimer::getPredictedTargetPosition(double time, Vec3& out) const
{
    static float grenadeExplosionTime = 2.8f;   // ELF 0x4B7728 (.data)
    float dt = static_cast<float>(time - m_time);
    if (m_grenade)
        dt = grenadeExplosionTime;
    out.x = m_relativePos.x + m_targetVelocity.x * dt;
    out.y = m_relativePos.y + m_targetVelocity.y * dt;
    out.z = dt * m_targetVelocity.z + m_relativePos.z;
    if (m_grenade && !m_flag69)
    {
        out.x -= m_relativePos.x * 0.1f;
        out.y -= m_relativePos.y * 0.1f;
        out.z -= m_relativePos.z * 0.1f;
    }
}

// ELF 0x1C8850.
bool Aimer::getImpactPosition(Vec3& out, const Vec3& direction, float time) const
{
    if (time > 100.0f)     // 0x1C887A jbe: a NaN time integrates and returns true (0x1C88ED)
        return false;
    Vec3 velocity;
    velocity.x = direction.x * m_speed + m_shooterVelocity.x;
    velocity.y = direction.y * m_speed + m_shooterVelocity.y;
    velocity.z = m_speed * direction.z + m_shooterVelocity.z;
    getPredictedImpactPosition(velocity, time, out);
    return true;
}

// ELF 0x1C8900. Roots of a cubic in t built from the relative motion (the coefficients as the
// image computes them), then the root in [0, 100] whose impact lies closest to the target.
bool Aimer::getNearestImpactPoint(const Vec3& direction, Vec3& out, double& time) const
{
    std::vector<float> roots;
    const float vx = direction.x * m_speed + m_shooterVelocity.x;
    const float vy = direction.y * m_speed + m_shooterVelocity.y;
    const float vz = direction.z * m_speed + m_shooterVelocity.z;
    const float k = 1.0f / (1.0f - m_drag);
    const float ay = (vy * m_drag + m_gravity) * k;
    const float ax = (vx * m_drag + 0.0f) * k;
    const float az = k * (vz * m_drag + 0.0f);
    const float rx = vx - m_targetVelocity.x;
    const float ry = vy - m_targetVelocity.y;
    const float rz = vz - m_targetVelocity.z;
    const float c0 = -(rz * m_relativePos.z + m_relativePos.y * ry + m_relativePos.x * rx);
    const float c1 = (rz * rz + rx * rx + ry * ry)
                   - (m_relativePos.x * ax + m_relativePos.y * ay + m_relativePos.z * az);
    Polynom polynom(4);
    polynom[0] = c0 + c0;
    polynom[1] = c1 + c1;
    polynom[2] = (rz * az + rx * ax + ry * ay) * 3.0f;
    polynom[3] = az * az + ax * ax + ay * ay;
    polynom.findRoots(roots);
    if (roots.empty())
        return false;
    float best = 3.4028235e+38f;
    bool found = false;
    for (unsigned int i = 0; i < roots.size(); ++i)
    {
        const float r = roots[i];
        if (0.0f <= r && r <= 100.0f)
        {
            Vec3 impact;
            impact.x = 0.0f; impact.y = 0.0f; impact.z = 0.0f;
            getImpactPosition(impact, direction, r);
            const float t = roots[i];
            const float px = m_targetVelocity.x * t + m_relativePos.x;
            const float py = m_targetVelocity.y * t + m_relativePos.y;
            const float pz = t * m_targetVelocity.z + m_relativePos.z;
            const float dx = px - impact.x, dy = py - impact.y, dz = pz - impact.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best)
            {
                time = t;
                out = impact;
                found = true;
                best = d2;
            }
        }
    }
    return found;
}

// ELF 0x1C8E90.
double Aimer::getVerticalExtreme() const
{
    return m_verticalExtreme;
}

} // namespace ai
} // namespace hfe
} // namespace dice
