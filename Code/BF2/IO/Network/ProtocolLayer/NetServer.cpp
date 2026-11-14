// Owns the UDP listener, admission checks, receive thread and per-connection packet flow.

#include "NetServer.h"
#include "../../../Game/GameServer/BanList.h"   // isAddressBanned - the first gate
#include "SWConnection.h"                    // init news 64 - needs the complete type
#include "NetServerThread.h"                 // startReceiveThread news one
#include "../NetworkLayer/Socket/UDPSocket.h" // _update drives the listen socket directly
#include "../SocketManager.h"                // io::socketManager
#include "../../BitStream.h"                 // the two staging BitStreams
#include "../../../Core/StringUtil.h"        // formatString - the "%s:%d" server name
#include "../../../Core/Debug.h"             // startReceiveThread's failure site
#include "../../../Core/SettingsRepostitory.h" // settings->boolGet("GSDebugNetwork")
#include "../../../Core/Ids.h"                // IID_IBase / IID_INetServer
#include <cstring>                           // getServerAddress - memset / strlen

// [PLATFORM] Wall-clock timestamps use gettimeofday on Unix and file time on Windows.
#if defined(_WIN32)
#  include <winsock2.h>
#  include <windows.h>
#else
#  include <sys/time.h>
#  include <arpa/inet.h>
#endif

namespace dice {
namespace hfe {
namespace io {

#if !defined(_WIN32)
// [CHOSEN, not recovered] c-loopback: Darwin's struct timeval has a 32-bit tv_usec, so writing it
// over a {long, long} TimeStamp left the upper half of tv_usec stale. A TimeStamp on the stack
// then held garbage microseconds and the handshake timeout (state 1, 10 s) fired at random:
// about one loopback join in three was dropped with reason 10. ELF and LP64 Linux timevals are
// {long, long}, where this is the same as the shipped direct write.
template <class Stamp>
inline void gettimeofdayInto(Stamp& stamp)
{
    struct timeval now;
    gettimeofday(&now, 0);
    stamp.tv_sec  = now.tv_sec;
    stamp.tv_usec = now.tv_usec;
}
#endif

// Starts with one reference, 64 connections and a 10,000 ms timeout; init replaces the stock port.
// [UNRESOLVED] Unmodeled reference-layout storage. Use named members rather than retail offsets.
NetServer::NetServer()
    : m_refCount(1)                                     // +0x004
    // [PLATFORM] Use a valid end iterator instead of a singular iterator. init sets the receive cursor before use.
    , m_receiveCursor(m_connections.end())                       // +0x014
    , m_pendingRemovalCount(0)                                     // +0x440
    , m_maxNumberOfConnections(64)                      // +0x444
    , m_port(16567)                                     // +0x460
    , m_listenSocket2(0)                                // +0x464
    , m_listenSocket(0)                                 // +0x468
    , m_byte46C(false)                                  // +0x46C
    , m_packetOutBuffer(0)                              // +0x470
    , m_bitStreamOut(0)                                 // +0x474  - init() builds it
    , m_bitStreamIn(0)                                  // +0x478  - init() builds it
    , m_bInit(false)                                    // +0x47C
    , m_shuttingDown(false)                             // +0x47D  receive-thread exit
    , m_version(0)                                      // +0x484
    , m_connectionTimeOut(10000)                        // +0x488
    , m_time(0)                                         // +0x4A8
    , m_processedPacketCount(0)                         // +0x4AC
    , m_dword4B0(0)                                     // +0x4B0
    , m_reconnectPasswordSize(0)                        // +0x4B4
    , m_reservedPasswordSize(0)                         // +0x4B8
    , m_dword4BC(0)                                     // +0x4BC
    , m_netServerThread(0)                              // +0x4C4  - startReceiveThread's target
    , m_totalTimeSeconds(0)                             // +0x4D0
    , m_lastUpdateMs(0)                                     // +0x4D4
    , m_reconnectTimeout(0)                             // +0x4DC
    , m_bDebugNetwork(false)                            // +0x4E0  - init() reads "GSDebugNetwork"
    , m_swConnections(0)                                // +0x4E8  restored - see the note below
    , m_stopRequested(false)                            // +0x4EC  receive-thread stop
{
    // Initialize cleanup-owned pointers explicitly; the remaining packet fields are initialized later.
    m_dataPacket.pvoid14 = 0;                           // +0x45C

    // Capture construction time using the platform clock.
#if defined(_WIN32)
    // [CHOSEN] the Win32 equivalent. filetime is 100-ns ticks since 1601; the epoch offset is the
    // standard 11644473600 seconds, and the remainder converts to microseconds - so both fields end
    // up carrying what gettimeofday would have put there.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned __int64 ticks = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    ticks -= 116444736000000000ULL;                     // 1601-01-01 -> 1970-01-01
    m_initTime.tv_sec  = (long)(ticks / 10000000ULL);
    m_initTime.tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
#else
    gettimeofdayInto(m_initTime);   // +0x498, as read
#endif
}

// Protocol version used to accept or reject connecting clients.
void         NetServer::setVersion(unsigned int version) { m_version = version; }
unsigned int NetServer::getVersion() const               { return m_version; }

// Non-atomic reference counting. Releasing the last reference deletes the object without implicitly calling shutDown.
int NetServer::addRef() { return ++m_refCount; }
int NetServer::getRef() { return m_refCount; }

int NetServer::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// ELF 0x84787A0 / PE 0x665AA0: identity for exactly these two IIDs, without addRef.
void* NetServer::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_INetServer)
        return const_cast<NetServer*>(this);
    return 0;
}

// ELF 0x84757E0/0x8475800 / PE 0x662A20/0x662A30: unvalidated unsigned timeout.
void NetServer::setConnectionTimeOut(unsigned int timeout) { m_connectionTimeOut = timeout; }
unsigned int NetServer::getConnectionTimeOut() const { return m_connectionTimeOut; }

// ELF 0x8475810/0x8475830 / PE 0x662A40/0x662A60. The getter constructs a
// string in the hidden result object on both platforms; the old reference declaration was wrong.
void NetServer::setName(std::string name) { m_name.assign(name); }
std::string NetServer::getName() const { return m_name; }


// Connection count is map.size(), including queued removals.
const std::string& NetServer::getPassword() const { return m_password; }
unsigned int NetServer::getMaxNumberOfConnections() const { return m_maxNumberOfConnections; }
unsigned int NetServer::getNumberOfConnections() const
{
    return static_cast<unsigned int>(m_connections.size());
}
unsigned int NetServer::getMaxPacketSize() const { return 1472; }

// ELF 0x84758D0 / PE 0x662AD0: bool return, 32-byte maximum, no slot-count clamp.
bool NetServer::setReservedPassword(const std::string& password, int slots)
{
    if (password.length() > 32)
        return false;
    m_reservedPassword.assign(password);
    m_reservedPasswordSize = slots;
    m_dword4BC = slots;
    return true;
}

// ELF 0x8475930 / PE 0x662B10: four-byte maximum; every accepted assignment,
// including an empty password, resets the three-minute reservation timeout.
bool NetServer::setReconnectPassword(const std::string& password, int slots)
{
    if (password.length() > 4)
        return false;
    m_reconnectPassword.assign(password);
    m_reconnectPasswordSize = slots;
    m_reconnectTimeout = 180000;
    return true;
}

// ELF 0x8475790/0x84757C0 / PE 0x6629E0/0x662A00. Both use the outbound
// socket alias and neither locks. An absent socket reads as zero and ignores writes.
float NetServer::getPacketDropRatio() const
{
    return m_listenSocket2 ? m_listenSocket2->getPacketDropRatio() : 0.0f;
}

void NetServer::setPacketDropRatio(float ratio)
{
    if (m_listenSocket2)
        m_listenSocket2->setPacketDropRatio(ratio);
}

// ELF 0x84759C0/0x8475A20 / PE 0x6636E0/0x663720: find without insertion,
// tolerate both a missing key and a null mapped value, and do not take m_mutex.
void NetServer::setStatsGatheringEnabled(int connectionId, bool enabled)
{
    const std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    if (it != m_connections.end() && it->second)
        it->second->m_byte30 = enabled;
}

bool NetServer::getStatsGatheringEnabled(int connectionId)
{
    const std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    return it != m_connections.end() && it->second && it->second->m_byte30;
}

// ELF 0x8475A80 / PE 0x663760. Failed queries do not write the caller's record.
// The two loss-rate slots change order between the internal and public structures.
bool NetServer::getConnectionStatus(int connectionId, ConnectionStatus& status)
{
    CONNSTATSENTRY stats = {};
    const std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    if (it == m_connections.end() || !it->second || !it->second->getConnectionStats(&stats))
        return false;
    status.bytesReceivedPerSecond = stats.bytesReceivedPerSecond;
    status.packetsReceivedPerSecond = stats.packetsReceivedPerSecond;
    status.bytesSentPerSecond = stats.bytesSentPerSecond;
    status.packetsSentPerSecond = stats.packetsSentPerSecond;
    status.packetsLostSendPerSecond = stats.packetsLostSendPerSecond;
    status.packetsLostReceivePerSecond = stats.packetsLostReceivePerSecond;
    status.ping = stats.ping;
    return true;
}

// Sets the password when its length is at most 32. Longer input returns false and leaves the old password intact.
bool NetServer::setPassword(const std::string& password)
{
    if (password.length() > 32)
        return false;                                   // the old password survives
    m_password.assign(password);                        // +0x48C
    return true;
}

// Sets the connection limit when count is at most 256; larger values leave the existing limit unchanged.
void NetServer::setMaxNumberOfConnections(unsigned int count)
{
    if (count > 256)
        return;                                         // silent - no clamp, no error
    m_maxNumberOfConnections = count;                   // +0x444
}

// Returns elapsed milliseconds since initialization plus the configured clock bias.
int NetServer::getTime() const
{
    TimeStamp now;
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned __int64 ticks = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    ticks -= 116444736000000000ULL;
    now.tv_sec  = (long)(ticks / 10000000ULL);
    now.tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
#else
    gettimeofdayInto(now);
#endif

    return (int)((now.tv_sec  - m_initTime.tv_sec)  * 1000
               + (now.tv_usec - m_initTime.tv_usec) / 1000
               + (int)m_time);                          // +0x4A8, the bias
}

// Initializes the listener. Returns zero on success, six for socket setup, seven for allocation, or nine for thread failure.
// Registers packet filtering before opening the socket. The pool has 64 entries regardless of the free-ID limit.
int NetServer::init(const std::string& address, unsigned int port, bool /* [UNRESOLVED] - the
                                        mangled name carries it, the body never reads it */)
{
    // 1. the PunkBuster filter agent, before any socket work. operator new(4) - a vptr and nothing
    //    else - then registerAgent(type 0, agent).
    FilteringAgent* agent = new PunkBusterFilterAgent();
    if (!agent)
        return 7;
    m_packetFilter.registerAgent(FILTEREDPACKETTYPE_0, agent);

    // 2. the socket manager has to exist and initialise.
    if (!socketManager)                                          // ds:0x8AC8C9C
        return 6;
    if (!socketManager->init())                                  // vt +0x00
        return 6;

    // 3. reset the clock the whole protocol layer times from.
    m_time = 0;
#if defined(_WIN32)
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned __int64 ticks = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        ticks -= 116444736000000000ULL;
        m_initTime.tv_sec  = (long)(ticks / 10000000ULL);
        m_initTime.tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
    }
#else
    gettimeofdayInto(m_initTime);
#endif

