// [TEMP] [CHOSEN, not recovered] Actual GeometryTemplateManager ctor, table, registry and lifecycle
// bodies. Only geometry-template callbacks, ResourceManager's ready/settings seams and the
// synchronous progress event receiver are modeled. No template parser or game process is run.
#include "../BF2/Geom/GeometryTemplateManager.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::geom;

namespace {
int checks, failures;
bool ready, keepTemplates;
int readyCalls, settingCalls, progressCalls, managerFrees;
void* watchedManagers[2] = {};
std::vector<std::string> events;
GeometryTemplateManager* receiver;
int progressCurrent;
bool progressBeforeActiveRelease, settingAfterActiveRelease;
bool erasedBeforeRelease, activeVisibleDuringRelease, usedDuringLoad;
void* expectedErased;
void* expectedActive;
int keepLogCalls;

void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}

void* templateTable[0x30 / 4] = {};
struct FixtureTemplate
{
    void** table;
    unsigned char used;
    unsigned char padding[3];
    int reserved;
    int refs;
    const char* label;
    bool loaded;
    bool loadResult;
    int loadedQueries, loads, adds, releases, names;

    FixtureTemplate(const char* name, bool initiallyLoaded, bool result)
        : table(templateTable), used(0), padding{}, reserved(0), refs(1), label(name),
          loaded(initiallyLoaded), loadResult(result), loadedQueries(0), loads(0),
          adds(0), releases(0), names(0) {}
    IGeometryTemplate* asTemplate() { return reinterpret_cast<IGeometryTemplate*>(this); }
};
static_assert(offsetof(FixtureTemplate, used) == 4, "fixture preserves GeometryTemplate use byte");

void __fastcall unusedCallback(void*, void*)
{
    std::fprintf(stderr, "[FRONTIER] geometry lifecycle fixture unused callback reached\n");
    std::fflush(stderr);
    std::abort();
}
void __fastcall addTemplateRef(void* value, void*)
{
    FixtureTemplate& object = *static_cast<FixtureTemplate*>(value);
    ++object.refs; ++object.adds;
}
int __fastcall templateRefs(void* value, void*)
{ return static_cast<FixtureTemplate*>(value)->refs; }
int __fastcall releaseTemplate(void* value, void*)
{
    FixtureTemplate& object = *static_cast<FixtureTemplate*>(value);
    events.push_back(std::string("release:") + object.label);
    if (value == expectedErased)
        erasedBeforeRelease = receiver->getTemplate(renddx9abi::HostString(object.label)) == 0;
    if (value == expectedActive)
        activeVisibleDuringRelease = receiver->getActive() == object.asTemplate();
    ++object.releases;
    return --object.refs;
}
void __fastcall setTemplateFixtureName(void* value, void*, const std::string& name)
{
    FixtureTemplate& object = *static_cast<FixtureTemplate*>(value);
    check(std::strcmp(name.c_str(), object.label) == 0, "real registry forwards each template name");
    ++object.names;
}
bool __fastcall templateLoaded(void* value, void*)
{
    FixtureTemplate& object = *static_cast<FixtureTemplate*>(value);
    events.push_back(std::string("isLoaded:") + object.label);
    ++object.loadedQueries;
    return object.loaded;
}
bool __fastcall loadTemplate(void* value, void*)
{
    FixtureTemplate& object = *static_cast<FixtureTemplate*>(value);
    events.push_back(std::string("load:") + object.label);
    usedDuringLoad = usedDuringLoad && object.used != 0 && receiver->getActive() == 0;
    object.used = 0xA5; // The manager must clear the use byte after the callback even on failure.
    ++object.loads;
    object.loaded = object.loadResult;
    return object.loadResult;
}
unsigned int __fastcall postProgress(
    void*, void*, EventCategory category, unsigned int id, void* payload, float delay)
{
    struct Progress { int current; float weight; const char* label; };
    const Progress& progress = *static_cast<Progress*>(payload);
    check(category == ECCore && id == 4 && delay == 0.0f,
          "progress is synchronous ECCore id4");
    check(progress.weight == 1.0f && std::strcmp(progress.label, "Geometries") == 0,
          "progress contains the shipped Geometries label and unit weight");
    progressCurrent = progress.current;
    progressBeforeActiveRelease = receiver->getActive() == expectedActive;
    ++progressCalls;
    events.push_back("progress");
    return 17;
}

void* eventTable[0x18 / 4] = {};
struct EventReceiver { void** table; } eventReceiver = {eventTable};

