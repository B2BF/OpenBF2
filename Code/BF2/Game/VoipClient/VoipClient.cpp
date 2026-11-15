// BF2.exe 0x736C30..0x737B00 (VoipClient) and 0x737B60..0x737D4E (VoipBandPassFilter). See VoipClient.h.
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <ctime>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "../../External/GameSpy/Voice2/gv.h"
#include "VoipClient.h"
#include "VoipFakeDevice.h"
#include "VoipMixer.h"

#include <cmath>
#include <cstring>

namespace dice {
namespace hfe {

VoipClient*   VoipClient::s_instance = 0;                               // ds:0xA10880
std::uint32_t VoipClient::s_lastKeepAlive = 0;                          // ds:0xA10884

namespace {

// 0x736E0A..0x736E28: fcomp 0.0 then fcomp 1.0; only an ordered value outside [0, 1] is refused.
bool outsideUnitRange(float value) { return value < 0.0f || value > 1.0f; }

// [PLATFORM] Winsock on Windows; BSD sockets elsewhere, with the Winsock-1.1 call shapes kept.
#if defined(_WIN32)
const VoipSocket kInvalidSocket = static_cast<VoipSocket>(INVALID_SOCKET);   // -1, as the ctor stores
int  socketError() { return ::WSAGetLastError(); }                     // IAT 0x87F904
bool isWouldBlock(int error) { return error == WSAEWOULDBLOCK; }       // 0x2733
void closeVoipSocket(VoipSocket s) { ::closesocket(s); }               // 0x660E54
// 0x660B30: ioctlsocket(FIONBIO), the argument being "blocking == false".
void setNonBlocking(VoipSocket s, bool blocking)
{
    u_long nonBlocking = blocking ? 0 : 1;
    ::ioctlsocket(s, FIONBIO, &nonBlocking);
}
// 0x5A0350: Winsock 1.1 startup, the result unused.
void startupSockets()
{
    WSADATA data;
    ::WSAStartup(0x101, &data);
}
typedef int SockLen;
#else
const VoipSocket kInvalidSocket = -1;
int  socketError() { return errno; }
bool isWouldBlock(int error) { return error == EWOULDBLOCK || error == EAGAIN; }
void closeVoipSocket(VoipSocket s) { ::close(s); }
void setNonBlocking(VoipSocket s, bool blocking)
{
    int nonBlocking = blocking ? 0 : 1;
    ::ioctl(s, FIONBIO, &nonBlocking);
}
void startupSockets() {}
typedef socklen_t SockLen;
#endif

// Retail converts the unsigned tick through fild and adds 2^32 when the sign bit is set.
double tickAsDouble(std::uint32_t tick) { return static_cast<double>(tick); }

// The four codec qualities the session offers: BF2 quality 1..5 picks the GameSpy codec from
// low bandwidth up, anything else the low-bandwidth codec (0x736D80's jump table at 0x736DE8).
// [CHOSEN, not recovered] by name: the 2.08 SDK enum gained GVCodecRaw at 0, so retail's values
// 4,3,2,1,0 are SuperLowBandwidth..SuperHighQuality here (Speex quality 1,2,4,7,10 either way).
GVCodec codecForQuality(int quality)
{
    switch (quality)
    {
    case 1:  return GVCodecSuperLowBandwidth;
    case 2:  return GVCodecLowBandwidth;
    case 3:  return GVCodecAverage;
    case 4:  return GVCodecHighQuality;
    case 5:  return GVCodecSuperHighQuality;
    default: return GVCodecLowBandwidth;
    }
}

const int kPacketBuffer = 0x400;    // the receive/capture buffer on the stack
const int kCaptureRoom = 0x3FE;     // gvCapturePacket's room: two bytes are kept for the frame stamp

} // namespace

// ---- VoipBandPassFilter ----------------------------------------------------------------------------

VoipBandPassFilter::VoipBandPassFilter()
    : m_b0(0.0), m_b1(0.0), m_b2(0.0), m_a1(0.0), m_a2(0.0),
      m_x1(0.0), m_x2(0.0), m_y1(0.0), m_y2(0.0),
      m_enabled(false), m_gain(0.0), m_frequency(0.0), m_sampleRate(0.0), m_bandwidth(0.0)
{
}

void VoipBandPassFilter::reset()
{
    m_x2 = 0.0;
    m_x1 = 0.0;
    m_y2 = 0.0;
    m_y1 = 0.0;
}

double VoipBandPassFilter::process(double sample, double limit)
{
    if (!m_enabled)
        return sample;
    const double y = m_b1 * m_x1 + m_b2 * m_x2 + sample * m_b0 - m_a1 * m_y1 - m_a2 * m_y2;
    m_x2 = m_x1;
    m_x1 = sample;
    m_y2 = m_y1;
    m_y1 = y;
    double out = y * m_gain;
    if (!(out < limit))
        out = limit;
    if (out <= -limit)
        out = -limit;
    return out;
}

void VoipBandPassFilter::setup(double gain, double frequency, double sampleRate, double bandwidth,
                               bool enabled)
{
    m_gain = gain;
    m_enabled = enabled;
    m_frequency = frequency;
    m_sampleRate = sampleRate;
    m_bandwidth = bandwidth;
    const double w0 = frequency / sampleRate * 6.283185307179586;           // 0x920CF0
    const double sinW0 = std::sin(w0);
    const double alpha = std::sinh(w0 / sinW0 * bandwidth * 0.34657359027997264) * sinW0;  // ln2/2
    const double inverseA0 = 1.0 / (1.0 + alpha);
    m_b0 = inverseA0 * alpha;
    m_b1 = 0.0 * inverseA0;
    m_b2 = -alpha * inverseA0;
    m_a1 = std::cos(w0) * -2.0 * inverseA0;                                 // 0x920CE0
    m_a2 = (1.0 - alpha) * inverseA0;
    reset();
}

// ---- VoipClient --------------------------------------------------------------------------------------

std::uint32_t VoipClient::tickCount()
{
#if defined(_WIN32)
    return ::GetTickCount();                                            // IAT 0x87F0AC via 0x67CC40
#else
    timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(now.tv_sec) * 1000u
                                      + static_cast<std::uint64_t>(now.tv_nsec) / 1000000u);
#endif
}

VoipClient::VoipClient()
    : m_active(false), m_testing(false), m_playerId(0),
      m_serverAddress(0), m_serverPort(0), m_localPort(0), m_useMixer(true),
      m_quality(0), m_encodedFrameSize(0x14), m_socket(kInvalidSocket),
      m_captureDevice(0), m_playbackDevice(0), m_deviceLost(false), m_headerMode(1), m_channel(0),
      m_initialThreshold(0.1),                                          // 0x912480
      m_sendEnabled(true), m_receiveEnabled(false), m_keepAliveEnabled(true),
      m_keepAliveInterval(3.0f), m_unused60(false), m_capturing(false),
      m_captureLevel(0.0), m_captureStart(0.0), m_lastCapture(0.0), m_voiceTimeout(500.0f),
      m_channelListAllocator(0), m_localLoopback(false), m_loopbackSilent(false),
      m_savedMonitorMute(-1), m_savedBoost(-1)
{
    std::memset(&m_serverSockAddr, 0, sizeof(m_serverSockAddr));
    std::memset(&m_localSockAddr, 0, sizeof(m_localSockAddr));
    s_instance = this;                                                  // 0x737A98
}

VoipClient::~VoipClient()
{
    shutdown();                                                         // 0x737B0C
}

void VoipClient::setServerAddress(std::uint32_t address, std::uint16_t port)
{
    m_serverAddress = address;
    m_serverPort = port;
}

bool VoipClient::openSocket()
{
    startupSockets();
    m_socket = static_cast<VoipSocket>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));   // 0x660E72(2, 2, 17)
    if (m_socket == kInvalidSocket)
        return false;
    setNonBlocking(m_socket, false);
    sockaddr_in local;
    std::memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(m_localPort);
    std::memcpy(m_localSockAddr.bytes, &local, 16);
    if (::bind(m_socket, reinterpret_cast<const sockaddr*>(m_localSockAddr.bytes), 0x10) == -1)   // 0x67BD2A
        return false;
    sockaddr_in server;
    std::memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(m_serverPort);
    server.sin_addr.s_addr = m_serverAddress;
    std::memcpy(m_serverSockAddr.bytes, &server, 16);
    return true;
}

