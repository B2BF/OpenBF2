// dice::hfe::VoipMixer, the VoIP client's microphone mixer helper. BF2.exe 0x7356B0..0x736BA0,
// table 0x920C40 (one slot: the scalar deleting destructor), 0x17C bytes. [INFERRED] class name;
// the shipped image carries no symbols for it.
//
// A singleton (getInstance, BF2.exe 0x736930; instance ds:A10878) around WINMM mixer device 0
// (handle ds:A1087C). It finds the microphone source on the wave-in (recording) destination and on
// the speakers (playback) destination, then offers: select the microphone on the recording MUX,
// capture volume (the wave-in microphone's VOLUME), microphone boost (a LOUDNESS or ONOFF switch on
// a microphone source), and the playback-monitor mute (MUTE on the speakers' microphone source).
//
// Callers: the VoIP client vtable 0x89C1A8, slots +0xC4..+0xE0 (thunks 0x451A90..0x451B50), and
// the client base's mixer setup 0x736C30 and shutdown 0x737440 (which restores the saved mute and
// boost states and then calls destroy).
//
// [PLATFORM] Every WINMM call goes through a VoipMixerApi table: winmm itself on Windows, a
// CoreAudio shell on macOS (VoipMixerCoreAudio.cpp), a scripted fake in the tests.
#ifndef GAME_VOIPCLIENT_VOIPMIXER_H
#define GAME_VOIPCLIENT_VOIPMIXER_H

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#  include <windows.h>
#  include <mmsystem.h>
#  define BF2_VOIPMIXER_WINAPI WINAPI
#else
#  define BF2_VOIPMIXER_WINAPI
#endif

