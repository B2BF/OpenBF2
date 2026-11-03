// Per-bot radio message queue and timing state.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H

#include <list>
#include <map>
#include <queue>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class CommunicationHub;
class Information;

// Both opaque, and declared this way elsewhere in the tree too - see BFConsole.h for why no
// enumerator name is recoverable from either binary.
enum AIRadioChannelType : int;
enum AIRadioMessageType : int;
struct AIRadioMessageSettings;

// Radio-message record. [UNRESOLVED] Some fields are not yet identified.
struct AIRadioMessageInfo
{
    // ELF 0x1C3260: 1.0f, -1, 0.0, false, false, 18.
    AIRadioMessageInfo()
        : m_modifier(1.0f), m_senderPlayerId(-1), m_time(0.0), m_flag16(false), m_flag17(false), m_int20(0x12)
    {
    }

    // ELF 0x1C3310 (C1; C2 0x1C32E0 is the same): member by member, out of line in the image.
    AIRadioMessageInfo(const AIRadioMessageInfo& other)
        : m_modifier(other.m_modifier), m_senderPlayerId(other.m_senderPlayerId), m_time(other.m_time),
          m_flag16(other.m_flag16), m_flag17(other.m_flag17), m_int20(other.m_int20)
    {
    }

    // receiveRadioMessage (ELF 0x1C44A0) copies AIRadioMessageSettings +4 (the modifier) here.
    /*  0 */ float m_modifier;
    /*  4 */ int m_senderPlayerId;
    // clearOutOldMessages (ELF 0x1C4DD0) ages entries by this time; waitWithSendingMessage
    // (0x1C4D40) indexes the max-send periods with the int at +20.
    /*  8 */ double m_time;
    /* 16 */ bool m_flag16;                // [UNRESOLVED] false on receipt and on refresh
    /* 17 */ bool m_flag17;                // receiveRadioMessage's third argument
    /* 20 */ int m_int20;                  // the message type (AIRadioMessageSettings +0); waitWithSendingMessage indexes by it
};

class CommunicationAgent
{
public:
    // Queued priority message. The integer at +8 determines ordering.
    struct CAPrioMessage
    {
        // The whole of the ordering. A `std::priority_queue` with `std::less` is a MAX-heap, so
        // the largest value here is what the bot says next.
        bool operator<(const CAPrioMessage& rhs) const { return m_priority < rhs.m_priority; }

        // 28 bytes: peek/dequeueRadioMessage (ELF 0x1C4C00 / 0x1C4C50) copy seven words.
        /*  0 */ AIRadioMessageType m_type;     // getReceivedMessages(m_type) in waitWithSendingMessage
        // enqueueRadioMessage (ELF 0x1C4290) fills all seven words from its arguments.
        /*  4 */ AIRadioChannelType m_channel;  // postEvent's fifth hub argument
        /*  8 */ int m_priority;                // the unsigned argument
        /* 12 */ int m_int12;                   // the last argument; postEvent passes its low byte
        /* 16 */ double m_sendTime;             // now + delay; not sent before this time
        /* 24 */ float m_delay;
    };

    explicit CommunicationAgent(BotMain* owner);
    // ELF 0x1C3CF0 (D1; D2 0x1C3B50): an empty body; the members are torn down by the compiler.
    ~CommunicationAgent();

    void reset();
    void update();
    void pollForRadioMessage(double now);
    void clearOutOldMessages(double now);
    void sendCommunicationAgentMessages(double now);
    void sendCallForMedic(Information* equipment);
    void shutOffHorn(Information* equipment);
    // ELF 0x1C5210: horns at `target` (or radios type 11 without a horn), at most every 20 s
    // towards a bot and every 5 s towards a human.
    void honkHorn(Information* target);
    // ELF 0x1C4A50: true while the type's send block (+48) lies in the future.
    bool getSentMessage(AIRadioMessageType type) const;
    // ELF 0x1C4AA0: the type's AIRadioMessages.ai modifier, 1.0 without a voice.
    float getModifierForMessageType(AIRadioMessageType type) const;
    // ELF 0x1C5460: CommunicationHub::enemySpottedByAI(bot player, information's object, side).
    void setSeenNewEnemy(Information* enemy);
    // ELF 0x1C44A0: file a received message under its type (one entry per sender).
    void receiveRadioMessage(const AIRadioMessageSettings& settings, int senderPlayerId, bool flag);
    void enqueueRadioMessage(AIRadioMessageType type, AIRadioChannelType channel, unsigned int priority,
                             float delay, int c);
    bool peekRadioMessage(CAPrioMessage& out) const;
    bool dequeueRadioMessage(CAPrioMessage& out);
    bool waitWithSendingMessage(CAPrioMessage& message, double now);
    void postEvent(const CAPrioMessage& message) const;   // ELF 0x1C4B10

