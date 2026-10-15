// Client protocol connection. Integer obfuscation is omitted; PunkBuster activation retains a named [FRONTIER] stop.
#include "NetClient.h"
#if !DEDICATED

#include "SWConnection.h"
#include "../NetworkLayer/Socket/UDPSocket.h"
#include "../SocketManager.h"
#include "../../BitStream.h"
#include "../../../Core/StringUtil.h"                // formatString - the "%s:%d" address
#include "../../../Core/Debug.h"
#include "../../../Core/SettingsRepostitory.h"        // settings->boolGet("GSDebugNetwork"), stringGet("GSModDirectory")
#include "../../../Core/Ids.h"                        // IID_IBase / IID_INetClient
#include "../../../Core/IBase.h"
#include "../../../Core/Profiler.h"                   // registerThread - the "NetClientThread" bookkeeping
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

namespace dice {
namespace hfe {

extern IBase* profiler;                               // Core/Globals.cpp - the "Profiler" singleton (PE dword_9A5C70)

namespace io {

#define NETCLIENT_FILE "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\IO\\Network\\ProtocolLayer\\NetClient.cpp"

namespace {
    // The PE guards the game-thread entry points with `if (m_threaded) WaitForSingleObject(m_mutex)`
    // and the matching ReleaseMutex. One helper per side keeps the eleven sites identical.
    inline void lockIf(bool threaded, void* mutex)
    {
#if defined(_WIN32)
        if (threaded)
            ::WaitForSingleObject(mutex, INFINITE);
#else
        (void) threaded; (void) mutex;
#endif
    }
    inline void unlockIf(bool threaded, void* mutex)
    {
#if defined(_WIN32)
        if (threaded)
            ::ReleaseMutex(mutex);
#else
        (void) threaded; (void) mutex;
#endif
    }
    inline unsigned int bytesOf(const BitStream* stream)
    {
        return (stream->m_highWaterBits >> 3) + ((stream->m_highWaterBits & 7) != 0 ? 1u : 0u);
    }
}

// NetClient::NetClient - 6BE4D0, 213 B. The stores in address order. Like NetServer's, this
// constructor leaves the ping ring, the two accept-seeded scalars (+0x08 / +0x0C) and the request
// stamp uninitialised - handleConnectAccept seeds them before anything reads them. Kept.
NetClient::NetClient()
    : m_refCount(1)                                   // +0x004
    , m_syncClock(false)                              // +0x010
    , m_pingRingFull(false)                           // +0x011
    , m_connectionId(0)                               // +0x0A4
    , m_version(0)                                    // +0x0AC - the PE stores the encoding of 0
    , m_connectionTimeOut(10000)                      // +0x0B0
    , m_connectAttemptsLeft(0)                        // +0x0C0 - the canary at its "spent" value
    , m_connection(0)                                 // +0x0C4
    , m_socket(0)                                     // +0x0E0
    , m_timeBase(0)                                   // +0x0F8
    , m_packetOutBuffer(0)                            // +0x100
    , m_bitStreamIn(0)                                // +0x104
    , m_bitStreamOut(0)                               // +0x108
    , m_bInit(false)                                  // +0x10C
    , m_threaded(true)                                // +0x10D
    , m_byte10E(false)                                // +0x10E
    , m_mutex(0)                                      // +0x110
    , m_thread(0)                                     // +0x114
    , m_stopRequested(false)                          // +0x118
    , m_totalTimeSentinel(0)                          // +0x11C
    , m_bDebugNetwork(false)                          // +0x124
    , m_packetFilter()                                // +0x128
    , m_disconnectText()                              // +0x13C
{
    m_dataPacket.pvoid14 = 0;                         // +0x0DC - stored twice in the PE (6BE532, 6BE5A0)
#if defined(_WIN32)
    LARGE_INTEGER counter;
    ::QueryPerformanceCounter(&counter);              // +0x0E8
    m_startCounter = counter.QuadPart;
    ::QueryPerformanceFrequency(&counter);            // +0x0F0
    m_counterFrequency = counter.QuadPart;
#else
    m_startCounter = 0;
    m_counterFrequency = 1;
#endif
}

// ~NetClient - 6BD370, 62 B: shutDown, then the string and the filter (the compiler's order here).
NetClient::~NetClient()
{
    shutDown();
}

// The quartet - 6D0970 / 68AED0 / 6BC9A0 / 6BC960
int NetClient::addRef()  { return ++m_refCount; }
int NetClient::getRef()  { return m_refCount; }
int NetClient::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;                                  // the +0x74 deleting destructor
    return r;
}
void* NetClient::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_INetClient)
        return const_cast<NetClient*>(this);
    return 0;
}

