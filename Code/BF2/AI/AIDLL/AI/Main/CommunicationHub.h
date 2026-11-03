// Filters and distributes bot radio messages and enemy sightings.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_COMMUNICATIONHUB_H
#define AI_AIDLL_AI_MAIN_COMMUNICATIONHUB_H

#include <utility>
#include <vector>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {

// `filterAndSendAIRadioMessage`'s third argument. Engine-side enum; pointers and values only here.
enum ChannelMessageType : int;

namespace ai {

class BotMain;
class IAIObject;
class IAIPlayer;

// The three enums the radio path names. All [UNRESOLVED] as to enumerators - except that
// `CombatStrengthType` has at least seven, because `reset` fills with the literal 6.
enum AIRadioChannelType : int;
enum AIRadioMessageType : int;
enum CombatStrengthType : int;

class CommunicationHub
{
public:
    CommunicationHub();                     // ELF 0x3E4F10 (C1), 0x3E4EB0 (C2)
    virtual ~CommunicationHub();            // ELF 0x3E5010 (D1), 0x3E4F70 (D2), 0x3E50B0 (D0)

    void reset();                           // ELF 0x3E5170
    // ELF 0x3E57E0: reports a bot's sighting of `object` to the game (IAIGame +0x6C) when it is
    // new for one of `side`'s strategic areas.
    void enemySpottedByAI(IAIPlayer* player, IAIObject* object, int side);
    void enemySpottedByHuman(IAIObject *, unsigned char) const;   // ELF 0x3E5AF0
    // ELF 0x3E5330: hands a bot's message to the engine radio (IBFRadioComm +8). The BotMain
    // argument is not read. A type-2 message within 20 s of the side's last one is not sent;
    // it is delivered to `argument` directly when that player is a bot.
    void filterAndSendAIRadioMessage(BotMain* bot, int senderPlayerId, ChannelMessageType channelMessage,
                                     AIRadioChannelType channel, AIRadioMessageType type,
                                     unsigned char voice, int playerId, unsigned char side,
                                     unsigned char argument);
    // ELF 0x3E54C0: files `settings` with the bot of `receiverPlayerId` (unless it sent it).
    // Messages of types 0..2 from a human sender are filtered first; see the .cpp.
    void filterAndReceiveAIRadioMessage(const AIRadioMessageSettings& settings, int senderPlayerId,
                                        int receiverPlayerId, bool flag);

    // Offsets describe the original ELF layout. STL sizes and double alignment depend on
    // the target compiler; do not treat 52 bytes as a portable object size.

    // These timestamps are uninitialized until reset is called.
    // Game time of each side's last sent type-2 message (indexed side - 1); reset: -1000.0.
    /*  +4 */ double m_type2SentTime[2];
    // Two per-side combat-strength vectors. Each original ELF element occupies 12 bytes.
    /* +20 */ std::vector<std::pair<CombatStrengthType, double> > m_combatStrength[2];
    // enemySpottedByAI: last report of a strength-type 4/5 enemy, either side; reset: -1000.0.
    /* +44 */ double m_time44;

    // first wins here (`if (!m_instance)`), and the destructor does test `== this` - the opposite
    // discipline from `AerialHeightMap`, one step earlier in the same init sequence.
    static CommunicationHub* m_instance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_COMMUNICATIONHUB_H
