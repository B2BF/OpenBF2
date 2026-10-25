// [TEMP] Runtime checks for the CBF2VoipServer facade (Phase 6).
//
// Exercises the instance registry and lifecycle: create-before-init refusal, double-init refusal,
// instance ids, getInstance bounds, update, per-instance and bulk release, and shutdown/reinit
// behaviour. Binds loopback ports 27185/27186 (never 16567).

#include <winsock2.h>
#include <cstdio>
#include <cstring>

#include "../BF2/VoipServer/VoipServerApi.h"
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

int main()
{
    using namespace BF2VoipServer;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    CBF2VoipServer server;
    int id = -123;

    expect(!server.IsRunning(), "starts not running");
    expect(server.getNumInstances() == 0, "starts with no instances");
    expect(server.getInstance(0) == 0, "getInstance empty");

    expect(server.CreateInstance(27185, "pw", id) == VOIP::VRESULT_INVALID_STATE && id == -1,
           "create before initialize is refused");

    expect(server.Initialize(false) == VOIP::VRESULT_SUCCESS, "initialize");
    expect(server.Initialize(false) == VOIP::VRESULT_INVALID_STATE, "double initialize refused");
    expect(server.IsRunning(), "running after initialize");

    expect(server.CreateInstance(27185, "pw", id) == VOIP::VRESULT_SUCCESS, "create instance");
    expect(id == 1, "first instance id is 1");
    expect(server.getNumInstances() == 1, "one instance");
    expect(server.getInstance(0) != 0, "getInstance(0) returns the instance");
    expect(server.getInstance(1) == 0, "getInstance out of range is null");
    expect(server.getInstance(0)->GetInstanceId() == 1, "instance carries its id");

    expect(server.Update() == VOIP::VRESULT_SUCCESS, "update");
    expect(server.ReleaseInstance(id) == VOIP::VRESULT_SUCCESS, "release instance");
    expect(server.getNumInstances() == 0, "instance released");
    expect(server.ReleaseInstance(999) == VOIP::VRESULT_FAILURE, "release missing instance");

    expect(server.CreateInstance(27185, "pw", id) == VOIP::VRESULT_SUCCESS, "create instance again");
    int second = -1;
    expect(server.CreateInstance(27186, "pw", second) == VOIP::VRESULT_SUCCESS, "create a second instance");
    expect(id != second, "instance ids differ");
    expect(server.getNumInstances() == 2, "two instances");

    expect(server.ReleaseAllInstances() == VOIP::VRESULT_SUCCESS, "release all instances");
    expect(server.getNumInstances() == 0, "all instances released");

    expect(server.Shutdown() == VOIP::VRESULT_SUCCESS, "shutdown");
    expect(!server.IsRunning(), "not running after shutdown");
    expect(server.Shutdown() == VOIP::VRESULT_INVALID_STATE, "double shutdown refused");
    expect(server.CreateInstance(27185, "pw", id) == VOIP::VRESULT_INVALID_STATE, "create after shutdown is refused");

    std::printf("voip facade: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
