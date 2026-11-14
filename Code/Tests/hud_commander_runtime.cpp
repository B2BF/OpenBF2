// [TEMP] [CHOSEN, not recovered] Real Commander bodies and native storage.
// Reuse typed collaborators from the separate squad fixture, not its main.
// The remaining declaration-only Commander row rebuild is never replaced.
#define main unusedSquadFixtureMain
#include "hud_squad_runtime.cpp"
#undef main
#include <limits>
#define private public
#include "../BF2/Menu/HudCommander.h"
#undef private
#define protected public
#include "../BF2/Game/GameEvents/VoipPlayerMuteEvent.h"
#undef protected
#include "../BF2/Core/System.h"
#include "../BF2/Menu/HudColors.h"
namespace dice { namespace hfe { IGame* game = nullptr; } }
// Publish an explicit null voice client so diagnostic linking cannot substitute an image-base address.
namespace dice { namespace hfe { class BF2VoipClient; BF2VoipClient* bf2VoipClient = nullptr; } }
struct CommanderPlayers : Players {
    int counts[10] = {};
    std::list<world::IPlayer*> squadPlayers;
    int getNrOfPlayersInSquad(int team, int squad) override {
        check(team == 2, "Commander selection forwards team");
        squads.push_back(squad);
        return squad >= 0 && squad < 10 ? counts[squad] : 0;
    }
    const std::list<world::IPlayer*>& getPlayersInSquad(int team, int squad) override {
        check(team == 2 && squad >= -1, "Commander player-list query forwards team and selected squad");
        squads.push_back(squad);
        return squadPlayers;
    }
};
struct CommanderEvents : Events {
    HudCommander* owner = nullptr;
    bool registerEventHandler(EventCategory category, IEventListener* listener, int priority) override {
        if (owner) {
            check(category == ECHud && priority == 0 && listener == owner, "Commander primary event registration");
            check(owner->m_columnPositions[0] == static_cast<int>(0xa5a5a5a5) && owner->m_slotColumns == static_cast<int>(0xa5a5a5a5), "Commander column writes follow registration");
            check(owner->m_selectedSquads.none() && owner->m_mutedPlayers.empty(), "Commander native containers exist before registration");
            owner->m_selectedSquads.set(15);
            owner->m_mutedPlayers.push_back(3);
        }
        return EventManager::registerEventHandler(category, listener, priority);
    }
};
struct CommanderTextures : dice::meme::System {
    CommanderTextures() : dice::meme::System(640,480) {}
    std::vector<std::string> names;
    HudCommander* owner = nullptr;
    CommanderTextures* replacement = nullptr;
    int base = 0, releases = 0;
    int loadTexture(const char* name, bool menuPath) override {
        check(menuPath, "Commander texture menu path"); names.push_back(name);
        if (replacement) { owner->setSystem(replacement); replacement = nullptr; }
        return base + static_cast<int>(names.size());
    }
    void releaseTexture(int) override { ++releases; }
};
struct CommanderInformation : Information {
    int registerFloat(const std::string& name, float* p) override {
        names.push_back(name); addresses.push_back(p); return -1;
    }
};
struct CommanderHud : Hud {
    int mode = 16, reads = 0, writes = 0;
    bool commanderShown = true, scoreboard = false, flag229 = false, flag22A = false;
    HudCommander* owner = nullptr;
    std::vector<int> modeValues, radioAtModeWrite, disabledHelp;
    int getMode() override { ++reads; return mode; }
    void setMode(int value, bool force) override {
        ++writes; check(force, "Commander mode writes save previous mode");
        modeValues.push_back(value); radioAtModeWrite.push_back(owner->m_radioSquad); mode = value;
    }
    bool isCommanderShown() override { return commanderShown; }
    bool isScoreboardMode() override { return scoreboard; }
    bool getFlag229() override { return flag229; }
    bool getFlag22A() override { return flag22A; }
    void disableVOHelpMessage(int id) override { disabledHelp.push_back(id); }
    // E5: no map published (getMap's body lives in BF2HudManagerState.cpp, not linked here), so
    // every request marker in the commander rows is type 0.
    HudMap* getMap() override { return nullptr; }
};
struct CommanderActionPlayer : Player {
    bool leader = false, voip = true, muted = false;
    mutable unsigned leaderReads = 0;
    unsigned voipReads = 0, mutedReads = 0, mutedWrites = 0;
    bool getIsSquadLeader() const override { ++leaderReads; return leader; }
    bool getVoipEnabled() override { ++voipReads; return voip; }
    bool getIsMuted() override { ++mutedReads; return muted; }
    void setIsMuted(bool value) override { ++mutedWrites; muted = value; }
};
struct CommanderGame : ActionGameBoundary {
    struct RadioCall { TargetChannel channel; std::string message; world::IPlayer* player;
                       unsigned squad; bool first; bool second; };
    std::vector<RadioCall> radios;
    std::vector<std::pair<unsigned int, GameEvent*> > events;
    bool radioMessage(TargetChannel channel, const std::string& message, world::IPlayer* player,
                      unsigned int squad, bool first, bool second) override {
        radios.push_back(RadioCall{channel, message, player, squad, first, second});
        return false;
    }
    void sendGameEvent(unsigned int target, GameEvent* event) override {
        events.push_back(std::make_pair(target, event));
    }
    void clearEvents() {
        for (const auto& sent : events) if (sent.second) sent.second->release();
        events.clear();
    }
    ~CommanderGame() { clearEvents(); }
};
int main() {
    checks = failures = 0; trace.clear();
    static_assert(sizeof(void*) == 4, "x86 only");
    CommanderEvents events; eventManager = &events;
    CommanderPlayers players; world::playerManager = &players;
    CommanderInformation information; g_hudInformationLayer = &information;
    events.unregisterEventHandler(ECHud, &information);
    Player player;
    alignas(HudCommander) unsigned char memory[sizeof(HudCommander)];
    std::memset(memory, 0xa5, sizeof(memory));
    auto* expected = reinterpret_cast<HudCommander*>(memory); events.owner = expected;
    CommanderTextures first, second; first.owner = expected; first.replacement = &second; second.base = 100;
    HudCommander* commander = new(memory) HudCommander(&first); events.owner = nullptr;
    check(sizeof(HudCommander) == 344, "native Commander size includes list and double alignment");
    check(first.names.size() == 1 && second.names.size() == 13, "Commander reloads System after every load");
    const std::vector<std::string> textureNames = {
        "Ingame/Commander/squadFrame.tga", "Ingame/Commander/squadEmptyBox.tga",
        "Ingame/Kits/Icons/kit_Squad_big.tga", "Ingame/Commander/squadmemberHealtbar.tga",
        "Ingame/Respawn/iconframe.tga", "Ingame/Scoreboard/Icons/checkbox_voipMuteGreyCommander.tga",
        "Ingame/Scoreboard/Icons/checkbox_voipMuteCommander.tga", "Ingame/Scoreboard/Icons/checkbox_voipTalkCommander.tga",
        "Ingame/Scoreboard/Icons/checkbox_voipActiveCommander.tga", "Ingame/GeneralIcons/empty.tga",
        "Ingame/GeneralIcons/full.tga", "Ingame/Commander/icon_request.tga",
        "Ingame/Commander/icon_deny.tga", "Ingame/Commander/icon_accept.tga"};
    auto loaded = first.names; loaded.insert(loaded.end(), second.names.begin(), second.names.end());
    check(loaded == textureNames, "all fourteen Commander texture names and order");
    const int expectedTextures[] = {1,101,102,104,105,106,107,108,109,111,112,113,103,110};
    for (int i = 0; i < 14; ++i) check(commander->m_textures[i] == expectedTextures[i], "Commander texture slot mapping");
    check(reinterpret_cast<std::uintptr_t>(commander->m_data) == 0xa5a5a5a5, "Commander constructor preserves data pointer");
    check(commander->m_selectedSquads.none() && commander->m_mutedPlayers == std::list<int>{3}, "late reset clears all selected bits but preserves event list");
    check(commander->m_warningTime == 0 && commander->m_warningInterval == 30 && !commander->m_warningActive && !commander->m_warningPending, "warning defaults");
    check(commander->m_artilleryDuration == 60 && commander->m_supplyDuration == 60 && commander->m_vehicleDropDuration == 1, "three cooldown durations");
    check(commander->m_uavArmor == 1 && commander->m_satelliteArmor == 1 && commander->m_artilleryArmor == 1 && commander->m_previousArmor == 1, "all armor defaults");
    check(commander->m_artilleryRemaining == 0 && commander->m_supplyRemaining == 0 && commander->m_vehicleDropRemaining == 0 && commander->m_vehiclesAvailable == 1, "cooldown remaining and vehicles defaults");
    check(commander->m_blinkInterval == .5f && commander->m_lastBlink == 0 && !commander->m_blink && commander->m_radioSquad == -1, "blink and radio defaults");
    check(commander->m_visibleRows == 10 && commander->m_slotColumns == 5 && commander->m_team == 2 && commander->m_expandedSquad == -1 && commander->m_lastSelectedSquad == -1, "Commander row and selection defaults");
    for (float color : commander->m_colors) check(color == 1, "all eight color defaults");
    check(commander->registerFunctions(), "Commander retail true method");
    check(commander->registerData(), "Commander ignores rejected binding results");
    const std::vector<std::string> bindingNames = {"ArtilleryState","CommanderNoArtilleryShow","ArtilleryArmor","CommanderNoSatelliteShow","SatelliteArmor","CommanderNoUAVShow","UAVArmor","ArtilleryTimeUntilReloaded","ArtilleryReloading","CommanderRadioShow","VehiclesAvailable","CommanderNoVehicleDropShow","SupplyTimeUntilReloaded","SupplyState","SupplyReloading","VehicleDropTimeUntilReloaded","VehicleDropState","VehicleDropReloading"};
    check(information.names == bindingNames, "all eighteen binding names and order");
    const void* bindingAddresses[] = {&commander->m_artilleryState,&commander->m_noArtillery,&commander->m_artilleryArmor,&commander->m_noSatellite,&commander->m_satelliteArmor,&commander->m_noUAV,&commander->m_uavArmor,&commander->m_artilleryFraction,&commander->m_artilleryReloading,&commander->m_radioShown,&commander->m_vehiclesAvailable,&commander->m_noVehicleDrop,&commander->m_supplyFraction,&commander->m_supplyState,&commander->m_supplyReloading,&commander->m_vehicleDropFraction,&commander->m_vehicleDropState,&commander->m_vehicleDropReloading};
    for (int i = 0; i < 18; ++i) check(information.addresses[i] == bindingAddresses[i], "live Commander binding address");
    BfListBoxData* data = commander->m_data;
    check(std::string(data->getName()) == "SquadList" && data->m_columnCount == 44, "Commander default plus forty-three columns");
    // Expected widths/flags transcribed independently from 79253F..7928D2.
    const int widths[] = {13,13,13,13,13,38,38,38,38,38,63,63,63,63,63,88,88,88,88,88,113,113,113,113,113,145,145,145,145,145,168,174,174,174,194,194,194,214,220,220,220,0,0};
    const bool flags[] = {false,true,true,true,true,false,true,true,true,true,false,true,true,true,true,false,true,true,true,false,true,true,true,true,false,true,true,true,true,false,true,true,true,true,true,true,true,true,true,true,false,true,true};
    for (int i = 0; i < 43; ++i) {
        auto* column = data->m_columns[i+1];
        check(column->index == i+1 && column->width == widths[i] && column->field019 == flags[i] && !column->field01a, "Commander column width and flags");
    }
    check(data->m_columns[1]->field020 == 23 && data->m_columns[41]->field020 == 20, "Commander icon column heights");
    check(!data->m_columns[42]->enabled && !data->m_columns[43]->enabled && data->m_visibleRows == 10 && !data->m_field021 && !data->m_field023, "Commander hidden columns and list flags");
    int before = players.queries;
    commander->handleEvent(ECGame,8,nullptr,nullptr); check(players.queries == before, "Commander non-HUD category ignored");
    commander->handleEvent(ECHud,0,nullptr,nullptr); check(players.queries == before+1, "Commander unrelated HUD event still queries player");
    WorldPref::resetWorldTime(12.25);
    commander->m_artilleryDuration = 40; commander->m_supplyDuration = 50; commander->m_vehicleDropDuration = 7;
    commander->m_artilleryFraction = .2f; commander->m_supplyFraction = .3f; commander->m_vehicleDropFraction = .4f;
    events.postEvent(ECHud,8,nullptr,0);
    check(commander->m_artilleryDeadline == 52.25f && commander->m_supplyDeadline == 62.25f && commander->m_vehicleDropDeadline == 19.25f, "event8 uses world clock plus each duration");
    check(commander->m_artilleryRemaining == 40 && commander->m_supplyRemaining == 50 && commander->m_vehicleDropRemaining == 7 && commander->m_artilleryReloading && commander->m_supplyReloading && commander->m_vehicleDropReloading, "event8 resets remaining durations and reload flags");
    check(commander->m_artilleryFraction == .2f && commander->m_supplyFraction == .3f && commander->m_vehicleDropFraction == .4f, "event8 preserves fraction fields");
    players.local = &player; struct Payload { bool muted; int id; } payload{true,8};
    events.postEvent(ECHud,59,&payload,0); events.postEvent(ECHud,59,&payload,0);
    check(commander->m_mutedPlayers == std::list<int>({3,8,8}), "Commander event59 appends duplicate IDs");
    payload.muted = false; events.postEvent(ECHud,59,&payload,0);
    check(commander->m_mutedPlayers == std::list<int>({3,8}), "Commander event59 removes first duplicate only");
    payload.muted = true; payload.id = 7; events.postEvent(ECHud,59,&payload,0);
    check(commander->m_mutedPlayers.size() == 2, "Commander event59 ignores self");
    players.local = nullptr; before = players.queries;
    commander->m_lastBlink = 0; WorldPref::resetWorldTime(.5); commander->update(false);
    check(!commander->m_blink && players.queries == before, "blink equality does not toggle or query player");
    WorldPref::resetWorldTime(.5001); commander->update(false);
    check(commander->m_blink && commander->m_lastBlink == static_cast<float>(.5001), "blink toggles strictly above threshold and stores float time");
    WorldPref::resetWorldTime(2); commander->update(true);
    check(!commander->m_blink && players.queries == before+1 && commander->m_lastBlink == 2, "null-player refresh still updates blink");
    WorldPref::resetWorldTime(std::numeric_limits<double>::quiet_NaN()); commander->update(false);
    check(!commander->m_blink && commander->m_lastBlink == 2, "unordered clock does not toggle");
    WorldPref::resetWorldTime(0); commander->update(false);
    check(!commander->m_blink && commander->m_lastBlink == 2, "backward clock does not toggle");
    void* hudMemory = ::operator new(sizeof(CommanderHud)); auto* hud = new(hudMemory) CommanderHud; hudManager = hud; hud->owner = commander;
    commander->selectAllSquads(true); check(commander->m_selectedSquads.to_ulong() == 0xffff, "select-all sets exactly sixteen bits");
    check(!commander->isSquadSelected(0) && commander->isSquadSelected(-1) && commander->isSquadSelected(50), "selection zero special case precedes signed clamp");
    commander->selectAllSquads(false); players.counts[1] = 2; players.counts[2] = 3; players.counts[9] = 1; players.counts[0] = 1;
    commander->setSquadSelected(1,true); commander->setSquadSelected(1,false);
    check(commander->m_selectedSquads.to_ulong() == 2, "deselect preserves last selected squad");
    commander->setSquadSelected(2,true); commander->setSquadSelected(1,false);
    check(commander->m_selectedSquads.to_ulong() == 4, "deselect removes bit when multiple selected");
    commander->setSquadSelected(99,true); commander->setSquadSelected(-5,true);
    check(commander->m_selectedSquads.to_ulong() == 0x205, "selection clamps to zero and nine");
    commander->m_radioSquad = 9; commander->setSquadSelected(9,false);
    check(hud->reads == 1 && hud->writes == 1 && commander->m_selectedSquads.to_ulong() == 5, "radio deselection closes mode and clears selected bit");
    commander->selectAllSquads(false); commander->setSquadSelected(1,true);
    players.squads.clear(); commander->selectSingleSquad(2);
    check(commander->m_selectedSquads.to_ulong() == 4 && commander->getLastSelectedSquad() == 2 && players.squads == std::vector<int>({2,2}), "single selection checks occupancy then updates selection");
    commander->selectSingleSquad(2); check(commander->m_selectedSquads.to_ulong() == 4, "single selection retains an already selected squad");
    commander->selectSingleSquad(3); check(commander->m_selectedSquads.to_ulong() == 4 && commander->getLastSelectedSquad() == 2, "empty single selection leaves state and last selection");
    players.counts[3] = -1; commander->selectSingleSquad(3);
    check(commander->m_selectedSquads.none() && commander->getLastSelectedSquad() == -1, "negative count enters single-selection path but does not set a bit");
    players.counts[3] = 1; commander->toggleSquad(1); commander->toggleSquad(2); commander->toggleSquad(1);
    check(commander->m_selectedSquads.to_ulong() == 4 && commander->getLastSelectedSquad() == -1, "toggle tracks selected result after removing bit");
    commander->selectAllSquads(false); players.squads.clear(); commander->selectSquadRange(3,1);
    check(commander->m_selectedSquads.to_ulong() == 14 && players.squads == std::vector<int>({1,2,3}) && commander->getLastSelectedSquad() == 1, "reversed range visits ascending squads and records endpoint");
    players.squads.clear(); commander->selectSquadRange(2,-1);
    check(players.squads == std::vector<int>({1,2}) && commander->getLastSelectedSquad() == 1, "range minus-one endpoint reuses last selection");
    players.squads.clear(); commander->selectSquadRange(0,2);
    check(players.squads.empty() && commander->getLastSelectedSquad() == 2, "invalid range still updates last selected endpoint");
    commander->toggleExpandedSquad(20); check(commander->getExpandedSquad() == 20, "expanded selection is not clamped");
    commander->toggleExpandedSquad(20); check(commander->getExpandedSquad() == -1, "repeated expanded selection clears it");

    // CommanderMenu owner actions. Hidden columns 42/43 carry squad/row-kind
    // strings; column 34 selects the shipped VoIP-mute arm.
    check(data->appendCell(42, "2", 0, 0, 0, 0, 0)
        && data->appendCell(43, "0", 0, 0, 0, 0, 0),
        "Commander action fixture appends selected-row hidden values");
    data->m_row = 0; data->m_column = 33;
    ListBoxCell* const squadCell = data->m_columns[42]->cells[0];
    ListBoxCell* const rowKindCell = data->m_columns[43]->cells[0];

    CommanderGame commanderGame; game = &commanderGame;
    players.local = &player; players.counts[2] = 3;
    hud->commanderShown = true; hud->scoreboard = false;
    hud->flag229 = false; hud->flag22A = false; hud->mode = 15;
    hud->disabledHelp.clear(); commander->selectAllSquads(false);
    commander->singleClick();
    check(commander->m_selectedSquads.to_ulong() == 4 && commander->m_lastSelectedSquad == 2,
        "Commander single click selects the decoded occupied squad");
    check(hud->disabledHelp == std::vector<int>({25}),
        "Commander squad selection disables help message 25");

    rowKindCell->narrow = "1"; const unsigned long selectedBefore = commander->m_selectedSquads.to_ulong();
    commander->singleClick();
    check(commander->m_selectedSquads.to_ulong() == selectedBefore
        && hud->disabledHelp == std::vector<int>({25}),
        "Commander single click on an all-row returns without deselecting or help changes");
    rowKindCell->narrow = "0";

    hud->mode = 16; commander->m_radioSquad = 8;
    const int readsBeforeClose = hud->reads; const int writesBeforeClose = hud->writes;
    commander->singleClick();
    check(hud->reads == readsBeforeClose + 2 && hud->writes == writesBeforeClose + 1
        && hud->mode == 15 && commander->m_radioSquad == 8,
        "Commander single click reloads mode and closes radio mode without clearing radio squad");

    rowKindCell->narrow = "1"; hud->mode = 16;
    const int writesBeforeDeselect = hud->writes;
    commander->rightClick();
    check(hud->mode == 15 && hud->writes == writesBeforeDeselect + 1,
        "Commander right click on an all-row dispatches deselect");

    rowKindCell->narrow = "0"; hud->mode = 15; commander->m_radioSquad = -1;
    commander->selectAllSquads(false); hud->modeValues.clear(); hud->radioAtModeWrite.clear();
    commander->rightClick();
    check(commander->m_selectedSquads.to_ulong() == 4 && commander->m_radioSquad == 2
        && hud->mode == 16 && hud->modeValues == std::vector<int>({16}),
        "Commander occupied-squad right click enters radio mode and selects only that squad");
    check(hud->radioAtModeWrite == std::vector<int>({-1}),
        "Commander radio selection stores m_radioSquad after the mode callback");

    commander->selectAllSquads(false);
    players.counts[5] = 1;
    commander->setSquadSelected(2, true); commander->setSquadSelected(5, true);
    commander->setSquadSelected(9, true); commander->m_radioSquad = 5; hud->mode = 16;
    commanderGame.radios.clear(); hud->radioAtModeWrite.clear();
    commander->sendRadioMessage("commander message", TCSquad);
    check(commanderGame.radios.size() == 3 && commanderGame.radios[0].squad == 2
        && commanderGame.radios[1].squad == 5 && commanderGame.radios[2].squad == 9,
        "Commander radio send walks selected squads in ascending order");
    check(commanderGame.radios[0].first && !commanderGame.radios[1].first
        && !commanderGame.radios[2].first && !commanderGame.radios[0].second,
        "Commander radio send marks only the first selected squad");
    check(commanderGame.radios[0].channel == TCSquad
        && commanderGame.radios[0].message == "commander message"
        && commanderGame.radios[0].player == &player,
        "Commander radio send forwards channel, message and cached local player");
    check(commander->m_radioSquad == -1 && hud->mode == 15
        && hud->radioAtModeWrite.back() == -1,
        "Commander radio send clears radio squad before closing mode 16");
    players.local = nullptr; commander->m_radioSquad = 7; hud->mode = 16;
    const std::size_t radioCount = commanderGame.radios.size();
    commander->sendRadioMessage("ignored", TCGlobal);
    check(commanderGame.radios.size() == radioCount && commander->m_radioSquad == 7 && hud->mode == 16,
        "Commander radio send with no local player leaves selection mode untouched");

    players.local = &player; data->m_column = 34; squadCell->narrow = "2";
    CommanderActionPlayer firstCandidate, fallbackCandidate;
    firstCandidate.id = 21; fallbackCandidate.id = 22;
    firstCandidate.leader = false; fallbackCandidate.leader = false;
    players.squadPlayers = {&firstCandidate, &fallbackCandidate};
    commander->m_mutedPlayers.push_back(22); hud->mode = 15; hud->disabledHelp.clear();
    commanderGame.clearEvents(); commander->singleClick();
    check(!firstCandidate.muted && fallbackCandidate.muted
        && firstCandidate.leaderReads == 1 && fallbackCandidate.leaderReads == 1,
        "Commander mute traversal preserves retail fallback to the last player when no leader exists");
    check(commanderGame.events.size() == 1 && commanderGame.events[0].first == 0xffffffffu,
        "Commander mute sends one event to connection minus one");
    VoipPlayerMuteEvent* muteEvent = static_cast<VoipPlayerMuteEvent*>(commanderGame.events[0].second);
    check(muteEvent && muteEvent->m_muted && muteEvent->m_mutedPlayerId == 7
        && muteEvent->m_targetPlayerId == 22,
        "Commander mute event retains muted state, local id and selected player id");
    check(hud->disabledHelp == std::vector<int>({36}),
        "Commander mute direction alone disables help message 36");
    commanderGame.clearEvents(); commander->singleClick();
    muteEvent = static_cast<VoipPlayerMuteEvent*>(commanderGame.events[0].second);
    check(!fallbackCandidate.muted && muteEvent && !muteEvent->m_muted
        && hud->disabledHelp == std::vector<int>({36}),
        "Commander unmute sends false without repeating help disable");
    commanderGame.clearEvents();
    const unsigned actionCallsBeforeNoop = fallbackCandidate.mutedWrites;
    commander->doubleClick();
    check(fallbackCandidate.mutedWrites == actionCallsBeforeNoop,
        "Commander double click is the shipped empty action");

    // Rebuilds squad rows with a leader and member, voice disabled, and no map. Other squads are empty.
    // The rebuild clears cells left by action tests.
    {
        struct TeamRowPlayer : RowPlayer { int getTeam() const override { return 2; } };
        TeamRowPlayer localRow; localRow.id = 7;
        RowPlayer lead, mate;
        lead.id = 30; lead.squad = 1; lead.leader = true; lead.name = "Lead";
        mate.id = 31; mate.squad = 1; mate.name = "Mate";
        Squads rowSquads; rowSquads.leader = 30; squadManager = &rowSquads;
        players.byId = {&lead, &mate};
        players.squadByRank = {&lead, &mate}; players.squadByScore = {&mate, &lead};
        players.squadPlayers = {&lead, &mate};
        for (int& count : players.counts) count = 0;
        players.counts[1] = 2;
        players.squadRankCalls = players.squadScoreCalls = 0;
        players.local = &localRow; commander->m_radioSquad = -1; hud->mode = 15;
        commander->m_sortByScore = false; commander->m_expandedSquad = -1;
        commander->selectAllSquads(false); commander->setSquadSelected(1, true);
        voHelpCalls = 0; commander->m_talkingLeaders = 99; commander->m_rowCount = 99;
        commander->update(true);
        auto at = [&](int column, int row) -> ListBoxCell* {
            ListBoxColumn* col = data->m_columns[column];
            return static_cast<std::size_t>(row) < col->cells.size() ? col->cells[row] : nullptr;
        };
        auto same = [](const Color& c, const Color& d) { return c.r == d.r && c.g == d.g && c.b == d.b && c.a == d.a; };
        auto blank = [&](int column, int row) { ListBoxCell* c = at(column, row); return c && !c->isImage && c->narrow.empty(); };
        auto image = [&](int column, int row, int texture, const Color& color) {
            ListBoxCell* c = at(column, row); return c && c->isImage && c->texture == texture && same(c->color, color);
        };
        check(commander->m_team == 2 && commander->m_rowCount == 9 && commander->m_talkingLeaders == 0,
              "Commander rows: one row per squad 1..9, both counters reset");
        bool nine = true; for (int column = 0; column < 44; ++column) nine &= data->m_columns[column]->cells.size() == 9;
        check(nine, "Commander rows: every one of the 44 columns holds exactly nine cells");
        check(players.squadRankCalls == 9 && players.squadScoreCalls == 0 && rowSquads.leaderCalls == 9
              && rowSquads.infoCalls == 9, "Commander rows: rank lists, one leader lookup and one dropped info query per squad");
        // Row 0, squad 1, selected: palettes [1].
        check(image(21, 0, commander->m_textures[13], commanderCellColors[1])
              && image(22, 0, commander->m_textures[3], commanderFrameColors[1])
              && image(23, 0, 531, commanderIconColors[1]), "member 31 takes slot 4: background, frame, kit icon");
        check(image(24, 0, commander->m_textures[12], commanderCellColors[1]) && at(24, 0)->imageWidth == 2
              && at(24, 0)->imageHeight == 15 && at(24, 0)->imageX == 16 && at(24, 0)->imageY == 4,
              "slot 4 health-bar background, 2x15 at (16, 4)");
        check(at(25, 0)->isImage && at(25, 0)->texture == commander->m_textures[12] && at(25, 0)->imageHeight == 0
              && at(25, 0)->imageY == 19 && same(at(25, 0)->color, Color{0.7490196228027344f, 0.7764706015586853f, 0, 1}),
              "no vehicle: an empty bar at y 15+4, in the selected low-health colour");
        check(image(28, 0, 530, commanderIconColors[1]), "the leader (by id) takes slot 5");
        check(image(1, 0, commander->m_textures[13], commanderCellColors[1])
              && image(2, 0, commander->m_textures[3], commanderCellColors[1]) && blank(3, 0) && blank(5, 0),
              "an empty seat: its frame in the CELL palette, two blanks");
        check(blank(31, 0) && blank(32, 0) && blank(33, 0) && blank(34, 0) && blank(38, 0),
              "blank spacers; no voice cells for a leader with voice off");
        check(image(35, 0, commander->m_textures[13], commanderCellColors[1])
              && image(36, 0, commander->m_textures[3], commanderFrameColors[1])
              && image(37, 0, commander->m_textures[8], mapSquadRequestColors[0][1]), "no map: request type 0, the empty icon");
        check(image(39, 0, commander->m_textures[2], commanderSquadIconColors[1]) && blank(40, 0)
              && at(41, 0)->narrow == "1" && !at(41, 0)->isImage && same(at(41, 0)->color, commanderNumberColors[1]),
              "squad icon and number");
        check(at(42, 0)->narrow == "1" && at(43, 0)->narrow == "0"
              && image(0, 0, commander->m_textures[0], commanderRowFrameColors[1])
              && at(0, 0)->imageWidth == 239 && at(0, 0)->imageHeight == 23, "hidden id, not-empty flag, squadFrame");
        check(at(1, 0)->field06c && same(at(1, 0)->highlight, commanderHighlightColors[1])
              && !at(0, 0)->field06c && !at(42, 0)->field06c && !at(43, 0)->field06c,
              "the highlight is stamped before columns 0, 42 and 43 exist");
        // Row 1, squad 2, empty and not selected: palettes [0].
        bool emptySlots = true; for (int column = 2; column <= 30; ++column) emptySlots &= blank(column, 1);
        check(emptySlots && blank(35, 1) && blank(37, 1) && blank(39, 1) && blank(41, 1),
              "an empty squad: blank slots, request and number cells");
        check(at(1, 1)->narrow == "\xA7" "12" && !at(1, 1)->isImage && !at(1, 1)->isWide && at(1, 1)->imageX == -13
              && at(1, 1)->imageY == 0 && same(at(1, 1)->color, commanderNumberColors[0])
              && at(1, 1)->field064 == 1 && at(1, 1)->field090 == 0 && at(1, 1)->resourceIndex == -1,
              "7C8640 rewrites column 1 in place: the caption at -column[0]");
        check(image(0, 1, commander->m_textures[1], Color{1, 1, 1, 1}) && at(0, 1)->imageWidth == 23
              && at(42, 1)->narrow == "2" && at(43, 1)->narrow == "1", "squadEmptyBox, id 2, empty flag");
        check(at(2, 1)->field06c && same(at(2, 1)->highlight, Color{0, 0, 0, 0}),
              "an unselected row's highlight is the never-written zero colour");
        check(commander->m_selectedSquads.to_ulong() == 2 && voHelpCalls == 0,
              "painting leaves squad 1 selected; no VO help without talking or requests");
        // Voice on for the leader: column 33's frame follows the voice client's leader channel (+0xA0,
        // read with no null test at BF2.exe 0x79410A; the client image always constructs one). The
        // fake answers that slot only: the 41st virtual, as in BF2VoipClient's MSVC vtable.
        {
#define FAKE_VOIP_SLOT(n) virtual void slot##n() {}
            struct FakeVoip {
                FAKE_VOIP_SLOT(00) FAKE_VOIP_SLOT(01) FAKE_VOIP_SLOT(02) FAKE_VOIP_SLOT(03) FAKE_VOIP_SLOT(04)
                FAKE_VOIP_SLOT(05) FAKE_VOIP_SLOT(06) FAKE_VOIP_SLOT(07) FAKE_VOIP_SLOT(08) FAKE_VOIP_SLOT(09)
                FAKE_VOIP_SLOT(10) FAKE_VOIP_SLOT(11) FAKE_VOIP_SLOT(12) FAKE_VOIP_SLOT(13) FAKE_VOIP_SLOT(14)
                FAKE_VOIP_SLOT(15) FAKE_VOIP_SLOT(16) FAKE_VOIP_SLOT(17) FAKE_VOIP_SLOT(18) FAKE_VOIP_SLOT(19)
                FAKE_VOIP_SLOT(20) FAKE_VOIP_SLOT(21) FAKE_VOIP_SLOT(22) FAKE_VOIP_SLOT(23) FAKE_VOIP_SLOT(24)
                FAKE_VOIP_SLOT(25) FAKE_VOIP_SLOT(26) FAKE_VOIP_SLOT(27) FAKE_VOIP_SLOT(28) FAKE_VOIP_SLOT(29)
                FAKE_VOIP_SLOT(30) FAKE_VOIP_SLOT(31) FAKE_VOIP_SLOT(32) FAKE_VOIP_SLOT(33) FAKE_VOIP_SLOT(34)
                FAKE_VOIP_SLOT(35) FAKE_VOIP_SLOT(36) FAKE_VOIP_SLOT(37) FAKE_VOIP_SLOT(38) FAKE_VOIP_SLOT(39)
                virtual bool getUsingLeaderChannel() { ++reads; return leaderChannel; }
                bool leaderChannel = false;
                int reads = 0;
            };
#undef FAKE_VOIP_SLOT
            FakeVoip voip;
            bf2VoipClient = reinterpret_cast<BF2VoipClient*>(&voip);
            // The fixture's player manager answers the same squad list for every squad, so the
            // voice-enabled leader heads all nine rows and each row reads the client once (retail
            // 0x79410A runs per row, after that row's leader test).
            lead.voip = true;
            commander->update(true);
            check(voip.reads == 9 && image(33, 0, commander->m_textures[3], commanderFrameColors[1])
                  && image(34, 0, commander->m_textures[7], commanderIconColors[1])
                  && image(33, 1, commander->m_textures[3], commanderFrameColors[0]),
                  "voice-enabled leader, default channel: palette frames (selected and not) and the active icon");
            voip.leaderChannel = true;
            commander->update(true);
            check(voip.reads == 18 && image(33, 0, commander->m_textures[3], Color{1, 1, 0, 1})
                  && image(33, 8, commander->m_textures[3], Color{1, 1, 0, 1}),
                  "voice-enabled leader on the leader channel: the yellow frame in every row");
            lead.voip = false;
            bf2VoipClient = nullptr;
        }
        players.local = nullptr; squadManager = nullptr;
        players.byId.clear(); players.squadByRank.clear(); players.squadByScore.clear(); players.squadPlayers.clear();
    }

    game = nullptr;
    commander->~HudCommander(); before = players.queries; events.postEvent(ECHud,8,nullptr,0);
    check(players.queries == before && !first.releases && !second.releases && data->m_columnCount == 44, "Commander destructor unregisters and retains data and textures");
    cleanupData(data);
    { HudCommander noSystem(nullptr); bool zero = true; for(int texture : noSystem.m_textures) zero &= texture == 0; check(zero, "null System initializes every texture to zero"); }
    delete hud->m_field2dc;
    using W=std::wstring; using S=std::string; using L=std::list<int>; using SL=std::list<std::string>; using V=std::vector<dice::hfe::hud::HudPendingRequest>;
    hud->m_field010.~W(); hud->m_field038.~W(); hud->m_field060.~W(); hud->m_field08c.~S(); hud->m_field0c0.~S(); hud->m_field304.~S();
    hud->m_field0e8.~L(); hud->m_field2e0.~SL(); hud->m_field33c.~SL(); hud->m_field32c.~V(); ::operator delete(hudMemory); hudManager = nullptr;
    std::printf("HUD Commander: %d/%d checks passed\n",checks-failures,checks); return failures ? 1 : 0;
}