    // 4. the server name is "<address>:<port>", and it is also what gets listened on.
    const std::string name = formatString("%s:%d", address.c_str(), port);

    // Open the UDP listener and handle failure before starting the receive thread.
    m_listenSocket = reinterpret_cast<UDPSocket*>(socketManager->listen(name, false));   // vt +0x10
    if (!m_listenSocket)
    {
        shutDown();                                              // vt +0x14
        return 6;
    }

    m_listenAddrPort.assign(name);
    m_port          = port;                                      // +0x460 - stored, not passed above
    m_listenSocket2 = m_listenSocket;                            // the same socket in both slots

    // 6. the two 1,473-byte packet staging buffers.
    m_dataPacket.pvoid14 = new unsigned char[0x5C1];
    if (!m_dataPacket.pvoid14)
        return 7;                                                // no shutDown on this path
    m_packetOutBuffer = new unsigned char[0x5C1];
    if (!m_packetOutBuffer)
        return 7;

    // 7. the two staging BitStreams, both over a null buffer for now.
    m_bitStreamOut = new BitStream(0, 0);                        // operator new(0x64)
    if (!m_bitStreamOut)
        return 7;
    m_bitStreamIn = new BitStream(0, 0);
    if (!m_bitStreamIn)
        return 7;

    // 8. sixty-four connection objects, hard-coded - see the hazard note above.
    m_dataPacket.size = 0;
    m_swConnections = new SWConnection*[64];                     // operator new[](256)
    for (int i = 0; i <= 63; ++i)
        m_swConnections[i] = new SWConnection();                 // operator new(0x108)
    // Each connection owns receive and send packet rings.

    // 9. the receive thread starts here, inside init - so the server is concurrently receiving while
    //    the caller returns into GameServer::startServer and carries on with the map load.
    if (!startReceiveThread())
    {
        shutDown();
        return 9;
    }

    // Initialize iteration after starting the receive thread and before publishing the id pool.
    m_receiveCursor = m_connections.begin();

    // 10. the free-connection-id pool, filled high to LOW.
    for (int id = (int)m_maxNumberOfConnections - 1; id >= 0; --id)
        m_freeConnectionIds.push_back(id);

    // 11. up.
    m_bInit = true;
    m_bDebugNetwork = settings->boolGet(std::string("GSDebugNetwork"), false);
    return 0;                                                    // zero is success
}

// [CHOSEN] Shared elapsed-time helper. Preserves separate seconds and microseconds arithmetic and truncation.
unsigned int NetServer::elapsedMsSinceInit() const
{
    TimeStamp now;
#if defined(_WIN32)
    // [PLATFORM] the same substitution getTime() and the constructor use - kept identical on purpose.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned __int64 ticks = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    ticks -= 116444736000000000ULL;
    now.tv_sec  = (long)(ticks / 10000000ULL);
    now.tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
#else
    gettimeofdayInto(now);
#endif

    // the ELF order: usec term first, then 1000 * sec. Transcribed as written - see the note above.
    return (unsigned int)((now.tv_usec - m_initTime.tv_usec) / 1000
                        + 1000 * (now.tv_sec - m_initTime.tv_sec));
}