void observeFresh()
{
    events.clear();
    readyCalls = settingCalls = progressCalls = 0;
    progressBeforeActiveRelease = settingAfterActiveRelease = false;
    erasedBeforeRelease = activeVisibleDuringRelease = false;
    usedDuringLoad = true;
    expectedErased = expectedActive = 0;
}
}

namespace renddx9abi {
void* engineAlloc(unsigned int size) { return std::malloc(size); }
void engineFree(void* value)
{
    if (value && (value == watchedManagers[0] || value == watchedManagers[1])) ++managerFrees;
    std::free(value);
}
void trace(const char*, const void*, unsigned int) {}
void logf(const char* format, ...)
{
    if (std::strstr(format, "Do not keep geom templates"))
    { ++keepLogCalls; events.push_back("keepLog"); }
}
void frontier(const char* name, const void*, unsigned int)
{
    std::fprintf(stderr, "[FRONTIER] %s reached in geometry lifecycle fixture\n", name);
    std::fflush(stderr); std::abort();
}
unsigned int addr(const void* value) { return reinterpret_cast<unsigned int>(value); }
}
namespace dice { namespace hfe {
IClassManager* classManager = 0;
IBase* mainConsole = 0;
ConsoleObjects* mainConsoleObjects = 0;
EventManager* eventManager = reinterpret_cast<EventManager*>(&eventReceiver);
namespace geom { IBase* g_GeometryTemplateManager = 0; }
namespace dx9rend {
bool resourceManagerIsReady()
{ ++readyCalls; events.push_back("ready"); return ready; }
bool readLevelLoadBoolSetting(const char* key, bool fallback)
{
    check(std::strcmp(key, "GSKeepTemplates") == 0 && fallback == false,
          "load reads GSKeepTemplates with the shipped false default");
    settingAfterActiveRelease = receiver->getActive() == 0;
    ++settingCalls; events.push_back("setting"); return keepTemplates;
}
}
} }

