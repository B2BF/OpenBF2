// The generic voice-over-IP client BF2VoipClient derives from: GameSpy Voice 2 capture and playback
// devices, the Speex codec selection, and the UDP session with the voice server
// (Code/BF2/VoipServer). BF2.exe 0x736C30..0x737B00, vtable 0x920C48 (38 slots), ctor 0x7379C0.
// [INFERRED] class and member names: the image carries no symbols for this library.
#ifndef BF2_GAME_VOIPCLIENT_VOIPCLIENT_H
#define BF2_GAME_VOIPCLIENT_VOIPCLIENT_H

#include <cstddef>
#include <cstdint>
#include <vector>

// GameSpy Voice 2 (External/GameSpy/Voice2/gv.h). Only the implementation files include gv.h, so
// this header pulls in neither the SDK nor Winsock: the HUD and engine files that see the client
// keep their own include order.
struct GVIDevice;

namespace dice {
namespace hfe {

typedef struct ::GVIDevice* VoipDevice;                       // gv.h GVDevice
typedef void (*VoipUnpluggedCallback)(VoipDevice device);     // gv.h gvUnpluggedCallback

// [PLATFORM] The socket handle: Winsock's SOCKET (UINT_PTR; 4 bytes on x86, as retail), a
// descriptor elsewhere. The two socket addresses are kept as their 16 sockaddr_in bytes.
#if defined(_WIN32)
typedef std::uintptr_t VoipSocket;
#else
typedef int VoipSocket;
#endif
struct VoipSockAddr { std::uint8_t bytes[16]; };

// The second-order band-pass filter BF2VoipClient applies to captured audio (BF2.exe 0x737B60..0x737D4E,
// 0x70 bytes). The coefficients are the audio-EQ-cookbook band-pass with constant 0 dB peak gain,
// bandwidth in octaves.
class VoipBandPassFilter
{
public:
    VoipBandPassFilter();                                               // 0x737B60
    // Clears the four history values (0x737BE0).
    void reset();
    // One sample through the filter, times the gain, clamped to [-limit, limit]. A disabled filter
    // returns the input untouched, without gain or clamp (0x737C10).
    double process(double sample, double limit);
    // Stores the parameters and derives the coefficients; also clears the history (0x737C90).
    void setup(double gain, double frequency, double sampleRate, double bandwidth, bool enabled);

    double m_b0;                    // +0x00
    double m_b1;                    // +0x08 always 0 for this band-pass
    double m_b2;                    // +0x10
    double m_a1;                    // +0x18 a1/a0, subtracted
    double m_a2;                    // +0x20 a2/a0, subtracted
    double m_x1;                    // +0x28
    double m_x2;                    // +0x30
    double m_y1;                    // +0x38
    double m_y2;                    // +0x40
    bool   m_enabled;               // +0x48
    double m_gain;                  // +0x50
    double m_frequency;             // +0x58
    double m_sampleRate;            // +0x60
    double m_bandwidth;             // +0x68
};
#if defined(_M_IX86)
static_assert(sizeof(VoipBandPassFilter) == 0x70, "VoipBandPassFilter must keep the retail x86 size");
#endif

class VoipClient
{
public:
    VoipClient();                                                       // 0x7379C0
    virtual ~VoipClient();                                              // +0x00 0x737B00

    virtual void   setServerAddress(std::uint32_t address, std::uint16_t port);  // +0x04 0x736CD0
    virtual bool   openSocket();                                        // +0x08 0x736CF0
    virtual bool   initVoice();                                         // +0x0C 0x7376F0
    virtual bool   setCodec(int quality);                               // +0x10 0x736D80
    virtual bool   setCaptureThreshold(float threshold);                // +0x14 0x736EA0
    virtual bool   getCaptureThreshold(double* threshold);              // +0x18 0x736EF0
    virtual bool   setPlaybackVolume(float volume);                     // +0x1C 0x736E00
    virtual bool   setCaptureVolume(float volume);                      // +0x20 0x736E50
    virtual double getCaptureVolume();                                  // +0x24 0x451060
    virtual double getPlaybackVolume();                                 // +0x28 0x451070
    virtual bool   startSession(std::uint32_t address, std::uint16_t port, std::uint16_t localPort,
                                int quality, bool useMixer);            // +0x2C 0x7374F0
    virtual bool   startTest(int quality, bool useMixer);               // +0x30 0x737590
    virtual bool   isTesting();                                         // +0x34 0x46CA80 (+0x09)
    virtual bool   isActive();                                          // +0x38 0x46CA70 (+0x08)
    virtual bool   getCaptureLevel(double* level);                      // +0x3C 0x737040
    virtual void   setHeaderMode(std::uint8_t mode, std::uint8_t channel);   // +0x40 0x451080
    virtual bool   useChannelListHeader();                              // +0x44 0x737610
    virtual void   setSendEnabled(bool enabled);                        // +0x48 0x4510A0
    virtual void   setReceiveEnabled(bool enabled);                     // +0x4C 0x4510B0
    virtual bool   setEnabled(bool enabled);                            // +0x50 0x451040
    virtual void   setKeepAliveEnabled(bool enabled);                   // +0x54 0x4510C0
    virtual bool   sendKeepAlive();                                     // +0x58 0x7373C0
    virtual void   setKeepAliveInterval(float seconds);                 // +0x5C 0x5C94A0
    virtual void   setPlayerId(std::uint16_t id);                       // +0x60 0x4510D0
    virtual bool   update();                                            // +0x64 0x737640
    virtual bool   shutdown();                                          // +0x68 0x737440
    virtual int    sendCaptured(std::uint8_t* packet, int* length);     // +0x6C 0x736FA0
    virtual bool   handleIncoming(std::uint8_t* packet, int length, std::uint16_t* sender);  // +0x70 0x736F60
    virtual bool   think(int* receiveError, int* sendError);            // +0x74 0x7370C0
    virtual int    sendPacket(std::uint8_t* packet, int* length);       // +0x78 0x737930
    virtual void   onCaptureLevel(double level);                        // +0x7C 0x736FC0
    virtual void   onVoiceEnded();                                      // +0x80 0x737020
    virtual void   onIncomingHandled(std::uint16_t* sender);            // +0x84 0x5AB2A0 (empty)
    virtual float  getVoiceTimeout();                                   // +0x88 0x4510E0
    virtual void   setVoiceTimeout(float milliseconds);                 // +0x8C 0x65CF00
    virtual void   setUnpluggedCallback(VoipDevice device, VoipUnpluggedCallback callback);  // +0x90 0x736F10
    virtual void   closeSocket();                                       // +0x94 0x737420

