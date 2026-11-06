// AI runtime lifecycle, population management and simulation updates.

// static init: ELF 0x39F580 (__tcf_0 0x39F5E0, _GLOBAL__I 0x39F610): <iostream>'s Init object only.
#include <iostream>

#include "AIMain.h"

#include <algorithm>   // std::min - spawnBots' bot budget
#include <cmath>
#include <string>

#include "../AIUtils/checkPointer.h"                       // the ctor's dead null check
#include "../AIUtils/CustomCurve.h"                 // SCurve / JCurve / DecleiningSlopeCurve
#include "../../../AIInterface/Module/BFConsole.h"         // IBFModuleCourier::aiMainInit
#include "BotManager.h"                                           // getNumberOfActiveBots
#include "CommonSingletonServer.h"                                 // m_iaiGame / Settings / Effector
#include "AISettingsInterfaces.h"                                  // IAISettings
#include "../../../AIInterface/BFKnowledge/BFGame.h"              // IAIGame
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"      // IAIGameEffector::addBot
#include "IAIPathfinding.h"                                        // IAIPathfindingSetup::instance
#include "../AIPathFinding/AerialHeightMap/AerialHeightMap.h"                     // readHeightMapFromFile
#include "../AIKnowledge/BFInformationEnvironment.h"                // built and dropped by init
#include "../AIUtils/DebugRendering/AIRenderUtil.h"                // built and dropped by init
#include "../AIStrategic/SAI.h"                                   // ISAI::create, one per side
#include "../AIStrategicEntity/AIStrategicObjectManager.h"        // setSAIEnable's refusal test
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"           // IAITerrain, the world bounds
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"            // IAIPlayer - newPlayerCreated
#include "../AIBot/BotMain.h"                                      // Bot - switchSides
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"  // isValidPlayerId
#include "../AIStrategicEntity/UserOrders/UserOrderManager.h"                                     // recruitBot's first refusal
#include <utility>                                                // the per-bot pair series
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"     // getPlayerFromId, the id sweep
#include "../../../AIInterface/BFKnowledge/BFTimer.h"             // action's three timer methods
#include "../../../AIInterface/BFKnowledge/BFCollisionHandler.h"  // updateBotProjectiles
#include "../AIBot/SquadManager.h"                                // update(double)
#include "../AIKnowledge/AIMineManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"         // updateAIObject, vehicle changes
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"   // IAIObjectUnit
#include "CommunicationHub.h"                                       // resetLevel                                       // IAIMineManager::update

namespace dice {
namespace hfe {
namespace ai {

// IAIMain's constructor, destructor and m_instance live in IAIMain.cpp (AIDLL_r __FILE__).

// Returns whether initialization has completed. [INFERRED] State-field name.
// ELF 0x39F9F0
bool AIMain::isInitialised()
{
    return m_initialised;
}

// These eight methods intentionally do nothing in the dedicated-server image.

// The AI's debug rendering entry point - inert on a dedicated server build, which is the
// only build this pair comes from. So none of the ~20 show*/render* console switches this class
// exposes can produce any output here; they set flags that only a client-side AIMain would read.
// ELF 0x39DD60
void AIMain::draw()
{
}

// The sound-event hook is empty, so it never fills the sound queue.
// [UNRESOLVED] SoundEffect members cannot be established from this path.
// ELF 0x39EF60
void AIMain::queueSoundEffect(const Vec3& /*pos*/, int /*a*/, int /*b*/)
{
}

// These five commander notifications intentionally do nothing in AIMain.
// ELF 0x39F530
void AIMain::commanderOrderAccepted(int /*a*/)
{
}

// ELF 0x39F540
void AIMain::commanderOrderRejected(int /*a*/)
{
}

// ELF 0x39F550
void AIMain::appliedForCommander(int /*a*/)
{
}

// ELF 0x39F560
void AIMain::acceptedAsCommander(int /*a*/)
{
}

// ELF 0x39F570
void AIMain::resignedAsCommander(int /*a*/)
{
}

// Creates and activates bots up to the configured population budget when a round starts.
// ELF 0x39F3C0
void AIMain::spawnBots()
{
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    if (!game->isPlaying())
        return;

    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (!settings->getAutoSpawnBots())
        return;

    int wanted = settings->getMaxNBots();
    if (settings->getmaxBotsIncludeHumans())
        wanted = std::min(wanted, game->getMaxNumberOfPlayers()) - m_humanCount;

    IAIGameEffector* const effector =
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector);

