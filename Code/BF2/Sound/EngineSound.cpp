// Engine RPM, load, and sway sound updates.

#include "EngineSound.h"

#include <cmath>
#include <string>

#include "NullSound.h"                                   // ISound / ISoundTemplate
#include "SoundObjectTemplate.h"                         // initTemplates' addSoundTemplate product
#include "../Game/Objects/EngineTemplate.h"              // createSoundFromTemplate, +0x670 sound template
#include "../Game/Objects/IObject.h"                     // getAbsoluteTransformation
#include "../Game/Objects/Game/EngineBase.h"             // the engine is the sounds' owner object
#include "../Game/Utility/Vars.h"
#include "../Core/Random.h"
#include "../Core/System.h"
#include "../Math/Math.h"

namespace dice {
namespace hfe {

namespace {
    // `anonymous namespace'::g_zero - what a null rpm source pointer maps to.
    const float g_zero = 0.0f;
    // Retain live Vars handles; rpm-k is registered even though this file does not read it.
    float* const g_engineSoundSmoothTime = Vars::getFloat("engine-sound-smooth-time", 0.1f);
    float* const g_rpmWheel = Vars::getFloat("rpm-wheel", 0.06f);
    float* const g_rpmK = Vars::getFloat("rpm-k", 10.0f);
}


// Requires m_sound before querying its template; see the interface contract.

float ScaleEvaluator::getValue(float value, int sample)
{
    ISoundTemplate* t = m_sound->getTemplate();                                       // +0x70 (before the test - as read)
    ISoundEvaluator* evaluator = static_cast<ISoundEvaluator*>(t->getVolumeEvaluator());   // +0x30
    if (m_sound && evaluator)
        return evaluator->getValue(value, sample) * m_scale;
    return m_scale;
}


// Creates the engine sounds, binds their volume/pitch evaluators, and parents them to this node.

EngineSound::EngineSound(const world::EngineTemplate* engineTemplate, world::EngineBase* engine,
                         world::IObject* root, const EngineSoundTemplate* soundTemplate,
                         const float* rpmSource)
    : m_template(soundTemplate)
{
    m_velocity.x = m_velocity.y = m_velocity.z = 0.0f;                                // +0x60
    m_engine = engine;                                                                // +0x6C
    m_root = root;                                                                    // +0x70
    m_load = 0.0f;                                                                    // +0x78
    resetAudioRpm();
    const float* source = rpmSource ? rpmSource : &g_zero;
    m_velocity.x = m_velocity.y = m_velocity.z = 0.0f;                                // (again, as read)
    m_rpmSource = source;                                                             // +0x18
    m_values[1] = m_values[2] = m_values[0] = 0.0f;                                   // +0x24/+0x28/+0x20
    m_values[3] = 0.0f;                                                               // [CHOSEN] - the ELF leaves +0x2C uninitialised
    m_rpm = *source;                                                                  // +0x1C
    m_newStyleAudio = engineTemplate->m_engineSound.m_byte00;                         // +0x74 <- template +0x670
    m_sounds[0] = static_cast<ISound*>(engineTemplate->createSoundFromTemplate(std::string("Idle"), engine));
    m_sounds[1] = static_cast<ISound*>(engineTemplate->createSoundFromTemplate(std::string("Rpm1"), engine));
    m_sounds[2] = static_cast<ISound*>(engineTemplate->createSoundFromTemplate(std::string("Rpm2"), engine));
    m_sounds[3] = static_cast<ISound*>(engineTemplate->createSoundFromTemplate(std::string("Load"), engine));
    if (m_sounds[1])
        m_sounds[1]->getTemplate()->setStopType(0);                                   // +0x70 +0x70
    if (m_sounds[2])
        m_sounds[2]->getTemplate()->setStopType(0);
    for (int i = 0; i <= 3; ++i)
    {
        if (!m_sounds[i])
            continue;
        if (i != 3)
        {
            m_evaluators[i].m_sound = m_sounds[i];
            m_evaluators[i].m_scale = 1.0f;
            m_sounds[i]->setVolumeEvaluator(&m_evaluators[i], &m_values[i]);          // +0x44
            m_sounds[i]->setPitchEvaluator(0, &m_values[i]);                          // +0x4C
        }
        m_sounds[i]->setParent(this);                                                 // +0x3C
    }
}

// release the four sounds.
EngineSound::~EngineSound()
{
    for (int i = 0; i <= 3; ++i)
        if (m_sounds[i])
            m_sounds[i]->release();                                                   // +0x04
}

const Mat4& EngineSound::getTransform()          { return m_root->getAbsoluteTransformation(); }   // IObject +0x84
const Math::Vec3* EngineSound::getVelocity()     { return &m_velocity; }

// +0x78..+0x94 zeroed.
void EngineSound::resetAudioRpm()
{
    m_load = 0.0f;
    m_audioRpm = 0.0f;
    m_noiseCountdown = 0;
    m_noiseRpm = 0.0f;
    m_float88 = 0.0f;
    m_swayTime = 0.0f;
    m_lastGear = 0;
    m_audioVelocity = 0.0f;
}


// Starts the idle/RPM sounds when stopped, or stops active sounds using forceStop when requested.
// Starting resets audio RPM and listener velocity; notLocal controls the idle sound's locality flag.

void EngineSound::setIsRunning(bool running, bool forceStop, bool notLocal)
{
    static const float zero = 0.0f;                                                   // setIsRunning(bool,bool,bool)::zero
    if (running)
    {
        if (m_sounds[1] && m_sounds[1]->isPlaying())                                  // +0x38
            return;
        if (m_sounds[2] && m_sounds[2]->isPlaying())
            return;
        if (m_sounds[0] && m_sounds[0]->isPlaying())
            m_sounds[0]->forceStop();                                                 // +0x34
        resetAudioRpm();
        if (m_sounds[0])
        {
            const unsigned int flags = m_sounds[0]->getSoundFlags();                  // +0x0C
            m_sounds[0]->setSoundFlags(notLocal ? (flags & ~0x200u) : (flags | 0x200u));   // +0x08
            m_sounds[0]->setVolumeEvaluator(&m_evaluators[0], &m_values[0]);          // +0x44
            m_sounds[0]->setPitchEvaluator(0, &m_values[0]);                          // +0x4C
        }
        for (int i = 0; i <= 3; ++i)
        {
            m_velocity.x = m_velocity.y = m_velocity.z = 0.0f;
            if (m_sounds[i])
                m_sounds[i]->play();                                                  // +0x10
        }
    }
    else
    {
        if (m_sounds[0])
        {
            m_sounds[0]->setVolumeEvaluator(&m_evaluators[0], &zero);
            m_sounds[0]->setPitchEvaluator(0, &zero);
        }
        for (int i = 0; i <= 3; ++i)
        {
            if (m_sounds[i] && m_sounds[i]->isPlaying())
            {
                if (forceStop) m_sounds[i]->forceStop();                              // +0x34
                else           m_sounds[i]->stop();                                   // +0x30
            }
        }
    }
}

void EngineSound::setVelocity(const Vec3& velocity)      { m_velocity = velocity; }


// Updates load/RPM evaluators and returns their shared input, snapping values below 1e-4 to zero.
// New-style load changes by at most 0.2 per call; old-style audio uses absolute RPM.

float EngineSound::update(bool engineOn, float input, int material)
{
    float value;
    if (m_newStyleAudio)
    {
        const float target = engineOn ? input : 0.0f;
        float delta = target - m_load;
        if (delta < -0.2f)      delta = -0.2f;
        else if (delta > 0.2f)  delta = 0.2f;
        m_load += delta;
        float rpm1 = 1.0f - m_load;
        if (rpm1 < 0.0f) rpm1 = 0.0f; else if (rpm1 > 1.0f) rpm1 = 1.0f;
        m_evaluators[1].m_scale = rpm1;                                               // +0x44
        float rpm2 = m_load;
        if (rpm2 < 0.0f) rpm2 = 0.0f; else if (rpm2 > 1.0f) rpm2 = 1.0f;
        m_evaluators[2].m_scale = rpm2;                                               // +0x50
        value = updateAudioRpm(engineOn, input, material) / 1.1f;
        if (value < 0.0f) value = 0.0f; else if (value > 1.0f) value = 1.0f;
    }
    else
    {
        m_rpm = *m_rpmSource;
        value = std::fabs(m_rpm / 1.2f);
    }
    if (std::fabs(value) < 0.000099999997f)
        value = 0.0f;
    m_values[1] = m_values[2] = m_values[0] = value;
    return value;
}

// both read end to end. Noise uses the shared engine RNG,
// exactly two draws per refresh; the material bias is applied only on that refresh.
float EngineSound::updateAudioRpm(bool engineOn, float input, int material)
{
    const float rpm = std::fabs(*m_rpmSource);
    if (!engineOn || input == 0.0f)
        m_noiseRpm = 0.0f;
    else if (m_noiseCountdown <= 0)
    {
        const float amount = (0.1f * m_template->m_materialTweak)
                           * (6.5f / (rpm * 5.0f + 1.0f)) * *g_rpmWheel;
        const float speedSquared = m_velocity.x * m_velocity.x
                                 + m_velocity.y * m_velocity.y
                                 + m_velocity.z * m_velocity.z;
        const float low = amount * -0.30000001f;
        const float noiseDraw = static_cast<float>(((Rand.getRaw() >> 7) | 1u) + 1u);
        m_noiseRpm = (amount - low) * noiseDraw * 0.000000059604645f + low;
        const float delayDraw = static_cast<float>(((Rand.getRaw() >> 7) | 1u) + 1u);
        m_noiseCountdown = static_cast<int>((std::sqrt(speedSquared) * 0.033333335f
                                           * 23.0f + 2.0f)
                                          * delayDraw * 0.000000059604645f);
        if (material < 0 && material != -2)
            m_noiseRpm += 0.30000001f;
    }
    --m_noiseCountdown;
    const float sway = updateSwayEffect(engineOn);
    const float target = sway + m_noiseRpm + rpm;
    if (std::fabs(target - m_audioRpm) < 0.001f)
    {
        m_audioRpm = target;
        m_audioVelocity = 0.0f;
    }
    else
        m_audioRpm = smooth_in_out(m_audioRpm, target, &m_audioVelocity,
                                  *g_engineSoundSmoothTime, 0.033333335f);
    return m_audioRpm;
}

// Uses (gear - 1) / 5 for duration, but unscaled gear for amplitude and frequency.
// Downshifts halve amplitude even with the engine off. Requires engine and System.
float EngineSound::updateSwayEffect(bool engineOn)
{
    const int gear = m_engine->m_gear > 0 ? m_engine->m_gear : 1;
    const int oldGear = m_lastGear;
    if (gear != oldGear)
    {
        const float factor = static_cast<float>(gear - 1) * 0.2f;
        m_lastGear = gear;
        m_swayTime = m_template->m_swayTimeFirst
                   + factor * (m_template->m_swayTimeLast - m_template->m_swayTimeFirst);
        playGearShift();
    }
    else
        m_swayTime -= 0.033333335f;

    if (m_swayTime >= 0.0f)
    {
        const float gearValue = static_cast<float>(gear);
        float amplitude = (m_template->m_swayAmplitudeFirst
                           + (m_template->m_swayAmplitudeLast - m_template->m_swayAmplitudeFirst)
                           * gearValue) * m_swayTime;
        if (oldGear > gear)
            amplitude *= 0.5f;
        else
            amplitude *= engineOn ? 1.0f : 0.0f;
        const float time = static_cast<float>(g_system->getWallClockTime());
        const float frequency = m_template->m_swayFrequencyFirst
                              + (m_template->m_swayFrequencyLast - m_template->m_swayFrequencyFirst)
                              * gearValue;
        // [PLATFORM] PE uses double _CIfmod and x87 fsin; ELF calls fmodf/sinf.
        // Keep the PE's double math for the Windows native port; the period itself is float-valued.
#if defined(_WIN32)
        const double phase = std::fmod(static_cast<double>(time), static_cast<double>(6.2831855f))
                           * static_cast<double>(frequency) * static_cast<double>(0.85000002f);
        return static_cast<float>(std::sin(phase) * amplitude);
#else
        const float phase = std::fmod(time, 6.2831855f) * 0.85000002f * frequency;
        return std::sin(phase) * amplitude;
#endif
    }
    return 0.0f;
}

// the Load sound plays.
void EngineSound::playGearShift()
{
    if (m_sounds[3])
        m_sounds[3]->play();
}


// Creates and configures the engine's four named sound templates.

void EngineSound::initTemplates(world::EngineTemplate* t)
{
    struct Spec { const char* name; float minDistance; int loopCount; int stopType; };
    static const Spec s_specs[4] =
    {
        { "Idle", 3.0f, 0, 2 },
        { "Rpm1", 3.0f, 0, 0 },
        { "Rpm2", 3.0f, 0, 0 },
        { "Load", 1.0f, 1, 1 },
    };
    for (int i = 0; i < 4; ++i)
    {
        SoundObjectTemplate* s =
            static_cast<SoundObjectTemplate*>(t->addSoundTemplate(s_specs[i].name));
        s->impl()->setIs3dSound(true);                        // +0x68
        s->impl()->setMinDistance(s_specs[i].minDistance);     // +0x58
        s->impl()->setLoopCount(s_specs[i].loopCount);         // +0x18
        s->impl()->setStopType(s_specs[i].stopType);           // +0x70
    }
}

} // namespace hfe
} // namespace dice