// Reads or writes the basic header: four type bits followed by eight connection-id bits.
// A serialization failure clears the stream error and returns false.
bool readBasicHeader(BitStream* stream, unsigned char& connectionId, unsigned int& type)
{
    unsigned int t = 0;
    stream->readBits(&t, 4);                            // type first, 4 bits
    type = t;

    unsigned int id = 0;
    stream->readBits(&id, 8);                           // ID second, 8 bits
    connectionId = (unsigned char)id;

    if (!stream->hasError())                            // +0x1C
        return true;

    stream->clearError();                               // +0x20 = 0, then +0x1C = false - in that
    return false;                                       //   order, which is what clearError does
}

bool writeBasicHeader(BitStream* stream, unsigned char connectionId, unsigned int type)
{
    unsigned int t = type;
    stream->writeBits(&t, 4);                           // same order as the read
    unsigned int id = connectionId;
    stream->writeBits(&id, 8);

    if (!stream->hasError())
        return true;

    stream->clearError();
    return false;
}

// Reads or writes six-bit local and remote counters followed by a 32-bit acknowledgement field.
// Counters are masked here. A serialization failure clears the stream error and returns false.
bool writeExtendedHeader(BitStream* stream, unsigned int localCounter,
                         unsigned int remoteCounter, unsigned int delayScaleFactor)
{
    unsigned int local = localCounter & 0x3F;               // masked here, not by the caller
    stream->writeBits(&local, 6);

    unsigned int remote = remoteCounter & 0x3F;
    stream->writeBits(&remote, 6);

    unsigned int delay = delayScaleFactor;                  // the only unmasked field
    stream->writeBits(&delay, 32);

    if (!stream->hasError())
        return true;

    stream->clearError();
    return false;
}

bool readExtendedHeader(BitStream* stream, unsigned int& localCounter,
                        unsigned int& remoteCounter, unsigned int& delayScaleFactor)
{
    unsigned int local = 0;
    stream->readBits(&local, 6);
    localCounter = local;

    unsigned int remote = 0;
    stream->readBits(&remote, 6);
    remoteCounter = remote;

    unsigned int delay = 0;
    stream->readBits(&delay, 32);
    delayScaleFactor = delay;

    if (!stream->hasError())
        return true;

    stream->clearError();
    return false;
}

// Pings, times out and retires connections. GSDebugNetwork disables timeout handling.
// State one closes directly; state two queues reason ten. Updates the reconnect-password countdown.
int NetServer::_updateConnections(int* outConnectionId)
{
    // zero-as-sentinel; see above.
    if (!m_lastUpdateMs)                                            // +0x4D4
        m_lastUpdateMs = elapsedMsSinceInit();

    const unsigned int nowMs = elapsedMsSinceInit();

    // The per-connection pass. Empty with zero clients.
    for (std::map<int, SWConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        SWConnection* const c = it->second;

        if (nowMs > c->m_lastPingSentMs + 3000)                     // the 3-second ping interval
            sendPingRequest(it->first);

        // Debug networking disables connection timeouts.
        if (!m_bDebugNetwork)
        {
            const unsigned int state = c->m_handshakeState;                // SWConnection +0xA0
            if ((state == 1 || state == 2) &&
                nowMs > c->m_handshakeStampMs + m_connectionTimeOut)  // 10,000 ms
            {
                *outConnectionId = it->first;
                if (c->m_handshakeState == 1)                              // re-read, not the cached copy
                {
                    // The message string comes from a global the IDB calls `filename`, whose
                    // contents are [UNRESOLVED]. An empty string stands in.
                    _closeConnection(*outConnectionId, PResult_10, std::string());
                }
                else
                {
                    storeConnectionEvent(*outConnectionId, PResult_10);
                }
            }
        }
    }

    // Count down reconnect-password validity in milliseconds, even with no clients.
    // Expiry clears the stored length while retaining the string.
    m_reconnectTimeout -= (nowMs - m_lastUpdateMs);
    if (m_reconnectPasswordSize > 0 && (int)m_reconnectTimeout < 0)
        m_reconnectPasswordSize = 0;

    // Retire whatever was queued for removal. Empty with zero clients.
    if (m_pendingRemovalCount)                                      // +0x440
    {
        for (unsigned int i = 0; i < m_pendingRemovalCount; ++i)
        {
            const int id = m_pendingRemovals[i];                    // +0x040
            std::map<int, SWConnection*>::iterator it = m_connections.find(id);

            freeConnection(it->second);
            m_freeConnectionIds.push_back(id);                      // the id goes back in the pool
            m_connections.erase(it);                                // rebalance + deallocate(24)
        }
        // Rewind the cursor after the complete erase sweep so the next receive uses a valid iterator.
        m_receiveCursor       = m_connections.begin();
        m_pendingRemovalCount = 0;
    }

    m_lastUpdateMs = nowMs;
    return 0;
}

// Drains nonblocking packets, stopping on empty input. Unassociated peers may request connection or server information.
// Other traffic must match its connection address. Returns eight when the drain ends.
int NetServer::_update(int* outConnectionId)
{
    if (!m_totalTimeSeconds)                                        // +0x4D0, zero-as-sentinel again
        m_totalTimeSeconds = elapsedMsSinceInit();

    for (;;)
    {
        const int size = m_listenSocket2->receiveFrom(
            static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14)),
            (int)kPacketPayloadSize);                               // 1472
        if (size <= 0)
            break;                                                  // the zero-client exit

        if (!m_packetFilter.processPacket(
                static_cast<const unsigned char*>(m_dataPacket.pvoid14),
                (unsigned int)size))
        {
            // filtered. Hand it to the agent's store, keyed by the peer's address string.
            std::string address;
            m_listenSocket2->addressToString(m_listenSocket2->getPeerAddress(), &address);
            // the type argument is processPacket's own 0 - see the note above.
            m_packetFilter.storePacket(
                static_cast<const unsigned char*>(m_dataPacket.pvoid14),
                (unsigned int)size, FILTEREDPACKETTYPE_0, address.c_str());
            continue;
        }

        // Process game traffic.
        ++m_processedPacketCount;                                   // +0x4AC
        m_bitStreamIn->setBuffer(
            static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14)),
            (unsigned int)size);
        m_bitStreamIn->setReadMode();                               // +0x34 = 1, read mode

        unsigned char connectionId = 0;
        unsigned int  type         = 0;
        if (!readBasicHeader(m_bitStreamIn, connectionId, type))
        {
            m_dataPacket.size = 0;                                  // malformed - drop
            continue;
        }

        m_dataPacket.dword10 = elapsedMsSinceInit();                // the arrival time

        if (type == 9)                                              // server info request
        {
            m_dataPacket.size = 0;
            handleServerInfoRequest();                              // written - empty as shipped
            continue;
        }
        if (type == 1)                                              // connect request
        {
            m_dataPacket.size = 0;
            handleConnectRequest();
            continue;
        }

        // Remaining packet types require a live connection with a matching peer address.
        std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
        SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;

        ISocket::Address peer = m_listenSocket2->getPeerAddress();
        if (!c || !UDPSocket::equalAddressAndPort(&peer, &c->m_address))
        {
            m_dataPacket.size = 0;                                  // the id/peer bind - see above
            continue;
        }

        c->m_handshakeStampMs = m_dataPacket.dword10;                 // +0x4C is a timestamp

        if (type == 4)                                              // connect accept ack
        {
            m_dataPacket.size = 0;
            *outConnectionId  = connectionId;
            if (!handleConnectAcceptAck(*outConnectionId))          // 0 == accepted
                storeConnectionEvent(*outConnectionId, PResult_1);  // tell the game side
            continue;
        }

        const unsigned int state = c->m_handshakeState;                    // SWConnection +0xA0
        if (state == 1)
        {
            // any type reaches here, and the reply is always the same - see the note above.
            c->m_handshakeState = 1;                                       // redundant self-assign
            c->m_handshakeStampMs = m_time + elapsedMsSinceInit();    // note the m_time bias
            sendConnectAccept(connectionId);                        // written (sendConnectAccept was already recovered)
            continue;
        }
        if (state != 2)
        {
            m_dataPacket.size = 0;
            continue;
        }

        switch (type)
        {
        case 7:                                                     // ping request
            *outConnectionId  = connectionId;
            m_dataPacket.size = 0;
            handlePingRequest(*outConnectionId);
            break;
        case 8:                                                     // ping response
            *outConnectionId = connectionId;
            handlePingResponse(*outConnectionId);
            break;                                                  // does not zero size
        case 15:                                                    // data packet
            *outConnectionId  = connectionId;
            m_dataPacket.size = (unsigned int)size;                 // the only arm that keeps size
            handleDataPacket(*outConnectionId, (unsigned int)size);
            break;
        case 5:                                                     // disconnect
            m_dataPacket.size = 0;
            *outConnectionId  = connectionId;
            handleDisconnect(*outConnectionId);
            break;
        default:
            m_dataPacket.size = 0;
            break;
        }
    }

    m_dataPacket.size = 0;
    return 8;                                                       // always - see the note above
}