namespace dice {
namespace hfe {

#if !defined(_WIN32)
// [PLATFORM] The mmsystem.h vocabulary the mixer uses, for platforms without the Windows SDK.
// Byte-packed as mmsystem.h is. The pointer-free structs keep their x86 sizes (MIXERLINEA 0xA8,
// MIXERCONTROLA 0x94, MIXERCAPSA 0x30; retail writes those cbStruct values). DWORD_PTR fields are
// DWORDs for that reason; the two structs that hold pointers are pointer-sized here. The names
// live in dice::hfe, never at global scope (as Platform/macOS/HostWindowTypes.h does).
typedef std::uint32_t        DWORD;
typedef std::uint16_t        WORD;
typedef std::int32_t         LONG;
typedef unsigned int         UINT;
typedef std::uintptr_t       UINT_PTR;
typedef std::uintptr_t       DWORD_PTR;
typedef UINT                 MMRESULT;
typedef UINT                 MMVERSION;
typedef struct HMIXER__*     HMIXER;
typedef struct HMIXEROBJ__*  HMIXEROBJ;

const MMRESULT MMSYSERR_NOERROR      = 0;
const MMRESULT MMSYSERR_ERROR        = 1;
const MMRESULT MMSYSERR_BADDEVICEID  = 2;
const MMRESULT MMSYSERR_ALLOCATED    = 4;
const MMRESULT MMSYSERR_INVALHANDLE  = 5;
const MMRESULT MMSYSERR_NODRIVER     = 6;
const MMRESULT MMSYSERR_NOMEM        = 7;
const MMRESULT MMSYSERR_NOTSUPPORTED = 8;
const MMRESULT MMSYSERR_INVALFLAG    = 10;
const MMRESULT MMSYSERR_INVALPARAM   = 11;
const MMRESULT MIXERR_INVALLINE      = 1024;
const MMRESULT MIXERR_INVALCONTROL   = 1025;
const MMRESULT MIXERR_INVALVALUE     = 1026;

const DWORD CALLBACK_WINDOW       = 0x00010000u;

const DWORD MIXER_OBJECTF_HANDLE  = 0x80000000u;
const DWORD MIXER_OBJECTF_MIXER   = 0x00000000u;
const DWORD MIXER_OBJECTF_HMIXER  = 0x80000000u;

const DWORD MIXER_GETLINEINFOF_DESTINATION   = 0x00000000u;
const DWORD MIXER_GETLINEINFOF_SOURCE        = 0x00000001u;
const DWORD MIXER_GETLINEINFOF_LINEID        = 0x00000002u;
const DWORD MIXER_GETLINEINFOF_COMPONENTTYPE = 0x00000003u;
const DWORD MIXER_GETLINEINFOF_TARGETTYPE    = 0x00000004u;
const DWORD MIXER_GETLINEINFOF_QUERYMASK     = 0x0000000Fu;

const DWORD MIXER_GETLINECONTROLSF_ALL       = 0x00000000u;
const DWORD MIXER_GETLINECONTROLSF_ONEBYID   = 0x00000001u;
const DWORD MIXER_GETLINECONTROLSF_ONEBYTYPE = 0x00000002u;
const DWORD MIXER_GETLINECONTROLSF_QUERYMASK = 0x0000000Fu;

const DWORD MIXER_GETCONTROLDETAILSF_VALUE     = 0x00000000u;
const DWORD MIXER_GETCONTROLDETAILSF_LISTTEXT  = 0x00000001u;
const DWORD MIXER_GETCONTROLDETAILSF_QUERYMASK = 0x0000000Fu;
const DWORD MIXER_SETCONTROLDETAILSF_VALUE     = 0x00000000u;
const DWORD MIXER_SETCONTROLDETAILSF_QUERYMASK = 0x0000000Fu;

const DWORD MIXERLINE_LINEF_ACTIVE = 0x00000001u;
const DWORD MIXERLINE_LINEF_SOURCE = 0x80000000u;

const DWORD MIXERLINE_COMPONENTTYPE_DST_SPEAKERS    = 0x00000004u;
const DWORD MIXERLINE_COMPONENTTYPE_DST_WAVEIN      = 0x00000007u;
const DWORD MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED   = 0x00001000u;
const DWORD MIXERLINE_COMPONENTTYPE_SRC_LINE        = 0x00001002u;
const DWORD MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE  = 0x00001003u;
const DWORD MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC = 0x00001005u;
const DWORD MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT     = 0x00001008u;

const DWORD MIXERLINE_TARGETTYPE_UNDEFINED = 0;
const DWORD MIXERLINE_TARGETTYPE_WAVEOUT   = 1;
const DWORD MIXERLINE_TARGETTYPE_WAVEIN    = 2;

const DWORD MIXERCONTROL_CONTROLF_UNIFORM  = 0x00000001u;
const DWORD MIXERCONTROL_CONTROLF_MULTIPLE = 0x00000002u;

const DWORD MIXERCONTROL_CONTROLTYPE_ONOFF    = 0x20010001u;
const DWORD MIXERCONTROL_CONTROLTYPE_MUTE     = 0x20010002u;
const DWORD MIXERCONTROL_CONTROLTYPE_LOUDNESS = 0x20010004u;
const DWORD MIXERCONTROL_CONTROLTYPE_VOLUME   = 0x50030001u;
const DWORD MIXERCONTROL_CONTROLTYPE_MUX      = 0x70010001u;

#pragma pack(push, 1)
struct MIXERCAPSA
{
    WORD      wMid;
    WORD      wPid;
    MMVERSION vDriverVersion;
    char      szPname[32];
    DWORD     fdwSupport;
    DWORD     cDestinations;
};

struct MIXERLINEA
{
    DWORD cbStruct;
    DWORD dwDestination;
    DWORD dwSource;
    DWORD dwLineID;
    DWORD fdwLine;
    DWORD dwUser;                  // DWORD_PTR in mmsystem.h; x86 width kept
    DWORD dwComponentType;
    DWORD cChannels;
    DWORD cConnections;
    DWORD cControls;
    char  szShortName[16];
    char  szName[64];
    struct
    {
        DWORD     dwType;
        DWORD     dwDeviceID;
        WORD      wMid;
        WORD      wPid;
        MMVERSION vDriverVersion;
        char      szPname[32];
    } Target;
};

struct MIXERCONTROLA
{
    DWORD cbStruct;
    DWORD dwControlID;
    DWORD dwControlType;
    DWORD fdwControl;
    DWORD cMultipleItems;
    char  szShortName[16];
    char  szName[64];
    union
    {
        struct { LONG lMinimum; LONG lMaximum; };
        struct { DWORD dwMinimum; DWORD dwMaximum; };
        DWORD dwReserved[6];
    } Bounds;
    union
    {
        DWORD cSteps;
        DWORD cbCustomData;
        DWORD dwReserved[6];
    } Metrics;
};

struct MIXERLINECONTROLSA
{
    DWORD cbStruct;
    DWORD dwLineID;
    union
    {
        DWORD dwControlID;
        DWORD dwControlType;
    };
    DWORD          cControls;
    DWORD          cbmxctrl;
    MIXERCONTROLA* pamxctrl;
};

struct MIXERCONTROLDETAILS
{
    DWORD cbStruct;
    DWORD dwControlID;
    DWORD cChannels;
    union
    {
        void* hwndOwner;           // HWND in mmsystem.h
        DWORD cMultipleItems;
    };
    DWORD cbDetails;
    void* paDetails;
};

struct MIXERCONTROLDETAILS_LISTTEXTA
{
    DWORD dwParam1;
    DWORD dwParam2;
    char  szName[64];
};

struct MIXERCONTROLDETAILS_BOOLEAN  { LONG fValue; };
struct MIXERCONTROLDETAILS_UNSIGNED { DWORD dwValue; };
#pragma pack(pop)

static_assert(sizeof(MIXERCAPSA) == 0x30, "MIXERCAPSA x86 size");
static_assert(sizeof(MIXERLINEA) == 0xA8, "MIXERLINEA x86 size");
static_assert(sizeof(MIXERCONTROLA) == 0x94, "MIXERCONTROLA x86 size");
static_assert(sizeof(MIXERCONTROLDETAILS_LISTTEXTA) == 0x48, "LISTTEXTA x86 size");
#endif // !_WIN32

#if defined(_M_IX86)
static_assert(sizeof(MIXERCAPSA) == 0x30 && sizeof(MIXERLINEA) == 0xA8 && sizeof(MIXERCONTROLA) == 0x94
              && sizeof(MIXERLINECONTROLSA) == 0x18 && sizeof(MIXERCONTROLDETAILS) == 0x18,
              "the mixer structs retail sizes with cbStruct");
#endif

// [PLATFORM] The seven WINMM entry points the mixer imports (IAT 0x87F8BC..0x87F8D4), with WINMM's
// types and semantics. Windows points them at winmm; see VoipMixer::setApiForTesting.
struct VoipMixerApi
{
    MMRESULT (BF2_VOIPMIXER_WINAPI* open)(HMIXER* phmx, UINT uMxId, DWORD_PTR dwCallback,
                                          DWORD_PTR dwInstance, DWORD fdwOpen);       // mixerOpen
    MMRESULT (BF2_VOIPMIXER_WINAPI* close)(HMIXER hmx);                                // mixerClose
    MMRESULT (BF2_VOIPMIXER_WINAPI* getDevCaps)(UINT_PTR uMxId, MIXERCAPSA* pmxcaps,
                                                UINT cbmxcaps);                        // mixerGetDevCapsA
    MMRESULT (BF2_VOIPMIXER_WINAPI* getLineInfo)(HMIXEROBJ hmxobj, MIXERLINEA* pmxl,
                                                 DWORD fdwInfo);                       // mixerGetLineInfoA
    MMRESULT (BF2_VOIPMIXER_WINAPI* getLineControls)(HMIXEROBJ hmxobj, MIXERLINECONTROLSA* pmxlc,
                                                     DWORD fdwControls);               // mixerGetLineControlsA
    MMRESULT (BF2_VOIPMIXER_WINAPI* getControlDetails)(HMIXEROBJ hmxobj, MIXERCONTROLDETAILS* pmxcd,
                                                       DWORD fdwDetails);              // mixerGetControlDetailsA
    MMRESULT (BF2_VOIPMIXER_WINAPI* setControlDetails)(HMIXEROBJ hmxobj, MIXERCONTROLDETAILS* pmxcd,
                                                       DWORD fdwDetails);              // mixerSetControlDetails
};

#if defined(__APPLE__)
// [PLATFORM] The CoreAudio table (VoipMixerCoreAudio.cpp).
const VoipMixerApi* voipMixerCoreAudioApi();
#endif

// [CHOSEN, not recovered] A std::vector<MIXERLINEA> with the VC7.1 layout (allocator word, first,
// last, end: 0x10 bytes on x86), so the object keeps its 0x17C size under any STL. Only what the
// mixer uses: push_back (BF2.exe 0x736760, VC7.1 growth by half) and release.
class VoipMixerLineList
{
public:
    VoipMixerLineList();
    ~VoipMixerLineList();