// init - 6BC9C0, 349 B. SocketManager::init (dispatched on the pointer without a null test, unlike
// NetServer::init - shipped), the one SWConnection, the receive buffer, the two staging streams
// (out first, then in), the send buffer, the PunkBuster filter agent, then the flags. 6 when the
// socket manager refuses, 7 for any allocation, 0 up.
int NetClient::init(bool threaded)
{
    if (!socketManager->init())                       // vt +0x00 on ds:9FFCC4, unguarded
        return 6;
    m_connection = new (std::nothrow) SWConnection(); // operator new(0x100) + 6D63C0
    if (!m_connection)
        return 7;
    m_dataPacket.pvoid14 = new (std::nothrow) unsigned char[1473];
    if (!m_dataPacket.pvoid14)
        return 7;
    m_bitStreamOut = new (std::nothrow) BitStream(0, 0);
    if (!m_bitStreamOut)
        return 7;
    m_bitStreamIn = new (std::nothrow) BitStream(0, 0);
    if (!m_bitStreamIn)
        return 7;
    m_packetOutBuffer = new (std::nothrow) unsigned char[1473];
    if (!m_packetOutBuffer)
        return 7;
    FilteringAgent* agent = new (std::nothrow) PunkBusterFilterAgent();   // operator new(4), vtable 913404
    if (!agent)
        return 7;
    m_packetFilter.registerAgent(FILTEREDPACKETTYPE_0, agent);
    m_threaded          = threaded;
    m_dataPacket.size   = 0;
    m_bInit             = true;
    m_bDebugNetwork     = settings->boolGet(std::string("GSDebugNetwork"), false);
    return 0;
}

// shutDown - 6BC0B0, 190 B. The thread first (only when one was wanted), a disconnect with reason
// 13 through the vtable, then the buffers, the streams, the socket and the connection.
void NetClient::shutDown()
{
    if (m_threaded)
        stopReceiveThread();
    closeConnection(13);                              // 6BC0C9: `call [vptr+0x1C]`
    delete[] static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14));
    m_dataPacket.pvoid14 = 0;
    delete[] m_packetOutBuffer;
    m_packetOutBuffer = 0;
    if (m_bitStreamIn)
        delete m_bitStreamIn;                         // 6B63F0 frees the owned buffer, then operator delete
    m_bitStreamIn = 0;
    if (m_bitStreamOut)
        delete m_bitStreamOut;
    m_bitStreamOut = 0;
    if (m_socket)
        delete m_socket;                              // the deleting destructor through the vtable
    m_socket = 0;
    if (m_connection)
        delete m_connection;                          // 6D62B0 + operator delete
    m_connection = 0;
}

// Closes the client connection, sending its disconnect three times.
// The Windows mutex wait is unconditional, including non-threaded operation.
int NetClient::closeConnection(unsigned int reason)
{
    if (!m_socket)
        return 9;
#if defined(_WIN32)
    ::WaitForSingleObject(m_mutex, INFINITE);
#endif
    for (int i = 0; i < 3; ++i)
        sendDisconnect(reason);
#if defined(_WIN32)
    ::ReleaseMutex(m_mutex);
#endif
    return 0;
}

// bufferPacket - 6BBB90 (a stream: its buffer and its high-water byte count) / 6BBB30 (raw bytes).
int NetClient::bufferPacket(BitStream* stream, unsigned int extra)
{
    lockIf(m_threaded, m_mutex);
    const int r = m_connection->addPacketToSendBuffer(stream->getBuffer(), bytesOf(stream), extra);
    unlockIf(m_threaded, m_mutex);
    return r;
}
int NetClient::bufferPacket(unsigned char* src, unsigned int n, unsigned int extra)
{
    lockIf(m_threaded, m_mutex);
    const int r = m_connection->addPacketToSendBuffer(src, n, extra);
    unlockIf(m_threaded, m_mutex);
    return r;
}

// send - 6BBC00, 100 B: 9 before init or without a socket, else SWConnection::send(socket).
int NetClient::send()
{
    if (!m_bInit || !m_socket)
        return 9;
    lockIf(m_threaded, m_mutex);
    const int r = static_cast<int>(m_connection->send(m_socket));
    unlockIf(m_threaded, m_mutex);
    return r;
}

// getPacketDeliveryStatus - 6BBCF0, 78 B.
bool NetClient::getPacketDeliveryStatus(bool& delivered)
{
    lockIf(m_threaded, m_mutex);
    const bool r = m_connection->getPacketDeliveryStatus(delivered);
    unlockIf(m_threaded, m_mutex);
    return r;
}

// receive(BitStream*) - 6BBA50, 126 B: the queue's next entry; a data packet (0) lands the stream
// over the ring slice - setBuffer, both cursors zeroed, read mode - and the code is the answer
// either way (8 = empty, else a connection event).
int NetClient::receive(BitStream* stream)
{
    lockIf(m_threaded, m_mutex);
    unsigned char* buff = 0;
    unsigned int   length = 0;
    const int r = m_connection->getPacketPtrFromRecvQueue(&buff, &length);
    if (!r)
    {
        stream->setBuffer(buff, length);
        stream->setReadMode();                        // +0x10 = 0, +0x14 = 0 (setBuffer's init), +0x34 = 1
    }
    unlockIf(m_threaded, m_mutex);
    return r;
}
// receive(bytes) - 6BBAD0, 82 B.
int NetClient::receive(unsigned char* dest, unsigned int* length)
{
    lockIf(m_threaded, m_mutex);
    const int r = m_connection->getPacketFromRecvQueue(dest, length);
    unlockIf(m_threaded, m_mutex);
    return r;
}

