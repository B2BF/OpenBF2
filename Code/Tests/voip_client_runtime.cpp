// [TEMP] The voice client (Game/VoipClient): VoipClient's session, headers, capture, playback and
// timers over a real loopback UDP socket, and BF2VoipClient's game rules on fake game objects. The
// capture source is VoipFakeDevice with a manual clock: no microphone, no audio device and no macOS
// microphone permission prompt. Ephemeral ports only.
#include "../BF2/External/GameSpy/Voice2/gv.h"
#include "../BF2/Game/VoipClient/BF2VoipClient.h"
#include "../BF2/Game/VoipClient/VoipFakeDevice.h"
#include "../BF2/Game/VoipClient/VoipMixer.h"

#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/AudioSettings.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Menu/BF2HudManager.h"
#include "../BF2/Menu/HudCommander.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Sound/BfSound.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace dice::hfe;

// The engine globals BF2VoipClient reads; the fixture owns them (data symbols must resolve).
namespace dice { namespace hfe {
class BF2Engine;
IGameLogic* gameLogic = nullptr;
System* g_system = nullptr;
ClassManager* classManager = nullptr;
IGame* game = nullptr;
IBfSound* g_bfSound = nullptr;
BF2Engine* bf2Engine = nullptr;
hud::BF2HudManager* hudManager = nullptr;
BF2VoipClient* bf2VoipClient = nullptr;
namespace world { PlayerManager* playerManager = nullptr; }
} }

namespace {

int g_checks = 0, g_failures = 0;
void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// ---- fake objects through Itanium member pointers --------------------------------------------------
template <class F> std::size_t slot(F f)
{
    std::ptrdiff_t raw[2];
    static_assert(sizeof(f) == sizeof(raw), "Itanium member pointer");
    std::memcpy(raw, &f, sizeof(raw));
    return static_cast<std::size_t>(raw[0] - 1) / sizeof(void*);
}
void unexpected() { std::printf("FAIL unexpected virtual call\n"); std::abort(); }
struct FakeTable
{
    void* entries[512];
    FakeTable() { for (void*& e : entries) e = reinterpret_cast<void*>(&unexpected); }
    template <class F, class G> void set(F member, G function) { entries[slot(member)] = reinterpret_cast<void*>(function); }
};
struct Fake { void** vptr; };
template <class T> T* as(Fake& fake) { return reinterpret_cast<T*>(&fake); }

// ---- the manual clock -----------------------------------------------------------------------------
std::uint32_t g_now = 1000;
std::uint32_t manualClock() { return g_now; }

// ---- the fake voice server --------------------------------------------------------------------------
struct Server
{
    int fd = -1;
    std::uint16_t port = 0;
    Server()
    {
        fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in a; std::memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
        ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a));
        socklen_t len = sizeof(a);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
        ::fcntl(fd, F_SETFL, O_NONBLOCK);
    }
    ~Server() { ::close(fd); }
    std::vector<std::vector<std::uint8_t> > drain()
    {
        std::vector<std::vector<std::uint8_t> > out;
        for (;;)
        {
            std::uint8_t buffer[2048];
            const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0)
                break;
            out.push_back(std::vector<std::uint8_t>(buffer, buffer + n));
        }
        return out;
    }
    void sendTo(std::uint16_t clientPort, const std::vector<std::uint8_t>& packet)
    {
        sockaddr_in a; std::memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(clientPort);
        ::sendto(fd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    }
};

std::uint16_t localPortOf(const VoipClient& client)
{
    sockaddr_in a; socklen_t len = sizeof(a);
    ::getsockname(static_cast<int>(client.m_socket), reinterpret_cast<sockaddr*>(&a), &len);
    return ntohs(a.sin_port);
}

void pause() { ::usleep(20000); }   // let loopback datagrams arrive

// A VoipClient that counts the virtual notifications.
struct CountingClient : VoipClient
{
    int handled = 0;
    int ended = 0;
    void onIncomingHandled(std::uint16_t*) override { ++handled; }
    void onVoiceEnded() override { ++ended; VoipClient::onVoiceEnded(); }
};

