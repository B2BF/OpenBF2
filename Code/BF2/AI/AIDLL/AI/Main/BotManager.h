// Owns pooled bots and schedules their updates.
// Work queues use update deadlines and bot detail levels to distribute processing across ticks.

#ifndef AI_AIDLL_AI_MAIN_BOTMANAGER_H        // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_BOTMANAGER_H

#include <list>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "IBotManager.h"           // Base interfaces.

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class IAIObject;
class IAIMain;

// Owns bot scheduling and receives AIMain's remaining tick budget. Offsets describe the
// original 272-byte object; its real interface bases and STL members determine native layout.
class BotManager : public IBotManager, public IBotManagerModifier
{
public:
    // Four-byte scheduled-update tag. Construction seeds one entry for each of five types.
    struct BMUpdate
    {
        enum Type
        {
            Idle            = 0,    // the switch's empty arm - scheduled, does nothing
            DecisionMaking  = 1,    // share * 0.200
            Pathfinding     = 2,    // share * 0.300
            Hearing         = 3,    // share * 0.025
            Sensing         = 4     // share * 0.150
            // no default. Anything else reaches Debug(Error) at BotManager.cpp:1179 and aborts.
        };

        // The constructor seeds the ring with `BMUpdate(0)` through `BMUpdate(4)`.
        explicit BMUpdate(int t) : type(t) {}

        int type;
    };

// Queue predicates compare the bot fields used to schedule deferred work.
    struct BotInfo
    {
        explicit BotInfo(Bot* bot);
        // * +0
        // one dereferences the record: `return *(Bot)m_botInfos[index];`. So the first word
        // of a BotInfo is the bot it describes.
        Bot* m_bot;                 // +0
        unsigned m_word4;           // +4 = 0 [UNRESOLVED]
        int  m_priority;            // +8 - BotPriorityPred orders on this
        unsigned m_word12;          // +12 = 0 [UNRESOLVED]
        int  m_sensingPriority;     // +16 - BotSensingPriorityPred orders on this
        bool m_flag20;              // +20 = false [UNRESOLVED]
        double m_double24;          // +24 = 0 [UNRESOLVED]
        // [INFERRED] Accumulates decisionMaking's result. Construction leaves the initial count unset.
        int  m_behaviourUpdateCount; // +32 untouched by the constructor
        unsigned m_word36;          // +36 = 0 [UNRESOLVED]
        unsigned m_word40;          // +40 = 0 [UNRESOLVED]
        unsigned m_word44;          // +44 = 0 [UNRESOLVED]
        double m_double48;          // +48 = 0 [UNRESOLVED]
        double m_double56;          // +56 = 0 [UNRESOLVED]
        double m_double64;          // +64 = 0 [UNRESOLVED]
        bool m_flag72;              // +72 = true [UNRESOLVED]
        bool m_flag73;              // +73 = false [UNRESOLVED]
        double m_double76;          // ELF +76 / PE +80 = 0 [UNRESOLVED]
        // [INFERRED] Pending damage and the attacking bot's roster index. updateDamages applies
        // positive damage and clears it each tick; the writer remains unidentified.
        float m_damage;             // ELF +84 / PE +88 = 0 (the ctor's zero word)
        int m_attackerIndex;        // ELF +88 / PE +92 untouched by the constructor
    };

    // The two heap orderings, written from the inlined comparisons above. Both are `less`-shaped
    // so both queues are max-heaps on their respective field.
    struct BotPriorityPred
    {
        bool operator()(const BotInfo* a, const BotInfo* b) const
        {
            return a->m_priority < b->m_priority;
        }
    };

    struct BotSensingPriorityPred
    {
        bool operator()(const BotInfo* a, const BotInfo* b) const
        {
            return a->m_sensingPriority < b->m_sensingPriority;
        }
    };

    // Priority queue of bot records ordered by the supplied scheduling predicate.
    template <class Pred>
    class BotPriorityQueue : public std::priority_queue<BotInfo*, std::vector<BotInfo*>, Pred>
    {
    public:
        void clear() { this->c.resize(0); }
    };

    typedef BotPriorityQueue<BotPriorityPred>        BotQueue;
    typedef BotPriorityQueue<BotSensingPriorityPred> SensingQueue;

    BotManager();
    // Virtual destruction is inherited from both interfaces.
    virtual ~BotManager();

    // the tick
    void action(const double& budget);

