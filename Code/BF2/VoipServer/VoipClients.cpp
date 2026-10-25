// VOIP client table and per-client routing state.
//
// Recovered from fh2: ClientData copy constructor 0x845b40, CClients 0x845310-0x8456d0. The two
// vectors are copied deeply, so the copy constructor is the compiler's default; the table is an
// insertion-ordered std::map<unsigned short, ClientData>. Out-of-line definitions keep the shipped
// symbols (the win32 DLL exports CBF2VoipServerInstance, which embeds this table).

#include <utility>

#include "VoipClients.h"

namespace VOIP {

CClients::CClients()
{
}

CClients::~CClients()
{
}

// Rejects a zero session id or a single-channel index outside the client's outgoing sets, then
// inserts; an existing row is left untouched (insert, not assign).
VRESULT CClients::Add(unsigned short sessionId, const ClientData& data)
{
    if (sessionId == 0 || data.m_singleChannel >= data.m_outgoingSets.size())
        return VRESULT_FAILURE;
    m_clients.insert(std::make_pair(sessionId, data));
    return VRESULT_SUCCESS;
}

VRESULT CClients::Remove(unsigned short sessionId)
{
    std::map<unsigned short, ClientData>::iterator it = m_clients.find(sessionId);
    if (it == m_clients.end())
        return VRESULT_FAILURE;
    m_clients.erase(it);
    return VRESULT_SUCCESS;
}

VRESULT CClients::Reset()
{
    m_clients.clear();
    return VRESULT_SUCCESS;
}

ClientData* CClients::GetClientData(unsigned short sessionId)
{
    std::map<unsigned short, ClientData>::iterator it = m_clients.find(sessionId);
    if (it == m_clients.end())
        return 0;
    return &it->second;
}

std::map<unsigned short, ClientData>* CClients::GetClientDataMap()
{
    return &m_clients;
}

// Clears the multi-channel selection first, then range-checks the index (so a rejected index still
// leaves the multi set empty, as shipped).
VRESULT CClients::SelectOutgoingSingleChannelSet(unsigned short sessionId, unsigned int channelIndex)
{
    std::map<unsigned short, ClientData>::iterator it = m_clients.find(sessionId);
    if (it == m_clients.end())
        return VRESULT_FAILURE;
    it->second.m_multi = 0;
    it->second.m_multiChannels.clear();
    if (channelIndex >= it->second.m_outgoingSets.size())
        return VRESULT_FAILURE;
    it->second.m_singleChannel = channelIndex;
    return VRESULT_SUCCESS;
}

// Marks the client multi-channel, clears the selection, then appends each in-range channel; an
// out-of-range channel aborts the copy, leaving the ones already appended (as shipped).
VRESULT CClients::SelectOutgoingMultiChannelSet(unsigned short sessionId, const std::vector<unsigned char>& channels)
{
    std::map<unsigned short, ClientData>::iterator it = m_clients.find(sessionId);
    if (it == m_clients.end())
        return VRESULT_FAILURE;
    it->second.m_multi = 1;
    it->second.m_multiChannels.clear();
    for (std::vector<unsigned char>::const_iterator c = channels.begin(); c != channels.end(); ++c)
    {
        if (*c >= it->second.m_outgoingSets.size())
            return VRESULT_FAILURE;
        it->second.m_multiChannels.push_back(*c);
    }
    return VRESULT_SUCCESS;
}

} // namespace VOIP
