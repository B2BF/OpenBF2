// OpenAL provider discovery and initialization.

// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "OpenALProvider.h"
#include "SoundEngineMath.h"
#include "OpenALSample.h"
#include "Sound.h"
#include "SoundFile.h"
#include "BfSoundEngine.h"
#include "SoundTrace.h"

namespace dice {
namespace hfe {

namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\OpenALProvider.cpp";
}

// + the inlined constructor: the engine's factory. Returns null when the 128-byte
// allocation fails (the engine then stores a null provider and nothing can ever open).
OpenALProvider* OpenALProvider::create()
{
    return new OpenALProvider;
}

OpenALProvider::OpenALProvider()
    : m_device(0), m_context(0), m_dopplerFactor(1.0f), m_reverbEnabled(true),
      m_maxVoices(0), m_eaxVersion(0)
{
    addProviders();
}

// Does not close the device: the engine's shutdown does that first.
OpenALProvider::~OpenALProvider()
{
}

// The seven rows, in the order the client's AudioSettings indexes them
// (provider code 0..6 = this order).
void OpenALProvider::addProviders()
{
    ProviderInfo p;
    p.deviceName = "DirectSound";    p.displayName = "Software Audio + EAX";  p.systemOpenAL = false; p.wantEax = true;  p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "DirectSound3D";  p.displayName = "Hardware Audio";        p.systemOpenAL = false; p.wantEax = false; p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "DirectSound3D";  p.displayName = "Hardware Audio + EAX";  p.systemOpenAL = false; p.wantEax = true;  p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "Default Device"; p.displayName = "System OpenAL";         p.systemOpenAL = true;  p.wantEax = false; p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "Default Device"; p.displayName = "System OpenAL + EAX";   p.systemOpenAL = true;  p.wantEax = true;  p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "MMSYSTEM";       p.displayName = "Safe Mode";             p.systemOpenAL = false; p.wantEax = false; p.useSourcePool = false; p.use2dOcclusion = false; m_providers.push_back(p);
    p.deviceName = "DirectSound";    p.displayName = "Software Audio";        p.systemOpenAL = false; p.wantEax = false; p.useSourcePool = true;  p.use2dOcclusion = false; m_providers.push_back(p);
}

// The SoundFile's type picks the sample class: 1 = static (a .wav decoded into
// AL buffers), 2 = streaming (an .ogg kept in memory). Any other type - an unloaded file has 0 -
// answers a null sample. `pos` is the play*At offset, possibly null.
Sample** OpenALProvider::createSample(Sample** out, SoundTemplate* t, int index, const Vec3* pos)
{
    Sample* sample = 0;
    switch (t->getSoundFile(index)->m_type - 1)
    {
    case 0:  sample = new StaticSample(pos);    break;
    case 1:  sample = new StreamingSample(pos); break;
    default: break;
    }
    *out = sample;
    return out;
}

// Appends every displayName's c_str() and returns the row count.
int OpenALProvider::getProviderNames(std::vector<const char*>* out)
{
    for (unsigned int i = 0; i < m_providers.size(); ++i)
        out->push_back(m_providers[i].displayName.c_str());
    return (int) m_providers.size();
}

// The device/context bring-up. Idempotent on the name; anything else closes
// first. Every failure path closes again before answering its code.
int OpenALProvider::open(const std::string& name)
{
    if (m_device)
    {
        if (m_deviceName == name)
            return 0;
        close();
    }
    unsigned int i = 0;
    for (;; ++i)
    {
        if (i >= m_providers.size())
            return 1;
        if (!(m_providers[i].displayName != name))
            break;
    }
    const ProviderInfo& p = m_providers[i];

    int flags;
    if (p.systemOpenAL)
    {
        if (!loadOpenAL("OpenAL32.dll"))
        {
            g_soundTrace->warning(sndFormat("OpenAL error: Could not find OpenAL32.dll"), kFile, 90);
            close();
            return 2;
        }
        m_device = alc_OpenDevice(0);
        flags = getOpenALFlags() | 1;
    }
    else
    {
        if (!loadOpenAL("BF2OpenAL.dll"))
        {
            g_soundTrace->warning(sndFormat("OpenAL error: Could not find %s.", "BF2OpenAL.dll"), kFile, 102);
            close();
            return 2;
        }
#if defined(__linux__) && defined(BF2_LINUX_CLIENT)
        // [CHOSEN, not recovered] Keep the saved provider indices, but let OpenAL Soft
        // select the native default output instead of a Windows driver name (issue #36).
        m_device = alc_OpenDevice(0);
#else
        m_device = alc_OpenDevice(p.deviceName.c_str());
#endif
        flags = getOpenALFlags() & ~1;
    }
    setOpenALFlags(flags);
    if (!m_device)
    {
        g_soundTrace->warning(sndFormat("OpenAL error: Failed to open device %s", name.c_str()), kFile, 111);
        close();
        return 4;
    }
    ALint attributes[4] = { ALC_FREQUENCY, 44100, 0, 0 };
    m_context = alc_CreateContext(m_device, attributes);
    if (alc_GetError(m_device))
    {
        close();
        return 4;
    }
    m_deviceName = name;
    alc_MakeContextCurrent(m_context);
    if (alc_GetError(m_device))
    {
        close();
        return 4;
    }
    // A system OpenAL is only accepted when the "allow-any-system-openal" property says so or it
    // exposes EAX 5.0 (i.e. it is Creative's own).
    if ((getOpenALFlags() & 1) && g_soundProperties->get(SoundProperties::AllowAnySystemOpenAL) < 0.5f && !detectEax5())
    {
        g_soundTrace->warning(sndFormat("OpenAL error: Could not use system OpenAL, since it does not support EAX5.0"), kFile, 144);
        close();
        return 3;
    }
    if (!countVoices())
    {
        close();
        return 4;
    }
    if (p.wantEax)
        detectEax();
    al_GetError();                                  // clear whatever the probes left
    setDopplerFactor(m_dopplerFactor);
    al_DistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    ALfloat orientation[6] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
    al_Listenerfv(AL_ORIENTATION, orientation);
    const char* specifier = alc_GetString(m_device, ALC_DEVICE_SPECIFIER);
    g_soundTrace->info(sndFormat("Sound 3D provider: \"%s\" (%s)", specifier, p.displayName.c_str()), kFile, 177);
    g_soundTrace->info(sndFormat("  EAX version: %d", getEaxVersion()), kFile, 178);
    g_soundTrace->info(sndFormat("  Quality Level: %d", g_soundEngineInstance->getQualityLevel()), kFile, 179);
    m_info = p.displayName + " (" + std::string(specifier) + ")";
    g_soundProperties->set(SoundProperties::UseSourcePool,  p.useSourcePool  ? 1.0f : 0.0f);
    g_soundProperties->set(SoundProperties::Use2dOcclusion, p.use2dOcclusion ? 1.0f : 0.0f);
    if (!checkOpenALError("open device"))
    {
        close();
        return 4;
    }
    return 0;
}

// Purges the two pools first - they hold OpenAL objects of the context that is
// about to go - then the context, the device, the name, and finally the library.
void OpenALProvider::close()
{
    purgeStreamingBufferPool();
    purgeSourcePool();
    eax_Set = 0;
    eax_Get = 0;
    g_eaxAvailable = false;
    if (m_context)
        alc_DestroyContext(m_context);
    m_context = 0;
    if (m_device)
        alc_CloseDevice(m_device);
    m_device = 0;
    m_maxVoices = 0;
    m_eaxVersion = 0;
    m_deviceName = "";
    unloadOpenAL();
}

// Clamps listener speed to 300 m/s and negates z for OpenAL's coordinate frame.
void OpenALProvider::setListenerVelocity(const Vec3* v)
{
    Vec3 vel = *v;
    const float lengthSq = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
    if (lengthSq > 90000.0f)
    {
        sndmath::normalizeIfNeeded(vel);
        vel.x *= 300.0f; vel.y *= 300.0f; vel.z *= 300.0f;
    }
    al_Listener3f(AL_VELOCITY, vel.x, vel.y, -vel.z);
}

void OpenALProvider::setMasterVolume(float gain)
{
    al_Listenerf(AL_GAIN, gain);
}

// One EAXSet failure disables reverb for the rest of the session.
void OpenALProvider::updateReverb(const EAXLISTENERPROPERTIES* p)
{
    if (g_eaxAvailable && m_reverbEnabled)
    {
        if (eax_Set(&g_eaxListenerGuid, EAX_ALLPARAMETERS, 0, (ALvoid*) p, sizeof(EAXLISTENERPROPERTIES)))
        {
            checkOpenALError("updateReverb");
            g_soundTrace->error(sndFormat("Failed to update reverb. Disabling reverb from now on."), kFile, 245);
            m_reverbEnabled = false;
        }
    }
}

// Stored always; sent only while a device is open.
void OpenALProvider::setDopplerFactor(float f)
{
    m_dopplerFactor = f;
    if (m_device)
        al_DopplerFactor(f);
}

void OpenALProvider::detectEax()
{
    const char* extension = "EAX3.0";
    g_eaxAvailable = false;
    g_eaxAvailable = al_IsExtensionPresent("EAX3.0") != 0;
    if (!g_eaxAvailable)
    {
        extension = "EAX3.0Emulated";
        g_eaxAvailable = al_IsExtensionPresent("EAX3.0Emulated") != 0;
    }
    if (g_eaxAvailable)
    {
        eax_Set = (PFN_EAXSet) al_GetProcAddress("EAXSet");
        eax_Get = (PFN_EAXGet) al_GetProcAddress("EAXGet");
        if (!eax_Set || !eax_Get)
        {
            g_eaxAvailable = false;
            m_eaxVersion = 0;
        }
        else
        {
            m_eaxVersion = 3;
        }
    }
    else
    {
        m_eaxVersion = 0;
    }
    g_soundTrace->info(sndFormat("%s available: %s", extension, g_eaxAvailable ? "Yes" : "No"), kFile, 409);
}

// Generate sources until OpenAL refuses or 128 are held, log the count, delete
// them all again. That count becomes the provider's voice limit. Always answers true.
bool OpenALProvider::countVoices()
{
    std::vector<ALuint> sources;
    sources.reserve(128);
    al_GetError();
    ALuint source = 0;
    al_GenSources(1, &source);
    if (source)
    {
        while (!al_GetError())
        {
            sources.push_back(source);
            if (sources.size() == 128)
                break;
            source = 0;
            al_GenSources(1, &source);
            if (!source)
                break;
        }
    }
    g_soundTrace->info(sndFormat("Max number of voices: %u", (unsigned int) sources.size()), kFile, 440);
    al_DeleteSources((ALsizei) sources.size(), sources.empty() ? 0 : &sources[0]);
    m_maxVoices = (int) sources.size();
    al_GetError();
    return true;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
