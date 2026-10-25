// Voice server settings and player-routing packets. Enabled voice initializes the voice channel and,
// for local voice, the in-process voice server; routing is inert while the channel pointer is null.
// The padded layout models only accessed fields; std::string size shifts later fields between platforms.

#ifndef GAME_GAMESERVER_VOIPSERVERCONNECTION_H     // [INFERRED] guard style
#define GAME_GAMESERVER_VOIPSERVERCONNECTION_H

#include <iosfwd>
#include <string>

namespace dice {
namespace hfe {

namespace io { class UDPSocket; }
namespace world { class IPlayer; }
class VoipServerInternal;                          // real local owner; native body remains deferred

class VoipServerConnection
{
public:
    static VoipServerConnection* GetInstance();    // lazy new(0x4418) into ds:0x8AAC980
    static void ReleaseInstance();                 // delete singleton and clear pointer

    VoipServerConnection();                        // transcribed
    virtual ~VoipServerConnection();               // log/channel cleanup recovered; live local
                                                    // voice-library owner retains a diagnostic stop

    // Returns bool: both binaries explicitly materialize 0/1 at the shared epilogue. Itanium does
    // not encode the return type, which is why the earlier void declaration compiled unnoticed.
    bool Init();                                   // settings copy, then InitEnabledVoip when voice is on
    int  ResetPlayerRouting();                     // transcribed

    //  / PE 0x54B3B0, 839 B. Both ports explicitly return false when the
    // channel or PlayerManager is absent and true after sending the routing packet. There is no
    // player null check. Squad state setters are its three shipped callers.
    bool UpdatePlayerRouting(world::IPlayer* player);

    // A random 16-bit session id, collision-rescanned against the live players. The original
    // ignores playerId.
    int  CreateClientSessionId(int playerId);

    // the fields recovered bodies touch; pads hold the attested ELF offsets
    // PE offsets match through +0x43FC, then shift by its 0x18-larger std::string. This portable
    // representation uses named fields instead of making the current compiler's STL layout an ABI
    // claim; see the [CHOSEN] note below.
    io::UDPSocket* m_channel;                      // +0x04 - the exact class Init allocates
    unsigned char m_packet[0x430C - 0x08];         // +0x08 packet scratch + unrecovered state
    float         m_fullRoutingTimer;              // +0x430C  full-routing resend timer; ctor 0, +5 s per send
    unsigned char _pad4310[4];                     // +0x4310  ctor 0
    unsigned char m_infoByte4314;                  // +0x4314  enabled, ctor 0
    unsigned char m_infoByte4315;                  // +0x4315  remote-server mode, ctor 0
    unsigned char _pad4316[2];
    int           m_infoDword4318;                 // +0x4318  ctor 0
    unsigned char _pad431C[0x4400 - 0x431C];
    int           m_infoWord4400;                  // +0x4400  dword store; low word is published
    int           m_infoDword4404;                 // +0x4404  BF server port
    int           m_infoWord4408;                  // +0x4408  dword store; low word is published
    VoipServerInternal* m_localVoipServer440C;     // +0x440C  ctor null; PE +0x4424
    int           m_infoDword4410;                 // +0x4410  ctor 3
    std::ofstream* m_routingLog4414;               // +0x4414 (PE +0x442C) - ctor null; PE dtor
                                                   // [CHOSEN] Named version, challenge, and secret members are stored separately from the packet scratch region.
    std::string   m_sharedSecret;                  // ELF +0x43FC; here placed after the pads -
                                                   // layout is free, offsets documented.
    unsigned int  m_textCount;                     // ELF +0x43F4
    char          m_textBuffer[100];               // ELF +0x438C
    unsigned int  m_versionCount;                  // ELF/PE +0x4384
    char          m_versionBuffer[100];            // ELF/PE +0x431C

    // . Copies the complete voice-settings subset. A null receiver is the only false
    // result; disabled voice is a valid copied setting and is tested separately by Init.
    bool GetVoipServerSettings(class ServerSettings* settings);
#if !DEDICATED
    // BF2.exe 0x5B3820 (): a listen server's copy of the same subset, out of the
    // client's GameServerSettings object through its slots instead of the dedicated record. Init
    // picks the overload with isDedicated(); a null object is the only false result here too.
    bool GetVoipServerSettings(class IGameServerSettings* settings);
#endif

    // Creates the voice channel/socket and, for local voice, the in-process voice server.
    bool InitEnabledVoip();

    // PE 0x54B080: local owner first, routing log second, channel last. This named helper is a
    // chosen extraction of the destructor's cleanup body; the ordinary string destructor remains
    // the compiler-generated tail.
    void ReleaseEnabledVoipResources();

    //  / PE 0x54AB80, 160 B. The mangling's ERi is int&, not int*.
    // Appends the invariant 24-byte version and 20-byte challenge/session strings.
    void PacketAppendVersionAndSessionId(int& cursor);

    // The per-tick entry GameServer::update calls: 0x81278E0, 78 B, read - gated on
    // the +0x4314 byte (0 from the ctor, raised by Init), so a server that never Init'ed VoIP
    // does nothing here. Past the gate the PE resends the full routing table periodically, then
    // drains the channel (PE 0x54B900: FullRoutingUpdate then Receive).
    void Update(float dt);

    // PE 0x54B700, 502 B. Every ~5 s rebuilds the 65-slot routing table, appends the invariant
    // version/session header and sends it as message 103.
    void FullRoutingUpdate(float dt);

    // ELF 0x81275d0. Reads the trailing packet-id byte into packetId and decrements length; reports
    // false when length is already zero (nothing read).
    int ExtractPacketId(unsigned char* buffer, int& length, unsigned char& packetId);

    // PE 0x54B000, ~0x7e / ELF 0x8127810. Non-blocking recv on m_channel plus a peer-address
    // materialisation; a datagram whose trailing id byte is 104 is the BFVoipChallengeString
    // handshake, which recomputes the session stored in m_textBuffer.
    void Receive();

    // The challenge response (PE 0x54AEA0 / ELF 0x8127600): MD5(challenge + shared secret),
    // truncated to 20 bytes. Retail aborts on any other length, a remote DoS reachable from an
    // unauthenticated datagram; a non-10 length is dropped instead (intentional departure).
    void HandlePacketChallengeString(char* data, int length);

    // Declared from the ELF per-tick path; the PE Update calls only the two above, so no body is
    // recovered for it yet.
    static void UpdateDebugLogging();
};

} // namespace hfe
} // namespace dice

#endif