// [TEMP] Issue #26: each allocation ends at the supplied length, so ASan can also detect a read
// before a truncated trailer. The virtual call exercises both production parsers.
void malformedIncomingChecks(VoipClient& client, int trailerSize)
{
    const int frameSize = client.m_encodedFrameSize;
    for (int length = -1; length <= trailerSize + 2 * frameSize + 1; ++length)
    {
        if (length == trailerSize + frameSize || length == trailerSize + 2 * frameSize)
            continue;
        std::uint8_t* packet = length > 0 ? new std::uint8_t[length]() : nullptr;
        std::uint16_t sender[2] = { 0x1234, 0x5678 };
        check(!client.handleIncoming(packet, length, sender), "malformed incoming length rejected");
        check(sender[0] == 0x1234 && sender[1] == 0x5678, "malformed packet leaves sender untouched");
        delete[] packet;
    }
    std::uint16_t sender[2] = { 0x1234, 0x5678 };
    check(!client.handleIncoming(nullptr, std::numeric_limits<int>::min(), sender),
          "minimum signed length rejected without subtraction overflow");
    std::vector<std::uint8_t> packet(static_cast<std::size_t>(trailerSize + frameSize), 0);
    for (int invalidSize : { 0, -1 })
    {
        client.m_encodedFrameSize = invalidSize;
        check(!client.handleIncoming(packet.data(), static_cast<int>(packet.size()), sender),
              "invalid codec frame size rejected");
        check(sender[0] == 0x1234 && sender[1] == 0x5678, "invalid codec leaves sender untouched");
    }
    client.m_encodedFrameSize = frameSize;
}

// ---- part A: VoipBandPassFilter ---------------------------------------------------------------------
void filterChecks()
{
    VoipBandPassFilter f;
    f.setup(2.0, 2500.0, 8000.0, 1.75, true);
    const double w0 = 2500.0 / 8000.0 * 6.283185307179586;
    const double alpha = std::sinh(w0 / std::sin(w0) * 1.75 * 0.34657359027997264) * std::sin(w0);
    const double a0 = 1.0 + alpha;
    check(std::fabs(f.m_b0 - alpha / a0) < 1e-12 && f.m_b1 == 0.0 && std::fabs(f.m_b2 + alpha / a0) < 1e-12,
          "filter: band-pass numerator from the cookbook (b1 = 0)");
    check(std::fabs(f.m_a1 + 2.0 * std::cos(w0) / a0) < 1e-12 && std::fabs(f.m_a2 - (1.0 - alpha) / a0) < 1e-12,
          "filter: denominator a1 = -2cos(w0)/a0, a2 = (1-alpha)/a0");
    const double x[4] = { 1000.0, -500.0, 250.0, 0.0 };
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    bool same = true;
    for (double sample : x)
    {
        const double y = f.m_b0 * sample + f.m_b1 * x1 + f.m_b2 * x2 - f.m_a1 * y1 - f.m_a2 * y2;
        x2 = x1; x1 = sample; y2 = y1; y1 = y;
        same &= std::fabs(f.process(sample, 32767.0) - y * 2.0) < 1e-9;
    }
    check(same, "filter: difference equation times the gain");
    f.reset();
    check(f.m_x1 == 0 && f.m_x2 == 0 && f.m_y1 == 0 && f.m_y2 == 0, "filter: reset clears the history");
    f.setup(1000.0, 2500.0, 8000.0, 1.75, true);
    check(f.process(30000.0, 32767.0) == 32767.0, "filter: clamped to the positive limit");
    f.reset();
    check(f.process(-30000.0, 32767.0) == -32767.0, "filter: clamped to the negative limit");
    f.setup(2.0, 2500.0, 8000.0, 1.75, false);
    check(f.process(12345.0, 1.0) == 12345.0, "filter: disabled passes the sample through, unclamped");
}

