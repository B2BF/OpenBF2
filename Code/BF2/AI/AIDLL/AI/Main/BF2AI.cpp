// AI subsystem assembly and lifecycle. Constructors register their own singleton interfaces.
// Refresh cached pointers after construction and during teardown before using them again.
// These statics belong to the AI module; do not share their definitions with the engine image.
// [INFERRED] Source placement.

// static init: ELF 0x3EE360 (__tcf_0 0x3EE3C0, _GLOBAL__I 0x3EE3F0): <iostream>'s Init object only.
#include <iostream>

#include "BF2AI.h"

#include <string>

#include "../../../../Core/ClassManager.h"
#include "../../../AIInterface/Module/BFConsole.h"   // IBFModuleCourier - both wiring passes
#include "../AIKnowledge/BFInformationCourier.h"     // step 4
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/InformationFactory.h"       // step 8
#include "InformationManager.h"       // step 1, and clear's table teardown
#include "../AIUtils/WeaponInfoManager.h"        // step 2
#include "../AIStrategicMap/BFStrategicEnvironment.h"  // step 6
#include "../DrawPrimitives/DPManager.h"             // step 7
#include "../AIUtils/checkPointer.h"
#include "AICollisionHandler.h"                      // step 10
#include "AIEventCourier.h"                          // step 11
#include "AIMain.h"
#include "../AIKnowledge/AIMineManager.h"                           // step 13
#include "AIPlayerCourier.h"                         // step 12
#include "AIConsole.h"                               // step 15
#include "../AIUtils/DebugRendering/AIRenderUtil.h"   // clear: the debug renderer
#include "AILandingZoneManager.h" // clear: the landing-zone manager
#include "BotBehaviourManager.h"                   // step 16
#include "../AIStrategicEntity/AIStrategicObjectManager.h"   // step 17
#include "../AIStrategicEntity/CoverPosition/CoverPositionManager.h"  // step 18
#include "BotPlacementManager.h"                     // clear only - the static initializer builds it
#include "AILODManager.h"                            // clear only - `init` never builds one
#include "AIPathfinding.h"                          // step 21, and clear
#include "AIRandom.h"                               // step 22, and clear
#include "AISettings.h"                              // step 9
#include "BFPathfindingCourier.h"                    // step 5
#include "../AIStrategicEntity/UserOrders/UserOrderManager.h"                        // step 14
#include "StrategyManager.h"                         // step 19, and clear
#include "../AIPathFinding/AerialHeightMap/AerialHeightMap.h"       // step 20, and clear
#include "CommunicationHub.h"                        // step 23, and clear
#include "AISettingsInterfaces.h"   // IAISettings / IAISettingsModifier
#include "../../../AIInterface/BFKnowledge/BFGame.h"   // IAIGame::getAISettingsFromGame
#include "CommonSingletonServer.h"                   // + AISingletonServerUpdater, step 3

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// Registers the first AI in the simple-singleton registry. Later instances remain
// unregistered; this intentionally differs from AIModule::close's normal-registry lookup.
// ELF 0x3EE430 / 0x3EE540
IAI::IAI()
{
    if (!classManager->getSimpleSingleton("AI"))
        classManager->registerSimpleSingleton("AI", this);
}

// The exact mirror of the constructor: unregister only if the registered "AI" is still this object.
// A second IAI that never registered will not tear down the first one's entry on its way out.
// ELF 0x3EE650 / 0x3EE740 / 0x3EE830
IAI::~IAI()
{
    if (classManager->getSimpleSingleton("AI") == this)
        classManager->unregisterSimpleSingleton("AI");
}

// Two bytes. Everything that matters happened in the base constructor above.
// ELF 0x3EC720 / 0x3EC770
BF2AI::BF2AI()
    : m_initialized(false)
    , m_shuttingDown(false)
{
}

// Trivial - the vptr reset and the base
// call are the whole body. It does not call clear; tearing the AI down is shutdown's job and
// nothing chains the two. Destroying a BF2AI without shutting it down first leaks every one of the
// twenty-four subsystems init built.
// ELF 0x3EC7C0 / 0x3EC800 / 0x3EC840
BF2AI::~BF2AI()
{
}

