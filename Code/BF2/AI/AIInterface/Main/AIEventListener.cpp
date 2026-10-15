
#include "AIEventListener.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../../../Core/EventManager.h"
#include "../../../Core/Debug.h"                     // the line-494 diagnostic below
#include "../../../Game/Objects/IPlayer.h"          // world::IPlayer::getId / getIsAIPlayer
#include "../SingletonServer.h"                     // BFSingletonServer::m_iaiEventCourier
#include "../BFKnowledge/BFGame.h"                  // IAIGame::setIsPrecaching / isPlaying
#include "../BFKnowledge/AIPlayerFactory.h"         // IAIPlayerFactory
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/Main/AIEventCourier.h"     // IAIEventCourier - 55 named slots
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"  // IAISettings::getDraw
#include "../../AIDLL/AI/Main/AIMineManager.h"      // IAIMineManager::mineRemoved
#include "../../../Game/GameServer/ServerSettings.h"  // the team-ratio inversion below
#include "../../../Scene/Object/PlayerManager.h"      // the three lookups it needs
#include "../../AIDLL/AI/DrawPrimitives/DPManager.h"// IDPManager::drawAll / nodrawUpdate

namespace dice {
namespace hfe {

extern EventManager* eventManager;

namespace ai {

// The vptr and two bytes; nothing else.
AIEventListener::AIEventListener()
    : m_initialised(true)
    , m_unknown05(false)
{
}

// Unregisters category-3 events 0..23, then handlers 2, 3, 1, 0, 5 in initialization
// order. Does not guard against an uninitialized listener.
AIEventListener::~AIEventListener()
{
    for (int i = 0; i <= 23; ++i)
        eventManager->unregisterEvent(static_cast<EventCategory>(3), i);

    eventManager->unregisterEventHandler(static_cast<EventCategory>(2), this);
    eventManager->unregisterEventHandler(static_cast<EventCategory>(3), this);
    eventManager->unregisterEventHandler(static_cast<EventCategory>(1), this);
    eventManager->unregisterEventHandler(static_cast<EventCategory>(0), this);
    eventManager->unregisterEventHandler(static_cast<EventCategory>(5), this);
}

// Marks the listener initialized and registers handlers 2, 3, 1, 0, 5 at priority zero
// even when it was already initialized.
void AIEventListener::init()
{
    m_initialised = true;

    eventManager->registerEventHandler(static_cast<EventCategory>(2), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(3), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(1), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(0), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(5), this, 0);

    registerEvents();
}

// Registers category-3 event ids 0..23 under the same literal name, "ECAIi".
// Constructs each name inside the loop and does not require a listener instance.
void AIEventListener::registerEvents()
{
    for (int i = 0; i <= 23; ++i)
        eventManager->registerEvent(static_cast<EventCategory>(3), i, std::string("ECAIi"));
}

// Routes engine events to the AI couriers. Unsupported event paths retain named frontier stops.
// [RETIRE-WHEN: aidll-01-adapters] Remove when the corresponding implementation is complete and validated.

namespace {

// [FRONTIER] [TEMP] [CHOSEN, not recovered] - one per unwritten arm, each naming its own
// (category, id) so the next person knows exactly which dispatch entry is wanted rather than only
// that `handleEvent` was reached. [RETIRE-WHEN: aidll-01-adapters]
void unwrittenArm(const char* what, int category, unsigned int id)
{
    std::fprintf(stderr,
                 "[FRONTIER] dice::hfe::ai::AIEventListener::handleEvent - %s" "\n"
                 "           category=%d id=%u" "\n",
                 what, category, id);
    std::fflush(stderr);
    std::abort();
}

// Category 3 id 0x16 (enemy spotted): SpottedEvent::executeServer's eight-byte stack block, an AI
// object pointer followed by the low byte of the team. The x86 layout is retail's (team at +4);
// the struct keeps the team after the full pointer on native x64.
struct SpottedPayload
{
    IAIObject*    aiObject;     // +0x00
    unsigned char team;         // +0x04
};
#if defined(_M_IX86)
static_assert(offsetof(SpottedPayload, team) == 4, "retail spotted-event team byte");
#endif

} // anonymous namespace

void AIEventListener::handleEvent(EventCategory category,
                                  unsigned int  id,
                                  EventNode*    /*node*/,
                                  void*         userData)
{
    IAIEventCourier* const courier = BFSingletonServer::m_iaiEventCourier;

    switch ((int)category)
    {
    // category 0
    case 0:
        if (id == 1)
        {
            // The payload's first word is the state: 1 starts a round, 2 ends one.
            const int state = *static_cast<const int*>(userData);
            if (state == 1)
                courier->eventGameStart();
            else if (state == 2)
                courier->event_roundEnd();
        }
        else if (id == 6)
        {
            // the only null check on the courier in the whole function. Retail tests it here and
            // nowhere else; reproduced rather than made consistent.
            if (courier)
                courier->event_worldReset();
        }
        else if (id == 8)
        {
// Updates the AI team ratio when the only human player switches sides.
            const int* const p = static_cast<const int*>(userData);

            const int playerId = p[0];

            if (world::playerManager->getPlayerFromId(playerId)
                    == world::playerManager->getLocalHumanPlayer()
                && world::playerManager->getNumberOfHumanPlayers() == 1)
            {
                serverSettings->m_teamRatioPercent =
                    static_cast<float>(1.0f / (serverSettings->getTeamRatioPercent() * 0.0099999998f)
                                       * 100.0f);
            }

            courier->event_playerSwitchSides(p[1], playerId);
            return;
        }
        return;

    // category 1: player lifecycle
    case 1:
        {
            world::IPlayer* const player = static_cast<world::IPlayer*>(userData);
            if (id == 2)
            {
                // After level unloading begins, reports disconnects only for AI players.
                if (!m_unknown05 || player->getIsAIPlayer())
                    courier->eventPlayerDisconnect(player->getId());
            }
            else if (id == 4)
            {
                courier->eventPlayerCreated(player->getIsAIPlayer());
            }
            else if (id == 6)
            {
                courier->eventPlayerSpawned(player->getId());
            }
            else if (id == 10)
            {
                courier->event_freeCameraFire(*static_cast<const int*>(userData) != 0);
            }
        }
        return;

    // category 3: the AI's own twenty-four events
    case 3:
        switch (id)
        {
        case 4u:
            // Draws debug primitives when enabled; otherwise only ages them.
            if (static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getDraw())
            {
                courier->eventDrawDebug();
                static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->drawAll();
            }
            else
            {
                static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->nodrawUpdate();
            }
            return;

        case 8u:
            {
                // The payload is (target, orderType); 4/5/6 select move/attack/defend.
                const int* const p = static_cast<const int*>(userData);
                if (p[1] == 5)
                    courier->event_recievedAttackCommanderOrder(p[0]);
                else if (p[1] == 6)
                    courier->event_recievedDefendCommanderOrder(p[0]);
                else if (p[1] == 4)
                    courier->event_recievedMoveCommanderOrder(p[0]);
            }
            return;

        case 9u:
            {
                // The same shape one order-type wider - squad orders add 7, repair.
                const int* const p = static_cast<const int*>(userData);
                if (p[1] == 5)
                    courier->event_recievedAttackSquadOrder(p[0]);
                else if (p[1] == 6)
                    courier->event_recievedDefendSquadOrder(p[0]);
                else if (p[1] == 7)
                    courier->event_receivedRepairSquadOrder(p[0]);
                else if (p[1] == 4)
                    courier->event_recievedMoveSquadOrder(p[0]);
            }
            return;

        case 0xBu:
            {
                const int* const p = static_cast<const int*>(userData);
                courier->event_botTeamKilled(p[0], p[1]);
            }
            return;

        case 0xCu:
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                ->create(static_cast<world::IPlayer*>(userData));
            return;

        case 0xDu:
            {
                IAIPlayerFactory* const factory =
                    static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
                IAIPlayer* const aiPlayer = factory->getPlayerFromId(
                    static_cast<world::IPlayer*>(userData)->getId());
                if (aiPlayer)
                    factory->destroy(aiPlayer);
            }
            return;

        case 0x10u:
            {
                // retail copies five payload words to the stack here and uses two of them. The
                // other three are dead stores the compiler kept. Only the two live ones are written.
                const int* const p = static_cast<const int*>(userData);
                courier->event_requestNewOrders(p[1], p[0]);
            }
            return;

        case 0x15u:
            {
                // two subsystems are told, in this order: the mine manager first so its polygon
                // sets are current, then the AI proper.
                const unsigned int mineId = *static_cast<const unsigned int*>(userData);
                static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager)
                    ->mineRemoved(mineId);
                courier->event_mineRemoved(mineId);
            }
            return;

        case 0x16u:
            {
                // The payload is an object pointer followed by a team byte (offset 4 on x86).
                const SpottedPayload* const payload = static_cast<const SpottedPayload*>(userData);
                courier->event_enemySpotted(payload->aiObject, payload->team);
            }
            return;

        case 0x17u:
            courier->event_isRepaired(*static_cast<const int*>(userData));
            return;

        // the arms that dereference the payload as a world::IObject
        // Each of these walks a vehicle or a pathfinding map through `world::IObject` vtable slots
        // this tree does not name yet (decimal +116, +124, +128, +148, +152, +200, +204, +208, +252
        // and +296). Naming them is a `world::IObject` slot-map job, not an AI one, and guessing
        // them would put ten wrong method names in the tree. Each stop below says which arm it is.
        case 0u:
            unwrittenArm("category 3 id 0 - player killed, plus the vehicle's armament release",
                         (int)category, id);
            return;
        case 1u:
            unwrittenArm("category 3 id 1 - player entered vehicle (and the 'not prepared for use"
                         " with AI' Debug at line 166)", (int)category, id);
            return;
        case 2u:
            unwrittenArm("category 3 id 2 - player exited vehicle", (int)category, id);
            return;
        case 3u:
            unwrittenArm("category 3 id 3 - eventPlayerExitVehicle", (int)category, id);
            return;
        case 5u:
            unwrittenArm("category 3 id 5 - object slot +76", (int)category, id);
            return;
        case 6u:
            unwrittenArm("category 3 id 6 - man down", (int)category, id);
            return;
        case 7u:
            unwrittenArm("category 3 id 7 - revived", (int)category, id);
            return;
        case 0xAu:
            unwrittenArm("category 3 id 10 - radio message (needs BFConsole::getAIMessageSettings,"
                         " itself a frontier)", (int)category, id);
            return;
        case 0xEu:
            unwrittenArm("category 3 id 14 - vehicle armament release", (int)category, id);
            return;
        case 0xFu:
            unwrittenArm("category 3 id 15 - commander request (artillery/uav/supply/vehicle drop),"
                         " a 68-byte payload struct", (int)category, id);
            return;
        case 0x11u:
            unwrittenArm("category 3 id 17 - bridge destroyed", (int)category, id);
            return;
        case 0x12u:
            unwrittenArm("category 3 id 18 - bridge repaired", (int)category, id);
            return;
        case 0x13u:
            unwrittenArm("category 3 id 19 - object slot +204", (int)category, id);
            return;
        case 0x14u:
            unwrittenArm("category 3 id 20 - object slot +200", (int)category, id);
            return;

        default:
            // Reports an unknown AI event with the original diagnostic line number.
            Debug(DebugTypeError, __FILE__, 494, "") << "Unknown and unhandled AIEvent: " << id;
            return;
        }

    // category 5: level load/unload
    case 5:
        if (id == 2)
        {
            if (CommonSingletonServer::m_iaiGame)
                static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->setIsPrecaching(true);
        }
        else if (id == 6)
        {
            // Permanently restricts disconnect reporting to bots for this listener's remaining lifetime.
            m_unknown05 = true;
        }
        else if (id == 7)
        {
            if (CommonSingletonServer::m_iaiGame)
                static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->setIsPrecaching(false);
            courier->event_levelLoaded();
        }
        return;

    // everything else, including the whole of category 2
    default:
        return;
    }
}
} // namespace ai
} // namespace hfe
} // namespace dice
