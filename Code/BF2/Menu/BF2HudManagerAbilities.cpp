// Ability display. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "BF2HudManager.h"
#if !DEDICATED
#include "Bf2MenuMapNode.h"
#include "HudCommander.h"
#include "HudArtillerySeams.h"
#include "../AI/AIInterface/AIInstance.h"
#include "../AI/AIInterface/SingletonServer.h"
#include "../Core/EventManager.h"
#include "../Core/System.h"
#include "../Game/IGameLogic.h"
#include "../Game/GameEvents/UAVEvent.h"
#include "../Game/GameEvents/ArtilleryEvent.h"
#include "../Game/GameEvents/DropVehicleEvent.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/Game/SupplyDropManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Sound/BfSound.h"
#include <cfloat>
#include <new>
#if !defined(_M_IX86)
#include <cmath>
#include <cstring>
#endif

namespace dice { namespace hfe {
extern IBase* squadManager;
namespace hud {
namespace {
// [INFERRED] spelling of Vec2 inequality5EA630. The sum is retained in x87
// precision, and unordered values take the same arm as a sum above epsilon.
bool hasAbilityPosition(const Vec2& position)
{
    const float sentinel = -4000.0f;
    const float epsilon = FLT_EPSILON;
#if defined(_M_IX86)
    bool valid;
    __asm {
        mov ecx, position
        fld dword ptr [ecx+4]
        fsub sentinel
        fabs
        fld dword ptr [ecx]
        fsub sentinel
        fabs
        faddp st(1), st(0)
        fcomp epsilon
        fnstsw ax
        test ah, 41h
        setp valid
    }
    return valid;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Vec2 +4 is y and +0 x. setp after test ah,41h: valid above epsilon or unordered.
    const double sum = std::fabs(static_cast<double>(position.y) - sentinel)
                     + std::fabs(static_cast<double>(position.x) - sentinel);
    return !(sum <= epsilon);
#endif
}

// Preserve both the unrounded x87 sum and the two producers' distinct store
// order. These helpers name local expressions, not new engine interfaces.
void beginSupplyCooldown(double now, const float& duration, float& remaining,
                         bool& reloading, float& deadline)
{
#if defined(_M_IX86)
    __asm {
        fld now
        mov eax, duration
        fadd dword ptr [eax]
        mov ecx, [eax]
        mov edx, remaining
        mov [edx], ecx
        mov eax, reloading
        mov byte ptr [eax], 1
        mov ecx, deadline
        fstp dword ptr [ecx]
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Duration is read once, before the raw copy; the shipped store order is kept.
    const float length = duration;
    const double sum = now + length;
    std::memcpy(&remaining, &length, sizeof remaining);
    reloading = true;
    deadline = static_cast<float>(sum);
#endif
}

void beginVehicleCooldown(double now, const float& duration, float& remaining,
                          float& deadline, bool& reloading)
{
#if defined(_M_IX86)
    __asm {
        fld now
        mov eax, duration
        fadd dword ptr [eax]
        mov ecx, [eax]
        mov edx, remaining
        mov [edx], ecx
        mov eax, deadline
        fstp dword ptr [eax]
        mov ecx, reloading
        mov byte ptr [ecx], 1
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Duration is read once, before the raw copy; the shipped store order is kept.
    const float length = duration;
    const double sum = now + length;
    std::memcpy(&remaining, &length, sizeof remaining);
    deadline = static_cast<float>(sum);
    reloading = true;
#endif
}
}

void BF2HudManager::artillery()
{
    if (launchArtillery(false)) {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            for (int squad = 0; squad < 10; ++squad) {
                const int team = player->getTeam();
                static_cast<ISquadManager*>(squadManager)->setIsAccepted(
                    team, squad, false, static_cast<OrderType>(2),
                    static_cast<SquadOrderType>(0), false);
            }
        }
        disableVOHelpMessage(16);
    }
    // Retail plays this even when launch fails (and twice on a successful launch).
    g_bfSound->playSound(std::string("mapAction"));
}

bool BF2HudManager::launchArtillery(bool silent)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    if (!(getArtilleryEnabled() && getArtilleryAvailability() > 0.0f)) {
        if (!silent)
            game->localGameInfoMessage(static_cast<TargetChannel>(2),
                                      std::string("artillery_unavailable"));
        return false;
    }
    if (!m_field13c || m_field13c->m_artilleryReloading ||
        !hasAbilityPosition(getCommanderPosition(false))) {
        if (!silent)
            game->localGameInfoMessage(static_cast<TargetChannel>(2),
                                      std::string("artillery_in_use"));
        return false;
    }
    g_bfSound->playSound(std::string("mapAction"));
    void* storage = ::operator new(sizeof(ArtilleryEvent));
    ArtilleryEvent* event = nullptr;
    if (storage) {
        // Position is evaluated before the captured player's repeated team call.
        const Vec2 position = getCommanderPosition(false);
        const int team = player->getTeam();
        event = new(storage) ArtilleryEvent(team, position, 0u);
    }
    if (!gameLogic->isServer()) {
        game->sendGameEvent(static_cast<unsigned int>(-1), event);
        return true;
    }
    eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    if (ai::g_aiInstance && ai::aiInstance()->isInitialized()) {
        ai::IAIEventCourier* courier = ai::BFSingletonServer::m_iaiEventCourier;
        const Vec2 position = getCommanderPosition(false);
        const int team = player->getTeam();
        ai::dispatchArtilleryRequest(courier, team, position);
    }
    return true;
}

bool BF2HudManager::satellite(bool silent)
{
    if (!isCommanderShown())
        return false;
    setMode(15, true);
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    if (!(getSatelliteEnabled() && getSatelliteAvailability() > 0.0f)) {
        // The unavailable arm ignores silent; only the busy arm honors it.
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "satellite_unavailable");
        return false;
    }
    if (hudMap && !hudMap->m_satelliteReloading
        && player->getSatelliteIsReady()) {
        void* storage = ::operator new(sizeof(UAVEvent));
        UAVEvent* event = nullptr;
        if (storage) {
            const Vec2 position = getCommanderPosition(false);
            const int team = player->getTeam();
            event = new(storage) UAVEvent(team, position, true);
        }
        if (gameLogic->isServer())
            eventManager->postEventNextUpdate(ECGameLogic, 15, event);
        else
            game->sendGameEvent(static_cast<unsigned>(-1), event);
        // The shipped routine reports false after either successful handoff.
        return false;
    }
    if (!silent)
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "satellite_in_use");
    return false;
}

bool BF2HudManager::supplies(bool silent)
{
    if (!hudMap)
        return false;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    if (m_field13c && !m_field13c->m_supplyReloading
        && hasAbilityPosition(getCommanderPosition(false))) {
        g_bfSound->playSound(std::string("mapAction"));
        Vec2 position = getCommanderPosition(false);
        const int team = player->getTeam();
        if (!world::supplyDropManager->requestSupplyDrop(static_cast<unsigned char>(team), position))
            return false;
        const double now = WorldPref::getWorldTime();
        HudCommander* commander = m_field13c;
        beginSupplyCooldown(now, commander->m_supplyDuration, commander->m_supplyRemaining,
                            commander->m_supplyReloading, commander->m_supplyDeadline);
        for (int squad = 0; squad < 10; ++squad) {
            const int currentTeam = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->setIsAccepted(
                currentTeam, squad, false, static_cast<OrderType>(2),
                static_cast<SquadOrderType>(3), false);
        }
        disableVOHelpMessage(17);
        return true;
    }
    if (!silent)
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "supplies_in_use");
    return false;
}