void         NetClient::setConnectionTimeOut(unsigned int timeout) { m_connectionTimeOut = timeout; }
unsigned int NetClient::getConnectionTimeOut() const               { return m_connectionTimeOut; }      // 6BBD40 (folded)
void         NetClient::setVersion(unsigned int version)           { m_version = version; }             // the PE stores enc(version)
unsigned int NetClient::getVersion() const                         { return m_version; }                // and returns dec(stored)

// elapsedMs - 6B73D0, 80 B: (QPC - start) / frequency * 1000, truncated. getTime - 6B7370, 85 B:
// the same plus m_timeBase (the PE computes `base - (elapsed * -1000.0)`; dbl_8AA9D0 is -1000.0).
unsigned int NetClient::elapsedMs() const
{
#if defined(_WIN32)
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    return static_cast<unsigned int>(static_cast<double>(now.QuadPart - m_startCounter)
                                     / static_cast<double>(m_counterFrequency) * 1000.0);
#else
    return 0;
#endif
}
int NetClient::getTime() const
{
    return m_timeBase + static_cast<int>(elapsedMs());
}

unsigned int NetClient::getMaxPacketSize() const { return 1472; }
float NetClient::getPacketDropRatio() const
{
    return m_socket ? m_socket->getPacketDropRatio() : 0.0f;
}
void NetClient::setPacketDropRatio(float ratio)
{
    if (m_socket)
        m_socket->setPacketDropRatio(ratio);
}
void NetClient::setStatsGatheringEnabled(bool enabled)                                                 // 6BBCB0: +0x30 of the connection
{
    if (m_connection)
        m_connection->m_byte30 = enabled;
}
bool NetClient::getStatsGatheringEnabled() const
{
    return m_connection ? m_connection->m_byte30 : false;
}
bool NetClient::isConnected() const                                                                    // the handshake state is 2
{
    return m_connection != 0 && m_connection->m_handshakeState == 2;
}
bool NetClient::getPing(int* ping) const                                                               // 6BBD50: +0x88 of the connection
{
    if (!m_connection)
        return false;
    *ping = static_cast<int>(m_connection->m_dword88);
    return true;
}
// getConnectionStatus - 6BC170, 152 B: SWConnection::getConnectionStats into the seven fields, in
// NetServer::getConnectionStatus's order (the two loss rates cross over, as there).
bool NetClient::getConnectionStatus(ConnectionStatus& status)
{
    CONNSTATSENTRY stats = {};
    if (!m_connection || !m_connection->getConnectionStats(&stats))
        return false;
    status.bytesReceivedPerSecond    = stats.bytesReceivedPerSecond;
    status.packetsReceivedPerSecond  = stats.packetsReceivedPerSecond;
    status.bytesSentPerSecond        = stats.bytesSentPerSecond;
    status.packetsSentPerSecond      = stats.packetsSentPerSecond;
    status.packetsLostSendPerSecond  = stats.packetsLostSendPerSecond;
    status.packetsLostReceivePerSecond = stats.packetsLostReceivePerSecond;
    status.ping                      = stats.ping;
    return true;
}
const std::string& NetClient::getDisconnectText() const { return m_disconnectText; }                   // +0x13C

// joinServer - 6BEE80, 275 B. Blocking: connect() is called again for as long as it answers 18
// ("no answer yet, the request was resent"); non-blocking: one pass. A 0 with a thread wanted starts
// the receive thread (a failed start shuts the client down and answers 9) and sleeps 50 ms.
// The sixth argument is the reconnect password (sendConnectRequest writes it as the 32-bit field);
// GameClient passes "". The mod name comes from GSModDirectory, not from a parameter.
int NetClient::joinServer(const std::string& address, int port, bool blocking,
                          const std::string& password, unsigned int timeoutMs,
                          const std::string& reconnectPassword, bool punkBuster)
{
    int result;
    if (blocking)
    {
        do
        {
            result = connect(address, port, true, password, timeoutMs, reconnectPassword, punkBuster);
        }
        while (result == 18);
    }
    else
        result = connect(address, port, false, password, timeoutMs, reconnectPassword, punkBuster);

    if (result == 0 && m_threaded)
    {
        if (!startReceiveThread())
        {
            shutDown();                               // 6BEF6F: `call [vptr+0x18]`
            return 9;
        }
#if defined(_WIN32)
        ::Sleep(50);
#endif
    }
    return result;
}

