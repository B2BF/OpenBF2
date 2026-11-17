// [INFERRED] path. Full client instructions read, including the 4B1750 wrapper
// and 4B1420 record helper. No ELF implementation survives.
#include "AudioSettings.h"
#if !DEDICATED
#include "IGame.h"                    // game - in-game applies stop after the volumes
#include "../Menu/ISwiffHost.h"       // postMessage - reportError's {3, message, 3} record
#include "../Core/Ids.h"
#include "../Core/SmartItf.h"         // classManager
#include "../Sound/BfSound.h"
#if defined(_WIN32)
#include <windows.h>
#else
#if defined(__APPLE__)
#include <sys/sysctl.h>               // [CHOSEN, not recovered] hw.memsize for the GlobalMemoryStatus arm
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif
#include <cstdint>
#endif
#include <cstdio>
#include <cstdlib>
#include <new>
#include <ostream>
#include <sstream>

namespace dice { namespace hfe {
namespace {
//: preserve the legacy 32-bit API, unsigned overflow before the shift,
// rounding to the next four megabytes, and the actual C++ exception fallback.
int physicalMemoryMegabytes()
{
    try {
#if defined(_WIN32)
        MEMORYSTATUS status;
        GlobalMemoryStatus(&status);
        int megabytes = static_cast<int>((status.dwTotalPhys + 0x7FFFFu) >> 20);
#else
        // [CHOSEN, not recovered] GlobalMemoryStatus for the large-address-aware 32-bit BF2.exe:
        // dwTotalPhys is a 32-bit DWORD saturated at 0xFFFFFFFF above 4 GB. The same DWORD
        // arithmetic follows, so a machine with 4 GB or more wraps to 0 MB and selects "Low", as
        // the retail client does on such a machine.
        std::uint64_t physical = 0;
#if defined(__APPLE__)
        std::size_t length = sizeof(physical);
        if (sysctlbyname("hw.memsize", &physical, &length, 0, 0) != 0)
            physical = 0;
#elif defined(__linux__)
        // [CHOSEN, not recovered] Native Linux memory query (issue #36); the legacy
        // saturation, DWORD overflow and quality threshold below remain unchanged.
        struct sysinfo memory;
        if (::sysinfo(&memory) == 0)
            physical = static_cast<std::uint64_t>(memory.totalram) * memory.mem_unit;
#endif
        const std::uint32_t totalPhys = physical > 0xFFFFFFFFull ? 0xFFFFFFFFu
                                                                 : static_cast<std::uint32_t>(physical);
        int megabytes = static_cast<int>(static_cast<std::uint32_t>(totalPhys + 0x7FFFFu) >> 20);
#endif
        const int remainder = megabytes % 4;
        if (remainder) megabytes += 4 - remainder;
        return megabytes;
    } catch (...) {
        return 256;
    }
}

// [INFERRED] Table names. Provider codes pair software/hardware/creative rows
// without and with EAX: 6/0, 1/2, 3/4. Preserve the signed entry test.
const int providerCodes[6] = { 6, 0, 1, 2, 3, 4 };
// : voices per quality row Low/Medium/High/UltraHigh.
const int voicesForQuality[4] = { 16, 32, 32, 128 };
// : quality row -> engine SoundQuality (2 = low, 1 = medium, 0 = high).
const int engineQualityForQuality[4] = { 2, 1, 0, 0 };
}

AudioSettingsData::AudioSettingsData() { reset(); }

void AudioSettingsData::reset()
{
    voipEnabled = true;
    voipPlaybackVolume = 1.0f;
    voipCaptureVolume = 1.0f;
    voipCaptureThreshold = 0.1f;
    voipBoostEnabled = false;
    voipUsePushToTalk = true;
    provider = "software";
    soundQuality = physicalMemoryMegabytes() < 768 ? "Low" : "Medium";
    effectsVolume = 1.0f;
    helpVoiceVolume = 1.0f;
    musicVolume = 0.33f;
    englishOnlyVoices = false;
    alwaysAllowSystemOpenAL = false;
    forceNumberOfVoices = -1;
    enableEAX = true;
}

void AudioSettingsData::copyFrom(const AudioSettingsData& other)
{
    voipEnabled = other.voipEnabled;
    voipPlaybackVolume = other.voipPlaybackVolume;
    voipCaptureVolume = other.voipCaptureVolume;
    voipCaptureThreshold = other.voipCaptureThreshold;
    voipBoostEnabled = other.voipBoostEnabled;
    voipUsePushToTalk = other.voipUsePushToTalk;
    provider = other.provider;
    soundQuality = other.soundQuality;
    effectsVolume = other.effectsVolume;
    musicVolume = other.musicVolume;
    helpVoiceVolume = other.helpVoiceVolume;
    englishOnlyVoices = other.englishOnlyVoices;
    alwaysAllowSystemOpenAL = other.alwaysAllowSystemOpenAL;
    forceNumberOfVoices = other.forceNumberOfVoices;
    enableEAX = other.enableEAX;
}

bool AudioSettingsData::differsFrom(const AudioSettingsData& other) const
{
    return voipEnabled != other.voipEnabled
        || voipPlaybackVolume != other.voipPlaybackVolume
        || voipCaptureVolume != other.voipCaptureVolume
        || voipCaptureThreshold != other.voipCaptureThreshold
        || voipBoostEnabled != other.voipBoostEnabled
        || voipUsePushToTalk != other.voipUsePushToTalk
        || provider != other.provider
        || soundQuality != other.soundQuality
        || effectsVolume != other.effectsVolume
        || musicVolume != other.musicVolume
        || helpVoiceVolume != other.helpVoiceVolume
        || englishOnlyVoices != other.englishOnlyVoices
        || enableEAX != other.enableEAX
        || alwaysAllowSystemOpenAL != other.alwaysAllowSystemOpenAL
        || forceNumberOfVoices != other.forceNumberOfVoices;
}

// (93 B, retn 4), five comparisons against the other record, each
// straight to `mov al, 1` when equal - the strings through std::operator==, then the three bytes and
// the int. So the answer is true unless every one of the five differs. Kept as shipped.
bool AudioSettingsData::anyRestartFieldEqual(const AudioSettingsData& other) const
{
    return provider == other.provider
        || soundQuality == other.soundQuality
        || enableEAX == other.enableEAX
        || alwaysAllowSystemOpenAL == other.alwaysAllowSystemOpenAL
        || forceNumberOfVoices == other.forceNumberOfVoices;
}

void AudioSettingsData::write(std::ostream& out) const
{
    out << "AudioSettings.setVoipEnabled " << voipEnabled << std::endl;
    out << "AudioSettings.setVoipPlaybackVolume " << voipPlaybackVolume << std::endl;
    out << "AudioSettings.setVoipCaptureVolume " << voipCaptureVolume << std::endl;
    out << "AudioSettings.setVoipCaptureThreshold " << voipCaptureThreshold << std::endl;
    out << "AudioSettings.setVoipBoostEnabled " << voipBoostEnabled << std::endl;
    out << "AudioSettings.setVoipUsePushToTalk " << voipUsePushToTalk << std::endl;
    // The retail insertion overload is const char*, including embedded-NUL truncation.
    out << "AudioSettings.setProvider \"" << provider.c_str() << "\"" << std::endl;
    out << "AudioSettings.setSoundQuality \"" << soundQuality.c_str() << "\"" << std::endl;
    out << "AudioSettings.setEffectsVolume " << effectsVolume << std::endl;
    out << "AudioSettings.setMusicVolume " << musicVolume << std::endl;
    out << "AudioSettings.setHelpVoiceVolume " << helpVoiceVolume << std::endl;
    out << "AudioSettings.setEnglishOnlyVoices " << englishOnlyVoices << std::endl;
    out << "AudioSettings.setEnableEAX " << enableEAX << std::endl;
    if (alwaysAllowSystemOpenAL)
        out << "AudioSettings.setAlwaysAllowSystemOpenAL " << alwaysAllowSystemOpenAL << std::endl;
    if (forceNumberOfVoices > 0)
        out << "AudioSettings.setForceNumberOfVoices " << forceNumberOfVoices << std::endl;
}

void AudioSettingsEnum::add(const char* name, int value)
{
    {
        std::string text(name);
        names.push_back(text);
    }
    values.push_back(value);
}

// (146 B): VC7.1 vector::size() null-begin tests and std::operator!=(string,
// char*) are the source-level loop. The row is read from values after a names-size
// test; an absent name returns -1 (or eax, -1).
int AudioSettingsEnum::getValue(const char* name) const
{
    unsigned int i = 0;
    while (i < names.size() && names[i] != name) ++i;
    if (i < names.size()) return values[i];
    return -1;
}

// (115 B): the mirror lookup over values returns the row's c_str(), or null.
const char* AudioSettingsEnum::getName(int value) const
{
    unsigned int i = 0;
    while (i < values.size() && values[i] != value) ++i;
    if (i < values.size()) return names[i].c_str();
    return 0;
}

AudioSettings::AudioSettings() : m_refCount(1)
{
    // Each native data member has already reset itself, including its distinct
    // physical-memory query. The enum rows follow both resets in the original.
    m_providers.add("software", 0);
    m_providers.add("hardware", 2);
    m_providers.add("creative", 4);
    m_qualities.add("Low", 0);
    m_qualities.add("Medium", 1);
    m_qualities.add("High", 2);
    m_qualities.add("UltraHigh", 3);
    m_current.provider = "";
    m_status = 0;
}

AudioSettings::~AudioSettings() = default; // pending, current, quality values/names, provider values/names

IBase* AudioSettings::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(AudioSettings));
    AudioSettings* owner = allocation ? new (allocation) AudioSettings : nullptr;
    // Factory4105B9 has no guard between the allocation result and queryInterface.
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void AudioSettings::addRef() { ++m_refCount; }
int AudioSettings::getRef() { return m_refCount; }
int AudioSettings::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
void* AudioSettings::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IAudioSettings)
        return const_cast<AudioSettings*>(this);
    return nullptr;
}
std::string AudioSettings::getFileName() { return std::string("Audio.con"); }
std::string AudioSettings::getSettingsScript()
{
    std::stringstream stream;
    m_current.write(stream);
    return stream.str();
}

