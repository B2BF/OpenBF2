// [TEMP] [CHOSEN, not recovered] macOS port addition, not retail. See HitchTrace.h.
#if defined(__APPLE__)
#include "HitchTrace.h"
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <pthread.h>

namespace bf2hitch {
namespace {
const char* const kNames[CategoryCount] = {
    "fx-request", "fx-wait", "shader-create", "metal-library", "metal-pipeline", "metal-state", "texture", "capture",
    "gpu-wait", "drawable", "library-ready", "pipeline-ready"};
const int kSections = 6;

struct State {
    std::FILE* file = nullptr;
    double thresholdMs = 33.0;
    std::chrono::steady_clock::time_point start, frameStart;
    std::mutex mutex;                    // the file and the frame's detail text
    unsigned long frame = 0;
    double ms[CategoryCount] = {};       // main thread, current frame
    unsigned count[CategoryCount] = {};
    double sections[kSections] = {};
    std::string details;                 // the frame's main-thread events, for the stderr line
    double lastFlush = 0;
};
State* state()
{
    static State* s = [] () -> State* {
        const char* path = std::getenv("BF2_HITCH_TRACE");
        if (!path || !*path) return nullptr;
        std::FILE* f = std::fopen(path, "w");
        if (!f) { std::fprintf(stderr, "[HITCH] cannot write %s\n", path); return nullptr; }
        State* created = new State;      // never destroyed: worker threads may still report at exit
        created->file = f;
        if (const char* e = std::getenv("BF2_HITCH_TRACE_MS")) { const double v = std::atof(e); if (v > 0) created->thresholdMs = v; }
        created->start = created->frameStart = std::chrono::steady_clock::now();
        return created;
    }();
    return s;
}
double since(const State& s) { return std::chrono::duration<double>(std::chrono::steady_clock::now() - s.start).count(); }
// One line of text: control characters would break the line format.
std::string clean(const char* text, std::size_t limit)
{
    std::string out(text ? text : "");
    if (out.size() > limit) out.resize(limit);
    for (char& c : out) if (static_cast<unsigned char>(c) < 32) c = ' ';
    return out;
}
} // namespace

bool enabled() { return state() != nullptr; }
double seconds() { State* s = state(); return s ? since(*s) : 0.0; }

void add(Category category, double ms, const char* detail)
{
    State* s = state();
    if (!s || category < 0 || category >= CategoryCount) return;
    const bool main = pthread_main_np() != 0;
    std::lock_guard<std::mutex> lock(s->mutex);
    if (main) { s->ms[category] += ms; ++s->count[category]; }
    if (ms < 0.2) return;
    const std::string text = clean(detail, 160);
    std::fprintf(s->file, "E %lu %.4f %s %.3f %d %s\n", s->frame, since(*s), kNames[category], ms, main ? 1 : 0, text.c_str());
    if (main && ms >= 1.0 && s->details.size() < 600) {
        char head[64];
        std::snprintf(head, sizeof head, " %s %.1f", kNames[category], ms);
        s->details += head;
        if (!text.empty()) s->details += "(" + text.substr(0, 48) + ")";
    }
}

void request(const char* outcome, double ms, const char* key)
{
    State* s = state();
    if (!s) return;
    std::lock_guard<std::mutex> lock(s->mutex);
    std::fprintf(s->file, "R %lu %.4f %s %.3f %s\n", s->frame, since(*s), outcome, ms, key ? key : "-");
}

void build(double ms, const char* key)
{
    State* s = state();
    if (!s) return;
    std::lock_guard<std::mutex> lock(s->mutex);
    std::fprintf(s->file, "B %.4f %.3f %s\n", since(*s), ms, key ? key : "-");
}

void note(const char* text)
{
    State* s = state();
    if (!s) return;
    std::lock_guard<std::mutex> lock(s->mutex);
    std::fprintf(s->file, "N %lu %.4f %s\n", s->frame, since(*s), clean(text, 400).c_str());
    std::fflush(s->file);
}

void section(int index, double ms)
{
    State* s = state();
    if (s && index >= 0 && index < kSections) s->sections[index] += ms;
}

void frame(bool inGame)
{
    State* s = state();
    if (!s) return;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(s->mutex);
    const double ms = std::chrono::duration<double, std::milli>(now - s->frameStart).count();
    const double t = std::chrono::duration<double>(now - s->start).count();
    std::fprintf(s->file, "F %lu %.4f %.3f %d", s->frame, t, ms, inGame ? 1 : 0);
    for (double v : s->sections) std::fprintf(s->file, " %.2f", v);
    for (int c = 0; c < CategoryCount; ++c) std::fprintf(s->file, " %.2f %u", s->ms[c], s->count[c]);
    std::fputc('\n', s->file);
    if (ms >= s->thresholdMs) {
        std::fprintf(stderr, "[HITCH] frame=%lu t=%.2f %.1f ms game=%d | pre %.1f update %.1f script %.1f objects %.1f render %.1f tail %.1f |",
                     s->frame, t, ms, inGame ? 1 : 0, s->sections[0], s->sections[1], s->sections[2], s->sections[3],
                     s->sections[4], s->sections[5]);
        for (int c = 0; c < CategoryCount; ++c)
            if (s->count[c]) std::fprintf(stderr, " %s %.1f/%u", kNames[c], s->ms[c], s->count[c]);
        std::fprintf(stderr, " |%s\n", s->details.c_str());
    }
    if (t - s->lastFlush >= 1.0) { std::fflush(s->file); s->lastFlush = t; }
    for (int c = 0; c < CategoryCount; ++c) { s->ms[c] = 0; s->count[c] = 0; }
    for (double& v : s->sections) v = 0;
    s->details.clear();
    s->frameStart = now;
    ++s->frame;
}

} // namespace bf2hitch

// The input script's report lines (Platform/InputScript.cpp holds the weak default).
extern "C" void openbf2HitchNote(const char* text) { bf2hitch::note(text); }
#endif