// Starts GameSpy Voice, selects the codec and opens the first capture-capable and the first
// playback-capable device (one device may serve both). Success needs both.
bool VoipClient::initVoice()
{
#if defined(_WIN32)
    if (!gvStartup(0))                                                  // 0x58DBD0, no window
#else
    if (!gvStartup())
#endif
        return false;
    if (!setCodec(m_quality))
        return false;
    if (VoipFakeDevice::requested())
    {
        // [CHOSEN, not recovered] BF2_VOIP_CAPTURE=fake: one synthetic device serves both directions,
        // set up exactly as the loop below sets up a hardware device that offers both.
        GVDevice device = VoipFakeDevice::create();
        if (!device)
            return false;
        setUnpluggedCallback(device, &VoipClient::unpluggedCallback);
        m_captureDevice = device;
        gvSetCaptureThreshold(device, m_initialThreshold);
        gvSetDeviceVolume(device, GV_CAPTURE, 1.0);
        m_playbackDevice = device;
        gvSetDeviceVolume(device, GV_PLAYBACK, 1.0);
        gvStartDevice(device, GV_CAPTURE_AND_PLAYBACK);
        return true;
    }
    GVDeviceInfo devices[10];
    const int count = gvListDevices(devices, 10, GV_CAPTURE_AND_PLAYBACK);   // 0x58DC60
    if (count == 0)
        return false;
    for (int i = 0; i < count; ++i)
    {
        const GVDeviceInfo& info = devices[i];
        int capture = info.m_defaultDevice & GV_CAPTURE;
        int playback = info.m_defaultDevice & GV_PLAYBACK;
        if (!m_captureDevice && (info.m_deviceType & GV_CAPTURE))
            capture = info.m_deviceType & GV_CAPTURE;
        if (!m_playbackDevice && (info.m_deviceType & GV_PLAYBACK))
            playback = info.m_deviceType & GV_PLAYBACK;
        if (!playback && !capture)
            continue;
        GVDeviceType types = 0;
        if (playback && !m_playbackDevice)
            types = GV_PLAYBACK;
        if (capture && !m_captureDevice)
            types |= GV_CAPTURE;
        GVDevice device = gvNewDevice(info.m_id, types);               // 0x58DC70
        if (!device)
            continue;
        setUnpluggedCallback(device, &VoipClient::unpluggedCallback);
        if (types & GV_CAPTURE)
        {
            m_captureDevice = device;
            gvSetCaptureThreshold(device, m_initialThreshold);          // 0x58DD60
            gvSetDeviceVolume(device, GV_CAPTURE, 1.0);                 // 0x58DCB0
        }
        if (types & GV_PLAYBACK)
        {
            m_playbackDevice = device;
            gvSetDeviceVolume(device, GV_PLAYBACK, 1.0);
        }
        gvStartDevice(device, types);                                   // 0x58DCA0
    }
    return m_captureDevice && m_playbackDevice;
}

