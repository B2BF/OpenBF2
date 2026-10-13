// [TEMP] Runtime checks for the recovered VOIP container library (Phase 1).
//
// Compiles and runs Code/BF2/VoipServer/VoipClients.cpp and VoipOutgoingChannels.cpp. Every
// assertion mirrors a branch read from fh2 (CClients 0x845310-0x8456d0, COutgoingChannels
// 0x847670-0x847b26), including the shipped partial-copy and first-channel-only behaviours.

#include <cstdio>
#include <vector>

#include "../BF2/VoipServer/VoipClients.h"

static int g_checks = 0;
static int g_failures = 0;

static void expect(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

int main()
{
    using namespace VOIP;

    // ---- CClients ----
    CClients clients;

    ClientData d;
    d.m_sessionId = 100;
    d.m_outgoingSets.resize(3);          // three addressable channel slots
    d.m_singleChannel = 1;
    d.m_multi = 0;

    expect(clients.Add(0, d) == VRESULT_FAILURE, "Add rejects a zero session id");
    d.m_singleChannel = 3;               // == size, out of range
    expect(clients.Add(101, d) == VRESULT_FAILURE, "Add rejects an out-of-range single channel");
    d.m_singleChannel = 1;

    expect(clients.Add(100, d) == VRESULT_SUCCESS, "Add accepts a valid client");
    ClientData* got = clients.GetClientData(100);
    expect(got != 0, "GetClientData finds the row");
    expect(got != 0 && got->m_sessionId == 100, "GetClientData returns the stored row");
    expect(got != 0 && got->m_singleChannel == 1, "stored single channel matches");
    expect(clients.GetClientData(200) == 0, "GetClientData returns null on a miss");

    expect(clients.SelectOutgoingSingleChannelSet(100, 2) == VRESULT_SUCCESS, "single select in range");
    got = clients.GetClientData(100);
    expect(got != 0 && got->m_singleChannel == 2 && got->m_multi == 0, "single select applied");
    expect(clients.SelectOutgoingSingleChannelSet(100, 3) == VRESULT_FAILURE, "single select out of range");
    expect(clients.SelectOutgoingSingleChannelSet(999, 0) == VRESULT_FAILURE, "single select on a miss");

    ChannelArray channels;
    channels.push_back(0);
    channels.push_back(2);
    expect(clients.SelectOutgoingMultiChannelSet(100, channels) == VRESULT_SUCCESS, "multi select in range");
    got = clients.GetClientData(100);
    expect(got != 0 && got->m_multi == 1 && got->m_multiChannels.size() == 2
           && got->m_multiChannels[0] == 0 && got->m_multiChannels[1] == 2, "multi select applied");

    ChannelArray bad;
    bad.push_back(0);
    bad.push_back(3);                    // out of range
    expect(clients.SelectOutgoingMultiChannelSet(100, bad) == VRESULT_FAILURE, "multi select out of range");
    got = clients.GetClientData(100);
    expect(got != 0 && got->m_multiChannels.size() == 1 && got->m_multiChannels[0] == 0,
           "multi select keeps channels appended before the bad one");

    expect(clients.SelectOutgoingSingleChannelSet(100, 0) == VRESULT_SUCCESS, "single select clears multi");
    got = clients.GetClientData(100);
    expect(got != 0 && got->m_multi == 0 && got->m_multiChannels.empty(), "single select cleared multi");

    expect(clients.Remove(100) == VRESULT_SUCCESS, "Remove an existing client");
    expect(clients.Remove(100) == VRESULT_FAILURE, "Remove a missing client");
    clients.Add(100, d);
    clients.Add(101, d);
    expect(clients.Reset() == VRESULT_SUCCESS, "CClients Reset");
    expect(clients.GetClientData(100) == 0 && clients.GetClientData(101) == 0, "Reset cleared the table");

    // ---- COutgoingChannels ----
    COutgoingChannels outgoing;

    expect(outgoing.GetChannelSessionIdArray(5) == 0, "channel 5 not present yet");
    expect(outgoing.AddSessionIdToChannel(5, 100) == VRESULT_SUCCESS, "add 100 to channel 5");
    expect(outgoing.AddSessionIdToChannel(5, 101) == VRESULT_SUCCESS, "add 101 to channel 5");
    SessionIdArray* list = outgoing.GetChannelSessionIdArray(5);
    expect(list != 0 && list->size() == 2 && (*list)[0] == 100 && (*list)[1] == 101,
           "channel 5 lists 100 then 101");
    expect(outgoing.GetChannelSessionIdArray(6) == 0, "channel 6 still absent");

    expect(outgoing.RemoveSessionIdFromArray(list, 100) == VRESULT_SUCCESS, "remove 100 from the list");
    expect(list->size() == 1 && (*list)[0] == 101, "list is now {101}");
    expect(outgoing.RemoveSessionIdFromArray(list, 999) == VRESULT_FAILURE, "remove a value not in the list");
    expect(outgoing.RemoveSessionIdFromArray(0, 100) == VRESULT_FAILURE, "remove from a null list");

    expect(outgoing.RemoveSessionId(101) == VRESULT_SUCCESS, "RemoveSessionId 101");
    expect(outgoing.GetChannelSessionIdArray(5) == 0, "channel 5 row erased once emptied");
    expect(outgoing.RemoveSessionId(101) == VRESULT_FAILURE, "RemoveSessionId on a miss");

    outgoing.AddSessionIdToChannel(1, 42);
    outgoing.AddSessionIdToChannel(2, 42);
    expect(outgoing.RemoveSessionId(42) == VRESULT_SUCCESS, "RemoveSessionId removes the first match");
    expect(outgoing.GetChannelSessionIdArray(1) == 0, "channel 1 row erased");
    SessionIdArray* still = outgoing.GetChannelSessionIdArray(2);
    expect(still != 0 && still->size() == 1 && (*still)[0] == 42, "channel 2 still lists 42");

    expect(outgoing.Reset() == VRESULT_SUCCESS, "COutgoingChannels Reset");
    expect(outgoing.GetChannelSessionIdArray(2) == 0, "Reset cleared the table");

    std::printf("voip containers: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