    void push_back(const MIXERLINEA& line);
    void release();                                 // free the storage, as the destructor does
    bool empty() const { return m_first == m_last; }
    std::size_t size() const { return m_first ? static_cast<std::size_t>(m_last - m_first) : 0; }
    const MIXERLINEA* begin() const { return m_first; }
    const MIXERLINEA* end() const { return m_last; }

private:
    VoipMixerLineList(const VoipMixerLineList&);
    VoipMixerLineList& operator=(const VoipMixerLineList&);

    std::uint32_t m_allocator;                      // +00, the VC7.1 allocator slot (unused)
    MIXERLINEA*   m_first;                          // +04
    MIXERLINEA*   m_last;                           // +08
    MIXERLINEA*   m_end;                            // +0C
};

class VoipMixer
{
public:
    // The boost control state, +0x18..+0x15C (0x144 bytes). findBoostControl copies it out whole.
    // [INFERRED] names.
    struct BoostControl
    {
        MIXERCONTROLA control;      // +000 (object +018), the switch found on `line`
        MIXERLINEA    line;         // +094 (object +0AC), the microphone source that carries it
        DWORD         controlType;  // +13C (object +154), LOUDNESS or ONOFF
        bool          queried;      // +140 (object +158), set once findBoostControl has searched
        bool          found;        // +141 (object +159)
        unsigned char pad[2];
    };

