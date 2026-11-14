// dice::hfe::VoipMixer, BF2.exe 0x7356B0..0x736BA0 (see the header).
//
// Retail allocates with the engine's operator new/new[] (0x402CD0/0x402CF0) and frees with
// 0x402D10/0x402D20; plain new/delete stand in for them. The "Mic" name tests construct a
// std::string from the name and call find("Mic", 0, 3) (MSVCP71 imports 0x87F46C, 0x87F368).
#include "VoipMixer.h"

#include <cstdlib>
#include <cstring>

#include <cstring>
#include <new>
#include <string>

namespace dice {
namespace hfe {

namespace {

VoipMixer* s_instance = 0;      // ds:A10878
HMIXER     s_mixer = 0;         // ds:A1087C

const float kVolumeScale = 65535.0f;                // ds:8FCBF8 (0x477FFF00)
const float kVolumeInverseScale = 1.0f / 65535.0f;  // ds:920C3C (0x37800080)

inline HMIXEROBJ mixerObject() { return reinterpret_cast<HMIXEROBJ>(s_mixer); }

// "Mic" anywhere in a mixer name (0x735753, 0x736061). [CHOSEN, not recovered] the length is bounded
// by the array; retail reads to the terminator.
bool nameHasMic(const char* name, std::size_t capacity)
{
    std::size_t length = 0;
    while (length < capacity && name[length] != '\0')
        ++length;
    return std::string(name, length).find("Mic", 0, 3) != std::string::npos;
}

// 0x735C76..0x735C9E: fld, fmul 65535.0f, fistp with the rounding control forced to truncate.
// An out-of-range or NaN product stores the integer indefinite. The float product is exact in
// double, so the x87 and SSE arms agree.
DWORD volumeToControlValue(float volume)
{
    const double scaled = static_cast<double>(volume) * static_cast<double>(kVolumeScale);
    if (!(scaled > -2147483649.0 && scaled < 2147483648.0))
        return 0x80000000u;
    return static_cast<DWORD>(static_cast<std::int32_t>(scaled));
}

// 0x735E68..0x735E83: the unsigned value (fild, plus 2^32 when the sign bit is set) times
// 1/65535.0f, stored as float. Precision control is 53 bits in the MSVC runtime, so double then
// float rounds the same way.
float controlValueToVolume(DWORD value)
{
    return static_cast<float>(static_cast<double>(value) * static_cast<double>(kVolumeInverseScale));
}

// [PLATFORM] No mixer: the answers of a machine without a mixer device in retail. Used where no
// backend exists, by test builds, and with the synthetic voice device (BF2_VOIP_CAPTURE=fake), so a
// gate run never changes the machine's microphone volume, mute or boost.
MMRESULT BF2_VOIPMIXER_WINAPI noMixerOpen(HMIXER* phmx, UINT, DWORD_PTR, DWORD_PTR, DWORD)
{
    if (phmx)
        *phmx = 0;
    return MMSYSERR_BADDEVICEID;
}
MMRESULT BF2_VOIPMIXER_WINAPI noMixerClose(HMIXER) { return MMSYSERR_INVALHANDLE; }
MMRESULT BF2_VOIPMIXER_WINAPI noMixerGetDevCaps(UINT_PTR, MIXERCAPSA*, UINT) { return MMSYSERR_BADDEVICEID; }
MMRESULT BF2_VOIPMIXER_WINAPI noMixerGetLineInfo(HMIXEROBJ, MIXERLINEA*, DWORD) { return MMSYSERR_INVALHANDLE; }
MMRESULT BF2_VOIPMIXER_WINAPI noMixerGetLineControls(HMIXEROBJ, MIXERLINECONTROLSA*, DWORD) { return MMSYSERR_INVALHANDLE; }
MMRESULT BF2_VOIPMIXER_WINAPI noMixerGetControlDetails(HMIXEROBJ, MIXERCONTROLDETAILS*, DWORD) { return MMSYSERR_INVALHANDLE; }
MMRESULT BF2_VOIPMIXER_WINAPI noMixerSetControlDetails(HMIXEROBJ, MIXERCONTROLDETAILS*, DWORD) { return MMSYSERR_INVALHANDLE; }

const VoipMixerApi kNoMixerApi = {
    noMixerOpen, noMixerClose, noMixerGetDevCaps, noMixerGetLineInfo,
    noMixerGetLineControls, noMixerGetControlDetails, noMixerSetControlDetails,
};
#if defined(BF2_VOIPMIXER_NO_PLATFORM_BACKEND) || (!defined(_WIN32) && !defined(__APPLE__))
const VoipMixerApi* platformApi() { return &kNoMixerApi; }
#elif defined(_WIN32)
// [PLATFORM] Windows: winmm itself, so behaviour is retail's.
const VoipMixerApi kWinmmApi = {
    ::mixerOpen, ::mixerClose, ::mixerGetDevCapsA, ::mixerGetLineInfoA,
    ::mixerGetLineControlsA, ::mixerGetControlDetailsA, ::mixerSetControlDetails,
};
const VoipMixerApi* platformApi() { return &kWinmmApi; }
#else
const VoipMixerApi* platformApi() { return voipMixerCoreAudioApi(); }
#endif

const VoipMixerApi* s_api = 0;  // [PLATFORM] null: platformApi()

} // namespace

void VoipMixer::setApiForTesting(const VoipMixerApi* api)
{
    s_api = api;
}

const VoipMixerApi* VoipMixer::api()
{
    if (s_api)
        return s_api;
    // [CHOSEN, not recovered] The synthetic voice device (VoipFakeDevice) leaves the system mixer alone.
    const char* capture = std::getenv("BF2_VOIP_CAPTURE");
    if (capture && std::strcmp(capture, "fake") == 0)
        return &kNoMixerApi;
    return platformApi();
}

// --- VoipMixerLineList -------------------------------------------------------------------------

VoipMixerLineList::VoipMixerLineList() : m_allocator(0), m_first(0), m_last(0), m_end(0) {}

VoipMixerLineList::~VoipMixerLineList()
{
    release();
}

void VoipMixerLineList::release()
{
    ::operator delete(m_first);
    m_first = m_last = m_end = 0;
}

// push_back (0x736760) and its _Insert_n growth (0x736420): the capacity grows by half, or to the
// size needed if that is more.
void VoipMixerLineList::push_back(const MIXERLINEA& line)
{
    if (m_last == m_end) {
        const std::size_t count = size();
        const std::size_t capacity = m_first ? static_cast<std::size_t>(m_end - m_first) : 0;
        std::size_t grown = capacity + capacity / 2;
        if (grown < count + 1)
            grown = count + 1;
        MIXERLINEA* storage = static_cast<MIXERLINEA*>(::operator new(grown * sizeof(MIXERLINEA)));
        if (count)
            std::memcpy(storage, m_first, count * sizeof(MIXERLINEA));
        ::operator delete(m_first);
        m_first = storage;
        m_last = storage + count;
        m_end = storage + grown;
    }
    std::memcpy(m_last, &line, sizeof(MIXERLINEA));
    ++m_last;
}

// --- VoipMixer ---------------------------------------------------------------------------------

// BF2.exe 0x7367F0. Retail leaves the boost control and line uninitialised.
VoipMixer::VoipMixer()
    : m_initialized(false), m_speakersDestination(0), m_waveInDestination(0),
      m_speakersMicrophone(0), m_waveInMicrophone(0)
{
    std::memset(&m_boost, 0, sizeof(m_boost));  // [CHOSEN, not recovered] zeroed for determinism
    m_boost.controlType = 0;
    m_boost.queried = false;
    m_boost.found = false;
}

// BF2.exe 0x736840: the four lines, then the two lists (the body releases them, and the member
// destructors find them empty).
VoipMixer::~VoipMixer()
{
    delete m_speakersDestination;
    m_speakersDestination = 0;
    delete m_waveInDestination;
    m_waveInDestination = 0;
    delete m_speakersMicrophone;
    m_speakersMicrophone = 0;
    delete m_waveInMicrophone;
    m_waveInMicrophone = 0;
    m_waveInDestinations.release();
    m_speakersDestinations.release();
}

// BF2.exe 0x736930.
VoipMixer* VoipMixer::getInstance()
{
    if (!s_instance)
        s_instance = new (std::nothrow) VoipMixer();
    return s_instance;
}

// BF2.exe 0x7356B0 (from the client base's shutdown, 0x7374E3).
void VoipMixer::destroy()
{
    if (!s_instance)
        return;
    if (s_mixer) {
        api()->close(s_mixer);
        s_mixer = 0;
    }
    delete s_instance;      // slot 0, the scalar deleting destructor 0x736BA0
    s_instance = 0;
}

// BF2.exe 0x736BD0 (VoIP client slot +0xC4, thunk 0x451A90).
bool VoipMixer::open()
{
    const HMIXER previous = s_mixer;
    MIXERCAPSA caps;
    if (api()->getDevCaps(0, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        return false;
    // CALLBACK_WINDOW with no window: no notifications.
    if (api()->open(&s_mixer, 0, 0, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR)
        return false;
    if (previous)
        api()->close(previous);
    return findLines();
}

// BF2.exe 0x736960. Each pass appends to the lists; only their first entries are used.
bool VoipMixer::findLines()
{
    MIXERCAPSA caps;
    if (api()->getDevCaps(reinterpret_cast<UINT_PTR>(s_mixer), &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        return false;

    MIXERLINEA line;
    for (DWORD destination = 0; destination < caps.cDestinations; ++destination) {
        line.cbStruct = sizeof(MIXERLINEA);
        line.dwDestination = destination;
        if (api()->getLineInfo(mixerObject(), &line, MIXER_GETLINEINFOF_DESTINATION) != MMSYSERR_NOERROR)
            continue;
        if (line.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN)
            m_waveInDestinations.push_back(line);
        else if (line.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS)
            m_speakersDestinations.push_back(line);
    }

    if (m_waveInDestinations.size() == 0 || m_speakersDestinations.size() == 0)
        return false;

    MIXERLINEA destinationLine;
    MIXERLINEA source;
    if (m_waveInDestinations.begin() != m_waveInDestinations.end()) {
        destinationLine = *m_waveInDestinations.begin();
        if (findMicrophoneSource(&destinationLine, &source)) {
            if (!m_waveInDestination)
                m_waveInDestination = new MIXERLINEA;
            *m_waveInDestination = destinationLine;
            if (!m_waveInMicrophone)
                m_waveInMicrophone = new MIXERLINEA;
            *m_waveInMicrophone = source;
        }
    }
    if (m_speakersDestinations.begin() != m_speakersDestinations.end()) {
        destinationLine = *m_speakersDestinations.begin();
        if (findMicrophoneSource(&destinationLine, &source)) {
            if (!m_speakersDestination)
                m_speakersDestination = new MIXERLINEA;
            *m_speakersDestination = destinationLine;
            if (!m_speakersMicrophone)
                m_speakersMicrophone = new MIXERLINEA;
            *m_speakersMicrophone = source;
        }
    }

    if (!m_waveInDestination || !m_speakersDestination)
        return false;
    if (!m_waveInMicrophone && !m_speakersMicrophone)
        return false;
    m_initialized = true;
    findBoostControl(0);
    return m_initialized;
}

// BF2.exe 0x7356F0: the first source of `destination` that is a microphone by component type, or
// whose name holds "Mic".
bool VoipMixer::findMicrophoneSource(const MIXERLINEA* destination, MIXERLINEA* source)
{
    const DWORD connections = destination->cConnections;
    for (DWORD index = 0; index < connections; ++index) {
        MIXERLINEA line;
        line.cbStruct = sizeof(MIXERLINEA);
        line.dwDestination = destination->dwDestination;
        line.dwSource = index;
        if (api()->getLineInfo(mixerObject(), &line, MIXER_GETLINEINFOF_SOURCE) != MMSYSERR_NOERROR)
            continue;
        if (line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE
            || nameHasMic(line.szName, sizeof(line.szName))) {
            *source = line;
            return true;
        }
    }
    return false;
}

// BF2.exe 0x7357D0: all controls of `line`, then the first of `controlType`.
bool VoipMixer::getLineControl(const MIXERLINEA& line, DWORD controlType, MIXERCONTROLA* control)
{
    if (!s_mixer || !m_initialized)
        return false;
    const DWORD count = line.cControls;
    MIXERCONTROLA* controls = new (std::nothrow) MIXERCONTROLA[count];
    if (!controls)
        return false;

    MIXERLINECONTROLSA query;
    query.cbStruct = sizeof(MIXERLINECONTROLSA);
    query.dwLineID = line.dwLineID;
    query.dwControlID = 0;          // [CHOSEN, not recovered] retail leaves it unset (ALL ignores it)
    query.cControls = count;
    query.cbmxctrl = sizeof(MIXERCONTROLA);
    query.pamxctrl = controls;
    if (api()->getLineControls(mixerObject(), &query, MIXER_GETLINECONTROLSF_ALL) != MMSYSERR_NOERROR) {
        delete[] controls;
        return false;
    }

    bool found = false;
    for (DWORD index = 0; index < query.cControls; ++index) {
        if (controls[index].dwControlType == controlType) {
            *control = controls[index];
            found = true;
            break;
        }
    }
    delete[] controls;
    return found;
}

// BF2.exe 0x7358D0: read the value (as validation; it is discarded), then write the boolean.
// The single detail slot is retail's (its callers pass one channel).
bool VoipMixer::setControlBool(const MIXERCONTROLA& control, DWORD channels, bool value)
{
    if (!s_mixer || !m_initialized)
        return false;
    DWORD detail = channels;
    MIXERCONTROLDETAILS details;
    details.cbStruct = sizeof(MIXERCONTROLDETAILS);
    details.dwControlID = control.dwControlID;
    details.cChannels = channels;
    details.cMultipleItems = control.cMultipleItems;
    details.cbDetails = sizeof(DWORD);
    details.paDetails = &detail;
    if (api()->getControlDetails(mixerObject(), &details, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
        return false;
    detail = value ? 1u : 0u;
    return api()->setControlDetails(mixerObject(), &details, MIXER_SETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR;
}

// BF2.exe 0x735960: as setControlBool, with an unsigned value.
bool VoipMixer::setControlUnsigned(const MIXERCONTROLA& control, DWORD channels, DWORD value)
{
    if (!s_mixer || !m_initialized)
        return false;
    DWORD detail = channels;
    MIXERCONTROLDETAILS details;
    details.cbStruct = sizeof(MIXERCONTROLDETAILS);
    details.dwControlID = control.dwControlID;
    details.cChannels = channels;
    details.cMultipleItems = control.cMultipleItems;
    details.cbDetails = sizeof(DWORD);
    details.paDetails = &detail;
    if (api()->getControlDetails(mixerObject(), &details, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
        return false;
    detail = value;
    return api()->setControlDetails(mixerObject(), &details, MIXER_SETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR;
}

// BF2.exe 0x735A40 (VoIP client slot +0xC8, thunk 0x451AA0 with a null `out`). The search order:
// LOUDNESS, then ONOFF, on the speakers' microphone source; then the same on the wave-in's.
bool VoipMixer::findBoostControl(BoostControl* out)
{
    if (!s_mixer || !m_initialized)
        return false;
    if (!m_waveInMicrophone && !m_speakersMicrophone)
        return false;
    if (m_boost.queried)
        return m_boost.found;
    m_boost.queried = true;

    if (!m_waveInMicrophone || !m_speakersMicrophone) {
        m_boost.found = false;
        if (out)
            *out = m_boost;
        return false;
    }

    const MIXERLINEA* line = 0;
    DWORD type = 0;
    if (getLineControl(*m_speakersMicrophone, MIXERCONTROL_CONTROLTYPE_LOUDNESS, &m_boost.control)) {
        line = m_speakersMicrophone;
        type = MIXERCONTROL_CONTROLTYPE_LOUDNESS;
    } else if (getLineControl(*m_speakersMicrophone, MIXERCONTROL_CONTROLTYPE_ONOFF, &m_boost.control)) {
        line = m_speakersMicrophone;
        type = MIXERCONTROL_CONTROLTYPE_ONOFF;
    } else if (getLineControl(*m_waveInMicrophone, MIXERCONTROL_CONTROLTYPE_LOUDNESS, &m_boost.control)) {
        line = m_waveInMicrophone;
        type = MIXERCONTROL_CONTROLTYPE_LOUDNESS;
    } else if (getLineControl(*m_waveInMicrophone, MIXERCONTROL_CONTROLTYPE_ONOFF, &m_boost.control)) {
        line = m_waveInMicrophone;
        type = MIXERCONTROL_CONTROLTYPE_ONOFF;
    }
    if (line) {
        m_boost.controlType = type;
        m_boost.line = *line;
        m_boost.found = true;
    }
    if (out)
        *out = m_boost;
    return m_boost.found;
}

// BF2.exe 0x735BB0 (VoIP client slot +0xCC, thunk 0x451AB0; shutdown 0x7374DE restores the saved
// state). The control is fetched again into the cached slot before the write.
bool VoipMixer::setBoost(bool enabled)
{
    if (!s_mixer || !m_initialized)
        return false;
    if (!findBoostControl(0))
        return false;
    if (!getLineControl(m_boost.line, m_boost.controlType, &m_boost.control))
        return false;
    return setControlBool(m_boost.control, 1, enabled);
}

// BF2.exe 0x735C30: `volume` times 65535, truncated; no clamp, the control's bounds unused.
bool VoipMixer::setLineVolume(const MIXERLINEA* line, float volume)
{
    if (!s_mixer || !m_initialized)
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(*line, MIXERCONTROL_CONTROLTYPE_VOLUME, &control))
        return false;
    return setControlUnsigned(control, 1, volumeToControlValue(volume));
}

// BF2.exe 0x735CD0 (VoIP client slot +0xD0, thunk 0x451AD0; the setup 0x736C64 passes 1.0).
bool VoipMixer::setCaptureVolume(float volume)
{
    if (!s_mixer || !m_initialized || !m_waveInMicrophone)
        return false;
    return setLineVolume(m_waveInMicrophone, volume);
}

// BF2.exe 0x735D00 (VoIP client slot +0xD4, thunk 0x451AF0; the setup 0x736CAB saves it).
bool VoipMixer::getBoost(bool* enabled)
{
    if (!s_mixer || !m_initialized)
        return false;
    if (!findBoostControl(0))
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(m_boost.line, m_boost.controlType, &control))
        return false;
    DWORD value = 0;
    MIXERCONTROLDETAILS details;
    details.cbStruct = sizeof(MIXERCONTROLDETAILS);
    details.dwControlID = control.dwControlID;
    details.cChannels = 1;
    details.cMultipleItems = control.cMultipleItems;
    details.cbDetails = sizeof(DWORD);
    details.paDetails = &value;
    if (api()->getControlDetails(mixerObject(), &details, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
        return false;
    *enabled = value != 0;
    return true;
}

// BF2.exe 0x735DD0 (VoIP client slot +0xD8, thunk 0x451B10). The control value over 65535; the
// control's bounds are not read. The thunk takes its float by value and passes that slot's
// address, so the client slot only reports success; the volume read is discarded.
bool VoipMixer::getCaptureVolume(float* volume)
{
    if (!s_mixer || !m_initialized || !m_waveInMicrophone)
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(*m_waveInMicrophone, MIXERCONTROL_CONTROLTYPE_VOLUME, &control))
        return false;
    DWORD value = 0;
    MIXERCONTROLDETAILS details;
    details.cbStruct = sizeof(MIXERCONTROLDETAILS);
    details.dwControlID = control.dwControlID;
    details.cChannels = 1;
    details.cMultipleItems = control.cMultipleItems;
    details.cbDetails = sizeof(DWORD);
    details.paDetails = &value;
    if (api()->getControlDetails(mixerObject(), &details, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
        return false;
    *volume = controlValueToVolume(value);
    return true;
}

// BF2.exe 0x735EA0 (VoIP client slot +0xDC, thunk 0x451B30; the setup 0x736C53). Finds the MUX of
// the wave-in destination, picks the item whose line is a microphone (by component type, then by
// "Mic" in the item text) and sets that item alone. The set's result is not checked.
bool VoipMixer::selectMicrophoneRecordingSource()
{
    if (!s_mixer || !m_initialized || !m_waveInDestination)
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(*m_waveInDestination, MIXERCONTROL_CONTROLTYPE_MUX, &control))
        return false;

    const DWORD channels = (control.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM) ? 1u : m_waveInDestination->cChannels;
    const DWORD items = control.cMultipleItems;
    // [CHOSEN, not recovered] Sized items * channels: retail allocates `items` slots, which WINMM
    // overruns for a non-uniform MUX on a multi-channel line. Retail does not check this allocation.
    const DWORD slots = items * (channels ? channels : 1u);
    MIXERCONTROLDETAILS_BOOLEAN* selected = new (std::nothrow) MIXERCONTROLDETAILS_BOOLEAN[slots ? slots : 1u];
    if (!selected)
        return false;

    MIXERCONTROLDETAILS values;
    values.cbStruct = sizeof(MIXERCONTROLDETAILS);
    values.dwControlID = control.dwControlID;
    values.cChannels = channels;
    values.cMultipleItems = items;
    values.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    values.paDetails = selected;
    if (api()->getControlDetails(mixerObject(), &values, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR) {
        delete[] selected;
        return false;
    }

    MIXERCONTROLDETAILS_LISTTEXTA* texts = new (std::nothrow) MIXERCONTROLDETAILS_LISTTEXTA[items ? items : 1u];
    if (!texts) {
        delete[] selected;
        return false;
    }
    MIXERCONTROLDETAILS listText;
    listText.cbStruct = sizeof(MIXERCONTROLDETAILS);
    listText.dwControlID = control.dwControlID;
    listText.cChannels = channels;
    listText.cMultipleItems = items;
    listText.cbDetails = sizeof(MIXERCONTROLDETAILS_LISTTEXTA);
    listText.paDetails = texts;
    if (api()->getControlDetails(mixerObject(), &listText, MIXER_GETCONTROLDETAILSF_LISTTEXT) != MMSYSERR_NOERROR) {
        delete[] texts;
        delete[] selected;
        return false;
    }

    // dwParam1 of each item is the source's line id.
    DWORD microphone = 0xFFFFFFFFu;
    for (DWORD index = 0; index < items; ++index) {
        MIXERLINEA line;
        line.cbStruct = sizeof(MIXERLINEA);
        line.dwLineID = texts[index].dwParam1;
        if (api()->getLineInfo(mixerObject(), &line, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_LINEID) == MMSYSERR_NOERROR
            && line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE) {
            microphone = index;
            break;
        }
    }
    if (microphone == 0xFFFFFFFFu) {
        for (DWORD index = 0; index < items; ++index) {
            if (nameHasMic(texts[index].szName, sizeof(texts[index].szName))) {
                microphone = index;
                break;
            }
        }
    }
    if (microphone == 0xFFFFFFFFu) {
        delete[] texts;
        delete[] selected;
        return false;
    }

    for (DWORD index = 0; index < items; ++index)
        selected[index].fValue = 0;
    selected[microphone].fValue = 1;
    api()->setControlDetails(mixerObject(), &values, MIXER_SETCONTROLDETAILSF_VALUE);
    delete[] texts;
    delete[] selected;
    return true;
}

// BF2.exe 0x736120 (the setup 0x736C78 saves it to the client's +0x98).
bool VoipMixer::getMicrophoneMonitorMute(bool* muted)
{
    if (!s_mixer || !m_initialized || !m_speakersMicrophone)
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(*m_speakersMicrophone, MIXERCONTROL_CONTROLTYPE_MUTE, &control))
        return false;
    DWORD value = 0;
    MIXERCONTROLDETAILS details;
    details.cbStruct = sizeof(MIXERCONTROLDETAILS);
    details.dwControlID = control.dwControlID;
    details.cChannels = 1;
    details.cMultipleItems = control.cMultipleItems;
    details.cbDetails = sizeof(DWORD);
    details.paDetails = &value;
    if (api()->getControlDetails(mixerObject(), &details, MIXER_GETCONTROLDETAILSF_VALUE) != MMSYSERR_NOERROR)
        return false;
    *muted = value != 0;
    return true;
}

// BF2.exe 0x7361E0 (VoIP client slot +0xE0, thunk 0x451B40; the setup 0x736C95 mutes, and the
// shutdown 0x7374BB restores the saved state).
bool VoipMixer::setMicrophoneMonitorMute(bool muted)
{
    if (!s_mixer || !m_initialized || !m_speakersMicrophone)
        return false;
    MIXERCONTROLA control;
    if (!getLineControl(*m_speakersMicrophone, MIXERCONTROL_CONTROLTYPE_MUTE, &control))
        return false;
    return setControlBool(control, 1, muted);
}

} // namespace hfe
} // namespace dice
