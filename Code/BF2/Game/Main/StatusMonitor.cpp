// Dedicated-server status display. Output order, screen coordinates, and optional
// service checks define the display layout. [INFERRED] File placement.

#include "StatusMonitor.h"
#include "../PunkBuster/PbGlue.h"
#include "StatusMonitorImpl.h"

#include "../../IO/Network/ProtocolLayer/NetServer.h"
#include "../../IO/Network/NetworkStats.h"   // actual aggregate layout, including native STL members

#include <string>
#include <vector>
#include <sstream>

#include "../GameServer/ServerSettings.h"   // m_serverName +0x04, m_internet +0x0C, m_roundsPerMap +0x74
#include "../../IO/Console/Console.h"       // OldConsole::getLines, and the mainConsole singleton
#include "../MapList.h"                     // the mapList singleton, at its real type
#include "../IGame.h"                       // real IGame - `game` carries its real type
#include "../../Core/SettingsRepostitory.h" // real repository - ditto `settings`
#include "../GameLogic/ServerGameLogic.h"   // getCurrentLevel / getGameStatus / m_roundsPlayed
#include "../IGameServer.h"                 // THE interface, NOT GameServer - see update()

// These declarations have global scope; the PunkBuster query uses C linkage.
std::string getTimeString(unsigned int seconds);
extern "C" bool isPbSvEnabled();   // Game/PunkBuster/PbServer.cpp

// formatString is declared with the rest of the free functions further down, inside dice::hfe;
// getTimeString sits above that block (it is global, and the ELF puts it below the ctor), so it
// needs the declaration here.
namespace dice { namespace hfe { std::string formatString(const char* fmt, ...); } }

// Formats unsigned seconds as hours, minutes, and seconds. Hours wrap to zero at
// 100; negative values converted by callers become large unsigned times.
std::string getTimeString(unsigned int seconds)
{
    unsigned int hours   = seconds / 3600u;
    unsigned int rest    = seconds - hours * 3600u;
    unsigned int minutes = rest / 60u;
    unsigned int secs    = rest - minutes * 60u;

    if (hours   >= 100u) hours   = 0u;
    if (minutes >=  60u) minutes = 0u;
    if (secs    >=  60u) secs    = 0u;

    return dice::hfe::formatString("%02d:%02d:%02d", hours, minutes, secs);
}

