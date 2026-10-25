// Voice-server diagnostics. CDebugStandard is the standard-output sink; in the recovered images its
// Output is a bare return, so every message these classes are handed is discarded. CDebugRoutingState
// and CDebugVoiceTrace carry the per-instance state the routing code gates on and drives.
//
// Layouts are the recovered x86-64 ELF. CDebugRoutingState and CDebugVoiceTrace are 16 bytes;
// CDebugStandard is 8. The heavy routing-dump bodies (OutputFullState/OutputRDMismatch/Start and the
// voice-trace Output/Start) land in the debug phase; Enabled and the constructors are recovered here
// because normal routing paths call them.

#ifndef BF2_VOIPSERVER_VOIPDEBUG_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPDEBUG_H

namespace BF2VoipPacketData { struct RoutingDataPlayer; }

namespace BF2VoipServer {

class CBF2VoipServerInstance;

// Standard-output debug sink. Output is a no-op in the shipped images.
class CDebugStandard
{
public:
    // Severity label. The name set is [INFERRED]; only the values the module passes are attested.
    enum StandardDebugOutputType
    {
        DEBUG_OUTPUT_ROUTING = 0,
        DEBUG_OUTPUT_INFO    = 1,
        DEBUG_OUTPUT_WARN    = 2,
        DEBUG_OUTPUT_ERROR   = 3
    };

    static CDebugStandard* GetInstance();

    // Destroys the singleton and clears the cached pointer.
    static void Close();

    void Output(StandardDebugOutputType type, const char* message);

private:
    CDebugStandard();
    ~CDebugStandard();

    void* m_stream;      // +0x00
};

// Per-instance routing-dump state; 16 bytes. Enabled gates the voice-trace logging paths.
class CDebugRoutingState
{
public:
    CDebugRoutingState();
    virtual ~CDebugRoutingState();

    bool Enabled();

    // Routing dumps. Bodies land in the debug phase.
    void Start();
    void OutputFullState(CBF2VoipServerInstance* instance, unsigned char packetType);
    void OutputRDMismatch(CBF2VoipServerInstance* instance, int channel,
                          const BF2VoipPacketData::RoutingDataPlayer& serverSide,
                          const BF2VoipPacketData::RoutingDataPlayer& packetSide);

private:
    void* m_state;       // +0x08 (vptr at +0x00)
};

// Per-client voice-trace state; 16 bytes.
class CDebugVoiceTrace
{
public:
    CDebugVoiceTrace();
    virtual ~CDebugVoiceTrace();

    // Voice-trace logging. Bodies land in the debug phase.
    void Start(const char* name, int playerId, unsigned int connectionId);
    void Output(unsigned int fromConnection, unsigned int toConnection,
                const char* fromName, const char* toName, char* data, int length);
    void Reset();
    void Stop(unsigned int connectionId);
    bool IsRunning();

private:
    void* m_ptr;             // +0x00
    unsigned int m_field08;  // +0x08
    unsigned int m_field0C;  // +0x0C
};

} // namespace BF2VoipServer

#endif // BF2_VOIPSERVER_VOIPDEBUG_H
