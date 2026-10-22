// [TEMP] [CHOSEN, not recovered] Calls the actual BattleRecorder bodies of the SwiffPlayer TU - the
// "demos" and "demobm" lists of General.getListEntries and the registered Multiplay.playDemo /
// deleteDemo - against typed observers for the registry, the menu and player hosts, the demo
// library, the mod info and the profile manager. No movie, renderer, file or game is loaded.
#ifndef BF2_SWIFFPLAYER_SOURCE
#define BF2_SWIFFPLAYER_SOURCE "../BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp"
#endif
#include BF2_SWIFFPLAYER_SOURCE
#include "SwiffHostStandIn.h"
#include <vector>

namespace {
int checks = 0, failures = 0;
std::vector<std::string> calls;

void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}

__declspec(noreturn) void unexpected(const char* method)
{
    std::fprintf(stderr, "FAIL: unexpected fixture dependency %s\n", method);
    std::fflush(stderr);
    std::abort();
}

using dice::hfe::IBase;
using dice::hfe::IModInfo;
using dice::hfe::IDemoLibrary;
using dice::hfe::DemoLibraryEntry;
using dice::hfe::LocalDemoBookmark;
using dice::hfe::LocalProfile;

struct RegistryFixture : dice::hfe::IClassManager
{
    IBase* menuHost = nullptr;
    IBase* profiles = nullptr;
    IBase* getSingleton(const renddx9abi::HostString& name) override
    {
        if (std::strcmp(name.c_str(), "SwiffHost") == 0) { calls.push_back("lookup"); return menuHost; }
        if (std::strcmp(name.c_str(), "LocalProfileManager") == 0) return profiles;
        if (std::strcmp(name.c_str(), "GeneralSettings") == 0) return nullptr;
        if (std::strcmp(name.c_str(), "Bf2VoipClient") == 0) return nullptr;
        unexpected(name.c_str());
    }
#define SWIFF_TEST_UNEXPECTED(result, method, args, qualifier) result method args qualifier override { unexpected(#method); }
    SWIFF_TEST_UNEXPECTED(void, addRef, (), )
    SWIFF_TEST_UNEXPECTED(int, getRef, (), )
    SWIFF_TEST_UNEXPECTED(int, release, (), )
    SWIFF_TEST_UNEXPECTED(void*, queryInterface, (unsigned int), const)
    SWIFF_TEST_UNEXPECTED(void, reserved_vectorDeletingDestructor, (), )
    SWIFF_TEST_UNEXPECTED(void, registerServer, (dice::hfe::IClassServer*), )
    SWIFF_TEST_UNEXPECTED(void, unregisterServer, (dice::hfe::IClassServer*), )
    SWIFF_TEST_UNEXPECTED(void, reserved_setDefaultClass, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getDefaultClass, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getClassName, (), )
    SWIFF_TEST_UNEXPECTED(bool, getClassID, (const renddx9abi::HostString&, unsigned int*), )
    SWIFF_TEST_UNEXPECTED(void, reserved_createInstance_cid, (), )
    SWIFF_TEST_UNEXPECTED(IBase*, createInstance, (unsigned int, unsigned int, IBase*), )
    SWIFF_TEST_UNEXPECTED(void, reserved_createInstance_name, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_setSingletonClass, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getSingletonClass, (), )
    SWIFF_TEST_UNEXPECTED(void, registerSingleton, (const dice::hfe::SingletonRegInfo&), )
    SWIFF_TEST_UNEXPECTED(void, unregisterSingleton, (const renddx9abi::HostString&), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getSingletonAs, (), )
    SWIFF_TEST_UNEXPECTED(void, registerSimpleSingleton, (const renddx9abi::HostString&, void*), )
    SWIFF_TEST_UNEXPECTED(void, reserved_unregisterSimpleSingleton, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getSimpleSingleton, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getPrintableSingletonList, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_getAllClassIncludingSubstring, (), )
    SWIFF_TEST_UNEXPECTED(void, reserved_initSingletons, (), )
#undef SWIFF_TEST_UNEXPECTED
};