    for (int missing = wanted - m_botManager->getNumberOfActiveBots(); missing > 0; --missing)
    {
        int a;
        int b;
        std::string name;

        effector->addBot(a, b, name);
        activateBot(a, b, name);
    }
}

// Constructs the AI runtime and its bot manager.
// ELF 0x39C970 / 0x39CDD0
AIMain::AIMain()
    // Keep BotManager allocation in the initializer list to preserve its position between
    // the surrounding container constructions.
    : m_aiSystemQuotient(0.15f)     // the float constant, widened - see AIMain.h
    , m_botManagerQuotient(0.875)
    , m_componentQuotient(0.125)
    , m_double28(0.0)
    , m_dword36(0)
    , m_botStatisticUpdateSpeed(3.0)
    , m_dword100(0)
    , m_initialised(false)
    , m_saiEnabled(false)
    , m_dword108(0)
    , m_dword112(0)
    , m_float120(100.0f)
    , m_renderSide(1)
    , m_dword132(0)
    , m_showBotUrgencies(false)
    , m_showStrategies(true)
    , m_showSAIStats(false)
    , m_showBotStats(true)
    , m_showStrategicAreas(true)
    , m_showBotSpawnCoord(false)
    , m_dword144(0)
    , m_flag148(false)
    , m_componentCursor(0)
    , m_saiMapXDimension(64)
    , m_saiMapYDimension(64)
    // FLT_MAX in A double, as the float constant and widened - that is what the binary
    // stores and the `f` suffix is what reproduces it exactly. A "no value yet" sentinel.
    , m_double176(-3.4028235e38f)
    , m_double184(-3.4028235e38f)
    , m_botManager(checkPointer(new BotManager))
    , m_validateDataStructuresEnabled(false)
    , m_botCount(0)
    , m_humanCount(0)
{
    // The four vector members initialize their own storage.

    // Initializes all three shared curve tables for each AIMain construction.
    SCurve::init();
    JCurve::init();
    DecleiningSlopeCurve::init();

    // `aiMainInit` is `IBFModuleCourier` slot +0x10, and the name says what this line is: the
    // AI telling the engine that its top-level object now exists. It is the only outward call the
    // constructor makes.
    static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)->aiMainInit();

    // Clear SAI pointers only after curve initialization and the courier callback.
    // createSAI later fills side slots 1 and 2; slot 0 remains unused.
    m_sai[0] = 0;
    m_sai[1] = 0;
    m_sai[2] = 0;
}

// ELF 0x39D390 (D1, 538 B; recovered on the client track, docs/macos-c-singleplayer.md). The
// per-side SAIs 1..getNSides() go through their virtual deleting destructor (+4) and are zeroed
// - the side count is asked again on every pass (IAISettings +0x40) - then the bot manager the
// same way. The containers and the IAIMain base follow as members.
AIMain::~AIMain()
{
    for (int side = 1; side < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1; ++side)
    {
        if (m_sai[side])
        {
            delete m_sai[side];
            m_sai[side] = 0;
        }
    }
    if (m_botManager)
        delete m_botManager;
}

// ELF 0x39F920 (weak): list insert at end() - the iterator is the list header itself.
void AIMain::addNewPlayer(world::IPlayer* player)
{
    m_playerQueue.push_back(player);
}

// ELF 0x39F970 (weak): counts the list (SGI size()), answers null when empty, else erases the
// first node and returns its player.
world::IPlayer* AIMain::popPlayer()
{
    if (m_playerQueue.size() == 0)
        return 0;
    world::IPlayer* const player = m_playerQueue.front();
    m_playerQueue.erase(m_playerQueue.begin());
    return player;
}

// ELF 0x39E1B0: list insert at end() of the pending list action() hands to the SAIs.
void AIMain::registerFreeBot(Bot* bot)
{
    m_pendingBots.push_back(bot);
}

// Activates the bot associated with a player ID.
// ELF 0x39E3B0
bool AIMain::activateBot(int playerId, int side, const std::string& name)
{
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);