// Constructs the AI subsystems and refreshes their registered interfaces.
// ELF 0x3EC890
bool BF2AI::init()
{
    // Raises the shutdown flag before checking initialization. A repeated init returns
    // false without clearing that flag.
    const bool wasUninitialized = !m_initialized;
    m_shuttingDown = true;
    if (!wasUninitialized)
        return false;

    // Constructs and checks the information manager before publishing it.
    // The manager's internal pool allocation is unchecked.
    InformationManager* information = new InformationManager();
    checkPointer(information);
    informationManager = information;

    // WeaponInfoManager publishes itself; no local ownership pointer is retained.
    WeaponInfoManager* weaponInfo = new WeaponInfoManager();
    checkPointer(weaponInfo);
    (void)weaponInfo;

    // defect 2, reproduced. Twenty-one subsystems and both wiring passes are still ahead, and
    // the AI already reports itself initialized. The engine gates its AI tick on this byte.
    m_initialized = true;

    // Registers the updater that lets the engine refresh DLL singleton pointers.
    AISingletonServerUpdater* updater = new AISingletonServerUpdater();
    checkPointer(updater);
    (void)updater;

    // wiring pass 1
    // Snapshot every registered singleton into the CommonSingletonServer statics. At this point
    // that is the engine's side of the roster plus the three objects above; the twenty remaining
    // subsystems are published by pass 2 at the end of the function.
    AISingletonServer::updateStaticMethods();

    // Requires the engine module courier to be registered before this wiring pass.
    static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)->initSingletons();


    // Construct these self-registering subsystems in order. The following roster refresh
    // makes their registered interfaces available; local pointers are not retained.

    // step 4: the information courier
    BFInformationCourier* informationCourier = new BFInformationCourier();
    checkPointer(informationCourier);
    (void)informationCourier;

    // step 5: the pathfinding courier
    BFPathfindingCourier* pathfindingCourier = new BFPathfindingCourier();
    checkPointer(pathfindingCourier);
    (void)pathfindingCourier;

    // The strategic-environment interface guards registration with the DLL-side roster.
    BFStrategicEnvironment* strategicEnvironment = new BFStrategicEnvironment();
    checkPointer(strategicEnvironment);
    (void)strategicEnvironment;

    // DPManager registers four interfaces separately. Secondary-interface calls require
    // their base-pointer adjustments.
    DPManager* dpManager = new DPManager();
    checkPointer(dpManager);
    (void)dpManager;

    // Constructs the self-registering information factory without checkPointer.
    InformationFactory* informationFactory = new InformationFactory();
    (void)informationFactory;

    // Constructs settings with both the IAISettings and IAISettingsModifier interfaces.
    AISettings* settings = new AISettings();
    checkPointer(settings);
    (void)settings;

    // step 10: the AI-side collision handler
    // (!) not the engine's `IBFCollisionHandler`. Two different names, two different statics, one
    // subsystem; AI/aidll/AI/Main/CommonSingletonServer.h keeps them apart.
    AICollisionHandler* collisionHandler = new AICollisionHandler();
    checkPointer(collisionHandler);
    (void)collisionHandler;

    // Event-courier registration uses the registry directly; teardown resolves it by name.
    AIEventCourier* eventCourier = new AIEventCourier();
    checkPointer(eventCourier);
    (void)eventCourier;

    // step 12: the player courier
    // (!) Registry-guarded as well, and likewise destroyed by name in clear.
    AIPlayerCourier* aiPlayerCourier = new AIPlayerCourier();
    checkPointer(aiPlayerCourier);
    (void)aiPlayerCourier;

    // step 13: the mine manager
    // (*) Its build two
    // nested vectors and two maps, and size the outer dimension of each index to the two mine
    // types. Both nested records are recovered: `PolygonData` is eight bytes (two per-polygon
    // counters) and `MineInfo` is 112, proven by the 132-byte map node. See AIMineManager.h.
    AIMineManager* mineManager = new AIMineManager();
    checkPointer(mineManager);
    (void)mineManager;


    // UserOrderManager publishes only its own singleton pointer, with no ClassManager entry.
    UserOrderManager* userOrderManager = new UserOrderManager();
    checkPointer(userOrderManager);
    (void)userOrderManager;

    // Construct the Bot Action Plan primitive registry.
    AIConsole* aiConsole = new AIConsole();
    checkPointer(aiConsole);
    (void)aiConsole;

    // Registers the bot-behavior factories and publishes their manager singleton.
    BotBehaviourManager* botBehaviourManager = new BotBehaviourManager();
    checkPointer(botBehaviourManager);
    (void)botBehaviourManager;

    // Creates the first strategic-object manager; teardown retrieves it through getInstance.
    AIStrategicObjectManager* strategicObjectManager = new AIStrategicObjectManager();
    checkPointer(strategicObjectManager);
    (void)strategicObjectManager;

    // Creates the cover-position manager and its preallocated search resources.
    CoverPositionManager* coverPositionManager = new CoverPositionManager();
    checkPointer(coverPositionManager);
    (void)coverPositionManager;

    // StrategyManager has virtual reset but a nonvirtual destructor; teardown uses its concrete type.
    StrategyManager* strategyManager = new StrategyManager();
    checkPointer(strategyManager);
    (void)strategyManager;

    // AerialHeightMap replaces its singleton on construction and clears it unconditionally on destruction.
    AerialHeightMap* aerialHeightMap = new AerialHeightMap();
    checkPointer(aerialHeightMap);
    (void)aerialHeightMap;

    // Creates the pathfinder with its engines, debug paths, and search points, publishing
    // both pathfinding interfaces.
    AIPathfinding* pathfinding = new AIPathfinding();
    checkPointer(pathfinding);
    (void)pathfinding;

    // Construct and seed the random generator.
    AIRandom* random = new AIRandom();
    checkPointer(random);
    random->init();

    // CommunicationHub publishes itself; its time fields remain uninitialized.
    CommunicationHub* communicationHub = new CommunicationHub();
    checkPointer(communicationHub);
    (void)communicationHub;


    // Refresh cached pointers after all subsystems have registered.
    AISingletonServer::updateStaticMethods();

    // Mark initialization complete.
    m_shuttingDown = false;
    return true;
}