// The library rows the lists and buttons read; every other slot aborts.
struct LibraryFixture : IDemoLibrary
{
    std::vector<DemoLibraryEntry> rows;
    std::vector<unsigned int> removed;
    void addRef() override { unexpected("library addRef"); }
    int getRef() override { unexpected("library getRef"); }
    int release() override { unexpected("library release"); }
    void* queryInterface(unsigned int) const override { unexpected("library queryInterface"); }
    bool refresh() override { unexpected("refresh"); }
    bool download(const std::string&, const std::string&, const std::string&) override { unexpected("download"); }
    unsigned int getNumEntries() override { calls.push_back("count"); return static_cast<unsigned int>(rows.size()); }
    DemoLibraryEntry* getEntry(unsigned int index) override
    { calls.push_back("entry" + std::to_string(index)); return index < rows.size() ? &rows[index] : nullptr; }
    bool removeEntry(unsigned int index) override { removed.push_back(index); calls.push_back("remove"); return true; }
    void update() override { unexpected("update"); }
    bool isDownloading() override { unexpected("isDownloading"); }
    const std::string& getDownloadName() override { unexpected("getDownloadName"); }
    const std::string& getDownloadUrl() override { unexpected("getDownloadUrl"); }
    const std::string& getDownloadDescription() override { unexpected("getDownloadDescription"); }
    float getDownloadProgress() override { unexpected("getDownloadProgress"); }
    bool cancelDownload() override { unexpected("cancelDownload"); }
    bool getFlagA4() override { unexpected("getFlagA4"); }
    void setFlagA4(bool) override { unexpected("setFlagA4"); }
    void setPendingBookmark(int) override { unexpected("setPendingBookmark"); }
    bool hasDownloadFailed() override { unexpected("hasDownloadFailed"); }
};

// The current mod and its display names; every other slot aborts.
struct ModFixture : IModInfo
{
    Mod mod;
    void addRef() override { unexpected("mods addRef"); }
    int getRef() override { unexpected("mods getRef"); }
    int release() override { unexpected("mods release"); }
    void* queryInterface(unsigned int) const override { unexpected("mods queryInterface"); }
    bool init() override { unexpected("init"); }
    std::vector<Mod>* getModList() override { unexpected("getModList"); }
    const Map* getMap(const std::string&) const override { unexpected("getMap"); }
    const Mod* getCurrentMod() const override { calls.push_back("currentMod"); return &mod; }
    std::string getCurrentModPath() override { unexpected("getCurrentModPath"); }
    const Map* findMapFromCurrentMod(const std::string&) override { unexpected("findMapFromCurrentMod"); }
    const Map* findMapFromMod(const Mod*, const std::string&) override { unexpected("findMapFromMod"); }
    const Mod* getModFromName(const std::string&) override { unexpected("getModFromName"); }
    const Mod* getModFromPath(const std::string&) override { unexpected("getModFromPath"); }
    const Map* getMapFromGameSpyId(int) override { unexpected("getMapFromGameSpyId"); }
    std::string getMapNameFromMap(const std::string& map) override
    {
        calls.push_back("mapName");
        for (unsigned int i = 0; i < mod.maps.size(); ++i)
            if (mod.maps[i].path == map) return mod.maps[i].name;
        return std::string();
    }
    std::string getMapPathFromMap(const std::string&) override { unexpected("getMapPathFromMap"); }
    std::string getMapNameFromMap(const Mod*, const std::string&) override { unexpected("getMapNameFromMap(mod)"); }
    std::string getMapPathFromMap(const Mod*, const std::string&) override { unexpected("getMapPathFromMap(mod)"); }
    void addNameForGameMode(const std::string&, const std::string&) override { unexpected("addNameForGameMode"); }
    std::string getNameFromGameMode(const std::string&) override { unexpected("getNameFromGameMode"); }
};