// Starts or advances a join to address:port. Returns a protocol result code; 18 requests another attempt.
// Retries pending requests and polls for accept or denial within the timeout window.
int NetClient::connect(const std::string& address, int port, bool blocking, const std::string& password,
                       unsigned int timeoutMs, const std::string& reconnectPassword, bool punkBuster)
{
    if (m_connection->m_handshakeState != 1)
    {
        m_connection->reset();
        m_connection->m_handshakeState = 1;
        m_stopRequested = false;
        if (!socketManager)
            return 6;
        if (password.size() >= 32)
            return 17;
        const std::string name = formatString("%s:%d", address.c_str(), port);
        if (!m_socket)
        {
            m_socket = static_cast<UDPSocket*>(socketManager->connect(name, blocking));   // vt +0x0C
            if (!m_socket)
                return 4;
        }
        m_timeBase = 0;
#if defined(_WIN32)
        LARGE_INTEGER counter;
        ::QueryPerformanceCounter(&counter);
        m_startCounter = counter.QuadPart;
        ::QueryPerformanceFrequency(&counter);
        m_counterFrequency = counter.QuadPart;
#endif
        m_connectAttemptsLeft = kConnectRequestAttempts;   // the canary seeded (0x5959C58A)
        m_mustSendRequest = true;
    }
    if (m_connectAttemptsLeft <= 0)                   // the canary test refused
    {
        m_connection->m_handshakeState = 0;
        if (m_socket)
            delete m_socket;
        m_socket = 0;
        return 4;
    }
    if (m_mustSendRequest)
    {
        m_requestSentMs = elapsedMs();
        if (sendConnectRequest(punkBuster, password, reconnectPassword))
        {
            if (m_socket)
                delete m_socket;
            m_socket = 0;
            return 6;
        }
        m_mustSendRequest = false;
    }
    unsigned char* const buffer = static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14));
    do
    {
        const int n = m_socket->receiveFrom(buffer, 1472);   // PE vt +0x0C = ISocket::receiveFrom
        if (n)
        {
            m_bitStreamIn->setBuffer(buffer, static_cast<unsigned int>(n));
            m_bitStreamIn->setReadMode();
            unsigned char id = 0;
            unsigned int  type = 0;
            if (readBasicHeader(m_bitStreamIn, id, type))
            {
                if (type == 2)
                    return handleConnectAccept();
                if (type == 3)
                    return handleConnectDenied();
            }
            else
                m_dataPacket.size = 0;
        }
        if (!blocking)
            break;
    }
    while (static_cast<unsigned int>(getTime() - static_cast<int>(m_requestSentMs)) < timeoutMs / 3);
    if (static_cast<unsigned int>(getTime() - static_cast<int>(m_requestSentMs)) > timeoutMs / 3)
    {
        m_mustSendRequest = true;
        --m_connectAttemptsLeft;                      // the canary re-keyed
    }
    return 18;
}

// sendConnectRequest - 6BE9E0, 408 B. Type 1 with the basic header's id 1: protocol version 4098,
// the build version, the PunkBuster bit, the 4-byte reconnect password, the 32-byte password and
// the 32-byte GSModDirectory (default "mods/bf2") - exactly the six fields NetServer::
// handleConnectRequest reads. No hasError test before the send; 0 sent, 6 not.
int NetClient::sendConnectRequest(bool punkBuster, const std::string& password,
                                  const std::string& reconnectPassword)
{
    char pw[32];
    std::strncpy(pw, password.c_str(), 32);
    char reconnect[4];
    std::strncpy(reconnect, reconnectPassword.c_str(), 4);
    char mod[32];
    std::strncpy(mod, settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2")).c_str(), 32);

    m_bitStreamOut->setBuffer(m_packetOutBuffer, 1473u);
    m_bitStreamOut->setBitSize(0);
    writeBasicHeader(m_bitStreamOut, 1, 1);
    unsigned int protocolVersion = 4098;
    m_bitStreamOut->writeBits(&protocolVersion, 32);
    unsigned int buildVersion = m_version;
    m_bitStreamOut->writeBits(&buildVersion, 32);
    unsigned int pb = punkBuster ? 1u : 0u;
    m_bitStreamOut->writeBits(&pb, 1);
    m_bitStreamOut->writeBits(reconnect, 32);
    m_bitStreamOut->writeBits(pw, 256);
    m_bitStreamOut->writeBits(mod, 256);
    return m_socket->send(m_packetOutBuffer, static_cast<int>(bytesOf(m_bitStreamOut))) ? 0 : 6;
}

// handleConnectAccept - 6BD410, 284 B. The id (8 bits), the server's time (32), the PunkBuster bit;
// a stream error is 6; the clock is re-based onto the server's (m_timeBase += serverTime - now);
// the connection takes the socket's peer; the ack goes out (a failed send is 6); state 2, the id
// on the connection, the handshake stamp, and the ping ring / clock-sync scalars seeded.
int NetClient::handleConnectAccept()
{
    unsigned int id = 0;
    m_bitStreamIn->readBits(&id, 8);
    m_connectionId = static_cast<int>(id);
    unsigned int serverTime = 0;
    m_bitStreamIn->readBits(&serverTime, 32);
    unsigned int pb = 0;
    m_bitStreamIn->readBits(&pb, 1);
    if (pb == 1)
    {
        // [TEMP] [CHOSEN, not recovered] PunkBuster client integration is unavailable.
        // [RETIRE-WHEN: netclient-punkbuster] Replace with validated SDK initialization and callbacks.
        std::fprintf(stderr, "[FRONTIER] dice::hfe::io::NetClient::handleConnectAccept: the server runs PunkBuster and the "
                             "PunkBuster client SDK glue (BF2.exe 436060 / 436220) is unrecovered; join a server without it\n");
        std::fflush(stderr);
        std::abort();
    }
    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return 6;
    }
    m_timeBase += static_cast<int>(serverTime) - getTime();
    m_connection->setPeerAddress(m_socket->getPeerAddress());
    if (sendConnectAcceptAck())
        return 6;
    m_connection->m_handshakeState = 2;
    m_connection->setConnectionId(m_connectionId);
    m_connection->m_handshakeStampMs = elapsedMs();
    m_pingRingFull         = false;
    m_pingSampleIndex      = 0;
    m_clockAdjustThreshold = 5;
    m_pingInterval         = 200;
    m_syncClock            = true;
    return 0;
}