int main()
{
    static_assert(sizeof(void*) == 4, "GTM lifecycle fixture is x86-only");
    for (unsigned int i = 0; i < sizeof(templateTable) / sizeof(templateTable[0]); ++i)
        templateTable[i] = reinterpret_cast<void*>(&unusedCallback);
    templateTable[0x00 / 4] = reinterpret_cast<void*>(&addTemplateRef);
    templateTable[0x04 / 4] = reinterpret_cast<void*>(&templateRefs);
    templateTable[0x08 / 4] = reinterpret_cast<void*>(&releaseTemplate);
    templateTable[0x14 / 4] = reinterpret_cast<void*>(&setTemplateFixtureName);
    templateTable[0x28 / 4] = reinterpret_cast<void*>(&templateLoaded);
    templateTable[0x2C / 4] = reinterpret_cast<void*>(&loadTemplate);
    eventTable[0x10 / 4] = reinterpret_cast<void*>(&postProgress);

    receiver = static_cast<GeometryTemplateManager*>(
        createGeometryTemplateManager(IID_IGeometryTemplateManager, 0));
    GeometryTemplateManager* published = static_cast<GeometryTemplateManager*>(
        createGeometryTemplateManager(IID_IGeometryTemplateManager, 0));
    watchedManagers[0] = receiver; watchedManagers[1] = published;
    g_GeometryTemplateManager = published;
    check(receiver && published && receiver->getRef() == 1 && published->getRef() == 1,
          "actual factories create independent managers with one reference");

    FixtureTemplate unused("a_unused", false, true), loaded("b_loaded", true, true);
    FixtureTemplate pending("c_pending", false, true), failed("d_failed", false, false);
    FixtureTemplate lateUnused("e_unused", false, true);
    FixtureTemplate publishedA("p_first", true, true), publishedB("p_second", false, true);
    FixtureTemplate* initial[] = {&unused, &loaded, &pending, &failed};
    for (FixtureTemplate* value : initial)
    {
        receiver->registerTemplate(value->asTemplate(), renddx9abi::HostString(value->label));
        value->used = 7;
    }
    published->registerTemplate(publishedA.asTemplate(), renddx9abi::HostString(publishedA.label));
    published->registerTemplate(publishedB.asTemplate(), renddx9abi::HostString(publishedB.label));
    publishedA.used = publishedB.used = 1;
    check(unused.refs == 2 && loaded.refs == 2 && pending.refs == 2 && failed.refs == 3,
          "registration retains map ownership plus the current active reference");

    observeFresh(); ready = false;
    check(receiver->getUsedTemplateCount() == 0, "count returns zero before readiness without walking templates");
    receiver->loadUsedTemplates();
    check(readyCalls == 2 && settingCalls == 0 && progressCalls == 0 &&
          receiver->getActive() == failed.asTemplate() && receiver->getTemplateRegistry()->size() == 4,
          "unready load preserves registry and active ownership before all further callbacks");
    receiver->resetTemplateUse();
    check(readyCalls == 2 && !unused.used && !loaded.used && !pending.used && !failed.used,
          "reset clears every nonzero use byte independently of readiness");
    check(receiver->getTemplateRegistry()->size() == 4 && unused.refs == 2 && failed.refs == 3,
          "reset changes neither registry nor references");

    loaded.used = pending.used = failed.used = 1;
    ready = true;
    check(receiver->getUsedTemplateCount() == 3 && loaded.loadedQueries == 0,
          "ready count includes loaded templates and only reads the use byte");
    observeFresh(); keepTemplates = false;
    expectedActive = &failed; expectedErased = &unused;
    receiver->loadUsedTemplates();
    const char* expected[] = {"ready", "ready", "progress", "release:d_failed", "setting",
        "release:a_unused", "isLoaded:b_loaded", "isLoaded:c_pending", "load:c_pending",
        "isLoaded:d_failed", "load:d_failed"};
    check(events == std::vector<std::string>(expected, expected + 11),
          "full callback sequence preserves readiness, progress, active release, erase and ordered loads");
    check(progressCurrent == 1002 && publishedA.used && publishedB.used,
          "progress count is dispatched through the other published singleton, plus 1000");
    check(progressBeforeActiveRelease && activeVisibleDuringRelease && settingAfterActiveRelease,
          "active reference stays visible during release, after progress and before settings");
    check(erasedBeforeRelease && unused.refs == 1 &&
          receiver->getTemplate(renddx9abi::HostString("a_unused")) == 0,
          "unmarked entry is unlinked before its registry reference is released");
    check(receiver->getTemplateRegistry()->size() == 3 && receiver->getActive() == 0 && failed.refs == 2,
          "false keep setting erases only unmarked entries and drops active ownership");
    check(loaded.loadedQueries == 1 && loaded.loads == 0 && pending.loads == 1 && failed.loads == 1,
          "only marked unloaded templates load, including a failing loader");
    check(usedDuringLoad && !loaded.used && !pending.used && !failed.used &&
          pending.loaded && !failed.loaded,
          "every visited use byte clears after callbacks regardless of loaded state or load result");
    check(keepLogCalls == 0, "discarding unmarked templates does not emit the keep-branch log");

    receiver->registerTemplate(lateUnused.asTemplate(), renddx9abi::HostString(lateUnused.label));
    loaded.used = failed.used = 1;
    observeFresh(); keepTemplates = true; expectedActive = &lateUnused;
    receiver->loadUsedTemplates();
    const char* kept[] = {"ready", "ready", "progress", "release:e_unused", "setting", "keepLog",
        "isLoaded:b_loaded", "isLoaded:d_failed", "load:d_failed"};
    check(events == std::vector<std::string>(kept, kept + 9),
          "keep=true retains the retail counterintuitive log branch and skips unmarked callbacks");
    check(receiver->getTemplateRegistry()->size() == 4 && lateUnused.refs == 2 &&
          lateUnused.loadedQueries == 0 && lateUnused.loads == 0 && pending.loadedQueries == 1,
          "keep=true retains unmarked entries without querying or loading them");
    check(!loaded.used && !pending.used && !failed.used && !lateUnused.used && failed.loads == 2,
          "keep=true clears used bytes even when a repeated load fails");
    check(keepLogCalls == 1 && receiver->getUsedTemplateCount() == 0,
          "completed load leaves zero used templates and emits one keep-branch message");

    expectedActive = expectedErased = 0;
    receiver->clear(); published->clear();
    check(receiver->getTemplateRegistry()->size() == 0 && published->getTemplateRegistry()->size() == 0,
          "real clear empties both registries");
    check(unused.refs == 1 && loaded.refs == 1 && pending.refs == 1 && failed.refs == 1 &&
          lateUnused.refs == 1 && publishedA.refs == 1 && publishedB.refs == 1,
          "all registry and active references are released exactly to the fixture's external owners");
    check(receiver->release() == 0 && published->release() == 0 && managerFrees == 2,
          "real release-to-zero tears down and frees both managers");
    g_GeometryTemplateManager = 0;
    std::printf("geometry template lifecycle: %d checks, %d failures (ABI native=%d)\n",
                checks, failures, BF2_PLUGIN_ABI_NATIVE);
    return failures ? 1 : 0;
}