// ---- part A: VoipClient ------------------------------------------------------------------------------
void clientChecks()
{
    Server server;
    CountingClient client;
    const std::uint32_t loopback = ::inet_addr("127.0.0.1");

    check(client.m_socket == static_cast<VoipSocket>(-1) && client.m_headerMode == 1 && client.m_voiceTimeout == 500.0f
          && client.m_keepAliveInterval == 3.0f && client.m_savedBoost == -1 && client.m_initialThreshold == 0.1,
          "VoipClient ctor: retail defaults");
    check(VoipClient::s_instance == &client, "VoipClient ctor publishes the instance (ds:0xA10880)");
    check(!client.setCaptureThreshold(0.5f), "no capture device: threshold refused");

    check(client.startSession(loopback, server.port, 0, 3, false), "startSession with the fake device");
    check(client.isActive() && !client.isTesting() && client.m_captureDevice && client.m_playbackDevice
          && client.m_captureDevice == client.m_playbackDevice, "one synthetic device serves capture and playback");
    int spf = 0, efs = 0, bps = 0;
    gvGetCodecInfo(&spf, &efs, &bps);
    check(client.m_encodedFrameSize == efs && efs > 0 && spf == 160, "codec quality 3: Speex narrowband frame size");
    check(client.startSession(loopback, server.port, 0, 3, false), "a second startSession is a no-op success");
    malformedIncomingChecks(client, 6);

    client.setPlayerId(0x1234);
    check(client.sendKeepAlive(), "keep-alive sent");
    pause();
    std::vector<std::vector<std::uint8_t> > packets = server.drain();
    check(packets.size() == 1 && packets[0].size() == 3 && packets[0][0] == 0x34 && packets[0][1] == 0x12 && packets[0][2] == 6,
          "keep-alive: { session id, 6 }, three bytes");

    // The 0..1 range checks: only ordered values outside the range are refused.
    check(!client.setCaptureThreshold(-0.25f) && !client.setCaptureThreshold(1.5f), "threshold outside 0..1 refused");
    check(client.setCaptureThreshold(std::numeric_limits<float>::quiet_NaN()), "NaN threshold accepted, as retail");
    check(!client.setPlaybackVolume(2.0f) && client.setPlaybackVolume(0.5f)
          && std::fabs(client.getPlaybackVolume() - 0.5) < 1e-6, "playback volume through gvSetDeviceVolume");
    check(client.setCaptureVolume(1.0f) && std::fabs(client.getCaptureVolume() - 1.0) < 1e-6, "capture volume");

    // Capture: threshold 0, 100 ms of tone, mode-1 headers.
    client.setCaptureThreshold(0.0f);
    client.m_receiveEnabled = true;
    g_now += 100;
    int receiveError = -1, sendError = -1;
    check(client.think(&receiveError, &sendError) && receiveError == 0 && sendError == 0, "think: no socket errors");
    pause();
    packets = server.drain();
    bool mode1 = !packets.empty();
    for (const std::vector<std::uint8_t>& p : packets)
        mode1 &= p.size() == static_cast<std::size_t>(efs + 2 + 3) && p[p.size() - 1] == 1
                 && p[p.size() - 3] == 0x34 && p[p.size() - 2] == 0x12;
    check(packets.size() == 5 && mode1, "five 20 ms frames, each voice + stamp + { id, mode 1 }");
    check(client.m_capturing && client.m_captureStart != 0.0, "capture level marks the burst");

    client.setHeaderMode(2, 7);
    g_now += 20;
    client.think(0, 0);
    pause();
    packets = server.drain();
    check(packets.size() == 1 && packets[0].size() == static_cast<std::size_t>(efs + 2 + 4) && packets[0][efs + 2] == 7
          && packets[0].back() == 2, "mode 2: { channel, id, mode } after the stamp");

    client.m_channelList.push_back(2);
    client.m_channelList.push_back(5);
    check(client.useChannelListHeader() && client.m_headerMode == 3 && client.m_channel == 0, "channel list selects mode 3");
    g_now += 20;
    client.think(0, 0);
    pause();
    packets = server.drain();
    check(packets.size() == 1 && packets[0].size() == static_cast<std::size_t>(efs + 2 + 2 + 1 + 3)
          && packets[0][efs + 2] == 2 && packets[0][efs + 3] == 5 && packets[0][efs + 4] == 2 && packets[0].back() == 3,
          "mode 3: squads, count, id, mode");
    client.setHeaderMode(1, 0);

    client.setSendEnabled(false);
    g_now += 40;
    client.think(0, 0);
    pause();
    check(server.drain().empty(), "send disabled: captured frames are not sent");
    client.setSendEnabled(true);

    // Incoming: one frame, the frame stamp word and the source dword.
    g_now += 20;
    client.think(0, 0);
    pause();
    packets = server.drain();
    std::vector<std::uint8_t> incoming(packets.empty() ? std::vector<std::uint8_t>() : packets[0]);
    incoming.resize(efs > 0 ? static_cast<std::size_t>(efs) : 0);
    incoming.push_back(9); incoming.push_back(0);               // frame stamp 9
    incoming.push_back(42); incoming.push_back(0); incoming.push_back(0); incoming.push_back(0);   // source 42
    const int playedBefore = VoipFakeDevice::playedFrames();
    server.sendTo(localPortOf(client), std::vector<std::uint8_t>(1, 0));
    server.sendTo(localPortOf(client), std::vector<std::uint8_t>(7, 0));
    pause();
    client.think(0, 0);
    check(client.handled == 0, "short and partial-frame datagrams do not notify the base client");
    server.sendTo(localPortOf(client), incoming);
    pause();
    client.think(0, 0);
    check(client.handled == 1, "incoming datagram played and reported (onIncomingHandled)");
    g_now += 60;
    client.think(0, 0);
    check(VoipFakeDevice::playedFrames() > playedBefore, "the fake device drains playback on its clock");
    std::vector<std::uint8_t> twoFrames(incoming.begin(), incoming.begin() + efs);
    twoFrames.insert(twoFrames.end(), incoming.begin(), incoming.end());
    std::uint16_t receivedSender[2] = { 0, 0 };
    check(client.handleIncoming(twoFrames.data(), static_cast<int>(twoFrames.size()), receivedSender)
          && receivedSender[0] == 42 && receivedSender[1] == 0,
          "base client accepts multiple whole frames and preserves the source trailer");
    server.drain();

    // The burst ends once the silence is strictly longer than the 500 ms timeout (VoipClient times bursts
    // on the tick count, not on the device clock). Threshold 1 keeps the device from capturing meanwhile.
    client.setCaptureThreshold(1.0f);
    const int endedBefore = client.ended;
    client.m_captureStart = 1.0;
    client.m_lastCapture = static_cast<double>(VoipClient::tickCount()) - 400.0;
    client.update();
    check(client.ended == endedBefore, "update: inside the timeout the burst continues");
    client.m_lastCapture = static_cast<double>(VoipClient::tickCount()) - 600.0;
    client.update();
    check(client.ended == endedBefore + 1 && !client.m_capturing && client.m_captureStart == 0.0,
          "update: past the timeout ends the burst");
    client.setCaptureThreshold(0.0f);

    // Keep-alive pacing: strictly longer than the interval.
    pause();
    server.drain();
    VoipClient::s_lastKeepAlive = VoipClient::tickCount();
    client.keepAliveTick();
    pause();
    check(server.drain().empty(), "keep-alive: not before the interval");
    client.setKeepAliveInterval(0.0f);
    ::usleep(5000);
    client.keepAliveTick();
    pause();
    check(server.drain().size() == 1, "keep-alive: sent once the interval has passed");
    client.setKeepAliveEnabled(false);
    check(!client.keepAliveTick(), "keep-alive disabled");

    // Shutdown and the unplugged device.
    check(client.shutdown() && !client.isActive() && !client.m_captureDevice && client.m_socket == static_cast<VoipSocket>(-1),
          "shutdown stops the devices and closes the socket");
    check(client.shutdown(), "shutdown twice is harmless");
    check(client.startSession(loopback, server.port, 0, 3, false), "a new session after shutdown");
    VoipClient::unpluggedCallback(client.m_captureDevice);
    check(client.m_deviceLost && !client.isActive() && !client.m_captureDevice && !client.m_playbackDevice,
          "unplugged callback drops both devices and the session");
    client.closeSocket();
    check(!client.startSession(loopback, server.port, 0, 3, false), "a lost device refuses new sessions");
    client.closeSocket();
    gvCleanup();   // the unplugged session never reached shutdown's gvCleanup; the next client starts clean
}

