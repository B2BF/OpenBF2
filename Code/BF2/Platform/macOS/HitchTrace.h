// [TEMP] [CHOSEN, not recovered] macOS port addition, not retail: frame-hitch trace (measurement only,
// off unless BF2_HITCH_TRACE is set).
// BF2_HITCH_TRACE=<file> writes, as text lines:
//   F <frame> <t> <ms> <game> <6 mainLoop section ms> then <ms> <count> per category (Category order)
//       one per main-loop frame: its duration (from the end of the previous frame), whether a game
//       is running without a menu, and the main-thread time the frame spent in each category;
//   E <frame> <t> <category> <ms> <main> <detail>
//       one per timed event of at least 0.2 ms (main 0: on another thread);
//   R <frame> <t> <outcome> <ms> <key>
//       one per effect request (D3DXCreateEffect / D3DXCreateEffectCompiler);
//   B <t> <ms> <key>
//       one per finished background native effect build (any thread), from its submission;
//   N <frame> <t> <text>
//       a labelled moment (the input script's report lines).
// t is seconds since the trace started. Frames longer than BF2_HITCH_TRACE_MS (default 33) are also
// reported on stderr as "[HITCH] ...". Without the variable every call returns at once.
#pragma once
#if defined(__APPLE__)
#include <chrono>

namespace bf2hitch {

enum Category {
    FxRequest,      // effect request up to its entry: capture, cache lookup/read, skeleton, or a synchronous build
    FxWait,         // first use of a deferred effect: waiting for (or running) its native build
    ShaderCreate,   // device shader objects of an effect (MSL interface patches)
    MetalLibrary,   // newLibraryWithSource (MSL -> AIR)
    MetalPipeline,  // newRenderPipelineStateWithDescriptor
    MetalState,     // depth-stencil and sampler state objects
    TextureCreate,  // texture objects and uploads
    Capture,        // BF2_PRESENT_PNG frame captures (test tooling, not a game cost)
    GpuWait,        // waiting for submitted command buffers (also inside texture and capture times)
    Drawable,       // waiting for the layer's next drawable
    MetalLibraryReady,  // count only: a draw's first use of a library found it built
    MetalPipelineReady, // count only: a draw's first use of a pipeline state found it built
    CategoryCount
};

bool enabled();
double seconds();
// A timed event on the calling thread. Main-thread time is charged to the current frame.
void add(Category category, double ms, const char* detail = nullptr);
void request(const char* outcome, double ms, const char* key);
void build(double ms, const char* key);
void note(const char* text);
// BF2Engine::mainLoop: wall time of a section of the frame, then the end of the frame.
void section(int index, double ms);
void frame(bool inGame);

class Scope {
public:
    explicit Scope(Category category, const char* detail = nullptr)
        : category_(category), detail_(detail), armed_(enabled()) {
        if (armed_) start_ = std::chrono::steady_clock::now();
    }
    ~Scope() {
        if (armed_) add(category_, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count(), detail_);
    }
    void detail(const char* text) { detail_ = text; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    Category category_;
    const char* detail_;
    bool armed_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace bf2hitch
#endif