bool BF2HudManager::uav(bool silent)
{
    if (!hudMap)
        return false;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    if (!(getUAVEnabled() && getUAVAvailability() > 0.0f)) {
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "uav_unavailable");
        return false;
    }
    if (hudMap && !hudMap->m_uavReloading && player->getUAVIsReady()
        && hasAbilityPosition(getCommanderPosition(false))) {
        g_bfSound->playSound(std::string("mapAction"));
        void* storage = ::operator new(sizeof(UAVEvent));
        UAVEvent* event = nullptr;
        if (storage) {
            const Vec2 position = getCommanderPosition(false);
            const int team = player->getTeam();
            event = new(storage) UAVEvent(team, position, false);
        }
        if (gameLogic->isServer())
            eventManager->postEventNextUpdate(ECGameLogic, 15, event);
        else
            game->sendGameEvent(static_cast<unsigned>(-1), event);
        enableVOHelpMessage(20);
        disableVOHelpMessage(18);
        for (int squad = 0; squad < 10; ++squad) {
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->setIsAccepted(
                team, squad, false, static_cast<OrderType>(2),
                static_cast<SquadOrderType>(1), false);
        }
        return true;
    }
    if (!silent)
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "uav_in_use");
    return false;
}

bool BF2HudManager::vehicleDrop(bool silent)
{
    if (!hudMap || !(m_field2d0 > 0.0f))
        return false;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    // Unlike supplies, retail does not guard the Commander owner here.
    if (!m_field13c->m_vehicleDropReloading
        && hasAbilityPosition(getCommanderPosition(false))) {
        g_bfSound->playSound(std::string("mapAction"));
        const Vec2 first = getCommanderPosition(false);
        const Vec2 second = getCommanderPosition(false);
        const Vec3 position = {second.x, 0.0f, first.y};
        if (!isVehicleDropPositionValid(position))
            return false;
        const double now = WorldPref::getWorldTime();
        HudCommander* commander = m_field13c;
        beginVehicleCooldown(now, commander->m_vehicleDropDuration,
                             commander->m_vehicleDropRemaining, commander->m_vehicleDropDeadline,
                             commander->m_vehicleDropReloading);
        void* storage = ::operator new(sizeof(DropVehicleEvent));
        DropVehicleEvent* event = nullptr;
        if (storage) {
            const Vec2 destination = getCommanderPosition(false);
            const int team = player->getTeam();
            event = new(storage) DropVehicleEvent(team, destination);
        }
        if (gameLogic->isServer())
            eventManager->postEventNextUpdate(ECGameLogic, 15, event);
        else
            game->sendGameEvent(static_cast<unsigned>(-1), event);
        for (int squad = 0; squad < 10; ++squad) {
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->setIsAccepted(
                team, squad, false, static_cast<OrderType>(2),
                static_cast<SquadOrderType>(2), false);
        }
        return true;
    }
    if (!silent)
        game->localGameInfoMessage(static_cast<TargetChannel>(14), "vehicledrop_in_use");
    return false;
}

bool BF2HudManager::isVehicleDropPositionValid(const Vec3& position)
{
    world::ObjectFlagPredicator predicate(0x100);
    const Vec3 origin = {position.x, 2000.0f, position.z};
    const Vec3 direction = {0.0f, -4500.0f, 0.0f};
    std::vector<world::IObject*> objects;
    world::objectManager->setOverrideLineTestLod(1);
    world::objectManager->getObjectsFromGrid(origin, direction, objects, &predicate, true, 0, false);
    for (auto object = objects.begin(); object != objects.end(); ++object) {
        world::IObject* root = world::getRootParent(*object);
        if (!root)
            continue;
        auto* invalid = gameLogic->GetInvalidVehicleDropObjectList();
        for (auto entry = invalid->begin(); entry != invalid->end(); ++entry) {
            const world::IObjectTemplate* objectTemplate = root->getTemplate();
            if (objectTemplate && entry->first == objectTemplate->getName()
                && *reinterpret_cast<const unsigned char*>(&entry->second) == 1) {
                // Shipped defect: this early return leaves the override at1.
                return false;
            }
        }
    }
    world::objectManager->setOverrideLineTestLod(-1);
    return true;
}
} } }
#endif
