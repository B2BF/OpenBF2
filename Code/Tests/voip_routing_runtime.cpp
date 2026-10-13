// [TEMP] Runtime checks for the BF2 routing packet handlers (Phase 5).
//
// Drives HandlePacketBF2SetPlayerData (both forms), HandlePacketBF2RemovePlayer,
// HandlePacketBF2ResetRouting and HandlePacketBF2FullRoutingDataSet directly, inspecting the
// private client table and routing array through a local private/public include.

#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#define private public
#define protected public
#include "../BF2/VoipServer/VoipServerInstance.h"
#undef private
#undef protected

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
    using namespace BF2VoipServer;
    namespace P = BF2VoipPacketData;

    // Team 1 commander: flag 0x40 -> channel 0, outgoing sets {1..9},{1},...,{9}.
    {
        CBF2VoipServerInstance instance;
        P::RoutingDataPlayer entry;
        entry.m_flags = 0x40;
        entry.m_reserved = 0;
        entry.m_sessionId = 100;
        expect(instance.HandlePacketBF2SetPlayerData(entry, 0) == VOIP::VRESULT_SUCCESS, "commander accepted");
        expect(instance.GetNumClients() == 1, "commander registered");
        VOIP::ClientData* client = instance.m_clients.GetClientData(100);
        expect(client != 0 && client->m_channel == 0, "commander speaks on channel 0");
        expect(client != 0 && client->m_outgoingSets.size() == 10, "commander has ten outgoing sets");
        expect(client != 0 && client->m_outgoingSets[0].size() == 9
               && client->m_outgoingSets[0][0] == 1 && client->m_outgoingSets[0][8] == 9,
               "commander's first set is {1..9}");
        expect(instance.m_routing[0].m_flags == 0x40 && instance.m_routing[0].m_sessionId == 100,
               "routing slot 0 mirrors the entry");
    }

    // Team 1 squad member index 3: flag 0x03 -> channel 12, one set {12,3}.
    {
        CBF2VoipServerInstance instance;
        P::RoutingDataPlayer entry;
        entry.m_flags = 0x03;
        entry.m_reserved = 0;
        entry.m_sessionId = 101;
        expect(instance.HandlePacketBF2SetPlayerData(entry, 1) == VOIP::VRESULT_SUCCESS, "member accepted");
        VOIP::ClientData* client = instance.m_clients.GetClientData(101);
        expect(client != 0 && client->m_channel == 12, "team1 member speaks on channel 12");
        expect(client != 0 && client->m_outgoingSets.size() == 1
               && client->m_outgoingSets[0].size() == 2
               && client->m_outgoingSets[0][0] == 12 && client->m_outgoingSets[0][1] == 3,
               "member's set is {12,3}");
    }

    // Team 2 squad leader index 2: flag 0xA2 -> channel 21, sets {30},{19}.
    {
        CBF2VoipServerInstance instance;
        P::RoutingDataPlayer entry;
        entry.m_flags = 0xA2;
        entry.m_reserved = 0;
        entry.m_sessionId = 102;
        expect(instance.HandlePacketBF2SetPlayerData(entry, 2) == VOIP::VRESULT_SUCCESS, "leader accepted");
        VOIP::ClientData* client = instance.m_clients.GetClientData(102);
        expect(client != 0 && client->m_channel == 21, "team2 leader speaks on channel 21");
        expect(client != 0 && client->m_outgoingSets.size() == 2
               && client->m_outgoingSets[0].size() == 1 && client->m_outgoingSets[0][0] == 30
               && client->m_outgoingSets[1].size() == 1 && client->m_outgoingSets[1][0] == 19,
               "leader's sets are {30} and {19}");
    }

    // Squad index out of range is refused.
    {
        CBF2VoipServerInstance instance;
        P::RoutingDataPlayer entry;
        entry.m_flags = 0x0A;   // member, squad index 10
        entry.m_reserved = 0;
        entry.m_sessionId = 103;
        expect(instance.HandlePacketBF2SetPlayerData(entry, 3) == VOIP::VRESULT_FAILURE, "squad out of range refused");
        expect(instance.GetNumClients() == 0, "refused entry registers no client");
    }

    // RemovePlayer clears the client and its routing slot; ResetRouting clears everything.
    {
        CBF2VoipServerInstance instance;
        P::RoutingDataPlayer entry;
        entry.m_flags = 0x03;
        entry.m_reserved = 0;
        entry.m_sessionId = 104;
        instance.HandlePacketBF2SetPlayerData(entry, 4);
        expect(instance.m_routing[4].m_sessionId == 104, "slot 4 set");

        P::RemovePlayerData removeData;
        removeData.m_sessionId = 104;
        expect(instance.HandlePacketBF2RemovePlayer(removeData) == VOIP::VRESULT_SUCCESS, "remove player");
        expect(instance.GetNumClients() == 0, "client removed");
        expect(instance.m_routing[4].m_flags == 0 && instance.m_routing[4].m_sessionId == 0, "slot 4 cleared");

        instance.HandlePacketBF2SetPlayerData(entry, 5);
        expect(instance.GetNumClients() == 1, "client re-added");
        expect(instance.HandlePacketBF2ResetRouting() == VOIP::VRESULT_SUCCESS, "reset routing");
        expect(instance.GetNumClients() == 0, "reset removed clients");
        expect(instance.m_routing[5].m_flags == 0, "reset cleared the routing array");
    }

    // FullRoutingDataSet reconciles the whole table, and a matching table is a no-op.
    {
        CBF2VoipServerInstance instance;
        unsigned char table[260];
        std::memset(table, 0, sizeof(table));
        P::RoutingDataPlayer* slots = reinterpret_cast<P::RoutingDataPlayer*>(table);
        slots[5].m_flags = 0x03;
        slots[5].m_reserved = 0;
        slots[5].m_sessionId = 200;

        expect(instance.HandlePacketBF2FullRoutingDataSet(reinterpret_cast<const char*>(table), 260) == VOIP::VRESULT_SUCCESS,
               "full routing data set accepted");
        expect(instance.GetNumClients() == 1, "full table registers the changed slot");
        expect(instance.m_routing[5].m_sessionId == 200, "slot 5 reconciled");

        int clientCount = instance.GetNumClients();
        expect(instance.HandlePacketBF2FullRoutingDataSet(reinterpret_cast<const char*>(table), 260) == VOIP::VRESULT_SUCCESS,
               "matching table accepted");
        expect(instance.GetNumClients() == clientCount, "matching table changes nothing");

        expect(instance.HandlePacketBF2FullRoutingDataSet(reinterpret_cast<const char*>(table), 259) == static_cast<VOIP::VRESULT>(-4),
               "wrong length refused");
        expect(instance.HandlePacketBF2FullRoutingDataSet(0, 260) == static_cast<VOIP::VRESULT>(-4),
               "null data refused");
    }

    std::printf("voip routing: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
