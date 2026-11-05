// Strategic AI for one side: distributes bots, tracks strengths and handles commander requests.
// Side masks assume a two-sided game. State arrays have 44 entries, with element 43 initialized to one.
// [UNRESOLVED] The opaque block after the interface pointer is retained for layout compatibility.
// [RETIRE-WHEN: aidll-02-skeleton] Implement and validate the remaining strategic phases and object queries.

#ifndef AI_AIDLL_AI_AISTRATEGIC_SAI_H        // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGIC_SAI_H

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../../../Math/Vec3.h"   // Vec2 and Vec3 are typedefs in here - never forward-declare

namespace dice {
namespace hfe {
namespace ai {

class AIStrategicObject;
class Bot;
class SAI;
class SquadCoordinator;
class Strategy;

// `~SAI` deallocates the strategy vector's buffer with a
// count that divides by five, so `sizeof(StrategyContainer) == 20`; the destructor also runs
// `__destroy_aux` over the range, which is what proves the vector holds these by value rather than
// by pointer. updateStates subtracts the float at +8 from game time.
// Its name is [INFERRED]. SAI::addStrategy (ELF 0x326C10) builds {0, 0, 0, 1, strategy}.
struct StrategyContainer
{
    float m_f00;          // +0x00, [UNRESOLVED] role; SAI::reset zeroes it
    float m_f04;          // +0x04, [UNRESOLVED] role; SAI::reset zeroes it
    float m_startTime;    // +0x08, [INFERRED] name
    int   m_i0C;          // +0x0C, = 1 [UNRESOLVED]
    Strategy* strategy; // +0x10, borrowed; owned by the StrategyManager; name: AIDLL_r assert "stratIter->strategy != 0" (0x1015BA90)
};

// Running weighted averages over a fixed number of strength categories.
class EnemyStatistics
{
public:
    explicit EnemyStatistics(unsigned int size);
    EnemyStatistics(unsigned int size, float weight);
    ~EnemyStatistics();

    void sample(float* strength, int* count);
    void copy(EnemyStatistics& other);
    void add(EnemyStatistics& other);
    void clear();

    /* +0x00 */ unsigned int m_size;       // bucket count
    /* +0x04 */ float        m_weight;     // = 1.0f unless the two-argument form is used
    /* +0x08 */ float*       m_strength;   // [m_size]
    /* +0x0C */ float*       m_count;      // [m_size] float, fed from an int array
    /* +0x10 */ int          m_samples;    // how many observations have been folded in
};

// [INFERRED] placement beside EnemyStatistics: the complete named ELF methods sit
// immediately after its clear and before ISAI::create. No separate file is attested.
class SampleHelper
{
public:
    explicit SampleHelper(unsigned int size);
    void addSample(float* input, int index, float* strength, int* count);
    void clear(float* strength, int* count);
    unsigned int m_size; // [INFERRED] name, sole field at +0 on both ports
};

// Stateless commander-order interface owned by SAI; construction and destruction are empty.
class CommanderInterface
{
public:
    CommanderInterface();                                     // - (empty)
    ~CommanderInterface();                                    // - (empty)

    void giveAttackOrder(const Vec3& position, SquadCoordinator* squad);   // [FRONTIER]
    void giveMoveOrder(const Vec3& position, SquadCoordinator* squad);     // [FRONTIER]
    void giveDefendOrder(const Vec3& position, SquadCoordinator* squad);   // [FRONTIER]
    void giveAttackOrder(const Vec3& position, Bot* bot);                  // [FRONTIER]
    void giveMoveOrder(const Vec3& position, Bot* bot);                    // [FRONTIER]
    void giveDefendOrder(const Vec3& position, Bot* bot);                  // [FRONTIER]
};

// Strategic AI interface, including human-commander ability requests and their rate limits.
// Preserve the virtual declaration order.
class ISAI
{
public:
    virtual ~ISAI();                                                       // +0 / +1

