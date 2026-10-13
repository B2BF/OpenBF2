// In-process voice server implementation.
//
// Recovered from fh2: VoipServerInternalHookThread 0x4CD4C0/0x4CD590/0x4CD680/0x4CD520 and
// VoipServerInternal 0x4CD4D0/0x4CD530/0x4CD5C0/0x4CD6F0.

#include "VoipServerInternal.h"

#include "../../Core/System.h"

namespace dice {
namespace hfe {

VoipServerInternalHookThread::VoipServerInternalHookThread()
    : Thread()
    , m_state(0)
    , m_mutex(0)
    , m_stopFlag(false)
    , m_server(0)
{
}

VoipServerInternalHookThread::~VoipServerInternalHookThread()
{
}

void VoipServerInternalHookThread::init(BF2VoipServer::CBF2VoipServer* server, Mutex* mutex)
{
    m_server = server;
    m_mutex = mutex;
}

void VoipServerInternalHookThread::requestStop()
{
    m_stopFlag = true;
}

// Holds the shared mutex while it ticks the facade, releases it across the 10 ms sleep, and exits
// once the stop flag is set. The state field records 1 while ticking and 2 while sleeping.
void VoipServerInternalHookThread::run()
{
    m_mutex->lock();
    if (!m_stopFlag)
    {
        bool keepGoing;
        do
        {
            m_server->Update();
            m_state = 1;
            m_mutex->unlock();
            g_system->sleep(10);
            m_mutex->lock();
            keepGoing = (m_stopFlag == 0);
            m_state = 2;
        }
        while (keepGoing);
    }
    m_mutex->unlock();
}

VoipServerInternal::VoipServerInternal()
    : m_facade()
    , m_hookThread()
    , m_server()
    , m_mutex()
{
}

VoipServerInternal::~VoipServerInternal()
{
}

// The facade is started in the default (non-local) socket-manager mode, then one instance is
// created on port. The pump thread runs at above-normal priority.
void VoipServerInternal::Start(const std::string& password, unsigned int port)
{
    m_server.Initialize(false);
    int instanceId = -1;
    m_server.CreateInstance(port, password, instanceId);
    m_hookThread.init(&m_server, &m_mutex);
    m_hookThread.start(0, Thread::Priority_AboveNormal);
}

void VoipServerInternal::Stop()
{
    m_mutex.lock();
    m_hookThread.requestStop();
    m_mutex.unlock();
    m_hookThread.stop();
    m_server.Shutdown();
}

} // namespace hfe
} // namespace dice