//: comparison direction and early field comparisons remain observable.
bool AudioSettings::hasChanges()
{
    return m_pending.differsFrom(m_current);
}

// (20 B): the inlined self-assignment guard around the member copy; the
// pending record takes the current one. Nothing is applied.
void AudioSettings::revertChanges()
{
    m_pending.copyFrom(m_current);
}

// (18 B): status clears, then the pending record resets (tail call to 4B0C10,
// including its own physical-memory query). createDefaultProfile calls this after
// GameServerSettings' reset and before VideoSettings' defaults.
void AudioSettings::setDefaults()
{
    m_status = 0;
    m_pending.reset();
}

// (42 B, virtual +0x28), false outside a game (the dice::hfe::game
// test comes first) and false without pending changes (+0x1C, through the table); otherwise the
// pending record's 4B1090 comparison against the current one - with its shipped sense (see the
// header): true unless all five restart-relevant fields changed. SwiffPlayer's needsRestart row.
bool AudioSettings::needsRestart()
{
    if (!game)
        return false;
    if (!hasChanges())
        return false;
    return m_pending.anyRestartFieldEqual(m_current);
}

namespace {
//  / 4B13C0 share their first half: the pending provider's row value (-1 when unknown) plus one
// for a set EAX byte (`cmp [esi+105h], cl; setnz cl; add eax, ecx`), and the provider code that index
// selects - or -1 outside the table (`js`, then the unsigned `cmp eax, 6; jnb`).
int pendingProviderCode(const AudioSettingsEnum& providers, const AudioSettingsData& pending)
{
    const int index = providers.getValue(pending.provider.c_str()) + (pending.enableEAX ? 1 : 0);
    if (index < 0 || static_cast<unsigned int>(index) >= sizeof(providerCodes) / sizeof(providerCodes[0]))
        return -1;
    return providerCodes[index];
}
}