    virtual void addStrategy(Strategy* strategy) = 0;                      // +2
    virtual void update(double time) = 0;                                  // +3
    virtual void addBot(Bot* bot) = 0;                                     // +4
    virtual void setBotIsDead(int botId) = 0;                              // +5
    virtual void setBotIsAlive(int botId) = 0;                             // +6
    virtual void removeBot(Bot* bot) = 0;                                  // +7
    virtual void setMapXDimension(int x) = 0;                              // +8 empty in retail
    virtual void setMapYDimension(int y) = 0;                              // +9 empty in retail
    virtual bool isEnemy(int side) const = 0;                              // +10
    virtual bool isFriend(int side) const = 0;                             // +11
    virtual float* getEnemyStrengths(int index) const = 0;                 // +12
    virtual float* getEnemyTypes(int index) const = 0;                     // +13 (float counts)
    virtual float* getFriendlyStrengths(int index) const = 0;              // +14
    virtual float* getFriendlyTypes(int index) const = 0;                  // +15 (float counts)
    virtual void setBotHasChangedEquipment(int botId) = 0;                 // +16
    virtual void setNewBotKit(int botId, int kit) = 0;                     // +17
    // Borrowed map address: ELF +0x48 / PE +0x44; caller copies map<int,pair<int,int>>.
    // [INFERRED] const-reference spelling; the binaries cannot distinguish it from a pointer.
    virtual const std::map<int, std::pair<int, int> >& getDeadBots() const = 0; // +18
    virtual void setPlayerIsCommander(bool isCommander) = 0;               // +19
    virtual bool isPlayerCommander() = 0;                                  // +20

    // The commander ability requests - see the block above.
    virtual bool playerRequestsArtillery(int playerId, Vec3 position) = 0;    // +21
    virtual bool playerRequestsUav(int playerId, Vec3 position) = 0;          // +22
    virtual bool playerRequestsSupply(int playerId, Vec3 position) = 0;       // +23
    virtual bool playerRequestsVehicleDrop(int playerId, Vec3 position) = 0;  // +24
    virtual void updateRequestDenyTimers() = 0;                               // +25

    virtual void checkCommanderUseArtillery() = 0;                            // +26
    virtual bool hostileTakeoveInProgress(AIStrategicObject*& object, Vec2& position) = 0;  // +27
                                                                              // dice's spelling
    virtual bool isPosInsideOwnedControlPoint(AIStrategicObject*& object, Vec3 position,
                                              float radius) = 0;              // +28
    virtual bool haveAnyOwnedCps(int side) = 0;                               // +29
    virtual bool haveAnyOwnedAndTakeableCps(int side) = 0;                    // +30
    virtual bool ownAllTakeableCps(int side) = 0;                             // +31
    // Full released-squad count: ELF callers add EAX; PE +0x7C forwards that count.
    virtual int releaseSquadsFromObject(int a, int b) = 0;                    // +32
    virtual bool releaseUnitFromObject(int object, std::set<int>::iterator& it, int side) = 0;  // +33
    virtual bool foundDeadBot(int botId) = 0;                                 // +34
    virtual bool hasChangedEquipment(int botId) = 0;                          // +35
    virtual void equipmentChangedFinished(int botId) = 0;                     // +36
    virtual int  assignBot(std::set<int>::iterator& it, int object, int botId, bool attacker) = 0;      // +37
    virtual bool strategicObjectsHasChanged() = 0;                            // +38
    virtual void registerTeleporter(int id) = 0;                              // +39
    // ELF 0x32E380 returns the eight-byte {m_f00, m_f04} pair through the hidden result pointer;
    // the Vec2 spelling is [INFERRED].
    virtual Vec2 getStrategyHeat(unsigned int index) = 0;                     // +40
    virtual std::string listStrategies() = 0;                                 // +41, by value
    virtual Strategy* getActiveStrategy() = 0;                                // +42
    virtual void addSpawnToPoint(int point) = 0;                              // +43
    virtual int  getSpawnsToPoint(int point) const = 0;                       // +44
    virtual int  getBotCurrentPos(int botId) const = 0;                       // +45
    virtual int  getBotOrderedPos(int botId) const = 0;                       // +46
    virtual int  getBotLastOrderedPos(int botId) const = 0;                   // +47
    virtual CommanderInterface* getCommanderInterface() const = 0;            // +48

