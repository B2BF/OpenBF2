// GameSpy query reporting through the linked SDK.
#include "GameSpyBackend.h"

#include "ServerQueryCache.h"
#include "../../IGameLogic.h"                 // gameLogic - getGameStatus / getTeamName
#include "../../Main/BF2Engine.h"
#include "../../GameLogic/ScoreSystem/ScoreManager.h"
#include "../../../Core/StringUtil.h"
#include "../../../Core/Debug.h"
#include "../../GameServer/ServerSettings.h"   // serverSettings - gameSpyPort / interfaceIP / NAT

extern "C" {
#include "../../../External/GameSpy/qr2/qr2.h"                 // qr2_initA / think / shutdown / buffers
#include "../../../External/GameSpy/gcdkey/gcdkeys.h"         // gcd_init_qr2 / gcd_shutdown
#include "../../../External/GameSpy/common/gsAvailable.h"     // GSIStartAvailableCheckA / Think
}

#if !defined(_WIN32)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace dice {
namespace hfe {

extern BF2Engine* bf2Engine;
namespace world { extern IBase* scoreManager; }

namespace gamespy { const char* getSecretKey(); }   // returns "hW6m9a" (defined below)

const char* gamespy::getSecretKey()
{
    return "hW6m9a";   // ds - BF2's GameSpy secret key ()
}

// Maps GameSpy browser keys to cached server fields; -1 selects a special-case live read.
static int serverKeyToCacheIndex(int keyid)
{
    switch (keyid)
    {
    case 1: return 0;   case 3: return 1;   case 4: return 35;  case 5: return 3;
    case 6: return 4;   case 7: return 5;   case 8: return 6;   case 10: return 7;
    case 16: return 11; case 17: return 9;  case 19: return 13; case 51: return 15;
    case 52: return 14; case 53: return 16; case 54: return 2;  case 55: return 17;
    case 56: return 18; case 57: return 19; case 58: return 20; case 59: return 21;
    case 60: return 22; case 61: return 23; case 62: return 24; case 63: return 25;
    case 64: return 26; case 65: return 27; case 66: return 8;  case 67: return 28;
    case 68: return 29; case 69: return 30; case 70: return 31; case 71: return 32;
    case 72: return 33; case 73: return 34; case 74: return 36; case 76: return 38;
    case 77: return 39; case 78: return 40; case 79: return 41; case 80: return 42;
    case 81: return 43; case 82: return 44;
    default: return -1;
    }
}

void qr2_server_key_callback(int keyid, qr2_buffer_t outbuf, void* /*userdata*/)
{
    const char* value = 0;
    if (keyid == 2)                                          // hostname key -> the gamename
        value = gamespy::g_gameName;
    else if (keyid == 11)                                    // gamemode: open{waiting,playing}
        value = (gameLogic->getGameStatus() != 3) ? "openplaying" : "openwaiting";
    else if (keyid == 75)
        // Retail destroys this temporary before qr2_buffer_addA. Preserve the dangling-pointer
        // defect (ELF 0x80BED30 and PE 0x616850); lifetime hardening is deliberately deferred.
        value = formatString("%f", bf2Engine->getAverageFPS()).c_str();
    else
    {
        const int idx = serverKeyToCacheIndex(keyid);
        if (idx >= 0)
            value = serverQueryCache->getServerKey(static_cast<ServerQueryKey>(idx));
    }
    qr2_buffer_addA(outbuf, value ? value : "");
}

// keyid -> player-key index (0..7); the switch at 0x80BF060.
static int playerKeyToIndex(int keyid)
{
    switch (keyid)
    {
    case 21: return 0; case 22: return 3; case 23: return 4; case 24: return 1;
    case 25: return 2; case 26: return 5; case 27: return 6; case 30: return 7;
    default: return -1;
    }
}

void qr2_player_team_key_callback(int keyid, int index, qr2_buffer_t outbuf, void* /*userdata*/)
{
    if (keyid == 28)                                         // team name
    {
        if (gameLogic)
            qr2_buffer_addA(outbuf, gameLogic->getTeamName(index ? 2 : 1).c_str());
        else
            qr2_buffer_addA(outbuf, "");
        return;
    }
    if (keyid == 29)
    {
        if (static_cast<unsigned int>(index) > 1)
            qr2_buffer_addA(outbuf, "0");
        else
            qr2_buffer_add_int(outbuf, static_cast<world::IScoreManager*>(world::scoreManager)
                ->getTeamScore(index + 1)->m_block[12]); // direct +0x34, both ports
        return;
    }
    const int idx = playerKeyToIndex(keyid);
    const char* value = (idx >= 0) ? serverQueryCache->getPlayerKey(index, static_cast<PlayerQueryKey>(idx)) : 0;
    qr2_buffer_addA(outbuf, value ? value : "");
}

void qr2_key_list_callback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void* /*userdata*/)
{
    if (keytype == key_server)
    {
        static const int serverKeys[] = { 1,2,3,5,6,7,8,10,11,19,16,17,4,51,52,53,54,55,56,57,58,59,
            60,61,62,63,64,65,76,66,67,68,69,70,71,72,73,74,75,77,78,79,80,81,82 };
        for (unsigned i = 0; i < sizeof(serverKeys)/sizeof(serverKeys[0]); ++i)
            qr2_keybuffer_add(keybuffer, serverKeys[i]);
    }
    else if (keytype == key_player)
    {
        static const int playerKeys[] = { 21,22,24,25,26,27,23,30 };
        for (unsigned i = 0; i < sizeof(playerKeys)/sizeof(playerKeys[0]); ++i)
            qr2_keybuffer_add(keybuffer, playerKeys[i]);
    }
    else if (keytype == key_team)
    {
        qr2_keybuffer_add(keybuffer, 28);
        qr2_keybuffer_add(keybuffer, 29);
    }
}

