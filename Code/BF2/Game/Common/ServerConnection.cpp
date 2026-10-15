// Server connection state and object relevance for replication.

#include "ServerConnection.h"

#include "../../IO/Network/NetworkManager.h"     // the three descriptor tables
#include "../../IO/Network/INetworkable.h"       // m_networkableId (+0x04) - networkableNetId reads it
#include "../GameLogic/SquadManager/ISquadManager.h"   // getCommanderCam (+0x74)
#include "../IGameLogic.h"                       // gameLogic - getUAVRadius, the RC-object map
#include "../Objects/ICameraObject.h"
#include "../Objects/IObject.h"
#include "../../Core/Ids.h"                    // IID_ICameraObject
#include "../Objects/IPlayer.h"
#include "GhostManager.h"                     // the class is complete now
#include "GameEventManager.h"                 // m_gameEventManager->addEvent - clientSendPlayerDatabase
#include "../GameEvents/CreatePlayerEvent.h"  // clientSendPlayerDatabase announces each existing player
#include "../GameEvents/UnlockEvent.h"        // ...then this connection's own unlocks
#include "../../Scene/Object/PlayerManager.h" // world::playerManager->getPlayers()
#include "GameEvent.h"                        // PostRemoteEvent + EventCategory (ECNetwork) - clientSendDatabase
#include "../GameEvents/BeginRoundEvent.h"    // clientSendDatabase's round marker
#include "../GameEvents/VoipOnOffEvent.h"     // ...its per-player VoIP roster entry
#include "../GameEvents/CommanderEvent.h"     // ...and a commander's re-announce
#include "../GameServer/ServerSettings.h"     // serverSettings->getNoVehicles()
#include "../GameLogic/SpawnSystem/SpawnManager.h" // createAllSpawnGroupsOnClient (ISpawnManager +0x50)
#include "CreateObjectEvent.h"                // clientSendDatabase phase A - the dynamic-object create events
#include "../GameEvents/StickyProjectileEvent.h" // phase C - replay every projectile stuck to an object
#include "../GameEvents/EnterVehicleEvent.h"   // phase B - each player's vehicle association
#include "../GameEvents/HandlePickupEvent.h"   // phase B - a spawned soldier's kit
#include "../Objects/ISoldier.h"               // phase B - ISoldier::getKit
#include "../Objects/Game/StickyProjectileManager.h" // phase C - authoritative stuck-projectile set
#include "../Objects/Game/CommanderToolsManager.h" // typed commander UAV/satellite state
#include "../Objects/Game/KitTemplate.h"       // phase A - the two CreateKitEvent operands
#include "../Objects/Game/Zipline.h"           // phase A - m_endPoint rides CreateObjectEvent's rotation
#include "../Objects/Game/GrapplingHookRope.h" // phase A - the four rope creation fields
#include "../Objects/Game/GrapplingHookRopeContainer.h" // phase B - the active container's rope/segment
#include "../GameEvents/GrapplingHookCreateEvent.h"          // phase A - the rope's second event
#include "../GameEvents/GrapplingHookContainerCreateEvent.h" // phase B - the rider association
#include "../Objects/Projectiles/GenericProjectile.h" // phase C - parent/stick state accessors
#include "../Objects/IObjectTemplate.h"       // templ->getClassID() / getId()
#include "../../IO/Network/NetworkableInfo.h" // getForceNetworkableId - the phase-A replication gate
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // getDefaultTeam - the object's team
#include "../Objects/ObjectLayoutAccess.h"    // physicsNode (+0x44) / objectTemplate (+0x14)
#include "../Objects/RemoteControlledObjectTemplate.h"   // rcType - getSpecialCommanderObjects
#include "../../Physics/IPhysicsNode.h"       // getPositionalSpeed (+0x60) - the priority speed factor
#include "../../IO/Network/INetworkableObject.h"   // m_ownerId / m_object - the scope pass reads both
#include "../../Core/Debug.h"                 // the attested line-98 "Control object not found"
#include <cstdlib>                            // std::abort - the shipped asserts in clientSendDatabase
#include <cmath>                              // std::sqrt - calculateObjectPriority's distance