// Tears down AI subsystems in dependency order, refreshing cached interfaces during destruction.
// ELF 0x3EDDF0
bool BF2AI::clear()
{
    if (!m_initialized)
        return true;

    // lowered first, and lowered again at the very end - two stores to the same byte with the
    // whole teardown between them. Reproduced.
    m_initialized = false;

    // The AI's own singleton-server updater, destroyed by name. Note it is registered as
    // "AISingletonServerUpdater" but AISingletonServer::updateStaticMethods kicks
    // "BFSingletonServerUpdater" - two different objects with confusingly similar names.
    ISingletonServerUpdater* updater = static_cast<ISingletonServerUpdater*>(
        classManager->getSimpleSingleton("AISingletonServerUpdater"));
    if (updater)
        delete updater;

    // ELF D1 call at 0x3EDEAC: the debug renderer, nonvirtual D1 + operator delete.
    // Recovered on the client track (docs/macos-c-singleplayer.md); init never builds it, so
    // this only runs once a debug command has made one.
    if (AIRenderUtil::m_instance)
        delete AIRenderUtil::m_instance;

    if (CommonSingletonServer::m_iaiInformationCourier)
        delete static_cast<IAIInformationCourier*>(CommonSingletonServer::m_iaiInformationCourier);

    if (CommonSingletonServer::m_iaiPathfindingCourier)
        delete static_cast<IAIPathfindingCourier*>(CommonSingletonServer::m_iaiPathfindingCourier);

    if (AISingletonServer::m_iaiStrategicEnvironment)
        delete static_cast<IAIStrategicEnvironment*>(AISingletonServer::m_iaiStrategicEnvironment);

    if (CommonSingletonServer::m_iDPManager)
        delete static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager);

    // Delete through the registered interface so each compiler selects its deleting destructor.
    if (CommonSingletonServer::m_iInformationFactory)
        delete static_cast<IInformationFactory*>(CommonSingletonServer::m_iInformationFactory);

    if (CommonSingletonServer::m_iAICollisionHandler)
        delete static_cast<IAICollisionHandler*>(CommonSingletonServer::m_iAICollisionHandler);

    if (CommonSingletonServer::m_iAIMineManager)
        delete static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager);

    // by name, not through a static - "IAIEventCourier" has no CommonSingletonServer member.
    IAIEventCourier* eventCourier = static_cast<IAIEventCourier*>(
        classManager->getSimpleSingleton("IAIEventCourier"));
    if (eventCourier)
        delete eventCourier;

    if (AISingletonServer::m_iaiInformationEnvironment)
        delete static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);

    // the MID-teardown refresh. Everything destroyed above has unregistered itself, so this
    // turns exactly those statics null and every `if (x)` below skips what is already gone.
    AISingletonServer::updateStaticMethods();

    // Destruction releases owned orders and clears the matching manager singleton.
    if (UserOrderManager::m_instance)
        delete UserOrderManager::m_instance;

    // ELF D1 call at 0x3EDFFB: the console (its destructor resets it and deletes the SquadManager).
    // Recovered on the client track (docs/macos-c-singleplayer.md).
    if (AIConsole::m_instance)
        delete AIConsole::m_instance;

    // Destruction clears the behavior-manager singleton, making repeated cleanup harmless.
    if (BotBehaviourManager::m_instance)
        delete BotBehaviourManager::m_instance;

    // Destroy strategic object and cover-position owners through their registered instances.
    delete AIStrategicObjectManager::getInstance();
    delete StrategyManager::getInstance();

    // Destruction clears the height-map singleton unconditionally, even for a displaced instance.
    if (AerialHeightMap::m_instance)
        delete AerialHeightMap::m_instance;

    IAIPlayerCourier* playerCourier = static_cast<IAIPlayerCourier*>(
        classManager->getSimpleSingleton("IAIPlayerCourier"));
    if (playerCourier)
        delete playerCourier;

    if (IAIMain::m_instance)
        delete IAIMain::m_instance;

    // Deletes through the first pathfinding base interface and its virtual destructor.
    // The interface singleton, rather than ClassManager lookup, owns this teardown route.
    if (IAIPathfinding::instance)
        delete IAIPathfinding::instance;

    // The 512 KB slot table and its 4-byte holder, freed in that order. raw `operator delete` on
    // both, with no destructor call on either - which is what establishes that `SpHandlePool` is
    // trivially destructible and that `InformationManager` owns nothing else. Written as the two
    // raw frees the binary makes rather than as `delete informationManager`, because a typed delete
    // would call the destructor the binary does not call. See Main/InformationManager.h.
    if (informationManager)
    {
        if (informationManager->getPool())
            operator delete(informationManager->getPool());
        operator delete(informationManager);
    }
    informationManager = 0;

    // Deletes the weapon-info pool and clears the matching manager singleton.
    if (WeaponInfoManager::m_instance)
        delete WeaponInfoManager::m_instance;

    // Settings is destroyed after the pools and before the engine-side singleton teardown.
    if (CommonSingletonServer::m_iaiSettings)
        delete static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);

    // Requires the module courier to remain valid after the roster refresh.
    // Typed dispatch preserves the destructor-slot difference between PE and ELF.
    static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)->clearSingletons();

    // Everything below this line uses its own static, not the registry - which is what makes it
    // safe to null the whole table here rather than at the end.
    AISingletonServer::clearAllStaticMethods();

    // writeable as of chunk 02 - AI/aidll/AI/Main/AIRandom.h is a real class. Its destructor is
    // not virtual and the binary's call is direct, so this `delete` is the same instruction.
    if (AIRandom::m_instance)
        delete AIRandom::m_instance;

    // Deletes the LOD manager when present.
    delete AILODManager::m_instance;

    // ELF D1 call at 0x3EE1A0 / PE read at 0x10001D5B: the landing-zone manager the static installer made.
    // Its destructor does not zero m_instance (ELF 0x399D60, PE 0x10039B00), so the pointer dangles
    // until the library is unloaded and its static initializers run again on the next load
    // (AIConnection::clear unloads it right after this). No platform reset is added: on macOS
    // dlclose does unmap libaidll.dylib (no thread-local or Objective-C sections, only libc++ and
    // libSystem as dependencies; a dlopen/dlclose/RTLD_NOLOAD probe finds it gone after each close),
    // so the next load re-runs the installer exactly as the retail DLL reload does.
    if (AILandingZoneManager::m_instance)
        delete AILandingZoneManager::m_instance;
    if (CommunicationHub::m_instance)
        delete CommunicationHub::m_instance;
    // ELF D1 call at 0x3EE1D0, the last delete: the placement manager the static initializer
    // (0x3E4BD5) made at load. Its destructor zeroes m_instance.
    if (BotPlacementManager::m_instance)
        delete BotPlacementManager::m_instance;

    m_initialized = false;
    return true;
}