// Clears connection state and reads the denial reason and optional wrong-mod name, returning the reason.
// The local name buffer adds a terminator beyond the 32-byte wire field.
int NetClient::handleConnectDenied()
{
    m_connection->m_handshakeState = 0;
    if (m_socket)
        delete m_socket;
    m_socket = 0;
    unsigned int reason = 0;
    m_bitStreamIn->readBits(&reason, 32);
    unsigned int wrongMod = 0;
    m_bitStreamIn->readBits(&wrongMod, 1);
    if (wrongMod == 1)
    {
        char mod[33];
        std::memset(mod, 0, sizeof(mod));
        m_bitStreamIn->readBits(mod, 256);
        m_disconnectText = mod;
    }
    return static_cast<int>(reason);
}

// sendConnectAcceptAck - 6BCD30, 124 B: type 4 with the assigned id; 9 when the header failed.
int NetClient::sendConnectAcceptAck()
{
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 1473u);
    m_bitStreamOut->setBitSize(0);
    if (!writeBasicHeader(m_bitStreamOut, static_cast<unsigned char>(m_connectionId), 4))
        return 9;
    return m_socket->send(m_packetOutBuffer, static_cast<int>(bytesOf(m_bitStreamOut))) ? 0 : 6;
}

// sendDisconnect - 6BCDB0, 171 B: type 5 and the reason. Only the reason - the client's disconnect
// carries none of the text NetServer::sendDisconnect appends. A stream error answers 9.
int NetClient::sendDisconnect(unsigned int reason)
{
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 1473u);
    m_bitStreamOut->setBitSize(0);
    writeBasicHeader(m_bitStreamOut, static_cast<unsigned char>(m_connectionId), 5);
    unsigned int r = reason;
    m_bitStreamOut->writeBits(&r, 32);
    if (m_bitStreamOut->hasError())
    {
        m_bitStreamOut->clearError();
        return 9;
    }
    return m_socket->send(m_packetOutBuffer, static_cast<int>(bytesOf(m_bitStreamOut))) ? 0 : 6;
}

// sendPingRequest - 6BCBE0, 186 B: the stamp on the connection first, SWConnection::sendPingRequest
// into the out stream, the socket re-aimed at the connection's peer, then the send.
int NetClient::sendPingRequest()
{
    const unsigned int now = elapsedMs();
    m_connection->m_lastPingSentMs = now;
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 1473u);
    m_bitStreamOut->setBitSize(0);
    if (m_connection->sendPingRequest(m_bitStreamOut, static_cast<unsigned char>(m_connectionId), now) != 0)
        return 9;
    m_socket->setPeerAddress(m_connection->getPeerAddress());
    return m_socket->send(m_packetOutBuffer, static_cast<int>(bytesOf(m_bitStreamOut))) ? 0 : 6;
}

// sendPingResponse - 6BCB20, 187 B: the echoed stamp and our own elapsed ms.
int NetClient::sendPingResponse(unsigned int echoTimeStamp)
{
    const unsigned int now = elapsedMs();
    m_bitStreamOut->setBuffer(m_packetOutBuffer, 1473u);
    m_bitStreamOut->setBitSize(0);
    if (m_connection->sendPingResponse(m_bitStreamOut, static_cast<unsigned char>(m_connectionId),
                                       echoTimeStamp, now) != 0)
        return 9;
    m_socket->setPeerAddress(m_connection->getPeerAddress());
    return m_socket->send(m_packetOutBuffer, static_cast<int>(bytesOf(m_bitStreamOut))) ? 0 : 6;
}