// The PunkBuster server glue's status query (Game/PunkBuster/PbServer.cpp; ELF 0x84F62E0) controls the
// accept flag and client admission.
extern "C" bool isPbSvEnabled();

// Completes an accept handshake, assigns the connection id and promotes state 1 to state 2.
// Returns zero on success or 11 for an invalid state; the caller then queues the new-connection event.
unsigned int NetServer::handleConnectAcceptAck(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;

    if (!c || c->m_handshakeState != 1)
        return 11;

    c->m_handshakeState = 2;                            // established
    c->setConnectionId(connectionId);                   // +0xA4, first time it is set
    c->m_handshakeStampMs = m_time + elapsedMsSinceInit();
    return 0;
}

// Checks bans and capacity before admitting a connection. Malformed requests are ignored.
// Retransmissions use the same peer address/port; a second port is refused during a pending handshake.
unsigned int NetServer::handleConnectRequest()
{
    // The peer is copied by value up front: the socket's stored address changes under us as soon as
    // anything else touches it, and this function hands the same bytes to four different callees.
    ISocket::Address peerAddress = m_listenSocket2->getPeerAddress();

    std::string peerText;
    UDPSocket::UDPaddressToString(peerAddress, &peerText);   // never read - see above

    if (BanList::getInstance()->isAddressBanned(&peerAddress))
    {
        sendConnectDenied(22);
        return 22;
    }

    const unsigned int nowMs = elapsedMsSinceInit();

    // Is this address already talking to us?
    bool          sameSocket = false;
    SWConnection* existing   = 0;
    int           existingId = -1;

    for (std::map<int, SWConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        existing   = it->second;
        existingId = it->first;

        if (UDPSocket::equalAddressAndPort(&peerAddress, &existing->getPeerAddress()))
        {
            sameSocket = true;                              // a retransmit - go parse it
            break;
        }

        if (UDPSocket::equalAddress(&peerAddress, &existing->getPeerAddress())
            && existing->m_handshakeState != 2
            && (nowMs - existing->m_handshakeStampMs) <= m_connectionTimeOut)
            return 4;                                       // nothing is sent
    }

    // The six wire fields
    unsigned int protocolVersion = 0;
    m_bitStreamIn->readBits(&protocolVersion, 32);
    unsigned int buildVersion = 0;
    m_bitStreamIn->readBits(&buildVersion, 32);
    unsigned int clientHasPb = 0;
    m_bitStreamIn->readBits(&clientHasPb, 1);
    char reconnectPassword[4];
    m_bitStreamIn->readBits(reconnectPassword, 32);
    char password[32];
    m_bitStreamIn->readBits(password, 256);
    char modName[32];
    m_bitStreamIn->readBits(modName, 256);

    if (m_bitStreamIn->hasError())                          // truncated - drop it, send nothing
    {
        m_bitStreamIn->clearError();
        return 9;
    }

    if (isPbSvEnabled() && !clientHasPb)
    {
        sendConnectDenied(31);
        return 31;
    }

    if (compareIgnoreCase(settings->stringGet(std::string("GSModDirectory"),
                                              std::string("mods/bf2")),
                          std::string(modName)))
    {
        sendConnectDenied(36);
        return 36;
    }

    const bool passwordOk = checkPassword(std::string(password));
    const bool reservedOk = checkReservedPassword(std::string(password));

    if (m_reservedPasswordSize <= 0)
    {
        if (!passwordOk)
        {
            sendConnectDenied(17);
            return 17;
        }
    }
    else if (!passwordOk && !reservedOk)
    {
        sendConnectDenied(30);
        return 30;
    }

    const unsigned int versionDenial = checkVersion(protocolVersion, buildVersion);
    if (versionDenial)
    {
        sendConnectDenied(versionDenial);                   // 23 or 24 goes on the wire ...
        return 9;                                           // ... but 9 comes back. As shipped.
    }

    // A retransmit from a socket we are already handshaking with
    if (sameSocket)
    {
        if (static_cast<float>(m_time + elapsedMsSinceInit())
                > static_cast<float>(existing->m_handshakeStampMs) + 20.0f)
        {
            _closeConnection(existingId, 4, std::string());   // stale - tear the old one down
            return 9;
        }

        existing->m_handshakeState = 1;
        m_listenSocket2->setPeerAddress(peerAddress);
        existing->m_handshakeStampMs = m_time + elapsedMsSinceInit();
        sendConnectAccept(existingId);
        return 0;
    }

    // Reserved slots
    if (m_reservedPasswordSize > 0)
    {
        if (!reservedOk)
        {
            if (m_dword4BC + m_reconnectPasswordSize + m_connections.size()
                    >= m_maxNumberOfConnections)
            {
                if (m_dword4BC > 0)
                {
                    sendConnectDenied(30);                  // a reserved slot is free, but not yours
                    return 30;
                }
                sendConnectDenied(2);                       // genuinely full
                return 9;
            }
        }
        else
        {
            --m_dword4BC;                                   // spend one
        }
    }

    // Reconnect slots
    if (m_reconnectPasswordSize > 0)
    {
        char candidate[5];
        for (int i = 0; i <= 3; ++i)
            candidate[i] = reconnectPassword[i];
        candidate[4] = 0;

        if (!checkReconnectPassword(candidate))
        {
            if (m_dword4BC + m_connections.size() + m_reconnectPasswordSize
                    >= m_maxNumberOfConnections)
            {
                sendConnectDenied(2);
                return 9;
            }
        }
        else
        {
            --m_reconnectPasswordSize;                      // spend one
        }
    }

    // Accept
    if (m_connections.size() < m_maxNumberOfConnections)
    {
        int connectionId = -1;
        if (!m_freeConnectionIds.empty())                   // the inlined back() + pop_back()
        {
            connectionId = m_freeConnectionIds.back();
            m_freeConnectionIds.pop_back();
        }
        if (connectionId == -1)
            return 7;                                       // out of ids - nothing sent

        SWConnection* const c = allocConnection();
        if (!c)
            return 7;                                       // pool exhausted - nothing sent

        c->m_handshakeState = 1;                            // awaiting the client's ack
        c->setPeerAddress(peerAddress);
        m_listenSocket2->setPeerAddress(peerAddress);
        m_connections.insert(std::make_pair(connectionId, c));
        c->m_handshakeStampMs = m_time + elapsedMsSinceInit();

        if (m_reservedPasswordSize > 0 && reservedOk)
            c->m_hasReservedSlot = true;                    // _closeConnection gives it back

        sendConnectAccept(connectionId);
        return 0;
    }

    m_listenSocket2->setPeerAddress(peerAddress);
    sendConnectDenied(2);
    return 0;                                               // 0, not 2 - the send succeeded
}

