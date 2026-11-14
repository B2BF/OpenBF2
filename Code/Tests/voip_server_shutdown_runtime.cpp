// [TEMP] The voice server's own shutdown (CTest bf2_voip_server_shutdown_check). CBF2VoipServer::Shutdown
// holds mMutex and calls ReleaseAllInstances, which locks it again (BF2.exe and ELF alike); the local
// host's VoipServerInternal::Stop reaches it whenever a hosted game with voice ends. With a
// non-recursive dice::hfe::Mutex that relock hangs forever; the watchdog turns a hang into a failure.
#include "../BF2/VoipServer/VoipServerApi.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

static unsigned checks, failures;
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

int main()
{
    std::thread watchdog([] {
        std::this_thread::sleep_for(std::chrono::seconds(15));
        std::printf("FAIL watchdog: CBF2VoipServer::Shutdown hung (mMutex relocked by ReleaseAllInstances)\n");
        std::fflush(stdout);
        std::_Exit(2);
    });
    watchdog.detach();

    BF2VoipServer::CBF2VoipServer server;
    check(server.Initialize(false) == VOIP::VRESULT_SUCCESS, "Initialize (socket-manager mode, as VoipServerInternal::Start)");
    int id = -1;
    check(server.CreateInstance(55197, std::string("vtestsecret"), id) == VOIP::VRESULT_SUCCESS && id > 0,
          "one instance created");
    check(server.Shutdown() == VOIP::VRESULT_SUCCESS, "Shutdown returns (releasing every instance under the same mutex)");
    check(server.Shutdown() == VOIP::VRESULT_INVALID_STATE, "a second Shutdown reports not running");
    std::printf("voip server shutdown runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
