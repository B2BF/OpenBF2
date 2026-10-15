// [TEMP] [CHOSEN, not recovered] Calls the actual registered ActionScript method
// with separate registry and player hosts. No movie, renderer, socket or game is loaded.
// Allocation providers use this fixture's CRT with the native host ABI.
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

struct RegistryFixture : dice::hfe::IClassManager
{
    dice::hfe::IBase* host = nullptr;
    unsigned int hostLookups = 0;
    dice::hfe::IBase* getSingleton(const renddx9abi::HostString& name) override
    {
        if (std::strcmp(name.c_str(), "Bf2VoipClient") == 0) return nullptr;
        if (std::strcmp(name.c_str(), "SwiffHost") != 0) unexpected(name.c_str());
        ++hostLookups;
        calls.push_back("lookup");
        return host;
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
    SWIFF_TEST_UNEXPECTED(dice::hfe::IBase*, createInstance, (unsigned int, unsigned int, dice::hfe::IBase*), )
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

// Only scalar/no-argument slots are exercised, so this observer also supplies
// the attested x86 compatibility slots without crossing any STL boundary.
struct HostFixture : SwiffHostStandIn
{
    std::string name;
    bool answer = false;
    int busy = 0;
    unsigned int requests = 0;
    RegistryFixture* registry = nullptr;
    dice::hfe::IBase* replaceRegistryWith = nullptr;
    explicit HostFixture(const char* label) : name(label) {}
    void* getCursor(int kind) override
    { calls.push_back(name + ".cursor" + std::to_string(kind)); return nullptr; }
    bool setBusy(int value) override
    { calls.push_back(name + ".busy" + std::to_string(value)); busy = value; return false; }
    void clearBusy() override
    { calls.push_back(name + ".clear"); busy = 0; }
    bool requestShutdownGame() override
    {
        ++requests;
        calls.push_back(name + ".request");
        if (registry) registry->host = replaceRegistryWith;
        return answer;
    }
};

struct PlayerFixture : dice::hfe::ui::SwiffPlayer
{
    unsigned int hostReads = 0;
    dice::hfe::ui::ISwiffHost* getSwiffHost() override
    {
        ++hostReads;
        calls.push_back("playerHost");
        return SwiffPlayer::getSwiffHost();
    }
};
}

namespace dice { namespace hfe {
IClassManager* classManager = nullptr;
IBase* fileManager = nullptr;
} }

// [TEMP] Only heap/diagnostic services are replaced. The namespace registration,
// callback, busy helpers and SwiffPlayer host accessor are production bodies.
namespace renddx9abi {
void* engineAlloc(unsigned int size) { return std::malloc(size); }
void engineFree(void* value) { std::free(value); }
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
    PlayerFixture player;
    HostFixture menu("menu"), playerHost("session"), replacement("replacement");
    player.setSwiffHost(&playerHost);

    SwiffAsNamespace logic("dice.bf2.Logic");
    SwiffAsNamespace* namespaces[] = {&logic};
    const char* names[] = {"Logic"};
    registerAsMethods(nullptr, namespaces, names, 1);
    gameswf::as_value method;
    const bool found = logic.get_member(tu_stringi("disconnectGame"), &method);
    check(found && method.get_type() == gameswf::as_value::C_FUNCTION,
        "actual Logic namespace registers disconnectGame as a native callback");
    if (!found || method.get_type() != gameswf::as_value::C_FUNCTION) return 1;

    for (int present = 0; present != 2; ++present)
    for (int answer = 0; answer != 2; ++answer)
    for (int args = 0; args != 2; ++args)
    {
        registry.host = present ? &menu : nullptr;
        playerHost.answer = answer != 0;
        // A request can change the registry; busyClose must retain its original host.
        playerHost.registry = &registry;
        playerHost.replaceRegistryWith = &replacement;
        menu.busy = 0;
        const unsigned int beforeRequests = playerHost.requests;
        const unsigned int beforeReads = player.hostReads;
        const unsigned int beforeLookups = registry.hostLookups;
        calls.clear();
        gameswf::as_value result("unchanged result");
        // A null environment is intentional: this callback must never read arguments.
        gameswf::fn_call fn(&result, &logic, nullptr, args, 0);
        method.to_c_function()(fn);
        check(playerHost.requests == beforeRequests + 1 && menu.requests == 0 && replacement.requests == 0,
            "shutdown is requested exactly once from the distinct player host");
        check(player.hostReads == beforeReads + 1 && registry.hostLookups == beforeLookups + 1,
            "player host and optional registry host are each fetched once");
        check(result.get_type() == gameswf::as_value::STRING &&
              std::strcmp(result.to_string(), "unchanged result") == 0,
            "request return value and extra arguments do not replace the AS result");
        const std::vector<std::string> expected = present
            ? std::vector<std::string>{"lookup", "menu.cursor1", "menu.busy1", "playerHost",
                "session.request", "menu.cursor0", "menu.clear"}
            : std::vector<std::string>{"lookup", "playerHost", "session.request"};
        check(calls == expected, "busy bracket surrounds dispatch and closes the same retained registry host");
        check(menu.busy == 0 && replacement.busy == 0, "request leaves the optional menu host out of busy state");
    }
    std::printf("Swiff disconnect registration/dispatch: %d/%d checks passed (ABI native=%d)\n",
        checks - failures, checks, BF2_PLUGIN_ABI_NATIVE);
    return failures ? 1 : 0;
}