void loopbackChecks()
{
    VoipClient client;
    check(client.setLocalLoopback(true) && client.isTesting() && client.m_localLoopback && !client.m_loopbackSilent,
          "setLocalLoopback starts the local test");
    check(client.m_receiveEnabled && !client.m_sendEnabled, "the test receives but does not send");
    client.setCaptureThreshold(0.0f);
    const int playedBefore = VoipFakeDevice::playedFrames();
    g_now += 100;
    client.update();
    g_now += 100;
    client.update();
    check(VoipFakeDevice::playedFrames() > playedBefore && client.m_capturing, "loopback captures and plays back");
    client.m_loopbackSilent = true;
    check(client.setLocalLoopback(false) && !client.m_localLoopback, "loopback off");
    client.shutdown();
}

// ---- part B: BF2VoipClient on fake game objects ---------------------------------------------------------
struct World
{
    int status = 1;
    bool server = false;
    double time = 100.0;
    bool voipEnabled = true, ptt = true, boost = false;
    float playback = 0.75f, capture = 0.5f, threshold = 0.2f;
    int mode = 0;
    bool squadSelected[10] = {};
    int squadId = 0;
    bool commander = false, leader = false, muted = false;
    double stopTalking = 0.5;
    int setMutedCalls = 0, setTalkingCalls = 0, voipEnabledCalls = 0;
    bool lastTalking = false, lastVoipEnabled = false;
    int duckState = -1;
    bool localExists = true;
    bool senderMuted = false;
    int senderTalking = 0;
} W;