// Buffers packets per connection; send drains the queued data to the socket.
// The BitStream overload derives byte length from the stream and returns zero when queued or 15 when full.

// The raw-buffer overload - forwards (src, n, extra) straight through. No in-tree
// caller reaches it on the server path, but it is INetServer slot +0x18 and is written for completeness.
void NetServer::bufferPacket(int connectionId, unsigned char* src, unsigned int n, unsigned int extra)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return;                                             // the ELF's 5; declared void, nothing reads it

    c->addPacketToSendBuffer(src, n, extra);
}

// The BitStream overload - the one processGameStateAndSendPackets calls. It reads
// the byte count off the stream's high-water mark (rounded up), not off the caller's argument.
int NetServer::bufferPacket(int connectionId, BitStream* stream, unsigned int bytes)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return 5;                                           // no such connection

    unsigned int n = stream->m_highWaterBits >> 3;
    if ((stream->m_highWaterBits & 7) != 0)
        ++n;

    // `bytes` is passed through as the ignored third argument; `n` is what addPacketToSendBuffer uses.
    return c->addPacketToSendBuffer(stream->getBuffer(), n, bytes);
}

// Drain every connection's send ring. Guards on m_bInit and holds m_mutex for the
// walk. Declared void; the ELF returns 9 when uninitialised and 0 otherwise, and nothing reads it.
void NetServer::send()
{
    if (!m_bInit)
        return;                                             // the ELF's 9

    m_mutex.lock();
    for (std::map<int, SWConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        it->second->send(m_listenSocket2);                  // result discarded, as in the ELF
    }
    m_mutex.unlock();
}

// Drain one connection's send ring. no m_bInit guard here - only the no-arg form
// has one. Holds m_mutex only when a matching connection is found.
void NetServer::send(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return;                                             // the ELF's 9

    m_mutex.lock();
    c->send(m_listenSocket2);                               // result discarded, as in the ELF
    m_mutex.unlock();
}

// Queries connection ping and delivery status. Delivery-status retrieval drains the acknowledgement queue under the mutex.
bool NetServer::getPing(int connectionId, int* ping) const
{
    std::map<int, SWConnection*>::const_iterator it = m_connections.find(connectionId);
    const SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return false;
    *ping = static_cast<int>(c->m_dword88);                 // +0x88 - the RTT statistic
    return true;
}

// Writes the peer address and port to out. Returns false for a missing connection or failed conversion.
bool NetServer::getConnectionIp(int connectionId, std::string& out)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return false;

    return m_listenSocket2->addressToString(c->getPeerAddress(), &out);
}

bool NetServer::getPacketDeliveryStatus(int connectionId, bool& delivered)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return false;

    m_mutex.lock();
    const bool result = c->getPacketDeliveryStatus(delivered);
    m_mutex.unlock();
    return result;
}

// Handshake replies return 0 when sent, 5 for a missing connection, 6 for socket failure or 9 for serialization overflow.

// Sends a timestamped ping request. Returns zero on success or a protocol error code.
// The timestamp includes the clock bias, while the retry comparison uses unadjusted elapsed time.
unsigned int NetServer::sendPingRequest(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return 5;

    const unsigned int nowMs = m_time + elapsedMsSinceInit();
    c->m_lastPingSentMs = nowMs;                    // +0x84 - stamped before the send

    m_bitStreamOut->setBuffer(m_packetOutBuffer, 0x5C1u);
    m_bitStreamOut->setBitSize(0);

    if (c->sendPingRequest(m_bitStreamOut, m_byte46C, nowMs) != 0)
        return 9;

    m_listenSocket2->setPeerAddress(c->getPeerAddress());

    unsigned int bytes = m_bitStreamOut->m_highWaterBits >> 3;
    if ((m_bitStreamOut->m_highWaterBits & 7) != 0)
        ++bytes;
    return m_listenSocket2->send(m_packetOutBuffer, bytes) ? 0u : 6u;   // == m_bitStreamOut->m_buffer
}

unsigned int NetServer::sendConnectAccept(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return 5;

    m_bitStreamOut->setBuffer(m_packetOutBuffer, 0x5C1u);
    m_bitStreamOut->setBitSize(0);
    writeBasicHeader(m_bitStreamOut, m_byte46C, 2);

    unsigned int id = static_cast<unsigned int>(connectionId);
    m_bitStreamOut->writeBits(&id, 8);

    unsigned int nowMs = m_time + elapsedMsSinceInit();
    m_bitStreamOut->writeBits(&nowMs, 32);

    unsigned int pb = isPbSvEnabled() ? 1u : 0u;
    m_bitStreamOut->writeBits(&pb, 1);

    if (m_bitStreamOut->hasError())
    {
        m_bitStreamOut->clearError();
        return 9;
    }

    m_listenSocket2->setPeerAddress(c->getPeerAddress());

    unsigned int bytes = m_bitStreamOut->m_highWaterBits >> 3;
    if ((m_bitStreamOut->m_highWaterBits & 7) != 0)
        ++bytes;
    return m_listenSocket2->send(m_packetOutBuffer, bytes) ? 0u : 6u;
}

// Sends denial to the socket's last sender. Reason 36 also includes the server's mod directory.
// Returns zero on success, 6 on send failure, or 9 on serialization failure.
unsigned int NetServer::sendConnectDenied(unsigned int reason)
{
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 0x5C1u);
    m_bitStreamOut->setBitSize(0);
    writeBasicHeader(m_bitStreamOut, m_byte46C, 3);

    unsigned int r = reason;
    m_bitStreamOut->writeBits(&r, 32);

    unsigned int wrongMod = (reason == 36u) ? 1u : 0u;
    m_bitStreamOut->writeBits(&wrongMod, 1);

    if (reason == 36u)
    {
        char mod[32];
        std::strncpy(mod, settings->stringGet(std::string("GSModDirectory"),
                                              std::string("mods/bf2")).c_str(), 32);
        m_bitStreamOut->writeBits(mod, 256);            // a fixed 32-byte field, not the length
    }

    if (m_bitStreamOut->hasError())
    {
        m_bitStreamOut->clearError();
        return 9;
    }

    unsigned int bytes = m_bitStreamOut->m_highWaterBits >> 3;
    if ((m_bitStreamOut->m_highWaterBits & 7) != 0)
        ++bytes;
    return m_listenSocket2->send(m_packetOutBuffer, bytes) ? 0u : 6u;
}

