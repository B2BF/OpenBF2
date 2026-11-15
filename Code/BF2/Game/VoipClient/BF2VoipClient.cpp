// BF2.exe 0x4510F0..0x45235C. See BF2VoipClient.h.
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "../../External/GameSpy/Voice2/gv.h"
#include "BF2VoipClient.h"
#include "VoipMixer.h"

#include <cstring>
#include <sstream>
#include <string>

#include "../../Core/Ids.h"                                // IID_IGameServer / IID_IGameClient
#include "../../Core/SmartItf.h"                           // classManager (ds:0x9FF960)
#include "../../Core/System.h"                             // g_system (ds:0x99E2B8)
#include "../../IO/Network/NetworkLayer/Socket/UDPSocket.h"  // nameToAddress (0x6AD8B0)
#include "../../IO/Network/ProtocolLayer/NetServer.h"      // getServerAddress
#include "../../Menu/BF2HudManager.h"                      // getMode / getCommander
#include "../../Menu/HudCommander.h"                       // isSquadSelected
#include "../../Scene/Object/PlayerManager.h"              // world::playerManager (ds:0xA08F60)
#include "../../Sound/BfSound.h"                           // g_bfSound (ds:0xA23DC0)
#include "../AudioSettings.h"                              // IAudioSettings
#include "../IGame.h"                                      // game (ds:0x99E348)
#include "../IGameClient.h"
#include "../IGameLogic.h"                                 // gameLogic (ds:0x99EF80)
#include "../IGameServer.h"
#include "../Main/BF2ClientSeams.h"                        // hudManager (ds:0xA10890)
#include "../Objects/IPlayer.h"