    if (!environment->isValidPlayerId(playerId))
        return false;

    if (m_botManager->reactivateBot(playerId, side, name))
        return true;

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    return addUnspawnedBot(playerId, side, settings->getStandardPersonality(side));
}

// ELF 0x39DDA0
bool AIMain::addBot(int playerId, int side, float* personality, IAIObject* object)
{
    if (!m_botManager->addBot(playerId, side, personality, object))
        return false;
    const int index = m_botManager->getBotCount() - 1;
    Bot* const bot = m_botManager->getBot(static_cast<unsigned int>(index));
    if (!UserOrderManager::m_instance->recruitBot(bot, side))
    {
        if (m_sai[side])
            m_sai[side]->addBot(bot);
    }
    m_perBot48.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot60.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot72.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot84.push_back(0.0f);
    return true;
}

// Adds an unspawned bot to either player-directed orders or its side's strategic AI.
// ELF 0x39DFA0
bool AIMain::addUnspawnedBot(int playerId, int side, float* personality)
{
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    if (m_botManager->getBotCount() >= settings->getMaxNBots())
        return false;

    m_botManager->addUnspawnedBot(playerId, side, personality);

    // the pool'S length is the hand-off. See the block above.
    Bot* const bot = m_botManager->getBot(
                         static_cast<unsigned int>(m_botManager->getBotCount() - 1));

    if (!UserOrderManager::m_instance->recruitBot(bot, side))
    {
        if (m_sai[side])
            m_sai[side]->addBot(bot);
    }

    m_perBot48.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot60.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot72.push_back(std::pair<float, float>(0.0f, 0.0f));
    m_perBot84.push_back(0.0f);

    return true;
}

// ELF 0x39E210: the player leaves its squad, a bot of the side is deactivated (-1: the
// player id becomes -1, false), the side's SAI (when set) drops it (SAI +0x1C) and the player
// id becomes the bot's.
bool AIMain::deactivateBot(int& playerId, int side)
{
    SquadManager::m_instance->removeSquadMember(playerId, side);
    const int botId = m_botManager->deactivateBot(side);
    if (botId == -1)
    {
        playerId = -1;
        return false;
    }
    Bot* const bot = m_botManager->getBotFromId(static_cast<unsigned int>(botId));
    if (m_sai[side])
        m_sai[side]->removeBot(bot);
    playerId = bot->getPlayerId();
    return true;
}

// ELF 0x39E2E0: the player's squad member is destroyed, the dead bot of this player is
// deactivated (-1: false) and its side's SAI drops it.
bool AIMain::deactivateBot(int playerId)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(playerId);
    SquadManager::m_instance->destroySquadMember(playerId, player->getSide());
    const int botId = m_botManager->deactivateBotFromPlayerId(playerId);
    if (botId == -1)
        return false;
    Bot* const bot = m_botManager->getBotFromId(static_cast<unsigned int>(botId));
    if (m_sai[bot->getSide()])
        m_sai[bot->getSide()]->removeBot(bot);
    return true;
}