// Invokes virtual clear so a derived implementation can override shutdown cleanup.
// ELF 0x3EE270
bool BF2AI::shutdown()
{
    clear();            // virtual, IAI vtable +0x0C
    return true;
}

// Advances the AI using its own timer. The supplied engine delta is ignored.
// ELF 0x3EE290
int BF2AI::update(double /*dt*/)
{
    return IAIMain::m_instance->action();       // IAIMain vtable +0x34
}

// Copies game settings into the AI configuration and initializes the runtime as needed.
// ELF 0x3EE2C0
int BF2AI::getSettingsFromMenu(bool /*fromMenu*/)
{
    IAISettings* const settings =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);

    // the content'S OPT-out. `aiSettings.overrideMenuSettings 1` in a mod's AIDefault.ai makes the
    // script's values win; the stock file has that line commented out, so the game's win.
    if (settings->overrideMenuSettings())
        return 1;

    IAIGame* const game =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    IAISettingsModifier* const modifier =
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier);

    // Three out-parameters in one call, and the middle one is a byte in the ELF widened at the
    // point it is handed on.
    int   a = 0;
    bool  b = false;
    float c = 0.0f;
    if (!game->getAISettingsFromGame(a, b, c))
        return 0;

    modifier->setMaxNBots(a);
    modifier->maxBotsIncludeHumans(b);
    modifier->setBotSkill(c);
    return 1;
}

// - a bare byte load. The engine's gate on every .ai load and on the AI tick.
// ELF 0x3EE410
bool BF2AI::isInitialized() const
{
    return m_initialized;
}

// - a bare byte load. Also reports true for the whole of init; see BF2AI.h.
// ELF 0x3EE420
bool BF2AI::isShuttingDown() const
{
    return m_shuttingDown;
}

} // namespace ai
} // namespace hfe
} // namespace dice