    void updateDamages();
    void actionComAgent();
    void actionExecutePlan(const double& slice);
    void actionUpdateNavPoints();
    void actionDecisionMaking(const double& slice);
    void actionPathfinding(const double& slice);
    void actionHearing(const double& slice);
    void actionSensing(const double& slice);

    // Bot-manager interface overrides; preserve virtual declaration order.
    virtual int getBotCount() const;                                                                // +0x008
    virtual int getNumberOfActiveBots() const;                                                      // +0x00C
    virtual int getNumberOfDeactivedBots() const;                                                   // +0x010
    virtual Bot* getBot(unsigned int index) const;                                                  // +0x014
    virtual Bot* getBotFromId(unsigned int id) const;                                               // +0x018
    virtual Bot* getBotFromPlayerId(int playerId) const;                                            // +0x01C
    virtual Bot* getBotFromName(const std::string& name) const;                                     // +0x020
    virtual Bot* getBot(IAIObject* object) const;                                                   // +0x024
    virtual Bot* getLastCreatedBot() const;                                                         // +0x028
    virtual void updateBotVehicle(Bot* bot, IAIObject* vehicle);                                    // +0x02C
    virtual void setBotChangedVehicle(int id, IAIObject* vehicle);                                  // +0x030
    virtual void setBotIsDead(int id);                                                              // +0x034
    virtual void setBotIsAlive(int id);                                                             // +0x038
    virtual void setBotIsManDown(int id, IAIObject* object);                                        // +0x03C
    virtual void setBotIsRevived(int id, IAIObject* object);                                        // +0x040
    virtual void setBotReceivedRadioMessage(const AIRadioMessageSettings& settings, int a, int b);  // +0x044
    virtual void setBotRecievedAttackCommanderOrder(int id);                                        // +0x048
    virtual void setBotRecievedDefendCommanderOrder(int id);                                        // +0x04C
    virtual void setBotRecievedMoveCommanderOrder(int id);                                          // +0x050
    virtual void setBotRecievedAttackSquadOrder(int id);                                            // +0x054
    virtual void setBotRecievedDefendSquadOrder(int id);                                            // +0x058
    virtual void setBotRecievedMoveSquadOrder(int id);                                              // +0x05C
    virtual void setBotTeamKilled(int a, int b);                                                    // +0x060
    virtual void registerDamageOnBot(float damage, int a, int b);                                   // +0x064
    virtual float getVehicleBailValue();                                                            // +0x068
    virtual void removeMineTarget(unsigned int handle);                                             // +0x06C
    virtual bool reset();                                                                           // +0x070
    virtual void setRenderBotSensing(bool render);                                                  // +0x074
    virtual bool getRenderBotSensing() const;                                                       // +0x078
    virtual void writeBotPlans();                                                                   // +0x07C
    virtual void increaseTabLevel();                                                                // +0x080
    virtual void decreaseTabLevel();                                                                // +0x084
    virtual void tabulate(std::ostream& out);                                                       // +0x088
    virtual void setLodLevelTicks(unsigned full, unsigned medium, unsigned light);                  // +0x08C
    virtual void setLodLevelPriority(unsigned full, unsigned medium, unsigned light);               // +0x090
    virtual void setPlannedDecisionMakingThreshold(const double& full, const double& medium, const double& light); // +0x094
    virtual void setUnplannedDecisionMakingThreshold(const double& full, const double& medium, const double& light); // +0x098
    virtual void setDecisionMakingInterleave(unsigned a, unsigned b);                               // +0x09C
    virtual void setSensingQuotient(unsigned a, unsigned b);                                        // +0x0A0
    virtual void setSystemQuotient(unsigned a, unsigned b, unsigned c);                             // +0x0A4
    virtual unsigned getLodLevelFullTicks() const;                                                  // +0x0A8
    virtual unsigned getLodLevelMediumTicks() const;                                                // +0x0AC
    virtual unsigned getLodLevelLightTicks() const;                                                 // +0x0B0
    virtual unsigned getLodLevelFullPriority() const;                                               // +0x0B4
    virtual unsigned getLodLevelMediumPriority() const;                                             // +0x0B8
    virtual unsigned getLodLevelLightPriority() const;                                              // +0x0BC
    virtual double getPlannedDecisionMakingThresholdFull() const;                                   // +0x0C0
    virtual double getPlannedDecisionMakingThresholdMedium() const;                                 // +0x0C4
    virtual double getPlannedDecisionMakingThresholdLight() const;                                  // +0x0C8
    virtual double getUnplannedDecisionMakingThresholdFull() const;                                 // +0x0CC
    virtual double getUnplannedDecisionMakingThresholdMedium() const;                               // +0x0D0
    virtual double getUnplannedDecisionMakingThresholdLight() const;                                // +0x0D4
    virtual unsigned getPlannedDecisionMakingInterleave() const;                                    // +0x0D8
    virtual unsigned getUnplannedDecisionMakingInterleave() const;                                  // +0x0DC
    virtual double getSensingQuotient() const;                                                      // +0x0E0
    virtual double getMemoryUpdateQuotient() const;                                                 // +0x0E4
    virtual double getSystemDecisionMakingQuotient() const;                                         // +0x0E8
    virtual double getSystemPathfindingQuotient() const;                                            // +0x0EC
    virtual double getSystemSensingQuotient() const;                                                // +0x0F0
    virtual void resetStatistics();                                                                 // +0x0F4
    virtual void resetStatistics(unsigned int index) const;                                         // +0x0F8
    virtual unsigned getPlanExecutionUpdateCount(unsigned int index) const;                         // +0x0FC
    virtual unsigned getDecisionMakingUpdateCount(unsigned int index) const;                        // +0x100
    virtual unsigned getSensingUpdateCount(unsigned int index) const;                               // +0x104
    virtual unsigned getDecisionMakingBehaviourUpdateCount(unsigned int index) const;               // +0x108
    virtual double getPlanExecutionRunTime(unsigned int index) const;                               // +0x10C
    virtual double getDecisionMakingRunTime(unsigned int index) const;                              // +0x110
    virtual double getSensingRunTime(unsigned int index) const;                                     // +0x114
    virtual bool isPathfinding(unsigned int index) const;                                           // +0x118
    virtual bool getLastPathfindingSucceeded(unsigned int index) const;                             // +0x11C
    virtual bool getPathfindingStall(unsigned int index) const;                                     // +0x120
    virtual double getLastPathfindingRunTime(unsigned int index) const;                             // +0x124
    virtual void enableBotScriptDebugger(bool enable);                                              // +0x128

