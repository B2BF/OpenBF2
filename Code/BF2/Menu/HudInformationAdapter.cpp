// HUD information adapter. [INFERRED] Names and source placement.
#include "HudInformationAdapter.h"
#if !DEDICATED
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/IGameLogic.h"
#include "../Game/GameLogic/ScoreSystem/ScoreManager.h"
#include "../Game/GameLogic/ScoreSystem/TeamScore.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "BF2HudManager.h"
#include "Bf2MenuMapNode.h"
#include "HudKitSelection.h"
#include "../Scene/Object/PlayerManager.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/System.h"
#if defined(_WIN32)
#include <windows.h>
#elif (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../Platform/macOS/MacSystem.h"   // [CHOSEN, not recovered] MessageBoxA's counterpart
#endif
#include <new>
namespace dice { namespace hfe { namespace world { extern IBase* scoreManager; } } }
namespace dice { namespace hfe { namespace hud {
namespace {
HudInformationLayer* information()
{
    return static_cast<HudInformationLayer*>(g_hudInformationLayer);
}
bool registerBoolData(meme::System* system, bool* value,
                      const std::string& name, const std::string& container)
{
    // explicit allocation-null/constructor-result guard. The System
    // registration return value is ignored; this helper succeeds after construction.
    void* allocation = ::operator new(sizeof(meme::BoolRefData));
    meme::BoolRefData* data = allocation ? new(allocation) meme::BoolRefData(value, name.c_str()) : 0;
    if (data) {
        system->registerData(data, container.c_str());
        return true;
    }
    const std::string caption = formatString("Failed to register: %s in %s", name.c_str(), container.c_str());
#if (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
    macos::showMessageBox("Error RegisterData", caption.c_str());
#else
    MessageBoxA(0, "Error RegisterData", caption.c_str(), 0); // caption/text ordering is retail
#endif
    return false;
}
bool __fastcall executeConsoleCommand(const std::string& command)
{
    hudManager->queueConsoleCommand(command); // PE 0x78594B, virtual +35C
    return true;
}
std::wstring teamDisplayName(int team) // 0x787110; name [INFERRED]
{
    const std::string army = gameLogic->getTeamName(team);
    if (army == "MEC") return hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_ARMY_MEC");
    if (army == "US") return hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_ARMY_USMC");
    if (army == "CH") return hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_ARMY_CHINA");
    if (!army.empty()) {
        const std::string key = std::string("HUD_TEXT_MENU_SPAWN_ARMY_") + army;
        return hudManager->getLocalizedString(std::string(key.c_str()));
    }
    return std::wstring(L"");
}
std::wstring victoryHeadline() // 786080; [INFERRED] helper name
{
    int winningTeam;
    const int first = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(1)->m_tickets;
    if (first > static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(2)->m_tickets)
        winningTeam = 1;
    else {
        const int second = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(2)->m_tickets;
        if (second <= static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(1)->m_tickets)
            return hudManager->getLocalizedString("HUD_HUD_DRAW_STRING");
        winningTeam = 2;
    }
    std::string team(gameLogic->getTeamName(winningTeam).c_str());
    std::wstring text = hudManager->getLocalizedString("HUD_HUD_WIN_STRING");
    return hudManager->replaceWideTokenFromString(std::move(text), "#TEAMNAME#", std::move(team));
}
}
HudInformationAdapter::HudInformationAdapter() : m_system(0)
{
    eventManager->registerEventHandler(ECGame, this, 0);
    eventManager->registerEventHandler(ECHud, this, 0);
    m_field010 = 0;
    m_field014 = 0;
    m_field018 = 0;
    m_mapZoom = 1;
}
HudInformationAdapter::~HudInformationAdapter()
{
    eventManager->unregisterEventHandler(ECGame, this);
    eventManager->unregisterEventHandler(ECHud, this);
}
void HudInformationAdapter::setSystem(meme::System* system) { m_system = system; }
bool HudInformationAdapter::registerData()
{
    {
        const std::string container("MiddleLeft"), name("LeftShow"), source("SpawnShow");
        registerBoolData(m_system, information()->getBoolByName(source), name, container);
    }
    {
        const std::string container("TopLeft"), name("TopShow"), source("SpawnShow");
        registerBoolData(m_system, information()->getBoolByName(source), name, container);
    }
    return true;
}
bool HudInformationAdapter::registerFunctions()
{
    information()->registerStringFunction("executeConsoleCommand", executeConsoleCommand);
    return true; // Retail ignores duplicate-registration failure.
}
bool HudInformationAdapter::setBool(std::string name, bool value)
{
    bool* target = information()->getBoolByName(std::string(name.c_str()));
    if (!target) return false;
    *target = value;
    return true;
}
bool HudInformationAdapter::getBool(std::string name)
{
    bool* target = information()->getBoolByName(std::string(name.c_str()));
    return target ? *target : false;
}
bool HudInformationAdapter::setInt(std::string name, int value)
{
    int* target = information()->getIntByName(std::string(name.c_str()));
    if (!target) return false;
    *target = value;
    return true;
}
int HudInformationAdapter::getInt(std::string name)
{
    int* target = information()->getIntByName(std::string(name.c_str()));
    return target ? *target : 0;
}
bool HudInformationAdapter::setString(std::string name, std::string value)
{
    const char** target = information()->getStringByName(std::string(name.c_str()));
    if (!target) return false;
    information()->updateString(target, value.c_str());
    return true;
}
bool HudInformationAdapter::setWideString(std::string name, std::wstring value)
{
    const wchar_t** target = information()->getWideStringByName(std::string(name.c_str()));
    if (!target) return false;
    information()->updateWideString(target, value.c_str());
    return true;
}
float HudInformationAdapter::getFloat(std::string name)
{
    float* target = information()->getFloatByName(std::string(name.c_str()));
    return target ? *target : 0.0f;
}
bool HudInformationAdapter::setFloat(std::string name, float value)
{
    float* target = information()->getFloatByName(std::string(name.c_str()));
    if (!target) return false;
    *target = value;
    return true;
}
void HudInformationAdapter::handleEvent(EventCategory category, unsigned int, EventNode*, void*)
{
    if (category == ECGame) world::playerManager->getLocalHumanPlayer();
}
void HudInformationAdapter::setMapStatic(bool value)
{
    setBool("MapStatic", value);
    eventManager->postEvent(ECHud, 10u, static_cast<void*>(0), 0.0f);
}
void HudInformationAdapter::setCloseToMine(bool value)
{
    // The whole body: one setBool through +0x0C, its Boolean result discarded.
    // HudInformationLayer registers the same key against m_state.CloseToMine.
    setBool("CloseToMine", value);
}

void HudInformationAdapter::setMapZoom(int level)
{
    setInt("MapZoom", level); // The Boolean result is discarded before querying the HUD.
    if (!hudManager->getMap()->isExpandedMap()) {
        m_mapZoom = level;
    } else if (hudManager->isCommanderShown()) {
        m_field014 = level;
    } else {
        // Inlined source interpretation of complete helper74FEB0: one mode
        // query, then the two comparisons. No further virtual slot is implied.
        const int mode = hudManager->getMode();
        if (mode == 17 || mode == 18) m_field018 = level;
        else m_field010 = level;
    }
}
int HudInformationAdapter::getMapZoom()
{
    if (!hudManager->getMap()->isExpandedMap()) return m_mapZoom;
    if (hudManager->isCommanderShown()) return m_field014;
    const int mode = hudManager->getMode(); // complete helper74FEB0
    return mode == 17 || mode == 18 ? m_field018 : m_field010;
}
void HudInformationAdapter::setTeam(int team)
{
    const int enemy = team == 1 ? 2 : 1;
    if (gameLogic->getTeamName(1).empty()) gameLogic->setTeamName(1, "MEC");
    if (gameLogic->getTeamName(2).empty()) gameLogic->setTeamName(2, "US");
    const char* path = "Ingame/Flags/Icons/Hud/Score/%s/scoreBoard_Flag.tga";
    const std::string friendlyFlag = formatString(path, gameLogic->getTeamName(team).c_str());
    const std::string enemyFlag = formatString(path, gameLogic->getTeamName(enemy).c_str());
    const std::string team1Flag = formatString(path, gameLogic->getTeamName(1).c_str());
    const std::string team2Flag = formatString(path, gameLogic->getTeamName(2).c_str());
    const std::wstring friendlyName = teamDisplayName(team);
    const std::wstring enemyName = teamDisplayName(enemy);
    std::wstring team1Name = teamDisplayName(1);
    std::wstring team2Name = teamDisplayName(2);
    if (team1Name.empty()) team1Name = hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_ARMY_MEC");
    if (team2Name.empty()) team2Name = hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_ARMY_USMC");
    setString("FriendlyFlagIconPathString", friendlyFlag);
    setString("EnemyFlagIconPathString", enemyFlag);
    setWideString("FriendlyTeamNameString", friendlyName);
    setWideString("EnemyTeamNameString", enemyName);
    setString("Team1FlagIconPathString", team1Flag);
    setString("Team2FlagIconPathString", team2Flag);
    setWideString("Team1NameString", team1Name);
    setWideString("Team2NameString", team2Name);
}

void HudInformationAdapter::setSelectedTeam(int team)
{
    setBool("Team1Selected", team == 1);
    setBool("Team2Selected", team == 2);
}
bool HudInformationAdapter::setSelectedKit(int kit)
{
    bool result;
    for (int i = 0; i <= 7; ++i) {
        result = setBool(formatString("PlayerKitIcon%iSelectShow", i), i == kit);
        if (!result) break;
    }
    return result;
}
bool HudInformationAdapter::setKitIconCount(int count)
{
    bool visible = true;
    bool result;
    for (int i = 1; i <= 7; ++i) {
        if (i > count) visible = false;
        result = setBool(formatString("PlayerKitIcon%iShow", i - 1), visible);
        if (!result) break;
    }
    return result;
}

void HudInformationAdapter::updateVictory()
{
    setWideString("WinHeadlineString", victoryHeadline());
    const int first = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(1)->m_tickets;
    if (first > static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(2)->m_tickets) {
        if (world::playerManager->getLocalHumanPlayer()->getTeam() == 1) {
            setFloat("EnemyVictoryAlpha", 0.6f);
            setFloat("FriendlyVictoryAlpha", 1.0f);
        } else {
            setFloat("EnemyVictoryAlpha", 1.0f);
            setFloat("FriendlyVictoryAlpha", 0.6f);
        }
    } // Retail has no corresponding team-2/equal alpha arm.
}

void HudInformationAdapter::setHudMode(int mode) // 786260, complete two-switch transition
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    setBool("SetupShow", false);
    setBool("DemoRecInterfaceShow", false);
    setBool("DemoCameraInterfaceShow", false);
    switch (hudManager->getPreviousMode()) {
    case 0: setBool("VoipListShow", false); break;
    case 1:
        if (mode != 9 && mode != 12 && !hudManager->isChatActive())
            setBool("SpawnShow", false);
        break;
    case 2: case 10: case 13: case 14: case 22: case 23: case 24: case 25: case 28:
        break;
    case 3: setBool("SquadInterfaceShow", false); break;
    case 6:
        setBool("SpottedInterfaceShow", false);
        // Retail falls through to both radio flags.
    case 4: case 5:
        setBool("RadioInterfaceShow", false);
        setBool("RadioVehicleInterfaceShow", false);
        break;
    case 7: setBool("SquadLeaderInterfaceShow", false); break;
    case 8: setBool("CommanderInterfaceShow", false); break;
    case 9: case 12:
        setBool("ScoreboardShow", false);
        setBool("LevelsListShow", false);
        setBool("ServerInfoSelected", false);
        break;
    case 11:
        setBool("VictoryShow", false);
        setBool("VictoryRankShow", false);
        break;
    case 15:
        if (!hudManager->isCommanderMode(mode)) setBool("CommanderShow", false);
        break;
    case 16: setBool("CommanderRadioShow", false); break;
    case 17: case 18:
        if (mode != 9 && mode != 12 && !hudManager->isChatActive() &&
            mode != 20 && mode != 26 && mode != 13 && mode != 19)
            setBool("SpawnShow", false);
        break;
    case 19: setBool("MapMenuShow", false); break;
    case 20:
        setBool("SquadLeaderMenuShow", false);
        // Retail also closes the invite list.
    case 26: setBool("InviteListShow", false); break;
    case 21: setBool("CommanderMenuShow", false); break;
    case 27: setBool("ChoiceMenuShow", false); break;
    case 29: setBool("SetupShow", false); break;
    case 30: setBool("DemoCameraInterfaceShow", false); break;
    case 31: setBool("DemoRecInterfaceShow", false); break;
    default:
        setBool("MapShow", true);
        setBool("MapBorderShow", true);
        setBool("SpawnShow", false);
        setBool("RadioInterfaceShow", false);
        setBool("SpottedInterfaceShow", false);
        setBool("RadioVehicleInterfaceShow", false);
        setBool("SquadInterfaceShow", false);
        setBool("SquadLeaderInterfaceShow", false);
        setBool("CommanderInterfaceShow", false);
        setBool("MapMenuShow", false);
        setBool("SquadLeaderMenuShow", false);
        setBool("CommanderMenuShow", false);
        setBool("ChoiceMenuShow", false);
        setBool("CommanderRadioShow", false);
        setBool("ScoreboardShow", false);
        setBool("LevelsListShow", false);
        setBool("RenameSquadShow", false);
        setBool("VictoryShow", false);
        setBool("VictoryRankShow", false);
        setBool("VoipListShow", false);
        setBool("InviteListShow", false);
        setBool("CommanderShow", false);
        break;
    }
    switch (mode) {
    case 0:
        setBool("ScoreboardShow", false);
        setBool("ShowIngameHud", true);
        setBool("MapShow", true);
        setBool("MapBorderShow", true);
        setBool("VoipListShow", local && (local->getSquadId() || local->getIsCommander()));
        setBool("SpawnShow", false);
        setBool("CommanderInterfaceShow", false);
        setBool("CommanderShow", false);
        break;
    case 1: {
        setBool("ScoreboardShow", false);
        setBool("ShowIngameHud", true);
        setBool("MapBorderAlternateShow", true);
        setBool("SpawnShow", true);
        setBool("KitsShow", true);
        setBool("MembersShow", false);
        setKitIconCount(7);
        HudKitSelection* selection = hudManager->getKitSelection();
        const int selected = selection->m_selectedKit[selection->m_selectedTeam];
        hudManager->getKitSelection()->selectKit(selected);
        const int team = local ? local->getTeam() : hudManager->getKitSelection()->m_selectedTeam;
        hudManager->getKitSelection()->selectTeam(team);
        break;
    }
    case 2: setBool("MapShow", true); break;
    case 3: setBool("SquadInterfaceShow", true); break;
    case 4: setBool("RadioInterfaceShow", true); break;
    case 5: setBool("RadioVehicleInterfaceShow", true); break;
    case 6: setBool("SpottedInterfaceShow", true); break;
    case 7: setBool("SquadLeaderInterfaceShow", true); break;
    case 8: setBool("CommanderInterfaceShow", true); break;
    case 9:
        setBool("ScoreboardShow", true);
        setBool("LevelsListShow", true);
        break;
    case 10: case 12: case 13: case 14: case 22: case 23: case 24: case 25: case 28:
        break;
    case 11:
        setBool("ShowIngameHud", false);
        setBool("SpawnShow", false);
        setBool("RadioInterfaceShow", false);
        setBool("SpottedInterfaceShow", false);
        setBool("RadioVehicleInterfaceShow", false);
        setBool("SquadInterfaceShow", false);
        setBool("SquadLeaderInterfaceShow", false);
        setBool("CommanderInterfaceShow", false);
        setBool("MapMenuShow", false);
        setBool("SquadLeaderMenuShow", false);
        setBool("CommanderMenuShow", false);
        setBool("ChoiceMenuShow", false);
        setBool("CommanderRadioShow", false);
        setBool("ScoreboardShow", false);
        setBool("LevelsListShow", false);
        setBool("RenameSquadShow", false);
        setBool("VictoryShow", false);
        setBool("VictoryRankShow", false);
        setBool("VoipListShow", false);
        setBool("InviteListShow", false);
        setBool("CommanderShow", false);
        updateVictory();
        break;
    case 15:
        setBool("CommanderShow", true);
        setBool("SpawnShow", false);
        break;
    case 16: setBool("CommanderRadioShow", true); break;
    case 17:
        setBool("MapShow", true);
        setBool("SpawnShow", true);
        setBool("MembersShow", true);
        setBool("KitsShow", false);
        break;
    case 18:
        setBool("MembersShow", true);
        setBool("SpawnShow", true);
        setBool("KitsShow", false);
        break;
    case 19: setBool("MapMenuShow", true); break;
    case 20: setBool("SquadLeaderMenuShow", true); break;
    case 21: setBool("CommanderMenuShow", true); break;
    case 26: setBool("InviteListShow", true); break;
    case 27: setBool("ChoiceMenuShow", true); break;
    case 29: setBool("SetupShow", true); break;
    case 30: setBool("DemoCameraInterfaceShow", true); break;
    case 31: setBool("DemoRecInterfaceShow", true); break;
    default:
        setBool("MapShow", true);
        setBool("MapBorderShow", true);
        setBool("SpawnShow", false);
        setBool("RadioInterfaceShow", false);
        setBool("SpottedInterfaceShow", false);
        setBool("RadioVehicleInterfaceShow", false);
        setBool("SquadInterfaceShow", false);
        setBool("SquadLeaderInterfaceShow", false);
        setBool("CommanderInterfaceShow", false);
        setBool("MapMenuShow", false);
        setBool("SquadLeaderMenuShow", false);
        setBool("CommanderMenuShow", false);
        setBool("ChoiceMenuShow", false);
        setBool("CommanderRadioShow", false);
        setBool("ScoreboardShow", false);
        setBool("LevelsListShow", false);
        setBool("RenameSquadShow", false);
        setBool("VictoryShow", false);
        setBool("VictoryRankShow", false);
        setBool("VoipListShow", false);
        setBool("InviteListShow", false);
        setBool("CommanderShow", false);
        break;
    }
    // Reread the singleton and virtual previous mode after all Boolean callbacks.
    if (hudManager->getPreviousMode() != mode)
        eventManager->postEvent(ECHud, 2u, this, 0.0f);
}
void HudInformationAdapter::setField014(int value)
{ m_field014 = value; }
} } }
#endif
