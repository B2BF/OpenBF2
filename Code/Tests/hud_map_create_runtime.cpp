// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: HUD-MAP-CREATE-FIXTURE]
// Tests createMapNode through real builder, map lifecycle, descriptors, registry, and information layer.
// Renderer texture loading and EventManager callbacks are recording observers.
#include <windows.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <limits>
#include <string>
#include <vector>
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Game/HudInformationLayer.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/External/BFMeme/System.h"
#include "../BF2/External/BFMeme/NamedObjectRegistry.h"
#include "../BF2/External/BFMeme/TypeDescriptor.h"
#include "../BF2/Menu/Bf2SplitNode.h"
#include "../BF2/Menu/Bf2TransformNode.h"
#include "../BF2/Menu/HudColors.h"
#include "../BF2/Game/GameLogic/SquadManager/ISquadInfo.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Game/Objects/Fx/PersistentExplosionEffectManager.h"
#include "../BF2/External/BFMeme/Quad.h"
#include "../BF2/External/BFMeme/Node.h"        // D2: RenderContext for the supply-line draw
#include "../BF2/Core/Ids.h"                    // D2: the two control-point interface ids
[[noreturn]] static void fixtureUnsupported(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] hud map fixture unexpected %s\n", name);
    std::fflush(stderr);
    std::abort();
}
#include "hud_kit_observers.h"
// [TEMP] test-only access exposure for the two headers whose data and focused recovered helpers
// are checked. Production units are compiled separately, so their symbols and access remain real.
#define private public
#include "../BF2/Menu/Bf2MenuMapNode.h"
#include "../BF2/Menu/Bf2MiniMenuMapNode.h"
#include "../BF2/Menu/Bf2HudBuilder.h"   // the map handle at +08 is read directly
#include "../BF2/Menu/BF2HudManager.h"  // family L: 773630 reads the singleton
#include "../BF2/Menu/HudBlinker.h"     // its constructor owns a blink manager
#undef private
#include "../BF2/Game/GameLogic/SquadManager/SquadOrder.h"
// Redeclare world-clock functions to avoid ambiguity between engine and meme System types.
namespace dice { namespace hfe { namespace WorldPref {
double getWorldTime();
void resetWorldTime(double time);
} } }
namespace dice { namespace hfe {
class IGameLogic; class ClassManager; class SettingsRepostitory;
namespace world { class PlayerManager; class ObjectManager; }
// [TEMP] the globals the linked map/builder/information units read; none is exercised here.
IGameLogic* gameLogic = nullptr;
IGame* game = nullptr;                 // family P's ECHud id 8 reads getCurrentMapName
IBase* squadManager = nullptr;
hud::BF2HudManager* hudManager = nullptr;
ClassManager* classManager = nullptr;
SettingsRepostitory* settings = nullptr;
EventManager* eventManager = nullptr;
IBase* g_hudInformationLayer = nullptr;
namespace world {
PlayerManager* playerManager = nullptr;
ObjectManager* objectManager = nullptr;
CPersistentExplosionEffectManager* explosionEffectManager = nullptr;
}
} }
using namespace dice::hfe;
using namespace dice::meme;
using dice::hfe::hud::Bf2HudBuilder;

static int checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

namespace dice { namespace meme {
struct Bf2MenuMapNodePublicFixture {
    static Bf2MenuMapNode& m(Bf2MenuMapNode* map) { return *map; }
    static void populateControlPointMapIcon(Bf2MenuMapNode* map,
                                            MapIconRecord* icon,
                                            int team,
                                            bool useTeamTexture,
                                            std::string mapName,
                                            bool notCapturable,
                                            bool cachePresent,
                                            bool localCaptureActive)
    {
        map->populateControlPointMapIcon(icon, team, useTeamTexture, mapName,
                                         notCapturable, cachePresent,
                                         localCaptureActive);
    }
};
} }
using dice::meme::Bf2MenuMapNodePublicFixture;
// Defined in hud_map_private_access.cpp, which includes the production header
// without the private->public inspection macro so MSVC retains the real private
// member decoration for the recovered collectors.
void collectPlayerIconsForFixture(dice::meme::Bf2MenuMapNode* map);
void collectStrategicIconsForFixture(dice::meme::Bf2MenuMapNode* map);
void collectTimedPositionIconsForFixture(dice::meme::Bf2MenuMapNode* map);
void collectUavIconsForFixture(dice::meme::Bf2MenuMapNode* map);
dice::meme::Bf2MenuMapNode* createTimedIconFallbackNodeForFixture(
    dice::meme::System* system, const char* name);
void destroyTimedIconFallbackNodeForFixture(dice::meme::Bf2MenuMapNode* map);
int timedIconFallbackPopulateCallsForFixture(dice::meme::Bf2MenuMapNode* map);
std::size_t timedIconFallbackRequestedCountForFixture(
    dice::meme::Bf2MenuMapNode* map);
int timedIconFallbackRequestedIdForFixture(dice::meme::Bf2MenuMapNode* map,
                                            std::size_t index);
void completeLineRecordForFixture(dice::meme::Bf2MenuMapNode* map, float scale,
                                  dice::meme::MapLineRecord* line);
void addSquadRequestIconForFixture(dice::meme::Bf2MenuMapNode* map, unsigned int type,
                                   dice::hfe::world::IPlayer* player,
                                   const dice::meme::MapIconRecord* icon);
void collectSquadRequestIconsForFixture(dice::meme::Bf2MenuMapNode* map);
bool pickSquadRequestForFixture(dice::meme::Bf2MenuMapNode* map, float scale,
                                dice::hfe::Vec2 position, float maxDistance);
unsigned short pickEnemyVehicleForFixture(dice::meme::Bf2MenuMapNode* map, float scale,
                                          dice::hfe::Vec2 position, float maxDistance);
int pickPlayerForFixture(dice::meme::Bf2MenuMapNode* map, float scale, float x, float y,
                         float maxDistance);
void updateSupplyLineForFixture(dice::meme::Bf2MenuMapNode* map, dice::meme::System* system);
void drawSupplyLineForFixture(dice::meme::Bf2MenuMapNode* map, const dice::meme::Rect* adjusted,
                              const dice::meme::Rect* bounds, dice::meme::System* system,
                              dice::meme::RenderContext* context);

// [TEMP] the real System with a real registry; only the renderer slot is a recorder
struct FixtureSystem : System {
    std::vector<std::string> textures, fonts; int menuFlags = 0;
    // Records drawn quads and permits one named texture load to fail.
    std::string failTexture;
    struct DrawnQuad { int texture, texture2; Color color; };
    std::vector<DrawnQuad> drawn;
    FixtureSystem() : System(800.0f, 600.0f) { m_registry = new NameTreeRoot; }
    ~FixtureSystem() override { delete m_registry; m_registry = nullptr; }
    int loadTexture(const char* name, bool menuPath) override
    {
        textures.push_back(name); menuFlags += menuPath ? 1 : 0;
        return !failTexture.empty() && failTexture == name ? 0 : static_cast<int>(textures.size());
    }
    bool drawTexture(int texture, Quad* quad) override
    {
        const DrawnQuad d = { texture, quad->texture2, quad->corners[0].color };
        drawn.push_back(d);
        return true;
    }
    int loadFont(const char* name) override                 // +80; the base answers 0
    { fonts.push_back(name); return static_cast<int>(fonts.size()); }
    TypeDescriptor* getType() override { return nullptr; } // never queried here
    void setFixturePointer(float x, float y) { m_pointerX = x; m_pointerY = y; }
};

// [TEMP] the real EventManager with the two handler slots recording
struct Bus : EventManager {
    std::vector<int> registered, unregistered, priorities; std::vector<IEventListener*> who, whoOff;
    bool registerEventHandler(EventCategory c, IEventListener* l, int p) override
    { registered.push_back(c); who.push_back(l); priorities.push_back(p); return true; }
    bool unregisterEventHandler(EventCategory c, IEventListener* l) override
    { unregistered.push_back(c); whoOff.push_back(l); return true; }
};
// [TEMP] the real information layer, recording every registration name and target
struct Info : HudInformationLayer {
    std::vector<std::string> names; std::vector<void*> targets;
    int registerBool(const std::string& n, bool* p) override { names.push_back("b:" + n); targets.push_back(p); return HudInformationLayer::registerBool(n, p); }
    int registerInt(const std::string& n, int* p) override { names.push_back("i:" + n); targets.push_back(p); return HudInformationLayer::registerInt(n, p); }
    int registerFloat(const std::string& n, float* p) override { names.push_back("f:" + n); targets.push_back(p); return HudInformationLayer::registerFloat(n, p); }
};

// [TEMP]
// Substitutes isCommanderShown to avoid dereferencing an unconfigured HUD information adapter.
// The real HUD constructor runs; fixture teardown releases only the storage it initializes.
struct FixtureHud : hud::BF2HudManager {
    bool commanderShown = false; int commanderQueries = 0;
    bool isCommanderShown() override { ++commanderQueries; return commanderShown; }
};
// [TEMP]
// Records camera/map queries and click-related HUD commands. No heap members; fixture teardown uses raw storage.
struct MiniClickHud : FixtureHud {
    world::ICameraObject* cameraObject = nullptr; int cameraObjectQueries = 0, lastCameraTeam = -1;
    hud::HudMap* mapForMini = nullptr; int mapQueries = 0;
    int modeCalls = 0, lastMode = -1; bool lastSavePrevious = false;
    int cameraCalls = 0; Vec2 lastCamera = Vec2{0.0f, 0.0f};
    int voHelpCalls = 0, lastVoHelp = -1;
    world::ICameraObject* getCameraObject(int team) override
    { ++cameraObjectQueries; lastCameraTeam = team; return cameraObject; }
    hud::HudMap* getMap() override { ++mapQueries; return mapForMini; }
    void setMode(int mode, bool savePrevious) override
    { ++modeCalls; lastMode = mode; lastSavePrevious = savePrevious; }
    bool setCommanderCameraPosition(Vec2 position) override
    { ++cameraCalls; lastCamera = position; return false; }
    void disableVOHelpMessage(int id) override { ++voHelpCalls; lastVoHelp = id; }
};
// [TEMP]
// Camera observer returns the transform used for the local-player icon; translation is in m[12..14].
struct CameraInput {
    void** table; float transform[16]; int queries;
    static const float* __fastcall getTransformation(CameraInput* p, void*, float)      // +0x10
    { ++p->queries; return p->transform; }
};
namespace dice { namespace meme {
// [TEMP]
// Friend access preserves the real protected-member decoration.
struct Bf2MiniMenuMapNodeFixture {
    static double& lastUpdateTime(Bf2MiniMenuMapNode* mini) { return mini->m_lastUpdateTime; }
    static void update(Bf2MiniMenuMapNode* mini, float delta) { mini->update(delta); }
    static void collectLocalPlayerIcon(Bf2MiniMenuMapNode* mini) { mini->collectLocalPlayerIcon(); }
    static void colorMiniMapIcon(Bf2MiniMenuMapNode* mini, hfe::world::IPlayer* player,
                                 MapIconRecord* icon, bool highlightSelection)
    { mini->colorMiniMapIcon(player, icon, highlightSelection); }
    static void populateMiniPlayerIcon(Bf2MiniMenuMapNode* mini, hfe::world::IPlayer* player,
                                       MapIconRecord* icon, bool* headingOut, bool* rotateOut,
                                       bool showKit, bool specialRoleIcons)
    { mini->populateMiniPlayerIcon(player, icon, headingOut, rotateOut, showKit, specialRoleIcons); }
};
} }
using dice::meme::Bf2MiniMenuMapNodeFixture;
static void destroyHudStorage(hud::BF2HudManager* singleton)
{
    delete singleton->m_field2dc;
    using W = std::wstring; using S = std::string; using L = std::list<int>;
    using SL = std::list<std::string>; using V = decltype(singleton->m_field32c);
    singleton->m_field010.~W(); singleton->m_field038.~W(); singleton->m_field060.~W();
    singleton->m_field08c.~S(); singleton->m_field0c0.~S(); singleton->m_field304.~S();
    singleton->m_field0e8.~L(); singleton->m_field2e0.~SL(); singleton->m_field33c.~SL(); singleton->m_field32c.~V();
    ::operator delete(singleton);
}

// [TEMP]
// Supplies local-player and current-map queries through raw interface observers.
// Every unexpected slot reports and aborts; no linked production body is replaced.
static void unexpectedMapSlot()
{
    std::fprintf(stderr, "[TEMP] hud_map_create_runtime: unexpected virtual slot\n");
    std::fflush(stderr);
    std::abort();
}
struct PlayersInput {
    void** table; int localQueries;
    void* local = nullptr;            // nullptr for most of the suite, set per test
    void* byId = nullptr;             // what getPlayerFromId hands back
    std::vector<int> idLookups;
    // __fastcall with the explicit `this` is thiscall's ABI and, unlike a member
    // pointer, converts to void* - the same shape hud_chat_finish_runtime uses.
    static void* __fastcall getLocalHumanPlayer(PlayersInput* p, void*)   // +0x30
    { ++p->localQueries; return p->local; }
    static void* __fastcall getPlayerFromId(PlayersInput* p, void*, int id) // +0x20
    { p->idLookups.push_back(id); return p->byId; }
};

// [TEMP] A normal typed override over the real PlayerManager. It supplies only
// an empty/null-entry live registry, so 783440 can prove its leading byte stores
// without manufacturing any IPlayer/IObject success or positional dispatch.
struct PlayerIconManagerInput : world::PlayerManager {
    std::list<world::IPlayer*> players;
    int playerQueries = 0;
    int localQueries = 0;
    // Supplies a local player and per-team registry, recording requested teams.
    world::IPlayer* local = nullptr;
    std::list<world::IPlayer*> teamPlayers;
    std::vector<int> teamQueries;
    const std::list<world::IPlayer*>& getPlayers() override
    { ++playerQueries; return players; }
    const std::list<world::IPlayer*>& getPlayers(int team) override
    { teamQueries.push_back(team); return teamPlayers; }
    world::IPlayer* getLocalHumanPlayer() override
    { ++localQueries; return local; }
};
// [TEMP] A normal typed IGameLogic boundary. The only callable operation owns
// and returns a real, empty strategic-object registry; every other callback is
// inherited from LogicBoundary and aborts by name.
struct StrategicLogicInput : LogicBoundary {
    std::map<std::string, world::StrategicObjectHolder*> strategicObjects;
    int strategicQueries = 0;
    void suicide() override
    { fixtureUnsupported("StrategicLogicInput::suicide"); }
    bool selectTeam(world::IPlayer*, int) override
    { fixtureUnsupported("StrategicLogicInput::selectTeam"); }
    bool selectKitAndUnlockLevel(world::IPlayer*, int, int) override
    { fixtureUnsupported("StrategicLogicInput::selectKitAndUnlockLevel"); }
    std::map<std::string, world::StrategicObjectHolder*>* getStrategicObjectMap() override
    {
        ++strategicQueries;
        return &strategicObjects;
    }
};
struct GameInput {
    void** table; std::string mapName; int nameQueries;
    static const std::string& __fastcall getCurrentMapName(GameInput* p, void*) // +0xEC
    { ++p->nameQueries; return p->mapName; }
};
// The map's ECHud id-10 arm reaches the information adapter through the real
// BF2HudManager::getInformationAdapter (+244); only the adapter itself stands in,
// because HudInformationAdapter.cpp is not in this fixture's link.
struct AdapterInput {
    void** table; bool mapStatic; std::vector<std::string> boolQueries;
    int mapZoom; std::vector<std::string> intQueries;
    static bool __fastcall getBool(AdapterInput* p, void*, std::string name)      // +0x10
    { p->boolQueries.push_back(name); return p->mapStatic; }
    static int __fastcall getInt(AdapterInput* p, void*, std::string name)        // +0x18
    { p->intQueries.push_back(name); return p->mapZoom; }
};
// The four IPlayer slots setHudMode's role arms and applyPlayerRoleFilters reach.
// PE offsets throughout - each is ELF minus one destructor slot.
struct MapPlayerInput {
    void** table; int id; int squad; bool commander; bool leader; int team;
    int overheadCalls = 0; bool overheadValue = true;
    void* vehicle = nullptr; int vehicleQueries = 0;
    bool alive = true; bool manDown = false; int teamQueries = 0;
    void* camera = nullptr; int cameraQueries = 0;
    void* defaultVehicle = nullptr;
    static void* __fastcall getCamera(MapPlayerInput* p, void*)       // +0x17C
    { ++p->cameraQueries; return p->camera; }
    static void* __fastcall getDefaultVehicle(MapPlayerInput* p, void*) { return p->defaultVehicle; } // +0xA0
    static void __fastcall setCommanderCamOverhead(MapPlayerInput* p, void*, bool v)   // +0xC4
    { ++p->overheadCalls; p->overheadValue = v; }
    static void* __fastcall getVehicle(MapPlayerInput* p, void*)      // +0x44
    { ++p->vehicleQueries; return p->vehicle; }
    static bool __fastcall getIsAlive(MapPlayerInput* p, void*)       { return p->alive; }   // +0x68
    static bool __fastcall getIsManDown(MapPlayerInput* p, void*)     { return p->manDown; } // +0xFC
    static int  __fastcall getId(MapPlayerInput* p, void*)            { return p->id; }      // +0x50
    static int  __fastcall getTeam(MapPlayerInput* p, void*)          { ++p->teamQueries; return p->team; } // +0xE4
    static int  __fastcall getSquadId(MapPlayerInput* p, void*)       { return p->squad; }   // +0x108
    static bool __fastcall getIsCommander(MapPlayerInput* p, void*)   { return p->commander; } // +0x110
    static bool __fastcall getIsSquadLeader(MapPlayerInput* p, void*) { return p->leader; }  // +0x138
};
// [TEMP]
// Settings observer returns one extent for both keys, records lookups, and aborts other slots.
struct SettingsInput {
    void** table; int extent; std::vector<std::string> keys;
    static const int& __fastcall intGet(SettingsInput* p, void*, const std::string& key,
                                        const int&)                                    // +0x10
    { p->keys.push_back(key); return p->extent; }
};
// [TEMP]
// Vehicle observer exposes absolute transform +0x80 and networkable +0xD4; other slots abort.
// The networkable fixture supplies only the ID word at +4.
struct VehicleInput {
    void** table; float transform[16]; void* networkable;
    int transformQueries, networkableQueries;
    static const float* __fastcall getAbsoluteTransformation(VehicleInput* p, void*)  // +0x80
    { ++p->transformQueries; return p->transform; }
    static void* __fastcall getNetworkable(VehicleInput* p, void*)                   // +0xD4
    { ++p->networkableQueries; return p->networkable; }
};
struct NetworkableInput { void* table; unsigned short id; };
// [TEMP]
// Records getSquadInfo +0x28 and getRequest +0x18 calls; other slots abort.
struct SquadInfoInput {
    void** table; void* requests[4]; std::vector<int> types;
    static void* __fastcall getRequest(SquadInfoInput* p, void*, int type)            // +0x18
    { p->types.push_back(type); return type >= 0 && type < 4 ? p->requests[type] : nullptr; }
};
struct SquadManagerInput {
    void** table; SquadInfoInput* infos[10]; std::vector<std::pair<int, int> > queries;
    static void* __fastcall getSquadInfo(SquadManagerInput* p, void*, int team, int squad) // +0x28
    {
        p->queries.push_back(std::make_pair(team, squad));
        return squad >= 0 && squad < 10 ? p->infos[squad] : nullptr;
    }
};
// [TEMP]
// Supply-line observer exposes control-point team/capture/visibility and template ID/group queries.
// The object carries its template at +0x14; unused slots abort.
struct SupplyTemplateInput {
    void** table; int id; int group; int groupQueries;
    static void* __fastcall queryInterface(SupplyTemplateInput* p, void*, unsigned int iid) // +0x0C
    { return iid == world::IID_IControlPointTemplate ? p : nullptr; }
    static int __fastcall getControlPointId(SupplyTemplateInput* p, void*)                // +0x2C
    { return p->id; }
    static int __fastcall getGroupId(SupplyTemplateInput* p, void*)                       // +0xBC
    { ++p->groupQueries; return p->group; }
};
struct SupplyControlPointInput {
    void** table; int team; bool shown, capturableBy1; int teamQueries;
    std::vector<int> captureTeams;
    static int __fastcall getTeam(SupplyControlPointInput* p, void*)                      // +0x1C
    { ++p->teamQueries; return p->team; }
    static bool __fastcall getAllowCaptureByTeam(SupplyControlPointInput* p, void*, int team) // +0x2C
    { p->captureTeams.push_back(team); return p->capturableBy1; }
    static bool __fastcall getShowOnMinimap(SupplyControlPointInput* p, void*)            // +0x38
    { return p->shown; }
};
struct SupplyObjectInput {
    void** table; std::uint32_t words[4];
    SupplyTemplateInput* pointTemplate;                                  // +0x14, IObject::m_template
    SupplyControlPointInput* point;
    static void* __fastcall queryInterface(SupplyObjectInput* p, void*, unsigned int iid)   // +0x0C
    { return iid == world::IID_IControlPoint ? p->point : nullptr; }
};
static_assert(offsetof(SupplyObjectInput, pointTemplate) == 0x14, "IObject::m_template");
// [TEMP] the typed IGameLogic boundary for the supply line: the control-point map (+0x174) and
// the group count (+0x184). Everything else aborts by name, as StrategicLogicInput's does.
struct SupplyLineLogicInput : StrategicLogicInput {
    mutable std::map<unsigned int, world::IObject*> controlPoints;
    mutable int mapQueries = 0;
    int groups = 0, groupQueries = 0;
    std::map<unsigned int, world::IObject*>* getControlPointMap() const override
    { ++mapQueries; return &controlPoints; }
    int getNumberOfControlPointGroups() override
    { ++groupQueries; return groups; }
};

