
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
#include "../BFKnowledge/AIPlayerFactory.h"         // IAIPlayerFactory, isOccupiedObject
#include "../BFKnowledge/IAIObject.h"               // the vehicle and armament arms
#include "../BFKnowledge/IAIObjectPlugIn.h"         // IAIObjectUnit / IAIObjectPhysical
#include "../Module/BFConsole.h"                    // BFConsole::getAIMessageSettings (radio)
#include "../../AIDLL/AI/AIKnowledge/BFInformationCourier.h"  // IAIInformationCourier armament
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/Main/AIEventCourier.h"     // IAIEventCourier - 55 named slots
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"  // IAISettings::getDraw
#include "../../AIDLL/AI/AIKnowledge/AIMineManager.h"      // IAIMineManager::mineRemoved
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

// Category 3 ids 1 and 2 (seat entered / left): the five-dword block Soldier::enter posts
// (Soldier.cpp SoldierAiSeatPayload). The arms read +0x10 as a byte (`cmpb $0x0, 0x10(%esi)`).
struct SeatPayload
{
    IAIObject* object;   // +0x00 the entered object (id 1) / the leaving soldier (id 2)
    IAIObject* other;    // +0x04 id 1: the armament lender; id 2: the vehicle left
    int        team;     // +0x08
    int        playerId; // +0x0C
    int        flag;     // +0x10 low byte tested
};

// Category 3 id 0 (player killed): two ints, then the object whose borrowed armament is returned.
struct KilledPayload
{
    int        a;        // +0x00 eventPlayerKilled's second argument
    int        b;        // +0x04 its first
    IAIObject* object;   // +0x08
};

// Category 3 ids 6 / 7 (man down / revived): Soldier.cpp SoldierAiPlayerPayload.
struct ObjectPlayerPayload
{
    IAIObject* object;   // +0x00
    int        playerId; // +0x04
};

// Category 3 ids 3 and 17: an int followed by an object.
struct IntObjectPayload
{
    int        value;    // +0x00 id 3: eventPlayerExitVehicle's int; id 17: the bridge's int
    IAIObject* object;   // +0x04
};

// Category 3 id 10 (radio): the message key and two ints, all forwarded.
struct RadioPayload
{
    int message;         // +0x00 BFConsole::getAIMessageSettings key
    int a;               // +0x04
    int b;               // +0x08
};

// Category 3 id 15 (commander request), 68 bytes copied to the stack whole (bf2 0x842FE75).
// Only +0x00 (type), +0x30, +0x34 and +0x38 are read; the rest is [UNRESOLVED].
struct CommanderRequestPayload
{
    int            type;        // +0x00 0 artillery, 1 UAV, 2 vehicle drop, 3 supply
    unsigned short unknown04;   // +0x04
    Vec3           unknown08[3];// +0x08
    unsigned char  unknown2C;   // +0x2C
    int            a;           // +0x30
    int            b;           // +0x34
    Vec3           position;    // +0x38
};
static_assert(sizeof(CommanderRequestPayload) == 0x44, "retail 68-byte commander request");

#if defined(_M_IX86)
static_assert(sizeof(SeatPayload) == 20 && offsetof(KilledPayload, object) == 8
              && offsetof(IntObjectPayload, object) == 4, "retail ECAI payloads");
#endif

IAIInformationCourier* informationCourier()
{
    return static_cast<IAIInformationCourier*>(CommonSingletonServer::m_iaiInformationCourier);
}

