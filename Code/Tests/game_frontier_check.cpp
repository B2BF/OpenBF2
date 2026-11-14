// [TEMP] Cumulative fixture for game frontier-closure sites that no normal session reaches
// (docs/tracks/game/frontier-closure.md). Each case names the ledger row it covers.
// macOS: CTest bf2_game_frontier_check (built on request); death cases run in a forked child.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#define private public
#define protected public
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/External/BFMeme/Node.h"
#include "../BF2/External/BFMeme/System.h"
#include "../BF2/Menu/Bf2EditNode.h"
#include "../BF2/Menu/BfNewListBoxNode.h"
#include "../BF2/Menu/BfListBoxData.h"
#include "../BF2/External/BFMeme/Action.h"
#include "../BF2/External/BFMeme/Tree.h"
#include "../BF2/External/BFMeme/Stream.h"
#include "../BF2/External/BFMeme/TypeDescriptor.h"
#include "../BF2/Menu/ProfileConsoleBindings.h"
#include "../BF2/Game/Objects/IObject.h"
#include "../BF2/Game/DistributedLightRenderer/DistributedLightRenderer.h"
#include "../BF2/AI/AIInterface/BFKnowledge/BFGame.h"
#undef protected
#undef private
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Core/ConsoleObjectBase.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameServer.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/Common/GameEvent.h"
#include "../BF2/Geom/LightmapGeneration/ILightmapGeneration.h"
#include "../BF2/Game/GeneralSettings.h"
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/AI/AIDLL/AI/Main/AISettingsInterfaces.h"
#include "../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include <cstring>
#include <map>