int qr2_count_callback(qr2_key_type keytype, void* /*userdata*/)
{
    if (keytype == key_player)
        return serverQueryCache->getNumPlayers();
    if (keytype == key_team)
        return 2;
    return -1;
}

void qr2_add_error_callback(qr2_error_t error, gsi_char* errmsg, void* /*userdata*/)
{
    //  / PE 0x616470: both branches use nonfatal Debug type 8.
    if (error == e_qrnochallengeerror)
        Debug(DEBUGTYPE_8, "Game/MatchMaking/Server/GameSpyBackend.cpp", 196)
            << "GameSpy query failed: Please check your firewall configuration.";
    else
        Debug(DEBUGTYPE_8, "Game/MatchMaking/Server/GameSpyBackend.cpp", 201)
            << "dep: gamespy QR2 failed: add_error_cb: " << static_cast<int>(error)
            << ": " << (errmsg ? errmsg : "(unknown)");
}

// registerKeys - 0x80BDFE0. Registers the bf2_* custom browser keys with qr2 (once).
namespace { bool s_keysRegistered = false; }   // ELF: the memory[0x8AAB8C5] guard byte

void GameSpyBackend::registerKeys()
{
    if (s_keysRegistered)
        return;
    qr2_register_keyA(51, "bf2_dedicated");       qr2_register_keyA(52, "bf2_ranked");
    qr2_register_keyA(53, "bf2_anticheat");       qr2_register_keyA(54, "bf2_os");
    qr2_register_keyA(55, "bf2_autorec");         qr2_register_keyA(56, "bf2_d_idx");
    qr2_register_keyA(57, "bf2_d_dl");            qr2_register_keyA(58, "bf2_voip");
    qr2_register_keyA(59, "bf2_autobalanced");    qr2_register_keyA(60, "bf2_friendlyfire");
    qr2_register_keyA(61, "bf2_tkmode");          qr2_register_keyA(62, "bf2_startdelay");
    qr2_register_keyA(63, "bf2_spawntime");       qr2_register_keyA(64, "bf2_sponsortext");
    qr2_register_keyA(65, "bf2_sponsorlogo_url"); qr2_register_keyA(66, "bf2_scorelimit");
    qr2_register_keyA(67, "bf2_ticketratio");     qr2_register_keyA(68, "bf2_teamratio");
    qr2_register_keyA(79, "bf2_coopbotratio");    qr2_register_keyA(80, "bf2_coopbotcount");
    qr2_register_keyA(81, "bf2_coopbotdiff");     qr2_register_keyA(82, "bf2_novehicles");
    qr2_register_keyA(69, "bf2_team1");           qr2_register_keyA(70, "bf2_team2");
    qr2_register_keyA(71, "bf2_bots");            qr2_register_keyA(72, "bf2_pure");
    qr2_register_keyA(73, "bf2_mapsize");         qr2_register_keyA(74, "bf2_globalunlocks");
    qr2_register_keyA(75, "bf2_fps");             qr2_register_keyA(76, "bf2_communitylogo_url");
    qr2_register_keyA(77, "bf2_plasma");          qr2_register_keyA(78, "bf2_reservedslots");
    s_keysRegistered = true;
}

#if !defined(_WIN32)
// [PLATFORM] LAN listing for a server bound to one address. qr2_create_socket binds the query
// socket to sv.interfaceIP. Winsock delivers a 255.255.255.255 datagram to a socket bound to the
// receiving interface's address, so retail Windows answers the client's LAN echo; BSD/xnu and
// Linux deliver broadcasts only to sockets bound to INADDR_ANY, so such a POSIX server was
// invisible to the LAN browser. A second socket bound to INADDR_ANY on the query port receives
// those datagrams and hands them to qr2_parse_queryA, which answers through the qr2 socket: the
// reply's source stays interfaceIP:port (the address the game port is bound to and the one
// heartbeats advertise). Unicast to interfaceIP still reaches the more specific qr2 socket.
// The listener also receives unicast sent to other local addresses on the query port; those are
// answered from interfaceIP as well (a browser matches replies by source and drops them).
namespace { int s_lanListener = -1; }   // one GameSpyBackend per process

