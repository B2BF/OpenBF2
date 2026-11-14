// BF2-side PunkBuster game functions, BF2.exe 0x4D94D0..0x4DA49B and the ELF dedicated server
// 0x84F7340..0x84F8A90 (whose symbols give the names). They answer the glues' queries and carry their
// traffic. One body for both images; the dedicated image has no client glue, so it lacks
// PB_cl_sendToServer, the client forward in PB_handlePacket and the "team" setting, and its PB_homepath
// reads GSPunkBusterPath. [INFERRED] path.
#include "PbGlue.h"
#include "../IGame.h"
#include "../IGameClient.h"
#include "../IGameServer.h"
#include "../Objects/IPlayer.h"
#include "../MatchMaking/Server/MatchMakingServer.h"
#include "../GameServer/ServerSettings.h"
#include "../Utility/BuildNrUtil.h"
#include "../../Core/Ids.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../IO/Console/Console.h"
#include "../../IO/File/FileManager.h"
#include "../../IO/Network/NetworkLayer/Socket/UDPSocket.h"
#include "../../Scene/Object/PlayerManager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#if defined(_WIN32)
#include <direct.h>
#else
#include <strings.h>
#include <unistd.h>
#endif

using namespace dice::hfe;

// GameSpy CD-key hash lookup (External/GameSpy/gcdkey), declared narrowly as GameServer.cpp does.
extern "C" char* gcd_getkeyhash(int gameid, int localid);

namespace {
#if defined(_WIN32)
char* currentDirectory(char* buffer, int size) { return _getcwd(buffer, size); }
int compareNoCaseN(const char* a, const char* b, std::size_t n) { return _strnicmp(a, b, n); }
int compareNoCase(const char* a, const char* b) { return _stricmp(a, b); }
#else
char* currentDirectory(char* buffer, int size) { return getcwd(buffer, static_cast<size_t>(size)); }
int compareNoCaseN(const char* a, const char* b, std::size_t n) { return strncasecmp(a, b, n); }
int compareNoCase(const char* a, const char* b) { return strcasecmp(a, b); }
#endif

char s_pbPath[0x200];     // ds:0x9A7E68 (ELF ds:0x898FF40), PB_basepath's (and BF2.exe's PB_homepath's)
char s_pbVersion[0x50];   // ds:0x9A8068
char s_pbSetting[0x100];  // ds:0x9A80B8
char s_pbEmpty[1];        // ds:0x8AA769, PB_examineSetting's answer without a game

#if defined(_WIN32)
const char kServerLibraryPath[] = "pb/pbsv.dll";  // ds:0x8AA724
#else
const char kServerLibraryPath[] = "pb/pbsv.so";   // ELF ds:0x88E84D7
#endif

IGameClient* gameClient() { return static_cast<IGameClient*>(game->queryInterface(IID_IGameClient)); }
IGameServer* gameServer() { return static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)); }
} // namespace

// 0x4D94D0 / ELF 0x84F7340: the working directory (filled once).
char* PB_basepath()
{
    if (!s_pbPath[0]) currentDirectory(s_pbPath, 0x200);
    return s_pbPath;
}
#if !DEDICATED
// 0x4D9500: the working directory too, in the same buffer.
char* PB_homepath()
{
    if (!s_pbPath[0]) currentDirectory(s_pbPath, 0x200);
    return s_pbPath;
}
#else
// ELF 0x84F7370: the GSPunkBusterPath setting, re-read every call into a static string; the base path
// when it is empty.
char* PB_homepath()
{
    static std::string path;  // ELF ds:0x8AC8430
    path = settings->stringGet(std::string("GSPunkBusterPath"), std::string(""));
    if (path.empty()) return PB_basepath();
    return const_cast<char*>(path.c_str());
}
#endif