bool VoipClient::setCodec(int quality)
{
    if (!gvSetCodec(codecForQuality(quality)))                          // 0x58DC10
        return false;
    int samplesPerFrame = 0;
    int bitsPerSecond = 0;
    gvGetCodecInfo(&samplesPerFrame, &m_encodedFrameSize, &bitsPerSecond);   // 0x58DC20
    return true;
}

bool VoipClient::setCaptureThreshold(float threshold)
{
    if (!m_captureDevice || outsideUnitRange(threshold))
        return false;
    gvSetCaptureThreshold(m_captureDevice, threshold);
    return true;
}

bool VoipClient::getCaptureThreshold(double* threshold)
{
    if (!m_captureDevice)
        return false;
    *threshold = gvGetCaptureThreshold(m_captureDevice);                // 0x58DD80
    return true;
}

bool VoipClient::setPlaybackVolume(float volume)
{
    if (!m_playbackDevice || outsideUnitRange(volume))
        return false;
    gvSetDeviceVolume(m_playbackDevice, GV_PLAYBACK, volume);
    return true;
}

bool VoipClient::setCaptureVolume(float volume)
{
    if (!m_captureDevice || outsideUnitRange(volume))
        return false;
    gvSetDeviceVolume(m_captureDevice, GV_CAPTURE, volume);
    return true;
}

double VoipClient::getCaptureVolume()  { return gvGetDeviceVolume(m_captureDevice, GV_CAPTURE); }
double VoipClient::getPlaybackVolume() { return gvGetDeviceVolume(m_playbackDevice, GV_PLAYBACK); }

bool VoipClient::startSession(std::uint32_t address, std::uint16_t port, std::uint16_t localPort,
                              int quality, bool useMixer)
{
    if (m_active)
        return true;
    m_localPort = localPort;
    m_quality = quality;
    m_useMixer = useMixer;
    setServerAddress(address, port);
    if (!openSocket())
        return false;
    if (m_testing)
    {
        // A running test already owns the devices; only the codec follows the session.
        if (!setCodec(m_quality))
            return false;
        m_active = true;
        return true;
    }
    if (m_deviceLost)
        return false;
    if (!initVoice())
        return false;
    if (useMixer)
        initMixer();
    m_active = true;
    return true;
}