    // the factory is static and it is the only way in. : `operator new(0x1FC)` then the
    // constructor. `AIMain::createSAI` is its only caller in this tree.
    static SAI* create(int side, int gridWidth, int gridHeight, Vec2 min, Vec2 max);
};

// Strategic AI state. Offsets describe the original 508-byte object; its seventeen STL
// containers make layout and total size compiler-dependent.
class SAI : public ISAI
{
public:
    // [PLATFORM] BotInfo is 28 bytes with ELF double alignment and 32 with MSVC alignment.
    // Do not serialize its native object representation.
    struct BotInfo
    {
        explicit BotInfo(Bot* bot);
        // ELF 0x32F3D0: leave the assigned object (removeAssigned with release) and the current
        // object's present set - or, unassigned, the occupied set through free(true) - and the
        // free set (+0x44).
        void remove(SAI* sai);
        // ELF 0x32F5B0: removeAssigned through object->getAssignedUnits(side).find(id) (no end
        // check in the image).
        void removeAssigned(AIStrategicObject* object, bool release, SAI* sai);
        // ELF 0x32FF30: m_currentlyOrderedSquare/m_lastOrderedSquare/m_currentlyOccupiedSquare = -1, m_float10 = 0.0f.
        void setAsDead(SAI* sai);
        // ELF 0x32FF70: the id enters the free set (+0x44).
        void setAsFree(SAI* sai);
        // ELF 0x32FDF0: unregister the bot from the object's attacker/defender set (advancing
        // it), move its id from SAI's occupied set (+0xFC) to the free set (+0x44).
        void free(AIStrategicObject* object, bool attacker, std::set<int>::iterator& it, SAI* sai);
        // ELF 0x32FC80: with release, unregister from the current object (m_currentlyOccupiedSquare) and leave the
        // occupied set; the id always enters the free set (+0x44).
        void free(bool release, SAI* sai);
        // ELF 0x32F0E0: the bot respawned at an object: present there, id into the free set.
        void respawn(AIStrategicObject* object, SAI* sai);
        // ELF 0x32FB10: register as attacker/defender of the current object; the id moves from
        // the free set (erasing under the iterator) to the occupied set.
        void assign(std::set<int>::iterator& it, bool attacker, SAI* sai);
        int setLastOrderedSquare(int square);       // ELF 0x32FFD0; stamps m_double14
        // ELF 0x32F660: leave the object's assigned set (erasing under the iterator); with
        // release also SAI's occupied set (+0xFC), otherwise become an available attacker or
        // defender of the current object (m_currentlyOccupiedSquare). Clears m_currentlyOrderedSquare and m_double14.
        void removeAssigned(AIStrategicObject* object, std::set<int>::iterator& it, bool release, SAI* sai);
        // ELF 0x32F290: the bot moves from one object's present set (advancing it) to another's,
        // keeping its attacker/defender role there; m_currentlyOccupiedSquare becomes the new object's index.
        void occupy(std::set<int>::iterator& it, AIStrategicObject* from, AIStrategicObject* to, SAI* sai);
        // ELF 0x32F1F0: the iterator overload with from->getPresentUnits(side).find(id).
        void occupy(AIStrategicObject* from, AIStrategicObject* to, SAI* sai);
        // ELF 0x32F950: ordered to the object (m_currentlyOrderedSquare = m_lastOrderedSquare = its index, m_double14 = now);
        // leaves the current object's attackers/defenders under the iterator and joins the
        // object's assigned set.
        void assignToObject(std::set<int>::iterator& it, AIStrategicObject* object, bool attacker, SAI* sai);
        // ELF 0x32F860: the iterator overload with the current object's available attackers or
        // defenders (getAvailableBots(attacker, side)).find(id).
        void assignToObject(AIStrategicObject* object, bool attacker, SAI* sai);
        // ELF 0x32FA60: the iterator overload with SAI's free set (+0x44).find(id).
        void assign(bool attacker, SAI* sai);

        /* +0x00 */ Bot*   m_bot;
        /* +0x04 */ int    m_currentlyOrderedSquare;       // = -1  // name: AIDLL_r asserts 0x10157690, 0x10159C00 (+4)
        /* +0x08 */ int    m_lastOrderedSquare;       // = -1  // name: AIDLL_r assert 0x101596A0 (+8)
        /* +0x0C */ int    m_currentlyOccupiedSquare;       // = -1  // name: AIDLL_r asserts 0x10157250, 0x10159C00 (+0xC)
        /* +0x10 */ float  m_float10;     // = 0.0f; update accumulates the frame interval here
        /* +0x14 */ double m_double14;    // = 0.0
    };

