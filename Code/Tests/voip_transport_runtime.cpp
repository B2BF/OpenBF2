// [TEMP] Runtime checks for the CBF2VoipServerInstance UDP transport (Phase 3b).
//
// Binds two instances to distinct non-16567 loopback ports, sends a frame from one and reads it on
// the other, then verifies the source address decode and rebinding. Compiles the recovered module
// together with its UDPSocket, SystemInfo and MD5 dependencies.

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <windows.h>
#endif

#include "../BF2/VoipServer/VoipServerInstance.h"

static int g_checks = 0;
static int g_failures = 0;

static void expect(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

static void pause()
{
#if defined(_WIN32)
    Sleep(10);
#endif
}

int main()
{
    using namespace BF2VoipServer;
    typedef VOIP::VRESULT Result;

#if defined(_WIN32)
    // The engine normally starts Winsock during network init; the harness must do it itself.
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    const int portA = 27182;
    const int portB = 27183;

    CBF2VoipServerInstance a;
    CBF2VoipServerInstance b;

    Result ra = a.Start(portA, 55123, "secret-a", 0);
    Result rb = b.Start(portB, 55123, "secret-b", 1);
    expect(ra == VOIP::VRESULT_SUCCESS, "A Start");
    expect(rb == VOIP::VRESULT_SUCCESS, "B Start");
    expect(a.GetListenPort() == portA && b.GetListenPort() == portB, "listen ports recorded");

    VOIP::ClientData from;
    std::memset(&from, 0, sizeof(from));
    from.m_sessionId = 1;
    from.m_playerId = 7;

    VOIP::ClientData to;
    std::memset(&to, 0, sizeof(to));
    to.m_sessionId = 2;
    to.m_playerId = 9;
    to.m_clientId = static_cast<unsigned short>(b.GetListenPort());
    to.m_connectionId = 0x0100007Fu;   // loopback address bytes, as Receive would store them

    const char payload[] = "hello";
    Result sendResult = a.Send(false, &from, &to, const_cast<char*>(payload), 5);
    expect(sendResult == VOIP::VRESULT_SUCCESS, "Send succeeds");

    char buffer[64];
    int length = 0;
    unsigned int packetType = 0;
    unsigned short sessionId = 0;
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        length = 0;
        if (b.Receive(packetType, sessionId, buffer, 64, length) == VOIP::VRESULT_SUCCESS && length > 0)
            break;
        pause();
    }

    expect(length == 5, "received the sent length");
    expect(length == 5 && std::memcmp(buffer, "hello", 5) == 0, "received the sent payload");
    expect(sessionId == static_cast<unsigned short>(portA), "source port decoded as A's listen port");
    expect(packetType == 0x0100007Fu, "source address decoded as loopback");

    // A non-blocking receive with nothing waiting reports a zero length.
    {
        int emptyLength = -1;
        unsigned int emptyType = 0;
        unsigned short emptySession = 0;
        Result r = b.Receive(emptyType, emptySession, buffer, 64, emptyLength);
        expect(r == VOIP::VRESULT_SUCCESS && emptyLength == 0, "idle receive reports an empty datagram");
    }

    expect(a.Stop() == VOIP::VRESULT_SUCCESS, "A Stop");
    expect(b.Stop() == VOIP::VRESULT_SUCCESS, "B Stop");

    // The port is immediately reusable after Stop.
    CBF2VoipServerInstance c;
    expect(c.Start(portB, 55123, "secret-c", 2) == VOIP::VRESULT_SUCCESS, "rebind B's port after Stop");
    c.Stop();

    std::printf("voip transport: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