namespace dice { namespace hfe { ClassManager* classManager = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class EventManager; EventManager* eventManager = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class ServerSettings; ServerSettings* serverSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_audioSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_videoSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_globalSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_hapticSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_controlSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_generalSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class IBase; IBase* g_gameServerSettings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { namespace io { class FileManager; FileManager* fileManager = nullptr; } } }   // fixture-owned global
namespace dice { namespace hfe { namespace io { class OldConsole; OldConsole* mainConsole = nullptr; } } }   // fixture-owned global
namespace dice { namespace hfe { namespace io { class IBase; IBase* controlMapManager = nullptr; } } }   // fixture-owned global
namespace dice { namespace hfe { namespace io { class InputDeviceManager; InputDeviceManager* inputDeviceManager = nullptr; } } }   // fixture-owned global
namespace dice { namespace hfe { class MapList; MapList* mapList = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { class SettingsRepostitory; SettingsRepostitory* settings = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { namespace world { class PlayerManager; PlayerManager* playerManager = nullptr; } } }   // fixture-owned global
namespace dice { namespace hfe { class IGameLogic; IGameLogic* gameLogic = nullptr; } }   // fixture-owned global
namespace dice { namespace hfe { IGame* game = nullptr; } }   // fixture-owned global
void* dice::hfe::ai::CommonSingletonServer::m_iaiSettings = nullptr;   // fixture-owned (CommonSingletonServer.cpp is not linked)
void* dice::hfe::ai::CommonSingletonServer::m_iaiGame = nullptr;       // fixture-owned
namespace dice { namespace hfe { IBase* spawnManager = nullptr; namespace world { IBase* scoreManager = nullptr; } } }   // fixture-owned globals (BFGame.cpp)
namespace dice { namespace hfe { namespace world { ObjectManager* objectManager = nullptr; } } }   // fixture-owned global
std::string dice::hfe::io::Console::ignoredString_;   // fixture-owned (Console.cpp is not linked)
using namespace dice::hfe;

static int checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

// True when `body` terminates the child abnormally (retail's int 3 / abort), false if it returns.
template <class Body>
static bool dies(Body body)
{
    std::fflush(stdout);
    const pid_t child = fork();
    if (child == 0) {
        body();
        _exit(0);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

// Game/LocalProfileManager.h:47/48 - PE+30 4A7690 and PE+34 4553F0.
static void localProfileSlots()
{
    std::printf("-- LocalProfileManager PE+30 / PE+34\n");
    // Never destroyed: the destructor's shutdown() logs out through the class manager, which this
    // fixture does not have.
    LocalProfileManager& manager = *new LocalProfileManager;
    CHECK(!manager.hasCurrentProfile(0));
    CHECK(!manager.hasCurrentProfile(12345));                 // the argument is ignored
    manager.m_currentProfile = reinterpret_cast<LocalProfile*>(&manager);   // any non-null
    CHECK(manager.hasCurrentProfile(0));
    manager.m_currentProfile = nullptr;
    CHECK(manager.slot34(0, 0) && manager.slot34(-1, 7));
    ILocalProfileManager* iface = &manager;
    CHECK(!iface->hasCurrentProfile(1) && iface->slot34(1, 2));
}

// Physics/MaterialManager.cpp:102/312/332 - retail's own trap for an id >= 10000 (BF2.exe int 3,
// the bf2 ELF abort); valid ids are stored as before.
static void materialManagerTraps()
{
    std::printf("-- MaterialManager id guard\n");
    {
        world::MaterialManager manager;
        manager.setAttMaterial(5);
        manager.setDefMaterial(9999);
        CHECK(manager.m_attMaterial == 5 && manager.m_defMaterial == 9999);
        CHECK(manager.m_materials.count(5) == 1);
        manager.removeMaterial(5);
        CHECK(manager.m_materials.count(5) == 0);
    }
    CHECK(dies([] { world::MaterialManager m; m.removeMaterial(10000); }));
    CHECK(dies([] { world::MaterialManager m; m.setAttMaterial(10000); }));
    CHECK(dies([] { world::MaterialManager m; m.setDefMaterial(0xFFFFFFFFu); }));
    CHECK(!dies([] { world::MaterialManager m; m.setDefMaterial(9999); }));
}

namespace {
class FixtureSystem : public dice::meme::System
{
public:
    FixtureSystem() : System(800.0f, 600.0f) {}
    dice::meme::TypeDescriptor* getType() override { return dice::meme::Object::getClassType(); }
};
}

// External/BFMeme/Node.cpp:221 - Node virtual +A0: every class in the tree but the edit nodes keeps
// the 455410 false default; a focused BfEditNode/Bf2EditNode drops focus and unlocks (7AA9B0).
static void nodeEvent16()
{
    std::printf("-- Node +A0 (dispatchNodeEvent16)\n");
    FixtureSystem system;
    dice::meme::Node plain;
    CHECK(!dice::meme::dispatchNodeEvent16(&plain, &system, nullptr, nullptr));
    dice::meme::Bf2EditNode& edit = *new dice::meme::Bf2EditNode();
    edit.m_focused = true;
    system.lockPointer();
    CHECK(!dice::meme::dispatchNodeEvent16(&edit, &system, nullptr, nullptr));
    CHECK(!edit.m_focused && system.getLockCount() == 0);
}

namespace {
class CountingAction : public dice::meme::Action
{
public:
    int calls = 0;
    bool execute(dice::meme::System*, const dice::meme::Rect*, const dice::meme::NodeState*,
                 const dice::meme::NodeInputEvent*) override { ++calls; return true; }
};
}

// Menu/BfNewListBoxNode.cpp onEvent3 (BF2.exe 0x7C46D0): the double-click release.
static void listBoxEvent3()
{
    std::printf("-- BfNewListBoxNode::onEvent3\n");
    using namespace dice::meme;
    FixtureSystem system;
    BfListBoxData& data = *new BfListBoxData();
    data.addColumn(100, true, true);
    data.appendCell(0, std::string("a"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    data.appendCell(0, std::string("b"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    data.m_row = 1;
    BfNewListBoxNode& node = *new BfNewListBoxNode();
    CountingAction& action = *new CountingAction();
    node.m_data = &data;
    node.m_selectAction = &action;
    node.m_selectable = true;
    node.m_scrollWidth = 10.0f;
    node.m_scrollOffset = 0.0f;
    node.m_top = 60;                       // onEvent3 has no top-inset test: py 50 still passes
    node.m_bottom = 0;
    Rect bounds;
    bounds.x = 0.0f; bounds.y = 0.0f; bounds.width = 200.0f; bounds.height = 100.0f;
    NodeState pressed;
    pressed.target = -1.0f; pressed.current = -1.0f;
    system.m_pointerX = 50.0f;
    system.m_pointerY = 50.0f;
    auto arm = [&] { node.m_field058 = 0.5f; node.m_field060 = 0.2f; node.m_field05c = 0.3f; node.m_field064 = 1; };

    arm();
    CHECK(!node.onEvent3(&system, &bounds, &pressed, 1, 0));
    CHECK(action.calls == 1 && node.m_field064 == -1 && node.m_field058 == 0.0f && node.m_field05c == 0.0f);
    CHECK(!node.m_field074 && !node.m_field07c && node.m_field080 == -1);

    arm();
    CHECK(node.onEvent3(&system, &bounds, &pressed, 21, 0) && action.calls == 1);   // code 1 only

    arm();
    node.m_selectable = false;             // read after the timing marks: the marks fold
    CHECK(node.onEvent3(&system, &bounds, &pressed, 1, 0) && action.calls == 1 && node.m_field05c == 0.5f);
    node.m_selectable = true;

    arm();
    pressed.current = 1.0f;                // a released state: the marks fold
    CHECK(node.onEvent3(&system, &bounds, &pressed, 1, 0) && action.calls == 1 && node.m_field05c == 0.5f);
    pressed.current = -1.0f;

    arm();
    system.m_pointerX = 195.0f;            // over the scroll bar
    CHECK(node.onEvent3(&system, &bounds, &pressed, 1, 0) && action.calls == 1 && node.m_field05c == 0.3f);
}

namespace {
class RecordingCellResource : public dice::meme::IListBoxCellResource
{
public:
    std::vector<int> released;
    int finished = 0;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 0; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    void unused10() override {}
    void unused14() override {}
    void unused18() override {}
    void unused1C() override {}
    void releaseCell(int resourceIndex) override { released.push_back(resourceIndex); }
    void unused24() override {}
    void unused28() override {}
    void finishPaint() override { ++finished; }
};
}

// Menu/BfListBoxData.cpp:31 - clearCell/copyCell hand the +0x48 resource the cell's index at +0x20.
static void listBoxCellResource()
{
    std::printf("-- BfListBoxData cell resource\n");
    using namespace dice::meme;
    BfListBoxData& data = *new BfListBoxData();
    data.addColumn(100, true, true);
    data.appendCell(0, std::string("a"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    data.appendCell(0, std::string("b"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    RecordingCellResource resource;
    data.clearCell(0, 0);                                   // no resource: nothing to release
    data.m_resource = &resource;
    data.m_columns[0]->cells[1]->resourceIndex = 7;
    data.clearCell(0, 1);
    CHECK(resource.released.size() == 1 && resource.released[0] == 7);
    CHECK(data.m_columns[0]->cells[1]->resourceIndex == -1);
    data.m_columns[0]->cells[0]->resourceIndex = 3;
    data.copyCell(0, 0, data.m_columns[0]->cells[1]);
    CHECK(resource.released.size() == 2 && resource.released[1] == 3);
    data.clearCell(0, 5);                                   // out of range: untouched
    CHECK(resource.released.size() == 2);
    data.m_resource = nullptr;                              // the destructor would release it
}

namespace {
// Records streamBool on save and replays it on load.
class MarkerStream : public dice::meme::Stream
{
public:
    std::vector<bool> marks;
    size_t next = 0;
    bool replay = false;
    bool streamBool(bool* value, const char* name) override
    {
        if (std::string(name) != "Bool") return false;
        if (replay) { if (next >= marks.size()) return false; *value = marks[next++]; }
        else marks.push_back(*value);
        return true;
    }
};
int deletedNodes = 0;
class CountingNode : public dice::meme::TreeNode
{
public:
    using TreeNode::TreeNode;
    ~CountingNode() override { ++deletedNodes; }
};
class CountingTree : public dice::meme::Tree
{
public:
    dice::meme::TreeNode* createNode(dice::meme::System*, dice::meme::TreeNode* parent, dice::meme::TreeNode* next,
                                     dice::meme::TreeNode* previous, dice::meme::TreeNode* child) override
    { return new CountingNode(parent, next, previous, child); }
};
}

// External/BFMeme/Stream.cpp:21 - getTreeClassType is dice::meme::Tree (BF2.exe 0x83C8C0..0x83D0F0).
static void memeTree()
{
    std::printf("-- dice::meme::Tree\n");
    using namespace dice::meme;
    FixtureSystem system;
    TypeDescriptor* type = getTreeClassType();
    CHECK(type == Tree::getClassType() && std::string(type->getName()) == "dice::meme::Tree");
    Tree* created = static_cast<Tree*>(type->create());
    CHECK(created && created->m_root == nullptr && created->m_name == nullptr && created->getType() == type);
    created->setName("menuTree");
    CHECK(std::string(created->getName()) == "menuTree");

    // root A: child B, next C, previous D.
    Tree source;
    TreeNode* a = source.createNode(&system, nullptr, nullptr, nullptr, nullptr);
    a->m_child = source.createNode(&system, a, nullptr, nullptr, nullptr);
    a->m_next = source.createNode(&system, nullptr, nullptr, a, nullptr);
    a->m_previous = source.createNode(&system, nullptr, a, nullptr, nullptr);
    source.m_root = a;
    MarkerStream stream;
    CHECK(source.visit0x38(&system, &stream));
    const bool expected[] = { true, true, false, false, false, true, false, false, true, false, false };
    CHECK(stream.marks == std::vector<bool>(expected, expected + 11));

    CountingTree loaded;
    stream.replay = true;
    CHECK(loaded.visit0x34(&system, &stream) && stream.next == 11);
    TreeNode* root = loaded.m_root;
    CHECK(root && root->m_parent == nullptr && root->m_child && root->m_next && root->m_previous);
    CHECK(root->m_child->m_parent == root && !root->m_child->m_child && !root->m_child->m_next);
    CHECK(root->m_next->m_previous == root && root->m_next->m_parent == nullptr && !root->m_next->m_next);
    CHECK(root->m_previous->m_next == root && !root->m_previous->m_previous);

    stream.replay = false;
    stream.marks.clear();
    CHECK(loaded.streamProperties(&system, &stream) && stream.marks.empty());   // nodes have no properties
    deletedNodes = 0;
    CHECK(loaded.visit0x2c(&system, &stream) && deletedNodes == 4 && loaded.m_root == nullptr);

    TreeNode* single = loaded.createNode(&system, nullptr, nullptr, nullptr, nullptr);
    loaded.deleteNode(&system, single);
    CHECK(single == nullptr && deletedNodes == 5);
    bool found = true;
    CHECK(loaded.slot_0x40(&system, nullptr, &found, nullptr) == nullptr && !found);

    MarkerStream truncated;
    truncated.replay = true;                   // no marks: the first streamBool fails
    CHECK(!loaded.visit0x34(&system, &truncated) && loaded.m_root == nullptr);
}


// Menu/ProfileConsoleBindings.cpp:98/104/110/116 - the 17 profile rows (invokers at their tables' +5C)
// and the store behind them, BF2.exe 0x7BF950..0x7C1340. applySettings needs a live local player: the
// loopback script profile-console covers it.
static std::string runProfile(ConsoleObject* binding, std::vector<std::string> args)
{
    return static_cast<ConsoleObjectBaseImpl*>(binding)->execute(args.empty() ? nullptr : &args[0],
                                                                  static_cast<int>(args.size()));
}

static void profileConsole()
{
    std::printf("-- profile console rows\n");
    using namespace dice::hfe::hud;
    std::vector<ConsoleObject*> rows(profileConsoleBindingList().begin(), profileConsoleBindingList().end());
    CHECK(rows.size() == 17);
    if (rows.size() != 17) return;
    enum { create, activeProfile, activateProfile, rank, addCharacter, activeCharacter, activateCharacter,
           addMod, activeMod, playerName, visualUnlock, defaultKit, tagIcon, tagText, setIndex, getIndex, apply };
    CHECK(std::string(rows[apply]->getFunctionName()) == "applySettings");
    ProfileConsoleOwner* owner = new ProfileConsoleOwner;
    CHECK(profileConsoleOwner == owner && rows[create]->isActive() && !rows[rank]->isActive());

    CHECK(runProfile(rows[activeProfile], {}) == "");                  // the owner's empty name
    CHECK(runProfile(rows[create], {"Alpha"}) == "1");
    PlayerProfile* alpha = owner->m_selectedProfile;
    CHECK(alpha && alpha->m_name == "Alpha" && alpha->m_id == 1 && alpha->m_rank == -1 && rows[rank]->isActive());
    CHECK(runProfile(rows[create], {"Alpha"}) == "0" && owner->m_selectedProfile == nullptr);
    CHECK(runProfile(rows[create], {"Bravo"}) == "1" && owner->m_selectedProfile->m_id == 2);
    CHECK(runProfile(rows[activeProfile], {"Alpha"}) == "" && owner->m_selectedProfile == alpha);
    CHECK(runProfile(rows[activeProfile], {}) == "Alpha");
    CHECK(runProfile(rows[activeProfile], {"Nobody"}) == "" && owner->m_selectedProfile == nullptr);
    runProfile(rows[activeProfile], {"Alpha"});
    CHECK(runProfile(rows[rank], {}) == "-1");
    CHECK(runProfile(rows[rank], {"7"}) == "" && runProfile(rows[rank], {}) == "7");
    CHECK(runProfile(rows[create], {}) == "1");                       // wrong arity: this row's last result kept

    // No characters yet: activation passes an empty name and clears the active character.
    CHECK(runProfile(rows[activateProfile], {"Alpha"}) == "1" && owner->m_activeProfile == alpha);
    CHECK(alpha->m_activeCharacter == nullptr && !rows[apply]->isActive());
    CHECK(runProfile(rows[activateProfile], {"Nobody"}) == "0" && owner->m_activeProfile == nullptr);

    CHECK(runProfile(rows[addCharacter], {"Hero"}) == "1");
    CharacterProfile* hero = alpha->m_selectedCharacter;
    CHECK(hero && hero->m_name == "Hero" && hero->m_id == 1 && rows[activeMod]->isActive());
    CHECK(hero->m_mods.size() == 1 && hero->m_activeMod == hero->m_mods["BF2"]);
    CHECK(runProfile(rows[activeCharacter], {}) == "Hero" && runProfile(rows[activeMod], {}) == "BF2");
    ModSetting* bf2 = hero->m_activeMod;
    CHECK(bf2->m_playerName.empty() && bf2->m_visualUnlockLevel == 0 && bf2->m_tagIcon.empty() && bf2->m_kitIndices.empty());
    CHECK(runProfile(rows[addMod], {"xpack"}) == "1" && runProfile(rows[activeMod], {}) == "xpack");
    ModSetting* xpack = hero->m_activeMod;
    CHECK(runProfile(rows[addMod], {"xpack"}) == "0" && hero->m_activeMod == xpack);   // duplicate: unchanged
    CHECK(runProfile(rows[activeMod], {"none"}) == "" && hero->m_activeMod == xpack);  // unknown: unchanged
    CHECK(runProfile(rows[activeMod], {"BF2"}) == "" && hero->m_activeMod == bf2);

    CHECK(runProfile(rows[playerName], {"Pilot"}) == "" && runProfile(rows[playerName], {}) == "Pilot");
    CHECK(runProfile(rows[visualUnlock], {"2"}) == "" && runProfile(rows[visualUnlock], {}) == "2");
    CHECK(runProfile(rows[defaultKit], {"3"}) == "" && runProfile(rows[defaultKit], {}) == "3");
    CHECK(runProfile(rows[tagIcon], {"Icon"}) == "" && runProfile(rows[tagIcon], {}) == "Icon");
    CHECK(runProfile(rows[tagText], {"TT"}) == "" && runProfile(rows[tagText], {}) == "TT");
    CHECK(bf2->m_playerName == "Pilot" && bf2->m_defaultKit == 3 && xpack->m_playerName.empty());

    CHECK(runProfile(rows[setIndex], {"MEC", "4", "1"}) == "" && bf2->m_kitIndices["MEC"][1] == 4);
    CHECK(runProfile(rows[setIndex], {"MEC", "5", "1"}) == "" && bf2->m_kitIndices["MEC"][1] == 5);
    CHECK(runProfile(rows[getIndex], {"MEC", "1"}) == "" && !rows[getIndex]->hasReturnValue());
    CHECK(hero->getIndexForKit("MEC", 1) == 5 && hero->getIndexForKit("MEC", 2) == 0 && hero->getIndexForKit("US", 1) == 0);

    CHECK(runProfile(rows[addCharacter], {"Hero"}) == "0" && alpha->m_selectedCharacter == nullptr);
    CHECK(runProfile(rows[activeCharacter], {"Hero"}) == "" && alpha->m_selectedCharacter == hero);
    CHECK(runProfile(rows[addCharacter], {"Zulu"}) == "1" && alpha->m_selectedCharacter->m_id == 2);
    runProfile(rows[activeCharacter], {"Hero"});
    hero->setActiveMod("xpack");
    // Activation (applySettings itself is left to the live run): the first character, its BF2 setting.
    CHECK(runProfile(rows[activateProfile], {"Bravo"}) == "1" && owner->m_activeProfile->m_name == "Bravo");
    CHECK(owner->m_activeProfile->m_activeCharacter == nullptr && rows[activateCharacter]->isActive());
    CHECK(runProfile(rows[activateCharacter], {"Nobody"}) == "0");
    owner->m_activeProfile = alpha;
    alpha->m_characters.erase("Zulu");
    CHECK(rows[apply]->isActive() == false && rows[activateCharacter]->isActive());
    delete owner;
    CHECK(profileConsoleOwner == nullptr);
}

// Game/GameClient/GameClient.cpp:406/2486 - the client DistributedLightRenderer (BF2.exe 0x46B5B0..
// 0x46C690). Its collaborators are reached through vtables built from member-function pointers
// (Itanium: the pointer holds the slot offset + 1), so the large engine interfaces need no doubles.
namespace dlr {
using namespace dice::hfe;
template <class F> std::size_t slot(F f)
{
    std::ptrdiff_t raw[2];
    static_assert(sizeof(f) == sizeof(raw), "Itanium member pointer");
    std::memcpy(raw, &f, sizeof(raw));
    return static_cast<std::size_t>(raw[0] - 1) / sizeof(void*);
}
void unexpected() { std::printf("FAIL unexpected virtual call\n"); std::abort(); }
struct FakeTable {
    void* entries[256];
    FakeTable() { for (void*& e : entries) e = reinterpret_cast<void*>(&unexpected); }
    template <class F, class G> void set(F member, G function) { entries[slot(member)] = reinterpret_cast<void*>(function); }
};
struct Fake { void** vptr; };

// game: queryInterface and sendGameEvent.
bool serverMode = false;
int fakeServerToken;
std::vector<unsigned int> sentCodes;
void* gameQi(const void*, unsigned int iid) { return iid == IID_IGameServer && serverMode ? &fakeServerToken : nullptr; }
void gameSend(void*, unsigned int target, GameEvent* event)
{
    CHECK(target == 0xFFFFFFFFu);
    PostRemoteEvent* post = static_cast<PostRemoteEvent*>(event);
    CHECK(post->m_eventCategory == ECNetwork);
    sentCodes.push_back(post->m_eventId);
}

// classManager: the singletons by name.
IBase* writerSingleton = nullptr;
IBase* terrainSingleton = nullptr;
void* generationSingleton = nullptr;
IBase* cmGetSingleton(void*, const std::string& name)
{ return name == "DebugTextWriter" ? writerSingleton : name == "Terrain" ? terrainSingleton : nullptr; }
void* cmGetSimple(const void*, const std::string& name) { return name == "LightmapGeneration" ? generationSingleton : nullptr; }

// objectManager: the registry and the active object.
std::map<unsigned int, world::IObject*> registry;
world::IObject* activeObject = nullptr;
std::vector<world::IObject*> activeDuringTrace;
world::IObject* omGetRegistered(void*, int id) { std::map<unsigned int, world::IObject*>::iterator it = registry.find(id); return it == registry.end() ? nullptr : it->second; }
world::IObject* omGetActive(const void*) { return activeObject; }
void omSetActive(void*, world::IObject* object) { activeObject = object; }
const std::map<unsigned int, world::IObject*>& omAll(const void*) { return registry; }

// objects: flags, geometry, id, and the absolute position.
struct FakeObjectStorage { alignas(16) unsigned char bytes[sizeof(world::IObject)]; Vec3 position; };
const Vec3& objPosition(const void* self)
{ return reinterpret_cast<const FakeObjectStorage*>(self)->position; }
struct Sampleable : IBase {
    bool sampleable;
    explicit Sampleable(bool s) : sampleable(s) {}
    void addRef() override {} int getRef() override { return 1; } int release() override { return 1; }
    void* queryInterface(unsigned int iid) const override
    { return iid == geom::IID_ILightSampleable && sampleable ? const_cast<Sampleable*>(this) : nullptr; }
};

struct Generation : geom::ILightmapGeneration {
    bool result = true; int terrainPatch = -2; int activeTraces = 0;
    void addRef() override {} int getRef() override { return 1; } int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    bool traceAllObjects() override { return true; }
    bool traceActiveObject() override { ++activeTraces; activeDuringTrace.push_back(activeObject); return result; }
    bool traceObject(const geom::LightmapGenerationString&) override { return false; }
    bool traceTerrain(int patch) override { terrainPatch = patch; return result; }
    void setProgressCallback(geom::LightmapProgressCallback, void*) override {}
};

// The terrain singleton's table up to +0x30, and the editable interface it is asked for.
struct Terrain : IBase {
    int patches = 3; bool editable = true;
    void addRef() override {} int getRef() override { return 1; } int release() override { return 1; }
    void* queryInterface(unsigned int iid) const override
    { return iid == geom::IID_ITerrainEditable && editable ? const_cast<Terrain*>(this) : nullptr; }
    virtual unsigned int getReflectionTypeId() { return 0; }
    virtual void init() {}
    virtual void load(const std::string&) {}
    virtual void slot07() {}
    virtual void setRenderStyle(int) {}
    virtual int  getRenderStyle() { return 0; }
    virtual void update() {}
    virtual void slot11() {}
    virtual int  getPatchCount() { return patches; }
};

// The text writer's table up to +0x40 (queueText).
struct Writer : IBase {
    unsigned int colour = 0x11223344u; std::vector<std::pair<int, std::string> > lines; std::vector<unsigned int> colours;
    void addRef() override {} int getRef() override { return 1; } int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return nullptr; }
    virtual bool setFont(const std::string&) { return true; }
    virtual bool isEnabled() const { return true; }
    virtual void setEnabled(bool) {}
    virtual unsigned int getColour() const { return colour; }
    virtual void setColour(unsigned int c) { colour = c; colours.push_back(c); }
    virtual void q24() {} virtual void q28() {} virtual void q2C() {} virtual void q30() {}
    virtual void q34() {} virtual void q38() {} virtual void q3C() {}
    virtual void queueText(int x, int y, const std::string& text, int alignment)
    { CHECK(x == 20 && alignment == 0); lines.push_back(std::make_pair(y, text)); }
};

struct SentBlock { int resource; unsigned int type; DLRWorkItemBlock block; unsigned int size; bool onlyStarted; };
std::vector<SentBlock> sentBlocks;
} // namespace dlr

// The server half sends through GameServer::sendDataBlock (4C57B0); the fixture owns that definition.
void dice::hfe::GameServer::sendDataBlock(int connection, unsigned int type, const void* data, unsigned int size, bool onlyStarted)
{
    dlr::SentBlock sent;
    sent.resource = connection; sent.type = type; sent.size = size; sent.onlyStarted = onlyStarted;
    std::memcpy(&sent.block, data, sizeof(sent.block));
    dlr::sentBlocks.push_back(sent);
}

static void distributedLightRenderer()
{
    std::printf("-- DistributedLightRenderer\n");
    using namespace dlr;
    FakeTable gameTable, cmTable, omTable, objTable;
    gameTable.set(static_cast<void* (IGame::*)(unsigned int) const>(&IGame::queryInterface), &gameQi);
    gameTable.set(&IGame::sendGameEvent, &gameSend);
    cmTable.set(&ClassManager::getSingleton, &cmGetSingleton);
    cmTable.set(&ClassManager::getSimpleSingleton, &cmGetSimple);
    omTable.set(&world::ObjectManager::getRegisteredObject, &omGetRegistered);
    omTable.set(&world::ObjectManager::getActiveObject, &omGetActive);
    omTable.set(&world::ObjectManager::setActiveObject, &omSetActive);
    omTable.set(&world::ObjectManager::getAllRegisteredObjects, &omAll);
    objTable.set(&world::IObject::getAbsolutePosition, &objPosition);
    Fake fakeGame = { gameTable.entries }, fakeCm = { cmTable.entries }, fakeOm = { omTable.entries };
    game = reinterpret_cast<IGame*>(&fakeGame);
    classManager = reinterpret_cast<ClassManager*>(&fakeCm);
    world::objectManager = reinterpret_cast<world::ObjectManager*>(&fakeOm);

    // Factory and identity.
    IBase* created = DistributedLightRenderer::create(IID_IDistributedLightRenderer, nullptr);
    CHECK(created && created->queryInterface(IID_IBase) == created && !created->queryInterface(IID_IGameServer));
    CHECK(DistributedLightRenderer::create(IID_IGameServer, nullptr) == nullptr);
    CHECK(DistributedLightRenderer::create(IID_IBase, created) == nullptr);
    DistributedLightRenderer& r = *static_cast<DistributedLightRenderer*>(static_cast<IDistributedLightRenderer*>(created));
    CHECK(r.getRef() == 1 && r.m_jobs.empty() && r.m_resources.empty() && r.m_idleResources.empty());

    // Objects: alive and static with sampleable geometry within 10000 are queued; mobile, dead,
    // geometry-less, far and non-sampleable ones are not.
    Sampleable yes(true), no(false);
    FakeObjectStorage store[6];
    const unsigned int alive = world::IObject::OFIsAlive;
    struct Spec { unsigned int flags; IBase* geometry; float x; } specs[6] = {
        { alive, &yes, 100.0f }, { alive | world::IObject::OFHasMobilePhysics, &yes, 0.0f }, { 0, &yes, 0.0f },
        { alive, nullptr, 0.0f }, { alive, &yes, 10001.0f }, { alive, &no, 0.0f } };
    for (int i = 0; i < 6; ++i) {
        std::memset(store[i].bytes, 0, sizeof(store[i].bytes));
        world::IObject* object = reinterpret_cast<world::IObject*>(store[i].bytes);
        *reinterpret_cast<void***>(store[i].bytes) = objTable.entries;
        object->m_flags = specs[i].flags;
        object->m_geometry = specs[i].geometry;
        object->m_objectId = 40 + i;
        store[i].position.x = specs[i].x; store[i].position.y = 0.0f; store[i].position.z = 0.0f;
        registry[40 + i] = object;
    }
    world::IObject* first = registry[40];
    r.traceAllObjects();
    CHECK(r.m_jobs.size() == 1 && r.m_jobs[0]->object == first && r.m_jobs[0]->terrainPatch == -1 && r.m_jobs[0]->id == 0);

    // Terrain: one job per patch (3 x 3), numbered on from the object job.
    Terrain terrain;
    terrainSingleton = &terrain;
    r.traceTerrain();
    CHECK(r.m_jobs.size() == 10 && !r.m_jobs[1]->object && r.m_jobs[1]->terrainPatch == 0 && r.m_jobs[9]->terrainPatch == 8);
    CHECK(r.m_jobs[9]->id == 9 && r.m_nextJobId == 10);
    terrain.editable = false;
    CHECK(dies([&] { r.traceTerrain(); }));                    // DebugTypeError: a fatal assertion here
    terrainSingleton = nullptr;
    CHECK(dies([&] { r.traceTerrain(); }));

    // Resources: two clients; update hands each one job and sends block 4 on a server.
    serverMode = true;
    r.addRenderResource(7);
    r.addRenderResource(9);
    CHECK(r.m_resources.size() == 2 && r.m_idleResources.size() == 2);
    CHECK(dies([&] { r.addRenderResource(7); }));
    r.update();                                               // no DebugTextWriter: no overlay
    CHECK(r.m_jobs.size() == 8 && r.m_idleResources.empty() && sentBlocks.size() == 2);
    CHECK(sentBlocks[0].resource == 7 && sentBlocks[0].type == 4 && sentBlocks[0].size == sizeof(DLRWorkItemBlock) && sentBlocks[0].onlyStarted);
    CHECK(sentBlocks[0].block.id == 40 && !sentBlocks[0].block.isTerrain);
    CHECK(sentBlocks[1].resource == 9 && sentBlocks[1].block.id == 0 && sentBlocks[1].block.isTerrain);
    CHECK(r.m_resources[7] && r.m_resources[7]->object == first && r.m_resources[9]->terrainPatch == 0);

    // A finished job frees the client; a departing client's job returns to the queue's end.
    r.workItemFinished(7);
    CHECK(r.m_resources[7] == nullptr && r.m_idleResources.count(7) == 1);
    DLRWorkItem* pending = r.m_resources[9];
    r.removeRenderResource(9);
    CHECK(!r.m_resources.count(9) && r.m_jobs.size() == 9 && r.m_jobs.back() == pending);
    r.removeRenderResource(7);
    CHECK(r.m_resources.empty() && r.m_idleResources.empty());

    // The overlay: progress lines on a server, the node banner on a client; the colour is restored.
    Writer writer;
    writerSingleton = &writer;
    r.addRenderResource(3);
    r.update();
    CHECK(sentBlocks.size() == 3 && sentBlocks[2].resource == 3 && sentBlocks[2].block.id == 1);
    CHECK(writer.lines.size() == 4 && writer.lines[0].first == 500 && writer.lines[0].second == "Distributed lightmapGeneration in progress...");
    CHECK(writer.lines[1] == std::make_pair(510, std::string("Using 1 nodes")));
    CHECK(writer.lines[2] == std::make_pair(520, std::string("8 items enqued.")));
    CHECK(writer.lines[3] == std::make_pair(530, std::string("1 items in progress.")));
    CHECK(writer.colours.size() == 2 && writer.colours[0] == 0xFFFFFFFFu && writer.colour == 0x11223344u);
    serverMode = false;
    writer.lines.clear(); writer.colours.clear();
    r.update();
    CHECK(writer.lines.size() == 1 && writer.lines[0].second == "Distributed lightmapGeneration in progress...connected to server");
    CHECK(writer.colours.size() == 2 && writer.colours[0] == 0xAAFFAAFFu && writer.colour == 0x11223344u);
    writerSingleton = nullptr;

    // The node's half: an object job traces it as the active object (restored after), a terrain
    // job traces its patch; the result goes back as ECNetwork 0xE / 0xF.
    Generation generation;
    generationSingleton = &generation;
    world::IObject* before = registry[41];
    activeObject = before;
    DLRWorkItemBlock block;
    std::memset(&block, 0, sizeof(block));
    block.id = 40;
    r.performTraceObject(&block, sizeof(block));
    CHECK(generation.activeTraces == 1 && activeDuringTrace.back() == first && activeObject == before);
    CHECK(sentCodes == std::vector<unsigned int>(1, 0xEu));
    generation.result = false;
    block.isTerrain = true; block.id = 5;
    r.performTraceObject(&block, sizeof(block));
    CHECK(generation.terrainPatch == 5 && sentCodes.size() == 2 && sentCodes[1] == 0xFu);
    // A missing object: 0xF, then the trace still runs with no active object and reports again.
    generation.result = true;
    block.isTerrain = false; block.id = 99;
    r.performTraceObject(&block, sizeof(block));
    CHECK(sentCodes.size() == 4 && sentCodes[2] == 0xFu && sentCodes[3] == 0xEu && activeDuringTrace.back() == nullptr);
    CHECK(activeObject == before);
    generationSingleton = nullptr;
    CHECK(dies([&] { r.performTraceObject(&block, sizeof(block)); }));   // "LightmapGeneration not found"

    CHECK(r.release() == 0);
    game = nullptr; classManager = nullptr; world::objectManager = nullptr; registry.clear();
}

// AI/AIInterface/BFKnowledge/BFGame.cpp:106 - getAISettingsFromGame's menu arm, now compiled in both
// images (BF2.exe 0x815C50, bf2 0x83FED10 make the same calls). The dedicated image registers no
// "GeneralSettings", so the arm is driven here with fake singletons.
namespace bfgame {
using namespace dice::hfe;
IBase* generalSingleton = nullptr; IBase* profileSingleton = nullptr;
IBase* cmGet(void*, const std::string& name)
{ return name == "GeneralSettings" ? generalSingleton : name == "LocalProfileManager" ? profileSingleton : nullptr; }
int maxBots = 12; bool includeHumans = true; float skill = 0.75f; std::vector<int> setBots;
int gsGetMaxBots(const void*) { return maxBots; }
void gsSetMaxBots(void*, int value) { setBots.push_back(value); maxBots = value; }
bool gsGetInclude(const void*) { return includeHumans; }
float gsGetSkill(const void*) { return skill; }
LocalProfile* currentProfile = nullptr;
LocalProfile* lpCurrent(void*) { return currentProfile; }
std::string gameMode = "gpm_cq";
const std::string& stStringGet(void*, const std::string& key, const std::string& fallback)
{ CHECK(key == "GSGameMode" && fallback == "gpm_cq"); return gameMode; }
bool overrideMenu = false;
int aiMaxBots(const void*) { return 48; }
bool aiOverride(const void*) { return overrideMenu; }
}

static void bfGameMenuArm()
{
    std::printf("-- BFGame::getAISettingsFromGame menu arm\n");
    using namespace bfgame;
    dlr::FakeTable cm, gs, lp, st, ai;
    cm.set(&ClassManager::getSingleton, &cmGet);
    gs.set(&IGeneralSettings::getMaxBots, &gsGetMaxBots);
    gs.set(&IGeneralSettings::setMaxBots, &gsSetMaxBots);
    gs.set(&IGeneralSettings::getMaxBotsIncludeHumans, &gsGetInclude);
    gs.set(&IGeneralSettings::getBotSkill, &gsGetSkill);
    lp.set(&ILocalProfileManager::getCurrentProfile, &lpCurrent);
    st.set(&SettingsRepostitory::stringGet, &stStringGet);
    ai.set(&ai::IAISettings::getMaxNBots, &aiMaxBots);
    ai.set(&ai::IAISettings::overrideMenuSettings, &aiOverride);
    dlr::Fake fakeCm = { cm.entries }, fakeGs = { gs.entries }, fakeLp = { lp.entries }, fakeSt = { st.entries }, fakeAi = { ai.entries };
    classManager = reinterpret_cast<ClassManager*>(&fakeCm);
    settings = reinterpret_cast<SettingsRepostitory*>(&fakeSt);
    generalSingleton = reinterpret_cast<IBase*>(&fakeGs);

    int registered = 0;
    ai::CommonSingletonServer::m_iaiGame = &registered;   // another IAIGame is registered: no publication
    ai::BFGame game;
    int a = -1; bool b = false; float c = -1.0f;
    // No local profile manager, then no current profile: false, outputs untouched.
    CHECK(game.getAISettingsFromGame(a, b, c) == 0 && a == -1 && !b && c == -1.0f);
    profileSingleton = reinterpret_cast<IBase*>(&fakeLp);
    CHECK(game.getAISettingsFromGame(a, b, c) == 0 && a == -1);
    LocalProfile profile;
    currentProfile = &profile;
    // A non-sp1 mode reads the settings as they are.
    CHECK(game.getAISettingsFromGame(a, b, c) == 1 && a == 12 && b && c == 0.75f && setBots.empty());
    // sp1 (case-insensitive) forces 16 bots, or IAISettings' count when it overrides the menu.
    gameMode = "SP1";
    CHECK(game.getAISettingsFromGame(a, b, c) == 1 && a == 16 && setBots == std::vector<int>(1, 16));
    ai::CommonSingletonServer::m_iaiSettings = &fakeAi;
    CHECK(game.getAISettingsFromGame(a, b, c) == 1 && a == 16 && setBots.size() == 2 && setBots[1] == 16);
    overrideMenu = true;
    CHECK(game.getAISettingsFromGame(a, b, c) == 1 && a == 48 && setBots.back() == 48);
    ai::CommonSingletonServer::m_iaiSettings = nullptr;
    settings = nullptr; generalSingleton = nullptr; profileSingleton = nullptr; classManager = nullptr;
    ai::CommonSingletonServer::m_iaiGame = nullptr;
}

bool x87NarrowLongDoubleSelectsSoftExtended();   // game_frontier_x87_narrow.cpp

// Platform/X87Compat.h:42 - a long double narrower than x87 extended selects the soft Real.
static void x87NarrowLongDouble()
{
    std::printf("-- X87Compat on a binary64 long double\n");
    CHECK(x87NarrowLongDoubleSelectsSoftExtended());
}

int main()
{
    localProfileSlots();
    materialManagerTraps();
    x87NarrowLongDouble();
    nodeEvent16();
    listBoxEvent3();
    listBoxCellResource();
    memeTree();
    profileConsole();
    distributedLightRenderer();
    bfGameMenuArm();
    std::printf("game frontier checks %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