    // population, and not part of either base
    // These methods are absent from both vtables, so they are BotManager's own non-virtual API.
    bool addBot(int playerId, int side, float* weights, IAIObject* object);
    void buildObjectMap();
    // Adds an unspawned bot for playerId and side using personality weights; returns true.
    bool addUnspawnedBot(int playerId, int side, float* personality);
    // Returns whether the player's bot was reactivated on the requested side.
    bool reactivateBot(int playerId, int side, const std::string& name);
    // Both answer the deactivated bot's id, or -1 (ELF 0x3DD240 / 0x3DD450).
    int deactivateBot(int side);
    int deactivateBotFromPlayerId(int playerId);                    // indexes m_botByPlayerId

    // ELF .bss 0x4CE8F8 (4 bytes); increaseTabLevel / decreaseTabLevel step it. [INFERRED] type int.
    static int s_tabLevel;

private:
// Three priority queues hold pending bot work. Native container sizes depend on the compiler ABI.
    /*   8 */ BotQueue*     m_queuePriorityA;
    /*  12 */ BotQueue*     m_queuePriorityB;
    /*  16 */ SensingQueue* m_queueSensing;
    /*  20 */ std::list<Bot*> m_bots;        // the ctor inserts every preallocated BotMain
    /*  24 */ std::vector<BotInfo*> m_botInfos;         // reserve(getMaxNBots)
    // Deactivated bot ids in oldest-first order; values index m_botInfos.
    /*  36 */ std::list<unsigned int> m_deactivatedBots;   // bot ids, oldest first
    // [CHOSEN name] Player-id to bot-index table, sized to 256 and initialized to -1.
    /*  40 */ std::vector<int> m_botByPlayerId;
    // actionComAgent's round-robin position in m_botInfos (ELF +0x34 / PE +0x4C); unsigned from
    // its `jb` wrap test. Name [INFERRED].
    /*  52 */ unsigned int m_comAgentCursor;  // = 0
    /*  56 */ std::list<void*> m_list56;     // allocate(12) -> a 4-byte element. [UNRESOLVED] type
    /*  60 */ IAIMain* m_aiMain;             // = IAIMain::m_instance, cached at construction