namespace dice {
namespace hfe {

class BF2Engine;
extern BF2Engine* bf2Engine;                               // Core/Globals.cpp; ds:0x986F48

VoipBandPassFilter* BF2VoipClient::s_captureFilter = 0;    // ds:0x9A4DA4

namespace {

const int kGameStatusPreGame = 3;                          // GameStatus_PreGame
const int kHudModeCommanderMap = 0xF;                      // BF2HudManager::getMode, the commander screen

world::IPlayer* localPlayer()
{
    return world::playerManager->getLocalHumanPlayer();    // +0x30
}

double now()
{
    return g_system->getExactTime();                       // ds:0x99E2B8 +0x14
}

IAudioSettings* audioSettings()
{
    return static_cast<IAudioSettings*>(classManager->getSingleton(std::string("AudioSettings")));
}

} // namespace

BF2VoipClient::BF2VoipClient()
    : m_usingLeaderChannel(false), m_pushToTalkPressed(false), m_pushToTalkEnabled(true),
      m_localMuted(false), m_talkLimit(10.0), m_mutedAt(0.0), m_muteDuration(5.0),
      m_noSquadsSelected(false), m_userDisabled(false), m_internetServer(true)
{
    // 0x451A11: one filter, shared by every client and created by the first.
    if (!s_captureFilter)
        s_captureFilter = new VoipBandPassFilter();
    s_captureFilter->setup(2.0, 2500.0, 8000.0, 1.75, true);   // 0x89C180 / 0x89C188 / 0x89C190 / 0x89C198
}

BF2VoipClient::~BF2VoipClient()
{
    if (s_captureFilter)
    {
        delete s_captureFilter;
        s_captureFilter = 0;
    }
}

// Each captured frame goes through the band-pass filter (its history cleared per frame), then is
// clamped to 16 bits and truncated back into the buffer.
void BF2VoipClient::captureFilter(VoipDevice, std::int16_t* audio, std::uint16_t)
{
    s_captureFilter->reset();
    int samplesPerFrame = 0;
    gvGetCodecInfo(&samplesPerFrame, 0, 0);
    for (int i = 0; i < samplesPerFrame; ++i)
        audio[i] = static_cast<std::int16_t>(s_captureFilter->process(static_cast<double>(audio[i]), 32767.0));
}

bool BF2VoipClient::setEnabled(bool enabled)
{
    if (!m_active)
        return false;
    if (enabled && m_userDisabled)
        return false;
    if (gameLogic && gameLogic->getGameStatus() != kGameStatusPreGame && !localPlayer())
    {
        m_receiveEnabled = false;
        m_sendEnabled = false;
        return false;
    }
    m_receiveEnabled = enabled;
    m_sendEnabled = enabled;
    return true;
}

// Disabling turns the transport off and tells the server through the local player; enabling starts
// the session again when it is not running (outside the pre-game) and re-announces the player.
bool BF2VoipClient::setUserDisabled(bool disabled)
{
    world::IPlayer* local = world::playerManager ? localPlayer() : 0;
    if (disabled)
    {
        m_userDisabled = disabled;
        setEnabled(false);
        if (local)
            local->setVoipEnabled(false);                  // +0x28C
        return true;
    }
    if (!m_active && gameLogic && gameLogic->getGameStatus() != kGameStatusPreGame)
    {
        if (!startSession(m_serverAddress, m_serverPort, m_localPort, m_quality, m_useMixer))
            return false;
    }
    m_userDisabled = false;
    if (local)
        local->setVoipEnabled(true);
    return true;
}

// A squad leader talks on the commander channel and a commander on the leaders' channel; anybody
// else always uses the default channel.
bool BF2VoipClient::updateLeaderChannel(bool pressed)
{
    if (!m_active)
        return false;
    world::IPlayer* local = localPlayer();                 // no playerManager test, as shipped
    if (!local)
        return false;
    if (!local->getIsSquadLeader() && !local->getIsCommander())
    {
        m_usingLeaderChannel = false;
        setHeaderMode(1, 0);
        return m_usingLeaderChannel;
    }
    if (local->getIsSquadLeader())
        return squadLeaderChannel(pressed);
    if (local->getIsCommander())
        return commanderChannel(pressed);
    m_usingLeaderChannel = false;
    return false;
}

bool BF2VoipClient::getUsingLeaderChannel()        { return m_usingLeaderChannel; }
bool BF2VoipClient::getPushToTalkEnabled()         { return m_pushToTalkEnabled; }
void BF2VoipClient::setPushToTalkEnabled(bool on)  { m_pushToTalkEnabled = on; }
bool BF2VoipClient::getPushToTalkPressed()         { return m_pushToTalkPressed; }

// Push to talk opens the capture gate (threshold 0) on press and closes it (threshold 1) on release.
bool BF2VoipClient::setPushToTalk(bool pressed)
{
    if (!m_pushToTalkEnabled)
        return false;
    if (!m_pushToTalkPressed && pressed)
        setCaptureThreshold(0.0f);
    if (m_pushToTalkPressed && !pressed)
    {
        setCaptureThreshold(1.0f);
        onVoiceEnded();
    }
    m_pushToTalkPressed = pressed;
    return true;
}

bool BF2VoipClient::squadLeaderChannel(bool pressed)
{
    if (!m_active)
        return false;
    m_usingLeaderChannel = pressed;
    if (pressed)
        setHeaderMode(2, 1);
    else
        setHeaderMode(1, 0);
    return m_usingLeaderChannel;
}

// The commander's key held talks to every squad leader (default channel, mode 1); released, the
// commander screen's selected squads become the channel list (mode 3).
bool BF2VoipClient::commanderChannel(bool pressed)
{
    if (!m_active)
        return false;
    m_usingLeaderChannel = pressed;
    const bool onMap = hudManager->getMode() == kHudModeCommanderMap;      // +0x1E0
    if (pressed)
    {
        m_noSquadsSelected = false;
        setHeaderMode(1, 0);
        return m_usingLeaderChannel;
    }
    // The vector is released, not cleared (0x4522AB..0x4522D1).
    std::vector<std::uint8_t>().swap(m_channelList);
    if (onMap)
    {
        for (int squad = 1; squad < 10; ++squad)
        {
            if (hudManager->getCommander()->isSquadSelected(squad))         // +0x258, +0x30
                m_channelList.push_back(static_cast<std::uint8_t>(squad));
        }
    }
    if (m_channelList.empty())
    {
        if (onMap)
            m_noSquadsSelected = true;
        m_usingLeaderChannel = false;
        return false;
    }
    m_noSquadsSelected = false;
    useChannelListHeader();
    return m_usingLeaderChannel;
}

void BF2VoipClient::setLocalMuted(bool muted)
{
    if (!m_active)
        return;
    world::IPlayer* local = localPlayer();
    if (!local)
        return;
    local->setIsMuted(muted);                              // +0x284
    m_localMuted = muted;
    m_mutedAt = muted ? now() : 0.0;
}

// [INFERRED] name: mutes the local player and stores the duration (0x451880).
void BF2VoipClient::muteFor(double seconds)
{
    if (!m_active)
        return;
    setLocalMuted(true);
    m_muteDuration = seconds;
}

void BF2VoipClient::slotC0(int, int) {}

bool BF2VoipClient::mixerOpen()                       { return VoipMixer::getInstance()->open(); }
bool BF2VoipClient::mixerHasBoost()                   { return VoipMixer::getInstance()->findBoostControl(0); }
bool BF2VoipClient::mixerSetBoost(bool enabled)       { return VoipMixer::getInstance()->setBoost(enabled); }
bool BF2VoipClient::mixerSetCaptureVolume(float v)    { return VoipMixer::getInstance()->setCaptureVolume(v); }
bool BF2VoipClient::mixerGetBoost(bool* enabled)      { return VoipMixer::getInstance()->getBoost(enabled); }
bool BF2VoipClient::mixerGetCaptureVolume(float volume)
{
    return VoipMixer::getInstance()->getCaptureVolume(&volume);   // into the argument's own slot
}
bool BF2VoipClient::mixerSelectMicrophone()           { return VoipMixer::getInstance()->selectMicrophoneRecordingSource(); }
bool BF2VoipClient::mixerSetMonitorMute(bool muted)   { return VoipMixer::getInstance()->setMicrophoneMonitorMute(muted); }

// Talking loud enough for the HUD's "voice busy" cue (0x89C2B0 = 0.95).
bool BF2VoipClient::isBusy()
{
    return m_capturing && m_captureLevel > 0.95;
}

void BF2VoipClient::onDeviceUnplugged(GVDevice device)
{
    if (m_active && world::playerManager && localPlayer())
        localPlayer()->setVoipEnabled(false);
    deviceUnplugged(device);
}

// While anybody is heard, sound group 0 is muted (0x4518D0).
void BF2VoipClient::updateTalkDucking()
{
    if (!g_bfSound || !bf2Engine)
        return;
    GVSource talking[2];
    const int count = gvListTalkingSources(m_playbackDevice, talking, 1);   // 0x58DDA0
    g_bfSound->setMuteState(static_cast<MuteState>(0), count != 0);           // +0xB4
}

// True while at most 12 frames wait in the capture buffer. Only on internet servers; captured audio
// is then sent once the backlog exceeds that (0x451610).
bool BF2VoipClient::captureBacklogLow()
{
    if (!m_internetServer)
        return false;
    return gvGetAvailableCaptureBytes(m_captureDevice) / m_encodedFrameSize <= 12;   // 0x58DD50
}

bool BF2VoipClient::canTransmit()
{
    return !m_pushToTalkEnabled || m_pushToTalkPressed;
}

void BF2VoipClient::unpluggedCallback(GVDevice device)
{
    bf2VoipClient->onDeviceUnplugged(device);
    bf2VoipClient->setEnabled(false);
}

// Address 1 is the game server's own address: the listen server's NetServer address (127.0.0.1
// without one), or for a client the joined "host:port" resolved through UDPSocket. Address 0, a
// disabled VoIP setting or a failed start marks the user as disabled.
bool BF2VoipClient::startSession(std::uint32_t address, std::uint16_t port, std::uint16_t localPort,
                                 int quality, bool useMixer)
{
    if (address == 0)
    {
        m_userDisabled = true;
        return false;
    }
    m_serverPort = port;
    m_localPort = localPort;
    m_serverAddress = address;
    m_quality = quality;
    m_useMixer = useMixer;
    IAudioSettings* settings = audioSettings();
    if (!settings || !settings->getVoipEnabled())           // +0x38
    {
        m_userDisabled = true;
        return false;
    }
    if (address == 1)
    {
        if (gameLogic->isServer())                          // +0x13C
        {
            std::string local("127.0.0.1");
            IGameServer* server = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
            if (server)
            {
                io::NetServer* netServer = server->getNetServer();            // +0xBC
                if (netServer)
                    local = netServer->getServerAddress();                     // +0x8C
            }
            address = static_cast<std::uint32_t>(::inet_addr(local.c_str()));
        }
        else
        {
            IGameClient* client = game ? static_cast<IGameClient*>(game->queryInterface(IID_IGameClient)) : 0;
            if (client)
            {
                const std::string host = client->getServerAddress();          // +0x40
                io::ISocket::Address resolved;
                std::memset(&resolved, 0, sizeof(resolved));
                resolved.length = 16;
                io::UDPSocket::nameToAddress(host, &resolved, 0);
                std::memcpy(&address, &resolved.sockaddr[4], 4);
            }
        }
    }
    if (!VoipClient::startSession(address, port, localPort, quality, useMixer))
    {
        m_userDisabled = true;
        return false;
    }
    setUnpluggedCallback(m_captureDevice, &BF2VoipClient::unpluggedCallback);
    setUnpluggedCallback(m_playbackDevice, &BF2VoipClient::unpluggedCallback);
    gvSetFilter(m_captureDevice, GV_CAPTURE, &BF2VoipClient::captureFilter);  // 0x58DD20
    setPlaybackVolume(settings->getVoipPlaybackVolume());   // +0x40
    setCaptureVolume(settings->getVoipCaptureVolume());     // +0x48
    VoipMixer::getInstance()->setBoost(settings->getVoipBoostEnabled());       // +0x58
    m_pushToTalkEnabled = settings->getVoipUsePushToTalk(); // +0x60
    if (m_pushToTalkEnabled)
        setCaptureThreshold(1.0f);
    else
        setCaptureThreshold(settings->getVoipCaptureThreshold());              // +0x50
    IGameServer* server = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (server)
        m_internetServer = server->getIsInternetServer();   // +0x8C
    return true;
}

// The local player's datagrams: frame stamp, then sender session id at the end. Muted senders are
// dropped. Malformed lengths are rejected before reading the trailer or looking up the sender.
bool BF2VoipClient::handleIncoming(std::uint8_t* packet, int length, std::uint16_t* sender)
{
    if (!m_active || !m_playbackDevice)
        return false;
    // [CHOSEN, not recovered] Reject truncated trailers and incomplete codec frames.
    // https://github.com/kr4uzi/OpenBF2/issues/26
    if (length <= 4 || m_encodedFrameSize <= 0 || (length - 4) % m_encodedFrameSize != 0)
        return false;
    std::memcpy(sender, packet + length - 2, 2);
    length -= 2;
    world::IPlayer* player = world::playerManager->getPlayerFromId(*sender);   // +0x20
    if (!player || player->getIsMuted())                    // +0x274
        return false;
    std::uint16_t frameStamp = 0;
    std::memcpy(&frameStamp, packet + length - 2, 2);
    length -= 2;
    gvPlayPacket(m_playbackDevice, packet, length, static_cast<GVSource>(*sender), frameStamp, GVFalse);
    return true;
}

bool BF2VoipClient::think(int* receiveError, int* sendError)
{
    if (!m_active || !m_captureDevice)
        return false;
    if (receiveError)
        *receiveError = 0;
    if (sendError)
        *sendError = 0;
    if (!m_receiveEnabled)
        return true;
    std::uint8_t buffer[0x400];
    std::uint16_t sender[2] = { 0, 0 };
    int received;
    do
    {
        sockaddr_in from;
#if defined(_WIN32)
        int fromLength = 0x10;
#else
        socklen_t fromLength = 0x10;
#endif
        received = static_cast<int>(::recvfrom(m_socket, reinterpret_cast<char*>(buffer), 0x400, 0,
                                               reinterpret_cast<sockaddr*>(&from), &fromLength));
        if (received > 0)
        {
            if (handleIncoming(buffer, received, sender))
                onIncomingHandled(sender);
        }
        else if (received == -1)
        {
#if defined(_WIN32)
            const int error = ::WSAGetLastError();
            const bool wouldBlock = error == WSAEWOULDBLOCK;
#else
            const int error = errno;
            const bool wouldBlock = error == EWOULDBLOCK || error == EAGAIN;
#endif
            if (!wouldBlock && receiveError)
                *receiveError = error;
            break;
        }
    } while (received > 0);
    // 0x4514F4: the backlog test runs once, before the capture loop (its back edge is 0x451502).
    if (captureBacklogLow())
        return !(receiveError && *receiveError) && !(sendError && *sendError);
    for (;;)
    {
        int length = 0x3FE;
        GVFrameStamp stamp = 0;
        GVScalar volume = 0.0;
        if (!gvCapturePacket(m_captureDevice, buffer, &length, &stamp, &volume))
            break;
        onCaptureLevel(volume);
        if (m_localMuted || m_noSquadsSelected || !canTransmit())
            continue;
        std::memcpy(buffer + length, &stamp, 2);
        length += 2;
        if (sendCaptured(buffer, &length) != -1)
            continue;
#if defined(_WIN32)
        const int error = ::WSAGetLastError();
        const bool wouldBlock = error == WSAEWOULDBLOCK;
#else
        const int error = errno;
        const bool wouldBlock = error == EWOULDBLOCK || error == EAGAIN;
#endif
        if (!wouldBlock && sendError)
            *sendError = error;
    }
    if (receiveError && *receiveError)
        return false;
    if (sendError && *sendError)
        return false;
    return true;
}

// The local player's talking flag follows the capture level; ten seconds of continuous talk mute
// the player for m_muteDuration.
void BF2VoipClient::onCaptureLevel(double level)
{
    if (!m_localLoopback && !canTransmit())
        return;
    if (!m_active || !gameLogic || gameLogic->getGameStatus() == kGameStatusPreGame)
    {
        VoipClient::onCaptureLevel(level);
        return;
    }
    world::IPlayer* local = localPlayer();
    if (!local)
        return;
    if (m_captureStart == 0.0)
    {
        m_captureStart = now();
        m_capturing = true;
    }
    m_captureLevel = level;
    m_lastCapture = now();
    if (m_lastCapture - m_captureStart > m_talkLimit)
        muteFor(m_muteDuration);
    if (!m_localMuted)
        local->setIsTalking(m_capturing);                   // +0x288
}

void BF2VoipClient::onVoiceEnded()
{
    VoipClient::onVoiceEnded();
    world::IPlayer* local = localPlayer();
    if (local)
        local->setIsTalking(m_capturing);
}

void BF2VoipClient::onIncomingHandled(std::uint16_t* sender)
{
    if (!m_active)
        return;
    world::IPlayer* player = world::playerManager->getPlayerFromId(*sender);
    if (player)
        player->setIsTalking(true);
}

// Per frame: GameSpy's think, the channel enable from squad membership, the commander screen, the
// transport, the anti-spam unmute, the talk timeout, the keep-alive and the sound ducking.
bool BF2VoipClient::updateClient()
{
    if (!m_active)
        return false;
    if (!m_playbackDevice)
        return false;
    gvThink();
    if (!m_playbackDevice)
        return false;
    world::IPlayer* local = localPlayer();
    if (!local)
        return false;
    if (local->getSquadId() != 0 || local->getIsCommander())          // +0x108 / +0x110
        setEnabled(true);
    else
        setEnabled(false);
    if (m_noSquadsSelected
        && !(local->getIsCommander() && hudManager->getMode() == kHudModeCommanderMap))
        m_noSquadsSelected = false;
    if (m_localLoopback)
    {
        localLoopbackThink();
    }
    else
    {
        int receiveError = 0;
        int sendError = 0;
        if (!think(&receiveError, &sendError) && (receiveError || sendError))
        {
            // The message is built and dropped: its sink is compiled out of the shipped image.
            std::stringstream message(std::ios::in | std::ios::out);
            message << "BF2VoipClient socket send/receive error. ReceiveResult = " << receiveError
                    << " SendResult = " << sendError;
        }
    }
    if (local->getIsMuted() && !m_capturing)
        setLocalMuted(false);
    if (m_captureStart != 0.0 && local->getStopTalkingThreshold() < now() - m_lastCapture)   // +0x280
        onVoiceEnded();
    keepAliveTick();
    updateTalkDucking();
    return true;
}

} // namespace hfe
} // namespace dice