namespace dice {
namespace hfe {

extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp
extern IBase* spawnManager;                      // ds:0x8AD3A00 - Core/Globals.cpp - clientSendDatabase's phase E

namespace {

// Typed commander-tool predicates used by connection scoping.
bool commanderToolsSatelliteScanActive(int team)
{
    return world::commanderToolsManager->getSatelliteScanActive(team);
}

bool commanderToolsUAVActive(int team, Vec3* centreOut)
{
    return world::commanderToolsManager->getUAVActive(team, centreOut);
}

// Returns the root parent's player-control team, or zero without that interface.
int remoteControlledObjectTeam(world::IObject* object)
{
    world::IObject* const root = world::getRootParent(object);
    world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
        root->queryInterface(world::IID_IPlayerControlObject));
    if (pco)
        return pco->getTeam();                           // IPlayerControlObject +0x98
    return 0;
}

// ICameraObject's transform - both updateGhostManager call sites inline `vtable +0x10 with a 0
// argument` and copy the 64-byte result; that slot is getTransformation(float).
void cameraTransform(world::ICameraObject* camera, Mat4& out)
{
    out = camera->getTransformation(0.0f);               // ICameraObject +0x10
}

// io::INetworkable's net id at +0x04. m_networkableId is a named, public member
// (INetworkable.h) now - 0 means "not registered with the NetworkManager" - so this is the plain
// accessor both callers below want, no longer a raw-offset frontier.
unsigned short networkableNetId(io::INetworkable* networkable)
{
    return networkable->m_networkableId;
}

} // namespace

// Selects executeServer for received events after base construction. Initializes
// load state and priority-evaluation count to zero, and the optional game id to -1.
ServerConnection::ServerConnection(ConnectionDemoMode demoMode)
    : ClientConnection(demoMode)
    , m_dword17C(0)                                      // +0x17C
    , m_loadUnload(0)                                    // +0x180
    , m_gameId(-1)                                       // +0x184  -1 is "none", not 0
{
    m_executeServer = true;                              // +0x170, the base's field - see above
}

// ~ServerConnection - ELF D1 0x8088C40, 27 bytes / D0 0x8088C60, 40. Re-seat the vptr and chain to
// the base. It owns nothing of its own - all three fields are ints.
ServerConnection::~ServerConnection()
{
}

// Only demo mode 1 is a recording connection.
bool ServerConnection::getIsRecordingConnection() const
{
    return getDemoMode() == 1;
}

// Enables or disables roots and player-control objects; ordinary children are unchanged.
// Enabling a cached carcass is fatal. Enabling a root PCO also calls onFirstGhost.
// The Boolean parameters are unused.
void enableObject(world::IObject* object, bool /*unused - dead in the ELF*/)
{
    if (object->getFlags() & 0x20u)
        std::abort();
    void* const pco = object->queryInterface(world::IID_IPlayerControlObject);
    if (!object->getParent() || pco)
    {
        object->updateFlags(0, 1);
        if (pco && !object->getParent())
            static_cast<world::IPlayerControlObject*>(pco)->onFirstGhost();   // ipco +0x154
    }
}

void disableObject(world::IObject* object, bool /*unused - dead in the ELF*/)
{
    if (!object->getParent() || object->queryInterface(world::IID_IPlayerControlObject))
        object->updateFlags(1, 0);
}

// Changes ghost activity throughout an object subtree. Always updates the root;
// when includePlayerControlObjects is false, skips PCO children and their subtrees.
// Objects without networkables still have their children visited.
namespace {
void setGhostingRecursive(world::IObject* object, bool includePlayerControlObjects, bool active)
{
    if (io::INetworkable* const networkable = object->getNetworkable())   // ELF +0xD8
        networkable->setGhostActive(active);                              // ELF +0x3C
    for (world::IObject* child = object->getChild(); child; child = child->getSibling())
    {
        if (!includePlayerControlObjects
            && child->queryInterface(world::IID_IPlayerControlObject))
            continue;
        setGhostingRecursive(child, includePlayerControlObjects, active);
    }
}
}

void enableGhosting(world::IObject* object, bool includePlayerControlObjects)
{
    setGhostingRecursive(object, includePlayerControlObjects, true);
}

void disableGhosting(world::IObject* object, bool includePlayerControlObjects)
{
    setGhostingRecursive(object, includePlayerControlObjects, false);
}

// Scopes static descriptors at priority -1, excluding them from ordinary priority ordering.
void ServerConnection::addStaticGhosts()
{
    std::map<unsigned short, io::NetworkableDescriptor*>& statics =
        io::networkManager->getStaticDescriptors();
    for (std::map<unsigned short, io::NetworkableDescriptor*>::iterator it = statics.begin();
         it != statics.end(); ++it)
    {
        m_ghostManager->ghostObject(it->second, -1.0f, false);           // 1.0f = static ghost
    }
}

// Scopes every descriptor in every root group at priority 1 for demo recording,
// which has no camera viewpoint to cull against.
void ServerConnection::getAllObjects()
{
    typedef std::list<std::pair<io::INetworkableObject*, io::NetworkableDescriptor*> > Group;
    std::list<Group>& roots = io::networkManager->getScopableRootDescriptors();

    for (std::list<Group>::iterator group = roots.begin(); group != roots.end(); ++group)
    {
        for (Group::iterator it = group->begin(); it != group->end(); ++it)
            m_ghostManager->ghostObject(it->second, 1.0f, false);
    }
}

// Ghosts the team's remote-controlled objects in states 1–3 at priority 0.03.
// These objects uniquely set the final ghost flag; its meaning remains [UNRESOLVED].
void ServerConnection::getSpecialCommanderObjects(int team)
{
    // IGameLogic returns the map as void* (+0x1C4, `lea eax,[this+B4h]`) - the interface has no
    // typed getter, so the cast is the tree's own and is marked rather than hidden.
    std::map<unsigned int, world::IObject*>& rcObjects =
        *static_cast<std::map<unsigned int, world::IObject*>*>(
            gameLogic->getRemoteControlledObjectMap());
    for (std::map<unsigned int, world::IObject*>::iterator it = rcObjects.begin();
         it != rcObjects.end(); ++it)
    {
        world::IObject* const object = it->second;
        // Registry objects carry a RemoteControlledObjectTemplate; the PE reads its rcType at
        // retail +0x180. The named member, because this build's template layout is not retail's
        // (rcType sits at +0x208 on x86 here). [UNRESOLVED] Meanings of the selected type values.
        const int state = static_cast<const world::RemoteControlledObjectTemplate*>(
            world::object_detail::LayoutAccess::objectTemplate(object))->rcType;   // IObject +0x14
        if (remoteControlledObjectTeam(object) != team)
            continue;
        if (state != 1 && state != 2 && state != 3)
            continue;

        io::INetworkable* const networkable = object->getNetworkable();   // slot 54
        if (!networkable)
            continue;

        io::NetworkableDescriptor* const descriptor =
            io::networkManager->getDescriptor(networkableNetId(networkable));   // networkableNetId +0x04
        m_ghostManager->ghostObject(descriptor, 0.03f, true);           // keepHigher, the rare flag
    }
}

// Adds or removes ghost eligibility recursively through the object's children.
// Skips zero network ids but still visits descendants; requires a nonnull root.
void ServerConnection::updateGhostable(world::IObject* object, bool add)
{
    if (object->getNetworkable())                                        // called twice, as shipped
    {
        const unsigned short netId = networkableNetId(object->getNetworkable());   // +0x04
        if (netId)
        {
            if (add)
                m_ghostManager->addGhostable(netId);                    //  (written)
            else
                m_ghostManager->removeGhostable(netId);                 //  (written)
        }
    }

    for (world::IObject* child = object->getChild(); child; child = child->getSibling())
        updateGhostable(child, add);                                     // the whole subtree
}

// Announces other players to the joining client, then sends its own unlock table.
// Unlock-event construction can insert missing keys in the player's unlock maps.
void ServerConnection::clientSendPlayerDatabase()
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        if (player->getId() != m_connectionId)                          // skip this connection's own player
            m_gameEventManager->addEvent(new CreatePlayerEvent(player, true));
    }

    world::IPlayer* const own = getPlayer();
    if (own)
        m_gameEventManager->addEvent(new UnlockEvent(
            static_cast<UnlockEvent::UnlockEventTypes>(1), own->getId(), own->getUnlocks()));
}