// (82 B, virtual +0x2C), the pending provider offers EAX when its code
// is 4 or 2 - hardware or creative with the EAX box ticked. SwiffPlayer's hasEax row.
bool AudioSettings::hasEax()
{
    const int code = pendingProviderCode(m_providers, m_pending);
    return code == 4 || code == 2;
}

// (82 B, virtual +0x30), the same index, true for code 3 or 4 - the
// creative provider, with or without EAX. It is what unlocks the UltraHigh quality: +0xA4 below counts
// one quality fewer without it, and applyRecord downgrades UltraHigh on any other provider.
bool AudioSettings::hasZenithBoard()
{
    const int code = pendingProviderCode(m_providers, m_pending);
    return code == 3 || code == 4;
}

// (67 B, virtual +0xA0, sret), when the count pointer is given, the
// provider name count (0 without storage) is written through it; the provider names are returned
// as a copy (47EF90, the vector<string> copy constructor).
std::vector<std::string> AudioSettings::getProviderNames(int* count)
{
    if (count)
        *count = static_cast<int>(m_providers.names.size());
    return m_providers.names;
}

// Returns quality names. Optional count excludes UltraHigh without a creative
// provider, including a -1 result for an empty list.
std::vector<std::string> AudioSettings::getSoundQualityNames(int* count)
{
    if (count)
    {
        if (!hasZenithBoard())
            *count = static_cast<int>(m_qualities.names.size()) - 1;
        else
            *count = static_cast<int>(m_qualities.names.size());
    }
    return m_qualities.names;
}