// 0x4D9530: the profile's cl_punkbuster, written only while pb/pbcl.dll exists.
void PB_set_cl_punkbuster(int value)
{
    if (!io::fileManager->fileExists(std::string("pb/pbcl.dll"))) return;
    const bool enabled = value != 0;
    settings->boolSet(std::string("GSClPunkBuster"), enabled);
}

// 0x4D95B0 / ELF 0x84F7560: the server's PunkBuster flag, written only while the server library exists.
void PB_set_sv_punkbuster(int value)
{
    if (!io::fileManager->fileExists(std::string(kServerLibraryPath))) return;
    serverSettings->m_punkBuster = value != 0;
}

// 0x4D9610 / ELF 0x84F75F0
int PB_maxClients() { return serverSettings->getMaxPlayers(); }

// The record PB_getPlayerInfo fills for pbsv (query 0x66). [INFERRED] type and field names.
struct PbPlayerInfo {
    char name[0x21];      // +0x00
    char keyHash[0x21];   // +0x21 the CD-key hash, internet servers only
    char address[0x21];   // +0x42 "ip:port", or "localhost" for the hosting player
    char unresolved63[5]; // +0x63 [UNRESOLVED] untouched
    char extra[0x98];     // +0x68 "team=<n>"; pbsv's buffer is 0x100 bytes
};

// 0x4D9620 / ELF 0x84F7610: player `index`'s name, key hash, address and team for pbsv; 0 when there
// is no such player or (other than the host's) no ready client. (BF2.exe also tests
// `index < 0 && index > 63`, which is never true; gcc drops it.)
int PB_getPlayerInfo(int index, char* buffer)
{
    PbPlayerInfo* info = reinterpret_cast<PbPlayerInfo*>(buffer);
    info->address[0] = 0;
    if (!game) return 0;
    IGameServer* server = gameServer();
    if (!server || !world::playerManager) return 0;
    world::IPlayer* player = world::playerManager->getPlayerFromId(index);
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!player) return 0;
    if (player != local && !server->isClientReady(index)) return 0;
    std::strncpy(info->name, player->getName().c_str(), 0x20);
    info->name[0x20] = 0;
    if (serverSettings->m_internet && player->getIsConnected()) {
        std::strncpy(info->keyHash, gcd_getkeyhash(gamespy::g_gameID, index), 0x20);
        info->keyHash[0x20] = 0;
    } else {
        info->keyHash[0] = 0;
    }
    if (local == player) {
        std::strcpy(info->address, "localhost");
    } else {
        std::string address;
        if (server->getPlayerConnectionIpAndPort(player, address))
            std::strncpy(info->address, address.c_str(), 0x20);
        else
            info->address[0] = 0;
        info->address[0x20] = 0;
    }
    std::sprintf(info->extra, "team=%d", player->getTeam());
    return 1;
}

// 0x4D97E0: the DLL's text, newline appended, to the console; on a client also to the chat window
// unless it starts with "[skipnotify]".
void PB_outputText(char* text)
{
    std::string line(text);
    line += '\n';
    if (!game) return;
    if (game->queryInterface(IID_IGameServer)) {
        io::mainConsole->output(line);
        return;
    }
    io::mainConsole->output(line);
    if (compareNoCaseN(text, "[skipnotify]", 12)) game->addTextToChatWindow(line.c_str());
}

// 0x4D9890 / ELF 0x84F7950: a console command from pbsv ("Cmd_Exec"), newline appended, run silently.
void PB_issueConsoleCommand(char* command)
{
    std::string line(command);
    line.append(1, '\n');
    if (io::mainConsole)
        io::mainConsole->executeLine(line, io::Console::ignoredString_, false, false, false, false, false);
}

// 0x4D98F0 / ELF 0x84F7A00: pbsv's kick ("DropClient"), with PResult 0x22 and its reason text.
void PB_DropClient(int index, char* reason)
{
    if (!game) return;
    if (IGameServer* server = gameServer())
        server->kickPlayer(index, static_cast<io::PResult>(0x22), std::string(reason));
}