// Sends the joining client's world database: objects, player associations, projectiles,
// spawn groups, round state, and static ghosts. Finishes with ECNetwork event 4.
// Called after the connection enters state 2.
void ServerConnection::clientSendDatabase()
{
    // Replay create events for dynamic descriptors. Static objects (netId <= 0x281)
    // already exist from map loading and are skipped.
    if (io::networkManager)                                        // ELF guards on the singleton too
    {
        std::map<unsigned short, io::NetworkableDescriptor*>& descMap =
            io::networkManager->getDescriptorMap();
        for (std::map<unsigned short, io::NetworkableDescriptor*>::iterator it = descMap.begin();
             it != descMap.end(); ++it)
        {
            if (!it->second->addedToNetwork)
                continue;

            // Resolve the object through the descriptor's first networkable, not a separate
            // network-id lookup. Skip descriptors without an associated object.
            io::INetworkable* const descNetworkable = it->second->properties[0].networkables[0];
            io::INetworkableObject* const networkableObject = descNetworkable->m_networkableObject;
            world::IObject* const object = networkableObject ? networkableObject->m_object : 0;
            if (!object)
                continue;

            io::INetworkable* const networkable = object->getNetworkable();
            // skip forced-id networkables and objects flagged out of the create protocol (m_flags bit 5)
            if (networkable->m_networkableInfo->getForceNetworkableId() || (object->getFlags() & 0x20))
                continue;

            const world::IObjectTemplate* const templ = object->getTemplate();
            if (!templ)
                continue;
            if (networkable->m_networkableId <= 0x281)            // pre-stored/static - already on the client
                continue;

            const unsigned int classId = templ->getClassID();
            if (classId == world::CID_KitTemplate)
            {
                // a ground kit -> CreateKitEvent (no team, no rotation; the kit's own position, its
                // template's creation index, and the level it was created with).
                m_gameEventManager->addEvent(new CreateKitEvent(
                    templ->getId(), networkable->m_networkableId,
                    world::kitCreationIndex(object), world::kitChosenUnlockLevel(object),
                    object->getAbsolutePosition()));
                continue;                                          // kit handled - next descriptor
            }

            // the team the object's controlling PCO was given (0 if it has none) - walk up to the PCO.
            // (Used by the generic CreateObjectEvent branch and by both gadget arms below.)
            int defTeam = 0;
            for (world::IObject* p = object; p; p = p->getParent())
            {
                world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
                    p->queryInterface(world::IID_IPlayerControlObject));
                if (pco)
                {
                    defTeam = pco->getDefaultTeam();                              // ipco +0x94
                    break;
                }
            }

            // Replay ropes before ziplines, then generic objects. Typed members accommodate
            // the differing class layouts in PE and ELF.
            if (classId == world::CID_GrapplingHookRopeTemplate)
            {
                // A deployed rope replays as TWO events. First the ordinary create - same position
                // and sign-flipped Euler rotation as the generic branch below.
                world::GrapplingHookRope* const rope = static_cast<world::GrapplingHookRope*>(object);

                Vec3 ropeRotation;
                float ropeZ, ropeX, ropeY;
                getRotationZXY(object->getParent() ? object->getRelativeTransformation()
                                                   : object->getAbsoluteTransformation(),
                               ropeZ, ropeX, ropeY);
                ropeRotation.x = -ropeY;
                ropeRotation.y = -ropeX;
                ropeRotation.z = -ropeZ;

                m_gameEventManager->addEvent(new CreateObjectEvent(
                    templ->getId(), networkable->m_networkableId, defTeam,
                    object->getAbsolutePosition(), ropeRotation));

                // Then the rope's own state. These four are exactly what
                // GrapplingHookCreateEvent::executeClient writes back into the rope before calling
                // initialize(), so the joining client rebuilds the same span the owner sees.
                m_gameEventManager->addEvent(new GrapplingHookCreateEvent(
                    networkable->m_networkableId, rope->m_ownerPlayerId, rope->m_creationScalar,
                    rope->m_creationPosition, rope->m_creationDirection));
                continue;                                      // rope handled - next descriptor
            }

            if (classId == world::CID_ZiplineTemplate)
            {
                // A zipline does NOT send a rotation. Retail reuses CreateObjectEvent's rotation
                // slot to carry the line's FAR endpoint: the position is the start, m_endPoint is
                // the end, and the client needs both to build the wire. No getRotationZXY call is
                // made on this arm in either port.
                const world::Zipline* const zipline = static_cast<const world::Zipline*>(object);
                m_gameEventManager->addEvent(new CreateObjectEvent(
                    templ->getId(), networkable->m_networkableId, defTeam,
                    object->getAbsolutePosition(), zipline->m_endPoint));
                continue;                                      // zipline handled - next descriptor
            }

            const Vec3 position = object->getAbsolutePosition();

            // Euler-extract the transform, sign-flipped - exactly the inverse of
            // createObjectOnAllClients' setRotationZXY(-z, -y, -x) build.
            Vec3 rotation;
            float rotZ, rotX, rotY;
            getRotationZXY(object->getParent() ? object->getRelativeTransformation()
                                               : object->getAbsoluteTransformation(),
                           rotZ, rotX, rotY);
            rotation.x = -rotY;
            rotation.y = -rotX;
            rotation.z = -rotZ;

            m_gameEventManager->addEvent(new CreateObjectEvent(
                templ->getId(), networkable->m_networkableId, defTeam, position, rotation));

        }
    }

    // phase B: per-player associations.
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;

        // A spawned player's default vehicle -> EnterVehicleEvent, and if that is a soldier, its kit
        // as a HandlePickupEvent. (Both null before the player spawns, so a fresh joiner skips this.)
        world::IObject* const defVeh = player->getDefaultVehicle();
        if (defVeh)
        {
            m_gameEventManager->addEvent(new EnterVehicleEvent(
                player->getId(), defVeh->getNetworkable()->m_networkableId, true));

            if (defVeh->getTemplate()->getClassID() == world::CID_SoldierTemplate)
            {
                if (defVeh->getFlags() & 0x20)                     // the ELF's fatal assert (LABEL_83)
                    std::abort();
                world::ISoldier* const soldier = static_cast<world::ISoldier*>(
                    defVeh->queryInterface(world::IID_ISoldier));
                // ISoldier::getKit returns the (unrecovered) world::Kit*; its IObject base is at offset
                // 0, so reinterpret to reach getNetworkable()/getFlags().
                world::IObject* const kit = reinterpret_cast<world::IObject*>(soldier->getKit());
                if (kit)
                {
                    if (kit->getFlags() & 0x20)                    // the ELF's fatal assert (LABEL_83)
                        std::abort();
                    m_gameEventManager->addEvent(new HandlePickupEvent(
                        static_cast<unsigned char>(player->getId()),
                        kit->getNetworkable()->m_networkableId,
                        defVeh->getNetworkable()->m_networkableId));
                }
            }
        }

        // A current vehicle distinct from the default one -> EnterVehicleEvent, unless it is one of the
        // transient container states (a free camera, a ladder, or a zipline container send nothing; a
        // grappling-hook rope container sends its own create event).
        world::IObject* const veh = player->getVehicle();
        if (veh && veh != defVeh)
        {
            const unsigned int vehClass = veh->getTemplate()->getClassID();
            if (vehClass == world::CID_GrapplingHookRopeContainerTemplate)
            {
                // Replay the rider, rope ID, segment index, and entry angle. Requires the
                // container's rope pointer; the original has no null guard.
                const world::GrapplingHookRopeContainer* const container =
                    static_cast<const world::GrapplingHookRopeContainer*>(veh);
                m_gameEventManager->addEvent(new GrapplingHookContainerCreateEvent(
                    player->getId(), container->m_rope->m_networkableId,
                    container->m_segmentIndex, container->m_entryAngle));
            }
            else if (vehClass != world::CID_FreeCameraTemplate
                     && vehClass != world::CID_LadderContainerTemplate
                     && vehClass != world::CID_ZiplineContainerTemplate)
            {
                if (!player->getDefaultVehicle() || (veh->getFlags() & 0x20))   // ELF's fatal assert
                    std::abort();
                m_gameEventManager->addEvent(new EnterVehicleEvent(
                    player->getId(), veh->getNetworkable()->m_networkableId, false));
            }
            // free camera / ladder / zipline container: nothing - fall through to the VoIP entry.
        }

        // fires for every player: the VoIP roster entry.
        m_gameEventManager->addEvent(new VoipOnOffEvent(player->getId(), player->getVoipEnabled()));

        if (player->getIsCommander())
            m_gameEventManager->addEvent(new CommanderEvent(
                static_cast<CommanderEvent::CommanderEventTypes>(3), player->getTeam(), player->getId()));
    }

    // phase C: sticky projectiles.
    // The manager and set entries are unguarded in both retail ports. Only projectiles with a live
    // parent link are replayed. Preserve the shipped getter order: transformation, then host object.
    const std::set<world::GenericProjectile*>& stickyProjectiles =
        world::stickyProjectileManager->getAllStickyProjectiles();
    for (std::set<world::GenericProjectile*>::const_iterator it = stickyProjectiles.begin();
         it != stickyProjectiles.end(); ++it)
    {
        world::GenericProjectile* const projectile = *it;
        if (projectile->getParent())
        {
            const Mat4& transform = projectile->getStickTransformation();
            world::IObject* const target = projectile->getStickOnParent();
            m_gameEventManager->addEvent(new StickyProjectileEvent(projectile, target, transform));
        }
    }

    // phase D: the ELF builds two Triggerable lists here and immediately clears them without
    // sending anything; omitted as observably empty.

    // phase E: spawn groups - the client's spawn menu. fires.
    static_cast<SpawnManager*>(spawnManager)->createAllSpawnGroupsOnClient(m_connectionId);

    // phase F: the round state. fires.
    m_gameEventManager->addEvent(new BeginRoundEvent(
        gameLogic->getBeginRoundState(),
        gameLogic->getNextStateChangeAtTick(),
        serverSettings->getNoVehicles(),
        true));

    // phase G: static ghosts. fires.
    addStaticGhosts();

    // phase H: close the database. fires.
    m_gameEventManager->addEvent(new PostRemoteEvent(ECNetwork, 4, false, 0, 0, 0.0f));
}