// Updates population counts when a player is created.
// A human joining may remove a bot when humans count toward the configured population limit.
// ELF 0x39E7B0
void AIMain::newPlayerCreated(bool isAI)
{
    if (isAI)
        ++m_botCount;
    else
        ++m_humanCount;

    if (!isAI)
    {
        const IAISettings* const settings =
            static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

        if (settings->getmaxBotsIncludeHumans())
        {
            if (m_botCount > 0)
            {
                IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);

                if (m_humanCount + m_botCount > game->getMaxNumberOfPlayers())
                {
                    IAIPlayerFactory* const factory =
                        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);

                    int playersOnSide[2] = { 0, 0 };

                    // Preserve the distinct missing-candidate sentinels for each side.
                    int candidate[2];
                    candidate[0] = -1;
                    candidate[1] = -2;

                    for (int id = 0; id <= 255; ++id)
                    {
                        IAIPlayer* const player = factory->getPlayerFromId(id);

                        if (player)
                        {
                            const int side = player->getSide();

                            if (static_cast<unsigned int>(side - 1) <= 1)
                            {
                                ++playersOnSide[side - 1];

                                // Keep the last AI player found on each side.
                                if (player->getIsAIPlayer())
                                    candidate[side - 1] = player->getId();
                            }
                        }
                    }

                    int pick = 2;

                    if (playersOnSide[0] > playersOnSide[1])
                        pick = 1;

                    // Fall back to the other side when this side has no candidate.
                    if (candidate[pick - 1] < 0)
                        pick = (pick == 1) + 1;

                    // A missing candidate remains negative; preserve the absent second guard.
                    const int victimId = candidate[pick - 1];

                    IAIGameEffector* const effector =
                        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector);

                    effector->terminatePlayer(factory->getPlayerFromId(victimId));
                    deactivateBot(victimId);
                    effector->terminatePlayer(victimId);

                    --m_botCount;
                }
            }
        }
    }
}

// ELF 0x39E460: a leaving human. The human count drops, the player's squad member is
// destroyed, and when humans count toward the limit and the population is below the game's
// maximum, spawnBots refills it (IAIMain +0x54).
void AIMain::reactivateBot(int playerId)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(playerId);
    if (!player)
        return;
    --m_humanCount;
    SquadManager::m_instance->destroySquadMember(playerId, player->getSide());
    if (!static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getmaxBotsIncludeHumans())
        return;
    if (m_botCount + m_humanCount < static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getMaxNumberOfPlayers())
        spawnBots();
}

// ELF 0x39E510
void AIMain::switchSides(int side)
{
    IAIPlayer* selected = 0;
    const std::vector<IAIPlayer*> players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
    for (std::vector<IAIPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        IAIPlayer* const player = *it;
        if (!player || !player->getIsAIPlayer() || player->getSide() != side)
            continue;
        if (selected)
        {
            const int score = player->getScore();
            const int selectedScore = selected->getScore();
            if (score >= selectedScore)
                continue;
        }
        selected = player;
    }
    if (selected)
    {
        selected->setSide((side == 1) + 1);
        const int playerId = selected->getId();
        Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
        getSAI(side)->removeBot(bot);
        getSAI((side == 1) + 1)->addBot(bot);
    }
}

// Moves a bot between sides and updates both strategic AI owners.
// ELF 0x39E6E0
void AIMain::switchSides(int side, int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);

    if (bot)
    {
        if (bot->getSide())
        {
            // Re-read the side after the first virtual call.
            if (bot->getSide() != side)
            {
                if (side)
                {
                    // Remove from the opposite side before updating the bot.
                    getSAI((side == 1) + 1)->removeBot(bot);

                    bot->setSide(side);

                    getSAI(side)->addBot(bot);
                }
            }
        }
    }
}

// Recovered caller: . This retires the entry stop, not the
// scheduled callees' individual frontiers (RW-102); no completed tick is implied by linking it.
// ELF 0x39DB90
bool AIMain::action()
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double budget = timer->getTimeIncrease() * m_aiSystemQuotient;
    const double deadline = timer->getUpdateTime() + budget;

    if (!m_pendingBots.empty())
    {
        for (std::list<Bot*>::iterator it = m_pendingBots.begin(); it != m_pendingBots.end(); ++it)
        {
            const int side = (*it)->getSide();
            if (m_sai[side])
                m_sai[side]->addBot(*it);
        }
        m_pendingBots.clear();
    }

    if (m_saiEnabled)
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE multiplies by the component share before 0.95, then reads the
        // cursor after getNSides. ELF reverses the factors and captures cursor + 1 first.
        m_sai[m_componentCursor + 1]->update(budget * m_componentQuotient * 0.95);
        const unsigned int sides = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
            ->getNSides();
        m_componentCursor = (m_componentCursor + 1) % sides;