bool VoipClient::startTest(int quality, bool useMixer)
{
    if (m_active || m_testing)
        return true;
    m_quality = quality;
    if (m_deviceLost)
        return false;
    if (!initVoice())
        return false;
    m_receiveEnabled = true;
    m_sendEnabled = false;
    if (useMixer)
        initMixer();
    m_testing = true;
    return true;
}

bool VoipClient::isTesting() { return m_testing; }
bool VoipClient::isActive()  { return m_active; }

bool VoipClient::getCaptureLevel(double* level)
{
    *level = m_captureLevel;
    return m_capturing;
}

void VoipClient::setHeaderMode(std::uint8_t mode, std::uint8_t channel)
{
    m_headerMode = mode;
    m_channel = channel;
}

bool VoipClient::useChannelListHeader()
{
    if (m_channelList.empty())
        return false;
    m_headerMode = 3;
    m_channel = 0;
    return true;
}

void VoipClient::setSendEnabled(bool enabled)    { m_sendEnabled = enabled; }
void VoipClient::setReceiveEnabled(bool enabled) { m_receiveEnabled = enabled; }

bool VoipClient::setEnabled(bool enabled)
{
    m_receiveEnabled = enabled;
    m_sendEnabled = enabled;
    return true;
}

void VoipClient::setKeepAliveEnabled(bool enabled) { m_keepAliveEnabled = enabled; }

// A three-byte datagram { session id, 6 } that keeps the server's routing entry for this player.
bool VoipClient::sendKeepAlive()
{
    if (!m_active)
        return false;
    std::uint8_t packet[0x20];
    std::memset(packet, 0, sizeof(packet));
    std::memcpy(packet, &m_playerId, 2);
    packet[2] = 6;
    return ::sendto(m_socket, reinterpret_cast<const char*>(packet), 3, 0,
                    reinterpret_cast<const sockaddr*>(m_serverSockAddr.bytes), 0x10) != -1;   // 0x669E70
}

void VoipClient::setKeepAliveInterval(float seconds) { m_keepAliveInterval = seconds; }
void VoipClient::setPlayerId(std::uint16_t id)       { m_playerId = id; }

bool VoipClient::update()
{
    if (!m_active && !m_testing)
        return false;
    if (m_localLoopback)
        localLoopbackThink();
    else if (m_receiveEnabled)
        think(0, 0);
    if (!m_playbackDevice)
        return false;
    gvThink();                                                          // 0x58DC00
    if (!m_playbackDevice)
        return false;
    const std::uint32_t now = tickCount();
    // 0x7376BA: strictly longer than the timeout.
    if (m_captureStart != 0.0 && m_voiceTimeout < tickAsDouble(now) - m_lastCapture)
        onVoiceEnded();
    keepAliveTick();
    return true;
}

bool VoipClient::shutdown()
{
    if (!m_active && !m_testing)
        return true;
    setEnabled(false);
    m_testing = false;
    m_active = false;
    closeSocket();
    if (m_captureDevice)
        gvStopDevice(m_captureDevice, GV_CAPTURE);                      // 0x7385E0
    if (m_playbackDevice)
        gvStopDevice(m_playbackDevice, GV_PLAYBACK);
    gvCleanup();                                                        // 0x58DBF0
    m_playbackDevice = 0;
    m_captureDevice = 0;
    if (m_savedMonitorMute != -1)
        VoipMixer::getInstance()->setMicrophoneMonitorMute(m_savedMonitorMute != 0);   // 0x7361E0
    if (m_savedBoost != -1)
        VoipMixer::getInstance()->setBoost(m_savedBoost != 0);                         // 0x735BB0
    VoipMixer::destroy();                                               // 0x7356B0
    return true;
}

int VoipClient::sendCaptured(std::uint8_t* packet, int* length)
{
    if (!m_sendEnabled)
        return 0;
    return sendPacket(packet, length);
}

