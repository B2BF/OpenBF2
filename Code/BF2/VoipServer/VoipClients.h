// VOIP routing data structures: per-client state and the two lookup tables the router owns.
//
// Layouts and signatures are recovered from fh2's ClientData copy constructor (0x845b40) and the
// CClients/COutgoingChannels containers. Offsets beside the members are the recovered x86-64 ELF
// offsets; the x86 PE offsets differ with its STL layout and are checked with the x86 build.
//
// The routing semantics of most ClientData fields are only partly read; the fields are named for
// what the observed accesses prove and marked where the meaning is still open.

#ifndef BF2_VOIPSERVER_VOIPCLIENTS_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPCLIENTS_H

#include <map>
#include <vector>

#include "VoipResult.h"

namespace VOIP {

// Session ids addressed by a channel. Attested: the COutgoingChannels map value is
// VOIP::SessionIdArray and every access is a 2-byte stride.
typedef std::vector<unsigned short> SessionIdArray;

// A set of routing channels; one byte per channel index.
typedef std::vector<unsigned char> ChannelArray;

// Per-client routing state the BF server pushes with SET_PLAYER_DATA / FULL_ROUTING_DATA_SET.
// No user-declared constructors: the shipped copy constructor is the compiler default, and the
// handler that fills one builds it field by field, so default construction must stay trivial.
class ClientData
{
public:
    unsigned short m_sessionId;              // +0x00  map key; also the routed session id
    unsigned int   m_connectionId;           // +0x04  set from the received packet's connection id
    unsigned short m_clientId;               // +0x08  set from the received packet's client id
    unsigned short m_playerId;               // +0x0a  appended to routed voice frames
    unsigned char  m_channel;                // +0x0c  channel this client speaks on
    // +0x0d..0x0f padding
    std::vector<ChannelArray> m_outgoingSets;     // +0x10  per-set channel lists
    unsigned int   m_singleChannel;          // +0x28  index into m_outgoingSets when !m_multi
    // +0x2c..0x2f padding
    ChannelArray   m_multiChannels;          // +0x30  selected set indices when m_multi
    unsigned char  m_multi;                  // +0x48  non-zero: route through m_multiChannels
};

// Lookup by client session id. Virtual destructor only.
class CClients
{
public:
    CClients();
    virtual ~CClients();

    // Inserts or replaces data under sessionId. Fails (VRESULT_FAILURE) for a zero session id or
    // an out-of-range m_singleChannel.
    VRESULT Add(unsigned short sessionId, const ClientData& data);

    // Erases the client; VRESULT_FAILURE when absent.
    VRESULT Remove(unsigned short sessionId);

    // Erases every client.
    VRESULT Reset();

    // Returns the stored data, or null when absent.
    ClientData* GetClientData(unsigned short sessionId);

    // [CHOSEN, not recovered] The shipped CBF2VoipServerInstance::GetClientDataMap/GetNumClients
    // read this map's address and node count by raw offset; expose it so the port is not tied to a
    // single STL layout.
    std::map<unsigned short, ClientData>* GetClientDataMap();

    // Selects a single outgoing channel by index into the client's m_outgoingSets.
    VRESULT SelectOutgoingSingleChannelSet(unsigned short sessionId, unsigned int channelIndex);

    // Selects several outgoing channels at once.
    VRESULT SelectOutgoingMultiChannelSet(unsigned short sessionId, const std::vector<unsigned char>& channels);

private:
    std::map<unsigned short, ClientData> m_clients;   // +0x08 (vptr at +0x00)
};

// Lookup from a channel to the session ids that receive on it. Virtual destructor only.
class COutgoingChannels
{
public:
    COutgoingChannels();
    virtual ~COutgoingChannels();

    // Returns the channel's session-id list, or null when the channel has no entry.
    SessionIdArray* GetChannelSessionIdArray(unsigned char channel);

    // Erases every channel.
    VRESULT Reset();

    // Appends sessionId to channel, creating the channel entry when needed.
    VRESULT AddSessionIdToChannel(unsigned char channel, unsigned short sessionId);

    // Removes sessionId from every channel that lists it; VRESULT_FAILURE when it is in none.
    VRESULT RemoveSessionId(unsigned short sessionId);

    // Removes sessionId from one list; VRESULT_FAILURE for a null or empty list, or a miss.
    VRESULT RemoveSessionIdFromArray(SessionIdArray* array, unsigned short sessionId);

private:
    std::map<unsigned char, SessionIdArray> m_channels;   // +0x08 (vptr at +0x00)
};

// Fixed text buffer for the challenge and session strings. The shipped instance places four of
// these 112 bytes apart (2264, 2376, 2488, 2600), so the stride is fixed even though only the
// payload at +0x00 and the count at +0x68 are read.
class DataString
{
public:
    char m_data[0x68];   // +0x000  text (20-byte session id / challenge)
    int  m_count;        // +0x068  valid-byte count; ExtractServerSessionId stores 20
    int  m_reserved;     // +0x06C  padding to the shipped 112-byte stride
};

} // namespace VOIP

#endif // BF2_VOIPSERVER_VOIPCLIENTS_H