// Sends a disconnect reason and length-prefixed text, truncating the text to 200 characters.
void NetServer::sendDisconnect(unsigned int reason, const std::string& text)
{
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 0x5C1u);
    m_bitStreamOut->setBitSize(0);
    writeBasicHeader(m_bitStreamOut, m_byte46C, 5);

    unsigned int r = reason;
    m_bitStreamOut->writeBits(&r, 32);

    const std::string message(text, 0, 200);            // capped, as shipped
    unsigned int length = static_cast<unsigned short>(message.size());
    m_bitStreamOut->writeBits(&length, 16);
    m_bitStreamOut->writeBits(message.c_str(), 8 * length);

    if (m_bitStreamOut->hasError())
    {
        m_bitStreamOut->clearError();
        return;                                         // the ELF's 9; no caller reads it
    }

    unsigned int bytes = m_bitStreamOut->m_highWaterBits >> 3;
    if ((m_bitStreamOut->m_highWaterBits & 7) != 0)
        ++bytes;
    m_listenSocket2->send(m_packetOutBuffer, bytes);
}

// Sends the disconnect three times, returns any reserved slot and queues removal.
// Returns zero when queued or 9 when absent or already queued.
unsigned int NetServer::closeConnection(int connectionId, unsigned int reason, const std::string& text)
{
    m_mutex.lock();
    const unsigned int result = _closeConnection(connectionId, reason, text);
    m_mutex.unlock();
    return result;
}

unsigned int NetServer::_closeConnection(int connectionId, unsigned int reason, const std::string& text)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return 9;

    for (unsigned int i = 0; i < m_pendingRemovalCount; ++i)   // +0x440
        if (m_pendingRemovals[i] == connectionId)
            return 9;                                   // already queued

    m_listenSocket2->setPeerAddress(c->getPeerAddress());

    if (c->m_hasReservedSlot)                                    // held a reserved slot
        ++m_dword4BC;
    // Both ELF 0x847662A and PE 0x663BE8 use a signed comparison (jle).
    if (static_cast<int>(m_dword4BC) > static_cast<int>(m_reservedPasswordSize))
        m_dword4BC = m_reservedPasswordSize;

    for (unsigned int i = 0; i <= 2; ++i)               // three times, see above
        sendDisconnect(reason, text);

    m_pendingRemovals[m_pendingRemovalCount] = connectionId;
    ++m_pendingRemovalCount;
    return 0;
}

// Sends size bytes to a numeric IPv4 address and port. Leaves the socket peer changed.
// Requires a live socket; performs no DNS lookup, retry or error translation.
void NetServer::sendRawPacket(const char* address, unsigned short port, const char* data, int size)
{
    m_mutex.lock();
    sockaddr_in nativeAddress = {};
    nativeAddress.sin_addr.s_addr = ::inet_addr(address);
    nativeAddress.sin_family = AF_INET;
    nativeAddress.sin_port = htons(port);   // unqualified: a macro on macOS
    ISocket::Address peer = {};
    std::memcpy(peer.sockaddr, &nativeAddress, 16);
    peer.length = 16;
    m_listenSocket2->setPeerAddress(peer);
    m_listenSocket2->send(reinterpret_cast<const unsigned char*>(data), size);
    m_mutex.unlock();
}

// Returns zero for matching versions, 23 for older versions and 24 for newer versions.
unsigned int NetServer::checkVersion(unsigned int protocolVersion, unsigned int version)
{
    if (protocolVersion != 0x1002u)
        return protocolVersion < 0x1002u ? 23u : 24u;

    if (version != m_version)                           // +0x484
        return version < m_version ? 23u : 24u;

    return 0;
}

// Returns true when the server password is empty or exactly matches password.
bool NetServer::checkPassword(std::string password)
{
    if (m_password.compare("") == 0)                    // +0x48C - no password set
        return true;
    return password.compare(m_password) == 0;
}

// The same body against m_reservedPassword (+0x494) - the reserved-slot key, checked alongside the
// ordinary one so a reserved-slot holder can join a server that is otherwise full.
bool NetServer::checkReservedPassword(std::string password)
{
    if (m_reservedPassword.compare("") == 0)
        return true;
    return password.compare(m_reservedPassword) == 0;
}

// Compares the four-byte reconnect password. A configured stored password must contain at least four characters.
bool NetServer::checkReconnectPassword(char* password)
{
    if (m_reconnectPassword.compare("") == 0)           // +0x490 - not set, accept
        return true;

    for (int i = 0; i <= 3; ++i)
        if (password[i] != m_reconnectPassword[i])
            return false;

    return true;
}

// Takes and resets the first available preallocated connection under the pool lock.
// Returns null when the pool is exhausted.
SWConnection* NetServer::allocConnection()
{
    m_mutex4E4.lock();                                  // +0x4E4

    for (int i = 0; i <= 63; ++i)
    {
        SWConnection* const c = m_swConnections[i];     // +0x4E8
        if (c)
        {
            m_swConnections[i] = 0;
            c->reset();
            m_mutex4E4.unlock();
            return c;
        }
    }

    m_mutex4E4.unlock();
    return 0;
}

// Queues a connection event using the shared staging packet with its event type replaced.
// Unknown connection ids are ignored.
void NetServer::storeConnectionEvent(int connectionId, PResult reason)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return;

    m_dataPacket.connectionEventType = static_cast<unsigned int>(reason);   // Packet +0x0C
    c->addPacketToRecvQueue(m_dataPacket);
}

// Receive handlers reject truncated payloads, clear stream errors and drop the datagram.

// handleDataPacket - Type 15, the data carrier - and the arm the challenge
// response arrives through once the client is established. Sequence-gates the packet into the
// 32-wide window, confirms the piggybacked acks, then either stages it into the connection's recv
// ring or - if the ring is full (256 slots) - drops it and widens the delay scale as backpressure.
void NetServer::handleDataPacket(int connectionId, unsigned int size)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
    {
        m_dataPacket.size = 0;
        return;                                          // ELF returns 11
    }

    ++c->m_dword60;                                      // packets-seen, before the header read

    unsigned int local = 0, remote = 0, delay = 0;
    readExtendedHeader(m_bitStreamIn, local, remote, delay);

    // the window gate: the packet must advance our remote counter by 1..31 (mod 64).
    const unsigned int advance = (local - c->m_remotePacketCounter) & 0x3F;
    if (local == c->m_remotePacketCounter || advance > 0x1F)
        return;                                          // ELF returns 11 - out of window

    if (!c->confirmPacket(remote, delay))
        storeConnectionEvent(connectionId, PResult_13);

    // recv-ring backpressure: m_recvBuffer holds up to 256; a full ring drops the packet.
    bool full = true;
    if (c->m_recvBuffer)
        full = (int)(256 - c->m_recvBuffer->m_count) <= 0;
    if (full)
    {
        m_dataPacket.size = 0;
        const unsigned int behind = (local - c->m_remotePacketCounter) & 0x3F;
        if (behind > 1)
        {
            c->m_dword70 += behind - 1;
            c->m_delayScaleFactor <<= behind - 1;
        }
        c->m_delayScaleFactor *= 2;
        ++c->m_dword70;
        c->m_remotePacketCounter = local;
        storeConnectionEvent(connectionId, PResult_14);
        return;                                          // ELF returns 14
    }

    // accepted: stage the slice and push it onto the connection's receive ring.
    m_dataPacket.size = size;
    m_dataPacket.currBytePositionBitStream = m_bitStreamIn->getBitPosition() >> 3;   // read cursor, bytes
    m_dataPacket.dword8 = local;
    const unsigned int behind = (local - c->m_remotePacketCounter) & 0x3F;
    if (behind > 1)
    {
        c->m_dword70 += behind - 1;
        c->m_delayScaleFactor <<= behind - 1;
    }
    c->m_delayScaleFactor = (2 * c->m_delayScaleFactor) | 1;
    ++c->m_dword64;                                      // packets-accepted
    c->m_remotePacketCounter = local;
    c->m_dword6C += size;                                // bytes-received
    m_dataPacket.connectionEventType = 0;                // 0 = a data packet, not an event
    c->addPacketToRecvQueue(m_dataPacket);
    m_dataPacket.size = 0;                               // ELF returns 0
}