// 0x4D9950 / ELF 0x84F7AA0: a PB packet to client `index`: "\xff\xff\xff\xff" + data, sent to the host
// and port of that player's connection address.
void PB_sv_sendToClient(int index, char* data, int length)
{
    std::string address;
    if (!game) return;
    IGameServer* server = gameServer();
    if (!server) return;
    server->getPlayerConnectionIpAndPort(world::playerManager->getPlayerFromId(index), address);
    int port = 0;
    const std::string::size_type colon = address.find(":", 0, 1);
    if (colon != std::string::npos) {
        std::string portText;
        portText = address.substr(colon + 1, address.size());
        port = static_cast<short>(std::atoi(portText.c_str()));
        std::string host;
        host = address.substr(0, colon);
        address = host;
    }
    char packet[0x640] = { '\xff', '\xff', '\xff', '\xff', 0 };
    std::memcpy(packet + 4, data, static_cast<std::size_t>(length));  // retail has no bound either
    server->sendUDPPacket(address.c_str(), static_cast<unsigned short>(port), packet, length + 4);
}

#if !DEDICATED
// 0x4D9B30: a PB packet to the joined server: "\xff\xff\xff\xff" + data, sent to the host and port of
// the client's server address.
void PB_cl_sendToServer(char* data, int length)
{
    if (!game) return;
    IGameClient* client = gameClient();
    if (!client) return;
    std::string address = client->getServerAddressCopy();
    int port = 0;
    const std::string::size_type colon = address.find(":", 0, 1);
    if (colon != std::string::npos) {
        std::string portText;
        portText = address.substr(colon + 1, address.size());
        port = std::atoi(portText.c_str());
        std::string host;
        host = address.substr(0, colon);
        address = host;
    }
    char packet[0x640] = { '\xff', '\xff', '\xff', '\xff', 0 };
    std::memcpy(packet + 4, data, static_cast<std::size_t>(length));  // retail has no bound either
    client->sendUDPPacket(address.c_str(), static_cast<unsigned short>(port), packet, length + 4);
}
#endif

// 0x4D9CF0: a raw datagram through the client, or else through the server.
void PB_sendUDPPacket(char* address, unsigned short port, char* data, int length)
{
    if (!game) return;
    IGameClient* client = gameClient();
    IGameServer* server = gameServer();
    if (client) client->sendUDPPacket(address, port, data, length);
    else if (server) server->sendUDPPacket(address, port, data, length);
}

// 0x4D9D60: the build stamp, cached.
char* PB_getVersion()
{
    if (!s_pbVersion[0]) {
        const std::string stamp(BuildNrUtil::getBuildNrString());
        std::strncpy(s_pbVersion, stamp.c_str(), 0x4F);
        s_pbVersion[0x4F] = 0;
    }
    return s_pbVersion;
}

// 0x4D9DD0 / ELF 0x84F88A0: answers 0. [INFERRED] identity: in both images it is the one return-0 PB_
// function and sits immediately before PB_handlePacket; the client glue hands it to pbcl.dll as its
// +0x194 callback. [UNRESOLVED] parameter.
int PB_GlQuery(char*) { return 0; }

// 0x4D9DE0 / ELF 0x84F88B0: a "\xff\xff\xff\xff" PB datagram from `from`. Types other than S 2 G I Y B L
// go to the client glue (client image); types other than C 1 J go to the server glue with the index of
// the player whose connection address matches (the player count when none does).
void PB_handlePacket(char* data, int length, const char* from)
{
    std::string playerAddress;
    IGameServer* server = gameServer();
    if (length < 8 || std::strncmp(data, "\xff\xff\xff\xff", 4)) return;
    const char type = data[7];
#if !DEDICATED
    if (type != 'S' && type != '2' && type != 'G' && type != 'I' && type != 'Y' && type != 'B' && type != 'L')
        PbClAddEvent(0x0D, length - 4, data + 4);
#endif
    if (type == 'C' || type == '1' || type == 'J') return;
    int index = -1;
    if (serverSettings->getMaxPlayers() > -1) {
        do {
            if (server
                && server->getPlayerConnectionIpAndPort(world::playerManager->getPlayerFromId(index), playerAddress)
                && !std::strcmp(playerAddress.c_str(), from))
                break;
            ++index;
        } while (index < serverSettings->getMaxPlayers());
    }
    PbSvAddEvent(0x0D, index, length - 4, data + 4);
}