// Incoming datagram: voice frames, then the frame-stamp word and the sender's session-id dword.
// The base plays every sender. Malformed lengths are rejected before reading the trailer.
bool VoipClient::handleIncoming(std::uint8_t* packet, int length, std::uint16_t* sender)
{
    if (!m_playbackDevice)
        return false;
    // [CHOSEN, not recovered] Reject truncated trailers and incomplete codec frames.
    // https://github.com/kr4uzi/OpenBF2/issues/26
    if (length <= 6 || m_encodedFrameSize <= 0 || (length - 6) % m_encodedFrameSize != 0)
        return false;
    std::uint32_t stamp = 0;
    std::memcpy(&stamp, packet + length - 4, 4);
    length -= 4;
    *reinterpret_cast<std::uint32_t*>(sender) = stamp;                  // retail stores the dword
    std::uint16_t source = 0;
    std::memcpy(&source, packet + length - 2, 2);
    length -= 2;
    gvPlayPacket(m_playbackDevice, packet, length, static_cast<GVSource>(stamp),
                 static_cast<GVFrameStamp>(source), GVFalse);           // 0x58DD90
    return true;
}

bool VoipClient::think(int* receiveError, int* sendError)
{
    if ((!m_active && !m_testing) || !m_captureDevice)
        return false;
    if (receiveError)
        *receiveError = 0;
    if (sendError)
        *sendError = 0;
    if (!m_receiveEnabled)
        return false;
    std::uint8_t buffer[kPacketBuffer];
    std::uint16_t sender[2] = { 0, 0 };
    int received;
    do
    {
        sockaddr_in from;
        SockLen fromLength = 0x10;
        received = static_cast<int>(::recvfrom(m_socket, reinterpret_cast<char*>(buffer), kPacketBuffer,
                                               0, reinterpret_cast<sockaddr*>(&from), &fromLength));
        if (received > 0)
        {
            if (handleIncoming(buffer, received, sender))
                onIncomingHandled(sender);
        }
        else if (received == -1)
        {
            const int error = socketError();
            if (receiveError && !isWouldBlock(error))
                *receiveError = error;
            break;
        }
    } while (received > 0);
    for (;;)
    {
        int length = kCaptureRoom;
        GVFrameStamp stamp = 0;
        GVScalar volume = 0.0;
        if (!gvCapturePacket(m_captureDevice, buffer, &length, &stamp, &volume))   // 0x58DD40
            break;
        onCaptureLevel(volume);
        std::memcpy(buffer + length, &stamp, 2);
        length += 2;
        if (sendCaptured(buffer, &length) == -1 && sendError)
        {
            const int error = socketError();
            if (!isWouldBlock(error))
                *sendError = error;
        }
    }
    if (receiveError && *receiveError)
        return false;
    if (sendError && *sendError)
        return false;
    return true;
}

int VoipClient::sendPacket(std::uint8_t* packet, int* length)
{
    if ((!m_active && !m_testing) || !m_sendEnabled)
        return 0;
    bool written;
    switch (m_headerMode)
    {
    case 1:  written = writeHeader(packet, length); break;
    case 2:  written = writeChannelHeader(packet, length); break;
    case 3:  written = writeChannelListHeader(packet, length); break;
    default: written = false; break;
    }
    if (!written)
        return 0;
    return static_cast<int>(::sendto(m_socket, reinterpret_cast<const char*>(packet), *length, 0,
                                     reinterpret_cast<const sockaddr*>(m_serverSockAddr.bytes), 0x10));
}

void VoipClient::onCaptureLevel(double level)
{
    m_capturing = true;
    if (m_captureStart == 0.0)
        m_captureStart = tickAsDouble(tickCount());
    m_lastCapture = tickAsDouble(tickCount());
    m_captureLevel = level;
}

void VoipClient::onVoiceEnded()
{
    m_capturing = false;
    m_captureStart = 0.0;
    m_lastCapture = 0.0;
    m_captureLevel = 0.0;
}

void VoipClient::onIncomingHandled(std::uint16_t*) {}

float VoipClient::getVoiceTimeout()                   { return m_voiceTimeout; }
void  VoipClient::setVoiceTimeout(float milliseconds) { m_voiceTimeout = milliseconds; }

void VoipClient::setUnpluggedCallback(GVDevice device, gvUnpluggedCallback callback)
{
    gvSetUnpluggedCallback(device, callback);                           // 0x58DD10
}

void VoipClient::closeSocket()
{
    if (m_socket == kInvalidSocket)
        return;
    closeVoipSocket(m_socket);
    m_socket = kInvalidSocket;
}

