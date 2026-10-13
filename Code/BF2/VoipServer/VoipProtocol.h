// Voice-server packet identifiers and routing channel table.
//
// Both tables are attested from the module's own name-printing switches: the packet ids from
// CBF2VoipServerInstance::MapPacketTypeToName and the channel table from MapChannelToName
// (fh2 ELF 0x841800 and 0x841950). The symbolic names below are [INFERRED] combinations of those
// strings; the values are direct.

#ifndef BF2_VOIPSERVER_VOIPPROTOCOL_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPPROTOCOL_H

namespace BF2VoipServer {

// First byte of every packet on the voice wire. Voice frames and keep-alive travel from voice
// clients to the server; the BF2_* ids travel from the authenticating BF server to the voice
// server. Ids 4 and 5 are dispatched by CBF2VoipServerInstance::HandleIncomingPacket but are not
// named by MapPacketTypeToName, so their identity stays [UNRESOLVED] and they are left out of
// this enum until read.
enum VoipPacketType
{
    VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_DEFAULT_CHANNEL_SET   =   1,
    VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_ALTERNATE_CHANNEL_SET =   2,
    VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_MULTI_CHANNEL_SET     =   3,
    VOIP_PACKET_SERVER_INCOMING_KEEP_ALIVE                        =   6,
    BF2_VOIP_PACKET_SET_PLAYER_DATA                               = 100,
    BF2_VOIP_PACKET_REMOVE_PLAYER_DATA                            = 101,
    BF2_VOIP_PACKET_RESET_PLAYER_ROUTING                          = 102,
    BF2_VOIP_PACKET_FULL_ROUTING_DATA_SET                         = 103,
    BF2_VOIP_PACKET_DEBUG_DATA                                    = 200
};

// Routing channels addressable by a voice frame. Team 1 occupies 0..18 and team 2 occupies 19..37;
// each team is commander, then squad leader A..I, then squad member A..I. The trailing index in
// the retail strings is the squad slot, not part of the name.
enum VoipChannel
{
    VOIP_CHANNEL_TEAM_1_COMMANDER       =  0,
    VOIP_CHANNEL_TEAM_1_SQUAD_A_LEADER  =  1,
    VOIP_CHANNEL_TEAM_1_SQUAD_B_LEADER  =  2,
    VOIP_CHANNEL_TEAM_1_SQUAD_C_LEADER  =  3,
    VOIP_CHANNEL_TEAM_1_SQUAD_D_LEADER  =  4,
    VOIP_CHANNEL_TEAM_1_SQUAD_E_LEADER  =  5,
    VOIP_CHANNEL_TEAM_1_SQUAD_F_LEADER  =  6,
    VOIP_CHANNEL_TEAM_1_SQUAD_G_LEADER  =  7,
    VOIP_CHANNEL_TEAM_1_SQUAD_H_LEADER  =  8,
    VOIP_CHANNEL_TEAM_1_SQUAD_I_LEADER  =  9,
    VOIP_CHANNEL_TEAM_1_SQUAD_A_MEMBER  = 10,
    VOIP_CHANNEL_TEAM_1_SQUAD_B_MEMBER  = 11,
    VOIP_CHANNEL_TEAM_1_SQUAD_C_MEMBER  = 12,
    VOIP_CHANNEL_TEAM_1_SQUAD_D_MEMBER  = 13,
    VOIP_CHANNEL_TEAM_1_SQUAD_E_MEMBER  = 14,
    VOIP_CHANNEL_TEAM_1_SQUAD_F_MEMBER  = 15,
    VOIP_CHANNEL_TEAM_1_SQUAD_G_MEMBER  = 16,
    VOIP_CHANNEL_TEAM_1_SQUAD_H_MEMBER  = 17,
    VOIP_CHANNEL_TEAM_1_SQUAD_I_MEMBER  = 18,
    VOIP_CHANNEL_TEAM_2_COMMANDER       = 19,
    VOIP_CHANNEL_TEAM_2_SQUAD_A_LEADER  = 20,
    VOIP_CHANNEL_TEAM_2_SQUAD_B_LEADER  = 21,
    VOIP_CHANNEL_TEAM_2_SQUAD_C_LEADER  = 22,
    VOIP_CHANNEL_TEAM_2_SQUAD_D_LEADER  = 23,
    VOIP_CHANNEL_TEAM_2_SQUAD_E_LEADER  = 24,
    VOIP_CHANNEL_TEAM_2_SQUAD_F_LEADER  = 25,
    VOIP_CHANNEL_TEAM_2_SQUAD_G_LEADER  = 26,
    VOIP_CHANNEL_TEAM_2_SQUAD_H_LEADER  = 27,
    VOIP_CHANNEL_TEAM_2_SQUAD_I_LEADER  = 28,
    VOIP_CHANNEL_TEAM_2_SQUAD_A_MEMBER  = 29,
    VOIP_CHANNEL_TEAM_2_SQUAD_B_MEMBER  = 30,
    VOIP_CHANNEL_TEAM_2_SQUAD_C_MEMBER  = 31,
    VOIP_CHANNEL_TEAM_2_SQUAD_D_MEMBER  = 32,
    VOIP_CHANNEL_TEAM_2_SQUAD_E_MEMBER  = 33,
    VOIP_CHANNEL_TEAM_2_SQUAD_F_MEMBER  = 34,
    VOIP_CHANNEL_TEAM_2_SQUAD_G_MEMBER  = 35,
    VOIP_CHANNEL_TEAM_2_SQUAD_H_MEMBER  = 36,
    VOIP_CHANNEL_TEAM_2_SQUAD_I_MEMBER  = 37
};

} // namespace BF2VoipServer

#endif // BF2_VOIPSERVER_VOIPPROTOCOL_H