// pingConfirmation - 6BBDA0, 126 B: NetServer::pingConfirmation's body on the one connection.
int NetClient::pingConfirmation(unsigned int ackSequence)
{
    SWConnection* const c = m_connection;
    const unsigned int advance = (ackSequence - c->m_remotePacketCounter) & 0x3F;
    if (ackSequence == c->m_remotePacketCounter || advance > 0x1F)
    {
        m_dataPacket.size = 0;
        return 11;
    }
    if (advance > 1)
    {
        c->m_dword70 += advance - 1;
        c->m_delayScaleFactor <<= advance - 1;
    }
    ++c->m_dword70;
    c->m_delayScaleFactor *= 2;
    c->m_remotePacketCounter = ackSequence;
    m_dataPacket.size = 0;
    return 0;
}

// storeConnectionEvent - 6BBE20, 34 B: the event code into the packet and onto the receive ring.
int NetClient::storeConnectionEvent(unsigned int reason)
{
    m_dataPacket.connectionEventType = reason;
    return m_connection->addPacketToRecvQueue(m_dataPacket);
}

// handlePingRequest - 6BCE60, 244 B (named). NetServer::handlePingRequest's shape with the client's
// two differences: a ping response that cannot be sent stores event 6 (the server does the same),
// and an unconfirmed ack closes the connection with 13 where the server only stores 13.
int NetClient::handlePingRequest(int)
{
    m_dataPacket.size = 0;
    unsigned int extLocal = 0, extRemote = 0, extDelay = 0;
    if (!readExtendedHeader(m_bitStreamIn, extLocal, extRemote, extDelay))
        return 9;
    unsigned int haveSent = 0;
    m_bitStreamIn->readBits(&haveSent, 1);
    const bool sentFlag = (haveSent == 1);
    unsigned int timeStamp = 0;
    m_bitStreamIn->readBits(&timeStamp, 32);
    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return 9;
    }
    if (sentFlag)
        pingConfirmation(extLocal);
    const bool confirmed = m_connection->confirmPacket(extRemote, extDelay);
    if (sendPingResponse(timeStamp))
    {
        storeConnectionEvent(6);
        return 0;
    }
    if (!confirmed)
        closeConnection(13);                          // 6BCF3D: `call [vptr+0x1C]`
    return 0;
}

// handlePingResponse - 6BE370, 300 B (named). The extended header, the have-sent bit, the echoed
// stamp and the server's stamp; the round trip is elapsedMs() - echo (the request stamped elapsedMs);
// the connection's ping statistics (the five +0x88..+0x98 fields, NetServer's inline arithmetic is a
// call here, 6B7420); with clock sync on, updatePingStatistics(rtt, serverStamp, getTime()).
int NetClient::handlePingResponse(int)
{
    m_dataPacket.size = 0;
    unsigned int extLocal = 0, extRemote = 0, extDelay = 0;
    if (!readExtendedHeader(m_bitStreamIn, extLocal, extRemote, extDelay))
        return 9;
    unsigned int haveSent = 0;
    m_bitStreamIn->readBits(&haveSent, 1);
    const bool sentFlag = (haveSent == 1);
    unsigned int echoStamp = 0;
    m_bitStreamIn->readBits(&echoStamp, 32);
    unsigned int serverStamp = 0;
    m_bitStreamIn->readBits(&serverStamp, 32);
    const unsigned int now = elapsedMs();
    if (m_bitStreamIn->hasError())
    {
        m_bitStreamIn->clearError();
        return 9;
    }
    if (sentFlag)
        pingConfirmation(extLocal);
    const bool confirmed = m_connection->confirmPacket(extRemote, extDelay);
    {
        SWConnection* const c = m_connection;         // the ping statistics
        const unsigned int rtt = now - echoStamp;
        ++c->m_dword94;
        const bool underThreshold = (rtt <= c->m_dword98);
        c->m_dword88 = rtt;
        if (underThreshold)
        {
            if (rtt < c->m_dword8C)
                c->m_dword8C = rtt;
            c->m_dword90 += rtt;
        }
        else
        {
            c->m_dword98 = rtt;
            c->m_dword90 += rtt;
        }
    }
    if (m_syncClock)
        updatePingStatistics(m_connection->m_dword88, serverStamp, getTime());
    if (!confirmed)
        closeConnection(13);
    m_dataPacket.size = 0;
    return 0;
}

// updatePingStatistics - 6BE1E0, 172 B. A non-zero round trip under a second enters the 32-entry
// ring; a full ring (the flag stays up) is sorted (6BE000 - an introsort; the order is all that
// matters) and its 10th / 15th / 26th entries kept; when this round trip is at or under the low
// mark the clock is re-based by (serverStamp + rtt/2 - now) if that exceeds the threshold.
void NetClient::updatePingStatistics(unsigned int rtt, unsigned int serverStamp, int now)
{
    if (rtt < 1000 && rtt)
        m_pingSamples[m_pingSampleIndex++] = static_cast<int>(rtt);
    if (m_pingSampleIndex == 32)
    {
        m_pingSampleIndex = 0;
        m_pingRingFull    = true;
    }
    if (m_pingRingFull)
    {
        int sorted[32];
        std::memcpy(sorted, m_pingSamples, sizeof(sorted));
        std::sort(sorted, sorted + 32);
        const unsigned int low = static_cast<unsigned int>(sorted[10]);
        m_pingMedian = sorted[15];
        m_pingLow    = sorted[10];
        m_pingHigh   = sorted[26];
        if (rtt <= low)
        {
            const int delta = static_cast<int>(serverStamp + (rtt >> 1)) - now;
            if (m_clockAdjustThreshold < std::abs(delta))
                m_timeBase += delta;
        }
    }
}

