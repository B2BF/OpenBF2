// BF2VoipServer standalone shell: the recovered voice server as its own process.
//
// Recovered from BF2VoipServer.exe (Patch_1_50), BF2VoipServerShell.cpp: the run body 0x401830,
// the Voip.con reader 0x401540, the tick loop 0x402010 and the console printer 0x401190. The
// shipped shell also creates g_system/eventManager/classManager and drives its own frame tick; the
// recovered module references none of those globals, so this shell uses a plain sleep loop
// ([CHOSEN, not recovered]).
//
// NOT RECOVERED: the shipped tick loop also redraws a per-tick status banner (sub_401C10) through
// the engine console device (off_42D434), which is what keeps the retail "Battlefield 2 VoIP Server"
// banner pinned at the top of the window. That render and the console device are not recovered; the
// loop below drives only Update/IsRunning and never draws the status banner.
//
// It reads Voip.con lines of "<port> <password>", starts one instance per port, drains each
// instance's status queue (the messages the shipped console showed), ticks the facade until told to
// stop, then shuts down. It drives the recovered module through its imported facade, so it is also
// the drop-in replacement for the retail BF2VoipServer.exe whose std::string ABI does not match a
// modern build.

#include <cstdio>
#include <fstream>
#include <string>

#include "VoipServerApi.h"
#include "VoipServerInstance.h"

#if defined(_WIN32)
# include <windows.h>
#endif

namespace {

void printLine(const char* text)
{
    std::printf("%s\n", text);
    std::fflush(stdout);
}

void sleepMilliseconds(unsigned int milliseconds)
{
#if defined(_WIN32)
    ::Sleep(milliseconds);
#else
    // The module builds on Windows only; this arm keeps a non-Windows syntax check honest.
    (void)milliseconds;
#endif
}

// Reads Voip.con ("<port> <password>" per line) and starts one instance per entry, mirroring the
// shipped shell's failure text. A missing file is fatal; per-line failures are reported and skipped.
bool startInstances(BF2VoipServer::CBF2VoipServer& server)
{
    std::ifstream config("Voip.con");
    if (!config.is_open())
    {
        printLine("*** Error: Unable to open configuration file ***");
        return false;
    }

    int port = 0;
    std::string password;
    while (config >> port >> password)
    {
        int instanceId = -1;
        if (server.CreateInstance(static_cast<unsigned int>(port), password, instanceId)
            != VOIP::VRESULT_SUCCESS)
        {
            char message[512];
            std::sprintf(message,
                         "*** Error: Unable to create instance with port: %d and Shared Password: %s ***",
                         port, password.c_str());
            printLine(message);
        }
    }
    return true;
}

// Drains every instance's status queue, the messages the shipped console printed above the prompt
// ("BF2 server ... connected" / "invalid version" / "invalid authentication").
void drainInstanceOutput(BF2VoipServer::CBF2VoipServer& server)
{
    const unsigned int instanceCount = server.getNumInstances();
    for (unsigned int i = 0; i < instanceCount; ++i)
    {
        BF2VoipServer::CBF2VoipServerInstance* instance = server.getInstance(i);
        if (!instance)
            continue;

        for (;;)
        {
            std::string message = instance->popOutputString();
            if (message.empty())
                break;
            printLine(message.c_str());
        }
    }
}

#if defined(_WIN32)
volatile LONG g_stop = 0;

BOOL WINAPI consoleHandler(DWORD eventType)
{
    if (eventType == CTRL_C_EVENT || eventType == CTRL_BREAK_EVENT || eventType == CTRL_CLOSE_EVENT)
    {
        ::InterlockedExchange(&g_stop, 1);
        return TRUE;
    }
    return FALSE;
}
#endif

} // namespace

int main()
{
    BF2VoipServer::CBF2VoipServer server;

#if defined(_WIN32)
    char title[128];
    std::sprintf(title, "BF2 VoIP (v%i.%i.%i.%i, pid: %d)", 1, 5, 3153, 0,
                 static_cast<int>(::GetCurrentProcessId()));
    ::SetConsoleTitleA(title);
    ::SetConsoleCtrlHandler(consoleHandler, TRUE);
#endif

    if (server.Initialize(true) != VOIP::VRESULT_SUCCESS)
    {
        printLine("*** Error: Unable to Initialize Server ***");
        return 1;
    }

    startInstances(server);
    printLine("Server Running");

    // NOT RECOVERED: the shipped loop redraws its per-tick status banner here (sub_401C10 via the
    // engine console device); this loop only pumps the facade.
    for (;;)
    {
#if defined(_WIN32)
        if (::InterlockedCompareExchange(&g_stop, 0, 0) != 0)
            break;
#endif
        if (!server.IsRunning())
            continue;

        drainInstanceOutput(server);
        server.Update();
        sleepMilliseconds(10);
    }

    server.Shutdown();
    return 0;
}
