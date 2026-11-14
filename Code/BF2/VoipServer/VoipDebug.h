// Voice-server diagnostics. CDebugStandard is the standard-output sink; its Output is a bare return
// in the shipped images, so every message it is handed is discarded. CDebugRoutingState writes the
// routing dump a debug-routing packet (0xC8) starts; CDebugVoiceTrace logs one client's voice frames
// once a type-4 packet names it. Both write under ./mods/bf2/Logs/Voip/.
//
// Bodies and layouts: bf2 ia-32 ELF 0x84F2660..0x84F4170 (symbols). Each class holds an
// std::ofstream*; CDebugRoutingState is the only one with a vtable.
#ifndef BF2_VOIPSERVER_VOIPDEBUG_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPDEBUG_H

#include <iosfwd>
#include <string>

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

    // Date/time string with every character outside "0123456789_" replaced by '_' (0x84F3A80).
    std::string CTimeDateStringNoSpaces() const;

private:
    CDebugStandard();
    ~CDebugStandard();      // closes and deletes m_stream when one exists (0x84F3910)

    std::ofstream* m_stream;   // +0x00  never opened by the shipped code
};

// Per-instance routing-dump state; vptr + the dump stream. Enabled gates the routing and voice-trace
// logging paths.
class CDebugRoutingState
{
public:
    CDebugRoutingState();
    virtual ~CDebugRoutingState();          // closes and deletes the stream (0x84F26A0)
    bool Enabled();                         // m_stream != 0 (0x84F38D0)

    // Opens ./mods/bf2/Logs/Voip/VoipServerRouting_Instance_<time>.log. false when already running
    // (0x84F2800).
    bool Start();
    // Full routing dump: packet name, routing block, client tables, outgoing channel tables. true
    // (0x84F2BF0).
    bool OutputFullState(CBF2VoipServerInstance* instance, unsigned char packetType);
    // One player's routing byte, old and new. true (0x84F29A0).
    bool OutputRDMismatch(CBF2VoipServerInstance* instance, int channel,
                          const BF2VoipPacketData::RoutingDataPlayer& serverSide,
                          const BF2VoipPacketData::RoutingDataPlayer& packetSide);

private:
    std::ofstream* m_stream;   // +0x04 (vptr at +0x00)
};

// Per-client voice-trace state. No vtable: the constructor zeroes three words and the destructor is
// Reset (0x84F3B00).
class CDebugVoiceTrace
{
public:
    CDebugVoiceTrace();
    ~CDebugVoiceTrace();

    // Opens ./mods/bf2/Logs/Voip/VoiceTraffic_Player_<name>_<id>_<time>.log for connectionId.
    // false when a trace is already running (0x84F3B60).
    bool Start(const char* name, int playerId, unsigned int connectionId);
    // Logs a frame to or from the traced connection; stops after 30000 frames. true (0x84F3D90).
    bool Output(unsigned int fromConnection, unsigned int toConnection,
                const char* fromName, const char* toName, char* data, int length);
    // Closes the stream and clears all three fields (0x84F4100).
    void Reset();
    // Resets when connectionId is the traced one; true then (0x84F3D60).
    bool Stop(unsigned int connectionId);
    bool IsRunning();                       // m_stream != 0 (0x84F40E0)

private:
    std::ofstream* m_stream;         // +0x00
    unsigned int   m_connectionId;   // +0x04  the traced connection
    int            m_frameCount;     // +0x08  frames written; the trace stops past 29999
};

} // namespace BF2VoipServer

#endif // BF2_VOIPSERVER_VOIPDEBUG_H