// handleDisconnect - Type 5. Reads the client's 32-bit disconnect reason and
// stores it as a connection event, then a second event with reason 13 (dropped). the first
// reason is attacker-controlled - it is safe only because storeConnectionEvent treats it as opaque.
void NetServer::handleDisconnect(int connectionId)
{
    unsigned int reason = 0;
    m_bitStreamIn->readBits(&reason, 32);
    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return;                                          // ELF returns 9 - malformed, dropped
    }
    storeConnectionEvent(connectionId, static_cast<PResult>(reason));   // the wire-supplied reason
    storeConnectionEvent(connectionId, PResult_13);      // ELF returns 0
}

// handlePingRequest - Type 7. Confirms the piggybacked acks, answers with a
// type-8 ping response echoing the client's stamp, and records the round on failure.
void NetServer::handlePingRequest(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return;                                          // ELF returns 9

    unsigned int extLocal = 0, extRemote = 0, extDelay = 0;
    if (!readExtendedHeader(m_bitStreamIn, extLocal, extRemote, extDelay))
        return;                                          // ELF returns 9

    unsigned int haveSent = 0;
    m_bitStreamIn->readBits(&haveSent, 1);
    const bool sentFlag = (haveSent == 1);

    unsigned int timeStamp = 0;
    m_bitStreamIn->readBits(&timeStamp, 32);

    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return;                                          // ELF returns 9 - malformed, dropped
    }

    if (sentFlag)
        pingConfirmation(c, extLocal);
    const bool confirmed = c->confirmPacket(extRemote, extDelay);

    if (sendPingResponse(connectionId, timeStamp))       // non-zero == send failed
    {
        storeConnectionEvent(connectionId, PResult_6);
    }
    else
    {
        if (!confirmed)
            storeConnectionEvent(connectionId, PResult_13);
        m_dataPacket.size = 0;                           // ELF returns 0
    }
}

// handlePingResponse - Type 8. Reads the echoed stamp back and folds it into
// the connection's round-trip statistics (last/min/max/sum, ping count), then confirms the acks.
void NetServer::handlePingResponse(int connectionId)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
    {
        m_dataPacket.size = 0;
        return;                                          // ELF returns 9
    }

    unsigned int extLocal = 0, extRemote = 0, extDelay = 0;
    if (!readExtendedHeader(m_bitStreamIn, extLocal, extRemote, extDelay))
        return;                                          // ELF returns 9

    unsigned int haveSent = 0;
    m_bitStreamIn->readBits(&haveSent, 1);
    const bool sentFlag = (haveSent == 1);

    unsigned int echoStamp = 0;
    m_bitStreamIn->readBits(&echoStamp, 32);             // the stamp we sent, echoed back
    unsigned int clientStamp = 0;
    m_bitStreamIn->readBits(&clientStamp, 32);           // read as shipped; unused in the RTT

    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return;                                          // ELF returns 9 - malformed, dropped
    }

    if (sentFlag)
        pingConfirmation(c, extLocal);
    const bool confirmed = c->confirmPacket(extRemote, extDelay);

    // round-trip: now (ms since init) minus the echoed stamp. m_dword88 last / m_dword8C min /
    // m_dword98 max-or-threshold / m_dword90 running sum / m_dword94 count.
    const unsigned int rtt = elapsedMsSinceInit() - echoStamp;
    ++c->m_dword94;
    const bool underThreshold = (rtt <= c->m_dword98);
    c->m_dword88 = rtt;
    if (underThreshold)
    {
        if (rtt < c->m_dword8C)
            c->m_dword8C = rtt;
    }
    else
    {
        c->m_dword98 = rtt;
    }
    c->m_dword90 += c->m_dword88;

    if (!confirmed)
        storeConnectionEvent(connectionId, PResult_13);
    m_dataPacket.size = 0;                               // ELF returns 0
}

// Applies the ping's acknowledgement counter and widens the delay scale for a sequence gap.
void NetServer::pingConfirmation(SWConnection* c, unsigned int ackSequence)
{
    if (!c)
    {
        m_dataPacket.size = 0;
        return;                                          // ELF returns 11
    }
    const unsigned int advance = (ackSequence - c->m_remotePacketCounter) & 0x3F;
    if (ackSequence == c->m_remotePacketCounter || advance > 0x1F)
    {
        m_dataPacket.size = 0;
        return;                                          // ELF returns 11 - duplicate / out of window
    }
    if (advance > 1)
    {
        c->m_dword70 += advance - 1;
        c->m_delayScaleFactor <<= advance - 1;
    }
    ++c->m_dword70;
    c->m_delayScaleFactor *= 2;
    c->m_remotePacketCounter = ackSequence;
    m_dataPacket.size = 0;                               // ELF returns 0
}

// sendPingResponse - Builds the outbound type-8 into m_bitStreamOut and sends
// it to the connection's peer. returns unsigned int - 0 sent, 6 socket-send failed, 9 no such
// connection / build error. handlePingRequest reads this to decide whether to record the ping.
unsigned int NetServer::sendPingResponse(int connectionId, unsigned int echoTimeStamp)
{
    std::map<int, SWConnection*>::iterator it = m_connections.find(connectionId);
    SWConnection* const c = (it == m_connections.end()) ? 0 : it->second;
    if (!c)
        return 9;

    const unsigned int serverTimeStamp = m_time + elapsedMsSinceInit();
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 0x5C1u);
    m_bitStreamOut->setBitSize(0);

    if (c->sendPingResponse(m_bitStreamOut, m_byte46C, echoTimeStamp, serverTimeStamp) != 0)
        return 9;

    m_listenSocket2->setPeerAddress(c->getPeerAddress());

    unsigned int bytes = m_bitStreamOut->m_highWaterBits >> 3;
    if ((m_bitStreamOut->m_highWaterBits & 7) != 0)
        ++bytes;
    return m_listenSocket2->send(m_packetOutBuffer, bytes) ? 0u : 6u;   // m_packetOutBuffer == the stream's buffer
}

// Returns a connection to the 64-slot pool under its mutex. A full pool triggers a fatal diagnostic.
void NetServer::freeConnection(SWConnection* connection)
{
    m_mutex4E4.lock();
    for (int i = 0; i <= 63; ++i)
    {
        if (!m_swConnections[i])
        {
            m_swConnections[i] = connection;
            m_mutex4E4.unlock();
            return;
        }
    }
    Debug(DEBUGTYPE_6, std::string("IO/Network/ProtocolLayer/NetServer.cpp"), 236, std::string())
        << "broken connection pool behaviour, this can't really happen";
    m_mutex4E4.unlock();                                 // dead after the abort, as shipped
}