    // attested by a 7-byte body: `return m_receivedMessages.data + type;`
    const std::list<AIRadioMessageInfo>& getReceivedMessages(AIRadioMessageType type) const
    { return m_receivedMessages[type]; }
    // ELF 0x1C47F0: the first received message of `type` from senderPlayerId, copied out.
    bool getReceivedMessage(AIRadioMessageInfo& out, AIRadioMessageType type, int senderPlayerId) const;
    // ELF 0x1C4A00: any message of `type` from senderPlayerId (-1: from anyone).
    bool receivedMessageExists(AIRadioMessageType type, int senderPlayerId) const;
    // ELF 0x1C4850: marks every unconfirmed (+16, +17 clear) message of `type` confirmed (+16).
    bool generalConfirmReceivedMessage(AIRadioMessageType type);
    // ELF 0x1C4990: the same for the messages of one sender.
    bool confirmReceivedMessage(AIRadioMessageType type, int senderPlayerId);
    // ELF 0x1C48C0: the first message equal to `info` (type, sender, time) when unconfirmed.
    bool confirmReceivedMessage(const AIRadioMessageInfo& info);
    // ELF 0x1C4220: posts {type, channel, priority, c, now, 0.0f} without queueing.
    void sendImmediately(AIRadioMessageType type, AIRadioChannelType channel, unsigned int priority, int c);
    // [INFERRED] inline readers (D's waitForPickup reads +0x5C; F's BBAvoid reads +0x60).
    // +0x60 is written by honkHorn on the bot it horns at. +0x5C is written by reset (-1) and by
    // BBPChange::createPlan (0x23C474: the driver's player id when the driver is a bot, else 0xFF)
    // through the inline setter below ([CHOSEN] name).
    int getInt92() const { return m_int92; }
    void setInt92(int value) { m_int92 = value; }
    double getDouble96() const { return m_double96; }

private:
    typedef std::priority_queue<CAPrioMessage, std::vector<CAPrioMessage>,
                                std::less<CAPrioMessage> > MessageQueue;

    /*   0 */ BotMain* m_bot;                  // the ctor argument, and the first store
    // the constructor never sets this - `reset` does, and the constructor's last statement is a
    // call to `reset`. Same shape as `CommunicationHub`'s own two uninitialised timestamps.
    /*   4 */ CommunicationHub* m_hub;         // = CommunicationHub::m_instance
    // one inbox per message type - eighteen lists, indexed by the enum. See the header block.
    /*   8 */ std::vector<std::list<AIRadioMessageInfo> > m_receivedMessages;
    /*  20 */ std::map<AIRadioMessageType, double> m_map20;   // a time per type [UNRESOLVED]
    // what the bot says next. See the header block.
    /*  32 */ MessageQueue m_queue;            // +32..+47: a 12-byte vector and an empty comparator
    // Eighteen doubles, one per type, zeroed by `reset` and paired with the max-send-period cache
    // below. [INFERRED] that these are the per-type last-sent timestamps.
    /*  48 */ std::vector<double> m_lastSentTime;
    // update (ELF 0x1C4100) polls the radio at most once a second from this time.
    /*  60 */ double m_lastPollTime;           // reset: 0.0
    // the two cached columns of `AIRadioMessages.ai`. Filled once, in the constructor.
    /*  68 */ std::vector<float> m_receiveTimeToLive;
    /*  80 */ std::vector<float> m_maxSendPeriod;
    /*  92 */ int m_int92;                     // reset: -1; BBPChange::createPlan (see above)
    // the game time another bot last horned at this one (honkHorn); BBAvoid reads it.
    /*  96 */ double m_double96;               // reset: 0.0
    // shutOffHorn (ELF 0x1C53F0) releases the horn (PIFire) two seconds after this time.
    /* 104 */ double m_hornTime;               // reset: 0.0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H
