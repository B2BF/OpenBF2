// Network telemetry sampling and aggregation. Graphical statistics retain a client rendering frontier.

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered]
#endif
#include "NetworkStats.h"
#include "../../Core/Ids.h"
#include "../../Game/IGame.h"
#include "../../Game/IGameClient.h"
#include "../../Game/IGameServer.h"
#include "../../Game/Common/IClientConnection.h"
#include "../../Game/Common/IStreamManager.h"
#include "../../Game/Objects/IPlayer.h"
#include "../../Scene/Object/PlayerManager.h"

namespace dice {
namespace hfe {
namespace io {

// ELF C2 8470FE0 / C1 8471140; PE 6669D0. Both construct four empty vectors, resize
// the two stream vectors, then zero the percentage pairs. The ConnectionStatus tail is untouched.
BasicNetworkStats::BasicNetworkStats()
{
    m_streamManagerReadBits.resize(3, 0.0f);
    m_streamManagerWriteBits.resize(3, 0.0f);
    for (unsigned int i = 0; i < 3; ++i) {
        m_streamManagerReadPercent[i] = 0.0f;
        m_streamManagerWritePercent[i] = 0.0f;
    }
}

// ELF D2 84712A0 / D1 8471340 / D0 84713E0; PE 666900 / scalar 666C30.
// The only work is compiler-generated vector destruction in reverse declaration order.
BasicNetworkStats::~BasicNetworkStats()
{
}

// ELF 8471480; PE 666A70. Map order defines sample order; reset each signed counter only
// after push_back succeeds. resize(0), rather than reallocation, preserves the existing capacity.
void BasicNetworkStats::sampleNetworkableWriteStats()
{
    std::map<std::string, RegistredNetworkStats*>* stats =
        RegistredNetworkStats::getNetworkStatsMap();
    m_networkableWriteBits.resize(0);
    for (std::map<std::string, RegistredNetworkStats*>::iterator it = stats->begin();
         it != stats->end(); ++it) {
        m_networkableWriteBits.push_back(static_cast<float>(it->second->m_writeBits));
        it->second->m_writeBits = 0;
    }
}

// ELF 8471580; PE 666B50. Read and write counters are sampled/reset independently.
void BasicNetworkStats::sampleNetworkableReadStats()
{
    std::map<std::string, RegistredNetworkStats*>* stats =
        RegistredNetworkStats::getNetworkStatsMap();
    m_networkableReadBits.resize(0);
    for (std::map<std::string, RegistredNetworkStats*>::iterator it = stats->begin();
         it != stats->end(); ++it) {
        m_networkableReadBits.push_back(static_cast<float>(it->second->m_readBits));
        it->second->m_readBits = 0;
    }
}

// ELF 8471680 / PE 666580. The bool is accumulate (ELF mangling ends EPNS0_17IClientConnectionEb),
// not a stream index. Both images acquire each stream manager twice, with no null guards.
void BasicNetworkStats::sampleStreamManagerReadStats(IClientConnection* connection, bool accumulate)
{
    for (unsigned int i = 0; i < 3; ++i) {
        if (!accumulate)
            m_streamManagerReadBits[i] = 0.0f;
        float& sample = m_streamManagerReadBits[i];
        const int bits = connection->getStreamManager(i)->getReadBits();
#if defined(_WIN32)
        // [PLATFORM] PE 6665B4 fild adds the signed integer before storing a float. ELF
        // 84716D6 cvtsi2ss rounds the integer to float before the addition (84716E3).
        sample = static_cast<float>(static_cast<double>(bits) + sample);
#else
        sample = static_cast<float>(bits) + sample;
#endif
        connection->getStreamManager(i)->setReadBits(0);
    }
}

// ELF 8471710 / PE 6665E0; same acquisition/accumulation contract, using the +8 counter.
void BasicNetworkStats::sampleStreamManagerWriteStats(IClientConnection* connection, bool accumulate)
{
    for (unsigned int i = 0; i < 3; ++i) {
        if (!accumulate)
            m_streamManagerWriteBits[i] = 0.0f;
        float& sample = m_streamManagerWriteBits[i];
        const int bits = connection->getStreamManager(i)->getWriteBits();
#if defined(_WIN32)
        sample = static_cast<float>(static_cast<double>(bits) + sample);
#else
        sample = static_cast<float>(bits) + sample;
#endif
        connection->getStreamManager(i)->setWriteBits(0);
    }
}

// Refreshes cached status when the game exposes a client; ignores the query's success flag.
void BasicNetworkStats::updateClientStats()
{
    if (!game)
        return;
    IGameClient* client = static_cast<IGameClient*>(game->queryInterface(IID_IGameClient));
    if (client)
        client->getConnectionStatus(m_connectionStatus);
}

// ELF 84717A0; PE 666640. The first local player terminates the scan (not continue).
// Only successful status reads contribute, and only their four throughput fields are summed.
void BasicNetworkStats::updateServerStats()
{
    if (!game)
        return;
    IGameServer* server = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
    if (!server)
        return;

    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    m_connectionStatus.packetsLostSendPerSecond = 0.0f;
    m_connectionStatus.packetsLostReceivePerSecond = 0.0f;
    m_connectionStatus.ping = 0;
    m_connectionStatus.bytesReceivedPerSecond = 0.0f;
    m_connectionStatus.packetsReceivedPerSecond = 0.0f;
    m_connectionStatus.bytesSentPerSecond = 0.0f;
    m_connectionStatus.packetsSentPerSecond = 0.0f;

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it) {
        world::IPlayer* player = *it;
        if (!player->getIsRemote())
            break;
        ConnectionStatus status;
        if (server->getConnectionStatus(player->getId(), status)) {
            m_connectionStatus.packetsSentPerSecond += status.packetsSentPerSecond;
            m_connectionStatus.packetsReceivedPerSecond += status.packetsReceivedPerSecond;
            m_connectionStatus.bytesSentPerSecond += status.bytesSentPerSecond;
            m_connectionStatus.bytesReceivedPerSecond += status.bytesReceivedPerSecond;
        }
    }

#if defined(_WIN32)
    // [PLATFORM] Preserve float rounding boundaries and division-before-multiplication order.
    // Extended precision varies by compiler, so native results may differ at x87 rounding edges.
    float readTotal = m_streamManagerReadBits[0] + m_streamManagerReadBits[1];
    readTotal = readTotal + m_streamManagerReadBits[2];
    const long double writeTotal = static_cast<long double>(m_streamManagerWriteBits[0]) +
        m_streamManagerWriteBits[1] + m_streamManagerWriteBits[2];
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
    // ELF 847181E..847183C retains both totals in x87 and multiplies by 100 before division.
    bf2x87::Real readTotal = 0.0L;
    bf2x87::Real writeTotal = 0.0L;
    for (unsigned int i = 0; i < 3; ++i) {
        readTotal += m_streamManagerReadBits[i];
        writeTotal += m_streamManagerWriteBits[i];
    }
#else
    // ELF 847181E..847183C retains both totals in x87 and multiplies by 100 before division.
    long double readTotal = 0.0L;
    long double writeTotal = 0.0L;
    for (unsigned int i = 0; i < 3; ++i) {
        readTotal += m_streamManagerReadBits[i];
        writeTotal += m_streamManagerWriteBits[i];
    }
#endif
#endif
    for (unsigned int i = 0; i < 3; ++i) {
        if (readTotal > 0.0f) {
#if defined(_WIN32)
            m_streamManagerReadPercent[i] = static_cast<float>(
                static_cast<long double>(m_streamManagerReadBits[i]) / readTotal * 100.0L);
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
            m_streamManagerReadPercent[i] = static_cast<float>(
                static_cast<bf2x87::Real>(m_streamManagerReadBits[i]) * 100.0L / readTotal);
#else
            m_streamManagerReadPercent[i] = static_cast<float>(
                static_cast<long double>(m_streamManagerReadBits[i]) * 100.0L / readTotal);
#endif
#endif
        }
        if (writeTotal > 0.0f) {
#if defined(_WIN32)
            m_streamManagerWritePercent[i] = static_cast<float>(
                static_cast<long double>(m_streamManagerWriteBits[i]) / writeTotal * 100.0L);
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
            m_streamManagerWritePercent[i] = static_cast<float>(
                static_cast<bf2x87::Real>(m_streamManagerWriteBits[i]) * 100.0L / writeTotal);
#else
            m_streamManagerWritePercent[i] = static_cast<float>(
                static_cast<long double>(m_streamManagerWriteBits[i]) * 100.0L / writeTotal);
#endif
#endif
        }
        // Nonpositive/NaN totals leave the previous percentage intact, but still reset samples.
        m_streamManagerReadBits[i] = 0.0f;
        m_streamManagerWriteBits[i] = 0.0f;
    }
}

// ELF 8471960; PE 666810. Client collection precedes server collection on every update.
void BasicNetworkStats::update()
{
    updateClientStats();
    updateServerStats();
}

// the guarded function-local static - libstdc++'s allocator dance around the
// map's header node is the whole body; the map itself is empty at first ask.
std::map<std::string, RegistredNetworkStats*>* internal_getNetworkStatsMap()
{
    static std::map<std::string, RegistredNetworkStats*> networkStatsMap;
    return &networkStatsMap;
}

// copy the name, zero both counters, copy the colour, then insert (name -> this)
// into the process map. insert_unique - a second registration under the same name is ignored (the
// first object stays), as shipped.
RegistredNetworkStats::RegistredNetworkStats(const std::string& name, const Vec3& colour)
    : m_name(name)
{
    m_writeBits = 0;                                     // +0x04
    m_readBits  = 0;                                     // +0x08
    m_colour    = colour;                                // +0x0C
    internal_getNetworkStatsMap()->insert(std::make_pair(std::string(name), this));
}

// find by name; null when absent.
RegistredNetworkStats* RegistredNetworkStats::getNetWorkStats(const std::string& name)
{
    std::map<std::string, RegistredNetworkStats*>* map = internal_getNetworkStatsMap();
    std::map<std::string, RegistredNetworkStats*>::iterator it = map->find(name);
    return it != map->end() ? it->second : 0;
}


std::map<std::string, RegistredNetworkStats*>* RegistredNetworkStats::getNetworkStatsMap()
{
    return internal_getNetworkStatsMap();
}

} // namespace io
} // namespace hfe
} // namespace dice
