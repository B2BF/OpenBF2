// VOIP outgoing-channel table: channel -> the session ids that receive on it.
//
// Recovered from fh2: COutgoingChannels 0x847670-0x847b26. The table is a
// std::map<unsigned char, SessionIdArray>. RemoveSessionId scans in channel order and acts on the
// FIRST channel that lists the session id, erasing that channel's row once its list empties.

#include "VoipClients.h"

namespace VOIP {

COutgoingChannels::COutgoingChannels()
{
}

COutgoingChannels::~COutgoingChannels()
{
}

SessionIdArray* COutgoingChannels::GetChannelSessionIdArray(unsigned char channel)
{
    std::map<unsigned char, SessionIdArray>::iterator it = m_channels.find(channel);
    if (it == m_channels.end())
        return 0;
    return &it->second;
}

VRESULT COutgoingChannels::Reset()
{
    m_channels.clear();
    return VRESULT_SUCCESS;
}

// Creates the channel row on first use (default-constructed empty list), then appends.
VRESULT COutgoingChannels::AddSessionIdToChannel(unsigned char channel, unsigned short sessionId)
{
    m_channels[channel].push_back(sessionId);
    return VRESULT_SUCCESS;
}

VRESULT COutgoingChannels::RemoveSessionIdFromArray(SessionIdArray* array, unsigned short sessionId)
{
    if (!array)
        return VRESULT_FAILURE;
    for (SessionIdArray::iterator it = array->begin(); it != array->end(); ++it)
    {
        if (*it == sessionId)
        {
            array->erase(it);
            return VRESULT_SUCCESS;
        }
    }
    return VRESULT_FAILURE;
}

VRESULT COutgoingChannels::RemoveSessionId(unsigned short sessionId)
{
    for (std::map<unsigned char, SessionIdArray>::iterator it = m_channels.begin(); it != m_channels.end(); ++it)
    {
        if (RemoveSessionIdFromArray(&it->second, sessionId) == VRESULT_SUCCESS)
        {
            if (it->second.empty())
                m_channels.erase(it);
            return VRESULT_SUCCESS;
        }
    }
    return VRESULT_FAILURE;
}

} // namespace VOIP