// handleDataPacket - 6BCF60, 571 B (named). NetServer::handleDataPacket with the client's shape: an
// unconfirmed ack closes the connection (13) rather than storing an event, a full ring queues event
// 14 and answers 14, and a queue that refuses the packet logs "RecvQueue overflow" (Debug 6, line
// 1320) - the server does not log. The sequence numbers are named as the server names them.
int NetClient::handleDataPacket(int, unsigned int size)
{
    SWConnection* const c = m_connection;
    ++c->m_dword60;
    unsigned int local = 0, remote = 0, delay = 0;
    readExtendedHeader(m_bitStreamIn, local, remote, delay);
    if (local == c->m_remotePacketCounter || ((local - c->m_remotePacketCounter) & 0x3F) > 0x1F)
    {
        m_dataPacket.size = 0;
        return 11;
    }
    if (!c->confirmPacket(remote, delay))
        closeConnection(13);                          // 6BCFE9: `call [vptr+0x1C]`
    const bool room = c->m_recvBuffer != 0 && static_cast<int>(256 - c->m_recvBuffer->m_count) > 0;
    if (room)
    {
        m_dataPacket.size = size;
        m_dataPacket.currBytePositionBitStream = m_bitStreamIn->getBitPosition() >> 3;
        m_dataPacket.dword8 = local;
        const unsigned int behind = (local - c->m_remotePacketCounter) & 0x3F;
        if (behind > 1)
        {
            c->m_dword70 += behind - 1;
            c->m_delayScaleFactor <<= behind - 1;
        }
        c->m_delayScaleFactor = (2 * c->m_delayScaleFactor) | 1;
        c->m_remotePacketCounter = local;
        ++c->m_dword64;
        c->m_dword6C += size;
        m_dataPacket.connectionEventType = 0;
        if (!c->addPacketToRecvQueue(m_dataPacket))
            Debug(DEBUGTYPE_6, NETCLIENT_FILE, 1320, "") << formatString("RecvQueue overflow");
        m_dataPacket.size = 0;
        return 0;
    }
    m_dataPacket.size = 0;
    const unsigned int behind = (local - c->m_remotePacketCounter) & 0x3F;
    if (behind > 1)
    {
        c->m_dword70 += behind - 1;
        c->m_delayScaleFactor <<= behind - 1;
    }
    ++c->m_dword70;
    c->m_delayScaleFactor *= 2;
    c->m_remotePacketCounter = local;
    m_dataPacket.connectionEventType = 14;
    c->addPacketToRecvQueue(m_dataPacket);
    return 14;
}

// handleDisconnect - 6BE0C0, 109 B (named). The reason (32 bits) and a length-prefixed text (16-bit
// length, then the bytes - 54B290) into m_disconnectText; the reason queued as an event, then 13
// queued as another; 13 is the answer. No stream-error test, as shipped.
int NetClient::handleDisconnect(int)
{
    unsigned int reason = 0;
    m_bitStreamIn->readBits(&reason, 32);
    unsigned int length = 0;
    m_bitStreamIn->readBits(&length, 16);
    if (length)
    {
        std::vector<char> text(length);
        m_bitStreamIn->readBits(&text[0], 8 * length);
        m_disconnectText.assign(&text[0], length);
    }
    else
        m_disconnectText.resize(0);
    m_dataPacket.connectionEventType = reason;
    m_connection->addPacketToRecvQueue(m_dataPacket);
    m_dataPacket.connectionEventType = 13;
    m_connection->addPacketToRecvQueue(m_dataPacket);
    return 13;
}

