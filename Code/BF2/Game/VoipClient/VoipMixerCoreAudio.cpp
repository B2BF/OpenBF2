// [PLATFORM] macOS backend for VoipMixer: the seven WINMM mixer entry points on the CoreAudio HAL.
//
// The shell presents mixer device 0 as long as the default input device has an input-scope
// kAudioDevicePropertyVolumeScalar (master element, else channel 1). Without one it answers like a
// machine with no mixer in retail (MMSYSERR_BADDEVICEID from mixerGetDevCapsA and mixerOpen), and
// VoipMixer::open fails there.
//
// The shell's device, in the shape VoipMixer::findLines requires (it needs a microphone source on a
// speakers destination and on a wave-in destination):
//   destination 0  DST_SPEAKERS, one source: the microphone, no controls (no monitor mute);
//   destination 1  DST_WAVEIN,   one source: the microphone, one VOLUME control (0..65535) on the
//                  input volume scalar. No MUX, so selectMicrophoneRecordingSource answers false,
//                  and no LOUDNESS/ONOFF, so the recovered code takes retail's "no boost" branch.
// Only device properties are read and written; no input stream is opened, so there is no
// microphone permission prompt.
#if defined(__APPLE__)

#include "VoipMixer.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <cmath>
#include <cstring>

namespace dice {
namespace hfe {

namespace {

const DWORD kSpeakersDestinationId = 0xFFFF0000u;  // WINMM-style ids: destinations 0xFFFF00nn,
const DWORD kWaveInDestinationId   = 0xFFFF0001u;  // sources (dest << 16) | source
const DWORD kSpeakersMicrophoneId  = 0x00000000u;
const DWORD kWaveInMicrophoneId    = 0x00010000u;
const DWORD kVolumeControlId       = 1;
const DWORD kVolumeMaximum         = 65535;

// Open handles: the address of a slot. Each mixerOpen gets a fresh one, as WINMM's do, because
// VoipMixer::open closes the previous handle after opening the next.
bool s_handleOpen[8];

bool isOpenHandle(const void* handle)
{
    for (std::size_t i = 0; i < sizeof(s_handleOpen) / sizeof(s_handleOpen[0]); ++i)
        if (handle == &s_handleOpen[i])
            return s_handleOpen[i];
    return false;
}

// WINMM accepts mixer id 0 or an open handle here (retail passes its handle with OBJECTF_MIXER).
bool isMixerObject(const void* object)
{
    return object == 0 || isOpenHandle(object);
}

struct InputVolume
{
    AudioObjectID              device;
    AudioObjectPropertyElement element;
};

AudioObjectPropertyAddress volumeAddress(AudioObjectPropertyElement element)
{
    AudioObjectPropertyAddress address = { kAudioDevicePropertyVolumeScalar,
                                           kAudioDevicePropertyScopeInput, element };
    return address;
}

bool findInputVolume(InputVolume* out)
{
    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultInputDevice,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    if (::AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, 0, &size, &device) != noErr
        || device == kAudioObjectUnknown)
        return false;
    const AudioObjectPropertyElement elements[2] = { kAudioObjectPropertyElementMain, 1 };
    for (int i = 0; i < 2; ++i) {
        const AudioObjectPropertyAddress volume = volumeAddress(elements[i]);
        if (::AudioObjectHasProperty(device, &volume)) {
            out->device = device;
            out->element = elements[i];
            return true;
        }
    }
    return false;
}

void copyName(char* destination, std::size_t capacity, const char* source)
{
    std::strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

void deviceName(AudioObjectID device, char* destination, std::size_t capacity)
{
    destination[0] = '\0';
    AudioObjectPropertyAddress address = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    CFStringRef name = 0;
    UInt32 size = sizeof(name);
    if (::AudioObjectGetPropertyData(device, &address, 0, 0, &size, &name) == noErr && name) {
        if (!::CFStringGetCString(name, destination, static_cast<CFIndex>(capacity), kCFStringEncodingUTF8))
            destination[0] = '\0';
        ::CFRelease(name);
    }
}

bool readVolume(const InputVolume& input, DWORD* value)
{
    const AudioObjectPropertyAddress address = volumeAddress(input.element);
    Float32 scalar = 0.0f;
    UInt32 size = sizeof(scalar);
    if (::AudioObjectGetPropertyData(input.device, &address, 0, 0, &size, &scalar) != noErr)
        return false;
    if (!(scalar > 0.0f))
        scalar = 0.0f;
    if (scalar > 1.0f)
        scalar = 1.0f;
    *value = static_cast<DWORD>(std::floor(scalar * static_cast<Float32>(kVolumeMaximum) + 0.5f));
    return true;
}

// Values above the control's maximum are clamped, as WDM mixer drivers clamp to the range.
bool writeVolume(const InputVolume& input, DWORD value)
{
    const AudioObjectPropertyAddress address = volumeAddress(input.element);
    Boolean settable = false;
    if (::AudioObjectIsPropertySettable(input.device, &address, &settable) != noErr || !settable)
        return false;
    Float32 scalar = static_cast<Float32>(value > kVolumeMaximum ? kVolumeMaximum : value)
                     / static_cast<Float32>(kVolumeMaximum);
    return ::AudioObjectSetPropertyData(input.device, &address, 0, 0, sizeof(scalar), &scalar) == noErr;
}

bool fillLine(DWORD lineId, MIXERLINEA* line, const InputVolume& input)
{
    const DWORD cbStruct = line->cbStruct;
    std::memset(line, 0, sizeof(MIXERLINEA));
    line->cbStruct = cbStruct;
    line->dwLineID = lineId;
    line->cChannels = 1;
    switch (lineId) {
    case kSpeakersDestinationId:
        line->dwDestination = 0;
        line->fdwLine = MIXERLINE_LINEF_ACTIVE;
        line->dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
        line->cConnections = 1;
        copyName(line->szShortName, sizeof(line->szShortName), "Playback");
        copyName(line->szName, sizeof(line->szName), "Playback");
        line->Target.dwType = MIXERLINE_TARGETTYPE_WAVEOUT;
        break;
    case kWaveInDestinationId:
        line->dwDestination = 1;
        line->fdwLine = MIXERLINE_LINEF_ACTIVE;
        line->dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
        line->cConnections = 1;
        copyName(line->szShortName, sizeof(line->szShortName), "Recording");
        copyName(line->szName, sizeof(line->szName), "Recording");
        line->Target.dwType = MIXERLINE_TARGETTYPE_WAVEIN;
        break;
    case kSpeakersMicrophoneId:
    case kWaveInMicrophoneId:
        line->dwDestination = lineId >> 16;
        line->dwSource = 0;
        line->fdwLine = MIXERLINE_LINEF_SOURCE | MIXERLINE_LINEF_ACTIVE;
        line->dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
        line->cControls = lineId == kWaveInMicrophoneId ? 1 : 0;
        copyName(line->szShortName, sizeof(line->szShortName), "Microphone");
        copyName(line->szName, sizeof(line->szName), "Microphone");
        line->Target.dwType = MIXERLINE_TARGETTYPE_WAVEIN;
        break;
    default:
        return false;
    }
    deviceName(input.device, line->Target.szPname, sizeof(line->Target.szPname));
    return true;
}

void fillVolumeControl(MIXERCONTROLA* control)
{
    std::memset(control, 0, sizeof(MIXERCONTROLA));
    control->cbStruct = sizeof(MIXERCONTROLA);
    control->dwControlID = kVolumeControlId;
    control->dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    control->fdwControl = MIXERCONTROL_CONTROLF_UNIFORM;
    copyName(control->szShortName, sizeof(control->szShortName), "Volume");
    copyName(control->szName, sizeof(control->szName), "Microphone Volume");
    control->Bounds.dwMinimum = 0;
    control->Bounds.dwMaximum = kVolumeMaximum;
    control->Metrics.cSteps = kVolumeMaximum;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioOpen(HMIXER* phmx, UINT uMxId, DWORD_PTR, DWORD_PTR, DWORD)
{
    if (!phmx)
        return MMSYSERR_INVALPARAM;
    InputVolume input;
    if (uMxId != 0 || !findInputVolume(&input))
        return MMSYSERR_BADDEVICEID;
    for (std::size_t i = 0; i < sizeof(s_handleOpen) / sizeof(s_handleOpen[0]); ++i) {
        if (!s_handleOpen[i]) {
            s_handleOpen[i] = true;
            *phmx = reinterpret_cast<HMIXER>(&s_handleOpen[i]);
            return MMSYSERR_NOERROR;
        }
    }
    return MMSYSERR_NOMEM;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioClose(HMIXER hmx)
{
    for (std::size_t i = 0; i < sizeof(s_handleOpen) / sizeof(s_handleOpen[0]); ++i) {
        if (reinterpret_cast<void*>(hmx) == &s_handleOpen[i] && s_handleOpen[i]) {
            s_handleOpen[i] = false;
            return MMSYSERR_NOERROR;
        }
    }
    return MMSYSERR_INVALHANDLE;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioGetDevCaps(UINT_PTR uMxId, MIXERCAPSA* pmxcaps, UINT cbmxcaps)
{
    if (!pmxcaps)
        return MMSYSERR_INVALPARAM;
    if (uMxId != 0 && !isOpenHandle(reinterpret_cast<void*>(uMxId)))
        return MMSYSERR_BADDEVICEID;
    InputVolume input;
    if (!findInputVolume(&input))
        return uMxId != 0 ? MMSYSERR_NODRIVER : MMSYSERR_BADDEVICEID;
    MIXERCAPSA caps;
    std::memset(&caps, 0, sizeof(caps));
    deviceName(input.device, caps.szPname, sizeof(caps.szPname));
    caps.cDestinations = 2;
    std::memcpy(pmxcaps, &caps, cbmxcaps < sizeof(caps) ? cbmxcaps : sizeof(caps));
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioGetLineInfo(HMIXEROBJ hmxobj, MIXERLINEA* pmxl, DWORD fdwInfo)
{
    if (!isMixerObject(hmxobj))
        return MMSYSERR_INVALHANDLE;
    if (!pmxl || pmxl->cbStruct < sizeof(MIXERLINEA))
        return MMSYSERR_INVALPARAM;
    InputVolume input;
    if (!findInputVolume(&input))
        return MMSYSERR_NODRIVER;
    DWORD lineId = 0;
    switch (fdwInfo & MIXER_GETLINEINFOF_QUERYMASK) {
    case MIXER_GETLINEINFOF_DESTINATION:
        if (pmxl->dwDestination > 1)
            return MMSYSERR_INVALPARAM;
        lineId = pmxl->dwDestination == 0 ? kSpeakersDestinationId : kWaveInDestinationId;
        break;
    case MIXER_GETLINEINFOF_SOURCE:
        if (pmxl->dwDestination > 1 || pmxl->dwSource != 0)
            return MMSYSERR_INVALPARAM;
        lineId = pmxl->dwDestination == 0 ? kSpeakersMicrophoneId : kWaveInMicrophoneId;
        break;
    case MIXER_GETLINEINFOF_LINEID:
        lineId = pmxl->dwLineID;
        break;
    case MIXER_GETLINEINFOF_COMPONENTTYPE:
        if (pmxl->dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS)
            lineId = kSpeakersDestinationId;
        else if (pmxl->dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN)
            lineId = kWaveInDestinationId;
        else if (pmxl->dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE)
            lineId = kSpeakersMicrophoneId;
        else
            return MIXERR_INVALLINE;
        break;
    default:
        return MMSYSERR_NOTSUPPORTED;
    }
    return fillLine(lineId, pmxl, input) ? MMSYSERR_NOERROR : MIXERR_INVALLINE;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioGetLineControls(HMIXEROBJ hmxobj, MIXERLINECONTROLSA* pmxlc, DWORD fdwControls)
{
    if (!isMixerObject(hmxobj))
        return MMSYSERR_INVALHANDLE;
    if (!pmxlc || pmxlc->cbStruct < sizeof(MIXERLINECONTROLSA) || pmxlc->cbmxctrl < sizeof(MIXERCONTROLA)
        || !pmxlc->pamxctrl)
        return MMSYSERR_INVALPARAM;
    InputVolume input;
    if (!findInputVolume(&input))
        return MMSYSERR_NODRIVER;
    switch (fdwControls & MIXER_GETLINECONTROLSF_QUERYMASK) {
    case MIXER_GETLINECONTROLSF_ALL: {
        MIXERLINEA line;
        line.cbStruct = sizeof(line);
        if (!fillLine(pmxlc->dwLineID, &line, input))
            return MIXERR_INVALLINE;
        if (line.cControls == 0 || pmxlc->cControls != line.cControls)
            return MMSYSERR_INVALPARAM;
        fillVolumeControl(pmxlc->pamxctrl);
        return MMSYSERR_NOERROR;
    }
    case MIXER_GETLINECONTROLSF_ONEBYID:
        if (pmxlc->dwControlID != kVolumeControlId)
            return MIXERR_INVALCONTROL;
        pmxlc->dwLineID = kWaveInMicrophoneId;
        fillVolumeControl(pmxlc->pamxctrl);
        return MMSYSERR_NOERROR;
    case MIXER_GETLINECONTROLSF_ONEBYTYPE:
        if (pmxlc->dwLineID != kWaveInMicrophoneId || pmxlc->dwControlType != MIXERCONTROL_CONTROLTYPE_VOLUME)
            return MIXERR_INVALCONTROL;
        fillVolumeControl(pmxlc->pamxctrl);
        return MMSYSERR_NOERROR;
    default:
        return MMSYSERR_NOTSUPPORTED;
    }
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioGetControlDetails(HMIXEROBJ hmxobj, MIXERCONTROLDETAILS* pmxcd, DWORD fdwDetails)
{
    if (!isMixerObject(hmxobj))
        return MMSYSERR_INVALHANDLE;
    if (!pmxcd || pmxcd->cbStruct < sizeof(MIXERCONTROLDETAILS) || !pmxcd->paDetails)
        return MMSYSERR_INVALPARAM;
    if (pmxcd->dwControlID != kVolumeControlId)
        return MIXERR_INVALCONTROL;
    if ((fdwDetails & MIXER_GETCONTROLDETAILSF_QUERYMASK) != MIXER_GETCONTROLDETAILSF_VALUE)
        return MMSYSERR_INVALPARAM;     // a fader has no list text
    if (pmxcd->cChannels != 1 || pmxcd->cbDetails < sizeof(MIXERCONTROLDETAILS_UNSIGNED))
        return MMSYSERR_INVALPARAM;
    InputVolume input;
    if (!findInputVolume(&input))
        return MMSYSERR_NODRIVER;
    DWORD value = 0;
    if (!readVolume(input, &value))
        return MMSYSERR_ERROR;
    static_cast<MIXERCONTROLDETAILS_UNSIGNED*>(pmxcd->paDetails)->dwValue = value;
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI coreAudioSetControlDetails(HMIXEROBJ hmxobj, MIXERCONTROLDETAILS* pmxcd, DWORD fdwDetails)
{
    if (!isMixerObject(hmxobj))
        return MMSYSERR_INVALHANDLE;
    if (!pmxcd || pmxcd->cbStruct < sizeof(MIXERCONTROLDETAILS) || !pmxcd->paDetails)
        return MMSYSERR_INVALPARAM;
    if (pmxcd->dwControlID != kVolumeControlId)
        return MIXERR_INVALCONTROL;
    if ((fdwDetails & MIXER_SETCONTROLDETAILSF_QUERYMASK) != MIXER_SETCONTROLDETAILSF_VALUE)
        return MMSYSERR_NOTSUPPORTED;
    if (pmxcd->cChannels != 1 || pmxcd->cbDetails < sizeof(MIXERCONTROLDETAILS_UNSIGNED))
        return MMSYSERR_INVALPARAM;
    InputVolume input;
    if (!findInputVolume(&input))
        return MMSYSERR_NODRIVER;
    const DWORD value = static_cast<const MIXERCONTROLDETAILS_UNSIGNED*>(pmxcd->paDetails)->dwValue;
    return writeVolume(input, value) ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
}

const VoipMixerApi kCoreAudioApi = {
    coreAudioOpen, coreAudioClose, coreAudioGetDevCaps, coreAudioGetLineInfo,
    coreAudioGetLineControls, coreAudioGetControlDetails, coreAudioSetControlDetails,
};

} // namespace

const VoipMixerApi* voipMixerCoreAudioApi()
{
    return &kCoreAudioApi;
}

} // namespace hfe
} // namespace dice

#endif // __APPLE__