    // Layout recovered from the inlined request producers and deny-timer sweep. No vptr.
    // Native double alignment gives ELF (+0,+4,+12) / PE 24 (+0,+8,+16).
    // Member names are [INFERRED]; the integer request type, double expiry and player id are
    // pinned by their stores and the typed commander calls. This record is never serialized.
    struct DenyRequest
    {
        int m_type;
        double m_deadline;
        int m_playerId;
    };

    SAI(int side, int gridWidth, int gridHeight, Vec2 min, Vec2 max);
    virtual ~SAI();

    bool reset();                                // ELF 0x32E8A0, non-virtual; answers true

    // the 49 interface slots. Unwritten bodies stop at named frontiers.
    virtual void addStrategy(Strategy* strategy);
    virtual void update(double budget);
    virtual void addBot(Bot* bot);                                     // typed squad registration
    virtual void setBotIsDead(int botId);
    virtual void setBotIsAlive(int botId);
    virtual void removeBot(Bot* bot);
    virtual void setMapXDimension(int x);                              // - empty
    virtual void setMapYDimension(int y);                              // - empty
    virtual bool isEnemy(int side) const;
    virtual bool isFriend(int side) const;
    virtual float* getEnemyStrengths(int index) const;
    virtual float* getEnemyTypes(int index) const;
    virtual float* getFriendlyStrengths(int index) const;
    virtual float* getFriendlyTypes(int index) const;
    virtual void setBotHasChangedEquipment(int botId);
    virtual void setNewBotKit(int botId, int kit);
    virtual const std::map<int, std::pair<int, int> >& getDeadBots() const;
    virtual void setPlayerIsCommander(bool isCommander);
    virtual bool isPlayerCommander();
    virtual bool playerRequestsArtillery(int playerId, Vec3 position);
    virtual bool playerRequestsUav(int playerId, Vec3 position);
    virtual bool playerRequestsSupply(int playerId, Vec3 position);
    virtual bool playerRequestsVehicleDrop(int playerId, Vec3 position);
    virtual void updateRequestDenyTimers();
    virtual void checkCommanderUseArtillery();
    virtual bool hostileTakeoveInProgress(AIStrategicObject*& object, Vec2& position);
    virtual bool isPosInsideOwnedControlPoint(AIStrategicObject*& object, Vec3 position,
                                              float radius);
    virtual bool haveAnyOwnedCps(int side);
    virtual bool haveAnyOwnedAndTakeableCps(int side);
    virtual bool ownAllTakeableCps(int side);
    virtual int releaseSquadsFromObject(int a, int b);                    // frontier
    virtual bool releaseUnitFromObject(int object, std::set<int>::iterator& it, int side);   // ELF 0x32D6D0
    virtual bool foundDeadBot(int botId);                    // ELF 0x329250
    virtual bool hasChangedEquipment(int botId);
    virtual void equipmentChangedFinished(int botId);
    virtual int  assignBot(std::set<int>::iterator& it, int object, int botId, bool attacker);   // ELF 0x32D870
    virtual bool strategicObjectsHasChanged();
    virtual void registerTeleporter(int id);
    virtual Vec2 getStrategyHeat(unsigned int index);
    virtual std::string listStrategies();
    virtual Strategy* getActiveStrategy();
    virtual void addSpawnToPoint(int point);
    virtual int  getSpawnsToPoint(int point) const;
    virtual int  getBotCurrentPos(int botId) const;
    virtual int  getBotOrderedPos(int botId) const;
    virtual int  getBotLastOrderedPos(int botId) const;
    virtual CommanderInterface* getCommanderInterface() const;