FakeTable gameLogicTable, playerManagerTable, playerTable, senderTable, systemTable, classManagerTable,
          audioTable, hudTable, commanderTable, soundTable;
Fake gameLogicFake, playerManagerFake, playerFake, senderFake, systemFake, classManagerFake, audioFake,
     hudFake, commanderFake, soundFake;

int    glStatus(void*) { return W.status; }
bool   glIsServer(const void*) { return W.server; }
world::IPlayer* pmLocal(void*) { return W.localExists ? as<world::IPlayer>(playerFake) : nullptr; }
world::IPlayer* pmById(void*, int id) { return id == 77 ? as<world::IPlayer>(senderFake) : id == 5 ? as<world::IPlayer>(playerFake) : nullptr; }
int    plSquad(const void*) { return W.squadId; }
bool   plCommander(const void*) { return W.commander; }
bool   plLeader(const void*) { return W.leader; }
bool   plMuted(void*) { return W.muted; }
double plStop(void*) { return W.stopTalking; }
void   plSetMuted(void*, bool m) { W.muted = m; ++W.setMutedCalls; }
void   plSetTalking(void*, bool t) { W.lastTalking = t; ++W.setTalkingCalls; }
void   plSetVoip(void*, bool e) { W.lastVoipEnabled = e; ++W.voipEnabledCalls; }
bool   senderMuted(void*) { return W.senderMuted; }
void   senderTalking(void*, bool) { ++W.senderTalking; }
double sysTime(void*) { return W.time; }
IBase* cmSingleton(void*, const std::string& name) { return name == "AudioSettings" && W.voipEnabled >= 0 ? as<IBase>(audioFake) : nullptr; }
bool   asEnabled(const void*) { return W.voipEnabled; }
float  asPlayback(const void*) { return W.playback; }
float  asCapture(const void*) { return W.capture; }
float  asThreshold(const void*) { return W.threshold; }
bool   asBoost(const void*) { return W.boost; }
bool   asPtt(const void*) { return W.ptt; }
int    hudMode(void*) { return W.mode; }
hud::HudCommander* hudCommander(void*) { return as<hud::HudCommander>(commanderFake); }
bool   cmdSelected(void*, int squad) { return squad >= 0 && squad < 10 && W.squadSelected[squad]; }
void   sndMute(void*, MuteState state, bool on) { W.duckState = static_cast<int>(state) * 10 + (on ? 1 : 0); }

void installWorld()
{
    gameLogicTable.set(&IGameLogic::getGameStatus, &glStatus);
    gameLogicTable.set(&IGameLogic::isServer, &glIsServer);
    gameLogicFake.vptr = gameLogicTable.entries;
    gameLogic = as<IGameLogic>(gameLogicFake);

    playerManagerTable.set(&world::PlayerManager::getLocalHumanPlayer, &pmLocal);
    playerManagerTable.set(&world::PlayerManager::getPlayerFromId, &pmById);
    playerManagerFake.vptr = playerManagerTable.entries;
    world::playerManager = as<world::PlayerManager>(playerManagerFake);

    playerTable.set(&world::IPlayer::getSquadId, &plSquad);
    playerTable.set(&world::IPlayer::getIsCommander, &plCommander);
    playerTable.set(&world::IPlayer::getIsSquadLeader, &plLeader);
    playerTable.set(&world::IPlayer::getIsMuted, &plMuted);
    playerTable.set(&world::IPlayer::getStopTalkingThreshold, &plStop);
    playerTable.set(&world::IPlayer::setIsMuted, &plSetMuted);
    playerTable.set(&world::IPlayer::setIsTalking, &plSetTalking);
    playerTable.set(&world::IPlayer::setVoipEnabled, &plSetVoip);
    playerFake.vptr = playerTable.entries;

    senderTable.set(&world::IPlayer::getIsMuted, &senderMuted);
    senderTable.set(&world::IPlayer::setIsTalking, &senderTalking);
    senderFake.vptr = senderTable.entries;

    systemTable.set(&System::getExactTime, &sysTime);
    systemFake.vptr = systemTable.entries;
    g_system = as<System>(systemFake);

    classManagerTable.set(static_cast<IBase* (ClassManager::*)(const std::string&)>(&ClassManager::getSingleton), &cmSingleton);
    classManagerFake.vptr = classManagerTable.entries;
    classManager = as<ClassManager>(classManagerFake);

    audioTable.set(&IAudioSettings::getVoipEnabled, &asEnabled);
    audioTable.set(&IAudioSettings::getVoipPlaybackVolume, &asPlayback);
    audioTable.set(&IAudioSettings::getVoipCaptureVolume, &asCapture);
    audioTable.set(&IAudioSettings::getVoipCaptureThreshold, &asThreshold);
    audioTable.set(&IAudioSettings::getVoipBoostEnabled, &asBoost);
    audioTable.set(&IAudioSettings::getVoipUsePushToTalk, &asPtt);
    audioFake.vptr = audioTable.entries;

    hudTable.set(&hud::BF2HudManager::getMode, &hudMode);
    hudTable.set(&hud::BF2HudManager::getCommander, &hudCommander);
    hudFake.vptr = hudTable.entries;
    hudManager = as<hud::BF2HudManager>(hudFake);
    commanderTable.set(&hud::HudCommander::isSquadSelected, &cmdSelected);
    commanderFake.vptr = commanderTable.entries;

    soundTable.set(&IBfSound::setMuteState, &sndMute);
    soundFake.vptr = soundTable.entries;
    g_bfSound = as<IBfSound>(soundFake);
    static int engineToken;
    bf2Engine = reinterpret_cast<BF2Engine*>(&engineToken);
}