//: the +1C call is virtual; unchanged settings really return true.
// The retry result is ignored and the original pending result is returned.
// After the retry the core engine restarts the music (4B17AF, ISoundEngine +C0):
// the failed re-initialisation silenced it. That result is ignored too.
bool AudioSettings::apply(bool reportErrors)
{
    bool result = true;
    if (hasChanges()) {
        result = applyRecord(m_pending, reportErrors);
        if (result) {
            m_current.copyFrom(m_pending);
        } else {
            applyRecord(m_current, reportErrors);
            g_bfSound->engine()->playMusic();
        }
        // [TEMP] [CHOSEN, not recovered] BF2_AUDIO_TRACE=1: what an apply left in the sound
        // facade, for unattended option runs. [RETIRE-WHEN: macos-options-validated]
        const char* trace = std::getenv("BF2_AUDIO_TRACE");
        if (trace && trace[0] == '1')
        {
            std::fprintf(stderr, "[TEMP] [AUDIO] apply result=%d status=%d provider=\"%s\" eax=%d quality=\"%s\" "
                         "groups effects=%.3f music=%.3f voice=%.3f effects3=%.3f\n",
                         result ? 1 : 0, m_status, m_current.provider.c_str(), m_current.enableEAX ? 1 : 0,
                         m_current.soundQuality.c_str(), g_bfSound->getVolumeForGroup(0),
                         g_bfSound->getVolumeForGroup(1), g_bfSound->getVolumeForGroup(2),
                         g_bfSound->getVolumeForGroup(3));
            std::fflush(stderr);
        }
    }
    return result;
}

// Posts an error message when SwiffHost exists; otherwise does nothing.
void AudioSettings::reportError(const std::string& message)
{
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (host) static_cast<ui::ISwiffHost*>(host)->postMessage(SwiffHostMessage(3, message, 3));   // +0x1E4
}

// Applies volumes and downgrades UltraHigh in place unless the provider is creative.
// Further device changes are deferred while a game exists.
bool AudioSettings::applyRecord(AudioSettingsData& data, bool reportErrors)
{
    g_bfSound->engine()->setMasterVolume(1.0f);
    g_bfSound->setVolumeForGroup(0, data.effectsVolume);
    g_bfSound->setVolumeForGroup(3, data.effectsVolume);
    g_bfSound->setVolumeForGroup(1, data.musicVolume);
    g_bfSound->setVolumeForGroup(2, data.helpVoiceVolume);
    if (game) return true;

    // 1 for an unknown provider name is still a valid index once EAX adds one:
    // "unknown" + EAX selects code 6 exactly as retail does. The upper bound is the
    // unsigned table-size comparison (jnb) the PE emits.
    const int providerIndex = m_providers.getValue(data.provider.c_str()) + (data.enableEAX ? 1 : 0);
    int providerCode = -1;
    if (providerIndex >= 0
        && static_cast<unsigned int>(providerIndex) < sizeof(providerCodes) / sizeof(providerCodes[0]))
        providerCode = providerCodes[providerIndex];
    if (providerCode < 0) {
        m_status = 1;
        return false;
    }
    char providerString[2] = " ";
    providerString[0] = static_cast<char>('0' + providerCode);

    int quality = m_qualities.getValue(data.soundQuality.c_str());
    if (providerCode != 3 && providerCode != 4 && quality == 3)
        data.soundQuality = m_qualities.getName(2);
    // The quality is looked up again after the possible downgrade. [UNRESOLVED]
    // retail defect: an unknown quality name (a hand-edited Audio.con) indexes both
    // tables with -1; the PE reads the preceding table's last entry. No guard exists.
    quality = m_qualities.getValue(data.soundQuality.c_str());
    int voices = voicesForQuality[quality];
    if (data.forceNumberOfVoices > 0) voices = data.forceNumberOfVoices;
    g_bfSound->setProperty(std::string("enable-culling"), quality == 3 ? 0.0f : 1.0f);
    g_bfSound->setProperty(std::string("allow-any-system-openal"), data.alwaysAllowSystemOpenAL ? 1.0f : 0.0f);
    const int result = g_bfSound->initialize(engineQualityForQuality[quality], std::string(providerString), voices);
    switch (result) {
    case 0:
        m_status = 0;
        return true;
    case 1:
        if (reportErrors) reportError(std::string("noprovider"));
        break;
    case 2:
        if (reportErrors) reportError(std::string("noproviderdll"));
        break;
    case 3:
        if (reportErrors) reportError(std::string("openaldenied"));
        break;
    case 4:
        if (reportErrors) reportError(std::string("openalerror"));
        break;
    default:
        break;
    }
    m_status = 1;
    return false;
}

} }
#endif