#else
        m_sai[m_componentCursor + 1]->update(budget * 0.95 * m_componentQuotient);
        const unsigned int next = m_componentCursor + 1;
        const unsigned int sides = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
            ->getNSides();
        m_componentCursor = next % sides;
#endif
    }

#if defined(_MSC_VER)
    // [PLATFORM] PE retains the squad singleton before getGameTime; ELF loads it afterward.
    SquadManager* const squads = SquadManager::m_instance;
    const double gameTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
#else
    const double gameTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    SquadManager* const squads = SquadManager::m_instance;
#endif
    squads->update(gameTime);
    static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler)
        ->updateBotProjectiles();

    const double remaining = deadline - timer->getUpdateTime();
    m_botManager->action(remaining);
    static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager)->update();
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->damageSeverelyDamagedVehicles();
#if defined(_MSC_VER)
    // PE also computes the final remainder; neither port consumes the value.
    (void)(deadline - timer->getUpdateTime());
#else
    timer->getUpdateTime();
#endif
    return true; // Explicit EAX=1 (ELF) / AL=1 (PE), after every scheduled call returns.
}

// ELF 0x39EB00: `movzbl 0x69(this)`.
bool AIMain::getSAIEnable()
{
    return m_saiEnabled;
}

// an unchecked side-indexed pointer read.
// ELF 0x39EA00
SAI* AIMain::getSAI(int side)
{
    return m_sai[side];
}

// ELF 0x39EF40: IAIObject +0x104 positionChanged.
void AIMain::updateAIObject(IAIObject* object)
{
    object->positionChanged();
}

// ELF 0x39EF70
bool AIMain::reset()
{
    if (m_initialised)
    {
        for (int side = 1; side < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1; ++side)
        {
            if (m_sai[side])
            {
                delete m_sai[side];
                m_sai[side] = 0;
            }
        }
        m_dword132 = 0;
        m_saiMapYDimension = 32;
        m_dword144 = 0;
        m_aiSystemQuotient = 0.2;
        m_renderSide = 1;
        m_dword100 = 0;
        m_initialised = false;
        m_botManagerQuotient = 0.75f;
        m_saiMapXDimension = 32;
        m_saiEnabled = false;
        m_showBotUrgencies = false;
        m_componentQuotient = 0.25f;
        m_validateDataStructuresEnabled = false;
        m_flag148 = false;
        m_dword36 = 0;
        m_double28 = 0.0f;
        m_botStatisticUpdateSpeed = 3.0f;
        IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
        const float width = static_cast<float>(terrain->getWorldSizeX());
        const float height = static_cast<float>(terrain->getWorldSizeZ());
        m_float120 = 100.0f;
        // [CHOSEN, not recovered] Native libm marks log const; this barrier retains all four shipped calls.
        double (*volatile logarithm)(double) = &std::log;
        (void)logarithm(static_cast<double>(width));
        (void)logarithm(2.0);
        (void)logarithm(static_cast<double>(height));
        (void)logarithm(2.0);
        IAIPathfinding::instance->reset();
        IBotManager::instance->reset();
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->reset();
        static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)->reset();
    }
    return true;
}

// ELF 0x39F140. The bridges are reset, then the strategic areas (AIStrategicObjectManager +0x04
// resetAreas); every side's SAI is reset only while all previous resets succeeded (the && chain
// short-circuits). Then the projectiles (IBFCollisionHandler +0x08), the destroyable map parts
// (IAIPathfinding +0x10), the bridges a second time, the communication hub and the mines
// (IAIMineManager +0x0C). Returns the combined strategic reset result.
bool AIMain::resetLevel()
{
    AIStrategicObjectManager::getInstance()->resetBridges();
    bool ok = AIStrategicObjectManager::getInstance()->resetAreas();
    for (int side = 1; side < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1; ++side)
    {
        if (m_sai[side])
            ok = ok && m_sai[side]->reset();
    }
    static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler)->removeAllProjectiles();
    IAIPathfinding::instance->resetDestroyableMapParts();
    AIStrategicObjectManager::getInstance()->resetBridges();
    CommunicationHub::m_instance->reset();
    static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager)->reset();
    return ok;
}