// The armament hand-back of ids 0, 2 and 14 once the unit must borrow armament: reset it, and the
// information courier resets both sides' handles (1 then 2).
void returnArmament(IAIObject* object)
{
    object->resetArmament();                                         // +0x80
    informationCourier()->resetArmament(object->getHandle(1));      // +0x30
    informationCourier()->resetArmament(object->getHandle(2));
}

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

        // bf2 0x842F7BE: the kill, then a non-null unit object's borrowed armament goes back
        // (isUnit +0x74 gates the getUnit dereference).
        case 0u:
            {
                const KilledPayload* const p = static_cast<const KilledPayload*>(userData);
                courier->eventPlayerKilled(p->b, p->a);
                IAIObject* const object = p->object;
                if (object && object->isUnit()                        // no getUnit null test here
                    && object->getUnit()->mustBorrowArmament(object))
                    returnArmament(object);
            }
            return;

        // bf2 0x842F870: entering a seat. The entered object takes the team (setSide +0x128), its
        // primary object's physics are enabled for that team (IAIObjectPhysical +0x74), a unit that
        // must borrow armament borrows the lender's (+0x7C) and the information courier links both
        // sides' handles (+0x2C lender, borrower), the lender leaves the grid (+0xC8), and the AI
        // hears eventPlayerEnterVehicle(playerId, object, lender).
        case 1u:
            {
                const SeatPayload* const p = static_cast<const SeatPayload*>(userData);
                IAIObject* const object = p->object;
                if (!object)
                {
                    if (static_cast<unsigned char>(p->flag))
                        return;
                    // The image logs and carries on into the null object (line 166).
                    Debug(DebugTypeError, "AI/AIInterface/Main/AIEventListener.cpp", 166, "")
                        << "You have tried to enter a vehicle that is not prepared for use with AI."
                           " Game will crash.";
                }
                IAIObject* const primary = object->getPrimaryObject();
                IAIObject* const lender = p->other;
                object->setSide(p->team);
                if (primary)
                {
                    IAIObjectPhysical* const physical = primary->getPhysical();
                    if (physical)
                        physical->enablePhysics(p->team);
                }
                if (lender && object->getUnit() && object->getUnit()->mustBorrowArmament(object))
                {
                    object->borrowArmament(lender);
                    {
                        const unsigned int borrower = object->getHandle(1);
                        informationCourier()->borrowArmament(lender->getHandle(1), borrower);
                    }
                    {
                        const unsigned int borrower = object->getHandle(2);
                        informationCourier()->borrowArmament(lender->getHandle(2), borrower);
                    }
                }
                if (p->other)
                    p->other->removeFromGrid();
                courier->eventPlayerEnterVehicle(p->playerId, p->object, p->other);
            }
            return;

        // bf2 0x842FAB0: leaving a seat. The vehicle left returns borrowed armament and, when
        // nobody else occupies it, goes neutral (setSide 0); the soldier re-enters the grid.
        case 2u:
            {
                const SeatPayload* const p = static_cast<const SeatPayload*>(userData);
                if (static_cast<unsigned char>(p->flag))
                    return;
                IAIObject* const vehicle = p->other;
                IAIObject* const soldier = p->object;
                if (vehicle)
                {
                    if (vehicle->getUnit() && vehicle->getUnit()->mustBorrowArmament(vehicle))
                        returnArmament(vehicle);
                    if (!isOccupiedObject(vehicle, vehicle))
                        vehicle->setSide(0);
                }
                if (soldier)
                    soldier->reInsertInGrid();                       // +0xCC
            }
            return;

        // bf2 0x842FB84.
        case 3u:
            {
                const IntObjectPayload* const p = static_cast<const IntObjectPayload*>(userData);
                courier->eventPlayerExitVehicle(p->object, p->value);
            }
            return;

        // bf2 0x842FBE9: the payload is the AI object (Soldier's kit pick-up); update +0x4C.
        case 5u:
            static_cast<IAIObject*>(userData)->update();
            return;

        // bf2 0x842FBF6.
        case 6u:
            {
                const ObjectPlayerPayload* const p = static_cast<const ObjectPlayerPayload*>(userData);
                courier->eventManDown(p->playerId, p->object);
            }
            return;

        // bf2 0x842FC16: the object is updated first.
        case 7u:
            {
                const ObjectPlayerPayload* const p = static_cast<const ObjectPlayerPayload*>(userData);
                p->object->update();
                courier->eventRevived(p->playerId, p->object);
            }
            return;

        // bf2 0x842FD3F: a known message (BFConsole::m_instance, read directly) is forwarded.
        case 0xAu:
            {
                const RadioPayload* const p = static_cast<const RadioPayload*>(userData);
                AIRadioMessageSettings settings;
                if (BFConsole::m_instance->getAIMessageSettings(p->message, settings))
                    courier->eventRadioMessage(settings, p->a, p->b);
            }
            return;

        // bf2 0x842FDF7: armament release of a non-null object.
        case 0xEu:
            {
                IAIObject* const object = static_cast<IAIObject*>(userData);
                if (object && object->getUnit() && object->getUnit()->mustBorrowArmament(object))
                    returnArmament(object);
            }
            return;

        // bf2 0x842FE75: the commander's requests by type; others are logged at line 449.
        case 0xFu:
            {
                const CommanderRequestPayload request = *static_cast<const CommanderRequestPayload*>(userData);
                switch (request.type)
                {
                case 0: courier->event_receivedArtilleryRequest(request.a, request.b, request.position); break;
                case 1: courier->event_receivedUavRequest(request.a, request.b, request.position); break;
                case 2: courier->event_receivedVehicleDropRequest(request.a, request.b, request.position); break;
                case 3: courier->event_receivedSupplyRequest(request.a, request.b, request.position); break;
                default:
                    Debug(DebugTypeError, "AI/AIInterface/Main/AIEventListener.cpp", 449, "")
                        << "AI received unknown request type: " << request.type;
                    break;
                }
            }
            return;

        // bf2 0x84301D9: while playing (IAIGame +0x60), the bridge's map position id is destroyed
        // (with a null object) and the bridge reported destroyed.
        case 0x11u:
            if (static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->isPlaying())
            {
                const IntObjectPayload* const p = static_cast<const IntObjectPayload*>(userData);
                IAIObject* const bridge = p->object;
                courier->event_pathfindingMapIdDestroyed(bridge->getPhysical()->getMapPosId(), 0);
                courier->event_bridgeDestroyed(bridge, p->value);
            }
            return;

        // bf2 0x843023F: the payload is the AI object (ArmorGLComp's repair post).
        case 0x12u:
            {
                IAIObject* const bridge = static_cast<IAIObject*>(userData);
                courier->event_pathfindingMapIdRestored(bridge->getPhysical()->getMapPosId());
                courier->event_bridgeRepaired(bridge);
            }
            return;

        // bf2 0x8430280 / 0x843028A: the payload is the AI object.
        case 0x13u:
            static_cast<IAIObject*>(userData)->reInsertInGrid();
            return;
        case 0x14u:
            static_cast<IAIObject*>(userData)->removeFromGrid();
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