// Resets ghost scopes, then selects objects relevant to the connection's camera and tools.
// Recording connections include every object. Commander overhead view replaces the player view.
// Satellite scanning covers the map and takes precedence over the UAV radius;
// commander-specific objects are added independently.
void ServerConnection::updateGhostManager(float dt)
{
    world::IPlayer* const player = getPlayer();
    m_ghostManager->resetGhostStates(false);                             // the mark phase - first

    if (getIsRecordingConnection())                                      // a demo has no viewpoint
    {
        getAllObjects();
        return;
    }
    if (!player || !player->getCamera())
        return;

    Mat4 view;
    cameraTransform(player->getCamera(), view);                          // slot +0x10, arg 0.0f

    if (player->getOverheadCameraTopDownView())                          // +0xC4
    {
        ISquadManager* const squads = static_cast<ISquadManager*>(squadManager);
        world::IObject* const cam = static_cast<world::IObject*>(
            squads->getCommanderCam(player->getTeam()));                 // ISquadManager +0x74
        if (cam)
        {
            world::ICameraObject* const cameraObject = static_cast<world::ICameraObject*>(
                cam->queryInterface(world::IID_ICameraObject));
            cameraTransform(cameraObject, view);                         // overwrites - see above
        }
    }

    getRelevantObjects(view, dt);

    // Commander-tool scoping requires an existing manager.
    if (world::commanderToolsManager)
    {
        Vec3 centre;
        centre.x = centre.y = centre.z = 0.0f;

        if (player->getIsCommander() &&
            commanderToolsSatelliteScanActive(player->getTeam()))
        {
            getRelevantEnemyObjects(centre, -1.0f);                      // Negative radius selects the whole map.
        }
        else if (commanderToolsUAVActive(player->getTeam(), &centre))
        {
            getRelevantEnemyObjects(centre, gameLogic->getUAVRadius());
        }

        if (player->getIsCommander())                                    // +0x114
            getSpecialCommanderObjects(player->getTeam());
    }
}

