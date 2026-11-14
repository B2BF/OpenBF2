// [TEMP] [CHOSEN, not recovered] VoipMixer (BF2.exe 0x7356B0..0x736BA0) against a scripted WINMM
// mixer installed through VoipMixer::setApiForTesting: no hardware, no microphone permission.
// The whole recovered logic runs: open with and without a mixer, the line search, the boost
// search (LOUDNESS, then ONOFF; speakers microphone first), volume scaling, the MUX selection,
// the monitor mute, close and re-open.
//
// macOS:  clang++ -std=c++14 -DBF2_VOIPMIXER_NO_PLATFORM_BACKEND Code/Tests/voip_mixer_runtime.cpp
//         Code/BF2/Game/VoipClient/VoipMixer.cpp -o voip_mixer_runtime
#include "../BF2/Game/VoipClient/VoipMixer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using namespace dice::hfe;

namespace {

unsigned g_checks = 0;
unsigned g_failures = 0;

void check(bool condition, const char* what)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// --- the scripted mixer ------------------------------------------------------------------------

struct FakeControl
{
    MIXERCONTROLA                              control;
    std::vector<DWORD>                         values;  // cMultipleItems (or 1) entries
    std::vector<MIXERCONTROLDETAILS_LISTTEXTA> list;
};

struct FakeLine
{
    MIXERLINEA               line;
    std::vector<FakeControl> controls;
    std::vector<FakeLine>    sources;   // destinations only
};

struct FakeDevice
{
    bool                  present;
    bool                  openFails;
    bool                  failGetDetails;
    std::vector<FakeLine> destinations;