// Opens the mixer, selects the microphone as the recording source at full volume, mutes the
// microphone's speaker monitor and remembers that mute and the boost for shutdown. The answer is the
// monitor-mute call's; both callers ignore it.
bool VoipClient::initMixer()
{
    if (!VoipMixer::getInstance()->open())                              // 0x736BD0
        return false;
    VoipMixer::getInstance()->selectMicrophoneRecordingSource();        // 0x735EA0
    VoipMixer::getInstance()->setCaptureVolume(1.0f);                   // 0x735CD0
    bool muted = false;
    if (VoipMixer::getInstance()->getMicrophoneMonitorMute(&muted))     // 0x736120
        m_savedMonitorMute = muted;
    const bool result = VoipMixer::getInstance()->setMicrophoneMonitorMute(true);  // 0x7361E0(1)
    bool boost = false;
    if (VoipMixer::getInstance()->getBoost(&boost))                     // 0x735D00
        m_savedBoost = boost;
    return result;
}

// Local test without a server: capture is drained (and played back unless silent).
void VoipClient::localLoopbackThink()
{
    if ((!m_active && !m_testing) || !m_captureDevice)
        return;
    std::uint8_t buffer[kPacketBuffer];
    if (m_active)
    {
        sockaddr_in from;
        SockLen fromLength = 0x10;
        ::recvfrom(m_socket, reinterpret_cast<char*>(buffer), kPacketBuffer, 0,
                   reinterpret_cast<sockaddr*>(&from), &fromLength);
    }
    for (;;)
    {
        int length = kCaptureRoom;
        GVFrameStamp stamp = 0;
        GVScalar volume = 0.0;
        if (!gvCapturePacket(m_captureDevice, buffer, &length, &stamp, &volume))
            return;
        onCaptureLevel(volume);
        if (m_loopbackSilent)
            continue;
        std::memcpy(buffer + length, &stamp, 2);
        length += 2;
        std::memset(buffer + length, 0, 4);
        length += 4;
        if (!m_playbackDevice)
            continue;
        std::uint32_t source = 0;
        std::memcpy(&source, buffer + length - 4, 4);
        std::uint16_t frameStamp = 0;
        std::memcpy(&frameStamp, buffer + length - 6, 2);
        gvPlayPacket(m_playbackDevice, buffer, length - 6, static_cast<GVSource>(source),
                     frameStamp, GVFalse);
    }
}

bool VoipClient::writeHeader(std::uint8_t* packet, int* length)
{
    if (!m_active && !m_testing)
        return false;
    std::memcpy(packet + *length, &m_playerId, 2);
    *length += 2;
    packet[*length] = m_headerMode;
    *length += 1;
    return true;
}

bool VoipClient::writeChannelHeader(std::uint8_t* packet, int* length)
{
    if (!m_active && !m_testing)
        return false;
    packet[*length] = m_channel;
    *length += 1;
    std::memcpy(packet + *length, &m_playerId, 2);
    *length += 2;
    packet[*length] = m_headerMode;
    *length += 1;
    return true;
}

// The squad list, its count, the session id and the mode, after the voice frames.
bool VoipClient::writeChannelListHeader(std::uint8_t* packet, int* length)
{
    if ((!m_active && !m_testing) || m_channelList.empty())
        return false;
    for (std::size_t i = 0; i < m_channelList.size(); ++i)
    {
        packet[*length] = m_channelList[i];
        *length += 1;
    }
    packet[*length] = static_cast<std::uint8_t>(m_channelList.size());
    *length += 1;
    std::memcpy(packet + *length, &m_playerId, 2);
    *length += 2;
    packet[*length] = m_headerMode;
    *length += 1;
    return true;
}

bool VoipClient::keepAliveTick()
{
    if (!m_keepAliveEnabled)
        return false;
    const std::uint32_t now = tickCount();
    const std::uint32_t elapsed = now - s_lastKeepAlive;
    if (m_keepAliveInterval * 1000.0f < static_cast<float>(elapsed))        // 0x8AA3C8, strict
    {
        s_lastKeepAlive = now;
        sendKeepAlive();
    }
    return true;
}

void VoipClient::deviceUnplugged(GVDevice device)
{
    if (device == m_captureDevice)
        m_captureDevice = 0;
    if (device == m_playbackDevice)
        m_playbackDevice = 0;
    m_deviceLost = true;
    m_testing = false;
    m_active = false;
}

void VoipClient::unpluggedCallback(GVDevice device)
{
    VoipClient* client = s_instance;
    if (client)
        client->deviceUnplugged(device);
}

} // namespace hfe
} // namespace dice