// 0x4D9F40 / ELF 0x84F8A10: whether the server library is present.
bool PB_get_sv_punkbusterInstalled() { return io::fileManager->fileExists(std::string(kServerLibraryPath)); }

// 0x4D9F80 / ELF 0x84F7F50: the settings the DLL may read, answered from a 256-byte buffer.
char* PB_examineSetting(char* name)
{
    if (!game) return s_pbEmpty;
    s_pbSetting[0] = 0;
    if (!compareNoCase(name, "cl_punkbuster")) {
        const bool fallback = false;
        int value = settings->boolGet(std::string("GSClPunkBuster"), fallback) ? 1 : 0;
        if (game->queryInterface(IID_IGameServer)) value = 0;
        std::sprintf(s_pbSetting, "%d", value);
    } else if (!compareNoCase(name, "sv_punkbuster")) {
        int value = serverSettings->getSvPunkBuster() ? 1 : 0;
        if (game->queryInterface(IID_IGameClient)) value = 0;
        std::sprintf(s_pbSetting, "%d", value);
    } else if (!compareNoCase(name, "mapname")) {
        std::strncpy(s_pbSetting, game->getCurrentMapName().c_str(), 0xFF);
    } else if (!compareNoCase(name, "server")) {
        IGameClient* client = gameClient();
        IGameServer* server = gameServer();
        if (client) std::strncpy(s_pbSetting, client->getServerAddressCopy().c_str(), 0xFF);
        if (server) {
            std::stringstream text;
            text << server->getServerAddress();
            std::strncpy(s_pbSetting, text.str().c_str(), 0xFF);
        }
    } else if (!compareNoCase(name, "sv_hostname")) {
        gameServer();  // queried and discarded
        if (IGameClient* client = gameClient())
            std::strncpy(s_pbSetting, client->getServerAddressCopy().c_str(), 0xFF);
        else
            std::strncpy(s_pbSetting, serverSettings->m_serverName.c_str(), 0xFF);
    } else if (!compareNoCase(name, "name")) {
        if (world::IPlayer* local = world::playerManager->getLocalHumanPlayer())
            std::strncpy(s_pbSetting, local->getName().c_str(), 0xFF);
        else
            std::strncpy(s_pbSetting,
                         settings->stringGet(std::string("GSPlayerName"), std::string("player")).c_str(), 0xFF);
#if !DEDICATED
    } else if (!compareNoCase(name, "team")) {
        if (world::IPlayer* local = world::playerManager->getLocalHumanPlayer())
            std::sprintf(s_pbSetting, "%d", local->getTeam());
        else
            s_pbSetting[0] = 0;
#endif
    } else if (!compareNoCase(name, "gamename")) {
        if (IGameServer* server = gameServer())
            std::strncpy(s_pbSetting, server->getName().c_str(), 0x100);
        else
            s_pbSetting[0] = 0;
    } else if (!compareNoCaseN(name, "pb_isresolved", 13)) {
        io::ISocket::Address address = {};
        address.length = 0x10;
        std::string text;
        const bool resolved = io::UDPSocket::nameToAddress(std::string(name + 14), &address, 0) == 0
                              && io::UDPSocket::UDPaddressToString(address, &text);
        if (resolved) std::strncpy(s_pbSetting, text.c_str(), 0x100);
        else s_pbSetting[0] = 0;
    } else {
        s_pbSetting[0] = 0;
    }
    s_pbSetting[0xFF] = 0;
    return s_pbSetting;
}