// Updates the controlled ghost and its position-compression reference. Uses the
// commander camera while overhead view is active. Returns false without a player,
// vehicle, or positive frame count; a missing control descriptor is fatal.
bool ServerConnection::updatePlayerControlObject(int frameCount)
{
    world::IPlayer* const player = getPlayer();
    if (!player || !player->getVehicle() || frameCount <= 0)
        return false;

    world::IObject* const vehicle = player->getVehicle();   // the second fetch, as shipped
    io::NetworkableDescriptor* const descriptor =
        io::networkManager->getDescriptor(networkableNetId(vehicle->getNetworkable()));
    if (!descriptor)
    {
        Debug(DEBUGTYPE_6, "Game/Common/ServerConnection.cpp", 98)
            << "Control object not found";
        // type 6 aborts; the dereference below is unreachable on this arm, as shipped.
    }

    io::INetworkableObject* const objEntry =
        descriptor->properties[0].networkables[0]->m_networkableObject;
    if (objEntry)
    {
        world::IObject* const root = world::getRootParent(objEntry->m_object);
        m_ghostManager->setCompressionVector(root->getAbsolutePosition());   // slot 27
    }

    if (player->getOverheadCameraTopDownView())              // +0xC4 - the commander override
    {
        ISquadManager* const squads = static_cast<ISquadManager*>(squadManager);
        world::IObject* const cam = static_cast<world::IObject*>(
            squads->getCommanderCam(player->getTeam()));     // ISquadManager +0x74
        if (cam)
        {
            world::ICameraObject* const cameraObject = static_cast<world::ICameraObject*>(
                cam->queryInterface(world::IID_ICameraObject));
            const Mat4& camTransform = cameraObject->getTransformation(0.0f);   // +0x10
            // the translation row - `add eax, 30h` in the ELF, floats [12..14].
            m_ghostManager->setCompressionVector(
                reinterpret_cast<const Vec3&>(camTransform.m[12]));
        }
    }

    return m_ghostManager->updatePlayerControlObject(player, frameCount);
}

