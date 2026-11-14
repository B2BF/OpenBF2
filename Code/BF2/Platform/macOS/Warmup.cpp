// [CHOSEN, not recovered] macOS port addition, not retail. See Warmup.h.
#include "Warmup.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace bf2warmup {
namespace {
std::string gLevel;
std::atomic<bool> gLoaded{false}, gJoined{false}, gWorld{false};
std::atomic<unsigned> gStarted{0}, gFinished{0}, gFailed{0};
std::chrono::steady_clock::time_point gStart;
double gLoadedAt = 0;
unsigned gLastStarted = ~0u, gLastFinished = ~0u;
unsigned gQuietStarted = ~0u;
std::chrono::steady_clock::time_point gQuietSince;
unsigned gFramesSinceLoad = 0;
std::chrono::steady_clock::time_point gLastPrint;
std::mutex gPrintMutex;
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count(); }
// "warmup: effects N/M" at most once a second, whenever N or M changed. Called from the frame
// (tick) and from every effect start/finish: the level load runs inside one frame and the native
// builds finish on worker threads, so the frame alone printed the counter only before and after it.
void progress() {
    std::unique_lock<std::mutex> lock(gPrintMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    const unsigned started = gStarted, finished = gFinished;
    const auto now = std::chrono::steady_clock::now();
    if ((started != gLastStarted || finished != gLastFinished) && now - gLastPrint >= std::chrono::seconds(1)) {
        std::printf("warmup: effects %u/%u\n", finished, started);
        std::fflush(stdout);
        gLastStarted = started; gLastFinished = finished; gLastPrint = now;
    }
}
// The cache entries are written as each build finishes; nothing else needs an orderly shutdown.
[[noreturn]] void finish(int code) {
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(code);
}
double limitSeconds() {
    if (const char* e = std::getenv("BF2_WARMUP_TIMEOUT")) { const double v = std::atof(e); if (v > 0) return v; }
    return 3600;   // a cold night map takes minutes on slow Macs; one hour stops a hung run
}
} // namespace

void configure(const std::string& level) {
    gLevel = level;
    gStart = std::chrono::steady_clock::now();
    gLastPrint = gStart;
    if (!gLevel.empty()) { std::printf("warmup: start %s\n", gLevel.c_str()); std::fflush(stdout); }
}
bool active() { return !gLevel.empty(); }
void profile(const std::string& name) {
    if (!active()) return;
    std::printf("warmup: profile %s\n", name.empty() ? "(none, defaults)" : name.c_str());
    std::fflush(stdout);
}
const std::string& level() { return gLevel; }
void levelLoaded() {
    if (!active() || gLoaded.exchange(true)) return;
    gLoadedAt = seconds();
    std::printf("warmup: level loaded %.1f s\n", gLoadedAt);
    std::fflush(stdout);
}
void localPlayerJoined() {
    if (!active() || gJoined.exchange(true)) return;
    std::printf("warmup: player joined %.1f s\n", seconds());
    std::fflush(stdout);
}
void worldFrame() {
    if (!active() || gWorld.exchange(true)) return;
    std::printf("warmup: world frame %.1f s\n", seconds());
    std::fflush(stdout);
}
bool wantsJoinGame() {
    static std::chrono::steady_clock::time_point last;
    static bool tried = false;
    if (!active() || !gLoaded || !gJoined || gWorld) return false;
    const auto now = std::chrono::steady_clock::now();
    if (tried && now - last < std::chrono::seconds(5)) return false;
    tried = true; last = now;
    std::printf("warmup: join game %.1f s\n", seconds());
    std::fflush(stdout);
    return true;
}
void effectStarted() { if (!active()) return; ++gStarted; progress(); }
void effectFinished(bool ok) { if (!active()) return; ++gFinished; if (!ok) ++gFailed; progress(); }
void menuEventDropped(int type) {
    if (!active()) return;
    std::printf("warmup: menu event %d dropped %.1f s\n", type, seconds());
    std::fflush(stdout);
}
void startResult(bool ok) {
    if (!active() || ok) return;
    std::printf("warmup: failed start %s\n", gLevel.c_str());
    finish(ExitStartFailed);
}
void tick() {
    if (!active()) return;
    progress();
    const unsigned started = gStarted, finished = gFinished;
    const auto now = std::chrono::steady_clock::now();
    // Effects made on the first world frames (shadows, sky, HUD, the player's kit) count too: finish
    // only once the level has loaded, the local player has joined and a world frame was drawn, then
    // after 10 s without a new build with every build finished.
    if (gLoaded) ++gFramesSinceLoad;
    if (started != gQuietStarted) { gQuietStarted = started; gQuietSince = now; }
    if (gLoaded && gJoined && gWorld && finished == started && now - gQuietSince >= std::chrono::seconds(10)) {
        const unsigned failed = gFailed;
        std::printf("warmup: done effects %u/%u failed %u %.1f s\n", finished, started, failed, seconds());
        finish(failed ? ExitBuildFailures : ExitOk);
    }
    if (seconds() > limitSeconds()) {
        std::printf("warmup: timeout effects %u/%u %.1f s\n", finished, started, seconds());
        finish(ExitTimeout);
    }
}

} // namespace bf2warmup
