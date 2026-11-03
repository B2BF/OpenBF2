// Owns the AI simulation lifecycle, population and per-frame work.
// Uses the AI timer rather than the engine frame delta.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AIMAIN_H       // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_AIMAIN_H

#include <list>
#include <map>
#include <string>
#include <utility>   // the per-bot pair series below
#include <vector>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {

// Include Vec3's definition: it is a Math alias and cannot be forward-declared as a class.
namespace world { class IPlayer; }

namespace ai {

class Bot;
class BotManager;
class IAIObject;
class SAI;

class IAIMain
{
public:
    IAIMain();
    virtual ~IAIMain();                                                     // +0x00 / +0x04

    virtual void        addNewPlayer(world::IPlayer* player)         = 0;   // +0x08
    virtual world::IPlayer* popPlayer()                              = 0;   // +0x0C front of the queue, or null
    virtual void        registerFreeBot(Bot* bot)                    = 0;   // +0x10
    // Returns whether the player was activated or queued successfully.
    virtual bool        activateBot(int playerId, int side, const std::string& name) = 0; // +0x14
    virtual bool        deactivateBot(int& playerId, int side)       = 0;   // +0x18 ELF answers 0/1
    virtual bool        deactivateBot(int playerId)                  = 0;   // +0x1C ELF answers 0/1
    virtual void        newPlayerCreated(bool b)                     = 0;   // +0x20
    virtual void        reactivateBot(int a)                         = 0;   // +0x24
    virtual void        switchSides(int a)                           = 0;   // +0x28
    // parameter names
    // the body looks it up with `IBotManager::getBotFromPlayerId` rather than `getBotFromId`.
    virtual void        switchSides(int side, int playerId)          = 0;   // +0x2C
    virtual bool        isInitialised()                              = 0;   // +0x30 (British - see header)
    virtual bool        action()                                     = 0;   // +0x34 <- the AI tick
    virtual bool        getSAIEnable()                               = 0;   // +0x38
    virtual SAI*        getSAI(int side)                             = 0;   // +0x3C
    virtual void        draw()                                       = 0;   // +0x40
    virtual void        updateAIObject(IAIObject* o)                 = 0;   // +0x44
    virtual bool        reset()                                      = 0;   // +0x48
    virtual bool        resetLevel()                                 = 0;   // +0x4C
    virtual void        queueSoundEffect(const Vec3& pos, int a, int b) = 0;// +0x50
    virtual void        spawnBots()                                  = 0;   // +0x54
    virtual void        setAISystemQuotient(const double& q)         = 0;   // +0x58
    virtual double      getAISystemQuotient() const                  = 0;   // +0x5C
    virtual void        setPlayerExitedVehicle(IAIObject* o, int a)  = 0;   // +0x60
    virtual void        setPlayerEnteredVehicle(IAIObject* o, IAIObject* v, int a) = 0; // +0x64
    virtual void        commanderOrderAccepted(int a)                = 0;   // +0x68
    virtual void        commanderOrderRejected(int a)                = 0;   // +0x6C
    virtual void        appliedForCommander(int a)                   = 0;   // +0x70
    virtual void        acceptedAsCommander(int a)                   = 0;   // +0x74
    virtual void        resignedAsCommander(int a)                   = 0;   // +0x78 <- last slot

    // Construction unconditionally replaces this pointer; callers must maintain a single
    // live IAIMain rather than rely on registry ownership.
    static IAIMain* m_instance;
};

// AI runtime implementation. Native object size depends on the compiler's STL layout;
// do not use retail object sizes for allocation in this build.
class AIMain : public IAIMain
{
public:
    AIMain();
    virtual ~AIMain();