    // non-virtual. The vtable does not carry these.
    void clearStates(float* a, float* b);
    // update calls these directly. Strength/state/preparation callers are recovered; later phases retain
    // named stopping bodies. No field or virtual slot is added by these declarations.
    void updateStrengths();
    void updateStates(float* a, float* b);
    void prepareUpdate();
    void clearSpawnsToPoints();                      // PE inlined in prepareUpdate
    void handleTeleportingBot(int botId);            // named frontier
    bool updateStrategicObjects();
    void chooseStrategy();
    void changeStrategy(std::vector<StrategyContainer>::iterator& it);   // ELF 0x329610
    void modifyStrategicObjectsTemperatures();
    void updateBotPositions();
    void updateDistribution();
    void distributeResources();
    void distributeDefence();
    void distributeAttack();
    void distributeWithNoAreas();
    void categoryDistributeWithNoAreas(const std::vector<AIStrategicObject*>& objects);  // ELF 0x32B770
    void categoryDistributeAttack(const std::vector<AIStrategicObject*>& objects);       // ELF 0x32BA50
    void categoryDistributeDefence(const std::vector<AIStrategicObject*>& objects);      // ELF 0x32AF10
    void distributeArtillery();                     // genuinely empty, PE update elides it
    void updateMap(double budget);                  // ELF 0x32CEE0, empty
    float* createUnitMods() const;                  // ELF 0x329490
    bool isBotFree(int botId) const;                // ELF 0x32E2D0
    bool isBotAssigned(int botId) const;            // ELF 0x32E320
    Strategy* getActiveStrategy(float& a, float& b, float& c);   // ELF 0x32EAB0

private:
    // ELF 0x328DB0 (setBotIsDead's body, a direct call).
    void handleDeadBot(int botId);
    // ELF 0x328FE0 (setBotHasChangedEquipment's tail, a direct call).
    void handleChangingBot(int botId);
    /* +0x000 */ // vptr - ISAI
    // forty bytes no constructor store and no destructor call ever touches. [UNRESOLVED]; kept
    // opaque so every offset below stays where the binary puts it.
    /* +0x004 */ unsigned char m_unresolved04[40];

    /* +0x02C */ std::vector<int> m_vector2C;        // 4-byte element; type [UNRESOLVED]
    /* +0x038 */ std::vector<int> m_vector38;        // 4-byte element; type [UNRESOLVED]
    /* +0x044 */ std::set<int>    m_freeBots;   // name: AIDLL_r assert "sai->m_freeBots" 0x10157AB0 (PE +0x4C)
    /* +0x050 */ std::vector<BotInfo*> m_bots;    // resized to getMaxNBots by the constructor  // name: AIDLL_r assert "m_bots[botId] != 0" 0x1015CF40 (PE +0x58)
    /* +0x05C */ std::map<int, std::pair<int, int> > m_map5C;

    // Five doubles. Four are stamped from the timer at construction: three at now + 60 and one at
    // now + 10, with the fifth left at zero. Deadlines, not durations.
    /* +0x068 */ double m_time68;                    // = now + 60
    /* +0x070 */ double m_time70;                    // = now + 60
    /* +0x078 */ double m_time78;                    // = now + 60
    /* +0x080 */ double m_time80;                    // = now + 10
    /* +0x088 */ double m_time88;                    // = 0

    /* +0x090 */ std::list<DenyRequest*> m_denyRequests;
    /* +0x094 */ std::vector<Bot*> m_vector94;       // addBot's push_back attests the element type
    /* +0x0A0 */ std::vector<Bot*> m_vectorA0;       // removal queue; ELF prepareUpdate's erase names Bot*
    /* +0x0AC */ bool m_flagAC;                      // = true
    /* +0x0B0 */ std::set<int> m_setB0;
    /* +0x0BC */ std::vector<int> m_vectorBC;        // 4-byte element; type [UNRESOLVED]

    // four behaviour IDS resolved by name at construction, through BotBehaviourManager. If a
    // behaviour is missing the manager returns -1 and the id is stored anyway.
    /* +0x0C8 */ int m_idleBehaviourId;                // "Idle"  // name: AIDLL_r assert 0x10160AB0 (PE +0xEC)
    /* +0x0CC */ int m_moveToBehaviourId;              // "MoveTo"  // name: AIDLL_r assert 0x10160AB0 (PE +0xF0)
    /* +0x0D0 */ int m_avoidBehaviourId;               // "Avoid"  // name: AIDLL_r assert 0x10160AB0 (PE +0xF4)
    /* +0x0D4 */ int m_fireBehaviourId;                // "Fire"  // name: AIDLL_r assert 0x10160AB0 (PE +0xF8)

