// BF2 server -> voice server packet payloads.
//
// CBF2VoipServerInstance parses these; CBF2VoipServerInstance::HandlePacketBF2FullRoutingDataSet
// and HandlePacketBF2SetPlayerData (fh2 0x844760 and 0x844700/0x843790) fix the layouts. Sizes are
// exact because the full-routing handler gates on a 260-byte payload.

#ifndef BF2_VOIPSERVER_VOIPPACKETS_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPPACKETS_H

namespace BF2VoipPacketData {

// One 4-byte routing entry, carried by FULL_ROUTING_DATA_SET, RESET_PLAYER_ROUTING and the
// per-player add/remove paths. The flag bits are read as: bit7 team 2, bit6 commander,
// bit5 squad leader, bits0-3 squad index (1..9). Bit 4 and the whole of m_reserved are unused by
// the recovered readers and stay [UNRESOLVED].
struct RoutingDataPlayer
{
    unsigned char  m_flags;      // +0x00
    unsigned char  m_reserved;   // +0x01  [UNRESOLVED]
    unsigned short m_sessionId;  // +0x02
};

// One client's routing metadata for a single update. HandlePacketBF2SetPlayerData(RoutingDataPlayer)
// synthesizes this from a RoutingDataPlayer plus the target channel index.
struct SetPlayerData
{
    unsigned char  m_team;          // +0x00  1 (team 1) or 2 (team 2)
    unsigned char  m_isCommander;   // +0x01
    unsigned char  m_isSquadLeader; // +0x02
    unsigned char  m_squadIndex;    // +0x03  1..9
    unsigned short m_sessionId;     // +0x04
    unsigned short m_channel;       // +0x06  resolved channel 0..63, or 255 for the server slot
};

// BF2_VOIP_PACKET_REMOVE_PLAYER_DATA payload.
struct RemovePlayerData
{
    unsigned short m_sessionId;   // +0x00
};

// BF2_VOIP_PACKET_FULL_ROUTING_DATA_SET payload: exactly 65 routing entries (0x104 bytes). Slot 64
// is the server/overflow entry, the one RoutingDataPlayer writes at +0x0BAC when a packet names
// channel 255.
typedef RoutingDataPlayer FullRoutingDataSet[65];

} // namespace BF2VoipPacketData

#endif // BF2_VOIPSERVER_VOIPPACKETS_H