static void openLanBroadcastListener(qr2_t qr2)
{
    sockaddr_in bound;
    socklen_t boundLen = sizeof(bound);
    if (getsockname(qr2->hbsock, reinterpret_cast<sockaddr*>(&bound), &boundLen) != 0
        || bound.sin_addr.s_addr == htonl(INADDR_ANY))
        return;   // no interfaceIP (or loopback, which the SDK maps to INADDR_ANY): already listening

    // Linux allows the wildcard bind next to the specific one only when both set SO_REUSEADDR;
    // the existing socket's flag is checked at the time of the second bind.
    int on = 1;
    setsockopt(qr2->hbsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
        return;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in any;
    std::memset(&any, 0, sizeof(any));
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = htonl(INADDR_ANY);
    any.sin_port = bound.sin_port;
    if (bind(s, reinterpret_cast<sockaddr*>(&any), sizeof(any)) != 0
        || fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) != 0)
    {
        std::fprintf(stderr, "GameSpy: LAN broadcast listener on port %d failed (%s); "
                     "LAN clients will not list this server\n", ntohs(bound.sin_port), std::strerror(errno));
        close(s);
        return;
    }
    s_lanListener = s;
}

static void pollLanBroadcastListener(qr2_t qr2)
{
    if (s_lanListener < 0)
        return;
    char data[256];   // qr2.c INBUF_LEN; the SDK parses a NUL-terminated copy
    for (;;)
    {
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(s_lanListener, data, sizeof(data) - 1, 0,
                             reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n < 0)
            break;   // EAGAIN: drained
        data[n] = '\0';
        qr2_parse_queryA(qr2, data, static_cast<int>(n), reinterpret_cast<sockaddr*>(&from));
    }
}

static void closeLanBroadcastListener()
{
    if (s_lanListener >= 0)
        close(s_lanListener);
    s_lanListener = -1;
}
#endif

// GameSpyBackend lifecycle
GameSpyBackend::GameSpyBackend(MatchMakingServer::Server* mmServer)
    : m_isInitialized(0)
    , m_matchmakingSv(mmServer)
    , m_qr2(0)
    , m_gameSpyPort(serverSettings->m_gameSpyPort)
    , m_gameSpyIP(serverSettings->m_interfaceIP)
    , m_allowNATNegotiation(serverSettings->m_allowNATNegotiation)
    , m_cdkAuth(mmServer->internetServer)
{
    // Wait for the SDK's UDP availability result. Non-Available leaves qr2 down; the upstream
    // SDK also has fail-open availability outcomes on lookup/socket/timeout failures.
    GSIStartAvailableCheckA(gamespy::g_gameName);
    GSIACResult avail;
    while ((avail = GSIAvailableCheckThink()) == GSIACWaiting)
        msleep(100);
    if (avail != GSIACAvailable)
        return;

    const char* ip = m_gameSpyIP.length() ? m_gameSpyIP.c_str() : 0;
    qr2_error_t err = qr2_initA(reinterpret_cast<qr2_t*>(&m_qr2), ip, m_gameSpyPort,
                                gamespy::g_gameName, gamespy::getSecretKey(),
                                m_matchmakingSv->internetServer, m_allowNATNegotiation,
                                &qr2_server_key_callback,
                                &qr2_player_team_key_callback, &qr2_player_team_key_callback,
                                &qr2_key_list_callback, &qr2_count_callback, &qr2_add_error_callback,
                                this);
    if (err)
        return;
    registerKeys();
    if (gcd_init_qr2(reinterpret_cast<qr2_t>(m_qr2), gamespy::g_gameID))
        return;
    if (gcd_init_qr2(reinterpret_cast<qr2_t>(m_qr2), gamespy::g_XPackGameID))
        return;
    m_isInitialized = 1;
#if !defined(_WIN32)
    openLanBroadcastListener(reinterpret_cast<qr2_t>(m_qr2));
#endif
}

GameSpyBackend::~GameSpyBackend()
{
    if (m_isInitialized)
        shutdown();
    // m_cdkAuth and m_gameSpyIP are destroyed by the member teardown.
}

void GameSpyBackend::shutdown()
{
    gcd_shutdown();
#if !defined(_WIN32)
    closeLanBroadcastListener();
#endif
    qr2_shutdown(reinterpret_cast<qr2_t>(m_qr2));
    m_isInitialized = 0;
}

bool GameSpyBackend::isInitialized()
{
    return m_isInitialized != 0;
}

GameSpyCDKeyAuthenticator* GameSpyBackend::getAuthenticator()
{
    return &m_cdkAuth;
}

void GameSpyBackend::update()   //  / PE 0x616450 (SDK body inlined on PE)
{
    if (m_isInitialized && m_qr2)
    {
        qr2_think(reinterpret_cast<qr2_t>(m_qr2));
#if !defined(_WIN32)
        pollLanBroadcastListener(reinterpret_cast<qr2_t>(m_qr2));
#endif
    }
}

} // namespace hfe
} // namespace dice