namespace dice {
namespace hfe {

extern SettingsRepostitory* settings;
extern IGame*               game;

// [CHOSEN] Partial class declarations retain the exact external symbol signatures.
// Do not include them with the complete class definitions; replace them with shared
// headers when this translation unit can use those definitions.
class BF2Engine { public: float getAverageFPS() const; };

// Global declarations must retain their defining types for MSVC symbol identity.
class IGameLogic;
extern IGameLogic* gameLogic;               // ds:0x8ACBFA0, as declared in BF2Engine.cpp
extern BF2Engine*  bf2Engine;               // ds:0x8AACBD8
extern const unsigned int IID_IGameServer;

std::string formatString(const char* fmt, ...);
void        setLowerCase(std::string&);
namespace WorldPref { int getTickCount(); }
namespace BuildNrUtil { const char* getBuildNrString(); }

// Shared monitor strings have translation-unit lifetime.
namespace {

// . Empty at static-init time; filled by the constructor below. [INFERRED] name.
std::string g_versionString;

// . Built at static-init from asc_8893700, which is exactly 79 '-' characters - measured,
// not assumed. 79 rather than 80 keeps ncurses from wrapping the cursor onto the next row when the
// line is written at column 0 of an 80-column terminal. [INFERRED] name.
const std::string g_separator(79, '-');

} // anonymous namespace


// Almost all of that is one inlined ostringstream. The whole constructor is: make the platform
// backend, and cache the version banner.
StatusMonitor::StatusMonitor()
    : m_impl(StatusMonitorImpl::createInstance())
{
    std::ostringstream version;
    // [CHOSEN, not recovered] Display the native port's architecture.
    // Keep the shipped x86 suffix; only the status screen gains the native bitness.
    // BuildNrUtil's shared protocol/build number remains unchanged.
    version << "v" << BuildNrUtil::getBuildNrString()
            << (sizeof(void*) == 8 ? "(x86-64)" : "(IA-32)");
    g_versionString.assign(version.str());              // std::string::assign into ds:0x8AACC44

    // No space before the suffix: the shipped literal at 0x8146EEE is "(IA-32)".
    // BuildNrUtil::getBuildNrString is recovered and has no trailing space.
}

//  (D1) / 0x8147030 (D2), 29 bytes each - a null test, a virtual delete, a return.
StatusMonitor::~StatusMonitor()
{
    delete m_impl;
}

void StatusMonitor::update(bool duringLoad)
{
    // vtable +0x08. Returns false on the 0.5 s throttle, true on a keypress or on the timer expiring.
    // Everything below is skipped when it says no.
    if (!m_impl->shouldUpdate(duringLoad))
        return;

    // Only server interfaces supply the central status display. Retain queryInterface's
    // adjusted IGameServer pointer; treating it as a GameServer pointer would apply the
    // secondary-base adjustment twice.
    ServerGameLogic* svGameLogic     = 0;
    IGameServer*     gameSvInterface = 0;
    if (gameLogic && game)
    {
        gameSvInterface = (IGameServer*)game->queryInterface(IID_IGameServer);   // IGame vt +0x0C
        if (gameSvInterface)
            svGameLogic = (ServerGameLogic*)gameLogic;
    }

    const MapList::Entry* entry = 0;
    if (svGameLogic)
        entry = mapList->getEntry(svGameLogic->getCurrentLevel());

    // "mods/bf2" -> "bf2". Lowercase first, then erase everything up to AND including the first '/'.
    // The original clamps the erase length to the string size before calling replace, which only
    // matters for a path that is a single trailing slash.
    std::string modDir = settings->stringGet("GSModDirectory", "mods/bf2");
    setLowerCase(modDir);
    const std::string::size_type slash = modDir.find('/');
    if (slash != std::string::npos)
    {
        std::string::size_type n = slash + 1;
        if (n >= modDir.size())
            n = modDir.size();
        modDir.erase(0, n);
    }

    // vtable +0x10. width is fetched and then never read - every column below is a literal. On a
    // terminal narrower than ~84 columns the right-hand fields run off the edge. Faithful.
    int width = 0, height = 0;
    m_impl->getDimensions(width, height);

    std::string title = "Battlefield 2 Dedicated Server " + g_versionString;
    if (serverSettings->getRanked())
        title.append(" - Official rankings enabled");

    m_impl->clearScreen();                              // vtable +0x0C
    m_impl->outputString(0, 0, title);                  // vtable +0x14

    std::ostringstream o;
    int nextRow;

    if (settings->boolGet("GSShowNetGraph", false))
    {
        // Net-graph mode. Columns 0 and 53.
        m_impl->outputString(0, 1, "Network Statistics");

        o.str(""); o.precision(4); o << "Avg FPS:" << bf2Engine->getAverageFPS();
        m_impl->outputString(53, 1, o.str());

        nextRow = 2;

        // ..8147B1C / PE 422B60..422C91. Preserve all five getNetworkStats calls:
        // presence, read percentages, write percentages, byte rates, packet rates. The first
        // percentage block is read (+34 ELF/+44 PE); the second is write (+40 ELF/+50 PE).
        // The former local shape omitted the vptr/vectors and reversed telemetry directions.
        if (svGameLogic && game->getNetworkStats())
        {
            const float* readPercent = static_cast<const io::BasicNetworkStats*>(
                game->getNetworkStats())->getStreamManagerReadPercent();
            const float* writePercent = static_cast<const io::BasicNetworkStats*>(
                game->getNetworkStats())->getStreamManagerWritePercent();
            m_impl->outputString(0, 2, formatString(
                "SM w[%]: (%2.1f, %2.1f, %2.1f) r[%]: (%2.1f, %2.1f, %2.1f)",
                writePercent[0], writePercent[1], writePercent[2],
                readPercent[0], readPercent[1], readPercent[2]));

            const io::ConnectionStatus& byteRates = static_cast<const io::BasicNetworkStats*>(
                game->getNetworkStats())->getConnectionStatus();
            m_impl->outputString(53, 2, formatString("R:%3.1f S:%3.1f kB/s",
#if defined(_WIN32)
                // /422C26 multiply by the float 0.001f (3A83126F at 78A724),
                // retaining the product until double vararg materialization; ELF divides by 1000.
                static_cast<double>(byteRates.bytesReceivedPerSecond) * 0.001f,
                static_cast<double>(byteRates.bytesSentPerSecond) * 0.001f));
#else
                byteRates.bytesReceivedPerSecond / 1000.0, byteRates.bytesSentPerSecond / 1000.0));
#endif

            const io::ConnectionStatus& packetRates = static_cast<const io::BasicNetworkStats*>(
                game->getNetworkStats())->getConnectionStatus();
            m_impl->outputString(53, 3, formatString("RP:%4.0f SP:%4.0f Pkt/s",
                packetRates.packetsReceivedPerSecond, packetRates.packetsSentPerSecond));

            nextRow = 3;
        }
    }
    else
    {
        // Normal mode. Columns 0, 41 and 61.
        o.str(""); o << '"' << serverSettings->m_serverName << '"';     // +0x04
        m_impl->outputString(0, 1, o.str());

        if (svGameLogic)
        {
            o.str(""); o.precision(4);
            o << "Average FPS:" << bf2Engine->getAverageFPS()
              << " [d:" << gameSvInterface->getNumDroppedFrames()
              << ", o:" << gameSvInterface->getNumOvershotFrames() << "]";
            m_impl->outputString(41, 1, o.str());

            std::string address = "(unknown)";
            int         port    = 0;
            io::NetServer* netServer = gameSvInterface->getNetServer();
            if (netServer)
            {
                address = netServer->getServerAddress();
                // Access the native member by name; original-image offsets are not valid addresses
                // within this source-level NetServer layout.
                port = (int)netServer->m_port;                     // +0x460 in retail
            }

            o.str(""); o << "IP: " << address << " Port: " << port << ' ';
            if (!serverSettings->m_internet)    o << "(LAN)";        // +0x0C
            if (isPbSvEnabled())                o << " (PB)";
            m_impl->outputString(0, 2, o.str());
        }

        o.str(""); o << "Map: ";
        if (entry) o << entry->name;
        m_impl->outputString(41, 2, o.str());

        o.str(""); o << "Game mode: ";
        if (entry)
        {
            // The live map size wins over the map-list entry's, so a server that changed size
            // mid-session shows the size it is actually running, not the one it was listed with.
            o << entry->gameMode << "/"
              << (game ? game->getCurrentMapSize() : entry->mapSize);   // IGame +0x100
        }
        if (serverSettings->getNoVehicles())    o << " (No Vehicles)";
        m_impl->outputString(0, 3, o.str());

        m_impl->outputString(41, 3, "Mod: " + modDir);

        // The original re-reads ds:0x8ACBFA0 here rather than reusing the local; svGameLogic is the
        // same object, already narrowed, and is used instead.
        if (svGameLogic && svGameLogic->getNextStateChangeAtTick())
        {
            // The 30 is a literal in the instruction stream. Another independent witness for
            // BF2's 30 Hz fixed timestep, from a completely different subsystem.
            const int ticks = svGameLogic->getNextStateChangeAtTick() - WorldPref::getTickCount();
            m_impl->outputString(61, 3, "TimeLeft: " + getTimeString((unsigned int)(ticks / 30)));
        }

        nextRow = 4;

        if (svGameLogic)
        {
            o.str("");
            o << "Players: " << gameSvInterface->getNumberOfConnectedClients()
              << "/" << serverSettings->getMaxPlayers()
              << " (" << serverSettings->getNumReservedSlots() << " r)"
              << " (" << gameSvInterface->getNumberOfConnectingClients() << " connecting)";
            m_impl->outputString(0, 4, o.str());

            // Transcribed exactly. `remaining` counts down, so the displayed round is
            // roundsPerMap - remaining, and the sentinel for "no round in progress" is remaining
            // == -1 rather than 0. Simplifying this changes what the screen shows.
            const int roundsPerMap = serverSettings->m_roundsPerMap;             // +0x74
            // Compute rounds remaining from the server's completed-round count.
            const int remaining    = roundsPerMap - (svGameLogic->m_roundsPlayed + 1);
            o.str("");
            if (remaining == -1) o << "Round: N/A";
            else                 o << "Round: " << (roundsPerMap - remaining) << "/" << roundsPerMap;
            m_impl->outputString(41, 4, o.str());
        }

        const char* status = "[no game]";
        if (svGameLogic)
        {
            switch (svGameLogic->getGameStatus())
            {
            case 1:  status = "[playing]";      break;
            case 2:  status = "[endgame]";      break;
            case 3:  status = "[pregame]";      break;
            case 4:  status = "[paused]";       break;
            case 5:  status = "[restart]";      break;
            case 6:  status = "[disconnected]"; break;
            default: status = "[unknown]";      break;
            }
        }
        o.str(""); o << "Status: " << status;
        m_impl->outputString(61, 4, o.str());
    }

    // The separator, then the console filling everything below it.
    const int separatorRow = nextRow + 1;
    m_impl->outputString(0, separatorRow, g_separator);

    const int firstConsoleRow = separatorRow + 1;
    const int available       = height - firstConsoleRow;

    std::vector<std::string> lines;
    io::mainConsole->getLines(available, lines);        // vtable +0x24

    // bottom-aligned, not top-aligned. The first line goes at height - lines.size(), so the newest
    // line is always on the terminal's last row however little scrollback exists. Writing this as
    // `firstConsoleRow + i` would look right on a full screen and drift on an empty one.
    for (std::vector<std::string>::size_type i = 0; i < lines.size(); ++i)
        m_impl->outputString(0, height - (int)lines.size() + (int)i, lines[i]);

    m_impl->flushToScreen();                            // vtable +0x18
}


} // namespace hfe
} // namespace dice