// Scores a descriptor using view-cone relevance, distance falloff and the object's speed.
// Null, player-owned and out-of-range descriptors score zero. rawOnly skips base-priority scaling.
// Viewer velocity is unused; each evaluation increments the per-frame counter.
float ServerConnection::calculateObjectPriority(io::NetworkableDescriptor* descriptor,
                                                float fovThreshold, world::IPlayer* player,
                                                Mat4& viewTransform, const Vec3& /* viewerVelocity - dead, see above */,
                                                float range, float rangeSq, bool rawOnly)
{
    ++m_dword17C;                                        // the per-frame evaluation counter

    if (!descriptor)
        return 0.0f;

    io::INetworkable* const networkable = descriptor->properties[0].networkables[0];
    io::INetworkableObject* const objEntry = networkable->m_networkableObject;   // unchecked

    if (objEntry->m_ownerId == player->getId())          // never priority-ghost your own object
        return 0.0f;

    world::IObject* const root = world::getRootParent(objEntry->m_object);
    const Vec3& objectPosition = root->getAbsolutePosition();               // slot 27
    const float* const view = viewTransform.m;

    // the range gate, squared - view minus object
    const float gx = view[12] - objectPosition.x;
    const float gy = view[13] - objectPosition.y;
    const float gz = view[14] - objectPosition.z;
    if (!(rangeSq >= gz * gz + gy * gy + gx * gx))
        return 0.0f;

    // the distance again, object minus view - see the note above
    const float dx = objectPosition.x - view[12];
    const float dy = objectPosition.y - view[13];
    const float dz = objectPosition.z - view[14];
    float distance = std::sqrt(dy * dy + dx * dx + dz * dz);
    if (!(distance >= 1.1920929e-07f))                   // the FLT_EPSILON guard
        distance = 0.001f;

    const float inv = 1.0f / distance;
    const float falloff = 1.0f - distance / range;

    float cone = 0.0f;
    if (dy * inv * view[9] + dx * inv * view[8] + dz * inv * view[10] >= fovThreshold)
        cone = 1.0f;
    if (!(distance >= 10.0f))                            // inside ten metres: always "seen"
        cone = 1.0f;

    // the scored object's speed, from its root's physics node
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(root));             // root +0x44
    if (node)
    {
        const Vec3& velocity = node->getPositionalSpeed();                  // +0x60
        vx = velocity.x; vy = velocity.y; vz = velocity.z;
    }
    float speed = (vx * vx + vy * vy + vz * vz) / 1600.0f;                  // 40 m/s = full weight
    if (1.0f <= speed)
        speed = 1.0f;

    const float priority = cone + falloff + speed;
    if (rawOnly)
        return priority;                                 // getRelevantObjects multiplies itself

    float bonus = 0.0f;
    if (objEntry->m_object)
    {
        bonus = objEntry->m_object->queryInterface(world::IID_IProjectileObject) != 0
                    ? 0.30000001f : 0.0f;                // projectiles get the same +0.3 as below
    }
    return (priority + bonus) * networkable->m_basePriority;                // +0x10
}

