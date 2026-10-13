// VOIP routing core: the packet pump and voice-frame fan-out shared by the voice servers.
//
// CBF2VoipServerInstance derives from this class and supplies the socket transport by overriding
// Send and Receive, so the routing logic can run behind any transport. The virtual declaration
// order below is the recovered vtable order (fh2 instance vtable 0xbb7220): Send, Receive,
// HandleIncomingPacket, RouteVoiceData, RouteVoiceDataToChannelSet, destructor, then the extract,
// client-table and lifecycle methods. MSVC emits one destructor slot where Itanium emits two, so
// only the relative order is preserved, never a raw byte offset.
//
// Member offsets are the recovered x86-64 ELF offsets.

#ifndef BF2_VOIPSERVER_VOIPROUTER_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPROUTER_H

#include "VoipClients.h"
#include "VoipResult.h"

namespace VOIP {

class CVoipRouterInstance
{
public:
    CVoipRouterInstance();

    // Transports. Pure in the base; the derived voice-server instance implements them over UDP.
    // from/to identify the speaking and receiving client (null on broadcast paths).
    virtual VRESULT Send(bool loopback, const ClientData* from, const ClientData* to, char* data, int length) = 0;
    virtual VRESULT Receive(unsigned& packetType, unsigned short& sessionId, char* buffer, int bufferSize, int& length) = 0;

    // Dispatches one received packet. Authentication is the derived class's responsibility before
    // this runs.
    virtual VRESULT HandleIncomingPacket(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int length);

    // Fans a captured voice frame out to the listeners of the speaker's selected channel(s).
    virtual VRESULT RouteVoiceData(unsigned short sessionId, char* data, int length);

    // Sends the frame to the session ids listed for the channels in the speaker's selected set,
    // excluding the speaker and any session id that maps to the sender's own connection/client.
    // setIndex selects from the speaker's m_outgoingSets; from is the speaking client.
    virtual VRESULT RouteVoiceDataToChannelSet(unsigned int setIndex, const ClientData* from, unsigned short sessionId, char* data, int length);

    virtual ~CVoipRouterInstance();

    // Pops a 16-bit session id from the tail of the received packet.
    virtual VRESULT ExtractClientSessionId(char* data, int& length, unsigned short& sessionId);

    // Pops the leading 8-bit packet id from the received packet.
    virtual VRESULT ExtractPacketId(char* data, int& length, unsigned char& packetId);

    // Drops a client from both tables.
    virtual VRESULT RemoveClient(unsigned short sessionId);

    // Empties both tables.
    virtual VRESULT RemoveAllClients();

    // Replaces a client's routing state across both tables.
    virtual VRESULT SetClientData(const ClientData& data);

    // Reads and routes every pending packet; the transport Receive does the I/O.
    virtual VRESULT Update();

    // Enables routing. VRESULT_SUCCESS.
    virtual VRESULT Start();

    // Disables routing. VRESULT_SUCCESS.
    virtual VRESULT Stop();

protected:
    // Voice-frame decoders. Not virtual; HandleIncomingPacket dispatches to them by packet id. The
    // alternate and multi forms carry their channel data at the tail and reduce the routed length.
    VRESULT HandlePacketVoiceFrameDefaultChannelSet(unsigned short sessionId, char* data, int length);
    VRESULT HandlePacketVoiceFrameAlternateChannelSet(unsigned short sessionId, char* data, int length);
    VRESULT HandlePacketVoiceFrameMultiChannelSet(unsigned short sessionId, char* data, int length);

    bool m_running;                              // +0x0008  set by Start, cleared by Stop
    char m_packetBuffer[2048];                   // +0x0009  Receive sink, passed to HandleIncomingPacket
    COutgoingChannels m_outgoingChannels;        // +0x0810
    CClients m_clients;                          // +0x0848
};

} // namespace VOIP

#endif // BF2_VOIPSERVER_VOIPROUTER_H