    // Loops capture straight back to playback for the menu's voice test; turning it on first starts the
    // local test (quality 3, mixer). Header-inline: SwiffPlayer.dll links its own copy (0x100BC8B0)
    // and calls it on the client it got from getSingleton.
    bool setLocalLoopback(bool on)
    {
        if (on)
            startTest(3, true);                                                 // +0x30
        if (!m_active && !m_testing)
            return false;
        m_localLoopback = on;
        if (on)
            m_loopbackSilent = false;
        return true;
    }

    // Nonvirtual helpers.
    bool initMixer();                                                   // 0x736C30
    void localLoopbackThink();                                          // 0x737240
    bool writeHeader(std::uint8_t* packet, int* length);                // 0x737330, mode 1
    bool writeChannelHeader(std::uint8_t* packet, int* length);         // 0x737370, mode 2
    bool writeChannelListHeader(std::uint8_t* packet, int* length);     // 0x737880, mode 3
    bool keepAliveTick();                                               // 0x737060
    void deviceUnplugged(VoipDevice device);                              // 0x736F30
    static void unpluggedCallback(VoipDevice device);                     // 0x7375E0, through ds:0xA10880

    // [PLATFORM] GetTickCount: Win32 on Windows, a monotonic millisecond count elsewhere.
    static std::uint32_t tickCount();

    // +0x04: no member. MSVC pads the vfptr to the class's 8-byte alignment (the doubles below), which
    // is why retail never writes +0x04.
    bool               m_active;              // +0x08 session started (startSession)
    bool               m_testing;             // +0x09 local test started (startTest)
    std::uint16_t      m_playerId;            // +0x0A voice session id, sent in every header
    std::uint32_t      m_serverAddress;       // +0x0C network order
    std::uint16_t      m_serverPort;          // +0x10
    std::uint16_t      m_localPort;           // +0x12
    bool               m_useMixer;            // +0x14 initial 1
    int                m_quality;             // +0x18 codec quality 1..5
    int                m_encodedFrameSize;    // +0x1C initial 0x14; gvGetCodecInfo
    VoipSocket         m_socket;              // +0x20 initial -1
    VoipSockAddr       m_serverSockAddr;      // +0x24 sockaddr_in
    VoipSockAddr       m_localSockAddr;       // +0x34 sockaddr_in
    VoipDevice           m_captureDevice;       // +0x44
    VoipDevice           m_playbackDevice;      // +0x48
    bool               m_deviceLost;          // +0x4C set by the unplugged callback
    std::uint8_t       m_headerMode;          // +0x4D 1 plain, 2 channel, 3 channel list; initial 1
    std::uint8_t       m_channel;             // +0x4E
    double             m_initialThreshold;    // +0x50 0.1, given to a new capture device
    bool               m_sendEnabled;         // +0x58 initial 1
    bool               m_receiveEnabled;      // +0x59
    bool               m_keepAliveEnabled;    // +0x5A initial 1
    float              m_keepAliveInterval;   // +0x5C seconds, initial 3.0
    bool               m_unused60;            // +0x60 written 0 by the ctor only
    bool               m_capturing;           // +0x61 voice above the threshold since m_captureStart
    double             m_captureLevel;        // +0x68 last capture volume
    double             m_captureStart;        // +0x70 0 while silent
    double             m_lastCapture;         // +0x78
    float              m_voiceTimeout;        // +0x80 milliseconds of silence that end a burst, 500
    std::uint32_t      m_channelListAllocator;// +0x84 [CHOSEN] the VC7.1 vector's allocator word
    std::vector<std::uint8_t> m_channelList;  // +0x88 squads a commander talks to (mode 3)
    bool               m_localLoopback;       // +0x94 capture straight back to playback
    bool               m_loopbackSilent;      // +0x95 loopback captures without playing back
    int                m_savedMonitorMute;    // +0x98 initial -1; the mic's speaker-monitor mute before initMixer
    int                m_savedBoost;          // +0x9C initial -1; the mic boost before initMixer

    static VoipClient* s_instance;                                      // ds:0xA10880
    static std::uint32_t s_lastKeepAlive;                               // ds:0xA10884
};
#if defined(_M_IX86)
static_assert(sizeof(VoipClient) == 0xA0, "VoipClient must keep the retail x86 layout");
static_assert(offsetof(VoipClient, m_captureDevice) == 0x44, "VoipClient +0x44");
static_assert(offsetof(VoipClient, m_channelList) == 0x88, "VoipClient +0x88");
#endif

} // namespace hfe
} // namespace dice

#endif