// Receives and dispatches client packets while maintaining timeout and keepalive state.
// Runs under the connection mutex and returns protocol result codes to the receive thread.
int NetClient::_update(int*)
{
    if (!m_totalTimeSentinel)
        m_totalTimeSentinel = elapsedMs();
    if (!m_bInit || !m_socket)
        return 8;
    if (!m_bDebugNetwork)
    {
        const unsigned int state = m_connection->m_handshakeState;
        if (state == 1 || state == 2)
        {
            if (elapsedMs() > m_connection->m_handshakeStampMs + m_connectionTimeOut)
                storeConnectionEvent(10);
        }
    }
    if (m_connection->m_handshakeState == 2)
    {
        unsigned int limit;
        if (!m_syncClock || m_pingRingFull)
            limit = 3000;
        else
            limit = m_pingInterval <= 3000 ? m_pingInterval : 3000;
        if (elapsedMs() > m_connection->m_lastPingSentMs + limit)
            sendPingRequest();
    }
    unsigned char* const buffer = static_cast<unsigned char*>(const_cast<void*>(m_dataPacket.pvoid14));
    for (;;)
    {
        const int n = m_socket->receive(buffer, 1472);       // PE vt +0x10 = ISocket::receive
        if (!(n > 0))                                        // the obfuscated `received > 0` test
            break;
        if (!m_packetFilter.processPacket(buffer, static_cast<unsigned int>(n)))
        {
            std::string address;
            m_socket->addressToString(m_socket->getPeerAddress(), &address);
            m_packetFilter.storePacket(buffer, static_cast<unsigned int>(n), FILTEREDPACKETTYPE_0, address.c_str());
            m_dataPacket.size = 0;
            continue;
        }
        m_bitStreamIn->setBuffer(buffer, static_cast<unsigned int>(n));
        m_bitStreamIn->setReadMode();
        unsigned char id = 0;
        unsigned int  type = 0;
        if (!readBasicHeader(m_bitStreamIn, id, type))
        {
            m_dataPacket.size = 0;
            return 11;
        }
        m_dataPacket.dword10 = elapsedMs();
        m_connection->m_handshakeStampMs = m_dataPacket.dword10;
        switch (type)
        {
        case 2:
            if (handleConnectAccept())
                return 6;
            m_dataPacket.size = 0;
            break;
        case 5:
            m_dataPacket.size = 0;
            return handleDisconnect(0);
        case 7:
        {
            m_dataPacket.size = 0;
            const int r = handlePingRequest(0);
            if (r)
                return r;
            break;
        }
        case 8:
            handlePingResponse(0);
            break;
        case 15:
            m_dataPacket.size = static_cast<unsigned int>(n);
            return handleDataPacket(0, static_cast<unsigned int>(n));
        default:
            m_dataPacket.size = 0;
            return 11;
        }
    }
    m_dataPacket.size = 0;
    return 8;
}

// Windows receive thread: waits for socket readability, then updates under the mutex until a terminal result.
// Startup creates and registers the thread; shutdown signals, joins and closes its handles.
#if defined(_WIN32)
namespace {
    DWORD WINAPI netClientThreadProc(LPVOID parameter)
    {
        NetClient* const client = static_cast<NetClient*>(parameter);
        ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        std::vector<ISocket*> sockets(1, static_cast<ISocket*>(client->m_socket));
        while (!client->m_stopRequested)
        {
            socketManager->sleep(sockets, 3.0f);                                 // vt +0x20
            ::WaitForSingleObject(client->m_mutex, INFINITE);
            int result;
            do
            {
                result = client->_update(0);                                     // `call [vptr+0x38]`
            }
            while (result != 8 && result != 14);
            ::ReleaseMutex(client->m_mutex);
        }
        return 0;
    }
}
#endif

bool NetClient::startReceiveThread()
{
#if defined(_WIN32)
    m_mutex = ::CreateMutexA(0, 0, 0);
    DWORD threadId = 0;
    m_thread = ::CreateThread(0, 0, &netClientThreadProc, this, 0, &threadId);
    if (m_thread)
    {
        // The shipped signature takes the handle as a 32-bit value; its (empty) body ignores it, so
        // the explicit narrowing on x64 loses nothing.
        static_cast<Profiler*>(profiler)->registerThread(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(m_thread)), threadId,
                                                         std::string("NetClientThread"));   // 4594F0 on the Profiler singleton
        return true;
    }
#endif
    Debug(DEBUGTYPE_6, NETCLIENT_FILE, 1416, "") << "clientReceiveThread was not created successfully";
    return false;
}

void NetClient::stopReceiveThread()
{
#if defined(_WIN32)
    if (m_thread && m_mutex)
    {
        m_stopRequested = true;
        if (m_threaded)
            ::WaitForSingleObject(m_thread, INFINITE);
        ::CloseHandle(m_mutex);
        m_mutex = 0;
        ::CloseHandle(m_thread);
        m_thread = 0;
    }
#endif
}

// sendRawPacket - 6BBEB0, 160 B. NetServer::sendRawPacket's client twin under the mutex: an AF_INET
// sockaddr from inet_addr / htons, the socket aimed at it, one send. No socket null test, as
// shipped; IGame::sendUDPPacket only reaches it while a NetClient exists.
void NetClient::sendRawPacket(const char* address, unsigned short port, const void* data, int size)
{
#if defined(_WIN32)
    ::WaitForSingleObject(m_mutex, INFINITE);
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(port);
    sa.sin_addr.s_addr = inet_addr(address);
    ISocket::Address peer;
    std::memcpy(peer.sockaddr, &sa, 16);
    peer.length = 16;
    m_socket->setPeerAddress(peer);
    m_socket->send(static_cast<const unsigned char*>(data), size);
    ::ReleaseMutex(m_mutex);
#else
    (void) address; (void) port; (void) data; (void) size;
#endif
}

} // namespace io
} // namespace hfe
} // namespace dice

#endif // !dedicated
