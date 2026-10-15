// Spotted-object handling. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "BF2HudManager.h"
#if !DEDICATED
#include "../Core/Ids.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/Game/CommanderToolsManager.h"
#include "../IO/Network/NetworkManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include <cfloat>
#include <cmath>
#include <windows.h>
namespace dice { namespace hfe { namespace hud {
namespace {
bool invalidSpottedPosition(const Vec3& position) // complete Vec3 comparison53C200
{
    return std::fabs(static_cast<double>(position.x) + 4000.0) <= FLT_EPSILON
        && std::fabs(static_cast<double>(position.y) + 4000.0) <= FLT_EPSILON
        && std::fabs(static_cast<double>(position.z) + 4000.0) <= FLT_EPSILON;
}
}

void BF2HudManager::updateOrderPositionColor(const Vec3& position)
{
    m_field35c = false;
    m_field350 = 1.0f;
    m_field354 = 1.0f;
    m_field358 = 1.0f;
    // 9EDDA0 is CommanderToolsManager::m_instance; ECX+4 selects team state0.
    // The folded7B3230 body also implements IsVehiclePermittedtoLand(Vec3&)
    // and does not change position. Retain its actual shared owner/call.
    if (world::commanderToolsManager && !world::commanderToolsManager->getToolStateForTeam(1)
            ->IsVehiclePermittedtoLand(const_cast<Vec3&>(position))) {
        m_field35c = true;
        m_field350 = 0.6f;
        m_field354 = 0.6f;
        m_field358 = 0.6f;
    }
}

void BF2HudManager::sendSpottedMessage(bool announce, world::VehicleType type)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!local) return;
    world::IObject* object = io::networkManager->getObject(m_field1e4);
    if (object) {
        auto* control = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        if (control) {
            world::IPlayer* occupant = control->getOccupingPlayer();
            if (!occupant) {
                const std::map<int, world::IPlayerControlObject*> pcos(*control->getPcos());
                for (auto it = pcos.begin(); it != pcos.end(); ++it) {
                    occupant = it->second->getOccupingPlayer(); // No null-seat guard.
                    if (occupant) break;
                }
            }
            if (occupant && occupant->getIsManDown()) {
                // Shipped BF2142-install check. The opened handle is leaked and
                // this arm bypasses spam filtering and radio announcement.
                HKEY installedKey;
#if defined(_M_IX86)
                const REGSAM installedAccess = KEY_QUERY_VALUE;
#else
                // [CHOSEN, not recovered] x64 (issue #7): the 32-bit installer's registry view.
                const REGSAM installedAccess = KEY_QUERY_VALUE | KEY_WOW64_32KEY;
#endif
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2142\\InstallDir",
                        0, installedAccess, &installedKey) == ERROR_SUCCESS) {
                    if (m_field1e4 || !invalidSpottedPosition(m_field1e8))
                        game->sendSpottedMessage(local, m_field1e4,
                            Vec2{m_field1e8.x, m_field1e8.z}, type);
                }
                cacheMouseInput(true);
                return;
            }
        }
    }
    if ((m_field1e4 || !invalidSpottedPosition(m_field1e8)) && !local->getIsThisASpam(false))
        game->sendSpottedMessage(local, m_field1e4, Vec2{m_field1e8.x, m_field1e8.z}, type);
    if (announce) {
        enableVOHelpMessage(50);
        // The player/recipient/flags pushed before getSpottedMessage belong to
        // this outer radio call. Its string producer still takes only the ID.
        game->radioMessage(static_cast<TargetChannel>(1), game->getSpottedMessage(m_field1e4),
                           local, 0, true, false);
    } else if (getMode() == 6) {
        setMode(getPreviousMode(), true);
    }
    cacheMouseInput(true);
}
} } }
#endif