static const char* const expectedTextures[] = {
    "Ingame/Minimap/map_CombatArea32.dds", "Ingame/Minimap/map_Shadow.tga", "Ingame/Minimap/map_Frame.tga",
    "Ingame/Minimap/map_Mask.tga", "Ingame/Player/Icons/Minimap/mini_commander1.tga",
    "Ingame/Player/Icons/Minimap/mini_commander1.tga", "Ingame/Player/Icons/Minimap/mini_commander2.tga",
    "Ingame/Minimap/map_Line.tga", "Ingame/Flags/Icons/Minimap/SupplyLine/sl2_line.tga",
    "Ingame/Flags/Icons/Minimap/SupplyLine/sl2_arrow.tga", "Ingame/Minimap/Icons/request_circle.tga",
    "Ingame/Minimap/Icons/icon_artillery.tga", "Ingame/Minimap/Icons/icon_UAV.tga", "Ingame/Minimap/Icons/icon_supplies.tga",
    "Ingame/Minimap/Icons/icon_vehicledrop.tga", "Ingame/Minimap/Icons/orderMove.tga", "Ingame/Minimap/Icons/orderAttack.tga",
    "Ingame/Minimap/Icons/orderDefend.tga", "Ingame/Minimap/Icons/orderAttack.tga", "Ingame/Minimap/Icons/orderRepair.tga",
    "Ingame/Minimap/Icons/orderDestroy.tga", "Ingame/Minimap/Icons/orderMine.tga", "Ingame/Minimap/Icons/icon_artillery.tga",
    "Ingame/Minimap/Icons/icon_UAV.tga", "Ingame/Minimap/Icons/icon_supplies.tga", "Ingame/Minimap/Icons/icon_vehicledrop.tga",
    "Ingame/Minimap/Icons/orderMoveCommander.tga", "Ingame/Minimap/Icons/orderAttackCommander.tga",
    "Ingame/Minimap/Icons/orderDefendCommander.tga", "Ingame/Minimap/Icons/orderAttackCommander.tga",
    "Ingame/Minimap/Icons/orderRepairCommander.tga", "Ingame/Minimap/Icons/orderDestroyCommander.tga",
    "Ingame/Minimap/Icons/orderMineCommander.tga", "Ingame/Player/Icons/Minimap/mini_Squad_request.tga",
    "Ingame/Minimap/Icons/spawn_SelectedInactive.tga", "Ingame/Minimap/Icons/spawn_Selected.tga",
    "Ingame/Minimap/Icons/spawn_SquadSelectedInactive.tga", "Ingame/Minimap/Icons/spawn_SquadSelected.tga",
    "Ingame/Minimap/Icons/spawn_UnSelectedInactive.tga", "Ingame/Minimap/Icons/spawn_UnSelected.tga",
    "Ingame/Minimap/Icons/spawn_SquadUnSelectedInactive.tga", "Ingame/Minimap/Icons/spawn_SquadUnSelected.tga",
    "Ingame/Player/Icons/Minimap/mini_LocalArrow2.tga", "Ingame/Player/Icons/Minimap/mini_Soldier.tga",
    "Ingame/Player/Icons/Minimap/mini_Health.tga", "Ingame/Player/Icons/Minimap/mini_headingArrow.tga",
    "Ingame/Player/Icons/Minimap/mini_Medic.tga", "Ingame/Player/Icons/Minimap/mini_MedicArrow.tga",
    "Ingame/Player/Icons/Minimap/mini_Ammo.tga", "Ingame/Player/Icons/Minimap/mini_Repair.tga",
    "Ingame/Player/Icons/Minimap/mini_Revive.tga", "Ingame/Minimap/Icons/laserpaintTarget.tga",
    "Ingame/Minimap/Icons/laserpaintTargetActive.tga", "Ingame/Minimap/Icons/unitSpotted.tga",
    "Ingame/Player/Icons/Minimap/mini_SatelliteObject.tga", "Ingame/Player/Icons/Minimap/mini_SatelliteObject.tga",
    "Ingame/GeneralIcons/full.tga", "Ingame/GeneralIcons/empty.tga", "Ingame/Minimap/map_SatelliteScanLine.tga",
    "Ingame/Minimap/map_UAV.tga", "Ingame/Minimap/Icons/icon_UAV.tga", "Ingame/Minimap/Icons/placedAirstrike.tga",
    "Ingame/Minimap/Icons/icon_artillery.tga", "Ingame/Minimap/Icons/icon_supplies.tga",
};

