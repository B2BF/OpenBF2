// VOIP routing core: the packet pump, BF-server-authenticated voice-frame fan-out and the two
// routing tables it drives.
//
// Recovered from fh2 (CVoipRouterInstance 0x847e50-0x848770). Send and Receive stay pure; a derived
// transport supplies them. The wire shortens from the tail: ExtractPacketId pops the last byte, the
// voice-frame decoders pop channel bytes before the 16-bit session id.

#include <cstddef>

#include "VoipRouter.h"

namespace VOIP {

CVoipRouterInstance::CVoipRouterInstance()
    : m_running(false)
{
}

CVoipRouterInstance::~CVoipRouterInstance()
{
}

VRESULT CVoipRouterInstance::Start()
{
    m_running = true;
    return VRESULT_SUCCESS;
}

VRESULT CVoipRouterInstance::Stop()
{
    m_running = false;
    return VRESULT_SUCCESS;
}

// Empties the client table and then the channel table.
VRESULT CVoipRouterInstance::RemoveAllClients()
{
    m_clients.Reset();
    m_outgoingChannels.Reset();
    return VRESULT_SUCCESS;
}

// Drops the session from both tables; the client table's result is the return value.
VRESULT CVoipRouterInstance::RemoveClient(unsigned short sessionId)
{
    m_outgoingChannels.RemoveSessionId(sessionId);
    return m_clients.Remove(sessionId);
}

// Rebuilds a client's routing state: drop any previous rows, insert the client, then add its
// current channel to the outgoing table.
VRESULT CVoipRouterInstance::SetClientData(const ClientData& data)
{
    m_clients.Remove(data.m_sessionId);
    m_outgoingChannels.RemoveSessionId(data.m_sessionId);
    VRESULT result = m_clients.Add(data.m_sessionId, data);
    if (result == VRESULT_SUCCESS)
        return m_outgoingChannels.AddSessionIdToChannel(data.m_channel, data.m_sessionId) != 0
                   ? VRESULT_SUCCESS
                   : VRESULT_FAILURE;
    return result;
}

// Pops the trailing 16-bit session id; VRESULT_FAILURE when fewer than two bytes remain.
VRESULT CVoipRouterInstance::ExtractClientSessionId(char* data, int& length, unsigned short& sessionId)
{
    if (length <= 1)
        return VRESULT_FAILURE;
    int index = length - 2;
    length = index;
    sessionId = *reinterpret_cast<unsigned short*>(&data[index]);
    return VRESULT_SUCCESS;
}

// Pops the trailing 8-bit packet id; VRESULT_FAILURE on an empty buffer.
VRESULT CVoipRouterInstance::ExtractPacketId(char* data, int& length, unsigned char& packetId)
{
    if (length <= 0)
        return VRESULT_FAILURE;
    packetId = static_cast<unsigned char>(data[--length]);
    return VRESULT_SUCCESS;
}

// Reads the speaker's session id from the tail, refreshes its connection identity, then dispatches
// by packet id: 1/2/3 voice frames, 6 keep-alive. -4 when the id is missing, -7 when the speaker is
// unknown, -1 otherwise.
VRESULT CVoipRouterInstance::HandleIncomingPacket(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int length)
{
    int remaining = length;
    unsigned short sessionId = 0;
    if (ExtractClientSessionId(m_packetBuffer, remaining, sessionId) != VRESULT_SUCCESS)
        return VRESULT_INVALID_STATE;
    ClientData* client = m_clients.GetClientData(sessionId);
    if (!client)
        return VRESULT_AUTH_FAILED;
    client->m_connectionId = connectionId;
    client->m_clientId = clientId;
    if (!data)
        return VRESULT_FAILURE;
    switch (packetType)
    {
    case 1:
        return HandlePacketVoiceFrameDefaultChannelSet(sessionId, data, remaining);
    case 2:
        return HandlePacketVoiceFrameAlternateChannelSet(sessionId, data, remaining);
    case 3:
        return HandlePacketVoiceFrameMultiChannelSet(sessionId, data, remaining);
    case 6:
        return VRESULT_SUCCESS;
    default:
        return VRESULT_FAILURE;
    }
}

// Selects outgoing set 0, then routes the whole frame.
VRESULT CVoipRouterInstance::HandlePacketVoiceFrameDefaultChannelSet(unsigned short sessionId, char* data, int length)
{
    VRESULT result = m_clients.SelectOutgoingSingleChannelSet(sessionId, 0);
    if (result == VRESULT_SUCCESS)
        return RouteVoiceData(sessionId, data, length);
    return result;
}

// The trailing byte is the outgoing set index and is not part of the routed audio.
VRESULT CVoipRouterInstance::HandlePacketVoiceFrameAlternateChannelSet(unsigned short sessionId, char* data, int length)
{
    if (!data || length <= 0)
        return VRESULT_FAILURE;
    unsigned char setIndex = static_cast<unsigned char>(data[length - 1]);
    VRESULT result = m_clients.SelectOutgoingSingleChannelSet(sessionId, setIndex);
    if (result == VRESULT_SUCCESS)
        return RouteVoiceData(sessionId, data, length - 1);
    return result;
}

// The trailing byte is a channel count; that many set indices precede it (read back to front) and
// are not part of the routed audio.
VRESULT CVoipRouterInstance::HandlePacketVoiceFrameMultiChannelSet(unsigned short sessionId, char* data, int length)
{
    if (!data || length <= 0)
        return VRESULT_FAILURE;
    int remaining = length - 1;
    unsigned char count = static_cast<unsigned char>(data[length - 1]);
    ChannelArray setIndices;
    if (count)
    {
        int index = length - 1;
        do
        {
            setIndices.push_back(static_cast<unsigned char>(data[index - 1]));
            --index;
        }
        while (count > static_cast<unsigned char>((length - 1) - index));
        remaining = index;
    }
    VRESULT result = m_clients.SelectOutgoingMultiChannelSet(sessionId, setIndices);
    if (result == VRESULT_SUCCESS)
        result = RouteVoiceData(sessionId, data, remaining);
    return result;
}

// Appends the speaker's player id to the frame, then fans out through the client's selected
// single set or every entry of its multi set. Refuses a frame that cannot take the extra field.
VRESULT CVoipRouterInstance::RouteVoiceData(unsigned short sessionId, char* data, int length)
{
    ClientData* client = m_clients.GetClientData(sessionId);
    if (!client || static_cast<unsigned int>(length + 4) > 0x7FF)
        return VRESULT_FAILURE;
    int newLength = length + 2;
    *reinterpret_cast<unsigned short*>(&data[length]) = client->m_playerId;
    if (client->m_multi)
    {
        if (client->m_multiChannels.empty())
            return VRESULT_SUCCESS;
        for (std::vector<unsigned char>::iterator it = client->m_multiChannels.begin();
             it != client->m_multiChannels.end(); ++it)
            RouteVoiceDataToChannelSet(*it, client, sessionId, data, newLength);
        return VRESULT_SUCCESS;
    }
    RouteVoiceDataToChannelSet(client->m_singleChannel, client, sessionId, data, newLength);
    return VRESULT_SUCCESS;
}

// Fans the frame out to every session listed on every channel of from->m_outgoingSets[setIndex],
// skipping the speaker and any destination that is the sender's own connection/client. The index
// is used unchecked, as shipped.
VRESULT CVoipRouterInstance::RouteVoiceDataToChannelSet(unsigned int setIndex, const ClientData* from, unsigned short sessionId, char* data, int length)
{
    const ChannelArray& channels = from->m_outgoingSets[setIndex];
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        SessionIdArray* listeners = m_outgoingChannels.GetChannelSessionIdArray(channels[i]);
        if (!listeners)
            continue;
        for (std::size_t j = 0; j < listeners->size(); ++j)
        {
            unsigned short destination = (*listeners)[j];
            if (destination == sessionId)
                continue;
            ClientData* to = m_clients.GetClientData(destination);
            if (!to)
                continue;
            if (to->m_connectionId == 0)
                continue;
            if (to->m_clientId == 0)
                continue;
            if (to->m_connectionId == from->m_connectionId && to->m_clientId == from->m_clientId)
                continue;
            Send(true, from, to, data, length);
        }
    }
    return VRESULT_SUCCESS;
}

// Pumps up to 128 packets. A failed or empty Receive ends the pass; a zero connection id is a
// VRESULT_INVALID_STATE stop.
VRESULT CVoipRouterInstance::Update()
{
    if (!m_running)
        return VRESULT_INVALID_STATE;
    int budget = 128;
    int length = 0;
    VRESULT result = VRESULT_SUCCESS;
    for (;;)
    {
        unsigned int connectionId = 0;
        unsigned short clientId = 0;
        result = Receive(connectionId, clientId, m_packetBuffer, static_cast<int>(sizeof(m_packetBuffer)), length);
        if (result != VRESULT_SUCCESS)
            break;
        if (length <= 0)
            return result;
        if (connectionId == 0)
            return VRESULT_INVALID_STATE;
        unsigned char packetId = 0;
        --budget;
        ExtractPacketId(m_packetBuffer, length, packetId);
        HandleIncomingPacket(packetId, connectionId, clientId, m_packetBuffer, length);
        if (length <= 0 || budget <= 0)
            return result;
    }
    return result;
}

} // namespace VOIP
