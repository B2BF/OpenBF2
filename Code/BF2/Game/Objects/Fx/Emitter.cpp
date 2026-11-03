// Legacy particle emission. Emission and the camera/LOD message tail compile only into the client.
// Start fills the shared template's alternate cache; stop requires a parent. Keep repeated virtual calls.
// Emission preserves the pre-decrement delay, cumulative per-particle rotation and previous object on null alternates.
// Zero delta is unguarded; hidden updates retain the previous position for later interpolation.
// [PLATFORM] Client sampling draws relative sign first; emitter-count virtuals use different base faces.
// Source path is attested by __FILE__; the original TU also contains EmitterTemplate.

#include "Emitter.h"
#include "EffectBundle.h"                                   // g_effectsDisabled, add/decNumEmittersPlaying,
                                                            // m_fovOverride (EffectBundle befriends Emitter)
#include "../TemplateLeaves.h"                              // EmitterTemplate
#include "../ObjectTemplate.h"                              // ObjectTemplate::getIsPreCaching
#include "../ObjectLayoutAccess.h"                          // physicsNode / geometry of other objects
#include "../../../Core/Ids.h"                              // CID_EffectBundle/Particle/SpriteParticleTemplate
#include "../../../Core/Random.h"                           // RandomDistribution, Rand (ELF dice::hfe::Rand)
#include "../../../Core/OverTimeDistribution.h"
#include "../../../Core/System.h"                           // g_system->getExactTime (ELF +0x18 / PE +0x14)
#include "../../../Physics/MaterialManager.h"               // playParticleCollisionSound
#include "../../../Sound/NullSound.h"                       // ISound
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#if !DEDICATED
#include "Particle.h"                                       // fx2_particle: setEmitter + three raw fields
#include "../GeometryPartAccess.h"                          // geometry_part::slot - the raw geometry seam
#include "../IPlayer.h"
#include "../ICameraObject.h"
#include "../../../Core/Debug.h"
#include "../../../Math/Mat4.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../Main/BF2Engine.h"                           // bf2Engine->getBF2Render()
#include "../../Main/BF2ClientSeams.h"                      // BF2Render::getRenderer
#include "../../../RendMisc/IRenderer.h"                    // IRenderer::getFrameOrdinal (+0x54)
#include <cstddef>
#endif
#include <cfloat>
#include <cmath>