    std::vector<HMIXER>   openHandles;
    std::vector<HMIXER>   closedHandles;
    unsigned              nextHandle;
    unsigned              openCalls;
    unsigned              getDetailsCalls;
    unsigned              setDetailsCalls;
    unsigned              badHandleCalls;
};

FakeDevice g_device;

void resetDevice()
{
    g_device = FakeDevice();
    g_device.present = true;
    g_device.nextHandle = 0x100;
}

bool isOpen(const void* handle)
{
    for (std::size_t i = 0; i < g_device.openHandles.size(); ++i)
        if (reinterpret_cast<const void*>(g_device.openHandles[i]) == handle)
            return true;
    return false;
}

bool validObject(HMIXEROBJ object)
{
    if (isOpen(object))
        return true;
    ++g_device.badHandleCalls;
    return false;
}

FakeLine* findLine(DWORD lineId)
{
    for (std::size_t d = 0; d < g_device.destinations.size(); ++d) {
        FakeLine& destination = g_device.destinations[d];
        if (destination.line.dwLineID == lineId)
            return &destination;
        for (std::size_t s = 0; s < destination.sources.size(); ++s)
            if (destination.sources[s].line.dwLineID == lineId)
                return &destination.sources[s];
    }
    return 0;
}

FakeControl* findControl(DWORD controlId)
{
    for (std::size_t d = 0; d < g_device.destinations.size(); ++d) {
        FakeLine& destination = g_device.destinations[d];
        for (std::size_t c = 0; c < destination.controls.size(); ++c)
            if (destination.controls[c].control.dwControlID == controlId)
                return &destination.controls[c];
        for (std::size_t s = 0; s < destination.sources.size(); ++s)
            for (std::size_t c = 0; c < destination.sources[s].controls.size(); ++c)
                if (destination.sources[s].controls[c].control.dwControlID == controlId)
                    return &destination.sources[s].controls[c];
    }
    return 0;
}

void copyLine(const FakeLine& fake, DWORD destination, DWORD source, MIXERLINEA* out)
{
    *out = fake.line;
    out->cbStruct = sizeof(MIXERLINEA);
    out->dwDestination = destination;
    out->dwSource = source;
    out->cConnections = static_cast<DWORD>(fake.sources.size());
    out->cControls = static_cast<DWORD>(fake.controls.size());
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeOpen(HMIXER* phmx, UINT uMxId, DWORD_PTR, DWORD_PTR, DWORD fdwOpen)
{
    ++g_device.openCalls;
    check(fdwOpen == CALLBACK_WINDOW, "mixerOpen flags are CALLBACK_WINDOW (0x736BF2)");
    if (!g_device.present || uMxId != 0)
        return MMSYSERR_BADDEVICEID;
    if (g_device.openFails)
        return MMSYSERR_ALLOCATED;
    *phmx = reinterpret_cast<HMIXER>(static_cast<std::uintptr_t>(g_device.nextHandle++));
    g_device.openHandles.push_back(*phmx);
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeClose(HMIXER hmx)
{
    for (std::size_t i = 0; i < g_device.openHandles.size(); ++i) {
        if (g_device.openHandles[i] == hmx) {
            g_device.openHandles.erase(g_device.openHandles.begin() + static_cast<std::ptrdiff_t>(i));
            g_device.closedHandles.push_back(hmx);
            return MMSYSERR_NOERROR;
        }
    }
    return MMSYSERR_INVALHANDLE;
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeGetDevCaps(UINT_PTR uMxId, MIXERCAPSA* caps, UINT cb)
{
    if (!g_device.present)
        return MMSYSERR_BADDEVICEID;
    if (uMxId != 0 && !isOpen(reinterpret_cast<void*>(uMxId)))
        return MMSYSERR_INVALHANDLE;
    if (cb < sizeof(MIXERCAPSA))
        return MMSYSERR_INVALPARAM;
    std::memset(caps, 0, sizeof(*caps));
    std::strcpy(caps->szPname, "Scripted Mixer");
    caps->cDestinations = static_cast<DWORD>(g_device.destinations.size());
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeGetLineInfo(HMIXEROBJ object, MIXERLINEA* line, DWORD flags)
{
    if (!validObject(object))
        return MMSYSERR_INVALHANDLE;
    if (line->cbStruct != sizeof(MIXERLINEA))
        return MMSYSERR_INVALPARAM;
    switch (flags & MIXER_GETLINEINFOF_QUERYMASK) {
    case MIXER_GETLINEINFOF_DESTINATION:
        if (line->dwDestination >= g_device.destinations.size())
            return MMSYSERR_INVALPARAM;
        copyLine(g_device.destinations[line->dwDestination], line->dwDestination, 0, line);
        return MMSYSERR_NOERROR;
    case MIXER_GETLINEINFOF_SOURCE: {
        if (line->dwDestination >= g_device.destinations.size())
            return MMSYSERR_INVALPARAM;
        const FakeLine& destination = g_device.destinations[line->dwDestination];
        if (line->dwSource >= destination.sources.size())
            return MMSYSERR_INVALPARAM;
        copyLine(destination.sources[line->dwSource], line->dwDestination, line->dwSource, line);
        return MMSYSERR_NOERROR;
    }
    case MIXER_GETLINEINFOF_LINEID: {
        check((flags & MIXER_OBJECTF_HMIXER) == MIXER_OBJECTF_HMIXER, "line-id query names a handle (0x736008)");
        const FakeLine* fake = findLine(line->dwLineID);
        if (!fake)
            return MIXERR_INVALLINE;
        copyLine(*fake, 0, 0, line);
        return MMSYSERR_NOERROR;
    }
    default:
        return MMSYSERR_NOTSUPPORTED;
    }
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeGetLineControls(HMIXEROBJ object, MIXERLINECONTROLSA* query, DWORD flags)
{
    if (!validObject(object))
        return MMSYSERR_INVALHANDLE;
    if (query->cbStruct != sizeof(MIXERLINECONTROLSA) || query->cbmxctrl != sizeof(MIXERCONTROLA))
        return MMSYSERR_INVALPARAM;
    if ((flags & MIXER_GETLINECONTROLSF_QUERYMASK) != MIXER_GETLINECONTROLSF_ALL)
        return MMSYSERR_NOTSUPPORTED;
    const FakeLine* line = findLine(query->dwLineID);
    if (!line)
        return MIXERR_INVALLINE;
    if (query->cControls == 0 || query->cControls != line->controls.size())
        return MMSYSERR_INVALPARAM;
    for (std::size_t i = 0; i < line->controls.size(); ++i)
        query->pamxctrl[i] = line->controls[i].control;
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeGetControlDetails(HMIXEROBJ object, MIXERCONTROLDETAILS* details, DWORD flags)
{
    ++g_device.getDetailsCalls;
    if (!validObject(object))
        return MMSYSERR_INVALHANDLE;
    if (g_device.failGetDetails)
        return MMSYSERR_ERROR;
    FakeControl* control = findControl(details->dwControlID);
    if (!control)
        return MIXERR_INVALCONTROL;
    const DWORD items = details->cMultipleItems ? details->cMultipleItems : 1;
    if (details->cChannels != 1 || details->cMultipleItems != control->control.cMultipleItems)
        return MMSYSERR_INVALPARAM;
    if ((flags & MIXER_GETCONTROLDETAILSF_QUERYMASK) == MIXER_GETCONTROLDETAILSF_LISTTEXT) {
        if (details->cbDetails != sizeof(MIXERCONTROLDETAILS_LISTTEXTA) || control->list.size() != items)
            return MMSYSERR_INVALPARAM;
        std::memcpy(details->paDetails, &control->list[0], items * sizeof(MIXERCONTROLDETAILS_LISTTEXTA));
        return MMSYSERR_NOERROR;
    }
    if (details->cbDetails != sizeof(DWORD))
        return MMSYSERR_INVALPARAM;
    std::memcpy(details->paDetails, &control->values[0], items * sizeof(DWORD));
    return MMSYSERR_NOERROR;
}

MMRESULT BF2_VOIPMIXER_WINAPI fakeSetControlDetails(HMIXEROBJ object, MIXERCONTROLDETAILS* details, DWORD)
{
    ++g_device.setDetailsCalls;
    if (!validObject(object))
        return MMSYSERR_INVALHANDLE;
    FakeControl* control = findControl(details->dwControlID);
    if (!control)
        return MIXERR_INVALCONTROL;
    const DWORD items = details->cMultipleItems ? details->cMultipleItems : 1;
    if (details->cChannels != 1 || details->cbDetails != sizeof(DWORD) || items != control->values.size())
        return MMSYSERR_INVALPARAM;
    std::memcpy(&control->values[0], details->paDetails, items * sizeof(DWORD));
    return MMSYSERR_NOERROR;
}

const VoipMixerApi kFakeApi = {
    fakeOpen, fakeClose, fakeGetDevCaps, fakeGetLineInfo,
    fakeGetLineControls, fakeGetControlDetails, fakeSetControlDetails,
};

// --- device builders ---------------------------------------------------------------------------

FakeLine makeLine(DWORD lineId, DWORD componentType, const char* name)
{
    FakeLine fake;
    std::memset(&fake.line, 0, sizeof(fake.line));
    fake.line.dwLineID = lineId;
    fake.line.dwComponentType = componentType;
    fake.line.cChannels = 1;
    std::strncpy(fake.line.szName, name, sizeof(fake.line.szName) - 1);
    return fake;
}

FakeControl makeControl(DWORD controlId, DWORD type, DWORD value, DWORD minimum = 0, DWORD maximum = 1)
{
    FakeControl fake;
    std::memset(&fake.control, 0, sizeof(fake.control));
    fake.control.cbStruct = sizeof(MIXERCONTROLA);
    fake.control.dwControlID = controlId;
    fake.control.dwControlType = type;
    fake.control.fdwControl = MIXERCONTROL_CONTROLF_UNIFORM;
    fake.control.Bounds.dwMinimum = minimum;
    fake.control.Bounds.dwMaximum = maximum;
    fake.values.push_back(value);
    return fake;
}

FakeControl makeMux(DWORD controlId, const DWORD* lineIds, const char* const* names, DWORD count, DWORD selected)
{
    FakeControl fake = makeControl(controlId, MIXERCONTROL_CONTROLTYPE_MUX, 0);
    fake.control.fdwControl = MIXERCONTROL_CONTROLF_UNIFORM | MIXERCONTROL_CONTROLF_MULTIPLE;
    fake.control.cMultipleItems = count;
    fake.values.assign(count, 0);
    fake.values[selected] = 1;
    for (DWORD i = 0; i < count; ++i) {
        MIXERCONTROLDETAILS_LISTTEXTA text;
        std::memset(&text, 0, sizeof(text));
        text.dwParam1 = lineIds[i];
        text.dwParam2 = MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED;
        std::strncpy(text.szName, names[i], sizeof(text.szName) - 1);
        fake.list.push_back(text);
    }
    return fake;
}

DWORD controlValue(DWORD controlId, std::size_t index = 0)
{
    const FakeControl* control = findControl(controlId);
    return control ? control->values[index] : 0xDEADBEEFu;
}

void setControlValue(DWORD controlId, DWORD value)
{
    findControl(controlId)->values[0] = value;
}

// A typical AC97 layout: speakers with Wave and Microphone (volume, mute, LOUDNESS "Mic Boost"),
// recording with a MUX over CD / Microphone / Line In.
enum { kSpkMicVolume = 10, kSpkMicMute = 11, kSpkMicBoost = 12, kRecMux = 20, kRecCdVolume = 21,
       kRecMicVolume = 22, kRecMicOnOff = 30 };

void buildFullDevice()
{
    resetDevice();
    FakeLine speakers = makeLine(0xFFFF0000u, MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, "Master Volume");
    speakers.controls.push_back(makeControl(1, MIXERCONTROL_CONTROLTYPE_VOLUME, 40000, 0, 65535));
    speakers.sources.push_back(makeLine(0x00000000u, MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT, "Wave"));
    FakeLine speakersMic = makeLine(0x00000001u, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, "Microphone");
    speakersMic.controls.push_back(makeControl(kSpkMicVolume, MIXERCONTROL_CONTROLTYPE_VOLUME, 30000, 0, 65535));
    speakersMic.controls.push_back(makeControl(kSpkMicMute, MIXERCONTROL_CONTROLTYPE_MUTE, 0));
    speakersMic.controls.push_back(makeControl(kSpkMicBoost, MIXERCONTROL_CONTROLTYPE_LOUDNESS, 0));
    speakers.sources.push_back(speakersMic);

    FakeLine recording = makeLine(0xFFFF0001u, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, "Recording Control");
    const DWORD ids[3] = { 0x00010000u, 0x00010001u, 0x00010002u };
    const char* const names[3] = { "CD Player", "Microphone", "Line In" };
    recording.controls.push_back(makeMux(kRecMux, ids, names, 3, 0));
    FakeLine cd = makeLine(0x00010000u, MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC, "CD Player");
    cd.controls.push_back(makeControl(kRecCdVolume, MIXERCONTROL_CONTROLTYPE_VOLUME, 1000, 0, 65535));
    recording.sources.push_back(cd);
    FakeLine recordingMic = makeLine(0x00010001u, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, "Microphone");
    recordingMic.controls.push_back(makeControl(kRecMicVolume, MIXERCONTROL_CONTROLTYPE_VOLUME, 20000, 0, 65535));
    recording.sources.push_back(recordingMic);
    recording.sources.push_back(makeLine(0x00010002u, MIXERLINE_COMPONENTTYPE_SRC_LINE, "Line In"));

    g_device.destinations.push_back(speakers);
    g_device.destinations.push_back(recording);
}

// No boost anywhere; the microphones are found by name only ("Mic" in szName), the recording
// fader's bounds are 0..100, and the MUX item is found by "Mic" in its text (its line id resolves
// to no line).
void buildNoBoostDevice()
{
    resetDevice();
    FakeLine speakers = makeLine(0xFFFF0000u, MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, "Speakers");
    FakeLine speakersMic = makeLine(0x00000003u, MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED, "Rear Mic Playback");
    speakersMic.controls.push_back(makeControl(kSpkMicMute, MIXERCONTROL_CONTROLTYPE_MUTE, 1));
    speakers.sources.push_back(speakersMic);

    FakeLine recording = makeLine(0xFFFF0001u, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, "Capture");
    const DWORD ids[2] = { 0x00010000u, 0x00010007u };
    const char* const names[2] = { "Line In", "Front Mic" };
    recording.controls.push_back(makeMux(kRecMux, ids, names, 2, 0));
    recording.sources.push_back(makeLine(0x00010000u, MIXERLINE_COMPONENTTYPE_SRC_LINE, "Line In"));
    FakeLine recordingMic = makeLine(0x00010001u, MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED, "External Mic");
    recordingMic.controls.push_back(makeControl(kRecMicVolume, MIXERCONTROL_CONTROLTYPE_VOLUME, 50, 0, 100));
    recording.sources.push_back(recordingMic);

    g_device.destinations.push_back(speakers);
    g_device.destinations.push_back(recording);
}

// The retail conversions, spelled out independently of VoipMixer.cpp.
float expectedVolume(DWORD value)
{
    const float inverse = 1.0f / 65535.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &inverse, sizeof(bits));
    check(bits == 0x37800080u, "1/65535.0f is ds:920C3C");
    return static_cast<float>(static_cast<double>(value) * static_cast<double>(inverse));
}

void fresh()
{
    VoipMixer::destroy();
}

// --- scenarios ---------------------------------------------------------------------------------

void testNoMixer()
{
    resetDevice();
    g_device.present = false;
    VoipMixer* mixer = VoipMixer::getInstance();
    check(mixer != 0, "getInstance constructs");
    check(VoipMixer::getInstance() == mixer, "getInstance is a singleton");
    check(!mixer->open(), "no mixer: open fails");
    check(g_device.openCalls == 0, "no mixer: caps fail before mixerOpen");
    bool flag = true;
    float volume = -1.0f;
    check(!mixer->findBoostControl(0), "no mixer: no boost");
    check(!mixer->setBoost(true), "no mixer: setBoost fails");
    check(!mixer->getBoost(&flag) && flag, "no mixer: getBoost fails, output untouched");
    check(!mixer->setCaptureVolume(0.5f), "no mixer: setCaptureVolume fails");
    check(!mixer->getCaptureVolume(&volume) && volume == -1.0f, "no mixer: getCaptureVolume fails");
    check(!mixer->selectMicrophoneRecordingSource(), "no mixer: MUX selection fails");
    check(!mixer->getMicrophoneMonitorMute(&flag), "no mixer: getMicrophoneMonitorMute fails");
    check(!mixer->setMicrophoneMonitorMute(true), "no mixer: setMicrophoneMonitorMute fails");
    fresh();
    check(g_device.closedHandles.empty(), "no mixer: destroy closes nothing");

    resetDevice();
    g_device.openFails = true;
    check(!VoipMixer::getInstance()->open(), "mixerOpen failure: open fails");
    check(g_device.openCalls == 1, "mixerOpen was called once");
    fresh();
    check(g_device.closedHandles.empty(), "mixerOpen failure: nothing to close");
}

void testFullDevice()
{
    buildFullDevice();
    VoipMixer* mixer = VoipMixer::getInstance();
    check(mixer->open(), "full device: open");
    check(g_device.openHandles.size() == 1, "one handle open");
    const HMIXER first = g_device.openHandles[0];

    // findLines already searched with a null `out`; later searches answer the cache only.
    VoipMixer::BoostControl info;
    std::memset(&info, 0xCD, sizeof(info));
    check(mixer->findBoostControl(&info), "full device: boost supported");
    check(info.controlType == 0xCDCDCDCDu, "cached search leaves `out` untouched (0x735B7D)");

    bool flag = true;
    check(mixer->getBoost(&flag) && !flag, "boost reads off");
    check(mixer->setBoost(true), "setBoost(true)");
    check(controlValue(kSpkMicBoost) == 1, "LOUDNESS on the speakers' microphone is set");
    check(mixer->getBoost(&flag) && flag, "boost reads on");
    check(mixer->setBoost(false) && controlValue(kSpkMicBoost) == 0, "setBoost(false)");

    // Retail scaling: value = trunc(volume * 65535.0f); volume = value * (1/65535.0f).
    float volume = 0.0f;
    check(mixer->getCaptureVolume(&volume) && volume == expectedVolume(20000), "initial capture volume");
    check(mixer->setCaptureVolume(0.5f) && controlValue(kRecMicVolume) == 32767, "0.5 -> 32767 (truncated)");
    check(mixer->getCaptureVolume(&volume) && volume == expectedVolume(32767), "32767 -> volume");
    check(mixer->setCaptureVolume(0.7f) && controlValue(kRecMicVolume) == 45874, "0.7f -> 45874");
    check(mixer->setCaptureVolume(1.0f) && controlValue(kRecMicVolume) == 65535, "1.0 -> 65535");
    check(mixer->getCaptureVolume(&volume) && volume == expectedVolume(65535) && std::fabs(volume - 1.0f) < 1e-6f,
          "65535 -> 1.0");
    check(mixer->setCaptureVolume(0.0f) && controlValue(kRecMicVolume) == 0, "0.0 -> 0");
    check(mixer->setCaptureVolume(1.5f) && controlValue(kRecMicVolume) == 98302, "1.5 -> 98302 (no clamp)");
    check(mixer->setCaptureVolume(-0.25f) && controlValue(kRecMicVolume) == 0xFFFFC001u, "-0.25 -> -16383 (no clamp)");
    check(mixer->getCaptureVolume(&volume) && volume == expectedVolume(0xFFFFC001u), "negative value reads unsigned");
    check(mixer->setCaptureVolume(std::numeric_limits<float>::quiet_NaN())
          && controlValue(kRecMicVolume) == 0x80000000u, "NaN -> integer indefinite");
    check(mixer->setCaptureVolume(1.0e10f) && controlValue(kRecMicVolume) == 0x80000000u, "overflow -> integer indefinite");
    check(controlValue(kSpkMicVolume) == 30000 && controlValue(kRecCdVolume) == 1000, "other faders untouched");

    // The writes read first (0x7358D0/0x735960): one get per set.
    const unsigned gets = g_device.getDetailsCalls;
    const unsigned sets = g_device.setDetailsCalls;
    check(mixer->setCaptureVolume(0.25f), "setCaptureVolume(0.25)");
    check(g_device.getDetailsCalls == gets + 1 && g_device.setDetailsCalls == sets + 1, "get then set");

    check(mixer->selectMicrophoneRecordingSource(), "MUX: microphone selected");
    check(controlValue(kRecMux, 0) == 0 && controlValue(kRecMux, 1) == 1 && controlValue(kRecMux, 2) == 0,
          "MUX: only the microphone item is set");

    check(mixer->getMicrophoneMonitorMute(&flag) && !flag, "monitor mute reads off");
    check(mixer->setMicrophoneMonitorMute(true) && controlValue(kSpkMicMute) == 1, "monitor mute set");
    check(mixer->getMicrophoneMonitorMute(&flag) && flag, "monitor mute reads on");

    // A failed read aborts the write.
    g_device.failGetDetails = true;
    const unsigned setsBefore = g_device.setDetailsCalls;
    check(!mixer->setBoost(true), "setBoost fails when the read fails");
    check(!mixer->getBoost(&flag), "getBoost fails when the read fails");
    check(!mixer->selectMicrophoneRecordingSource(), "MUX fails when the read fails");
    check(g_device.setDetailsCalls == setsBefore, "no write after a failed read");
    g_device.failGetDetails = false;

    // open() again: the new handle first, then the old one closed (0x736C11).
    check(mixer->open(), "re-open without destroy");
    check(g_device.closedHandles.size() == 1 && g_device.closedHandles[0] == first, "previous handle closed");
    check(g_device.openHandles.size() == 1 && g_device.openHandles[0] != first, "a fresh handle is current");
    check(mixer->setCaptureVolume(0.5f) && controlValue(kRecMicVolume) == 32767, "calls use the fresh handle");
    check(g_device.badHandleCalls == 0, "no call used a closed handle");

    // destroy (0x7356B0) closes the handle and drops the instance; a new instance opens again.
    const HMIXER second = g_device.openHandles[0];
    fresh();
    check(g_device.openHandles.empty() && g_device.closedHandles.back() == second, "destroy closes the handle");
    mixer = VoipMixer::getInstance();
    check(!mixer->setCaptureVolume(0.5f), "new instance before open: not initialised");
    check(mixer->open(), "open after destroy");
    check(mixer->setBoost(true) && controlValue(kSpkMicBoost) == 1, "boost works after re-open");
    fresh();
}

void testNoBoostDevice()
{
    buildNoBoostDevice();
    VoipMixer* mixer = VoipMixer::getInstance();
    check(mixer->open(), "no-boost device: open (microphones found by name)");
    VoipMixer::BoostControl info;
    std::memset(&info, 0xCD, sizeof(info));
    check(!mixer->findBoostControl(&info), "no boost control");
    check(info.controlType == 0xCDCDCDCDu, "cached negative answer leaves `out` untouched");
    bool flag = true;
    check(!mixer->setBoost(true), "setBoost fails without a boost control");
    check(!mixer->getBoost(&flag) && flag, "getBoost fails without a boost control");

    // Retail ignores the fader's bounds (0..100 here) and writes the 0..65535 scale.
    check(mixer->setCaptureVolume(0.5f) && controlValue(kRecMicVolume) == 32767, "bounds ignored on set");
    setControlValue(kRecMicVolume, 100);
    float volume = 0.0f;
    check(mixer->getCaptureVolume(&volume) && volume == expectedVolume(100), "bounds ignored on get");

    check(mixer->selectMicrophoneRecordingSource(), "MUX item found by its text");
    check(controlValue(kRecMux, 0) == 0 && controlValue(kRecMux, 1) == 1, "MUX: \"Front Mic\" selected");
    check(mixer->getMicrophoneMonitorMute(&flag) && flag, "monitor mute on a name-matched microphone");
    fresh();

    // Boost by ONOFF on the wave-in microphone only: the last of the four searches.
    buildFullDevice();
    std::vector<FakeControl>& spk = g_device.destinations[0].sources[1].controls;
    spk.pop_back();                                                     // no LOUDNESS on speakers
    g_device.destinations[1].sources[1].controls.push_back(
        makeControl(kRecMicOnOff, MIXERCONTROL_CONTROLTYPE_ONOFF, 1));
    mixer = VoipMixer::getInstance();
    check(mixer->open(), "ONOFF device: open");
    check(mixer->getBoost(&flag) && flag, "ONOFF boost reads on");
    check(mixer->setBoost(false) && controlValue(kRecMicOnOff) == 0, "ONOFF on the wave-in microphone cleared");
    fresh();

    // ONOFF on the speakers' microphone wins over LOUDNESS on the wave-in microphone.
    buildFullDevice();
    std::vector<FakeControl>& spk2 = g_device.destinations[0].sources[1].controls;
    spk2.back() = makeControl(kSpkMicBoost, MIXERCONTROL_CONTROLTYPE_ONOFF, 0);
    g_device.destinations[1].sources[1].controls.push_back(
        makeControl(kRecMicOnOff, MIXERCONTROL_CONTROLTYPE_LOUDNESS, 0));
    mixer = VoipMixer::getInstance();
    check(mixer->open() && mixer->setBoost(true), "speakers ONOFF device: setBoost");
    check(controlValue(kSpkMicBoost) == 1 && controlValue(kRecMicOnOff) == 0, "speakers ONOFF chosen first");
    fresh();
}

void testIncompleteDevices()
{
    // No speakers destination: findLines fails, the handle stays open until destroy.
    buildFullDevice();
    g_device.destinations.erase(g_device.destinations.begin());
    VoipMixer* mixer = VoipMixer::getInstance();
    check(!mixer->open(), "no speakers destination: open fails");
    check(g_device.openHandles.size() == 1, "the handle is kept");
    check(!mixer->setCaptureVolume(0.5f), "not initialised: setCaptureVolume fails");
    fresh();
    check(g_device.openHandles.empty() && g_device.closedHandles.size() == 1, "destroy closes it");

    // No microphone on the speakers destination.
    buildFullDevice();
    g_device.destinations[0].sources.pop_back();
    check(!VoipMixer::getInstance()->open(), "no speakers microphone: open fails");
    fresh();

    // No microphone on the recording destination.
    buildFullDevice();
    g_device.destinations[1].sources.erase(g_device.destinations[1].sources.begin() + 1);
    g_device.destinations[1].sources.back().line.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_LINE;
    check(!VoipMixer::getInstance()->open(), "no recording microphone: open fails");
    fresh();

    // No MUX: the selection fails, everything else works.
    buildFullDevice();
    g_device.destinations[1].controls.clear();
    mixer = VoipMixer::getInstance();
    check(mixer->open(), "no MUX: open");
    check(!mixer->selectMicrophoneRecordingSource(), "no MUX: selection fails");
    check(mixer->setCaptureVolume(1.0f), "no MUX: volume still works");
    fresh();

    // A MUX without a microphone item.
    buildFullDevice();
    {
        FakeControl& mux = g_device.destinations[1].controls[0];
        mux.list[1].dwParam1 = 0x00010002u;
        std::strcpy(mux.list[1].szName, "Aux");
    }
    mixer = VoipMixer::getInstance();
    check(mixer->open(), "no MUX microphone: open");
    check(!mixer->selectMicrophoneRecordingSource(), "no MUX microphone: selection fails");
    check(controlValue(kRecMux, 0) == 1, "no MUX microphone: nothing written");
    fresh();
}

} // namespace

int main()
{
    // Installed before the first getInstance (VoipMixer::setApiForTesting).
    VoipMixer::setApiForTesting(&kFakeApi);
    check(VoipMixer::api() == &kFakeApi, "the fake table is installed");

    testNoMixer();
    testFullDevice();
    testNoBoostDevice();
    testIncompleteDevices();

    VoipMixer::setApiForTesting(0);
    check(VoipMixer::api() != &kFakeApi, "null restores the platform table");

    std::printf("voip mixer runtime %u/%u checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
