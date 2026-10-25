// [TEMP] Runtime checks for CBF2VoipServerInstance::BF2ServerPacketAuthentication (Phase 4).
//
// Drives the handshake gate directly: voice frames and keep-alives pass, unknown packet ids are
// rejected, a matching (session id + version) handshake connects and records the BF server, and a
// second server is refused. The private challenge fields are read through a local private/public
// include so the test uses this build's real layout rather than a hard-coded offset.

#include <winsock2.h>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#define private public
#define protected public
#include "../BF2/VoipServer/VoipServerInstance.h"
#undef private
#undef protected

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

int main()
{
    using namespace BF2VoipServer;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    CBF2VoipServerInstance instance;
    expect(instance.Start(27184, 55123, "secret", 0) == VOIP::VRESULT_SUCCESS, "Start");

    const VOIP::DataString& challenge = instance.m_field08D8;    // MD5(session) challenge, 20 bytes
    const VOIP::DataString& version = instance.m_challengeString; // fixed version string, 24 bytes
    expect(challenge.m_count == 20, "challenge hash is 20 bytes");
    expect(version.m_count == 24, "version string is 24 bytes");

    int length = 0;

    // Voice frames and keep-alives bypass authentication; other low ids and unknown ids do not.
    expect(instance.BF2ServerPacketAuthentication(1, 1, 1, 0, length) == true, "packet 1 (voice) passes");
    expect(instance.BF2ServerPacketAuthentication(6, 1, 1, 0, length) == true, "packet 6 (keep-alive) passes");
    expect(instance.BF2ServerPacketAuthentication(0, 1, 1, 0, length) == false, "packet 0 rejected");
    expect(instance.BF2ServerPacketAuthentication(7, 1, 1, 0, length) == false, "packet 7 rejected");
    expect(instance.BF2ServerPacketAuthentication(104, 1, 1, 0, length) == false, "packet 104 rejected");
    expect(instance.BF2ServerPacketAuthentication(199, 1, 1, 0, length) == false, "packet 199 rejected");

    // A BF-server packet with a wrong session id is refused while unconnected.
    char wrong[44];
    std::memcpy(wrong, version.m_data, 24);
    std::memset(wrong + 24, 0xAB, 20);
    length = 44;
    expect(instance.BF2ServerPacketAuthentication(100, 0x0100007Fu, 12346, wrong, length) == false,
           "wrong session id rejected");

    // A matching session id and version connects and records the BF server.
    char packet[44];
    std::memcpy(packet, version.m_data, 24);
    std::memcpy(packet + 24, challenge.m_data, 20);
    length = 44;
    expect(instance.BF2ServerPacketAuthentication(100, 0x0100007Fu, 12345, packet, length) == true,
           "matching handshake accepted");
    expect(instance.GetBFServerPort() == 12345, "BF server port recorded");
    expect(instance.GetBFServerIPAddr() == 0x0100007Fu, "BF server IP recorded");

    // A second BF server is refused while one is connected.
    length = 44;
    expect(instance.BF2ServerPacketAuthentication(100, 0x0100007Fu, 12347, packet, length) == false,
           "second BF server refused while connected");

    // The already-connected BF server re-authenticates cleanly.
    length = 44;
    expect(instance.BF2ServerPacketAuthentication(100, 0x0100007Fu, 12345, packet, length) == true,
           "connected BF server re-authenticates");

    expect(instance.Stop() == VOIP::VRESULT_SUCCESS, "Stop");

    std::printf("voip auth: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