double captureThreshold(BF2VoipClient& c) { double t = -1; c.getCaptureThreshold(&t); return t; }

void bf2Checks()
{
    installWorld();
    Server server;
    const std::uint32_t loopback = ::inet_addr("127.0.0.1");

    BF2VoipClient client;
    bf2VoipClient = &client;
    check(BF2VoipClient::s_captureFilter && BF2VoipClient::s_captureFilter->m_enabled
          && BF2VoipClient::s_captureFilter->m_gain == 2.0 && BF2VoipClient::s_captureFilter->m_frequency == 2500.0
          && BF2VoipClient::s_captureFilter->m_sampleRate == 8000.0 && BF2VoipClient::s_captureFilter->m_bandwidth == 1.75,
          "BF2VoipClient ctor: the shared capture filter (2.0, 2500 Hz, 8 kHz, 1.75 octaves)");
    check(client.m_pushToTalkEnabled && client.m_internetServer && client.m_talkLimit == 10.0 && client.m_muteDuration == 5.0,
          "BF2VoipClient ctor: push to talk, internet server, 10 s talk limit, 5 s mute");

    check(!client.startSession(0, server.port, 0, 3, true) && client.m_userDisabled, "address 0 disables the user");
    W.voipEnabled = false;
    client.m_userDisabled = false;
    check(!client.startSession(loopback, server.port, 0, 3, true) && client.m_userDisabled, "VoIP off in AudioSettings disables");
    W.voipEnabled = true;
    check(client.startSession(loopback, server.port, 0, 3, true), "session with AudioSettings");
    check(client.m_userDisabled, "a successful start leaves the earlier user-disabled flag (+0xC1) alone");
    client.m_userDisabled = false;
    check(std::fabs(client.getPlaybackVolume() - 0.75) < 1e-6 && std::fabs(client.getCaptureVolume() - 0.5) < 1e-6,
          "AudioSettings volumes reach the devices");
    check(client.m_pushToTalkEnabled && captureThreshold(client) == 1.0, "push to talk closes the gate (threshold 1)");

    check(client.setPushToTalk(true) && client.m_pushToTalkPressed && captureThreshold(client) == 0.0, "PTT press opens the gate");
    client.m_captureStart = 5.0;
    check(client.setPushToTalk(false) && captureThreshold(client) == 1.0 && client.m_captureStart == 0.0,
          "PTT release closes the gate and ends the burst");
    client.m_pushToTalkEnabled = false;
    check(!client.setPushToTalk(true), "PTT ignored when disabled");
    check(client.canTransmit(), "without PTT the client may always transmit");
    client.m_pushToTalkEnabled = true;
    check(!client.canTransmit(), "with PTT released nothing is transmitted");

    // Channels.
    W.leader = false; W.commander = false;
    client.setHeaderMode(2, 1);
    check(!client.updateLeaderChannel(true) && client.m_headerMode == 1 && client.m_channel == 0, "plain soldier: default channel");
    W.leader = true;
    check(client.updateLeaderChannel(true) && client.m_headerMode == 2 && client.m_channel == 1 && client.getUsingLeaderChannel(),
          "squad leader pressed: commander channel (mode 2, channel 1)");
    check(!client.updateLeaderChannel(false) && client.m_headerMode == 1, "squad leader released: default channel");
    W.leader = false; W.commander = true; W.mode = 15;
    check(client.updateLeaderChannel(true) && client.m_headerMode == 1, "commander pressed: every squad leader");
    W.squadSelected[2] = true; W.squadSelected[5] = true;
    client.m_usingLeaderChannel = true;
    check(!client.updateLeaderChannel(false) && client.m_headerMode == 3 && client.m_channelList.size() == 2
          && client.m_channelList[0] == 2 && client.m_channelList[1] == 5 && !client.m_noSquadsSelected,
          "commander released on the map: selected squads (answers the released key, false)");
    W.squadSelected[2] = false; W.squadSelected[5] = false;
    check(!client.updateLeaderChannel(false) && client.m_noSquadsSelected && client.m_channelList.empty(),
          "commander on the map with nothing selected: muted transmit");
    W.mode = 0;
    W.commander = false;

    // setEnabled: pre-game passes, a missing local player outside it disables.
    W.status = 3; W.localExists = false;
    check(client.setEnabled(true) && client.m_sendEnabled, "pre-game: enabled without a local player");
    W.status = 1;
    check(!client.setEnabled(true) && !client.m_sendEnabled && !client.m_receiveEnabled, "in game without a local player: disabled");
    W.localExists = true;
    check(client.setEnabled(true) && client.m_receiveEnabled, "with a local player: enabled");
    client.m_userDisabled = true;
    check(!client.setEnabled(true), "user disabled refuses enable");
    client.m_userDisabled = false;

    // Incoming: muted senders are dropped, others played and marked talking.
    const std::uint16_t clientPort = localPortOf(client);
    int spf = 0, efs = 0, bps = 0;
    gvGetCodecInfo(&spf, &efs, &bps);
    std::vector<std::uint8_t> packet(static_cast<std::size_t>(efs), 0);
    packet.push_back(3); packet.push_back(0);        // frame stamp
    packet.push_back(77); packet.push_back(0);       // sender session 77
    // Rejection must happen before player lookup as well as before decoder submission.
    world::PlayerManager* savedManager = world::playerManager;
    world::playerManager = nullptr;
    malformedIncomingChecks(client, 4);
    world::playerManager = savedManager;
    W.senderMuted = true;
    std::uint16_t sender = 0;
    check(!client.handleIncoming(packet.data(), static_cast<int>(packet.size()), &sender) && sender == 77, "muted sender dropped");
    W.senderMuted = false;
    check(client.handleIncoming(packet.data(), static_cast<int>(packet.size()), &sender), "unmuted sender played");
    packet[packet.size() - 2] = 78;
    check(!client.handleIncoming(packet.data(), static_cast<int>(packet.size()), &sender) && sender == 78,
          "unknown sender remains rejected");
    packet[packet.size() - 2] = 77;
    std::vector<std::uint8_t> twoFrames(static_cast<std::size_t>(efs), 0);
    twoFrames.insert(twoFrames.end(), packet.begin(), packet.end());
    check(client.handleIncoming(twoFrames.data(), static_cast<int>(twoFrames.size()), &sender) && sender == 77,
          "game client accepts multiple whole frames and preserves the sender trailer");
    const int talkingBefore = W.senderTalking;
    server.sendTo(clientPort, std::vector<std::uint8_t>(1, 0));
    server.sendTo(clientPort, std::vector<std::uint8_t>(5, 0));
    pause();
    client.think(0, 0);
    check(W.senderTalking == talkingBefore, "short and partial-frame datagrams do not mark the sender talking");
    server.sendTo(clientPort, packet);
    pause();
    W.squadId = 3;
    client.m_internetServer = false;
    client.think(0, 0);
    check(W.senderTalking == talkingBefore + 1, "a played datagram marks its sender talking (onIncomingHandled)");

    // Capture while PTT is released: levels are seen, nothing is sent; pressed, frames go out.
    client.m_noSquadsSelected = false;
    client.setHeaderMode(1, 0);    // the HUD filter's per-frame updateLeaderChannel would reset it
    server.drain();
    client.setCaptureThreshold(0.0f);
    g_now += 60;
    client.think(0, 0);
    pause();
    check(server.drain().empty(), "PTT released: captured frames are kept back");
    client.m_pushToTalkPressed = true;
    g_now += 60;
    client.think(0, 0);
    pause();
    check(!server.drain().empty(), "PTT pressed: captured frames are sent");

    // The backlog test on internet servers holds capture back while at most 12 frames wait.
    client.m_internetServer = true;
    check(client.captureBacklogLow(), "internet server: a small backlog waits");
    client.m_internetServer = false;
    check(!client.captureBacklogLow(), "LAN server: no backlog wait");

    // Anti-spam: more than ten seconds of continuous talk mutes the local player.
    W.muted = false; W.setMutedCalls = 0;
    client.m_captureStart = 0.0;
    W.time = 200.0;
    client.onCaptureLevel(0.5);
    check(client.m_capturing && client.m_captureStart == 200.0 && W.lastTalking, "talking starts on the simulation clock");
    W.time = 210.0;
    client.onCaptureLevel(0.5);
    check(!client.m_localMuted, "exactly ten seconds does not mute");
    W.time = 210.5;
    client.onCaptureLevel(0.5);
    check(client.m_localMuted && W.muted && client.m_mutedAt == 210.5, "past ten seconds the local player is muted");
    check(client.isBusy() == false, "level 0.5 is not busy");
    client.m_captureLevel = 0.96;
    check(client.isBusy(), "capturing above 0.95 is busy");

    // updateClient: squad gate, unmute when silent, stop-talking threshold, ducking.
    client.m_capturing = false;
    W.squadId = 0; W.commander = false;
    client.updateClient();
    check(!client.m_sendEnabled, "no squad and not commander: transport off");
    check(!W.muted && !client.m_localMuted, "muted while silent: unmuted");
    W.squadId = 4;
    client.m_captureStart = 300.0; client.m_lastCapture = 300.0; W.time = 300.4;
    const int endedTalking = W.setTalkingCalls;
    client.updateClient();
    check(client.m_sendEnabled && client.m_captureStart == 300.0, "in a squad: enabled; inside the stop threshold: still talking");
    W.time = 300.6;
    client.updateClient();
    check(client.m_captureStart == 0.0 && W.setTalkingCalls > endedTalking && !W.lastTalking, "past the stop threshold: talking ends");
    check(W.duckState == 0 || W.duckState == 1, "sound group 0 follows the talking sources");

    // The user switch and the unplugged device.
    W.voipEnabledCalls = 0;
    check(client.setUserDisabled(true) && client.m_userDisabled && !W.lastVoipEnabled && !client.m_sendEnabled,
          "user disable: transport off, player announced off");
    check(client.setUserDisabled(false) && !client.m_userDisabled && W.lastVoipEnabled, "user enable: player announced on");
    BF2VoipClient::unpluggedCallback(client.m_captureDevice);
    check(!client.isActive() && client.m_deviceLost && !W.lastVoipEnabled, "unplugged: session dropped, player announced off");

    // The capture filter on one frame.
    std::vector<std::int16_t> frame(static_cast<std::size_t>(spf));
    for (int i = 0; i < spf; ++i)
        frame[i] = static_cast<std::int16_t>(i % 2 ? 8000 : -8000);
    std::vector<std::int16_t> expected(frame);
    VoipBandPassFilter reference;
    reference.setup(2.0, 2500.0, 8000.0, 1.75, true);
    for (int i = 0; i < spf; ++i)
        expected[i] = static_cast<std::int16_t>(reference.process(expected[i], 32767.0));
    BF2VoipClient::captureFilter(0, frame.data(), 0);
    check(frame == expected, "capture filter: per-frame reset, band-pass, clamp, truncation");

    // Mixer forwards without a platform mixer: retail's failure branches.
    check(!client.mixerOpen() && !client.mixerHasBoost() && !client.mixerSetBoost(true) && !client.mixerGetCaptureVolume(0.5f),
          "mixer forwards answer false without a mixer");
    client.closeSocket();
    gvCleanup();
    bf2VoipClient = nullptr;
}

} // namespace

int main()
{
    ::setenv("BF2_VOIP_CAPTURE", "fake", 1);
    VoipFakeDevice::setClock(&manualClock);
    check(VoipFakeDevice::requested(), "BF2_VOIP_CAPTURE=fake selects the synthetic device");
    filterChecks();
    clientChecks();
    loopbackChecks();
    bf2Checks();
    std::printf("voip client runtime %d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures ? 1 : 0;
}