// The real information layer registers itself with the event manager in its own constructor
// (HudInformationLayer.cpp: ECHud and ECGame at priority 0), so the bus is published first.
struct World {
    Bus bus; Info* info; FixtureSystem system; Bf2HudBuilder builder;
    Bf2SplitNode* root; Bf2MenuMapNode* map;
    World() : info(nullptr), root(nullptr), map(nullptr)
    {
        eventManager = &bus;
        info = new Info; g_hudInformationLayer = info;
        CHECK(bus.registered.size() == 2 && bus.registered[0] == 2 && bus.registered[1] == 1); // the layer's own two
        root = new Bf2SplitNode("IngameHud");
        system.registerObject(root, nullptr);
        builder.setSystem(&system);
        CHECK(builder.createSplitNode("IngameHud", "MapSplit"));
        bus.registered.clear(); bus.who.clear(); bus.priorities.clear();
    }
};

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    turnOffAllDebug(DebugTypeError); // fixture-only: the shipped post-Debug arms return instead of aborting
    static_assert(sizeof(void*) == 4, "native x86 only");
    static_assert(sizeof(Bf2TransformNode) == 60, "native Bf2TransformNode scalar storage");
    static_assert(sizeof(Bf2MenuMapNode) == 3000, "current release native STL layout");

    // [TEMP] the two globals family P's ECHud arm reads; see the note above them.
    void* playersTable[0x40 / 4];
    void* gameTable[0x120 / 4];
    void* adapterTable[0x40 / 4];
    for (auto& e : playersTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
    for (auto& e : gameTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
    for (auto& e : adapterTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
    void* mapPlayerTable[0x180 / 4];
    for (auto& e : mapPlayerTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
    playersTable[0x30 / 4] = reinterpret_cast<void*>(&PlayersInput::getLocalHumanPlayer);
    playersTable[0x20 / 4] = reinterpret_cast<void*>(&PlayersInput::getPlayerFromId);
    gameTable[0xEC / 4] = reinterpret_cast<void*>(&GameInput::getCurrentMapName);
    adapterTable[0x10 / 4] = reinterpret_cast<void*>(&AdapterInput::getBool);
    adapterTable[0x18 / 4] = reinterpret_cast<void*>(&AdapterInput::getInt);
    mapPlayerTable[0xC4 / 4] = reinterpret_cast<void*>(&MapPlayerInput::setCommanderCamOverhead);
    mapPlayerTable[0x44 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getVehicle);
    mapPlayerTable[0x68 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getIsAlive);
    mapPlayerTable[0x17C / 4] = reinterpret_cast<void*>(&MapPlayerInput::getCamera);
    mapPlayerTable[0xA0 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getDefaultVehicle);
    mapPlayerTable[0xFC / 4] = reinterpret_cast<void*>(&MapPlayerInput::getIsManDown);
    mapPlayerTable[0x50 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getId);
    mapPlayerTable[0xE4 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getTeam);
    mapPlayerTable[0x108 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getSquadId);
    mapPlayerTable[0x110 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getIsCommander);
    mapPlayerTable[0x138 / 4] = reinterpret_cast<void*>(&MapPlayerInput::getIsSquadLeader);
    PlayersInput players = { playersTable, 0 };
    GameInput fixtureGame = { gameTable, std::string("Strike_At_Karkand"), 0 };
    AdapterInput adapter = { adapterTable, false, std::vector<std::string>(),
                             0, std::vector<std::string>() };
    MapPlayerInput mapPlayer = { mapPlayerTable, 77, 0, false, false, 2 };
    PlayerIconManagerInput playerIconManager;
    world::playerManager = reinterpret_cast<world::PlayerManager*>(&players);
    game = reinterpret_cast<IGame*>(&fixtureGame);

    (void)argc; (void)argv;

    // descriptors
    CHECK(TypeDescriptor::find("dice::meme::Bf2MenuMapNode") == Bf2MenuMapNode::getClassType());
    CHECK(TypeDescriptor::find("dice::meme::Bf2TransformNode") == Bf2TransformNode::getClassType());
    CHECK(Bf2MenuMapNode::getClassType()->getBaseType() == Bf2TransformNode::getClassType());
    CHECK(Bf2TransformNode::getClassType()->getBaseType() == TransformNode::getClassType());
    CHECK(Bf2MenuMapNode::getClassType()->getInstanceSize() == 3000 && Bf2TransformNode::getClassType()->getInstanceSize() == 60);

    World w;
    // the stock line: hudBuilder.createMapNode MapSplit Minimap
    const int instancesBefore = Bf2MenuMapNode::getClassType()->getInstanceCount();
    CHECK(w.builder.createMapNode("MapSplit", "Minimap"));
    Bf2MenuMapNode* map = w.builder.m_field008;
    CHECK(map != nullptr);
    if (!map) { std::printf("no map\n"); return 1; }
    Bf2MenuMapNode& m = Bf2MenuMapNodePublicFixture::m(map);
    CHECK(!std::strcmp(map->getName(), "Minimap"));
    CHECK(map->getType() == Bf2MenuMapNode::getClassType());
    CHECK(map->getObjectSize() == 3000);
    CHECK(Bf2MenuMapNode::getClassType()->getInstanceCount() == instancesBefore + 1); // registerObject counted it
    CHECK(w.system.findNamedObject("Minimap") == map);
    // the transform named after the parent sits as MapSplit's child 1 and carries the map as its child 1
    Bf2SplitNode* split = static_cast<Bf2SplitNode*>(w.system.findNamedObject("MapSplit"));
    CHECK(split && split->getType() == Bf2SplitNode::getClassType()); // the first "MapSplit" is still the split
    Node* transform = split ? split->getChild(1) : nullptr;
    CHECK(transform && transform->getType() == Bf2TransformNode::getClassType());
    CHECK(transform && !std::strcmp(transform->getName(), "MapSplit"));
    CHECK(transform && transform->getChild(1) == map);
    CHECK(transform && static_cast<TransformNode*>(transform)->getSize().x == 800.0f && static_cast<TransformNode*>(transform)->getSize().y == 600.0f);
    CHECK(w.builder.m_field050 == "MapSplit");

    // constructor: the borrowed System, the 64 texture loads in shipped order, all through menuPath=true
    CHECK(m.m_system == &w.system);
    CHECK(w.system.textures.size() == 64 && w.system.menuFlags == 64);
    bool order = w.system.textures.size() == 64;
    for (size_t i = 0; order && i != w.system.textures.size(); ++i)
        if (w.system.textures[i] != expectedTextures[i]) { order = false; std::printf("texture %u: %s\n", unsigned(i), w.system.textures[i].c_str()); }
    CHECK(order);
    CHECK(m.m_defaultMinimapTexture == 57 && m.m_field8a0 == 58 && m.m_field89c == 34); // full, empty, mini_Squad_request
    CHECK(m.m_field8a8[13] == 23 && m.m_field8a8[8] == 17);                        // +8DC keeps its second load
    CHECK(m.m_controlPointTextures[0] == 0 && m.m_controlPointTextures[2] == 0);   // +904 loaded then zeroed
    CHECK(m.m_uavTexture == 60 && m.m_uavIcon == 61 && m.m_airstrikeTexture == 62 && m.m_artilleryIcon == 63 && m.m_suppliesTexture == 64);
    CHECK(m.m_fielda90 == 59 && m.m_fielda94 == 55 && m.m_field93c[22] == 56 && m.m_field93c[2] == 1 && m.m_field93c[3] == 1);
    CHECK(m.m_minimapTexture == 0);

    // 7755A0, virtual +13C: the base control-point record always receives a
    // white colour with the map alpha and a 32x32 size. Only the conjunction
    // `useTeamTexture && !notCapturable` selects the team texture; all other
    // flag combinations select the base texture. The null record is a silent
    // return before any member is touched.
    {
        const float savedAlpha = m.m_field6fc;
        int savedTeamTextures[3];
        int savedBaseTextures[3];
        for (int i = 0; i != 3; ++i) {
            savedTeamTextures[i] = m.m_controlPointTextures[i];
            savedBaseTextures[i] = m.m_controlPointBaseTextures[i];
            m.m_controlPointTextures[i] = 200 + i;
            m.m_controlPointBaseTextures[i] = 100 + i;
        }
        m.m_field6fc = 0.375f;

        MapIconRecord icon;
        Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
            map, &icon, 1, true, "ignored_map", false, true, true);
        CHECK(icon.texture == 201);
        CHECK(icon.size.x == 32.0f && icon.size.y == 32.0f);
        CHECK(icon.color.r == 1.0f && icon.color.g == 1.0f
              && icon.color.b == 1.0f && icon.color.a == 0.375f);

        icon.texture = -1;
        Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
            map, &icon, 1, false, "another_map", false, false, false);
        CHECK(icon.texture == 101);
        CHECK(icon.size.x == 32.0f && icon.size.y == 32.0f
              && icon.color.a == 0.375f);

        icon.texture = -1;
        Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
            map, &icon, 1, true, "gpm_cq", true, false, false);
        CHECK(icon.texture == 101);
        Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
            map, nullptr, 1, true, "ignored", false, false, false);

        m.m_field6fc = savedAlpha;
        for (int i = 0; i != 3; ++i) {
            m.m_controlPointTextures[i] = savedTeamTextures[i];
            m.m_controlPointBaseTextures[i] = savedBaseTextures[i];
        }
    }

    // the information-layer registrations: ten named, then MapFilter0..31 (decoded 32)
    CHECK(w.info->names.size() == 42);
    static const char* const first[] = {"f:MinimapDelayedMapAngle", "f:MinimapSatelliteTimeUntilReloaded", "b:MinimapSatelliteActive",
        "b:MinimapSatelliteReloading", "f:MinimapUAVTimeUntilReloaded", "b:MinimapUAVState", "b:MinimapUAVActive", "b:MinimapUAVReloading",
        "b:MinimapShowKitsOnMap", "i:MinimapZoomDisplay"};
    for (int i = 0; i != 10 && i < int(w.info->names.size()); ++i) CHECK(w.info->names[i] == first[i]);
    for (int i = 0; i != 32 && 10 + i < int(w.info->names.size()); ++i) {
        char expected[64]; std::sprintf(expected, "b:MinimapMapFilter%iActive", i);
        CHECK(w.info->names[10 + i] == expected);
        CHECK(w.info->targets[10 + i] == &m.m_mapFilterActive[i]);
    }
    CHECK(w.info->targets.size() == 42 && w.info->targets[0] == &m.m_delayedMapAngle && w.info->targets[8] == &m.m_showKitsOnMap && w.info->targets[9] == &m.m_zoomDisplay);
    CHECK(w.info->targets[2] == &m.m_satelliteActive && w.info->targets[6] == &m.m_uavActive);

    // event face registered for ECHud (2) and ECCore (5) at priority 0, through the +3C face
    IEventListener* face = map;
    CHECK(w.bus.registered.size() == 2 && w.bus.registered[0] == 2 && w.bus.registered[1] == 5);
    CHECK(w.bus.who.size() == 2 && w.bus.who[0] == face && w.bus.who[1] == face && w.bus.priorities[0] == 0 && w.bus.priorities[1] == 0);

    // decoded and replayed constants
    CHECK(m.m_zoomLevel == 1 && m.m_zoomDisplay == 0 && m.m_spawnGroup == 0 && m.m_field674 == 1 && m.m_field678 == 8);
    CHECK(map->getPosition().x == 197.0f && map->getPosition().y == -300.0f && map->getSize().x == 194.0f && map->getSize().y == 194.0f);
    CHECK(m.m_mapExtent.x == 512.0f && m.m_mapExtent.y == 512.0f && m.m_mapOffset.x == 0.0f && m.m_fieldb84 == 20.0f);
    CHECK(m.m_commanderPosition.x == -4000.0f && m.m_field7b0.x == 558.0f && m.m_field7a8.y == 20.0f && m.m_textureSize.y == -297.0f);
    CHECK(m.m_uavRadius == 100 && m.m_satelliteLineSize == 150 && m.m_satelliteReloadFraction == 1.0f && m.m_uavReloadFraction == 1.0f);
    CHECK(m.m_zoomExponents[0][2] == 2.5f && m.m_zoomExponents[1][0] == 0.0f && m.m_zoomExponents[2][2] == 2.0f);
    CHECK(m.m_iconScales[0].x == 0.7f && m.m_iconScales[0].y == 0.7f && m.m_iconScales[4].y == -1.0f && m.m_iconScales[8].y == -1.0f && m.m_iconScales[9].y == 0.7f);
    CHECK(m.m_filterStates[0].to_ullong() == 0x000002BF7F437FFBULL && m.m_filterStates[3].to_ullong() == 0x000001BF7F43FFFBULL);
    CHECK(m.m_filterSets[0][0].to_ullong() == 0x000000308001045BULL && m.m_filterSets[5][5].to_ullong() == 0x000001C341380101ULL);
    CHECK(m.m_filterSets[6][1].count() == 43 && m.m_filterSets[6][0].none() && m.m_filterSets[6][5].count() == 43);
    CHECK(m.m_field3c0.to_ullong() == 0x0000007F000000FFULL);
    bool filtersOff = true; for (int i = 0; i != 32; ++i) filtersOff = filtersOff && !m.m_mapFilterActive[i];
    CHECK(filtersOff && !m.m_showKitsOnMap && m.m_field36c);
    bool reserved = true; for (int i = 0; i != 43; ++i) reserved = reserved && m.m_icons[i].capacity() >= 32 && m.m_icons[i].empty();
    CHECK(reserved);
    CHECK(m.m_fieldb4c.r > 0.392f && m.m_fieldb4c.r < 0.393f && m.m_fieldb4c.b > 0.941f && m.m_fieldb4c.a == 1.0f);
    CHECK(m.m_fielda70 == -1 && m.m_fielda7b && m.m_field7f9 && m.m_field775 && m.m_field776 && !m.m_field774);
    CHECK(m.m_selectedRequestSquad == -1 && m.m_selectedRequestOrderType == static_cast<OrderType>(2) && m.m_fieldb44 == 6);
    CHECK(m.m_controlPoints.empty() && m.m_vehicleDrops.empty() && m.m_timedMarkers.empty() && m.m_timedMarkers.capacity() == 0);
    CHECK(m.m_uavPosition.x == 0.0f && m.m_airstrikePosition.z == 0.0f && m.m_suppliesPosition.y == 0.0f);
    CHECK(!m.m_uavActive && !m.m_airstrikeActive && !m.m_suppliesActive && !m.m_satelliteReloading && !m.m_expandedMap);

    // 772580: updateSelf only claims the pointer while +6C is set and the
    // pointer is strictly inside the supplied bounds. Edges, outside values and
    // unordered coordinates all leave the current owner untouched.
    const Rect updateBounds = {10.0f, 20.0f, 100.0f, 80.0f, 0.0f};
    w.system.setPointerOwner(nullptr);
    w.system.setFixturePointer(50.0f, 50.0f);
    CHECK(map->updateSelf(&w.system, nullptr, &updateBounds, 123.0f));
    CHECK(w.system.getPointerOwner() == nullptr);             // +6C false
    m.m_field06c = true;
    for (const Vec2 edge : {Vec2{10.0f, 50.0f}, Vec2{110.0f, 50.0f},
                            Vec2{50.0f, 20.0f}, Vec2{50.0f, 100.0f},
                            Vec2{9.0f, 50.0f}, Vec2{50.0f, 101.0f}}) {
        w.system.setPointerOwner(nullptr);
        w.system.setFixturePointer(edge.x, edge.y);
        CHECK(map->updateSelf(&w.system, nullptr, &updateBounds, -1.0f));
        CHECK(w.system.getPointerOwner() == nullptr);
    }
    w.system.setFixturePointer(std::numeric_limits<float>::quiet_NaN(), 50.0f);
    CHECK(map->updateSelf(&w.system, nullptr, &updateBounds, 0.0f));
    CHECK(w.system.getPointerOwner() == nullptr);
    w.system.setFixturePointer(50.0f, 50.0f);
    CHECK(map->updateSelf(&w.system, nullptr, &updateBounds, 0.0f));
    CHECK(w.system.getPointerOwner() == map);
    m.m_field06c = false;

    // the crash line of HudKitSelection::setSpawnGroup now has an object to write to
    m.m_spawnGroup = 7; CHECK(m.m_spawnGroup == 7); m.m_spawnGroup = 0;

    // event face: the join-path ids (3, 4, 31, 67) and the other no-op ids leave the map alone
    for (unsigned id : {3u, 4u, 5u, 31u, 67u, 100u, 0u}) map->handleEvent(static_cast<EventCategory>(2), id, nullptr, nullptr);
    for (unsigned id : {13u, 2u, 8u}) map->handleEvent(static_cast<EventCategory>(1), id, nullptr, nullptr); // other categories: ignored
    CHECK(m.m_zoomLevel == 1 && m.m_mapExtent.x == 512.0f && !m.m_satelliteReloading);
    // ECCore 13: offset flag/vector cleared, extent back to 512
    m.m_fieldb70 = true; m.m_mapOffset.x = 5.0f; m.m_mapOffset.y = 6.0f; m.m_mapExtent.x = 1.0f; m.m_mapExtent.y = 2.0f;
    map->handleEvent(static_cast<EventCategory>(5), 12, nullptr, nullptr);
    CHECK(m.m_fieldb70 && m.m_mapOffset.x == 5.0f);                       // only id 13 acts
    map->handleEvent(static_cast<EventCategory>(5), 13, nullptr, nullptr);
    CHECK(!m.m_fieldb70 && m.m_mapOffset.x == 0.0f && m.m_mapOffset.y == 0.0f && m.m_mapExtent.x == 512.0f && m.m_mapExtent.y == 512.0f);
    // ECHud 8: both reload resets run (null GameLogic arms); ECHud 82: only the UAV reset
    m.m_satelliteActive = true; m.m_uavActive = true;
    map->handleEvent(static_cast<EventCategory>(2), 8, nullptr, nullptr);
    CHECK(!m.m_satelliteActive && m.m_satelliteReloading && !m.m_uavActive && m.m_uavReloading);
    m.m_uavReloading = false; m.m_satelliteReloading = false;
    map->handleEvent(static_cast<EventCategory>(2), 82, nullptr, nullptr);
    CHECK(m.m_uavReloading && !m.m_satelliteReloading);
    // ECHud 42: the airstrike marker from a 13-byte payload (three floats and the active byte)
    struct Payload { float x, y, z; unsigned char active; } payload = {10.0f, 20.0f, 30.0f, 1};
    map->handleEvent(static_cast<EventCategory>(2), 42, nullptr, &payload);
    CHECK(m.m_airstrikeActive && m.m_airstrikePosition.x == 10.0f && m.m_airstrikePosition.z == 30.0f && m.m_fieldb0c == 25.0f);
    payload.active = 0;
    map->handleEvent(static_cast<EventCategory>(2), 42, nullptr, &payload);
    CHECK(!m.m_airstrikeActive);
    map->handleEvent(static_cast<EventCategory>(2), 42, nullptr, nullptr);
    // ECHud 75 requires a HUD singleton and is excluded here.
    // BuilderMap console rows act directly on the builder's map handle and cannot report failure.
    FixtureHud* const singleton = new FixtureHud;
    hudManager = singleton;

    // Checks team-presence clearing with an empty player list and a null entry.
    // No Player/Object is fabricated; live-player team publication is outside this fixture.
    world::PlayerManager* const savedPlayerManager = world::playerManager;
    world::playerManager = &playerIconManager;
    m.m_playerTeamPresent[0] = true;
    m.m_playerTeamPresent[1] = true;
    m.m_field082[0] = 0x5Au;
    m.m_field082[1] = 0xA5u;
    collectPlayerIconsForFixture(map);
    CHECK(playerIconManager.playerQueries == 1);
    CHECK(playerIconManager.localQueries == 1);
    CHECK(!m.m_playerTeamPresent[0] && !m.m_playerTeamPresent[1]);
    CHECK(m.m_field082[0] == 0x5Au && m.m_field082[1] == 0xA5u);

    playerIconManager.players.push_back(nullptr);
    m.m_playerTeamPresent[0] = true;
    m.m_playerTeamPresent[1] = true;
    collectPlayerIconsForFixture(map);
    CHECK(playerIconManager.playerQueries == 2);
    CHECK(playerIconManager.localQueries == 2);
    CHECK(!m.m_playerTeamPresent[0] && !m.m_playerTeamPresent[1]);
    CHECK(m.m_field082[0] == 0x5Au && m.m_field082[1] == 0xA5u);
    bool playerIconsEmpty = m.m_timedMarkers.empty();
    for (int i = 0; i != 43; ++i)
        playerIconsEmpty = playerIconsEmpty && m.m_icons[i].empty();
    CHECK(playerIconsEmpty);
    playerIconManager.players.clear();
    world::playerManager = savedPlayerManager;

    // 77E500 has two deterministic boundaries before it needs a strategic
    // object: null GameLogic returns immediately; a non-null logic with an
    // empty real registry obtains the local player once and leaves every icon
    // family untouched. Seed family 16 so this checks preservation rather than
    // merely observing that empty containers stayed empty.
    {
        IGameLogic* const savedGameLogic = gameLogic;
        world::PlayerManager* const savedStrategicPlayerManager = world::playerManager;
        StrategicLogicInput strategicLogic;
        MapIconRecord sentinel;
        sentinel.position = Vec2{11.0f, 12.0f};
        sentinel.color = Color{0.1f, 0.2f, 0.3f, 0.4f};
        sentinel.size = Vec2{13.0f, 14.0f};
        sentinel.rotation = 15.0f;
        sentinel.texture = 16;
        sentinel.clampToVisibleMap = true;
        m.m_icons[16].push_back(sentinel);
        const auto sentinelUntouched = [&m]() {
            if (m.m_icons[16].size() != 1)
                return false;
            const MapIconRecord& icon = m.m_icons[16][0];
            return icon.position.x == 11.0f && icon.position.y == 12.0f
                && icon.color.r == 0.1f && icon.color.g == 0.2f
                && icon.color.b == 0.3f && icon.color.a == 0.4f
                && icon.size.x == 13.0f && icon.size.y == 14.0f
                && icon.rotation == 15.0f && icon.texture == 16
                && icon.clampToVisibleMap;
        };

        std::size_t iconSizes[43];
        for (int i = 0; i != 43; ++i)
            iconSizes[i] = m.m_icons[i].size();
        const std::size_t timedMarkerCount = m.m_timedMarkers.size();
        const int localQueriesBefore = playerIconManager.localQueries;
        world::playerManager = &playerIconManager;
        gameLogic = nullptr;
        collectStrategicIconsForFixture(map);
        CHECK(playerIconManager.localQueries == localQueriesBefore);
        bool nullLogicIconsUntouched = m.m_timedMarkers.size() == timedMarkerCount;
        for (int i = 0; i != 43; ++i)
            nullLogicIconsUntouched = nullLogicIconsUntouched
                && m.m_icons[i].size() == iconSizes[i];
        CHECK(nullLogicIconsUntouched);
        CHECK(sentinelUntouched());

        gameLogic = &strategicLogic;
        collectStrategicIconsForFixture(map);
        CHECK(strategicLogic.strategicQueries == 1);
        CHECK(playerIconManager.localQueries == localQueriesBefore + 1);
        bool emptyMapIconsUntouched = m.m_timedMarkers.size() == timedMarkerCount;
        for (int i = 0; i != 43; ++i)
            emptyMapIconsUntouched = emptyMapIconsUntouched
                && m.m_icons[i].size() == iconSizes[i];
        CHECK(emptyMapIconsUntouched);
        CHECK(sentinelUntouched());

        m.m_icons[16].clear();
        gameLogic = savedGameLogic;
        world::playerManager = savedStrategicPlayerManager;
    }


    // 777030, the real +108 scaling predicate inherited by the mini-map too.
    // Save the three touched fields because the larger fixture continues below
    // with the constructor state as its baseline.
    {
        const bool savedZoomIcons = m.m_field7f9;
        const bool savedExpanded = m.m_expandedMap;
        const int savedZoomLevel = m.m_zoomLevel;

        m.m_field7f9 = false;
        m.m_expandedMap = false;
        m.m_zoomLevel = 0;
        CHECK(!map->shouldScaleMapIcon(4));
        m.m_field7f9 = true;
        CHECK(!map->shouldScaleMapIcon(4));
        m.m_zoomLevel = 1;
        CHECK(map->shouldScaleMapIcon(4));
        m.m_expandedMap = true;
        CHECK(!map->shouldScaleMapIcon(4));
        m.m_zoomLevel = 99;
        CHECK(map->shouldScaleMapIcon(4));

        m.m_field7f9 = savedZoomIcons;
        m.m_expandedMap = savedExpanded;
        m.m_zoomLevel = savedZoomLevel;
    }

    CHECK(map->setMaxiPosForHud(-122.0f, -273.0f));             // row 36, +7C0
    CHECK(m.m_field7c0.x == -122.0f && m.m_field7c0.y == -273.0f);
    CHECK(map->setMaxiSizeForHud(512.0f, 512.0f));              // row 35, +7B8
    CHECK(m.m_textureSize.x == 512.0f && m.m_textureSize.y == 512.0f);
    CHECK(map->setCommanderPosForHud(-161.0f, -281.0f));        // row 38, +7A8
    CHECK(m.m_field7a8.x == -161.0f && m.m_field7a8.y == -281.0f);
    CHECK(map->setCommanderSizeForHud(561.0f, 561.0f));         // row 37, +7B0
    CHECK(m.m_field7b0.x == 561.0f && m.m_field7b0.y == 561.0f);
    CHECK(map->setZoomIconsForHud(false) && !m.m_field7f9);     // row 39, +7F9
    CHECK(map->setZoomIconsForHud(true) && m.m_field7f9);
    // the constructor's two shadow copies at +778/+780 are left alone by all nine
    CHECK(m.m_field778.x == 194.0f && m.m_field780.x == 197.0f && m.m_field780.y == -300.0f);

    // Row 33 also drives the node's own +C8 slot, which is Bf2TransformNode's size
    // accessor inherited unchanged - so the pair lands in TWO places.
    CHECK(map->setMiniSizeForHud(197.0f, 197.0f));
    CHECK(m.m_field798.x == 197.0f && m.m_field798.y == 197.0f);
    CHECK(map->getSize().x == 197.0f && map->getSize().y == 197.0f);

    // Row 34 stores the mini position and hands the same pair to 773630, whose
    // result it discards. With the commander view down - all the HUD build path
    // ever sees - that only records the offset at +740. The commander-up arm
    // re-centres through the declaration-only +11C seam and is not run here.
    CHECK(map->setMiniPosForHud(201.0f, -305.0f));
    CHECK(m.m_field788.x == 201.0f && m.m_field788.y == -305.0f);
    CHECK(m.m_miniViewOffset.x == 201.0f && m.m_miniViewOffset.y == -305.0f);
    CHECK(singleton->commanderQueries == 1);                          // one query per call
    CHECK(map->getSize().x == 197.0f);                          // row 34 leaves the size
    CHECK(m.m_field748.x == 0.0f && m.m_field748.y == 0.0f
          && m.m_field750.x == 0.0f && m.m_field750.y == 0.0f); // +748..+754 stay zero

    // Row 40 loads through the map's OWN System and stores the handle verbatim.
    const std::size_t fontsBefore = w.system.fonts.size();
    map->setControlPointFontForHud(std::string("Fonts/commanderChatFontLocal_6.dif"));
    CHECK(w.system.fonts.size() == fontsBefore + 1);
    CHECK(w.system.fonts.back() == "Fonts/commanderChatFontLocal_6.dif");
    CHECK(m.m_fieldc64 == static_cast<std::uint32_t>(w.system.fonts.size()));
    // A map with no System silently keeps the handle it had - and still reports
    // nothing either way, because the console row's declared type is void.
    System* const borrowed = m.m_system;
    m.m_system = nullptr;
    map->setControlPointFontForHud(std::string("Fonts/never.dif"));
    CHECK(w.system.fonts.size() == fontsBefore + 1);
    CHECK(m.m_fieldc64 == static_cast<std::uint32_t>(w.system.fonts.size()));
    m.m_system = borrowed;

    // Row 41 copies four floats straight into +C68 and leaves the +B4C colour alone.
    map->setControlPointFontColorForHud(1.0f, 0.792f, 0.043f, 0.5f);
    CHECK(m.m_fieldc68.r == 1.0f && m.m_fieldc68.g == 0.792f);
    CHECK(m.m_fieldc68.b == 0.043f && m.m_fieldc68.a == 0.5f);
    CHECK(m.m_fieldb4c.a == 1.0f && m.m_fieldb4c.r > 0.392f && m.m_fieldb4c.r < 0.393f);

    // ECHud 10 reads MapStatic synchronously from the information adapter and clears related state.
    // Install the adapter explicitly: the HUD constructor leaves it null and this path has no guard.
    singleton->m_field104 = reinterpret_cast<hud::HudInformationAdapter*>(&adapter);
    m.m_field75c = 0x1234u; m.m_delayedMapAngle = 7.5f; m.m_field76c = 3.25f;
    adapter.mapStatic = true;
    map->handleEvent(static_cast<EventCategory>(2), 10, nullptr, nullptr);
    CHECK(adapter.boolQueries.size() == 1 && adapter.boolQueries[0] == "MapStatic");
    CHECK(m.m_mapStatic);
    CHECK(m.m_field75c == 0 && m.m_delayedMapAngle == 0.0f && m.m_field76c == 0.0f);
    // Only the LOW byte of +73C is written; +770 is NOT one of the three.
    m.m_field770 = 0x9ABCu;
    adapter.mapStatic = false;
    map->handleEvent(static_cast<EventCategory>(2), 10, nullptr, &adapter);  // payload ignored
    CHECK(!m.m_mapStatic && adapter.boolQueries.size() == 2);
    CHECK(m.m_field770 == 0x9ABCu);
    singleton->m_field104 = nullptr;

    // Tests setHudMode directly, including its unconditional filter/UAV prologue.
    // Modes that reach expansion or sound frontiers are checked separately; no local player is supplied.
    m.m_field36c = true;                          // the constructor's value: tail returns early
    m.m_field674 = 9; m.m_uavState = true; m.m_field06c = false;
    map->setHudMode(22);
    CHECK(m.m_field674 == 1 && !m.m_uavState && m.m_field06c);
    m.m_field06c = false; map->setHudMode(24); CHECK(m.m_field06c);
    m.m_field06c = false; map->setHudMode(25); CHECK(m.m_field06c);
    // 23 puts the UAV state back up after the prologue has just cleared it.
    m.m_field06c = false; map->setHudMode(23);
    CHECK(m.m_uavState && m.m_field06c);
    static const int offModes[] = { 9, 12, 13, 14, 16, 19, 20, 21, 26, 27, 28 };
    for (int off : offModes) { m.m_field06c = true; map->setHudMode(off); CHECK(!m.m_field06c); }
    // The gap modes (3..8, 10, 11) and anything out of range leave +6C untouched.
    m.m_field06c = true;  map->setHudMode(5);   CHECK(m.m_field06c);
    m.m_field06c = false; map->setHudMode(11);  CHECK(!m.m_field06c);
    m.m_field06c = true;  map->setHudMode(999); CHECK(m.m_field06c);
    m.m_field06c = false; map->setHudMode(-3);  CHECK(!m.m_field06c);
    // 15 returns early when the map is already in commander mode, so it never
    // reaches setExpanded and m_field674 keeps the prologue's 1, not the arm's 2.
    m.m_commanderMode = true; m.m_field06c = false; m.m_field674 = 0;
    map->setHudMode(15);
    CHECK(m.m_field06c && m.m_field674 == 1 && m.m_commanderMode);
    // 17 and 18 return early when the previous mode is the other one (+1E4).
    singleton->m_field254 = 20; m.m_field06c = false;
    map->setHudMode(17); CHECK(m.m_field06c);
    singleton->m_field254 = 19; m.m_field06c = false;
    map->setHudMode(18); CHECK(m.m_field06c);
    singleton->m_field254 = 0;

    // The tail every path falls through to: filter row 1 is subtracted from the
    // live states, but only while +36C is false AND row 1 is already wholly
    // contained in state 0. The prologue overwrites the live states from row 2
    // first, so row 2 is what seeds them.
    m.m_field36c = false;
    for (int i = 0; i != 6; ++i) { m.m_filterSets[2][i].set(); m.m_filterSets[1][i].reset(); }
    m.m_filterSets[1][0].set(3); m.m_filterSets[1][1].set(5);
    map->setHudMode(22);
    CHECK(!m.m_filterStates[0].test(3) && !m.m_filterStates[1].test(5));
    CHECK(m.m_filterStates[0].count() == 42 && m.m_filterStates[1].count() == 42);
    // Not contained -> the whole six-row subtraction is skipped.
    for (int i = 0; i != 6; ++i) { m.m_filterSets[2][i].reset(); m.m_filterSets[1][i].reset(); }
    m.m_filterSets[1][0].set(7);
    map->setHudMode(22);
    CHECK(m.m_filterStates[0].none());
    // +36C true short-circuits it even when row 1 IS contained.
    m.m_field36c = true;
    for (int i = 0; i != 6; ++i) { m.m_filterSets[2][i].set(); m.m_filterSets[1][i].reset(); }
    m.m_filterSets[1][0].set(3);
    map->setHudMode(22);
    CHECK(m.m_filterStates[0].test(3) && m.m_filterStates[0].count() == 43);


    // setViewCenter adds half a unit to x, subtracts it from y, then clamps both axes.
    m.m_minCenterX = -2.0f; m.m_maxCenterX = 3.0f;
    m.m_minCenterY = -4.0f; m.m_maxCenterY = 5.0f;
    map->setViewCenter(Vec2{1.0f, 1.0f});
    CHECK(m.m_viewCenter.x == 1.5f && m.m_viewCenter.y == 0.5f);
    map->setViewCenter(Vec2{100.0f, 100.0f});
    CHECK(m.m_viewCenter.x == 3.0f && m.m_viewCenter.y == 5.0f);      // clamped up
    map->setViewCenter(Vec2{-100.0f, -100.0f});
    CHECK(m.m_viewCenter.x == -2.0f && m.m_viewCenter.y == -4.0f);
    // With commander mode and field690 clear, setExpanded skips zoom updates.
    // A null gameLogic supplies zero map-centre offset, leaving only the half-unit bias.
    m.m_commanderMode = false; m.m_field690 = false;
    m.m_expandedMap = false; m.m_field774 = true;
    m.m_minCenterX = -9.0f; m.m_maxCenterX = 9.0f;
    m.m_minCenterY = -9.0f; m.m_maxCenterY = 9.0f;
    map->setExpanded(true);
    CHECK(m.m_expandedMap && !m.m_field774);
    CHECK(m.m_viewCenter.x == 0.5f && m.m_viewCenter.y == -0.5f);


    // ECHud 2 on the MAP now runs end to end. It used to be a death run: with
    // updateViewBounds (+120) and recenterOnPlayer (+118) written, mode 0's whole
    // arm completes and the only thing left undone is the audible half, which the
    // sound seam reports and continues past.
    singleton->m_field250 = 0;                        // getMode() -> mode 0
    m.m_field690 = true; m.m_commanderMode = true; m.m_squadMode = true;
    m.m_field06c = true; m.m_expandedMap = true; m.m_overheadZoomActive = true;
    m.m_showKitsOnMap = true; m.m_field708[1] = 9.0f; m.m_field774 = true;
    m.m_satelliteActive = true; m.m_fielda8c = 7u;
    m.m_satelliteScanX = 9.0f; m.m_satelliteScanY = 9.0f; m.m_zoomLevel = 9;
    // setExpanded runs early in arm 0, so setViewCenter clamps against these -
    // updateViewBounds only replaces them afterwards, from inside setZoom.
    m.m_minCenterX = -9.0f; m.m_maxCenterX = 9.0f;
    m.m_minCenterY = -9.0f; m.m_maxCenterY = 9.0f;
    map->handleEvent(static_cast<EventCategory>(2), 2, nullptr, nullptr);
    CHECK(!m.m_field690 && !m.m_commanderMode && !m.m_squadMode && !m.m_field06c);
    CHECK(!m.m_expandedMap && !m.m_overheadZoomActive && !m.m_field774);
    // resetMapFilterBits put the icon bitset back to the constructor's value.
    CHECK(!m.m_showKitsOnMap && m.m_field3c0.to_ullong() == 0x7F000000FFull);
    CHECK(m.m_field708[1] == 0.0f);
    CHECK(m.m_field780.x == m.m_field788.x && m.m_field778.y == m.m_field798.y);
    CHECK(!m.m_satelliteActive && m.m_fielda8c == 0u);
    CHECK(m.m_satelliteScanX == 0.0f && m.m_satelliteScanY == 0.0f);
    // updateViewBounds's NOT-expanded arm plants the four constants...
    CHECK(m.m_minCenterY == -1.0f && m.m_maxCenterY == 0.0f);
    CHECK(m.m_minCenterX == 0.0f && m.m_maxCenterX == 1.0f);
    // ...and setViewCenter had already biased (0,0) by half a unit each way.
    CHECK(m.m_viewCenter.x == 0.5f && m.m_viewCenter.y == -0.5f);
    CHECK(m.m_zoomLevel == 0);
    // No local player means no vehicle-specific saved zoom entry.
    // ECHud 76 restores the saved zoom for the active view.
    m.m_savedZoom = 1; m.m_savedCommanderZoom = 2; m.m_savedSquadZoom = 0;
    // A collapsed map ignores it outright.
    m.m_expandedMap = false; m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 76, nullptr, nullptr);
    CHECK(m.m_zoomLevel == 7);
    // +690 wins over both mode flags and asks for level 0 without remembering.
    m.m_expandedMap = true; m.m_field690 = true;
    m.m_commanderMode = true; m.m_squadMode = true; m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 76, nullptr, nullptr);
    CHECK(m.m_zoomLevel == 0);
    // Then commander, then squad, then the plain saved zoom - in that order.
    m.m_field690 = false; m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 76, nullptr, nullptr);
    CHECK(m.m_zoomLevel == 2);                       // m_savedCommanderZoom
    m.m_commanderMode = false; m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 76, nullptr, nullptr);
    CHECK(m.m_zoomLevel == 0);                       // m_savedSquadZoom
    m.m_squadMode = false; m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 76, nullptr, nullptr);
    CHECK(m.m_zoomLevel == 1);                       // m_savedZoom

    // ECHud 9 - the zoom the HUD remembers, read back by name.
    singleton->m_field104 = reinterpret_cast<hud::HudInformationAdapter*>(&adapter);
    adapter.mapZoom = 2; adapter.intQueries.clear(); m.m_zoomLevel = 7;
    map->handleEvent(static_cast<EventCategory>(2), 9, nullptr, nullptr);
    CHECK(adapter.intQueries.size() == 1 && adapter.intQueries[0] == "MapZoom");
    CHECK(m.m_zoomLevel == 2);
    singleton->m_field104 = nullptr;

    // ECHud 14/20/21/22/24/37 - a role change, and 33 which adds the vehicle drop.
    // The target is looked up before the identity check, so a stray id still
    // costs a lookup - that is what the second case asserts.
    players.local = &mapPlayer;
    players.byId = &mapPlayer;
    mapPlayer.id = 77; mapPlayer.squad = 0; mapPlayer.commander = false;
    mapPlayer.leader = true;                          // -> filter row 4
    for (int i = 0; i != 6; ++i) { m.m_filterSets[4][i].reset(); m.m_filterStates[i].reset(); }
    m.m_filterSets[4][0].set(9);
    players.idLookups.clear(); m.m_field674 = 0;
    int rolePayload[2] = { 77, 0 };
    map->handleEvent(static_cast<EventCategory>(2), 20, nullptr, rolePayload);
    CHECK(players.idLookups.size() == 1 && players.idLookups[0] == 77);
    CHECK(m.m_filterStates[0].test(9) && m.m_field674 == 2);
    // A payload naming someone else: looked up, then dropped.
    for (int i = 0; i != 6; ++i) m.m_filterStates[i].reset();
    players.idLookups.clear(); m.m_field674 = 0;
    rolePayload[0] = 78;
    map->handleEvent(static_cast<EventCategory>(2), 37, nullptr, rolePayload);
    CHECK(players.idLookups.size() == 1 && players.idLookups[0] == 78);
    CHECK(!m.m_filterStates[0].test(9) && m.m_field674 == 0);
    // id 33 sets the vehicle-drop availability either WAY - matched or not.
    singleton->m_field2d0 = 0.0f;
    rolePayload[0] = 78; *reinterpret_cast<float*>(&rolePayload[1]) = 3.5f;
    map->handleEvent(static_cast<EventCategory>(2), 33, nullptr, rolePayload);
    CHECK(singleton->m_field2d0 == 3.5f);
    CHECK(!m.m_filterStates[0].test(9));             // still not our player

    // Exercises five event IDs here. IDs 43, 46..50 and 60 need NetworkManager;
    // ID 41's live arm needs GameLogic and the sound facade, which this fixture omits.
    {
        // ECHud 25 (7832A2): +F8 772CA0 (setSquadRequestType) stores payload word [2] - the
        // request type - for the squad in word [0]; a negative squad clears all ten; a null
        // payload does nothing.
        int words[3] = { 7, 0xDEAD, 0xABCD };                          // [1] is never read
        for (int i = 0; i != 10; ++i) m.m_squadRequestTypes[i] = 0x11;
        map->handleEvent(static_cast<EventCategory>(2), 25, nullptr, words);
        CHECK(m.m_squadRequestTypes[7] == 0xABCD && m.m_squadRequestTypes[6] == 0x11
              && m.m_squadRequestTypes[8] == 0x11);
        map->handleEvent(static_cast<EventCategory>(2), 25, nullptr, nullptr);
        CHECK(m.m_squadRequestTypes[7] == 0xABCD);
        words[0] = -1;
        map->handleEvent(static_cast<EventCategory>(2), 25, nullptr, words);
        bool cleared = true;
        for (int i = 0; i != 10; ++i) cleared = cleared && m.m_squadRequestTypes[i] == 0;
        CHECK(cleared);

        // ECHud 41 (7830EF): with no GameLogic the arm returns before touching anything.
        m.m_satelliteReloadRemaining = 0.0f; m.m_satelliteActive = false; m.m_satelliteReloading = false;
        map->handleEvent(static_cast<EventCategory>(2), 41, nullptr, nullptr);
        CHECK(!m.m_satelliteActive && !m.m_satelliteReloading);

        // ECHud 74 (783247): the HUD's availability (+330) and one appended vehicle drop whose
        // icon is (re)loaded through the System on this very call.
        struct { int id; float availability; float x, y, z; } drop = { 5, 7.25f, 1.0f, 2.0f, 3.0f };
        const std::size_t dropsBefore = m.m_vehicleDrops.size();
        const std::size_t texturesBefore74 = w.system.textures.size();
        map->handleEvent(static_cast<EventCategory>(2), 74, nullptr, &drop);
        CHECK(singleton->m_field2d0 == 7.25f);
        CHECK(m.m_vehicleDrops.size() == dropsBefore + 1);
        CHECK(w.system.textures.size() == texturesBefore74 + 1
              && w.system.textures.back() == "Ingame/Minimap/Icons/icon_vehicledrop.tga");
        if (!m.m_vehicleDrops.empty()) {
            const MapVehicleDrop& added = m.m_vehicleDrops.back();
            CHECK(added.active && added.id == 5
                  && added.texture == static_cast<int>(w.system.textures.size())
                  && added.position.x == 1.0f && added.position.y == 2.0f && added.position.z == 3.0f);
        }
        m.m_vehicleDrops.clear();

        // ECHud 77 (782B3D). Not a commander: +7F8 drops, then the player's own +C4(false).
        const bool savedCommander = m.m_commanderMode; const int savedZoom = m.m_zoomLevel;
        m.m_commanderMode = false; m.m_overheadZoomActive = true;
        map->handleEvent(static_cast<EventCategory>(2), 77, nullptr, nullptr);
        CHECK(!m.m_overheadZoomActive && mapPlayer.overheadCalls == 1 && !mapPlayer.overheadValue);
        // A commander at zoom level 2: nothing at all.
        m.m_commanderMode = true; m.m_zoomLevel = 2; m.m_overheadZoomActive = true;
        map->handleEvent(static_cast<EventCategory>(2), 77, nullptr, nullptr);
        CHECK(m.m_overheadZoomActive && mapPlayer.overheadCalls == 1);
        // A commander below it: squadManager is null here, so only +7F8 drops.
        m.m_zoomLevel = 1;
        map->handleEvent(static_cast<EventCategory>(2), 77, nullptr, nullptr);
        CHECK(!m.m_overheadZoomActive && mapPlayer.overheadCalls == 1);
        m.m_commanderMode = savedCommander; m.m_zoomLevel = savedZoom;

        // ECHud 83 (783212): the supply-drop marker at the payload's position, for 25 seconds.
        float supplies[3] = { 4.0f, 5.0f, 6.0f };
        m.m_suppliesActive = false; m.m_fieldb10 = 9.0f; m.m_fieldb28 = 0.0f;
        map->handleEvent(static_cast<EventCategory>(2), 83, nullptr, supplies);
        CHECK(m.m_suppliesActive && m.m_fieldb10 == 0.0f && m.m_fieldb28 == 25.0f
              && m.m_suppliesPosition.x == 4.0f && m.m_suppliesPosition.y == 5.0f
              && m.m_suppliesPosition.z == 6.0f);
        map->handleEvent(static_cast<EventCategory>(2), 83, nullptr, nullptr);   // null: nothing
        CHECK(m.m_suppliesActive);
    }
    players.local = nullptr; players.byId = nullptr;

    hudManager = nullptr;
    destroyHudStorage(singleton);


    // Family P: `createMiniMapNode` (row 32), the only row in its family. The
    // node is a Bf2MiniMenuMapNode - the map with twenty overridden slots, eight
    // added ones and a 1,458-byte constructor that rewrites a slice of what the
    // base just set up.
    {
        World p;
        // The base constructor runs first, so the node's own loads follow the
        // map's 64. The World has loaded nothing yet.
        CHECK(p.system.textures.empty());
        const std::size_t baseTextures = 64;
        CHECK(p.builder.createMiniMapNode("MapSplit", "CommanderMiniMap", 0, 20, 137, 132));
        Object* found = p.system.findNamedObject("CommanderMiniMap");
        CHECK(found && found->getType() == Bf2MiniMenuMapNode::getClassType());
        Bf2MiniMenuMapNode* mini = static_cast<Bf2MiniMenuMapNode*>(found);
        CHECK(mini != nullptr);
        CHECK(sizeof(Bf2MiniMenuMapNode) == 3008);
        CHECK(mini && mini->getObjectSize() == 3008);    // native; retail is 0xC88 = 3208
        CHECK(std::strcmp(Bf2MiniMenuMapNode::getClassType()->getName(),
                          "dice::meme::Bf2MiniMenuMapNode") == 0);
        CHECK(Bf2MiniMenuMapNode::getClassType()->getBaseType()
              == Bf2MenuMapNode::getClassType());
        CHECK(mini && mini->getType() == Bf2MiniMenuMapNode::getClassType());

        // The transform is named after the parent, like createMapNode's and
        // unlike every other create row; the node keeps the raw second argument.
        Bf2SplitNode* split = static_cast<Bf2SplitNode*>(p.system.findNamedObject("MapSplit"));
        CHECK(split != nullptr);
        CHECK(!std::strcmp(mini->getName(), "CommanderMiniMap"));

        Bf2MenuMapNode& f = Bf2MenuMapNodePublicFixture::m(mini);
        // The geometry: position forced to (0,0), size from the row's w/h.
        CHECK(mini->getPosition().x == 0.0f && mini->getPosition().y == 0.0f);
        CHECK(mini->getSize().x == 137.0f && mini->getSize().y == 132.0f);
        // The values this class overwrites, against the base's own defaults.
        CHECK(!f.m_showKitsOnMap);
        CHECK(f.m_field3c0.to_ullong() == 0x80ULL);            // base: 0x7F000000FF
        CHECK(f.m_field678 == 2);                              // base: 8
        CHECK(f.m_field694 == 1.0f);                           // base: 0.5
        CHECK(f.m_currentZoomExponent == 0.0f);
        CHECK(f.m_field68c && f.m_expandedMap && !f.m_field68d);
        CHECK(!f.m_fielda7b);                                  // base: true
        CHECK(!f.m_field7f9);                                  // base: zoom icons ON
        CHECK(f.m_field708[2] == 0.5f);                        // base: 1.0
        // The +158 self-call the constructor makes near the end.
        CHECK(f.m_field7e8.x == 0.5f && f.m_field7e8.y == -0.5f);
        mini->resetViewBiasForHud();
        CHECK(f.m_field7e8.x == 0.5f && f.m_field7e8.y == -0.5f);

        // The filter sets: set 5 masked, set 6 cleared.
        CHECK(f.m_filterSets[6][0].none() && f.m_filterSets[6][5].none());
        bool sameRow = true;
        for (int i = 1; i != 6; ++i)
            sameRow = sameRow && f.m_filterSets[5][i] == f.m_filterSets[5][0];
        CHECK(sameRow);
        // The mask only ever clears bits, so the result is a subset of the base's.
        CHECK((f.m_filterSets[5][0].to_ullong() & ~0x000001DE7F787FF7ULL) == 0ULL);

        // The textures. Twenty-two of the twenty-five loads use the empty
        // name: Ref2System::loadTexture("") hands back one shared blank.
        CHECK(p.system.textures.size() == baseTextures + 26);
        const std::size_t loads = p.system.textures.size() - baseTextures;
        CHECK(loads == 26);   // 3 named, 22 blank, 1 zoomSquare
        int blanks = 0;
        for (std::size_t i = baseTextures; i < p.system.textures.size(); ++i)
            if (p.system.textures[i].empty()) ++blanks;
        CHECK(blanks == 22);
        CHECK(p.system.textures[baseTextures] == "Ingame/Minimap/map_Line.tga");
        CHECK(p.system.textures[baseTextures + 1]
              == "Ingame/Minimap/Icons/placedAction_micromap.tga");
        CHECK(p.system.textures[baseTextures + 2]
              == "Ingame/Minimap/Icons/placedAction_micromap.tga");
        // The obfuscation pair is an identity: the four members that travel
        // through 779E70/779ED0 hold the handle itself.
        CHECK(f.m_field8a8[0] == int(baseTextures) + 1);
        CHECK(f.m_field8a8[3] == int(baseTextures) + 2);
        CHECK(f.m_field8a8[4] == f.m_field8a8[3]);
        CHECK(f.m_field8a8[5] == f.m_field8a8[3] && f.m_field8a8[6] == f.m_field8a8[3]);
        CHECK(f.m_field8a8[13] == int(baseTextures) + 3);
        CHECK(f.m_field8a8[14] == f.m_field8a8[13]);
        CHECK(f.m_field8a8[15] == f.m_field8a8[13] && f.m_field8a8[16] == f.m_field8a8[13]);
        // +8DC is written twice: the default first, then the second handle.
        CHECK(f.m_field8a8[7] == f.m_defaultMinimapTexture);
        CHECK(f.m_field8a8[11] == f.m_defaultMinimapTexture);
        CHECK(f.m_field8a8[21] == f.m_defaultMinimapTexture);
        // The one real name among the blanks.
        CHECK(p.system.textures[baseTextures + 17]
              == "Ingame/Commander/micromap_zoomSquare.tga");
        CHECK(f.m_field93c[18] == int(baseTextures) + 18);
        // Every blank slot shares the recorder's ordinals, one per call.
        CHECK(f.m_controlPointTextures[0] == int(baseTextures) + 4);
        CHECK(f.m_field93c[22] == int(p.system.textures.size()));

        // 7CF5A0, the mini-map override of +13C. It ignores the base texture
        // selectors, fixes the record at 5x5 and chooses its colour palette from
        // the control point's team relative to the local player's team. Neutral
        // points use miniControlPointColors; owned/enemy points use mapObjectColors.
        {
            const int queriesBefore = players.localQueries;
            players.local = reinterpret_cast<void*>(&mapPlayer);
            mapPlayer.team = 2;

            Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
                mini, nullptr, 2, true, "ignored", false, false, false);
            CHECK(players.localQueries == queriesBefore); // null icon returns before player lookup

            MapIconRecord neutral;
            neutral.color = Color{-1.0f, -2.0f, -3.0f, -4.0f};
            Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
                mini, &neutral, 0, true, "ignored", true, true, true);
            CHECK(neutral.texture == f.m_defaultMinimapTexture);
            CHECK(neutral.size.x == 5.0f && neutral.size.y == 5.0f);
            CHECK(neutral.color.r == hud::miniControlPointColors[1].r
                  && neutral.color.g == hud::miniControlPointColors[1].g
                  && neutral.color.b == hud::miniControlPointColors[1].b
                  && neutral.color.a == hud::miniControlPointColors[1].a);

            MapIconRecord friendly;
            Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
                mini, &friendly, 2, false, "ignored", true, false, true);
            CHECK(friendly.texture == f.m_defaultMinimapTexture);
            CHECK(friendly.size.x == 5.0f && friendly.size.y == 5.0f);
            CHECK(friendly.color.r == hud::mapObjectColors[2].r
                  && friendly.color.g == hud::mapObjectColors[2].g
                  && friendly.color.b == hud::mapObjectColors[2].b
                  && friendly.color.a == hud::mapObjectColors[2].a);

            MapIconRecord enemy;
            Bf2MenuMapNodePublicFixture::populateControlPointMapIcon(
                mini, &enemy, 1, true, "ignored", false, true, false);
            CHECK(enemy.texture == f.m_defaultMinimapTexture);
            CHECK(enemy.size.x == 5.0f && enemy.size.y == 5.0f);
            CHECK(enemy.color.r == hud::mapObjectColors[1].r
                  && enemy.color.g == hud::mapObjectColors[1].g
                  && enemy.color.b == hud::mapObjectColors[1].b
                  && enemy.color.a == hud::mapObjectColors[1].a);
            CHECK(players.localQueries == queriesBefore + 3);
            players.local = nullptr;
        }

        // The mini-map event override replaces the base handler; only IDs 2, 8, 25, and 60 act.
        players.localQueries = 0;
        const std::size_t texturesBefore = p.system.textures.size();
        const int menuLoadsBefore = p.system.menuFlags;
        // The discarded local-player fetch at 7CF799 happens for every id,
        // including the ones the switch ignores - and 42, 75 and 82 are three of
        // the MAP's own arms that this class simply does not have.
        static const unsigned int quietIds[] = { 0u, 1u, 3u, 7u, 26u, 42u, 59u, 61u, 75u, 82u, 4000u };
        for (unsigned int quiet : quietIds)
            mini->handleEvent(static_cast<EventCategory>(2), quiet, nullptr, nullptr);
        CHECK(players.localQueries == 11);
        CHECK(p.system.textures.size() == texturesBefore);       // no arm ran
        // The ECCore arm (+148) is inherited unchanged and makes no such fetch.
        mini->handleEvent(static_cast<EventCategory>(5), 13, nullptr, nullptr);
        CHECK(players.localQueries == 11);

        // id 25 ignores a null payload outright; id 60 does NOT test its
        // payload pointer at all - it dereferences it and tests the first word,
        // the marker id. Both return early.
        mini->handleEvent(static_cast<EventCategory>(2), 25, nullptr, nullptr);
        int zeroMarker = 0;
        mini->handleEvent(static_cast<EventCategory>(2), 60, nullptr, &zeroMarker);
        CHECK(players.localQueries == 13);
        CHECK(p.system.textures.size() == texturesBefore);

        // id 60's nonzero arm marshals a kind-1 timed marker through 781FF0.
        // A duplicate id is ignored before allocation, exactly as retail.
        int voiceMarker = 11;
        mini->handleEvent(static_cast<EventCategory>(2), 60, nullptr, &voiceMarker);
        CHECK(f.m_timedMarkers.size() == 1);
        if (!f.m_timedMarkers.empty()) {
            const MapTimedMarker& marker = f.m_timedMarkers[0];
            CHECK(marker.networkId == voiceMarker && marker.texture == 0);
            CHECK(marker.size.x == 4.0f && marker.size.y == 4.0f);
            CHECK(marker.kind == 1 && marker.remaining == f.m_fielda2c);
            CHECK(marker.initialDuration == f.m_fielda2c);
            CHECK(marker.field20 == f.m_fielda24 && marker.field24 == f.m_fielda28);
            CHECK(marker.field18 == 0.0f && !marker.negativeDuration);
            CHECK(marker.position.x == 0.0f && marker.position.y == 0.0f);
        }
        mini->handleEvent(static_cast<EventCategory>(2), 60, nullptr, &voiceMarker);
        CHECK(f.m_timedMarkers.size() == 1);

        // ID 25 uses the inherited setSquadRequestType: payload word [2] is stored
        // at index [0]. Restore the query count for the assertions below.
        {
            const int queriesBefore25 = players.localQueries;
            int words[3] = { 7, 0xDEAD, 0xABCD };                          // [1] is never read
            f.m_squadRequestTypes[7] = 0;
            mini->handleEvent(static_cast<EventCategory>(2), 25, nullptr, words);
            CHECK(f.m_squadRequestTypes[7] == 0xABCD);
            CHECK(players.localQueries == queriesBefore25 + 1);           // the prologue fetch
            players.localQueries = queriesBefore25;
        }

        // id 8: the level's own art. The map's id 8 resets the satellite and UAV
        // reloads; this one formats game->getCurrentMapName() into the minimap
        // path and hands it to the base's setMinimapTexture (772900).
        fixtureGame.nameQueries = 0;
        mini->handleEvent(static_cast<EventCategory>(2), 8, nullptr, nullptr);
        CHECK(fixtureGame.nameQueries == 1 && players.localQueries == 16);
        CHECK(p.system.textures.size() == texturesBefore + 1);
        CHECK(p.system.textures.back() == "Levels/Strike_At_Karkand/Hud/Minimap/ingameMap.tga");
        // menuPath is false here - setMinimapTexture is the only loader in the
        // whole class that passes false, and the satellite/UAV state is untouched.
        CHECK(p.system.menuFlags == menuLoadsBefore);
        CHECK(f.m_minimapTexture == int(p.system.textures.size()));
        CHECK(!f.m_satelliteReloading && !f.m_uavReloading);
        // Missing minimap files use the default texture without a diagnostic.
        // Mode changes dispatch the mini-map override. Publish the HUD singleton after node construction.
        FixtureHud* const miniHud = new FixtureHud;
        hudManager = miniHud;
        miniHud->m_field250 = 0;                          // any non-commander mode
        f.m_field06c = true; f.m_field674 = 0; f.m_commanderMode = false;
        mini->handleEvent(static_cast<EventCategory>(2), 2, nullptr, nullptr);
        CHECK(!f.m_field06c);                             // the whole body for a stray mode
        CHECK(f.m_field674 == 0 && !f.m_commanderMode);   // nothing else is touched
        CHECK(players.localQueries == 17);                // the arm still makes the fetch
        // 15 and 21 only - the first TWO entries of isCommanderMode's list
        // (74FE40), not the whole commander set.
        static const int commanderModes[] = { 15, 21 };
        for (int commanderMode : commanderModes) {
            miniHud->m_field250 = commanderMode;
            f.m_field06c = false; f.m_field690 = true;
            f.m_commanderMode = false; f.m_squadMode = true; f.m_field674 = 0;
            f.m_field708[1] = 9.0f; f.m_field718 = 9.0f;
            f.m_field700 = 9.0f; f.m_field704 = 9.0f;
            f.m_filterStates[0].reset(); f.m_filterStates[5].reset();
            mini->handleEvent(static_cast<EventCategory>(2), 2, nullptr, nullptr);
            CHECK(f.m_field06c && !f.m_field690 && f.m_commanderMode && !f.m_squadMode);
            CHECK(f.m_field674 == 2);                     // the constructor left 1
            // The three one-line setters: 7728B0 clears +70C, 772880 writes +718
            // AND +700 because its flag is 1, 7728A0 writes +704.
            CHECK(f.m_field708[1] == 0.0f && f.m_field718 == 1.0f
                  && f.m_field700 == 1.0f && f.m_field704 == 1.0f);
            // applyFilterSet (772060) copied the +4B8 row into the live states.
            CHECK(f.m_filterStates[0] == f.m_filterSets[5][0]);
            CHECK(f.m_filterStates[5] == f.m_filterSets[5][5]);
        }
        CHECK(players.localQueries == 19);

        // +118 and +11C are two more of the twenty overrides, and both collapse
        // to this class's own added slot +158. setViewCenter throws the position
        // away - a mini-map keeps no view centre - so the base's member stays put.
        f.m_field7e8.x = 0.0f; f.m_field7e8.y = 0.0f;
        f.m_viewCenter.x = 0.0f; f.m_viewCenter.y = 0.0f;
        mini->setViewCenter(Vec2{123.0f, 456.0f});
        CHECK(f.m_field7e8.x == 0.5f && f.m_field7e8.y == -0.5f);
        CHECK(f.m_viewCenter.x == 0.0f && f.m_viewCenter.y == 0.0f);
        f.m_field7e8.x = 0.0f; f.m_field7e8.y = 0.0f;
        mini->recenterOnPlayer();
        CHECK(f.m_field7e8.x == 0.5f && f.m_field7e8.y == -0.5f);

        // Micro-map update, drawing, local-player icon and input dispatch.
        // With null gameLogic and exponent zero, getZoomScale(false) is 1.
        {
            IGameLogic* const savedMiniLogic = gameLogic;
            SettingsRepostitory* const savedMiniSettings = settings;
            gameLogic = nullptr;
            MiniClickHud* const clickHud = new MiniClickHud;
            hudManager = clickHud;
            clickHud->m_field250 = 0;                                  // getMode(): no squad screen
            void* miniSettingsTable[0x40 / 4];
            for (auto& e : miniSettingsTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            miniSettingsTable[0x10 / 4] = reinterpret_cast<void*>(&SettingsInput::intGet);
            SettingsInput miniSettings = { miniSettingsTable, 1024, std::vector<std::string>() };
            settings = reinterpret_cast<SettingsRepostitory*>(&miniSettings);
            const auto asRaw = [](MapPlayerInput& player) {
                return reinterpret_cast<world::IPlayer*>(&player);
            };
            const auto sameColor = [](const Color& a, const Color& b) {
                return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
            };

            // update: the blink clock accumulates, the 0.5 / 1.0 gate flips +A18, and the world
            // time is stamped last.
            f.m_fielda24 = 0.5f; f.m_fielda28 = 1.0f; f.m_fielda30 = 0.0f;
            WorldPref::resetWorldTime(10.0);
            Bf2MiniMenuMapNodeFixture::update(mini, 0.25f);
            CHECK(f.m_fielda30 == 0.25f && f.m_fielda18);
            CHECK(Bf2MiniMenuMapNodeFixture::lastUpdateTime(mini) == 10.0);
            Bf2MiniMenuMapNodeFixture::update(mini, 0.5f);
            CHECK(f.m_fielda30 == 0.75f && !f.m_fielda18);
            Bf2MiniMenuMapNodeFixture::update(mini, 0.5f);
            CHECK(f.m_fielda30 == 0.0f && !f.m_fielda18);              // past +A28: restarted

            // paintSelf, filter 0 alone: +E0 runs with (world time - last stamp), the extents are
            // read, the local player is asked (null here - no early return, and +12C asks again),
            // the map texture is drawn once with `bounds` as both rectangles, +7C..7E cleared.
            for (auto& row : f.m_filterStates) { row.reset(); row.set(0); }
            WorldPref::resetWorldTime(20.0);
            Bf2MiniMenuMapNodeFixture::lastUpdateTime(mini) = 19.75;
            f.m_fielda30 = 0.0f;
            f.m_field07c = f.m_field07d = f.m_field07e = true;
            f.m_field088 = f.m_worldToMapScale = 0.0f;
            p.system.drawn.clear();
            players.local = nullptr;
            const int miniLocalFetches = players.localQueries;
            RenderContext miniContext = { nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            const Rect miniBounds = { 0.0f, 20.0f, 137.0f, 132.0f, 0.0f };
            mini->paintSelf(&p.system, nullptr, &miniBounds, nullptr, &miniContext);
            CHECK(f.m_fielda30 == 0.25f);
            CHECK(Bf2MiniMenuMapNodeFixture::lastUpdateTime(mini) == 20.0);
            CHECK(miniSettings.keys.size() == 2 && miniSettings.keys[0] == "GLSWorldSizeZ"
                  && miniSettings.keys[1] == "GLSWorldSizeX");
            CHECK(f.m_field088 == 1.0f / 1024.0f && f.m_worldToMapScale == 1.0f / 1024.0f);
            CHECK(players.localQueries == miniLocalFetches + 2);
            CHECK(p.system.drawn.size() == 1);
            if (p.system.drawn.size() == 1)
                CHECK(p.system.drawn[0].texture == f.m_minimapTexture
                      && p.system.drawn[0].color.a == f.m_field708[2]);
            CHECK(!f.m_field07c && !f.m_field07d && !f.m_field07e);
            p.system.drawn.clear();

            // collectLocalPlayerIcon (+12C): the commander view square at the camera - the
            // soldier's own, or for a commander the HUD's team camera. The BIG map's zoom (2.3^0,
            // halved, then clamped UP to 1) leaves 120x120 alone, and its centre for ITS zoom
            // level, times this node's +84, is added after the projection. getMap is asked three
            // times; +14C's colour is overwritten by (1, 1, 1, +6FC).
            void* cameraTable[0x20 / 4];
            for (auto& e : cameraTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            cameraTable[0x10 / 4] = reinterpret_cast<void*>(&CameraInput::getTransformation);
            CameraInput ownCamera = { cameraTable }, teamCamera = { cameraTable };
            teamCamera.transform[12] = 256.0f; teamCamera.transform[14] = -256.0f;
            for (auto& row : f.m_filterStates) row.set(6);
            f.m_iconScales[6] = Vec2{-1.0f, -1.0f};                   // addMapIcon: no rescale
            f.m_field72c = 0.02f; f.m_currentZoomExponent = 0.0f; f.m_overheadZoomActive = false;
            f.m_field6fc = 0.75f;
            f.m_worldToMapScale = f.m_field088 = 1.0f / 1024.0f;
            const float savedBigExponent = m.m_currentZoomExponent;
            const int savedBigLevel = m.m_zoomLevel;
            const Vec2 savedBigCenter = m.m_centers[1];
            m.m_currentZoomExponent = 0.0f;
            m.m_zoomLevel = 1;
            m.m_centers[1] = Vec2{512.0f, -256.0f};                    // + (0.5, -0.25)
            clickHud->mapForMini = map;
            f.m_icons[6].clear();
            Bf2MiniMenuMapNodeFixture::collectLocalPlayerIcon(mini);   // no local player
            CHECK(f.m_icons[6].empty() && clickHud->mapQueries == 0);
            players.local = &mapPlayer;
            mapPlayer.commander = false;
            mapPlayer.camera = nullptr;
            Bf2MiniMenuMapNodeFixture::collectLocalPlayerIcon(mini);   // no camera
            CHECK(f.m_icons[6].empty() && clickHud->mapQueries == 0);
            mapPlayer.camera = &ownCamera;
            mapPlayer.cameraQueries = 0;
            Bf2MiniMenuMapNodeFixture::collectLocalPlayerIcon(mini);
            CHECK(ownCamera.queries == 1 && clickHud->mapQueries == 3);
            CHECK(f.m_icons[6].size() == 1);
            if (f.m_icons[6].size() == 1) {
                const MapIconRecord& square = f.m_icons[6][0];
                CHECK(square.position.x == 1.5f && square.position.y == -1.25f);   // (1, -1) + centre
                CHECK(square.size.x == 120.0f && square.size.y == 120.0f);
                CHECK(square.texture == f.m_field93c[18] && square.rotation == 0.0f);
                CHECK(sameColor(square.color, Color{1.0f, 1.0f, 1.0f, 0.75f}));
                CHECK(square.clampToVisibleMap);
            }
            f.m_icons[6].clear();
            mapPlayer.commander = true;                                // the team camera instead
            clickHud->cameraObject = reinterpret_cast<world::ICameraObject*>(&teamCamera);
            Bf2MiniMenuMapNodeFixture::collectLocalPlayerIcon(mini);
            CHECK(mapPlayer.cameraQueries == 2 && ownCamera.queries == 1 && teamCamera.queries == 1);
            CHECK(clickHud->cameraObjectQueries == 1 && clickHud->lastCameraTeam == mapPlayer.team);
            CHECK(f.m_icons[6].size() == 1);
            if (f.m_icons[6].size() == 1)
                CHECK(f.m_icons[6][0].position.x == 1.75f && f.m_icons[6][0].position.y == -1.0f);
            f.m_icons[6].clear();
            mapPlayer.commander = false;
            mapPlayer.camera = nullptr;
            clickHud->mapForMini = nullptr;
            m.m_currentZoomExponent = savedBigExponent;
            m.m_zoomLevel = savedBigLevel;
            m.m_centers[1] = savedBigCenter;

            // +160: the local player's square; a squad leader's 4x4 dot only with the special-role
            // flag, everyone else 2x2 with no texture. Both flags come back false, but a null icon
            // leaves rotateOut alone.
            MapPlayerInput leader = { mapPlayerTable, 80, 4, false, true, 2 };
            MapPlayerInput mate = { mapPlayerTable, 78, 3, false, false, 2 };
            MapIconRecord shape;
            bool heading = true, rotate = true;
            Bf2MiniMenuMapNodeFixture::populateMiniPlayerIcon(mini, asRaw(mapPlayer), &shape,
                                                              &heading, &rotate, true, true);
            CHECK(shape.size.x == 120.0f && shape.size.y == 120.0f
                  && shape.texture == f.m_field93c[18] && !heading && !rotate);
            Bf2MiniMenuMapNodeFixture::populateMiniPlayerIcon(mini, asRaw(leader), &shape,
                                                              nullptr, nullptr, false, true);
            CHECK(shape.size.x == 4.0f && shape.size.y == 4.0f && shape.texture == 0);
            Bf2MiniMenuMapNodeFixture::populateMiniPlayerIcon(mini, asRaw(leader), &shape,
                                                              nullptr, nullptr, true, false);
            CHECK(shape.size.x == 2.0f && shape.size.y == 2.0f);        // showKit is never read
            Bf2MiniMenuMapNodeFixture::populateMiniPlayerIcon(mini, asRaw(mate), &shape,
                                                              nullptr, nullptr, false, true);
            CHECK(shape.size.x == 2.0f && shape.size.y == 2.0f);
            heading = rotate = true;
            Bf2MiniMenuMapNodeFixture::populateMiniPlayerIcon(mini, asRaw(leader), nullptr,
                                                              &heading, &rotate, false, true);
            CHECK(!heading && rotate);

            // +14C: the mini-map's player colour, side = 2 - (teams differ). The local player is
            // white; the selection list (+A34) wins over the squad; a squad mate on another icon
            // than +A94 takes the squad colour; a commander on foot is white; the rest the object
            // colour. Outside the squad and commander screens (getMode 0 here, commander hidden).
            mapPlayer.squad = 3;
            MapPlayerInput enemy = { mapPlayerTable, 79, 0, false, false, 1 };
            MapPlayerInput officer = { mapPlayerTable, 81, 0, true, false, 2 };
            MapIconRecord colored;
            colored.texture = f.m_fielda94 + 1;
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(mate), nullptr, true);  // no-op
            colored.color = Color{0.0f, 0.0f, 0.0f, 0.0f};
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, nullptr, &colored, true);
            CHECK(sameColor(colored.color, hud::mapNeutralColors[2]));
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(mapPlayer), &colored, true);
            CHECK(sameColor(colored.color, Color{1.0f, 1.0f, 1.0f, 1.0f}));
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(mate), &colored, true);
            CHECK(sameColor(colored.color, hud::squadChatColors[2]));
            f.m_fielda34.push_back(78);
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(mate), &colored, true);
            CHECK(sameColor(colored.color, hud::mapSelectedColors[2]));
            f.m_fielda34.clear();
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(enemy), &colored, true);
            CHECK(sameColor(colored.color, hud::mapObjectColors[1]));
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(officer), &colored, true);
            CHECK(sameColor(colored.color, Color{1.0f, 1.0f, 1.0f, 1.0f}));
            colored.texture = f.m_fielda94;                            // the satellite icon
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, asRaw(mate), &colored, true);
            CHECK(sameColor(colored.color, hud::mapObjectColors[2]));
            players.local = nullptr;
            colored.color = Color{0.0f, 0.0f, 0.0f, 0.0f};
            Bf2MiniMenuMapNodeFixture::colorMiniMapIcon(mini, nullptr, &colored, true);
            CHECK(sameColor(colored.color, Color{0.0f, 0.0f, 0.0f, 0.0f}));  // no local: untouched
            mapPlayer.squad = 0;

            // onEvent1: a press inside the rect moves the commander camera there; a release only
            // sets the mode; every answer is false. The rect's centre on the 800x600 System is
            // (200, 100); at exponent 0 the scale is 1 / rect size per axis.
            const Rect clickRect = { 100.0f, 50.0f, 200.0f, 100.0f, 0.0f };
            f.m_field06c = false;
            p.system.setFixturePointer(250.0f, 125.0f);                // (+50, +25) from the centre
            CHECK(!mini->onEvent1(&p.system, &clickRect, nullptr, 1, 0, 0));
            CHECK(clickHud->modeCalls == 0 && clickHud->cameraCalls == 0);
            f.m_field06c = true;
            f.m_field7e8 = Vec2{0.5f, -0.5f};
            f.m_commanderPosition = Vec2{-4000.0f, -4000.0f};
            CHECK(!mini->onEvent1(&p.system, &clickRect, nullptr, 1, 0, 0));
            CHECK(f.m_commanderPosition.x == 0.25f && f.m_commanderPosition.y == 0.25f);
            CHECK(clickHud->modeCalls == 1 && clickHud->lastMode == 15 && clickHud->lastSavePrevious);
            CHECK(clickHud->cameraCalls == 1 && clickHud->lastCamera.x == 256.0f
                  && clickHud->lastCamera.y == -256.0f);               // (1/+84)x, -(1/+88)y
            CHECK(clickHud->voHelpCalls == 1 && clickHud->lastVoHelp == 26);
            CHECK(!mini->onEvent1(&p.system, &clickRect, nullptr, 21, 0, 0));   // release
            CHECK(clickHud->modeCalls == 2 && clickHud->cameraCalls == 1
                  && clickHud->voHelpCalls == 1);
            CHECK(!mini->onEvent1(&p.system, &clickRect, nullptr, 2, 0, 0));    // anything else
            CHECK(clickHud->modeCalls == 2);
            p.system.setFixturePointer(350.0f, 125.0f);                // 150 from the centre
            CHECK(!mini->onEvent1(&p.system, &clickRect, nullptr, 1, 0, 0));
            CHECK(clickHud->modeCalls == 2 && f.m_commanderPosition.x == 0.25f);
            f.m_field06c = false;

            hudManager = miniHud;
            destroyHudStorage(clickHud);
            settings = savedMiniSettings;
            gameLogic = savedMiniLogic;
        }

        hudManager = nullptr;
        destroyHudStorage(miniHud);

        // The row has NO duplicate test and no diagnostic at all - a repeat
        // silently builds a second node - and a missing System is a silent false.
        CHECK(p.builder.createMiniMapNode("MapSplit", "CommanderMiniMap", 0, 0, 1, 1));
        p.builder.setSystem(nullptr);
        CHECK(!p.builder.createMiniMapNode("MapSplit", "Other", 0, 0, 1, 1));
    }
    // World's constructor repoints both globals; the checks below belong to
    // the outer world, so put them back before the destruction run.
    eventManager = &w.bus;
    g_hudInformationLayer = w.info;

    // Tests UAV sentinel paths without ObjectManager: inactive entries disappear;
    // active entries draw the radius before the centre icon and remain registered.
    {
        const unsigned int sentinel = (std::numeric_limits<unsigned int>::max)();
        world::objectManager = nullptr;
        m.m_zoomLevel = 1;
        m.m_expandedMap = false;
        m.m_filterStates[1].set(32);
        m.m_field7f9 = false;                    // keep addMapIcon off the zoom callback
        m.m_overheadZoomActive = false;
        m.m_worldToMapScale = 1.0f;
        m.m_field088 = 1.0f;
        m.m_currentZoomExponent = 0.0f;
        map->getSize() = Vec2{10.0f, 20.0f};
        m.m_icons[32].clear();
        m.m_uavObjects.clear();

        collectUavIconsForFixture(map);
        CHECK(m.m_uavObjects.empty() && m.m_icons[32].empty());

        m.m_uavObjects.insert(std::make_pair(sentinel,
                                              static_cast<world::IObject*>(nullptr)));
        m.m_uavActive = false;
        collectUavIconsForFixture(map);
        CHECK(m.m_uavObjects.empty());
        CHECK(m.m_icons[32].empty());

        m.m_uavObjects.insert(std::make_pair(sentinel,
                                              static_cast<world::IObject*>(nullptr)));
        m.m_uavActive = true;
        m.m_uavPosition = Vec3{0.25f, 7.0f, -0.25f};
        m.m_uavRadius = 2;
        m.m_uavTexture = 707;
        m.m_uavIcon = 708;
        m.m_fieldabc = 0.375f;
        m.m_fieldaac = 5.0f;
        m.m_uavActiveTime = 10.0f;
        m.m_field6fc = 0.625f;
        collectUavIconsForFixture(map);
        CHECK(m.m_uavObjects.size() == 1
              && m.m_uavObjects.begin()->first == sentinel
              && m.m_uavObjects.begin()->second == nullptr);
        CHECK(m.m_icons[32].size() == 2);
        if (m.m_icons[32].size() == 2) {
            const MapIconRecord& radius = m.m_icons[32][0];
            const MapIconRecord& center = m.m_icons[32][1];
            CHECK(radius.texture == 707);
            CHECK(radius.size.x == 40.0f && radius.size.y == 40.0f);
            CHECK(radius.position.x == 1.25f && radius.position.y == -0.75f);
            CHECK(radius.rotation == 0.375f && !radius.clampToVisibleMap);
            CHECK(radius.color.r == 1.0f && radius.color.g == 1.0f
                  && radius.color.b == 1.0f && radius.color.a == 0.625f);
            CHECK(center.texture == 708);
            CHECK(center.size.x == 15.0f && center.size.y == 15.0f);
            CHECK(center.position.x == radius.position.x
                  && center.position.y == radius.position.y);
            CHECK(center.rotation == 0.0f && !center.clampToVisibleMap);
            CHECK(center.color.r == radius.color.r
                  && center.color.g == radius.color.g
                  && center.color.b == radius.color.b
                  && center.color.a == radius.color.a);
        }
    }

    // Timed markers always query the local player, but enabled/kind gates precede network-icon lookup.
    // Fallback uses the marker texture/size, or the default texture with 16x16 size when absent.
    {
        FixtureSystem timedSystem;
        Bf2MenuMapNode* const timed =
            createTimedIconFallbackNodeForFixture(&timedSystem, "TimedIconFixture");
        Bf2MenuMapNode& state = Bf2MenuMapNodePublicFixture::m(timed);
        state.m_zoomLevel = 1;
        state.m_expandedMap = false;
        state.m_filterStates[1].set(30);
        state.m_field7f9 = false;                    // keep addMapIcon off the zoom callback
        state.m_overheadZoomActive = false;
        state.m_worldToMapScale = 1.0f;
        state.m_field088 = 1.0f;
        state.m_field6fc = 0.625f;
        state.m_icons[30].clear();

        const auto marker = [](int kind, int id, int texture,
                               Vec2 size, Vec2 position) {
            MapTimedMarker value;
            value.texture = texture;
            value.networkId = id;
            value.size = size;
            value.kind = kind;
            value.remaining = 0.0f;
            value.field18 = 0.0f;
            value.initialDuration = 0.0f;
            value.field20 = 0.0f;
            value.field24 = 0.0f;
            value.negativeDuration = false;
            value.padding[0] = value.padding[1] = value.padding[2] = 0;
            value.position = position;
            return value;
        };

        players.local = nullptr;
        int localQueries = players.localQueries;
        state.m_timedMarkers.push_back(
            marker(3, -11, 701, Vec2{13.0f, 17.0f}, Vec2{0.25f, -0.25f}));
        state.m_fielda18 = false;
        collectTimedPositionIconsForFixture(timed);
        CHECK(players.localQueries == localQueries + 1); // query is outside the loop
        CHECK(timedIconFallbackPopulateCallsForFixture(timed) == 0
              && state.m_icons[30].empty());

        state.m_timedMarkers.clear();
        state.m_timedMarkers.push_back(
            marker(2, -12, 702, Vec2{14.0f, 18.0f}, Vec2{0.25f, -0.25f}));
        state.m_fielda18 = true;
        localQueries = players.localQueries;
        collectTimedPositionIconsForFixture(timed);
        CHECK(players.localQueries == localQueries + 1);
        CHECK(timedIconFallbackPopulateCallsForFixture(timed) == 0
              && state.m_icons[30].empty());

        state.m_timedMarkers.clear();
        state.m_timedMarkers.push_back(
            marker(3, -17, 701, Vec2{13.0f, 17.0f}, Vec2{0.25f, -0.25f}));
        localQueries = players.localQueries;
        collectTimedPositionIconsForFixture(timed);
        CHECK(players.localQueries == localQueries + 1);
        CHECK(timedIconFallbackPopulateCallsForFixture(timed) == 1
              && timedIconFallbackRequestedCountForFixture(timed) == 1
              && timedIconFallbackRequestedIdForFixture(timed, 0) == -17);
        CHECK(state.m_icons[30].size() == 1);
        if (state.m_icons[30].size() == 1) {
            const MapIconRecord& icon = state.m_icons[30][0];
            CHECK(icon.texture == 701);
            CHECK(icon.size.x == 13.0f && icon.size.y == 17.0f);
            CHECK(icon.position.x == 1.25f && icon.position.y == -0.75f);
            CHECK(icon.rotation == 0.0f && !icon.clampToVisibleMap);
            CHECK(icon.color.r == hud::mapObjectColors[0].r
                  && icon.color.g == hud::mapObjectColors[0].g
                  && icon.color.b == hud::mapObjectColors[0].b
                  && icon.color.a == 0.625f);
        }

        state.m_icons[30].clear();
        state.m_timedMarkers.clear();
        state.m_field9a8[10] = 909;
        players.local = reinterpret_cast<void*>(&mapPlayer);
        state.m_timedMarkers.push_back(
            marker(3, -19, 0, Vec2{21.0f, 22.0f}, Vec2{-0.25f, 0.25f}));
        localQueries = players.localQueries;
        collectTimedPositionIconsForFixture(timed);
        CHECK(players.localQueries == localQueries + 1);
        CHECK(timedIconFallbackPopulateCallsForFixture(timed) == 2
              && timedIconFallbackRequestedCountForFixture(timed) == 2
              && timedIconFallbackRequestedIdForFixture(timed, 1) == -19);
        CHECK(state.m_icons[30].size() == 1);
        if (state.m_icons[30].size() == 1) {
            const MapIconRecord& icon = state.m_icons[30][0];
            CHECK(icon.texture == 909);
            CHECK(icon.size.x == 16.0f && icon.size.y == 16.0f);
            CHECK(icon.position.x == 0.75f && icon.position.y == -1.25f);
            CHECK(icon.rotation == 0.0f && !icon.clampToVisibleMap);
            CHECK(icon.color.r == hud::mapObjectColors[1].r
                  && icon.color.g == hud::mapObjectColors[1].g
                  && icon.color.b == hud::mapObjectColors[1].b
                  && icon.color.a == 0.625f);
        }
        players.local = nullptr;
        destroyTimedIconFallbackNodeForFixture(timed);
    }

    // Tests order-icon lines, markers, and cached squad numbers with a settings observer.
    // No network manager is supplied; endpoints use explicit positions or the no-leader sentinel.
    {
        IGameLogic* const savedLogic = gameLogic;
        SettingsRepostitory* const savedSettings = settings;
        gameLogic = nullptr;                             // getZoomScale(false) == (1, 1) below
        void* settingsTable[0x40 / 4];
        for (auto& e : settingsTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
        settingsTable[0x10 / 4] = reinterpret_cast<void*>(&SettingsInput::intGet);
        SettingsInput levelSettings = { settingsTable, 1024, std::vector<std::string>() };
        settings = reinterpret_cast<SettingsRepostitory*>(&levelSettings);

        const auto textureName = [&](int texture) -> std::string {
            return texture > 0 && static_cast<std::size_t>(texture) <= w.system.textures.size()
                ? w.system.textures[texture - 1] : std::string("<none>");
        };
        const auto hasColor = [](const Color& c, float r, float g, float b, float a) {
            return c.r == r && c.g == g && c.b == b && c.a == a;
        };
        const auto prepare = [](Bf2MenuMapNode& node) {
            node.m_zoomLevel = 1;
            node.m_expandedMap = false;
            node.m_field7f9 = false;                     // keep addMapIcon off the zoom callback
            node.m_overheadZoomActive = false;
            node.m_currentZoomExponent = 0.0f;
            node.m_worldToMapScale = 1.0f;
            node.m_field6fc = 0.625f;
            node.m_fielda10 = 0.5f;                      // pulse base (1 - 0.25) * 58 = 43.5
            node.m_fielda14 = 0.25f;                     // pulse 0.25 * 58 = 14.5
            node.m_field678 = 3;
            node.m_field76c = 0.0f;
            node.m_filterStates[1].reset();
            node.m_filterStates[1].set(8);
            node.m_filterStates[1].set(19);
            node.m_filterStates[1].set(20);
            node.m_filterStates[1].set(21);
            for (int type : { 8, 19, 20, 21 }) node.m_icons[type].clear();
        };
        prepare(m);
        const Color& attack = hud::mapOrderColors[5];
        const Color& drop = hud::mapOrderColors[2];

        // 774240, both ends explicit (ids -1): midpoint, length times the zoom scale's y, and the
        // angle -pi/2 - atan2(dy, dx), wrapped by 2pi only below -pi.
        MapLineRecord line;
        line.kind = 5; line.networkIdA = -1; line.networkIdB = -1; line.squad = 0;
        line.field10 = true; line.valid = true;
        line.a = Vec2{0.25f, -0.5f};
        line.b = Vec2{1.25f, -0.5f};
        line.line.size = Vec2{3.0f, 9.0f};
        line.line.rotation = 9.0f;
        completeLineRecordForFixture(map, 1.0f, &line);
        CHECK(line.valid);
        CHECK(line.line.rotation == -1.5707964f);
        CHECK(line.line.size.x == 3.0f && line.line.size.y == 1.0f);
        CHECK(line.line.position.x == 0.75f && line.line.position.y == -0.5f);
        line.a = Vec2{1.25f, -0.5f};
        line.b = Vec2{0.25f, -0.5f};                     // pointing the other way: -3pi/2, wrapped
        completeLineRecordForFixture(map, 1.0f, &line);
        CHECK(line.valid && line.line.rotation > 1.5707f && line.line.rotation < 1.5709f);
        // A degenerate end invalidates and leaves the geometry alone...
        line.b = Vec2{0.0f, 0.0f};
        line.line.rotation = 7.0f;
        completeLineRecordForFixture(map, 1.0f, &line);
        CHECK(!line.valid && line.line.rotation == 7.0f);
        // ...and so does a start at the origin when no start networkable was named.
        line.valid = true;
        line.a = Vec2{0.0f, 0.0f};
        line.b = Vec2{1.0f, 0.0f};
        completeLineRecordForFixture(map, 1.0f, &line);
        CHECK(!line.valid && line.line.rotation == 7.0f);

        // +130 (77D560). It sets `valid` again for orders, whatever 774240 decided.
        MapIconRecord marker;
        marker.position = Vec2{0.0f, 0.0f};
        marker.size = Vec2{15.0f, 31.0f};
        marker.rotation = 0.0f;
        marker.clampToVisibleMap = false;
        line.kind = 3;                                   // a commander's supplies order: 15x15
        line.squad = -1;
        map->styleOrderIcons(1, &line, &marker, true);
        CHECK(line.valid);
        CHECK(marker.size.x == 15.0f && marker.size.y == 15.0f);
        CHECK(marker.texture == m.m_field8a8[16]
              && textureName(marker.texture) == "Ingame/Minimap/Icons/icon_supplies.tga");
        CHECK(hasColor(marker.color, 1.0f, 1.0f, 1.0f, 0.625f));
        CHECK(textureName(line.line.texture) == "Ingame/Minimap/map_Line.tga");
        CHECK(hasColor(line.line.color, hud::mapOrderColors[3].r, hud::mapOrderColors[3].g,
                       hud::mapOrderColors[3].b, 0.625f));
        line.kind = 5;                                   // any other commander order: 15x47
        marker.size = Vec2{15.0f, 31.0f};
        map->styleOrderIcons(1, &line, &marker, true);
        CHECK(marker.size.x == 15.0f && marker.size.y == 47.0f);
        CHECK(textureName(marker.texture) == "Ingame/Minimap/Icons/orderAttackCommander.tga");
        marker.size = Vec2{15.0f, 31.0f};                // a squad order keeps the size it came with
        map->styleOrderIcons(0, &line, &marker, false);
        CHECK(marker.size.x == 15.0f && marker.size.y == 31.0f);
        CHECK(textureName(marker.texture) == "Ingame/Minimap/Icons/orderAttack.tga");
        // Inactive: the fixed grey, its alpha scaled by 0.8 - neither the order colour nor +6FC.
        CHECK(hasColor(line.line.color, 0.7f, 0.7f, 0.7f, 0.8f));

        // A request (kind 2): 15x15, the squad-order table's texture, the order colour, never a
        // line - plus a pulse ring and, from squad 1 up, the number icon, both under type 19.
        const std::size_t texturesBefore = w.system.textures.size();
        m.m_squadNumberIcons[4] = 0;
        line.kind = 2;
        line.squad = 4;
        line.valid = true;
        MapIconRecord request;
        request.position = Vec2{0.5f, 0.25f};
        request.size = Vec2{15.0f, 31.0f};
        request.rotation = 0.0f;
        request.clampToVisibleMap = false;
        map->styleOrderIcons(2, &line, &request, true);
        CHECK(!line.valid);
        CHECK(request.size.x == 15.0f && request.size.y == 15.0f);
        CHECK(textureName(request.texture) == "Ingame/Minimap/Icons/icon_vehicledrop.tga");
        CHECK(hasColor(request.color, drop.r, drop.g, drop.b, 0.625f));
        CHECK(m.m_icons[19].size() == 2);
        if (m.m_icons[19].size() == 2) {
            const MapIconRecord& ring = m.m_icons[19][0];
            const MapIconRecord& number = m.m_icons[19][1];
            CHECK(ring.position.x == 0.5f && ring.position.y == 0.25f);
            CHECK(hasColor(ring.color, drop.r, drop.g, drop.b, 0.625f));
            CHECK(ring.texture == m.m_field928[0]
                  && textureName(ring.texture) == "Ingame/Minimap/Icons/request_circle.tga");
            CHECK(ring.size.x == 58.0f && ring.size.y == 58.0f && !ring.clampToVisibleMap);
            CHECK(number.position.x == 0.5f && number.position.y == 0.25f);
            CHECK(number.size.x == 36.0f && number.size.y == 36.0f);
            CHECK(number.rotation == 0.0f && !number.clampToVisibleMap);
            CHECK(textureName(number.texture)
                  == "Ingame/Player/Icons/Minimap/mini_squadNumber_4.tga");
        }
        // +100 loaded it once and caches it.
        CHECK(w.system.textures.size() == texturesBefore + 1
              && m.m_squadNumberIcons[4] == int(w.system.textures.size()));
        CHECK(map->getSquadNumberIcon(4) == m.m_squadNumberIcons[4]
              && w.system.textures.size() == texturesBefore + 1);
        m.m_icons[19].clear();
        line.squad = -1;                                 // not a commander's map: the ring only
        map->styleOrderIcons(2, &line, &request, true);
        CHECK(m.m_icons[19].size() == 1);
        m.m_icons[19].clear();

        // +114 (782220): a squad order (kind 0) of order 5 at world (512, 0, -256), with no leader
        // vehicle (-1). The marker lands at (0.5, 0.25). The line is added as well: 774240 found
        // the start degenerate, but +130 set `valid` again, so it goes in at the default origin with
        // zero length (the 3 is m_field678, the 0 is the unmeasured length times the width).
        players.local = reinterpret_cast<void*>(&mapPlayer);
        mapPlayer.commander = false;
        SquadOrder order;
        order.orderId = 5;
        order.targetNetId = 0;
        order.points[0] = Vec3{512.0f, 0.0f, -256.0f};
        order.points[1] = Vec3{0.0f, 0.0f, 0.0f};
        order.points[2] = Vec3{0.0f, 0.0f, 0.0f};
        order.flag2C = true;
        const Rect orderBounds = { 0.0f, 0.0f, 2.0f, 4.0f, 0.0f };
        int queries = players.localQueries;
        levelSettings.keys.clear();
        CHECK(map->addOrderIcons(&order, 4, -1, 0, &orderBounds));
        CHECK(players.localQueries == queries + 1);
        CHECK(levelSettings.keys.size() == 2 && levelSettings.keys[0] == "GLSWorldSizeX"
              && levelSettings.keys[1] == "GLSWorldSizeZ");  // one point converted: the end
        CHECK(m.m_icons[8].size() == 1 && m.m_icons[20].size() == 1
              && m.m_icons[19].empty() && m.m_icons[21].empty());
        if (m.m_icons[8].size() == 1 && m.m_icons[20].size() == 1) {
            const MapIconRecord& added = m.m_icons[8][0];
            const MapIconRecord& squadMarker = m.m_icons[20][0];
            CHECK(added.position.x == 0.0f && added.position.y == 0.0f);
            CHECK(added.size.x == 3.0f && added.size.y == 0.0f);
            CHECK(textureName(added.texture) == "Ingame/Minimap/map_Line.tga");
            CHECK(hasColor(added.color, attack.r, attack.g, attack.b, 0.625f));
            CHECK(squadMarker.position.x == 0.5f && squadMarker.position.y == 0.25f);
            CHECK(squadMarker.size.x == 15.0f && squadMarker.size.y == 31.0f);
            CHECK(squadMarker.rotation == 0.0f && !squadMarker.clampToVisibleMap);
            CHECK(textureName(squadMarker.texture) == "Ingame/Minimap/Icons/orderAttack.tga");
            CHECK(hasColor(squadMarker.color, 1.0f, 1.0f, 1.0f, 0.625f));
        }
        for (int type : { 8, 19, 20, 21 }) m.m_icons[type].clear();

        // A commander's inactive request (kind 2) for squad 4: numbered 4 + 1 = 5, never a line,
        // and the three icons under 19 in order - ring, number, then the marker +114 adds.
        mapPlayer.commander = true;
        m.m_squadNumberIcons[5] = 0;
        order.orderId = 2;
        order.flag2C = false;
        CHECK(map->addOrderIcons(&order, 4, -1, 2, &orderBounds));
        CHECK(m.m_icons[8].empty() && m.m_icons[20].empty() && m.m_icons[19].size() == 3);
        if (m.m_icons[19].size() == 3) {
            CHECK(m.m_icons[19][0].texture == m.m_field928[0]);
            CHECK(textureName(m.m_icons[19][1].texture)
                  == "Ingame/Player/Icons/Minimap/mini_squadNumber_5.tga");
            CHECK(textureName(m.m_icons[19][2].texture)
                  == "Ingame/Minimap/Icons/icon_vehicledrop.tga");
            CHECK(m.m_icons[19][2].size.x == 15.0f && m.m_icons[19][2].size.y == 15.0f);
        }
        mapPlayer.commander = false;
        for (int type : { 8, 19, 20, 21 }) m.m_icons[type].clear();

        // The exits, each answering true with nothing added: no local player, no order, and the
        // -4000 sentinel without a target (a sentinel with a target is still drawn).
        players.local = nullptr;
        order.orderId = 5;
        order.flag2C = true;
        CHECK(map->addOrderIcons(&order, 4, -1, 0, &orderBounds));
        players.local = reinterpret_cast<void*>(&mapPlayer);
        CHECK(map->addOrderIcons(nullptr, 4, -1, 0, &orderBounds));
        order.points[0] = Vec3{-4000.0f, 0.0f, 0.0f};
        levelSettings.keys.clear();
        CHECK(map->addOrderIcons(&order, 4, -1, 0, &orderBounds));
        CHECK(levelSettings.keys.empty());
        CHECK(m.m_icons[8].empty() && m.m_icons[19].empty() && m.m_icons[20].empty()
              && m.m_icons[21].empty());

        // +F4 (77A8D0), 77E420 and 782480: the squad-leader request marker (filter/type 38).
        {
            m.m_filterStates[1].set(38);
            m.m_icons[38].clear();
            for (int i = 0; i != 10; ++i) m.m_squadRequestTypes[i] = 0;
            m.m_squadRequestTypes[0] = 4;
            m.m_field068 = 0x5151u;                      // the word +F4's clamp reaches
            CHECK(map->getSquadRequestType(-3) == 4);     // negative -> [0]
            CHECK(map->getSquadRequestType(11) == 0x5151);  // clamped to 10: one past, +68
            m.m_squadRequestTypes[0] = 0;

            MapIconRecord source;
            source.position = Vec2{0.25f, -0.75f};
            source.color = Color{1.0f, 1.0f, 1.0f, 0.375f};
            source.size = Vec2{0.0f, 0.0f};
            source.rotation = 5.0f;
            source.clampToVisibleMap = false;
            world::IPlayer* const leader = reinterpret_cast<world::IPlayer*>(&mapPlayer);
            mapPlayer.squad = 3;
            mapPlayer.leader = false;
            m.m_squadRequestTypes[3] = 1;
            m.m_fielda18 = true;
            addSquadRequestIconForFixture(map, 38, leader, &source);
            CHECK(m.m_icons[38].empty());                // not the squad's leader
            mapPlayer.leader = true;
            m.m_squadRequestTypes[3] = 2;
            addSquadRequestIconForFixture(map, 38, leader, &source);
            CHECK(m.m_icons[38].empty());                // a request type other than 1
            m.m_squadRequestTypes[3] = 1;
            m.m_fielda18 = false;
            addSquadRequestIconForFixture(map, 38, leader, &source);
            CHECK(m.m_icons[38].empty());                // +A18 clear
            m.m_fielda18 = true;
            addSquadRequestIconForFixture(map, 38, nullptr, &source);
            CHECK(m.m_icons[38].empty());                // no player
            addSquadRequestIconForFixture(map, 38, leader, &source);
            CHECK(m.m_icons[38].size() == 1);
            if (m.m_icons[38].size() == 1) {
                const MapIconRecord& ring = m.m_icons[38][0];
                const Color& amber = hud::mapSquadRequestColors[1][0];
                CHECK(ring.position.x == 0.25f && ring.position.y == -0.75f);
                CHECK(textureName(ring.texture)
                      == "Ingame/Player/Icons/Minimap/mini_Squad_request.tga");
                CHECK(hasColor(ring.color, amber.r, amber.g, amber.b, 0.375f));
                CHECK(ring.size.x == 30.0f && ring.size.y == 30.0f && ring.rotation == 0.0f);
            }
            m.m_icons[38].clear();

            // 782480 over the typed registry: one registry read, the local fetch made and
            // discarded, then per non-null player getVehicle twice (the projection, then 778680's
            // argument) and 778680's own local fetch. A player without a vehicle classifies as 5.
            world::PlayerManager* const savedManager = world::playerManager;
            world::playerManager = &playerIconManager;
            playerIconManager.players.clear();
            playerIconManager.players.push_back(nullptr);
            playerIconManager.players.push_back(leader);
            const int registryReads = playerIconManager.playerQueries;
            const int localFetches = playerIconManager.localQueries;
            mapPlayer.vehicle = nullptr;
            mapPlayer.vehicleQueries = 0;
            collectSquadRequestIconsForFixture(map);
            CHECK(playerIconManager.playerQueries == registryReads + 1);
            CHECK(playerIconManager.localQueries == localFetches + 2);
            CHECK(mapPlayer.vehicleQueries == 2);
            CHECK(m.m_icons[38].empty());
            playerIconManager.players.clear();
            world::playerManager = savedManager;
            mapPlayer.leader = false;
            mapPlayer.squad = 0;
        }

        // The mini-map's overrides, through the base slots: 4x4 diamonds on +8A4 in the order's
        // colour (the second table copy, A23760 - one shared table here), no inactive grey; a
        // request 8x8 with the +A18 flag as its alpha and never a line; and +114 adds the line
        // only for an active order, the marker for an active order or any request.
        {
            Bf2MiniMenuMapNode* const miniD2 =
                new Bf2MiniMenuMapNode(&w.system, "D2Mini", 0, 0, 1, 1);
            Bf2MenuMapNode* const miniBase = miniD2;
            Bf2MenuMapNode& g = Bf2MenuMapNodePublicFixture::m(miniD2);
            prepare(g);
            MapLineRecord miniLine;
            miniLine.kind = 5; miniLine.networkIdA = -1; miniLine.networkIdB = -1;
            miniLine.squad = 0; miniLine.field10 = true; miniLine.valid = false;
            miniLine.a = Vec2{0.0f, 0.0f}; miniLine.b = Vec2{0.0f, 0.0f};
            MapIconRecord miniMarker;
            miniMarker.size = Vec2{15.0f, 31.0f};
            miniMarker.rotation = 0.0f;
            miniMarker.clampToVisibleMap = false;
            miniBase->styleOrderIcons(1, &miniLine, &miniMarker, false);
            CHECK(miniLine.valid);
            CHECK(miniMarker.texture == g.m_field8a8[18]);
            CHECK(miniMarker.size.x == 4.0f && miniMarker.size.y == 4.0f);
            CHECK(miniMarker.rotation == 0.785398185f);
            CHECK(hasColor(miniMarker.color, attack.r, attack.g, attack.b, 0.625f));
            CHECK(miniLine.line.texture == g.m_defaultMinimapTexture
                  && textureName(g.m_defaultMinimapTexture) == "Ingame/GeneralIcons/full.tga");
            CHECK(hasColor(miniLine.line.color, attack.r, attack.g, attack.b, 0.625f));
            miniLine.kind = 2;
            g.m_fielda18 = true;
            miniBase->styleOrderIcons(2, &miniLine, &miniMarker, true);
            CHECK(!miniLine.valid && miniMarker.texture == g.m_field8a8[5]);
            CHECK(miniMarker.size.x == 8.0f && miniMarker.size.y == 8.0f);
            CHECK(hasColor(miniMarker.color, drop.r, drop.g, drop.b, 1.0f));
            g.m_fielda18 = false;
            miniBase->styleOrderIcons(2, &miniLine, &miniMarker, true);
            CHECK(miniMarker.color.a == 0.0f);

            // +114: an inactive squad order adds nothing at all; an active one both icons (the
            // line as degenerate as the map's); an inactive request its marker alone.
            order.points[0] = Vec3{512.0f, 0.0f, -256.0f};
            order.orderId = 5;
            order.flag2C = false;
            mapPlayer.commander = true;                  // never asked: no squad number here
            CHECK(miniBase->addOrderIcons(&order, 4, -1, 0, &orderBounds));
            CHECK(g.m_icons[8].empty() && g.m_icons[20].empty());
            order.flag2C = true;
            CHECK(miniBase->addOrderIcons(&order, 4, -1, 0, &orderBounds));
            CHECK(g.m_icons[8].size() == 1 && g.m_icons[20].size() == 1);
            if (g.m_icons[20].size() == 1) {
                CHECK(g.m_icons[20][0].position.x == 0.5f && g.m_icons[20][0].position.y == 0.25f);
                CHECK(g.m_icons[20][0].size.x == 4.0f && g.m_icons[20][0].size.y == 4.0f);
            }
            order.orderId = 2;
            order.flag2C = false;
            CHECK(miniBase->addOrderIcons(&order, 4, -1, 2, &orderBounds));
            CHECK(g.m_icons[8].size() == 1 && g.m_icons[19].size() == 1);  // no ring, no number
            if (g.m_icons[19].size() == 1)
                CHECK(g.m_icons[19][0].size.x == 8.0f && g.m_icons[19][0].color.a == 0.0f);
            mapPlayer.commander = false;
            delete miniD2;
        }

        // Tests three pickers with exact float distances: A 2^-12, B 2^-14, C 2^-10.
        // C is outside pickPlayer's 0.0009 and inside pickEnemyVehicle's 0.0016.
        {
            world::PlayerManager* const savedPickManager = world::playerManager;
            IBase* const savedSquadManager = squadManager;
            const int savedTeam = mapPlayer.team;
            const float savedRadius = m.m_field734;
            world::playerManager = &playerIconManager;
            void* vehicleTable[0xE0 / 4];
            for (auto& e : vehicleTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            vehicleTable[0x80 / 4] = reinterpret_cast<void*>(&VehicleInput::getAbsoluteTransformation);
            vehicleTable[0xD4 / 4] = reinterpret_cast<void*>(&VehicleInput::getNetworkable);
            NetworkableInput netA = { nullptr, 0x0123 }, netC = { nullptr, 0x0456 };
            VehicleInput vehicleA = { vehicleTable }, vehicleB = { vehicleTable },
                         vehicleC = { vehicleTable };
            vehicleA.transform[12] = 0.53125f; vehicleA.transform[14] = 0.25f;
            vehicleA.networkable = &netA;
            vehicleB.transform[12] = 0.5f;     vehicleB.transform[14] = 0.265625f;   // no networkable
            vehicleC.transform[12] = 0.5625f;  vehicleC.transform[14] = 0.25f;
            vehicleC.networkable = &netC;
            MapPlayerInput pilotA = { mapPlayerTable, 11, 0, false, false, 1 };
            MapPlayerInput pilotB = { mapPlayerTable, 12, 0, false, false, 1 };
            MapPlayerInput pilotC = { mapPlayerTable, 13, 0, false, false, 1 };
            MapPlayerInput walker = { mapPlayerTable, 14, 0, false, false, 1 };
            MapPlayerInput dead = { mapPlayerTable, 15, 0, false, false, 1 };
            MapPlayerInput downed = { mapPlayerTable, 16, 0, false, false, 1 };
            pilotA.vehicle = &vehicleA; pilotB.vehicle = &vehicleB; pilotC.vehicle = &vehicleC;
            dead.vehicle = &vehicleA;   dead.alive = false;
            downed.vehicle = &vehicleA; downed.manDown = true;
            const auto asPlayer = [](MapPlayerInput& p) {
                return reinterpret_cast<world::IPlayer*>(&p);
            };
            const float scale = 0.5f, clickX = 0.25f, clickY = -0.125f;
            const float twoPow12 = 0.000244140625f, twoPow13 = 0.0001220703125f,
                        twoPow14 = 0.00006103515625f;

            // 776D10 over the live registry (PE +0x40): the nearest vehicle of any team, or -1.
            // getVehicle is asked twice of a player in a vehicle, once of one on foot.
            playerIconManager.players.clear();
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, 1.0f) == -1);
            playerIconManager.players.push_back(nullptr);
            playerIconManager.players.push_back(asPlayer(walker));
            playerIconManager.players.push_back(asPlayer(pilotC));
            playerIconManager.players.push_back(asPlayer(pilotA));
            playerIconManager.players.push_back(asPlayer(pilotB));
            const int registryReads = playerIconManager.playerQueries;
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, 1.0f) == 12);
            CHECK(playerIconManager.playerQueries == registryReads + 1);
            CHECK(walker.vehicleQueries == 1 && pilotA.vehicleQueries == 2
                  && pilotB.vehicleQueries == 2 && pilotC.vehicleQueries == 2);
            CHECK(vehicleA.networkableQueries == 0 && vehicleC.networkableQueries == 0);
            // Both limits are strict: B's own distance refuses B, and nothing else is nearer.
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, twoPow14) == -1);
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, twoPow13) == 12);
            playerIconManager.players.remove(asPlayer(pilotB));
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, 1.0f) == 11);
            playerIconManager.players.remove(asPlayer(pilotA));
            CHECK(pickPlayerForFixture(map, scale, clickX, clickY, 1.0f) == -1);  // C: 0.0009
            playerIconManager.players.clear();

            // 77B410: a copy of the enemy team's registry (PE +0x3C). No local, or no gameLogic,
            // answers 0 before getTeam is asked.
            StrategicLogicInput pickerLogic;
            playerIconManager.local = nullptr;
            gameLogic = &pickerLogic;
            playerIconManager.teamQueries.clear();
            mapPlayer.teamQueries = 0;
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, 1.0f) == 0);
            playerIconManager.local = asPlayer(mapPlayer);
            gameLogic = nullptr;
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, 1.0f) == 0);
            CHECK(mapPlayer.teamQueries == 0 && playerIconManager.teamQueries.empty());
            // Dead, man-down and on-foot players are refused before the second getVehicle. A
            // vehicle without a networkable is refused without tightening the limit: C (2^-10)
            // wins first, B (2^-14) is refused, and A (2^-12) still beats C.
            gameLogic = &pickerLogic;
            mapPlayer.team = 2;
            playerIconManager.teamPlayers.clear();
            for (MapPlayerInput* p : { &walker, &dead, &downed, &pilotC, &pilotB, &pilotA })
                p->vehicleQueries = 0;
            playerIconManager.teamPlayers.push_back(nullptr);
            for (MapPlayerInput* p : { &walker, &dead, &downed, &pilotC, &pilotB, &pilotA })
                playerIconManager.teamPlayers.push_back(asPlayer(*p));
            for (VehicleInput* v : { &vehicleA, &vehicleB, &vehicleC })
                v->transformQueries = v->networkableQueries = 0;
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, 1.0f) == 0x0123);
            CHECK(mapPlayer.teamQueries == 1);
            CHECK(playerIconManager.teamQueries.size() == 1 && playerIconManager.teamQueries[0] == 1);
            CHECK(walker.vehicleQueries == 1 && dead.vehicleQueries == 1
                  && downed.vehicleQueries == 1 && pilotA.vehicleQueries == 2);
            CHECK(vehicleA.transformQueries == 1 && vehicleA.networkableQueries == 1);
            CHECK(vehicleB.transformQueries == 1 && vehicleB.networkableQueries == 1);
            CHECK(vehicleC.transformQueries == 1 && vehicleC.networkableQueries == 1);
            // A's own distance as the limit: C is outside it, B has no networkable, A is not
            // strictly nearer - nothing.
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, twoPow12) == 0);
            // The enemy is `team == 1 ? 2 : 1`: team 1 asks 2, and ANY other team asks 1.
            mapPlayer.team = 1;
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, 1.0f) == 0x0123);
            CHECK(playerIconManager.teamQueries.back() == 2);
            mapPlayer.team = 3;
            CHECK(pickEnemyVehicleForFixture(map, scale, Vec2{clickX, clickY}, 1.0f) == 0x0123);
            CHECK(playerIconManager.teamQueries.back() == 1);
            playerIconManager.teamPlayers.clear();

            // 773B60: requests 0..3 of squads 0..9 of the local team, getTeam asked once per
            // squad. The radius is m_field734 / getZoomScale(false).y, with (1, 1) here.
            gameLogic = nullptr;
            m.m_field734 = 0.01f;
            void* squadManagerTable[0x40 / 4];
            void* squadInfoTable[0x28 / 4];
            for (auto& e : squadManagerTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            for (auto& e : squadInfoTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            squadManagerTable[0x28 / 4] = reinterpret_cast<void*>(&SquadManagerInput::getSquadInfo);
            squadInfoTable[0x18 / 4] = reinterpret_cast<void*>(&SquadInfoInput::getRequest);
            SquadInfoInput squad3 = { squadInfoTable }, squad5 = { squadInfoTable },
                           squad7 = { squadInfoTable };
            SquadManagerInput squadsInput = { squadManagerTable };
            squadsInput.infos[3] = &squad3;
            squadsInput.infos[5] = &squad5;
            squadsInput.infos[7] = &squad7;
            squadManager = reinterpret_cast<IBase*>(&squadsInput);
            // At 1/1024 the click (0.5, 0.25) is 0 from `nearRequest`, 2^-10 from `farRequest`
            // (inside the 0.01 radius) and 2^-6 from `outsideRequest` (outside it).
            SquadOrder nearRequest, farRequest, outsideRequest;
            nearRequest.points[0] = Vec3{512.0f, 0.0f, -256.0f};
            farRequest.points[0] = Vec3{544.0f, 0.0f, -256.0f};
            outsideRequest.points[0] = Vec3{640.0f, 0.0f, -256.0f};
            squad3.requests[0] = &farRequest;
            squad5.requests[1] = &outsideRequest;
            squad7.requests[2] = &nearRequest;
            squad7.requests[3] = &nearRequest;       // a tie: strictly nearer only, so type 2 stays
            const float requestScale = 0.0009765625f;
            const auto resetRequestPick = [&m]() {
                m.m_selectedRequestSquad = -1;
                m.m_selectedRequestType = 0;
                m.m_selectedRequestOrderType = static_cast<OrderType>(0);
                m.m_commanderPosition = Vec2{-4000.0f, -4000.0f};
            };
            resetRequestPick();
            playerIconManager.local = nullptr;
            CHECK(!pickSquadRequestForFixture(map, requestScale, Vec2{0.5f, 0.25f}, 1.0f));
            CHECK(squadsInput.queries.empty());
            playerIconManager.local = asPlayer(mapPlayer);
            mapPlayer.team = 2;
            mapPlayer.teamQueries = 0;
            levelSettings.keys.clear();
            CHECK(pickSquadRequestForFixture(map, requestScale, Vec2{0.5f, 0.25f}, 1.0f));
            CHECK(mapPlayer.teamQueries == 10 && squadsInput.queries.size() == 10);
            bool squadQueriesInOrder = squadsInput.queries.size() == 10;
            for (std::size_t i = 0; squadQueriesInOrder && i != squadsInput.queries.size(); ++i)
                squadQueriesInOrder = squadsInput.queries[i].first == 2
                    && squadsInput.queries[i].second == static_cast<int>(i);
            CHECK(squadQueriesInOrder);
            CHECK(squad3.types.size() == 4 && squad5.types.size() == 4 && squad7.types.size() == 4);
            CHECK(m.m_selectedRequestSquad == 7 && m.m_selectedRequestType == 2
                  && m.m_selectedRequestOrderType == static_cast<OrderType>(2));
            CHECK(m.m_commanderPosition.x == 0.5f && m.m_commanderPosition.y == 0.25f);
            CHECK(levelSettings.keys.size() == 4);   // two hits mapped: squad 3's, then squad 7's
            // The radius alone refuses: only the outside request left, the limit still wide open.
            squadsInput.infos[3] = nullptr;
            squadsInput.infos[7] = nullptr;
            resetRequestPick();
            CHECK(!pickSquadRequestForFixture(map, requestScale, Vec2{0.5f, 0.25f}, 1.0f));
            CHECK(m.m_selectedRequestSquad == -1 && m.m_commanderPosition.x == -4000.0f);
            // ... and a wider zoom radius admits it (0.02 > 2^-6).
            m.m_field734 = 0.02f;
            CHECK(pickSquadRequestForFixture(map, requestScale, Vec2{0.5f, 0.25f}, 1.0f));
            CHECK(m.m_selectedRequestSquad == 5 && m.m_selectedRequestType == 1);
            CHECK(m.m_commanderPosition.x == 0.625f && m.m_commanderPosition.y == 0.25f);

            resetRequestPick();
            m.m_field734 = savedRadius;
            playerIconManager.local = nullptr;
            mapPlayer.team = savedTeam;
            squadManager = savedSquadManager;
            world::playerManager = savedPickManager;
            gameLogic = nullptr;
        }

        // Supply-line collection visits group 2 first. A failed stage-3 texture load
        // leaves zero handles from stage 3 onward; these change neither texture nor latch.
        {
            SupplyLineLogicInput supplyLogic;
            supplyLogic.groups = 3;
            gameLogic = &supplyLogic;
            void* objectTable[0x100 / 4];
            void* pointTable[0x60 / 4];
            void* templateTable[0x100 / 4];
            for (auto& e : objectTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            for (auto& e : pointTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            for (auto& e : templateTable) e = reinterpret_cast<void*>(&unexpectedMapSlot);
            objectTable[0x0C / 4] = reinterpret_cast<void*>(&SupplyObjectInput::queryInterface);
            pointTable[0x1C / 4] = reinterpret_cast<void*>(&SupplyControlPointInput::getTeam);
            pointTable[0x2C / 4] =
                reinterpret_cast<void*>(&SupplyControlPointInput::getAllowCaptureByTeam);
            pointTable[0x38 / 4] = reinterpret_cast<void*>(&SupplyControlPointInput::getShowOnMinimap);
            templateTable[0x0C / 4] = reinterpret_cast<void*>(&SupplyTemplateInput::queryInterface);
            templateTable[0x2C / 4] = reinterpret_cast<void*>(&SupplyTemplateInput::getControlPointId);
            templateTable[0xBC / 4] = reinterpret_cast<void*>(&SupplyTemplateInput::getGroupId);
            struct Spec { int group, id; bool shown, capturable; };
            const Spec specs[6] = {
                { 1, 101, true, true }, { 2, 201, true, true }, { 2, 202, true, true },
                { 2, 203, false, true },     // not shown on the minimap: never counts
                { 2, 204, true, false },     // not capturable by team 1: never counts
                { 3, 301, true, true },
            };
            SupplyTemplateInput templates[6] = {};
            SupplyControlPointInput points[6] = {};
            SupplyObjectInput objects[7] = {};
            for (int i = 0; i != 6; ++i) {
                templates[i].table = templateTable;
                templates[i].id = specs[i].id;
                templates[i].group = specs[i].group;
                points[i].table = pointTable;
                points[i].team = 2;
                points[i].shown = specs[i].shown;
                points[i].capturableBy1 = specs[i].capturable;
                objects[i].table = objectTable;
                objects[i].pointTemplate = &templates[i];
                objects[i].point = &points[i];
            }
            objects[6].table = objectTable;          // an object that is no control point at all

            m.m_fieldb94 = m.m_fieldb95 = false;
            m.m_fieldb98 = m.m_fieldb9c = m.m_fieldba0 = 0;
            for (int& texture : m.m_supplyLineTextures) texture = 0;
            m.m_controlPointIds.clear();
            const int shownBefore = m.m_field93c[1];
            const std::size_t loadsBefore = w.system.textures.size();
            const auto ids = [&m]() {
                return std::vector<int>(m.m_controlPointIds.begin(), m.m_controlPointIds.end());
            };

            // An empty control-point map: no first pass, and no ids, so nothing at all.
            updateSupplyLineForFixture(map, &w.system);
            CHECK(supplyLogic.mapQueries == 1 && !m.m_fieldb94 && m.m_fieldba0 == 0);
            CHECK(w.system.textures.size() == loadsBefore && m.m_field93c[1] == shownBefore);

            for (unsigned int key = 1; key <= 6; ++key)
                supplyLogic.controlPoints[key] = reinterpret_cast<world::IObject*>(&objects[key - 1]);
            supplyLogic.controlPoints[7] = nullptr;
            supplyLogic.controlPoints[8] = reinterpret_cast<world::IObject*>(&objects[6]);
            w.system.failTexture = "Levels/Strike_At_Karkand/HUD/Minimap/supplyline_03.tga";
            const int nameQueries = fixtureGame.nameQueries;

            // The first pass: group 2's two counting points, stages 1..4 wanted, 3 fails.
            updateSupplyLineForFixture(map, &w.system);
            CHECK(w.system.textures.size() == loadsBefore + 3);
            if (w.system.textures.size() == loadsBefore + 3) {
                CHECK(w.system.textures[loadsBefore]
                      == "Levels/Strike_At_Karkand/HUD/Minimap/supplyline_01.tga");
                CHECK(w.system.textures[loadsBefore + 2]
                      == "Levels/Strike_At_Karkand/HUD/Minimap/supplyline_03.tga");
            }
            CHECK(fixtureGame.nameQueries == nameQueries + 3);           // once per stage tried
            const int stage1 = static_cast<int>(loadsBefore) + 1;
            const int stage2 = static_cast<int>(loadsBefore) + 2;
            CHECK(m.m_supplyLineTextures[1] == stage1 && m.m_supplyLineTextures[2] == stage2
                  && m.m_supplyLineTextures[3] == 0 && m.m_supplyLineTextures[4] == 0);
            CHECK(m.m_fieldb94 && m.m_fieldb98 == 1 && m.m_fieldba0 == 2 && m.m_fieldb9c == 1
                  && !m.m_fieldb95);
            CHECK(m.m_field93c[1] == stage1);
            CHECK(ids() == std::vector<int>({ 201, 202 }));
            CHECK(supplyLogic.groupQueries == 2);                         // 77C0A0, then 772D80
            // A match's group is asked twice in 77C0A0 and once more in the count; a point that
            // is hidden or not capturable never reaches its template.
            CHECK(templates[1].groupQueries == 3 && templates[0].groupQueries == 2);
            CHECK(templates[3].groupQueries == 0 && templates[4].groupQueries == 0);
            CHECK(points[4].captureTeams == std::vector<int>({ 1, 1 }));
            // Both group-2 points still held by team 2 (== the group's size): nothing moves.

            points[1].team = 1;                                   // one of two taken: one stage on
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 2 && m.m_field93c[1] == stage2 && m.m_fieldb95);
            updateSupplyLineForFixture(map, &w.system);           // latched: no second step
            CHECK(m.m_fieldb9c == 2 && m.m_field93c[1] == stage2 && m.m_fieldb95);
            points[1].team = 2;                                   // retaken: one stage back
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 1 && m.m_field93c[1] == stage1 && !m.m_fieldb95);
            points[1].team = 1;                                   // the whole group falls
            points[2].team = 1;
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 2 && m.m_field93c[1] == stage2 && !m.m_fieldb95);
            CHECK(m.m_fieldba0 == 3 && ids() == std::vector<int>({ 301 }));
            // Group 3's point turns neutral: fewer held than the group has, so one stage on - but
            // stage 3 has no texture, so neither +940 nor the latch changes, and the next pass
            // steps again. getTeam is asked three times of a neutral point.
            points[5].team = 0;
            points[5].teamQueries = 0;
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 3 && m.m_field93c[1] == stage2 && !m.m_fieldb95);
            CHECK(points[5].teamQueries == 3);
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 4 && m.m_field93c[1] == stage2 && !m.m_fieldb95);
            // Group 3 falls: on to group 4, which does not exist, so the ids stay empty and every
            // later pass returns right after reading the map.
            points[5].team = 1;
            updateSupplyLineForFixture(map, &w.system);
            CHECK(m.m_fieldb9c == 5 && m.m_field93c[1] == stage2 && m.m_fieldba0 == 4);
            CHECK(m.m_controlPointIds.empty());
            const int mapReads = supplyLogic.mapQueries;
            updateSupplyLineForFixture(map, &w.system);
            CHECK(supplyLogic.mapQueries == mapReads + 1 && m.m_fieldb9c == 5);

            // 775020: the local team is asked (and dropped) before the alpha test, then the stage
            // texture is drawn once over the bounds, with the secondary texture when +10C says so.
            world::PlayerManager* const savedDrawManager = world::playerManager;
            world::playerManager = &playerIconManager;
            playerIconManager.local = reinterpret_cast<world::IPlayer*>(&mapPlayer);
            RenderContext context = { nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            const Rect drawBounds = { 10.0f, 20.0f, 200.0f, 100.0f, 0.0f };
            const float savedAlpha = m.m_field708[2];
            const float savedAngle = m.m_field76c;
            const bool saved68c = m.m_field68c, saved68d = m.m_field68d, savedExpanded = m.m_expandedMap;
            m.m_field76c = 0.0f;
            m.m_field68c = m.m_field68d = m.m_expandedMap = false;   // +10C answers true
            w.system.drawn.clear();
            mapPlayer.teamQueries = 0;
            m.m_field708[2] = 0.7f;                                  // alpha 0: nothing drawn
            drawSupplyLineForFixture(map, &drawBounds, &drawBounds, &w.system, &context);
            CHECK(w.system.drawn.empty() && mapPlayer.teamQueries == 1);
            m.m_field708[2] = 0.9375f;
            drawSupplyLineForFixture(map, &drawBounds, &drawBounds, &w.system, &context);
            CHECK(w.system.drawn.size() == 1 && mapPlayer.teamQueries == 2);
            if (w.system.drawn.size() == 1) {
                const FixtureSystem::DrawnQuad& d = w.system.drawn[0];
                CHECK(d.texture == stage2 && d.texture2 == m.m_field928[1]);
                CHECK(d.color.r == 1.0f && d.color.g == 1.0f && d.color.b == 1.0f
                      && d.color.a == 0.9375f - 0.7f);
            }
            m.m_field708[2] = savedAlpha;
            m.m_field76c = savedAngle;
            m.m_field68c = saved68c; m.m_field68d = saved68d; m.m_expandedMap = savedExpanded;
            w.system.drawn.clear();
            playerIconManager.local = nullptr;
            world::playerManager = savedDrawManager;

            w.system.failTexture.clear();
            m.m_field93c[1] = shownBefore;
            m.m_controlPointIds.clear();
            gameLogic = nullptr;
        }

        players.local = nullptr;
        for (int type : { 8, 19, 20, 21 }) m.m_icons[type].clear();
        settings = savedSettings;
        gameLogic = savedLogic;
    }
    // The focused node used the same real event recorder; isolate the outer
    // map's destruction assertions from that node's two completed unregisters.
    w.bus.unregistered.clear();
    w.bus.whoOff.clear();

    // destruction unregisters both handlers, in the same order, through the same face
    delete map;
    CHECK(w.bus.unregistered.size() == 2 && w.bus.unregistered[0] == 2 && w.bus.unregistered[1] == 5);
    CHECK(w.bus.whoOff.size() == 2 && w.bus.whoOff[0] == face && w.bus.whoOff[1] == face);

    std::printf("HUD map creation %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