// Scopes root groups by shared camera priority, giving player-owned groups maximum priority.
// Positive-priority projectiles gain a bonus. Living teammates' ghostable vehicles retain
// a minimum priority without lowering a score already assigned by the camera pass.
void ServerConnection::getRelevantObjects(Mat4& viewTransform, float range)
{
    world::IPlayer* const player = getPlayer();
    if (!player)
        return;

    m_dword17C = 0;                                      // the counter reset - see above

    Vec3 rootVelocity;
    rootVelocity.x = rootVelocity.y = rootVelocity.z = 0.0f;
    if (player->getVehicle())
    {
        world::IObject* const vehicle = player->getVehicle();   // the second fetch, as shipped
        world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
            world::object_detail::LayoutAccess::physicsNode(world::getRootParent(vehicle)));
        if (node)
            rootVelocity = node->getPositionalSpeed();   // dead downstream - see above
    }

    const float fovThreshold = player->getFov() * 0.5f * 1.3333334f;

    typedef std::list<std::pair<io::INetworkableObject*, io::NetworkableDescriptor*> > Group;
    std::list<Group>& roots = io::networkManager->getScopableRootDescriptors();

    for (std::list<Group>::iterator group = roots.begin(); group != roots.end(); ++group)
    {
        float priority = -1.0f;                          // "not scored yet"

        // first walk: is any member of this group mine?
        for (Group::iterator it = group->begin(); it != group->end(); ++it)
        {
            if (it->first->m_ownerId == player->getId())
            {
                priority = 9999.0f;                      // my own vehicle's group
                break;
            }
        }

        // second walk: ghost every member that is not mine
        for (Group::iterator it = group->begin(); it != group->end(); ++it)
        {
            io::INetworkableObject* const objEntry = it->first;
            if (objEntry->m_ownerId == player->getId())
                continue;                                // my own seat: the control channel's job

            io::NetworkableDescriptor* const descriptor = it->second;
            if (priority < 0.0f)
            {
                // scored once, on the first not-mine member, and shared by the group.
                priority = calculateObjectPriority(descriptor, fovThreshold, player,
                                                   viewTransform, rootVelocity,
                                                   range, range * range, true);
            }
            if (priority <= 0.0f)
                break;                                   // drops the rest of the group

            float bonus = 0.0f;
            if (objEntry->m_object)
            {
                bonus = objEntry->m_object->queryInterface(world::IID_IProjectileObject) != 0
                            ? 0.30000001f : 0.0f;
            }
            m_ghostManager->ghostObject(
                descriptor,
                (bonus + priority)
                    * descriptor->properties[0].networkables[0]->m_basePriority,   // +0x10
                false);
        }
    }

    // living teammates' vehicles, at the 0.03 floor with keepHigher
    const std::list<world::IPlayer*>& teammates =
        world::playerManager->getPlayers(player->getTeam());                // +0x48, the team overload
    for (std::list<world::IPlayer*>::const_iterator it = teammates.begin();
         it != teammates.end(); ++it)
    {
        world::IPlayer* const teammate = *it;
        if (!teammate->getIsAlive())
            continue;
        if (teammate == player)
            continue;

        io::INetworkable* const networkable =
            teammate->getVehicle()->getNetworkable();    // unchecked derefs, as shipped
        if (!networkable->m_ghostActive)                 // +0x0C
            continue;
        if (!(networkable->m_basePriority > 0.0f))       // +0x10 - destroyables' -1 keeps them out
            continue;

        m_ghostManager->ghostObject(
            io::networkManager->getDescriptor(networkable->m_networkableId),
            0.03f, true);                                // keepHigher - never lowers a score
    }
}