// ELF 0x39DAE0. The two x87 tests against 0.0f (ELF 0x49CB34) and 1.0f (0x49CB2C) are joined
// with "or": the value is stored unless it is below 0 and above 1 at once, i.e. always (a NaN
// too: its first comparison is unordered and takes the storing branch).
void AIMain::setAISystemQuotient(const double& q)
{
    if (!(q < 0.0f) || !(q > 1.0f))
        m_aiSystemQuotient = q;
}

// ELF 0x39FA00 (weak): `fldl 0x4(this)`.
double AIMain::getAISystemQuotient() const
{
    return m_aiSystemQuotient;
}

// ELF 0x39F230. When a human (IAIPlayer +0x88 getIsAIPlayer false) leaves a unit, the unit
// records the game time as its last possession (IAIObjectUnit +0x64; the unit plug-in is
// fetched before the time) and its primary object is allowed to bots again (+0x9C).
void AIMain::setPlayerExitedVehicle(IAIObject* vehicle, int playerId)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(playerId);
    if (vehicle && vehicle->isUnit() && player && !player->getIsAIPlayer())
    {
        IAIObjectUnit* const unit = vehicle->getUnit();
        unit->setLastPlayerPossesionTime(
            static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
        vehicle->getPrimaryObject()->setIsAllowedByBots(true);
    }
}

// ELF 0x39F2F0. A human moving between two units with different primary objects leaves the
// first unit's last possession at -1000.0f (ELF 0x49CDC8). The second object is tested first.
void AIMain::setPlayerEnteredVehicle(IAIObject* from, IAIObject* to, int playerId)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(playerId);
    if (to && to->isUnit() && from && from->isUnit() && player && !player->getIsAIPlayer())
    {
        IAIObject* const toPrimary = to->getPrimaryObject();
        IAIObject* const fromPrimary = from->getPrimaryObject();
        if (toPrimary && fromPrimary && toPrimary != fromPrimary)
            from->getUnit()->setLastPlayerPossesionTime(-1000.0f);
    }
}

// Initializes the AI synchronously from the level script and returns a status message.
// The integer argument is unused.
// ELF 0x39D7E0
std::string AIMain::init(int)
{
    // built first, before any work. The message is constructed at the top of the body and
    // returned unchanged - there is no failure path that could alter it.
    std::string result("AIMain initiated.");

    m_initialised = true;

    // two self-installing singletons - see the block in AIMain.h. Both pointers are dropped.
    checkPointer(new BFInformationEnvironment());
    checkPointer(new AIRenderUtil());

    AISingletonServer::updateStaticMethods();

    // the two file reads. See the block above.
    AerialHeightMap::m_instance->readHeightMapFromFile();

    createSAI();
    setSAIEnable(true);

    IAIPathfindingSetup::instance->init();
    IAIPathfindingSetup::instance->loadQuadTrees();

    // the defaulting rule, as A fixup. See the block above.
    const float recomended =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getRecomendedViewDistance();

    if (static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getViewDistance() <= 0.0f)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->setViewDistance(recomended);

    return result;
}

// Creates one strategic AI instance for each side.
// ELF 0x39EA20
void AIMain::createSAI()
{
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const IAITerrain* const terrain =
        static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);

    for (int side = 1; side < settings->getNSides() + 1; ++side)
    {
        // `Vec2` is a plain aggregate here, with no two-argument constructor.
        Vec2 min;
        min.x = 0.0f;
        min.y = 0.0f;

        Vec2 max;
        max.x = static_cast<float>(terrain->getWorldSizeX());
        max.y = static_cast<float>(terrain->getWorldSizeZ());

        m_sai[side] = ISAI::create(side, m_saiMapXDimension, m_saiMapYDimension, min, max);
    }
}