// The profile manager's bookmarks; every other slot aborts.
struct ProfileFixture : dice::hfe::ILocalProfileManager
{
    std::vector<LocalDemoBookmark> marks;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool hasDefaultProfile() override { std::abort(); }
    bool createDefaultProfile() override { std::abort(); }
    bool init() override { std::abort(); }
    bool login(int,const std::string&,const std::string&) override { std::abort(); }
    void shutdown() override { std::abort(); }
    bool createProfile(const std::string&,const std::string&) override { std::abort(); }
    bool deleteProfile(int) override { std::abort(); }
    bool createOnlineAccount(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    void frontierPrimary30() override { std::abort(); }
    void frontierPrimary34() override { std::abort(); }
    bool hasProfile(const std::string&) override { std::abort(); }
    bool loadCurrentProfile() override { std::abort(); }
    bool saveCurrentProfile() override { std::abort(); }
    LocalProfile* getCurrentProfile() override { std::abort(); }
    bool getLocalProfiles(std::vector<LocalProfile>&,bool) override { std::abort(); }
    std::string getProfilesPath() override { std::abort(); }
    bool findProfileDirectory(const std::string&,std::string&) override { std::abort(); }
    bool loadProfileFile(const std::string&) override { std::abort(); }
    bool applySettings(bool) override { std::abort(); }
    bool loginOnline(const std::string&,const std::string&,bool) override { std::abort(); }
    bool loginOnlineAlternate(const std::string&,const std::string&,bool) override { std::abort(); }
    void setName(const std::string&) override { std::abort(); }
    void setGamespyNick(const std::string&) override { std::abort(); }
    void setEmail(const std::string&) override { std::abort(); }
    void setPassword(const std::string&,bool) override { std::abort(); }
    void setNick(const std::string&) override { std::abort(); }
    void setNumTimesLoggedIn(int) override { std::abort(); }
    std::string getName() override { std::abort(); }
    std::string getGamespyNick() override { std::abort(); }
    std::string getEmail() override { std::abort(); }
    std::string getPassword() override { std::abort(); }
    std::string getNick() override { std::abort(); }
    int getNumTimesLoggedIn() override { std::abort(); }
    bool hasDefaultVoiceSettings() override { std::abort(); }
    bool loadVoiceSettings() override { std::abort(); }
    bool saveVoiceSettings() override { std::abort(); }
    bool readVoiceSettings(float&,float&,float&,bool&) override { std::abort(); }
    void addDemoBookmark(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    unsigned int getNumDemoBookmarks() override { calls.push_back("bookmarks"); return static_cast<unsigned int>(marks.size()); }
    LocalDemoBookmark* getDemoBookmark(unsigned int index) override
    { calls.push_back("bookmark" + std::to_string(index)); return &marks[index]; }
    bool removeDemoBookmark(unsigned int) override { std::abort(); }
    float getTotalPlayedTime() override { std::abort(); }
    void setTotalPlayedTime(float) override { std::abort(); }
    bool validateProfileName(const std::string&,std::string&) override { std::abort(); }
    bool quickLogin(const std::string&,const std::string&) override { std::abort(); }
    bool saveGlobalSettings() override { std::abort(); }
    bool validateProfileFiles(const std::string&) override { std::abort(); }
};

// The registry SwiffHost: only the busy cursor pair.
struct MenuHostFixture : SwiffHostStandIn
{
    void* getCursor(int kind) override { calls.push_back("menu.cursor" + std::to_string(kind)); return nullptr; }
    bool setBusy(int value) override { calls.push_back("menu.busy" + std::to_string(value)); return false; }
    void clearBusy() override { calls.push_back("menu.clear"); }
};

// The player's host: the storage string the list prologue reads, the library, the mod info and
// the event queue.
struct SessionHostFixture : SwiffHostStandIn
{
    IBase* library = nullptr;
    IBase* mods = nullptr;
    std::vector<MenuEvent> events;
    std::string getStorageString(const std::string&, const std::string& fallback) override { return fallback; }
    IBase* getDemoLibrary() override { calls.push_back("library"); return library; }
    IBase* getModInfo() override { calls.push_back("modInfo"); return mods; }
    int pushEvent(MenuEvent& event) override { calls.push_back("push"); events.push_back(event); return 0; }
};

struct PlayerFixture : dice::hfe::ui::SwiffPlayer
{
    dice::hfe::ui::ISwiffHost* getSwiffHost() override
    {
        calls.push_back("playerHost");
        return SwiffPlayer::getSwiffHost();
    }
};

DemoLibraryEntry entry(int version, const char* map, const char* date, const char* name, const char* path)
{
    DemoLibraryEntry e;
    e.version = version; e.serverName = "server"; e.mapName = map; e.dateTime = date; e.name = name; e.path = path;
    return e;
}

// A list answer: element 0 is {totalCount}, then one array per row (its 0 the row object).
gameswf::as_object* element(gameswf::as_object* arr, int i)
{
    gameswf::as_value v;
    char key[16]; std::sprintf(key, "%d", i);
    if (!arr || !arr->get_member(tu_stringi(key), &v)) return nullptr;
    gameswf::as_object_interface* o = v.to_object();
    return o ? o->cast_to_as_object() : nullptr;
}
std::string text(gameswf::as_object* o, const char* key)
{
    gameswf::as_value v;
    if (!o || !o->get_member(tu_stringi(key), &v)) return "<absent>";
    return v.to_string();
}
std::string column(gameswf::as_object* row, int i)
{
    char key[16]; std::sprintf(key, "%d", i);
    return text(row, key);
}

gameswf::as_object* callList(gameswf::as_value& result, const char* id, int start, int count)
{
    gameswf::as_environment env;
    env.push(gameswf::as_value(static_cast<double>(count)));
    env.push(gameswf::as_value(static_cast<double>(start)));
    env.push(gameswf::as_value(id));
    gameswf::fn_call fn(&result, nullptr, &env, 3, 2);
    dice::hfe::ui::as_general_getListEntries(fn);
    gameswf::as_object_interface* o = result.to_object();
    return o ? o->cast_to_as_object() : nullptr;
}
}

namespace dice { namespace hfe {
IClassManager* classManager = nullptr;
IBase* fileManager = nullptr;
// [TEMP] DemoLibrary.cpp is not linked here; its row constructor is this one-line body
// (Game/DemoRecorder/DemoLibrary.cpp: the five strings, version 0).
DemoLibraryEntry::DemoLibraryEntry() : version(0) {}
} }

// [TEMP] Only heap/diagnostic services are replaced; the list, button and busy bodies are production.
namespace renddx9abi {
void* engineAlloc(unsigned int size) { return std::malloc(size); }
void engineFree(void* value) { std::free(value); }
void* crtMalloc(size_t size) { return std::malloc(size); }
void crtFree(void* value) { std::free(value); }
void logf(const char* format, ...)
{
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::fputc('\n', stderr);
}
void logClose() {}
void frontier(const char* name, const void*, unsigned int) { unexpected(name); }
void trace(const char*, const void*, unsigned int) {}
}
void swiffCrashLogInstall() {} // No movie or process crash reporter is needed in this fixture.

int main()
{
    using namespace dice::hfe;
    using namespace dice::hfe::ui;
    static_assert(sizeof(void*) == 4, "Swiff host compatibility tests require x86");
    RegistryFixture registry;
    classManager = &registry;
    MenuHostFixture menu;
    SessionHostFixture session;
    LibraryFixture library;
    ModFixture mods;
    ProfileFixture profiles;
    registry.menuHost = &menu;
    registry.profiles = &profiles;
    session.library = static_cast<IBase*>(static_cast<IDemoLibrary*>(&library));
    session.mods = &mods;
    PlayerFixture player;
    player.setSwiffHost(&session);

    IModInfo::Map karkand; karkand.path = "strike_at_karkand"; karkand.name = "Strike At Karkand";
    IModInfo::Map dalian;  dalian.path = "dalian_plant";       dalian.name = "Dalian Plant";
    mods.mod.maps.push_back(karkand);
    mods.mod.maps.push_back(dalian);
    library.rows.push_back(entry(3153, "strike_at_karkand", "2026-10-12 23:05:16", "dog_karkand", "@HOME@/Profiles/Default/demos/dog_karkand.bf2demo"));
    library.rows.push_back(entry(3153, "gulf_of_oman", "2026-10-13 10:00", "oman", "@HOME@/Profiles/Default/demos/oman.bf2demo"));
    library.rows.push_back(entry(3000, "dalian_plant", "2026-10-14 11:00", "dalian_old", "@HOME@/Profiles/Default/demos/dalian_old.bf2demo"));
    library.rows.push_back(entry(3153, "STRIKE AT KARKAND", "2026-10-15 12:00", "karkand_caps", "@HOME@/Profiles/Default/demos/karkand_caps.bf2demo"));

    // 1. The demos list, whole window.
    {
        gameswf::as_value result;
        gameswf::as_object* arr = callList(result, "demos", 0, -1);
        check(arr != nullptr, "demos answers a list");
        check(text(element(arr, 0), "totalCount") == "3", "the uninstalled map's row is neither shown nor counted");
        gameswf::as_object* r1 = element(arr, 1);
        gameswf::as_object* r2 = element(arr, 2);
        gameswf::as_object* r3 = element(arr, 3);
        check(element(arr, 4) == nullptr, "three rows");
        check(text(element(r1, 0), "id") == "0" && text(element(r2, 0), "id") == "2" && text(element(r3, 0), "id") == "3",
              "rows keep their library index as id");
        check(column(r1, 1) == "2026-10-12 23:05:16" && column(r1, 2) == "dog_karkand" && column(r1, 3) == "Strike At Karkand",
              "columns are date, demo name and the map's display name");
        check(text(element(r1, 0), "disabled") == "<absent>" && text(element(r2, 0), "disabled") == "1",
              "a demo of another build than 3153 is listed but disabled");
        check(column(r3, 3) == "", "the map lookup matches the display name case-insensitively, the column keeps the raw name lookup");
    }
    // 2. A window holding only the uninstalled row: the rows around it still count, it does not.
    {
        gameswf::as_value result;
        gameswf::as_object* arr = callList(result, "demos", 1, 1);
        check(text(element(arr, 0), "totalCount") == "3" && element(arr, 1) == nullptr,
              "out-of-window rows count whatever their map; the in-window uninstalled row does not");
    }
    // 3. The bookmarks list.
    {
        LocalDemoBookmark a; a.field00 = "=DOG= server"; a.field1c = "strike_at_karkand"; a.field38 = "url"; a.field54 = "2026-10-12";
        LocalDemoBookmark b; b.field00 = "other"; b.field1c = "gulf_of_oman"; b.field38 = "url2"; b.field54 = "2026-10-13";
        profiles.marks.push_back(a);
        profiles.marks.push_back(b);
        gameswf::as_value result;
        gameswf::as_object* arr = callList(result, "demobm", 0, -1);
        gameswf::as_object* r1 = element(arr, 1);
        check(text(element(arr, 0), "totalCount") == "1" && element(arr, 2) == nullptr,
              "a bookmark whose map has no display name in the mod is skipped");
        check(column(r1, 1) == "2026-10-12" && column(r1, 2) == "=DOG= server" && column(r1, 3) == "Strike At Karkand",
              "bookmark columns are field54, field00 and the display name");
    }

    // 4. The buttons, through the registered Multiplay namespace.
    SwiffAsNamespace multiplay("dice.bf2.Multiplay");
    SwiffAsNamespace* namespaces[] = {&multiplay};
    const char* names[] = {"Multiplay"};
    registerAsMethods(nullptr, namespaces, names, 1);
    gameswf::as_value play, remove;
    const bool playFound = multiplay.get_member(tu_stringi("playDemo"), &play) && play.get_type() == gameswf::as_value::C_FUNCTION;
    const bool removeFound = multiplay.get_member(tu_stringi("deleteDemo"), &remove) && remove.get_type() == gameswf::as_value::C_FUNCTION;
    check(playFound && removeFound, "Multiplay registers playDemo and deleteDemo as native callbacks");
    if (!playFound || !removeFound) return 1;

    struct Case { double index; bool pushes; const char* name; };
    const Case cases[] = {
        { 2.0, true,  "an existing row is played" },
        { 2.9, true,  "a fractional index is truncated" },
        { 9.0, false, "a row past the end only closes the busy cursor" },
        { -1.0, false, "a negative index wraps to a missing row" },
    };
    for (const Case& c : cases)
    {
        session.events.clear();
        calls.clear();
        gameswf::as_environment env;
        env.push(gameswf::as_value(c.index));
        gameswf::as_value result("unchanged result");
        gameswf::fn_call fn(&result, &multiplay, &env, 1, 0);
        play.to_c_function()(fn);
        const std::string row = "entry" + std::to_string(static_cast<unsigned int>(static_cast<int>(c.index)));
        std::vector<std::string> expected = {"lookup", "menu.cursor1", "menu.busy1", "playerHost", "library", row};
        if (c.pushes) expected.push_back("push");
        expected.push_back("menu.cursor0");
        expected.push_back("menu.clear");
        check(calls == expected, c.name);
        check(c.pushes ? (session.events.size() == 1 && session.events[0].type == 12 && session.events[0].args.size() == 1
                          && session.events[0].args[0] == "@HOME@/Profiles/Default/demos/dalian_old.bf2demo")
                       : session.events.empty(),
              "the row's file path goes to the engine as menu event 12");
        check(result.get_type() == gameswf::as_value::STRING && std::strcmp(result.to_string(), "unchanged result") == 0,
              "playDemo leaves the AS result alone");
    }
    {
        calls.clear();
        library.removed.clear();
        gameswf::as_environment env;
        env.push(gameswf::as_value(1.0));
        gameswf::as_value result("unchanged result");
        gameswf::fn_call fn(&result, &multiplay, &env, 1, 0);
        remove.to_c_function()(fn);
        check(library.removed == std::vector<unsigned int>({1}), "deleteDemo removes the row through the library");
        check(calls == std::vector<std::string>({"playerHost", "library", "remove"}), "deleteDemo takes no busy cursor");
        check(result.get_type() == gameswf::as_value::STRING && std::strcmp(result.to_string(), "unchanged result") == 0,
              "deleteDemo leaves the AS result alone");
    }

    std::printf("Swiff BattleRecorder lists and buttons: %d/%d checks passed (ABI native=%d)\n",
        checks - failures, checks, BF2_PLUGIN_ABI_NATIVE);
    return failures ? 1 : 0;
}