    // IAIMain, in vtable order
    virtual void        addNewPlayer(world::IPlayer* player);
    virtual world::IPlayer* popPlayer();
    virtual void        registerFreeBot(Bot* bot);
    virtual bool        activateBot(int playerId, int side, const std::string& name);
    virtual bool        deactivateBot(int& playerId, int side);
    virtual bool        deactivateBot(int playerId);
    virtual void        newPlayerCreated(bool b);
    virtual void        reactivateBot(int a);

    // Queues an unspawned bot with its side-specific personality weights.
    // This nonvirtual AIMain entry is distinct from BotManager::addUnspawnedBot.
    bool addUnspawnedBot(int playerId, int side, float* personality);
    bool addBot(int playerId, int side, float* personality, IAIObject* object);
    virtual void        switchSides(int a);
    virtual void        switchSides(int side, int playerId);
    virtual bool        isInitialised();
    virtual bool        action();
    virtual bool        getSAIEnable();
    virtual SAI*        getSAI(int side);
    virtual void        draw();                                             // - empty
    virtual void        updateAIObject(IAIObject* o);
    virtual bool        reset();
    virtual bool        resetLevel();
    virtual void        queueSoundEffect(const Vec3& pos, int a, int b);    // - empty
    virtual void        spawnBots();
    virtual void        setAISystemQuotient(const double& q);
    virtual double      getAISystemQuotient() const;
    virtual void        setPlayerExitedVehicle(IAIObject* o, int a);
    virtual void        setPlayerEnteredVehicle(IAIObject* o, IAIObject* v, int a);
    // These five commander hooks intentionally do nothing in this implementation.
    virtual void        commanderOrderAccepted(int a);                      // - empty
    virtual void        commanderOrderRejected(int a);                      // - empty
    virtual void        appliedForCommander(int a);                         // - empty
    virtual void        acceptedAsCommander(int a);                         // - empty
    virtual void        resignedAsCommander(int a);                         // - empty

    // Initializes AI and returns its status message. The integer parameter is unused.
    std::string init(int);
    void        setAISystemComponentsQuotient(unsigned int botManager, unsigned int strategic);

    // Creates per-side strategic AI. setSAIEnable returns the resulting enabled state,
    // including false when enabling is refused.
    void        createSAI();
    bool        setSAIEnable(bool enable);
    void        setSAIMapXDimension(int n);
    void        setSAIMapYDimension(int n);
    int         getSAIMapXDimension() const;
    int         getSAIMapYDimension() const;

    void        setRenderSide(int side);
    int         getRenderSide() const;

    void        setValidateDataStructuresEnabled(bool b);
    bool        getValidateDataStructuresEnabled() const;
    void        setShowBotUrgencies(bool b);
    bool        getShowBotUrgencies() const;
    void        setShowBotSpawnCoord(bool b);
    bool        getShowBotSpawnCoord() const;
    void        setShowStrategies(bool b);
    bool        getShowStrategies() const;
    void        setShowSAIStats(bool b);
    bool        getShowSAIStats() const;
    void        setShowBotStats(bool b);
    bool        getShowBotStats() const;
    void        setShowStrategicAreas(bool b);
    bool        getShowStrategicAreas() const;

    void        showAll(bool b);
    void        showStrategic(bool b);
    void        showBot(bool b);