    /* +0x0D8 */ int m_intD8;                        // [UNRESOLVED] - the ctor zeroes it
    /* +0x0DC */ int m_intDC;                        // [UNRESOLVED] - the ctor zeroes it
    /* +0x0E0 */ int m_intE0;                        // [UNRESOLVED]; not constructed, updateDistribution zeroes it
    /* +0x0E4 */ std::map<int, std::map<int, int> > m_mapE4;   // a map of maps - node 32
    /* +0x0F0 */ std::set<int> m_setF0;
    /* +0x0FC */ std::set<int> m_assignedBots;   // name: AIDLL_r assert "sai->m_assignedBots" 0x10157AB0 (PE +0x120)
    /* +0x108 */ std::list<int> m_list108;           // 4-byte element; type [UNRESOLVED]
    /* +0x10C */ std::list<int> m_list10C;           // 4-byte element; type [UNRESOLVED]
    /* +0x110 */ std::list<int> m_list110;           // teleporter ids (registerTeleporter)

    // by value, not by pointer - the destructor runs `__destroy_aux<StrategyContainer*>` over the
    // range, and the deallocation size divides by five, so `sizeof(StrategyContainer) == 20`.
    /* +0x114 */ std::vector<StrategyContainer> m_strategies;
    /* +0x120 */ StrategyContainer* m_activeStrategy;   // = m_strategies.end at construction

    /* +0x124 */ EnemyStatistics* m_enemyStats;      // six buckets
    /* +0x128 */ EnemyStatistics* m_friendlyStats;   // six buckets
    // Names [INFERRED] from updateStrengths' side selection; all four arrays are float on both
    // ports . Counts accumulate security, not whole units.
    /* +0x12C */ float m_enemyStrengths[6];          // all zeroed with the two above
    /* +0x144 */ float m_enemyCounts[6];
    /* +0x15C */ float m_friendlyStrengths[6];
    /* +0x174 */ float m_friendlyCounts[6];

    /* +0x18C */ int   m_int18C;                     // = 19; resumable strategic update state
    /* +0x190 */ float m_float190;                   // = -1000.0f; last-update timestamp used by state 4
    /* +0x194 */ int   m_side;                       // the constructor's first argument
    /* +0x198 */ int   m_sideMask;                   // = 1 << side
    /* +0x19C */ int   m_enemyMask;                  // = (side == 1) ? 4 : 2 - the other side's bit

    /* +0x1A0 */ std::map<int, int> m_map1A0;
    /* +0x1AC */ std::map<int, int> m_map1AC;
    /* +0x1B8 */ std::list<int> m_list1B8;           // 4-byte element; type [UNRESOLVED]
    /* +0x1BC */ std::list<int> m_list1BC;           // 4-byte element; type [UNRESOLVED]

    // Eight bools in a twenty-byte span the constructor writes individually; the gaps between them
    // are [UNRESOLVED] and untouched.
    /* +0x1C0 */ bool m_flag1C0;                     // = false
    /* +0x1C1 */ bool m_flag1C1;                     // = false
    /* +0x1C2 */ unsigned char m_unresolved1C2[10];  // +0x1C2 .. +0x1CB, never written
    /* +0x1CC */ bool m_flag1CC;                     // = false
    /* +0x1CD */ bool m_flag1CD;                     // = false
    /* +0x1CE */ bool m_flag1CE;                     // = true
    /* +0x1CF */ bool m_flag1CF;                     // = false; cumulative strategic-object change
    /* +0x1D0 */ bool m_flag1D0;                     // = true; restart strategic-object traversal
    /* +0x1D1 */ bool m_flag1D1;                     // = false; player is commander
    /* +0x1D2 */ unsigned char m_unresolved1D2[2];

    // ELF +0x1D4 / PE +0x224: persisted begin/dereference/increment/end cursor.
    // [INFERRED] name and native iterator spelling; never transplant retail STL offsets.
    std::vector<AIStrategicObject*>::const_iterator m_strategicObjectIterator;
    /* +0x1D8 */ std::map<int, int> m_map1D8;

    // The four 44-float state arrays. `clearStates` is what pairs them: (A, C) and (B, D).
    /* +0x1E4 */ float* m_stateA;
    /* +0x1E8 */ float* m_stateB;
    /* +0x1EC */ float* m_stateC;
    /* +0x1F0 */ float* m_stateD;

    /* +0x1F4 */ int m_int1F4;                       // = 0; state buffer selector, toggled with !value
    /* +0x1F8 */ CommanderInterface* m_commander;    // owned; deleted by ~SAI
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_SAI_H