    VoipMixer();                                            // BF2.exe 0x7367F0
    virtual ~VoipMixer();                                   // BF2.exe 0x736840 (slot 0: 0x736BA0)

    static VoipMixer* getInstance();                        // BF2.exe 0x736930
    // [INFERRED] name. Closes the mixer handle, deletes the instance and clears the pointer.
    static void destroy();                                  // BF2.exe 0x7356B0

    // Opens mixer device 0 (after checking its caps) and finds the lines. On success the previous
    // handle, if any, is closed. Answers findLines.
    bool open();                                            // BF2.exe 0x736BD0
    // [INFERRED] name. Enumerates the destinations, finds the microphone source on the first
    // wave-in and the first speakers destination; true when both were found.
    bool findLines();                                       // BF2.exe 0x736960

    // [INFERRED] name ("boost supported"). Searches once for the boost switch and caches the
    // answer; `out` (may be null) receives the state on the first search only.
    bool findBoostControl(BoostControl* out);               // BF2.exe 0x735A40
    bool setBoost(bool enabled);                            // BF2.exe 0x735BB0
    bool getBoost(bool* enabled);                           // BF2.exe 0x735D00

    // Capture volume, 0..1 mapped to 0..65535 on the wave-in microphone's VOLUME control. Retail
    // neither clamps nor reads the control's bounds.
    bool setCaptureVolume(float volume);                    // BF2.exe 0x735CD0
    bool getCaptureVolume(float* volume);                   // BF2.exe 0x735DD0

    // [INFERRED] name. Selects the microphone in the wave-in destination's MUX.
    bool selectMicrophoneRecordingSource();                 // BF2.exe 0x735EA0

    // [INFERRED] names. MUTE on the speakers destination's microphone source (the playback
    // monitor of the microphone).
    bool getMicrophoneMonitorMute(bool* muted);             // BF2.exe 0x736120
    bool setMicrophoneMonitorMute(bool muted);              // BF2.exe 0x7361E0

    // [PLATFORM] Installs the WINMM table; null restores the platform default. Install it before
    // the first getInstance()/open(). The table is read on every call, and nothing caches it.
    static void setApiForTesting(const VoipMixerApi* api);
    static const VoipMixerApi* api();

private:
    VoipMixer(const VoipMixer&);
    VoipMixer& operator=(const VoipMixer&);

    // [INFERRED] names. Retail passes the line and control by value.
    bool findMicrophoneSource(const MIXERLINEA* destination, MIXERLINEA* source);       // 0x7356F0
    bool getLineControl(const MIXERLINEA& line, DWORD controlType, MIXERCONTROLA* control); // 0x7357D0
    bool setControlBool(const MIXERCONTROLA& control, DWORD channels, bool value);     // 0x7358D0
    bool setControlUnsigned(const MIXERCONTROLA& control, DWORD channels, DWORD value); // 0x735960
    bool setLineVolume(const MIXERLINEA* line, float volume);                          // 0x735C30

    bool              m_initialized;          // +004, set by findLines
    MIXERLINEA*       m_speakersDestination;  // +008, DST_SPEAKERS (4)
    MIXERLINEA*       m_waveInDestination;    // +00C, DST_WAVEIN (7)
    MIXERLINEA*       m_speakersMicrophone;   // +010, microphone source on the speakers
    MIXERLINEA*       m_waveInMicrophone;     // +014, microphone source on the wave-in
    BoostControl      m_boost;                // +018
    VoipMixerLineList m_waveInDestinations;   // +15C, every DST_WAVEIN destination
    VoipMixerLineList m_speakersDestinations; // +16C, every DST_SPEAKERS destination
};

#if defined(_M_IX86)
static_assert(sizeof(VoipMixer::BoostControl) == 0x144, "BoostControl retail size");
static_assert(sizeof(VoipMixerLineList) == 0x10, "VC7.1 vector size");
static_assert(sizeof(VoipMixer) == 0x17C, "VoipMixer retail size (operator new 0x17C, 0x736939)");
#endif

} // namespace hfe
} // namespace dice

#endif // GAME_VOIPCLIENT_VOIPMIXER_H
