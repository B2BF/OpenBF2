// [TEMP] [CHOSEN, not recovered] Offline fixture for the recovered DemoLibrary HTTP side on the
// production HTTPRequest base. The GameSpy ghttp entry points are a
// link-time seam modelled on http_request_offline.cpp: no sockets, DNS or file writes. The class
// and file managers are observers derived from the real classes.
#include "../../BF2/Game/DemoRecorder/DemoLibrary.h"
#include "../../BF2/Game/Persistence/HTTPRequest.h"
#include "../../BF2/External/GameSpy/ghttp/ghttp.h"
#include "../../BF2/Core/ClassManager.h"
#include "../../BF2/IO/File/FileManager.h"
#include "../../BF2/Core/LoadStat.h"
#include "../SwiffHostStandIn.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0, startups = 0, thinks = 0, cancels = 0, saves = 0;
ghttpCompletedCallback completion = nullptr;
ghttpProgressCallback progressCallback = nullptr;
void* callbackData = nullptr;
GHTTPResult nextResult = GHTTPHostLookupFailed;
const int requestHandle = 41;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
[[noreturn]] void unexpected() { std::abort(); }
}

extern "C" {
void ghttpStartup() { ++startups; }
void ghttpCleanup() { --startups; }
GHTTPRequest ghttpSaveExA(const char*, const char*, const char*, GHTTPPost, GHTTPBool, GHTTPBool,
    ghttpProgressCallback progress, ghttpCompletedCallback callback, void* data)
{
    ++saves;
    completion = callback;
    progressCallback = progress;
    callbackData = data;
    return requestHandle;
}
GHTTPBool ghttpRequestThink(GHTTPRequest handle)
{
    ++thinks;
    check(handle == requestHandle, "update drives only the accepted request");
    if (completion) {
        ghttpCompletedCallback callback = completion;
        completion = nullptr;
        callback(handle, nextResult, nullptr, 0, callbackData);
    }
    return GHTTPTrue;
}
void ghttpCancelRequest(GHTTPRequest handle)
{
    check(handle == requestHandle, "cancel receives the accepted handle");
    ++cancels;
    completion = nullptr;
}
GHTTPBool ghttpSetProxy(const char*) { unexpected(); }
GHTTPPost ghttpNewPost() { unexpected(); }
GHTTPBool ghttpPostAddStringA(GHTTPPost, const char*, const char*) { unexpected(); }
GHTTPRequest ghttpGetA(const char*, GHTTPBool, ghttpCompletedCallback, void*) { unexpected(); }
GHTTPRequest ghttpPostA(const char*, GHTTPPost, GHTTPBool, ghttpCompletedCallback, void*) { unexpected(); }
}

namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
LoadStat* loadStat = nullptr;   // FileManager's load counters; never reached here
namespace io { FileManager* fileManager = nullptr; }
struct DemoLibraryFixture {
    static void setPath(DemoLibrary* d, const std::string& p) { d->m_downloadPath = p; }
    static int pending(DemoLibrary* d) { return d->m_pendingBookmark; }
};
} }
using namespace dice::hfe;

namespace {
struct Host : SwiffHostStandIn {
    std::vector<std::string> messages;
    bool postMessage(const SwiffHostMessage& m) override
    { check(m.type == 1 && m.code == 0, "reportError posts {1, text, 0}"); messages.push_back(m.text); return true; }
};
Host host;
struct Registry : ClassManager {
    std::vector<std::string> queries;
    IBase* getSingleton(const std::string& name) override
    {
        queries.push_back(name);
        if (name == "SwiffHost") return &host;
        return nullptr;               // no LocalProfileManager: the rescan stops at its first test
    }
};
struct Files : io::FileManager {
    std::vector<std::string> deleted;
    bool deleteFile(const std::string& path) override { deleted.push_back(path); return true; }
};
}

int main()
{
    Registry registry; classManager = &registry;
    Files& files = *new Files; io::fileManager = &files;
    {
        DemoLibrary* library = new DemoLibrary;
        IDemoLibrary* demos = library;
        check(startups == 1, "construction runs the HTTPRequest base (ghttpStartup once)");
        check(!demos->isDownloading() && !demos->hasDownloadFailed(), "a new library is idle");

        // The menu frame: SwiffHost::update calls +24 every frame. Idle, no SDK work and no logs.
        for (int frame = 0; frame < 600; ++frame)
            demos->update();
        check(thinks == 0 && saves == 0, "600 idle menu frames drive no request");
        check(!demos->cancelDownload() && files.deleted.empty() && cancels == 0,
              "idle cancel answers false and deletes nothing");

        // A transfer that fails offline (host lookup) takes the shipped failure path once.
        DemoLibraryFixture::setPath(library, "@HOME@/x.bf2demo");
        HTTPRequest* base = library;
        check(base->getFile("http://offline.invalid/d.bf2demo", "@HOME@/x.bf2demo", true), "transfer accepted");
        check(demos->isDownloading(), "a pending transfer is reported");
        demos->update();
        check(thinks == 1 && !demos->isDownloading(), "the failure clears the pending transfer");
        check(demos->hasDownloadFailed(), "the failure callback raises the failed flag");
        check(host.messages.size() == 1 && host.messages[0] == "<Failed looking up the hostname>",
              "the non-empty failure text is posted to the SwiffHost");
        check(files.deleted.size() == 1 && files.deleted[0] == "@HOME@/x.bf2demo",
              "the partial target is deleted");
        for (int frame = 0; frame < 600; ++frame)
            demos->update();
        // 59C420 keeps thinking the finished handle until a cancel resets it (the callbacks do not
        // clear +8); the SDK answers a finished handle without a second callback.
        check(thinks == 601 && host.messages.size() == 1 && files.deleted.size() == 1 && !demos->isDownloading(),
              "later frames do not repeat the failure");

        // Cancel while busy: the base's cancel, then the target deletion, then true.
        check(base->getFile("http://offline.invalid/d.bf2demo", "@HOME@/x.bf2demo", true), "second transfer");
        check(demos->cancelDownload() && cancels >= 1 && files.deleted.size() == 2 && !demos->isDownloading(),
              "busy cancel cancels, deletes the target and answers true");

        // Completion: the failed flag clears, no bookmark is pending, the rescan runs.
        nextResult = GHTTPSuccess;
        check(base->getFile("http://offline.invalid/d.bf2demo", "@HOME@/x.bf2demo", true), "third transfer");
        progressCallback(requestHandle, GHTTPReceivingFile, nullptr, 0, 25, 100, callbackData);
        check(demos->getDownloadProgress() == 0.25f, "progress is stored");
        const size_t before = registry.queries.size();
        demos->update();
        check(!demos->hasDownloadFailed() && !demos->isDownloading(), "completion clears the flags");
        check(registry.queries.size() == before + 1 && registry.queries.back() == "LocalProfileManager" &&
              DemoLibraryFixture::pending(library) == -1, "completion with no bookmark goes straight to the rescan");
        library->release();
    }
    check(startups == 0, "destruction balances ghttpStartup");
    std::printf("DemoLibrary offline: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