// update - The virtual pump (+0x2C): housekeeping then one receive drain.
// Returns 8 in the ELF when uninitialised; the vtable slot is void and no caller reads it.
void NetServer::update(int* outConnectionId)
{
    if (!m_bInit)
        return;
    _updateConnections(outConnectionId);
    _update(outConnectionId);
}

// Returns the peer's numeric challenge IP. Connection id -1 resolves the server's listen address instead.
unsigned int NetServer::getChallengeIp(int connectionId) const
{
    if (connectionId == -1)
    {
        ISocket::Address addr = ISocket::Address();                 // 16 zero bytes + length
        if (UDPSocket::nameToAddress(m_listenAddrPort, &addr, 0))   // non-zero == failure
            return 0;
        return m_listenSocket2->getChallengeIp(addr);
    }

    std::map<int, SWConnection*>::const_iterator it = m_connections.find(connectionId);
    if (it == m_connections.end() || !it->second)
        return 0;
    return m_listenSocket2->getChallengeIp(it->second->getPeerAddress());
}


// Returns the listen host without its port. Caches the first nonempty result process-wide, including DNS errors.
// An empty result is retried on the next call.
std::string NetServer::getServerAddress() const
{
    static std::string address;                             // ds:0x8AC3D38, guard at 0x8AC3D30

    std::string text;                                       // constructed before the test, as shipped

    if (address.length() == 0 && m_listenSocket2)
    {
        ISocket::Address addr;
        std::memset(addr.sockaddr, 0, sizeof(addr.sockaddr));
        addr.length = 16;                                   // `mov [ebp+var_58], 10h`

        if (UDPSocket::nameToAddress(m_listenAddrPort, &addr, 0) != 0)
        {
            address.assign("(dns error)", std::strlen("(dns error)"));
        }
        else if (m_listenSocket2->addressToString(addr, &text))          // vslot +0x28, always true
        {
            unsigned short port;                            // written and never read
            UDPSocket::stringToNameAndPort(text, &address, &port);
        }
    }

    return address;
}


// Server-info requests intentionally produce no reply in this protocol.
void NetServer::sendServerInfo()
{
    // 0x8476320: `xor eax, eax; retn`. Empty as shipped - see the block above. The declared return
    // is void; the ELF materialises 0 and no recovered caller reads it.
}

void NetServer::handleServerInfoRequest()
{
    sendServerInfo();                                   // a tail jump, nothing else
}

bool NetServer::startReceiveThread()
{
    m_netServerThread = new NetServerThread(this);       // operator new(0xC), 0x8477D05
    if (!m_netServerThread)
    {
        Debug(DEBUGTYPE_6, "IO/Network/ProtocolLayer/NetServer.cpp", 1961)
            << "serverReceiveThread was not created successfully";
        return false;                                    // unreachable - the Debug aborts
    }

    // above normal, and a name nothing reads - see above.
    m_netServerThread->start("NetServerThread", Thread::Priority_AboveNormal);
    return true;
}

// Sets both stop flags, then stops and deletes the receive thread.
// Only stopRequested uses the stop mutex; POSIX publishes shutting-down through its atomic member.
void NetServer::endReceiveThread()
{
    if (!m_netServerThread)
        return;

    m_shuttingDown = true;                               // +0x47D  no lock - see above

    m_stopMutex.lock();
    m_stopRequested = true;                              // +0x4EC  under the stop mutex
    m_stopMutex.unlock();

    m_netServerThread->stop();                           // Thread::stop
    delete m_netServerThread;                            // vtable +4, the deleting destructor
    m_netServerThread = 0;
}

// Drains connections under the mutex using a persistent cursor. Zero supplies a payload; other events leave stream untouched.
// Result eight advances or rewinds the cursor. init must establish the cursor before receiving.
int NetServer::receive(int* connectionId, BitStream* stream)
{
    m_mutex.lock();

    while (m_receiveCursor != m_connections.end())
    {
        SWConnection* const c = m_receiveCursor->second;         // node +0x14
        *connectionId         = m_receiveCursor->first;          // node +0x10

        unsigned char* buffer = 0;
        unsigned int   size   = 0;
        const int ev = c->getPacketPtrFromRecvQueue(&buffer, &size);

        if (ev == 0)                                             // 0 = a data packet
        {
            stream->setBuffer(buffer, size);
            stream->resetPositions();
            stream->setReadMode();                               // 0x84749E8  [edi+0x34] = 1
        }

        if (ev != 8)
        {
            m_mutex.unlock();
            return ev;
        }

        ++m_receiveCursor;                                       // 0x8474982  _M_increment
    }

    m_receiveCursor = m_connections.begin();                     // rewind, not clear
    m_mutex.unlock();
    return 8;                                                    // 8 = nothing left this pass
}

// Receives into caller-owned bytes and length through the persistent connection cursor.
void NetServer::receive(int* connectionId, unsigned char* dest, unsigned int* length)
{
    m_mutex.lock();

    while (m_receiveCursor != m_connections.end())
    {
        SWConnection* const c = m_receiveCursor->second;
        *connectionId         = m_receiveCursor->first;

        const int ev = c->getPacketFromRecvQueue(dest, length);
        if (ev != 8)
        {
            m_mutex.unlock();
            // [CHOSEN] void. The ELF materialises `ev` in eax and the vtable slot's mangled name
            // encodes no return type; no recovered caller reads one. NetServer.h declares it void.
            return;
        }

        ++m_receiveCursor;
    }

    m_receiveCursor = m_connections.begin();
    m_mutex.unlock();
}

// Disconnects clients, stops receiving and frees owned transport state.
// Must run only once: the socket alias and pool pointer are not cleared after deletion.
void NetServer::shutDown()
{
    m_mutex.lock();
    for (std::map<int, SWConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        const unsigned int state = it->second->m_handshakeState;     // SWConnection +0xA0
        if (state == 2 || state == 1)
            _closeConnection(it->first, PResult_13, std::string(""));
    }
    m_mutex.unlock();

    endReceiveThread();

    if (m_dataPacket.pvoid14)                                         // +0x45C
    {
        delete[] static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14));
        m_dataPacket.pvoid14 = 0;
    }
    if (m_packetOutBuffer)                                            // +0x470
    {
        delete[] m_packetOutBuffer;
        m_packetOutBuffer = 0;
    }
    if (m_bitStreamIn)                                                // +0x478
    {
        delete m_bitStreamIn;
        m_bitStreamIn = 0;
    }
    if (m_bitStreamOut)                                               // +0x474
    {
        delete m_bitStreamOut;
        m_bitStreamOut = 0;
    }
    if (m_listenSocket)                                               // +0x468, vtable +4
    {
        delete m_listenSocket;
        m_listenSocket = 0;
    }
    if (m_swConnections)                                              // +0x4E8
    {
        for (int i = 0; i <= 63; ++i)
        {
            SWConnection* const c = m_swConnections[i];
            if (c)
                delete c;                                             // ~SWConnection + operator delete
        }
        if (m_swConnections)                                          // re-tested, as shipped
            delete[] m_swConnections;                                 // not nulled - see above
    }
}

// Destroys member containers and the packet filter; does not call shutDown.
// Call shutDown first to join the thread and release sockets, buffers, streams and connections.
// The connection map owns only its nodes. PacketFilter owns queued payloads and its classifier.
// [PLATFORM] use typed deletion for the differing MSVC/Itanium destructor layouts.
NetServer::~NetServer()
{
}

} // namespace io
} // namespace hfe
} // namespace dice