    // Scheduling priorities, rates and deferred-work queues.
    /*  64 */ unsigned m_lodLevelFullPriority;        // ] setLodLevelPriority(3, 3, 3)
    /*  68 */ unsigned m_lodLevelMediumPriority;      // ]
    /*  72 */ unsigned m_lodLevelLightPriority;       // ]
    /*  76 */ unsigned m_plannedDecisionMakingInterleave;    // ] setDecisionMakingInterleave(2, 2)
    /*  80 */ unsigned m_unplannedDecisionMakingInterleave;  // ]
    /*  84 */ double m_plannedDecisionMakingThresholdFull;     // ] setPlanned...(0.5, 0.5, 0.5)
    /*  92 */ double m_plannedDecisionMakingThresholdMedium;   // ] each stored as fabs
    /* 100 */ double m_plannedDecisionMakingThresholdLight;    // ]
    /* 108 */ double m_unplannedDecisionMakingThresholdFull;   // ] setUnplanned...(0.3, 0.4, 0.4)
    /* 116 */ double m_unplannedDecisionMakingThresholdMedium; // ] each stored as fabs
    /* 124 */ double m_unplannedDecisionMakingThresholdLight;  // ]
    /* 132 */ unsigned m_lodLevelFullTicks;           // ] setLodLevelTicks(6, 6, 6)
    /* 136 */ unsigned m_lodLevelMediumTicks;         // ]
    /* 140 */ unsigned m_lodLevelLightTicks;          // ]
    // setSystemQuotient(30, 40, 30) -> 0.3 and 0.4. The third share is computed and discarded.
    /* 144 */ double m_systemDecisionMakingQuotient;
    /* 152 */ double m_systemQuotient2;               // [UNRESOLVED] which subsystem
    // setSensingQuotient(1, 1) -> 0.5 and 0.5, stored as q and 1 - q.
    /* 160 */ double m_sensingQuotient;
    /* 168 */ double m_sensingQuotientInverse;
    // [INFERRED] Per-tick counters reset by actionExecutePlan. Construction leaves them unset.
    // [UNRESOLVED] The other two counters' writers remain unidentified.
    /* 176 */ unsigned int m_planExecutionsThisTick;
    /* 180 */ unsigned int m_tickCount180;   // [UNRESOLVED]
    /* 184 */ unsigned int m_tickCount184;   // [UNRESOLVED]
    // [INFERRED] Player id at which hearing's occupied-object traversal resumes.
    /* 188 */ int    m_hearingResumeId;      // = 0
    /* 192 */ void*  m_ptr192;               // = 0
    /* 196 */ void*  m_ptr196;               // = 0
    /* 200 */ void*  m_ptr200;               // = 0
    // Object -> bot id, std::map<IAIObject*, unsigned int> by the shipped instantiations
    // (ELF getBot(IAIObject*) 0x3DB120 find, updateBotVehicle 0x3E0100 insert_unique). Name [INFERRED].
    /* 204 */ std::map<IAIObject*, unsigned int> m_vehicleBots;  // +204 node, +208 count, +212 comparator
    // [INFERRED] Shared scratch selection, cleared and filled from queues by decision-making
    // and sensing passes before they process the selected bots.
    /* 216 */ std::vector<BotInfo*> m_selectedBots;
    /* 228 */ Bot*   m_lastCreatedBot;       // addUnspawnedBot / getLastCreatedBot (PE +276)
    /* 232 */ float  m_float232;             // Initially 0.3f. [UNRESOLVED] Purpose is unknown.
    /* 236 */ bool   m_flag236;              // = false
    // [INFERRED] Player-id/object pairs populated by the environment for hearing updates.
    /* 240 */ std::vector<std::pair<int, IAIObject*> > m_occupiedObjects;
    // actionUpdateNavPoints' round-robin position (ELF +0xFC / PE +0x130), advanced with an
    // unsigned `div`. Name [INFERRED].
    /* 252 */ unsigned int m_navPointCursor; // = 0
    // [INFERRED] Alternates the smaller half-roster pass for odd bot counts and stores the
    // plan executor's round-robin cursor.
    /* 256 */ bool   m_planHalfToggle;       // = true
    /* 260 */ unsigned int m_planCursor;     // = 0
    // [INFERRED] Pathfinding cursor, advanced modulo the live bot count with unsigned arithmetic.
    /* 264 */ unsigned int m_pathfindingCursor; // = 0
    // Round-robin work list seeded with one entry for each of the five BMUpdate types.
    /* 268 */ std::list<BMUpdate> m_updates;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_BOTMANAGER_H
