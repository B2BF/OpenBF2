// Per-frame HUD and pending-request updates. [INFERRED] Source split.
#include "BF2HudManager.h"
#if !DEDICATED
#include "HudVoip.h"
#include "Ref2System.h"
#include "Bf2MenuMapNode.h"
#include "HudAmmo.h"
#include "HudVehicleInformation.h"
#include "HudInformationAdapter.h"
#include "HudKitSelection.h"
#include "HudSquad.h"
#include "HudCommander.h"
#include "HudScoreboard.h"
#include "HudVoteMap.h"
#include "HudMessages.h"
#include "HudChat.h"
#include "HudBlinker.h"
#include "HudQuestionManager.h"
#include "../IO/Input/FalconDevice.h"
#include "../Game/IGameLogic.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/GameLogic/SpawnSystem/SpawnManager.h"
#include "../Game/GameLogic/SpawnSystem/SpawnGroup.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Core/System.h"
#include "../Core/SettingsRepostitory.h"
#if !defined(_M_IX86)
#include <cmath>
#endif

namespace dice { namespace hfe {
class IMenuLogic;
extern IMenuLogic* menuLogic;
extern IBase* spawnManager;
extern IBase* squadManager;
namespace hud {
namespace {
// Retail stores the timer but compares the retained x87 result, before float
// rounding. Inclusive expiry uses ordered <=0; icon expiry uses ordered <0.
bool subtractTimer(float& timer, float delta, bool inclusive)
{
#if defined(_M_IX86)
    bool expired;
    __asm {
        mov ecx, timer
        fld dword ptr [ecx]
        fsub delta
        fst dword ptr [ecx]
        ftst
        fnstsw ax
        fstp st(0)
        cmp inclusive, 0
        je strict_expiry
        test ah, 41h
        setnp expired
        jmp expiry_done
    strict_expiry:
        test ah, 5
        setnp expired
    expiry_done:
    }
    return expired;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // setnp: inclusive expiry is an ordered <= 0, strict expiry an ordered < 0.
    const double remaining = static_cast<double>(timer) - delta;
    timer = static_cast<float>(remaining);
    return inclusive ? remaining <= 0.0 : remaining < 0.0;
#endif
}
bool addDemoTimer(float& timer, float delta)
{
    const float threshold = 10.0f;
#if defined(_M_IX86)
    bool due;
    __asm {
        mov ecx, timer
        fld delta
        fadd dword ptr [ecx]
        fst dword ptr [ecx]
        fcomp threshold
        fnstsw ax
        test ah, 1
        setz due
    }
    return due;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // C0 clear means an ordered sum at or above the threshold.
    const double elapsed = static_cast<double>(delta) + timer;
    timer = static_cast<float>(elapsed);
    return elapsed >= threshold;
#endif
}
std::string requestInUseMessage(int type)
{
    switch (type) {
    case 0: return "artillery_in_use";
    case 1: return "uav_in_use";
    case 2: return "vehicledrop_in_use";
    case 3: return "supplies_in_use";
    default: return "";
    }
}
} // namespace

bool BF2HudManager::update(float delta, world::IPlayer* player, Vec3 direction)
{
    if (menuLogic) return false;
    if (!m_field15c) {
        if (io::g_falconMouseMode != -1) io::g_falconMouseMode = 0;
    } else if (io::g_falconMouseMode != 2) io::g_falconMouseMode = 1;

    if (gameLogic && gameLogic->getGameStatus() != 3 && player && player->getVehicle()) {
        if (!player->getIsAlive() && hudMap) {
            const int groupId = m_field11c->m_selectedSpawnGroup[m_field11c->m_selectedTeam];
            ISpawnGroup* group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(groupId);
            if (m_field228 && m_field2a8 >= 0.0f) {
                bool countDown = !group;
                if (group) {
                    if (!group->getActive(false)) countDown = true;
                    else {
                        const int groupTeam = group->getTeam();
                        countDown = groupTeam != player->getTeam();
                    }
                }
                if (countDown) m_field2a8 -= delta;
            }
            // Retail deliberately repeats this virtual query.
            if (getMode() == 0 && getMode() != 1 && (!m_field24a || m_field2a8 <= 0.0f)) {
                m_field2a8 = m_field2ac;
                setMode(1, true);
                gameLogic->selectSpawnGroup(player, 0);
                setDone(false);
            }
        }
        if (ref2System) {
            if (hudMap) {
                updateMapPosition(player, direction);
                hudMap->update(delta);            // +E0 77C330 - was dispatchMapUpdate
            }
            if (hudAmmo) hudAmmo->update(delta);
            if (hudVehicleInformation) hudVehicleInformation->update(delta);
            ref2System->update(delta);
            if (hudChat) hudChat->update(delta);
            if (m_field104->getBool("ShowRankUpIcon") && subtractTimer(m_field234, delta, false))
                m_field104->setBool("ShowRankUpIcon", false);
            if (m_field104->getBool("ShowMedalIcon") && subtractTimer(m_field238, delta, false))
                m_field104->setBool("ShowMedalIcon", false);
            if (m_field104->getBool("IngameHelpShow") && subtractTimer(m_field22c, delta, false))
                m_field104->setBool("IngameHelpShow", false);
        }
        if (!m_field0bf || gameLogic->getGameStatus() == 4) {
            updateModeWidgets(delta);
            updateNotifications(delta);
            tickChatHook(delta);
        }
        if (m_field2f5) m_field2f8 = delta + m_field2f8;
        if (getMode() == 25) {
            const Vec2 pointer = hudMap->getPointerMapPosition(ref2System);
            Vec3 ground = {0.0f, 0.0f, 0.0f};
            meme::mapCoordinatesToWorld(ground, pointer);
            const bool blocked = isVehicleDropPositionBlocked(ground);
            const int mode = getMode();
            enablePointer(blocked ? m_field178 : m_field174, mode);
        }
        return true;
    }
    if (m_field248) ref2System->update(delta);
    if (game && game->isPlayingDemo()) {
        updateDemoHelp(delta);
        if (ref2System && m_field104->getBool("IngameHelpShow") && subtractTimer(m_field22c, delta, false))
            m_field104->setBool("IngameHelpShow", false);
    }
    return false;
}

void BF2HudManager::updateModeWidgets(float delta)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    switch (getMode()) {
    case 0:
        if (!player) return;
        // 750385..7503D7. The squad test is `!= 0`, not `> 0`: 7503A0 is
        // `test eax,eax` followed by `ja`, so a negative squad id also counts
        // as being in a squad.
        if (m_field124 && (player->getSquadId() != 0 || player->getIsCommander())
            && m_field124->m_enabled) {
            m_field124->update(true);
        } else {
            // This call requires a live owner even on the branch where its earlier test is false.
            m_field124->update(false);
        }
        break;
    case 1:
        if (m_field138) m_field138->refreshTitle();
        break;
    case 15: case 16: case 21: case 22: case 23: case 24: case 25:
        if (m_field13c) m_field13c->update(true);
        if (m_field140) m_field140->update(delta);
        break;
    case 14: case 17: case 18: case 26:
        if (m_field138) m_field138->setVisible(true);
        break;
    case 9: case 12:
        if (m_field120) m_field120->update(true);
        if (m_field128) m_field128->show(true);
        break;
    case 10:
        if (!m_field148 || !m_field148->update(delta)) setMode(getPreviousMode(), false);
        break;
    case 11: showRoundResult(); break;
    default: break;
    }
    // No local-player null guard in retail's common tail.
    if (m_field13c && player->getIsCommander()) m_field13c->refreshAbilities();
}

void BF2HudManager::updateNotifications(float delta)
{
    world::playerManager->getLocalHumanPlayer();
    m_field2dc->update(delta);
    updatePendingRequests(delta);
    if (m_field2b8 > 0.0f) m_field2b8 -= delta;
    else m_field2b0 = false;
    m_field100->update(delta);
}

bool BF2HudManager::updatePendingRequests(float delta)
{
    if (m_field32c.empty()) return true;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return false;
    auto request = m_field32c.begin();
    while (request != m_field32c.end()) {
        if (subtractTimer(request->remaining, delta, true)) {
            const int squad = request->squad;
            const SquadOrderType type = static_cast<SquadOrderType>(request->type);
            const int team = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->setIsAccepted(team, squad, false,
                static_cast<OrderType>(2), type, false);
            const std::string message = requestInUseMessage(request->type);
            sendSquadReply(request->squad, message, false);
            request = m_field32c.erase(request);
        } else ++request;
    }
    return true;
}

void BF2HudManager::updateDemoHelp(float delta)
{
    if (m_field23c >= 0.0f && addDemoTimer(m_field23c, delta)) {
        if (!m_field240) {
            showWideHelp(replaceWideInputKeys(getLocalizedString("HUD_HELP_BATTLERECORDER_controlsQ")), 5.0f);
            m_field240 = true;
            m_field23c = 0.0f;
        } else {
            showWideHelp(replaceWideInputKeys(getLocalizedString("HUD_HELP_BATTLERECORDER_controlsT")), 5.0f);
            m_field23c = -1.0f;
        }
    }
}

void BF2HudManager::updateMapPosition(world::IPlayer* player, Vec3 direction)
{
    if (!player || !player->getCamera() || !hudMap) return;
    world::ICameraObject* camera = player->getCamera();
    if (player->getIsCommander() && (hudMap->m_overheadZoomActive
        || (isCommanderShown() && m_field104->getMapZoom() == 1)))
        camera = getCameraObject(player->getTeam());
    if (!camera) return;
    const Mat4& transform = camera->getTransformation(0.0f);
    const float cameraX = transform.m[12], cameraZ = transform.m[14];
    float worldZ, worldX;
    {
        const std::string keyZ("GLSWorldSizeZ"), keyX("GLSWorldSizeX");
        const int zeroZ = 0, zeroX = 0;
        worldZ = static_cast<float>(settings->intGet(keyZ, zeroZ));
        worldX = static_cast<float>(settings->intGet(keyX, zeroX));
    }
    const float scaleZ = -1.0f / worldZ, scaleX = 1.0f / worldX;
    const float half = 0.5f;
    Vec2 position;
    float heading;
    if (player->getIsAlive()) {
        const float x = direction.x, z = direction.z;
#if defined(_M_IX86)
        __asm {
            fld x
            fld z
            fpatan
            fstp heading
            fld worldX
            fmul half
            fadd cameraX
            fmul scaleX
            fstp position.x
            fld worldZ
            fmul half
            fadd cameraZ
            fmul scaleZ
            fstp position.y
        }
#else
        // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates); the CRT atan2
        // replaces fpatan (st1 = x over st0 = z) and agrees after the float store.
        heading = static_cast<float>(std::atan2(static_cast<double>(x), static_cast<double>(z)));
        position.x = static_cast<float>((static_cast<double>(worldX) * half + cameraX) * scaleX);
        position.y = static_cast<float>((static_cast<double>(worldZ) * half + cameraZ) * scaleZ);
#endif
    } else {
        heading = 0.0f;
#if defined(_M_IX86)
        __asm {
            fld worldX
            fmul half
            fmul scaleX
            fstp position.x
            fld worldZ
            fmul half
            fmul scaleZ
            fstp position.y
        }
#else
        // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
        position.x = static_cast<float>(static_cast<double>(worldX) * half * scaleX);
        position.y = static_cast<float>(static_cast<double>(worldZ) * half * scaleZ);
#endif
    }
    hudMap->setPlayerPosition(position, heading);
}

bool BF2HudManager::isVehicleDropPositionBlocked(const Vec3& position)
{
    world::ObjectFlagPredicator predicate(0x100);
    const Vec3 origin = {position.x, 2000.0f, position.z};
    const Vec3 direction = {0.0f, -4500.0f, 0.0f};
    std::vector<world::IObject*> objects;
    world::objectManager->setOverrideLineTestLod(1);
    world::objectManager->getObjectsFromGrid(origin, direction, objects, &predicate, true, 0, false);
    for (auto object = objects.begin(); object != objects.end(); ++object) {
        world::IObject* root = world::getRootParent(*object);
        if (!root) continue;
        auto* invalid = gameLogic->GetInvalidVehicleDropObjectList();
        for (auto entry = invalid->begin(); entry != invalid->end(); ++entry) {
            const world::IObjectTemplate* objectTemplate = root->getTemplate();
            if (objectTemplate && entry->first == objectTemplate->getName()
                && *reinterpret_cast<const unsigned char*>(&entry->second) == 1) {
                // Shipped defect: this early return leaves the override at1.
                return true;
            }
        }
    }
    world::objectManager->setOverrideLineTestLod(-1);
    return false;
}
} } }
#endif
