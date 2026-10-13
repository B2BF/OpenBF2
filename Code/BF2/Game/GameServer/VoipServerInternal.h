// In-process voice server: the facade instance plus the thread that pumps it.
//
// Recovered from fh2 (VoipServerInternal 0x4CD4C0-0x4CD7BF). VoipServerConnection::Init creates one
// when voice is local and not remote, then Start()s it. The thread takes the same mutex the facade
// owns, ticks CBF2VoipServer::Update, and sleeps 10 ms between ticks.
//
// Member offsets are the recovered x86-64 ELF; the CBF2VoipServer layout lives in
// Code/BF2/VoipServer/VoipServerApi.h.

#ifndef GAME_GAMESERVER_VOIPSERVERINTERNAL_H   // [INFERRED] guard style
#define GAME_GAMESERVER_VOIPSERVERINTERNAL_H

#include <string>

#include "../../Core/Mutex.h"
#include "../../Core/Thread.h"
#include "../../VoipServer/VoipServerApi.h"

namespace dice {
namespace hfe {

// Pumps the voice server on its own thread until Stop is requested.
class VoipServerInternalHookThread : public Thread
{
public:
    VoipServerInternalHookThread();
    virtual ~VoipServerInternalHookThread();

    // Records the facade to tick and the mutex shared with the owner. [INFERRED] name.
    void init(BF2VoipServer::CBF2VoipServer* server, Mutex* mutex);

    // Requests the run loop to exit; the flag is read under the shared mutex.
    void requestStop();

    virtual void run();

private:
    unsigned int m_state;                        // +0x10
    Mutex* m_mutex;                              // +0x18
    bool m_stopFlag;                             // +0x20
    BF2VoipServer::CBF2VoipServer* m_server;     // +0x28
};

// Owns the local voice server and its pump thread.
class VoipServerInternal
{
public:
    VoipServerInternal();
    ~VoipServerInternal();

    // Initializes the facade, creates one instance on port, and starts the pump thread.
    void Start(const std::string& password, unsigned int port);

    // Requests the pump to stop, joins it, and shuts the facade down.
    void Stop();

private:
    BF2VoipServer::CBF2VoipServer m_facade;      // +0x00
    VoipServerInternalHookThread m_hookThread;   // +0x38
    BF2VoipServer::CBF2VoipServer m_server;      // +0x68
    Mutex m_mutex;                               // +0xA0
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMESERVER_VOIPSERVERINTERNAL_H
