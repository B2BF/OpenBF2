// Voice-server diagnostics implementation.
//
// CDebugStandard::Output is a bare return in the shipped images, so the standard messages are
// discarded. CDebugRoutingState::Enabled and the constructors are recovered here; the routing-dump
// and voice-trace bodies are named frontiers until the debug phase.

#include <cstdio>
#include <cstdlib>

#include "VoipDebug.h"

namespace BF2VoipServer {

// The singleton lives here so Close() can reset the pointer GetInstance() caches.
static CDebugStandard* s_standardInstance = 0;

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: voip-debug] Named stop for a debug body that is not
// yet recovered. Remove each call when its real body lands (the debug phase).
#define BF2_VOIP_DEBUG_FRONTIER(name)                                                    \
    do {                                                                                 \
        std::fprintf(stderr, "[FRONTIER] BF2VoipServer::%s reached\n", name);            \
        std::fflush(stderr);                                                             \
        std::abort();                                                                    \
    } while (0)

// ---- CDebugStandard ----

CDebugStandard::CDebugStandard()
    : m_stream(0)
{
}

CDebugStandard::~CDebugStandard()
{
}

CDebugStandard* CDebugStandard::GetInstance()
{
    if (!s_standardInstance)
        s_standardInstance = new CDebugStandard();
    return s_standardInstance;
}

void CDebugStandard::Close()
{
    if (s_standardInstance)
    {
        delete s_standardInstance;
        s_standardInstance = 0;
    }
}

// The shipped Output is a single retn; every message is discarded.
void CDebugStandard::Output(StandardDebugOutputType type, const char* message)
{
    (void)type;
    (void)message;
}

// ---- CDebugRoutingState ----

CDebugRoutingState::CDebugRoutingState()
    : m_state(0)
{
}

CDebugRoutingState::~CDebugRoutingState()
{
}

bool CDebugRoutingState::Enabled()
{
    return m_state != 0;
}

void CDebugRoutingState::Start()
{
    BF2_VOIP_DEBUG_FRONTIER("CDebugRoutingState::Start");
}

void CDebugRoutingState::OutputFullState(CBF2VoipServerInstance* instance, unsigned char packetType)
{
    (void)packetType;
    if (!instance || !Enabled())
        return;
    BF2_VOIP_DEBUG_FRONTIER("CDebugRoutingState::OutputFullState");
}

void CDebugRoutingState::OutputRDMismatch(CBF2VoipServerInstance* instance, int channel,
                                          const BF2VoipPacketData::RoutingDataPlayer& serverSide,
                                          const BF2VoipPacketData::RoutingDataPlayer& packetSide)
{
    (void)channel;
    (void)serverSide;
    (void)packetSide;
    if (!instance || !Enabled())
        return;
    BF2_VOIP_DEBUG_FRONTIER("CDebugRoutingState::OutputRDMismatch");
}

// ---- CDebugVoiceTrace ----

CDebugVoiceTrace::CDebugVoiceTrace()
    : m_ptr(0), m_field08(0), m_field0C(0)
{
}

CDebugVoiceTrace::~CDebugVoiceTrace()
{
}

void CDebugVoiceTrace::Start(const char* name, int playerId, unsigned int connectionId)
{
    (void)name;
    (void)playerId;
    (void)connectionId;
    BF2_VOIP_DEBUG_FRONTIER("CDebugVoiceTrace::Start");
}

void CDebugVoiceTrace::Output(unsigned int fromConnection, unsigned int toConnection,
                              const char* fromName, const char* toName, char* data, int length)
{
    (void)fromConnection;
    (void)toConnection;
    (void)fromName;
    (void)toName;
    (void)data;
    (void)length;
    BF2_VOIP_DEBUG_FRONTIER("CDebugVoiceTrace::Output");
}

void CDebugVoiceTrace::Reset()
{
    BF2_VOIP_DEBUG_FRONTIER("CDebugVoiceTrace::Reset");
}

void CDebugVoiceTrace::Stop(unsigned int connectionId)
{
    (void)connectionId;
    BF2_VOIP_DEBUG_FRONTIER("CDebugVoiceTrace::Stop");
}

bool CDebugVoiceTrace::IsRunning()
{
    BF2_VOIP_DEBUG_FRONTIER("CDebugVoiceTrace::IsRunning");
    return false;
}

} // namespace BF2VoipServer