namespace dice {
namespace hfe {
#if !DEDICATED
extern BF2Engine* bf2Engine;                                // Core/Globals.cpp; PE ds:0x986F48
#endif
namespace world {

extern IBase* materialManager;                              // ds:0x8AD2820 / PE ds:0x9C6C88 - Core/Globals.cpp
#if !DEDICATED
extern IBase* heightmapCluster;                             // ds:0x8ABAF24 / PE ds:0xA07E28 - Core/Globals.cpp
#endif

// _ZN4dice3hfe5world15getCurrentFrameEv - Game/Objects/Fx/WheelEffectBundle.cpp (ELF 0x8215050 `return 0`,
// BF2.exe 0x6447B0 = renderer frame ordinal + 100). Declared again here as Game/Main/BF2Engine.h does.
unsigned int getCurrentFrame();

// ELF ds:0x8AAE429 / BF2.exe ds:0x9F6D59 - .bss, zero-initialised in both images. See Emitter.h for why
// its twin mDebugNoParticles (ds:0x8AAE428 / 0x9F6D58) is this tree's g_effectsDisabled.
bool Emitter::mDebugNoOldParticles = false;

namespace {
typedef object_detail::LayoutAccess L;

// Samples the distribution using the shared generator; unit draws lie in (0,1].
// [PLATFORM] Relative distributions draw sign before value on the client, after value in the ELF.
// This follows client order; dedicated factories do not instantiate emitters.
float unitDraw(Random& random)
{
    return static_cast<float>((static_cast<int>(random.getRaw() | 0x80u) >> 7) + 1) * 0.000000059604645f;
}

float sampleDistribution(const RandomDistribution& distribution, Random& random)
{
    const float sign = (distribution.relative && unitDraw(random) <= 0.5f) ? -1.0f : 1.0f;
    float result;
    switch (distribution.distribution)
    {
    case CRD_NONE:        result = distribution.value; break;
    case CRD_UNIFORM:     result = distribution.value + (distribution.variation - distribution.value) * unitDraw(random); break;
    case CRD_EXPONENTIAL: result = -distribution.value * std::log(unitDraw(random)); break;
    case CRD_NORMAL:      result = distribution.value + random.getNormal() * distribution.variation; break;
    default:              result = 0.0f; break;
    }
    return result * sign;
}

#if !DEDICATED
// BF2.exe 0x5A2800 (191 B), read in full: rotate the 3x3 basis of `m` by `degrees` about `axis` and keep
// its translation - identity (0x440B20), setRotateAboutLine (client 0x6DA7E0), a copy (0x4531C0), then
// rotMult (0x6D9CC0). Nothing happens unless |degrees| > FLT_EPSILON (ds:0x87FE7C, `test ah, 41h; jne` -
// equal and NaN also skip). Camera.cpp writes the same shape inline. [CHOSEN] name: the client symbol
// is stripped and the ELF never needed it (both callers are the client-only handleUpdate).
// Both callers pass `m`'s own row 2 as the axis: setRotateAboutLine reads it before `m` is written.
void rotateAboutLine(Mat4& m, float degrees, const Vec3& axis)
{
    if (std::fabs(degrees) > FLT_EPSILON)
    {
        Mat4 rotation = Mat4::getIdentity();
        setRotateAboutLine(rotation, degrees, axis);
        const Mat4 previous = m;
        m.rotMult(previous, rotation);
    }
}

// The speed handleUpdate hands calcInvItensity, built inline at all three BF2.exe sites (0x647F71..,
// .., 0x6488E2..): the root's physics node's tangent speed at the emitter's visual position
// (+0x8C getVisualAbsoluteTransformation, translation row), as Vec3::length (0x463980,
// sqrt((x*x + y*y) + z*z)); 0 when the root has no physics node. The transform is fetched afresh at
// every call, as shipped.
float emitterSpeed(Emitter* emitter, IPhysicsNode* rootPhysics)
{
    if (!rootPhysics)
        return 0.0f;
    const Mat4& visual = emitter->getVisualAbsoluteTransformation();
    const Vec3& tangent = rootPhysics->getTangentSpeed(*reinterpret_cast<const Vec3*>(&visual.m[12]));
    return std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
}

// BF2.exe 0x647FC8..0x647FD7, inline: bf2Engine (ds:0x986F48) -> bf2Render (+0x18) -> renderer (+0x10)
// -> IRenderer::getFrameOrdinal (+0x54) - the RAW ordinal; m_visibleFrame already carries
// getCurrentFrame's +100. The PE loads both pointers before it tests m_visibleFrame == 0 (a hoist;
// only a missing BF2Render could tell the difference).
unsigned int currentFrameOrdinal()
{
    return bf2Engine->getBF2Render()->getRenderer()->getFrameOrdinal();
}

// BF2.exe 0x647930 (68 B), read in full: clamp t to 1 (`test ah, 41h; jne` keeps <= 1 and NaN), scale
// by 100, truncate (0x83D84C), fraction = scaled - index - the OverTimeDistribution step calcInvItensity
// inlines. dead IN THE shipped client: its only caller, handleUpdate's emission block (0x648148),
// passes m_age / m_timeToLive and overwrites both outputs before any read. Kept at the source level so
// the body reads as shipped; an optimising build drops the call (the verifier's /O2 listing has none).
// [CHOSEN] name; the client passes the template's intensityOverTime as an unused `this`.
void intensityCurvePosition(float t, int& index, float& fraction)
{
    if (t > 1.0f)
        t = 1.0f;
    const float scaled = t * 100.0f;
    index = static_cast<int>(scaled);
    fraction = scaled - static_cast<float>(index);
}

// Sets first-person/alternate-view flags on a valid renderer geometry; callees consume the low byte.
// Uses the [CHOSEN] raw geometry seam. ELF slot shifts remain [INFERRED]; this path is client-only.
void geometrySetByteFlag(void* geometry, unsigned int peSlot, unsigned int elfSlot, bool value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    typedef void (__thiscall* Function)(void*, unsigned int);
#else
    typedef void (*Function)(void*, unsigned int);
#endif
    geometry_part::slot<Function>(geometry, peSlot, elfSlot)(geometry, value ? 1u : 0u);
}

// Seeds particle geometry selection before init; randomization overrides the explicit template part.
// Preserve repeated template-part reads.
void presetParticleGeometryPart(Particle* particle, const EmitterTemplate* objectTemplate)
{
    if (objectTemplate->m_randomizeGeomPart)
        particle->m_randomizeGeomPart = true;
    else if (objectTemplate->m_geometryPartId != -1)
    {
        particle->m_geometryPart = objectTemplate->m_geometryPartId;
        particle->m_assignedGeometryPart = objectTemplate->m_geometryPartId;
    }
}
#endif // !DEDICATED
} // namespace

// Constructor - ELF C2 0x81FE310 / C1 0x81FE3D0 (identical, 186 B), BF2.exe 0x647D20 (143 B). Same stores
// in both images (PE offsets +0x18): +0x138 0, +0x13C -1.0f, +0x148 0, +0x14C 0, +0x150..+0x158 0,
// +0x15C 0, +0x160 300.0f, +0x168 -1.0f, byte +0x164 0 - +0x140 (m_delay) and +0x144 (m_timeToLive) are
// NOT written. Then updateFlags(1, 0) - the disabled bit; the emitter sleeps until start enables it -
// and, while the template manager is precaching (ObjectTemplate::getIsPreCaching, ELF 0x8322610 /
// BF2.exe 0x71BD80), the template's preCache (template vtable ELF +0x6C / PE +0x68).
Emitter::Emitter(const EmitterTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
{
    m_particleTemplate = 0;
    m_unknown13C = -1.0f;
    m_age = 0.0f;
    m_nextEmitTime = 0.0f;
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
    m_visibleFrame = 0;
    m_lodDistance = 300.0f;
    m_startTime = -1.0f;
    m_hasFrameUpdate = false;
    updateFlags(1, 0);
    if (ObjectTemplate::getIsPreCaching())
        objectTemplate->preCache();
#if !DEDICATED && defined(_MSC_VER) && defined(_M_IX86)
    // EffectBundle::init's LOD cap was a raw store at sizeof(SimpleObject) + 0x28 until this class was
    // published (ELF +0x160 / PE +0x178 - 0x28 past each image's SimpleObject). Keep the x86 client's
    // relative layout honest for anything that still assumes it.
    static_assert(offsetof(Emitter, m_lodDistance) == sizeof(SimpleObject) + 0x28,
                  "Emitter keeps retail's relative field layout on the x86 client");
#endif
}

// ELF D2 0x81FE490 / D1 0x81FE4C0 / D0 0x81FE4F0 (reset both vptrs, SimpleObject's D2, D0 deletes);
// BF2.exe 0x647DB0 (the same plus the scalar-deleting flag). No own cleanup: the cached templates are
// not owned and nothing else is allocated.
Emitter::~Emitter()
{
}

// (95 B) / BF2.exe 0x647980 (88 B). A startAtCreation emitter without a parent starts
// itself (virtual handleMessage(0) - ELF +0x104 / PE +0x100); every emitter then takes the template's
// lodDistance (ELF +0x174 / PE +0x224). No stock Emitter sets startAtCreation.
void Emitter::init()
{
    SimpleObject::init();
    const EmitterTemplate* const t = static_cast<const EmitterTemplate*>(getTemplate());
    if (t->m_startAtCreation && !getParent())
        handleMessage(static_cast<TemplateMessage>(0), 0, false);
    m_lodDistance = t->m_lodDistance;
}

// Returns seconds per particle from sampled intensity, lifetime curve and speed scaling.
// Near-zero/nonfinite intensity returns 100; otherwise returns the absolute reciprocal.
// Test the double product for finiteness before float rounding and preserve NaN comparison behavior.
float Emitter::calcInvItensity(const EmitterTemplate* objectTemplate, float speed) const
{
    const float speedFactor = objectTemplate->m_intensityAtSpeed < FLT_EPSILON
                            ? 1.0f : speed / objectTemplate->m_intensityAtSpeed;
    float t = m_age / m_timeToLive;
    if (t > 1.0f)
        t = 1.0f;
    const float scaled = t * 100.0f;
    const int index = static_cast<int>(scaled);
    const float fraction = scaled - static_cast<float>(index);
    const float curve = (1.0f - fraction) * objectTemplate->m_intensityOverTime.m_samples[index]
                      + fraction * objectTemplate->m_intensityOverTime.m_samples[index + 1];
    const double product = static_cast<double>(sampleDistribution(objectTemplate->m_intensity, Rand))
                         * curve * speedFactor;
    float intensity = static_cast<float>(product);
    if (!std::isfinite(product))
        intensity = 100.0f;
    if (std::fabs(intensity) < FLT_EPSILON)
        return 100.0f;
    return std::fabs(1.0f / intensity);
}

// handleVisualUpdate - ELF 0x81FE990 (55 B) / BF2.exe 0x647AE0 (84 B). The first child's geometry is
// refreshed (IObject +0xF0 getChild / child +0xB8 updateGeometry; PE +0xEC / +0xB4), then the emitter
// records that it was seen: m_visibleFrame = getCurrentFrame() (BF2.exe inlines it - ordinal + 100), or 0
// once the view distance passes m_lodDistance (`test ah, 41h; jne` - equal and NaN keep the frame).
// The ELF stores a constant 0: getCurrentFrame folds to 0 there and both arms collapse.
void Emitter::handleVisualUpdate(float /*deltaTime*/, float distance)
{
    if (IObject* const child = getChild())
        child->updateGeometry();
    m_visibleFrame = getCurrentFrame();
    if (distance > m_lodDistance)
        m_visibleFrame = 0;
}

// (15 B) / BF2.exe 0x647B40 (10 B): the frame-update latch that lets m_age advance.
void Emitter::handleFrameUpdate(float /*deltaTime*/)
{
    m_hasFrameUpdate = true;
}

// Advances client emission timing and spawns due particles along the frame's travel.
// Nonlooping emitters stop at expiry; loops resample timing. Hidden/out-of-LOD emitters skip spawning
// and catch up only after falling 1.5 seconds behind. Dedicated updates are empty.
void Emitter::handleUpdate(float deltaTime, unsigned int flags)
{
#if !DEDICATED
    if (flags & 4)
        return;
    if (g_effectsDisabled)                          // Emitter::mDebugNoParticles, ds:0x9F6D58
        return;
    if (mDebugNoOldParticles)                       // ds:0x9F6D59
        return;
    if (!m_particleTemplate)
        return;
    const EmitterTemplate* const t = static_cast<const EmitterTemplate*>(getTemplate());
    IObject* const root = getRootParent(this);
    if (!root)
        return;
    IPhysicsNode* const rootPhysics = static_cast<IPhysicsNode*>(L::physicsNode(root));   // root +0x44

    // 2. the wall-clock end of a one-shot emitter: `test ah, 41h` (> 0 strictly) and
    // `fcompp; test ah, 5; jp` (lifetime < elapsed strictly) - the elapsed time stays double.
    if (!t->m_looping && m_startTime > 0.0f &&
        m_timeToLive < g_system->getExactTime() - m_startTime)
    {
        handleMessage(static_cast<TemplateMessage>(1), 0, false);
        return;
    }

    // 3. the start delay. quirk 3: once it runs out, the frame's delta becomes
    // (delay - delta) + delta - the PRE-decrement delay - rather than the time left after it.
    // `test ah, 5; jp` leaves on anything but a strictly negative remainder (0 and NaN included).
    if (m_delay > 0.0f)
    {
        m_delay = m_delay - deltaTime;
        if (!(m_delay < 0.0f))
            return;
        deltaTime = m_delay + deltaTime;
        m_delay = 0.0f;
    }

    // 4. the emission window. `fcomp; test ah, 1; je` takes the window-over arm only on an
    // ordered age >= lifetime; otherwise (age below, or either NaN) a lifetime of exactly 0 also ends it.
    if (m_hasFrameUpdate)
        m_age = deltaTime + m_age;
    const bool windowOver = (m_age >= m_timeToLive) ? true : (m_timeToLive == 0.0f);
    if (windowOver)
    {
        if (!t->m_looping)
        {
            handleMessage(static_cast<TemplateMessage>(1), 0, false);
            return;
        }
        m_delay = sampleDistribution(t->m_delay, Rand);
        m_timeToLive = sampleDistribution(t->m_timeToLive, Rand);
        if (m_timeToLive < 0.01f)                   // ds:0x892ADC, NaN kept
            m_timeToLive = 0.01f;
        m_age = 0.0f;
        m_nextEmitTime = calcInvItensity(t, emitterSpeed(this, rootPhysics));
    }

    // 5. visibility. Culled when the emitter has a particle template (re-tested, as shipped) and was
    // not visually updated within LOD in the last 100 frames (unsigned `jae`).
    bool culled = false;
    if (m_particleTemplate && (m_visibleFrame == 0 || m_visibleFrame < currentFrameOrdinal()))
        culled = true;
    bool firstPerson = false;
    bool hidden = false;
    if (IPlayerControlObject* const owner = findOwningPlayerControlObject(this))
    {
        // getOccupingPlayer [sic] is called twice, as shipped (0x647FFB / 0x648006).
        if (owner->getOccupingPlayer() && owner->getOccupingPlayer()->hasFirstPersonView())
        {
            firstPerson = true;
            hidden = t->m_showInThirdPerson && !t->m_showInFirstPerson;     // PE +0x5B4 / +0x5B3
        }
        else
        {
            firstPerson = false;
            hidden = !t->m_showInThirdPerson && t->m_showInFirstPerson;
        }
    }
    if (hidden || culled)
    {
        // only a looping emitter catches up, and only once it is more than 1.5 s behind
        // (ds:0x8E8CCC); quirk 7: m_lastPosition is left alone.
        if (t->m_looping && m_nextEmitTime + 1.5f < m_age)
            while (m_age > m_nextEmitTime)
                m_nextEmitTime = calcInvItensity(t, emitterSpeed(this, rootPhysics)) + m_nextEmitTime;
        return;
    }

    // 6. emission, 0x6480EE.
    Vec3 position = { 0.0f, 0.0f, 0.0f };           // [ebp-14h] the spawn point
    Vec3 speed = { 0.0f, 0.0f, 0.0f };              // [ebp-24h] the spawn velocity
    IObject* object = 0;                            // [ebp+0Ch] quirk 5: survives across iterations
    Mat4 transform = Mat4::getIdentity();           // [ebp-84h] 0x440B20
    {
        // quirk 9: dead as shipped - both results are overwritten before any read.
        int index = 0;
        float fraction = 0.0f;
        intensityCurvePosition(m_age / m_timeToLive, index, fraction);
        (void)index;
        (void)fraction;
    }
    transform = getVisualAbsoluteTransformation();                     // +0x8C
    const Mat4& visual = getVisualAbsoluteTransformation();            // fetched a second time
    const Vec3 currentPosition = { visual.m[12], visual.m[13], visual.m[14] };   // [ebp-44h]
    const Vec3 travel = { currentPosition.x - m_lastPosition.x,
                          currentPosition.y - m_lastPosition.y,
                          currentPosition.z - m_lastPosition.z };
    float emitOffset = 0.0f;                        // [ebp-28h] seconds into this frame
    const float inverseDeltaTime = 1.0f / deltaTime;  // quirk 6: unguarded
    if (m_age >= m_nextEmitTime)
    {
        // The alternate-template picker: a uniform [0, 1] RandomDistribution built once (the constructor's
        // seed word included) - but every draw goes to the global generator, not the local's own.
        RandomDistribution selector;
        selector.distribution = CRD_UNIFORM;
        selector.value = 0.0f;
        selector.variation = 1.0f;
        do
        {
            // Velocity from the emitter's axes - row 2 dof, row 1 up, row 0 right - drawn in that order.
            // quirk 4: `transform` carries the previous particles' roll (rows 0/1).
            const float speedDof = sampleDistribution(t->m_positionalSpeed[0], Rand);   // +0x50C
            speed.x = transform.m[8] * speedDof;
            speed.y = transform.m[9] * speedDof;
            speed.z = transform.m[10] * speedDof;
            const float speedUp = sampleDistribution(t->m_positionalSpeed[1], Rand);    // +0x520
            speed.x = transform.m[4] * speedUp + speed.x;
            speed.y = transform.m[5] * speedUp + speed.y;
            speed.z = transform.m[6] * speedUp + speed.z;
            const float speedRight = sampleDistribution(t->m_positionalSpeed[2], Rand); // +0x534
            speed.x = transform.m[0] * speedRight + speed.x;
            speed.y = transform.m[1] * speedRight + speed.y;
            speed.z = transform.m[2] * speedRight + speed.z;

            // Spawn point: this emission's share of the frame's travel (0x647CC0, a + b * s), then the
            // relative offsets along the same axes, then the distance its own velocity covers.
            const float along = emitOffset * inverseDeltaTime;
            position.x = travel.x * along + m_lastPosition.x;
            position.y = travel.y * along + m_lastPosition.y;
            position.z = travel.z * along + m_lastPosition.z;
            const float relativeDof = sampleDistribution(t->m_relativePosition[0], Rand);    // +0x4D0
            position.x = transform.m[8] * relativeDof + position.x;
            position.y = transform.m[9] * relativeDof + position.y;
            position.z = transform.m[10] * relativeDof + position.z;
            const float relativeUp = sampleDistribution(t->m_relativePosition[1], Rand);     // +0x4E4
            position.x = transform.m[4] * relativeUp + position.x;
            position.y = transform.m[5] * relativeUp + position.y;
            position.z = transform.m[6] * relativeUp + position.z;
            const float relativeRight = sampleDistribution(t->m_relativePosition[2], Rand);  // +0x4F8
            position.x = transform.m[0] * relativeRight + position.x;
            position.y = transform.m[1] * relativeRight + position.y;
            position.z = transform.m[2] * relativeRight + position.z;
            position.x = speed.x * emitOffset + position.x;
            position.y = speed.y * emitOffset + position.y;
            position.z = speed.z * emitOffset + position.z;

            // Roll: zRotation snapped down (truncated) to zRotationSnap steps, plus startRotation.
            // Draw order as shipped: zRotation (+0x598) before startRotation (+0x584).
            const float zRotation = sampleDistribution(t->m_zRotation, Rand);
            const float snapped = static_cast<float>(static_cast<int>(zRotation / t->m_zRotationSnap))
                                * t->m_zRotationSnap;
            const float angle = sampleDistribution(t->m_startRotation, Rand) + snapped;
            if (!t->m_addChild)                                                              // +0x5B2
            {
                if (angle != 0.0f)          // `fucompp; test ah, 44h; jnp` - NaN rotates (0x5A2800 then skips it)
                    rotateAboutLine(transform, angle, *reinterpret_cast<const Vec3*>(&transform.m[8]));
                if (t->m_moveToWaterSurface)                                                 // +0x5B5
                {
                    transform.m[4] = 0.0f;
                    transform.m[5] = 1.0f;
                    transform.m[6] = 0.0f;
                    // result ignored.
                    reinterpret_cast<Vec3*>(&transform.m[0])->makeOrthonormalBasis(
                        *reinterpret_cast<Vec3*>(&transform.m[4]), *reinterpret_cast<Vec3*>(&transform.m[8]));
                    // The emitter's position, not the spawn point's; 0.2f is ds:0x8E5D84. PE +0x94 is the
                    // (const Vec3&) overload - MSVC reverses the pair against the ELF's +0x98/+0x9C.
                    position.y = static_cast<IHeightmapCluster*>(heightmapCluster)
                                     ->getWaterLevelInWorldCoords(getAbsolutePosition()) + 0.2f;
                }
            }
            transform.m[12] = position.x;
            transform.m[13] = position.y;
            transform.m[14] = position.z;

            // Which template: uniform over the main one and the alternates. floor via CRT floor
            // (0x402EC0), then truncated; an index at or past the count (signed `jge`) is the main one.
            if (t->m_numAlternateTemplates)                                                  // +0x5B8
            {
                const float draw = sampleDistribution(selector, Rand)
                                 * static_cast<float>(t->m_numAlternateTemplates + 1);
                const int choice = static_cast<int>(std::floor(draw));
                if (choice < t->m_numAlternateTemplates)
                {
                    if (t->m_alternateTemplateCache[choice])                                 // +0x4B8
                        object = t->m_alternateTemplateCache[choice]->createObject();
                    // quirk 5: a null slot leaves `object` as the previous iteration's.
                }
                else
                    object = m_particleTemplate->createObject();
            }
            else
                object = m_particleTemplate->createObject();

            if (object)
            {
                object->setAbsoluteTransformation(transform);                                // PE +0x88
                if (object->getTemplate()->getClassID() == CID_ParticleTemplate)              // ds:0x91C1FC
                    presetParticleGeometryPart(static_cast<Particle*>(object), t);
                object->init();                                                              // PE +0x1C
                object->updateGeometry();                                                    // PE +0xB4
                if (void* const geometry = L::geometry(object))                              // +0x38
                {
                    // The parent bundle's FOV override (EffectBundle +0x1AC - m_fovOverride, which
                    // EffectBundle::getIsFirstPerson writes at 0x641637), read afresh through the parent.
                    bool fovOverride = false;
                    if (getParent() && getParent()->getTemplate()->getClassID() == CID_EffectBundleTemplate)
                        fovOverride = static_cast<EffectBundle*>(getParent())->m_fovOverride;
                    geometrySetByteFlag(geometry, 0xD4, 0xD8, firstPerson);
                    geometrySetByteFlag(geometry, 0xDC, 0xE0, fovOverride);
                }
                if (t->m_addChild)
                {
                    addChild(object);                                                        // PE +0xF0
                    // quirk 8 - Debug type 6 aborts in this tree; unreachable (see the header).
                    if (!object->getParent())
                        Debug(DebugTypeError, "Game/Objects/Fx/Emitter.cpp", 446, "")
                            << "Emitter add child on non composite object";
                    Mat4 relative = object->getRelativeTransformation();                     // PE +0xE4, copied
                    rotateAboutLine(relative, angle, *reinterpret_cast<const Vec3*>(&relative.m[8]));
                    object->setRelativeTransformation(relative);                             // PE +0xE8
                }
                if (object->getTemplate()->getClassID() == CID_ParticleTemplate)              // asked again
                    static_cast<Particle*>(object)->setEmitter(this);
                IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(object));   // +0x44
                if (!t->m_noPhysics && node)                                                 // +0x5B1
                {
                    const Vec3 noAcceleration = { 0.0f, 0.0f, 0.0f };
                    node->setPositionalAcceleration(noAcceleration);                         // PE +0x74
                    const Vec3 noRotationalAcceleration = { 0.0f, 0.0f, 0.0f };
                    node->setRotationalAcceleration(noRotationalAcceleration);               // PE +0x78
                    // quirk 9: 0x5B4510 (Vec3 -=) on the spawn point - never read again.
                    position.x -= currentPosition.x;
                    position.y -= currentPosition.y;
                    position.z -= currentPosition.z;
                    if (t->m_addEmitterSpeed && rootPhysics)                                 // +0x211
                    {
                        const float scale = t->m_emitterSpeedScale;                          // +0x214
                        const Vec3& tangent = rootPhysics->getTangentSpeed(node->getAbsolutePosition());
                        speed.x = tangent.x * scale + speed.x;
                        speed.y = tangent.y * scale + speed.y;
                        speed.z = tangent.z * scale + speed.z;
                        node->setPositionalAcceleration(rootPhysics->getPositionalAcceleration());
                    }
                    node->setPositionalSpeed(speed);                                         // PE +0x64
                    // [UNRESOLVED] m_flags bit 25 (0x02000000, `test byte [obj+7], 2`) - the spawned
                    // object opts out of the spin. No writer found in Emitter or Particle bodies.
                    if (!(object->getFlags() & 0x02000000u))
                    {
                        // Draw order as shipped: right (+0x570), UP (+0x55C), DOF (+0x548) - the source's
                        // Vec3(dof, up, right) arguments evaluated right to left by MSVC.
                        const float spinRight = sampleDistribution(t->m_rotationalSpeed[2], Rand);
                        const float spinUp = sampleDistribution(t->m_rotationalSpeed[1], Rand);
                        const float spinDof = sampleDistribution(t->m_rotationalSpeed[0], Rand);
                        Vec3 spin = { spinDof, spinUp, spinRight };
                        if (t->m_addEmitterSpeed && rootPhysics)
                        {
                            const float scale = t->m_emitterSpeedScale;
                            const Vec3& rootSpin = rootPhysics->getRotationalSpeed();
                            spin.x = rootSpin.x * scale + spin.x;
                            spin.y = rootSpin.y * scale + spin.y;
                            spin.z = rootSpin.z * scale + spin.z;
                            node->setRotationalAcceleration(rootPhysics->getRotationalAcceleration());
                        }
                        node->setRotationalSpeed(spin);                                      // PE +0x68
                    }
                }
                // Anything that is not a particle - another Emitter, a bundle, a light - is started.
                const unsigned int classId = object->getTemplate()->getClassID();          // third time
                if (classId != CID_SpriteParticleTemplate && classId != CID_ParticleTemplate)
                    object->handleMessage(static_cast<TemplateMessage>(0), 0, false);
            }

            // The next emission, due or not a particle was made.
            const float interval = calcInvItensity(t, emitterSpeed(this, rootPhysics));
            emitOffset = emitOffset + interval;
            m_nextEmitTime = interval + m_nextEmitTime;
        }
        while (m_age >= m_nextEmitTime);
    }
    m_lastPosition = currentPosition;
#else
    (void)deltaTime;
    (void)flags;
#endif
}

// handleMessage - ELF 0x81FE9F0 (2,412 B: every draw inlined) / BF2.exe 0x648970 (709 B), both read in
// full. Only 0 (start) and 1 (stop); every other message is ignored and nothing is forwarded.
void Emitter::handleMessage(TemplateMessage message, IPlayer* /*player*/, bool /*recursive*/)
{
    const EmitterTemplate* const t = static_cast<const EmitterTemplate*>(getTemplate());
    if (static_cast<int>(message) == 0)
    {
        // startProbability below 1 rolls a local uniform [0, 1] distribution against the global
        // generator (PE 0x648A29 / ELF 0x81FF300); a roll at or above the probability refuses the start.
        if (t->m_startProbability < 1.0f)
        {
            RandomDistribution roll;
            roll.distribution = CRD_UNIFORM;
            roll.value = 0.0f;
            roll.variation = 1.0f;
            if (sampleDistribution(roll, Rand) >= t->m_startProbability)
                return;
        }
        // A fresh start (the age is exactly 0 - after a stop, or never started).
        if (m_age == 0.0f)
        {
            if (getParent() && getParent()->getTemplate()->getClassID() == CID_EffectBundleTemplate)
                static_cast<EffectBundle*>(getParent())->addNumEmittersPlaying();
            if (!m_particleTemplate)
                m_particleTemplate = objectTemplateManager->getTemplate(t->m_template);      // PE +0x410
            // quirk 2: the object fills its template's cache, compacted over failed lookups, and looks
            // up empty names as well (answer 0). Once slot 0 is filled no emitter looks again.
            if (t->m_numAlternateTemplates > 0 && !t->m_alternateTemplateCache[0])
            {
                EmitterTemplate* const cache = const_cast<EmitterTemplate*>(t);
                int filled = 0;
                for (int i = 0; i < 5; ++i)
                    if (!cache->m_alternateTemplateCache[filled])
                    {
                        cache->m_alternateTemplateCache[filled] =
                            objectTemplateManager->getTemplate(t->m_alternateTemplates[i]);
                        if (cache->m_alternateTemplateCache[filled])
                            ++filled;
                    }
            }
            if (m_particleTemplate)
                updateFlags(0x3800, 1);     // updates + visual + frame updates on, the disabled bit off
        }
        m_delay = sampleDistribution(t->m_delay, Rand);
        m_timeToLive = sampleDistribution(t->m_timeToLive, Rand);
        m_startTime = static_cast<float>(g_system->getExactTime());
        m_age = 0.0f;
        m_nextEmitTime = 0.0f;
        m_lastPosition = getAbsolutePosition();
#if !DEDICATED
        // BF2.exe 0x648B98..0x648C26 - the client seeds the visibility frame from the local camera so a
        // fresh emitter in LOD range emits before its first visual update: strictly inside m_lodDistance
        // (`fcompp; test ah, 41h; jne` - equal and NaN clear it) -> getCurrentFrame() (inlined, +100),
        // otherwise 0. The distance is 0x4DC200: (dx*dx + dz*dz) + dy*dy, emitter minus camera.
        // quirk 10: getLocalHumanPlayer three times, getCamera twice (PlayerManager +0x30, IPlayer PE
        // +0x17C ICameraObject +0x10 getTransformation(0.0f)).
        if (playerManager->getLocalHumanPlayer() &&
            playerManager->getLocalHumanPlayer()->getCamera())
        {
            const Mat4& camera = playerManager->getLocalHumanPlayer()->getCamera()->getTransformation(0.0f);
            const float lodDistance = m_lodDistance;
            const float dx = m_lastPosition.x - camera.m[12];
            const float dy = m_lastPosition.y - camera.m[13];
            const float dz = m_lastPosition.z - camera.m[14];
            if (dx * dx + dz * dz + dy * dy < lodDistance * lodDistance)
                m_visibleFrame = getCurrentFrame();
            else
                m_visibleFrame = 0;
        }
#endif
    }
    else if (static_cast<int>(message) == 1)
    {
        updateFlags(1, 0x3800);
        m_hasFrameUpdate = false;
        m_age = 0.0f;
        // quirk 1: no null test on the parent here (the start arm has one).
        if (getParent()->getTemplate()->getClassID() == CID_EffectBundleTemplate)
            static_cast<EffectBundle*>(getParent())->decNumEmittersPlaying();
        if (t->m_looping)
        {
            m_timeToLive = sampleDistribution(t->m_timeToLive, Rand);
            m_nextEmitTime = 0.0f;
            return;
        }
        if (!getParent())
            destroy(true);                  // ELF +0x34 / PE +0x30
    }
}

// playParticleCollisionSound - ELF 0x81FF360 (106 B) / BF2.exe 0x647B50 (60 B), read in full. The
// material cell's collision sound (MaterialManager +0x68 getSound(a, b, 0, true) - the same call and
// the same two sound calls GameLogicCollision::doImpact and ResponsePhysics make): setSoundFlags(1)
// (+0x08 in both images) then playAt(position, 1) - ELF +0x1C / PE +0x18, the MSVC overload reversal,
// so it is called typed. world::Particle::handleCollision is the caller.
void Emitter::playParticleCollisionSound(const Vec3& position, int material, int otherMaterial)
{
    ISound* const sound = static_cast<ISound*>(static_cast<MaterialManager*>(materialManager)->getSound(
        static_cast<unsigned int>(material), static_cast<unsigned int>(otherMaterial), 0, true));
    if (sound)
    {
        sound->setSoundFlags(1);
        sound->playAt(position, 1);
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