    // Queued positional sound effect. [INFERRED] Field names and meanings are incomplete.
    struct SoundEffect
    {
        Vec3 m_position;    // [INFERRED]
        int  m_a;           // [INFERRED] [UNRESOLVED] meaning
        int  m_b;           // [INFERRED] [UNRESOLVED] meaning
    };

private:
    // Offsets describe the original layout. [CHOSEN] Names are provisional unless identified
    // by accessors. The three leading doubles partition frame time between AI and its subsystems.
    /*   0 */ // vptr
    /*   4 */ double        m_aiSystemQuotient;         // Frame-time share, initialized by widening 0.15f to double.
    /*  12 */ double        m_botManagerQuotient;       // = 0.875
    /*  20 */ double        m_componentQuotient;        // = 0.125; fraction of the AI budget
                                                        // handed to the selected side's SAI
                                                        // with action's additional 0.95 factor.
    /*  28 */ double        m_double28;                 // = 0.0 [UNRESOLVED]
    /*  36 */ int           m_dword36;                  // = 0
public:
    // [CHOSEN, not recovered] Visibility for the generated inline console property.
    /*  40 */ double        m_botStatisticUpdateSpeed;  // = 3.0; console stores its nonnegative magnitude
private:
    // Parallel per-bot data arrays, appended together when a bot is admitted.
    /*  48 */ std::vector<std::pair<float, float> > m_perBot48;
    /*  60 */ std::vector<std::pair<float, float> > m_perBot60;
    /*  72 */ std::vector<std::pair<float, float> > m_perBot72;
    /*  84 */ std::vector<float>                    m_perBot84;
    /*  96 */ std::list<world::IPlayer*> m_playerQueue;   // addNewPlayer / popPlayer (ELF 0x39F920 / 0x39F970). Name [INFERRED].
    /* 100 */ int           m_dword100;                 // = 0
    /* 104 */ bool          m_initialised;              // Set by init; read by isInitialised.
    // Gates strategic-AI updates; setSAIEnable controls it and init attempts to enable it.
    /* 105 */ bool          m_saiEnabled;
    /* 108 */ int           m_dword108;                 // = 0
    /* 112 */ int           m_dword112;                 // = 0
    /* 116 */ char          m_gap116[4];                // [UNRESOLVED] - the ctor never writes +116
    /* 120 */ float         m_float120;                 // Initially 100.0f; this field is a float.
    /* 124 */ char          m_gap124[4];                // [UNRESOLVED] - never written
    /* 128 */ int           m_renderSide;               // = 1
    /* 132 */ int           m_dword132;                 // = 0
    /* 136 */ bool          m_showBotUrgencies;         // = false
    /* 137 */ bool          m_showStrategies;           // = true
    /* 138 */ bool          m_showSAIStats;              // = false
    /* 139 */ bool          m_showBotStats;              // = true
    /* 140 */ bool          m_showStrategicAreas;        // = true
    /* 141 */ bool          m_showBotSpawnCoord;         // = false
    /* 144 */ int           m_dword144;                 // = 0
    /* 148 */ bool          m_flag148;                  // = false
    // One-based SAI array; slot zero remains null. Callers must supply a valid side.
    // The action cursor wraps modulo the side count, with no zero-count guard.
    /* 152 */ SAI*          m_sai[3];                   // 1-based; [0] is never used
    /* 164 */ unsigned int  m_componentCursor;          // [CHOSEN] - the round-robin cursor
    /* 168 */ int           m_saiMapXDimension;          // = 64
    /* 172 */ int           m_saiMapYDimension;          // = 64
    /* 176 */ double        m_double176;                // = -3.4028235e38 that is -FLT_MAX stored
    /* 184 */ double        m_double184;                // = -3.4028235e38 in a double. A "no value
                                                        // yet" sentinel written with the float
                                                        // constant. [UNRESOLVED] what they time.
    /* 192 */ BotManager*   m_botManager;               // Owned manager; action passes it the remaining time budget.
    /* 196 */ bool          m_validateDataStructuresEnabled; // = false
    /* 200 */ std::list<SoundEffect> m_soundEffects;    // allocate(28) -> 8 + 20; see SoundEffect
    // [UNRESOLVED] Map key and value types; current declarations preserve the observed storage shape.
    /* 204 */ std::map<int, void*> m_map204;            // +204 node, +208 count, +212 comparator
    /* 216 */ std::list<Bot*> m_pendingBots;            // Pending admissions processed by action.
    // Current bot and human population counts.
    /* 220 */ int           m_botCount;                 // = 0
    /* 224 */ int           m_humanCount;               // = 0 - the highest field the ctor writes
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIMAIN_H