// Sets the requested state and returns the value accepted, or false when refused.
// ELF 0x39EB10
bool AIMain::setSAIEnable(bool enable)
{
    if (AIStrategicObjectManager::getInstance()->getNumberOfStrategicObjects())
    {
        m_saiEnabled = enable;
        return enable;
    }

    Debug(DebugTypeError, "AI/AIDLL/AI/Main/AIMain.cpp", 910, "")
        << "There are no strategic areas defined! SAI will not be enabled. Game may be unstable.";

    m_saiEnabled = false;
    return false;
}

// ELF 0x39DB30
void AIMain::setAISystemComponentsQuotient(unsigned int botManager, unsigned int strategic)
{
    const unsigned int sum = botManager + strategic;
    if (sum)
    {
        m_botManagerQuotient = static_cast<double>(botManager) / sum;
        m_componentQuotient = static_cast<double>(strategic) / sum;
    }
}

// ELF 0x39EC60
void AIMain::setSAIMapXDimension(int n)
{
    m_saiMapXDimension = n;
    // Shipped defect: this tests the array address, not whether any SAI exists.
    if (m_sai)
        for (int side = 1; side < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1; ++side)
            if (m_sai[side])
                m_sai[side]->setMapXDimension(n);
}

// ELF 0x39ECE0
void AIMain::setSAIMapYDimension(int n)
{
    m_saiMapYDimension = n;
    // Shipped defect: this tests the array address, not whether any SAI exists.
    if (m_sai)
        for (int side = 1; side < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNSides() + 1; ++side)
            if (m_sai[side])
                m_sai[side]->setMapYDimension(n);
}

// ELF 0x39ED60
int AIMain::getSAIMapXDimension() const { return m_saiMapXDimension; }

// ELF 0x39ED70
int AIMain::getSAIMapYDimension() const { return m_saiMapYDimension; }

// ELF 0x39DD70
int AIMain::getRenderSide() const { return m_renderSide; }

// ELF 0x39DD80
void AIMain::setRenderSide(int side) { m_renderSide = side; }

// ELF 0x39ED80
bool AIMain::getValidateDataStructuresEnabled() const { return m_validateDataStructuresEnabled; }

// ELF 0x39ED90
void AIMain::setValidateDataStructuresEnabled(bool b) { m_validateDataStructuresEnabled = b; }

// ELF 0x39EDD0
bool AIMain::getShowBotUrgencies() const { return m_showBotUrgencies; }

// ELF 0x39EDB0
void AIMain::setShowBotUrgencies(bool b) { m_showBotUrgencies = b; }

// ELF 0x39EE00
bool AIMain::getShowBotSpawnCoord() const { return m_showBotSpawnCoord; }

// ELF 0x39EDE0
void AIMain::setShowBotSpawnCoord(bool b) { m_showBotSpawnCoord = b; }

// ELF 0x39EE30
bool AIMain::getShowStrategies() const { return m_showStrategies; }

// ELF 0x39EE10
void AIMain::setShowStrategies(bool b) { m_showStrategies = b; }

// ELF 0x39EE60
bool AIMain::getShowSAIStats() const { return m_showSAIStats; }

// ELF 0x39EE40
void AIMain::setShowSAIStats(bool b) { m_showSAIStats = b; }

// ELF 0x39EE90
bool AIMain::getShowBotStats() const { return m_showBotStats; }

// ELF 0x39EE70
void AIMain::setShowBotStats(bool b) { m_showBotStats = b; }

// ELF 0x39EEC0
bool AIMain::getShowStrategicAreas() const { return m_showStrategicAreas; }

// ELF 0x39EEA0
void AIMain::setShowStrategicAreas(bool b) { m_showStrategicAreas = b; }

// ELF 0x39EED0
void AIMain::showAll(bool b)
{
    m_showBotUrgencies = b;
    m_showStrategies = b;
    m_showSAIStats = b;
    m_showBotStats = b;
    m_showStrategicAreas = b;
}

// ELF 0x39EF00
void AIMain::showStrategic(bool b)
{
    m_showStrategies = b;
    m_showSAIStats = b;
    m_showStrategicAreas = b;
}

// ELF 0x39EF20
void AIMain::showBot(bool b)
{
    m_showBotUrgencies = b;
    m_showBotStats = b;
}

} // namespace ai
} // namespace hfe
} // namespace dice