// Scopes living enemy vehicles within the disc at priority 0.5, retaining higher
// priority. Negative radius disables distance filtering. Only teams 1/2 are
// supported; preserve repeated team and vehicle reads.
void ServerConnection::getRelevantEnemyObjects(const Vec3& centre, float radius)
{
    world::IPlayer* const player = getPlayer();
    if (!player)
        return;

    int enemyTeam;
    if (player->getTeam() == 1)
    {
        enemyTeam = 2;
    }
    else
    {
        if (player->getTeam() != 2)                      // the second getTeam(), as shipped
            return;
        enemyTeam = 1;
    }

    const float radiusSq = (radius > 0.0f) ? radius * radius : -1.0f;   // negative = no limit

    const std::list<world::IPlayer*>& enemies =
        world::playerManager->getPlayers(enemyTeam);     // +0x48, the team overload
    for (std::list<world::IPlayer*>::const_iterator it = enemies.begin();
         it != enemies.end(); ++it)
    {
        world::IPlayer* const enemy = *it;
        if (!enemy->getIsAlive())
            continue;
        if (enemy == player)                             // dead - see above
            continue;

        io::INetworkable* const networkable =
            enemy->getVehicle()->getNetworkable();       // unchecked derefs, as shipped
        if (!networkable->m_ghostActive)                 // +0x0C
            continue;
        if (!(networkable->m_basePriority > 0.0f))       // +0x10
            continue;

        bool inRange = radiusSq <= 0.0f;                 // the scan's -1 short-circuits here
        if (!inRange)
        {
            const Vec3& position =
                enemy->getVehicle()->getAbsolutePosition();   // the second fetch, slot 27
            const float dx = position.x - centre.x;
            const float dy = position.y - centre.y;
            const float dz = position.z - centre.z;
            inRange = dz * dz + dy * dy + dx * dx <= radiusSq;
        }
        if (inRange)
        {
            m_ghostManager->ghostObject(
                io::networkManager->getDescriptor(networkable->m_networkableId),
                0.5f, true);                             // keepHigher again
        }
    }
}

// Scopes one new descriptor using the player's camera, vehicle velocity, and field
// of view. Ghosts only positive priorities. Recording connections use priority 1;
// normal connections without a camera or vehicle do not use that fallback.
void ServerConnection::addScopableGhost(io::NetworkableDescriptor* descriptor, float range)
{
    Mat4 view = Mat4::getIdentity();                     // the zero-plus-diagonal build

    world::IPlayer* const player = getPlayer();
    if (player && player->getCamera() && player->getVehicle())
    {
        cameraTransform(player->getCamera(), view);      // slot +0x10, arg 0.0f

        const float fovThreshold = player->getFov() * 0.5f * 1.3333334f;

        Vec3 rootVelocity;
        rootVelocity.x = rootVelocity.y = rootVelocity.z = 0.0f;
        world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
            world::object_detail::LayoutAccess::physicsNode(
                world::getRootParent(player->getVehicle())));
        if (node)
            rootVelocity = node->getPositionalSpeed();   // dead downstream, as everywhere

        const float priority = calculateObjectPriority(descriptor, fovThreshold, player,
                                                       view, rootVelocity,
                                                       range, range * range, false);
        if (priority > 0.0f)
            m_ghostManager->ghostObject(descriptor, priority, false);
    }
    else if (getIsRecordingConnection())
    {
        m_ghostManager->ghostObject(descriptor, 1.0f, false);   // a demo scopes everything
    }
}

} // namespace hfe
} // namespace dice
