// [CHOSEN, not recovered] Metal shader builds ahead of the first draw. Included by MetalBridge.mm
// before DrawBridge.inc.
//
// A D3D9 driver compiles a shader when it is created, and D3D9 has no pipeline objects, so retail
// pays for an effect's shaders while the level loads. Metal compiles in two steps that this port
// used to run at the first draw, on the render thread: MSL to a library (newLibraryWithSource:
// about 10 ms for a vertex shader and 100 ms for a pixel shader that samples a texture on an Intel
// GPU with an empty system shader cache, nearly all of it a fixed cost per source) and the
// pipeline state of one vertex/fragment pair with its vertex layout, attachments and blend state
// (about 20 ms). The first shot of a weapon stalled for the sum over everything it drew for the
// first time.
//
//  - Libraries: every program is compiled on background threads from the moment its shader object
//    is created (level load). Programs with the same MSL share one library entry. The background
//    compiles several shaders as one source, each in a namespace of its own, which pays the fixed
//    cost once per batch (measured: 49 pixel shaders 5.4 s one by one, 0.4 s as one source). A
//    level creates about 2,900 programs (every effect's shaders and the vertex-fog variants of its
//    pixel shaders) of which a session draws a few hundred; D3D9 compiles them all at creation
//    too. Batches of 64 compile them in about 68 s of worker time that overlaps the load and keeps
//    it about as long as without the background work (batches of 16: 83 s, load 13 s longer).
//  - Pipelines: a pipeline state is described by a recipe (the two libraries by the hash of their
//    MSL, attachment formats, sample count, blend state, vertex attributes); equal recipes share one
//    state. A draw builds the state of a recipe it is the first to use.
//  - A draw that needs a library or state that is not ready builds it at once (or waits for the
//    thread that is building it). Exception (DrawBridge.inc draw): a colour-only blended draw
//    (particles, muzzle flashes, glows: blending on, no depth or stencil writes, no occlusion query)
//    is skipped instead, a worker builds what it needs, and it is drawn from the first frame after
//    that (~1-3 frames on an Intel GPU with an empty system shader cache).
//
// BF2_METAL_PREBUILD=0 turns the background work off (every build happens at the first draw);
// BF2_METAL_PREBUILD_THREADS sets the worker count, BF2_METAL_PREBUILD_BATCH the shaders per batch.
enum class BuildState { Queued, Building, Ready, Failed };

struct LibraryEntry {
    std::string hash;                     // 128 bits of SHA-256 over the MSL, hex
    std::string msl;                      // dropped once compiled
    bool vertex = false;
    bool variant = false;                 // a pixel shader variant (ProgramDesc::variant)
    std::atomic<unsigned> programs{0};    // live programs holding it
    std::atomic<bool> cancelled{false};   // its last program died: a background job leaves it alone
    BuildState state = BuildState::Queued; // guarded by Prebuild::mutex, like the fields below
    bool used = false;                    // a draw has taken it
    pthread_t builder = nullptr;          // while Building
    id<MTLLibrary> library;
    id<MTLFunction> function;
    std::string error;
};

// Everything a render pipeline state depends on, in a form that outlives the D3D objects and
// holds no shader text.
struct PipelineRecipe {
    std::string vs, fs;                   // library hashes; fs is empty without colour writes
    unsigned color = 0, depth = 0, samples = 1, writeMask = 0;
    bool blend = false;
    unsigned factors[6] = {};             // source/destination RGB, source/destination alpha, RGB/alpha operation
    struct Attribute { unsigned reg, format, offset, stream; };
    std::vector<Attribute> attributes;    // one per vertex shader input
    std::vector<std::pair<unsigned, unsigned>> strides; // stream, stride
    std::string text() const { // the cache key
        std::ostringstream o;
        o << vs << ' ' << (fs.empty() ? "-" : fs) << ' ' << color << ' ' << depth << ' ' << samples << ' '
          << writeMask << ' ' << (blend ? 1 : 0);
        if (blend) for (unsigned v : factors) o << ' ' << v;
        o << " A";
        for (const auto& a : attributes) o << ' ' << a.reg << ' ' << a.format << ' ' << a.offset << ' ' << a.stream;
        o << " S";
        for (const auto& s : strides) o << ' ' << s.first << ' ' << s.second;
        return o.str();
    }
};


// A pipeline state: built by the first draw that needs it, or by a worker for a draw that was
// skipped meanwhile (DrawBridge.inc).
struct PipelineEntry {
    PipelineRecipe recipe;
    std::shared_ptr<LibraryEntry> vs, fs;  // fs null without colour writes
    BuildState state = BuildState::Queued; // guarded by Prebuild::mutex, like the fields below
    pthread_t builder = nullptr;           // while Building
    bool queued = false;                   // in Prebuild::jobs
    id<MTLRenderPipelineState> object;
    std::string error;
    // The frame (Frame::presents) of the first draw skipped for it, 0 when none (render thread),
    // and the frame in which a worker finished it (guarded by Prebuild::mutex).
    unsigned long firstSkip = 0, readyFrame = 0;
};

struct Prebuild {
    id<MTLDevice> device;
    bool background = true;               // BF2_METAL_PREBUILD
    std::mutex mutex;
    std::condition_variable work, done;
    std::unordered_map<std::string, std::shared_ptr<LibraryEntry>> libraries;  // by hash
    std::unordered_map<std::string, std::shared_ptr<PipelineEntry>> pipelines; // by recipe text (the map: render thread only)
    std::deque<std::shared_ptr<PipelineEntry>> jobs;           // states of skipped draws, before any batch
    // Libraries join the open batch of their stream (vertex, pixel, pixel variants) in creation
    // order; a full batch, or one that got no new member for a while, is compiled as one source.
    struct Batch {
        std::vector<std::shared_ptr<LibraryEntry>> members;
        std::chrono::steady_clock::time_point last;
    };
    std::shared_ptr<Batch> open[3];
    std::deque<std::shared_ptr<Batch>> batches;                 // closed, waiting for a worker
    std::vector<std::thread> threads;
    unsigned threadLimit = 2, batchSize = 64;
    bool stop = false;
    unsigned waiting = 0;                 // the render thread waits for a build: no new batch starts
    // Counts for the summary line.
    unsigned libraryReady = 0, libraryWaited = 0, libraryNow = 0, libraryBackground = 0, batchCount = 0;
    unsigned pipelineShared = 0, pipelineNow = 0, pipelineWaited = 0, pipelineBackground = 0;
    unsigned skippedDraws = 0;
    std::vector<unsigned long> framesMissing;  // per state a skipped draw waited for: frames until it was built
    std::atomic<unsigned long> presents{0};    // Frame::presents, for the workers
    double backgroundMs = 0;
};

static std::string mslHash(const std::string& msl)
{
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(msl.data(), CC_LONG(msl.size()), digest);
    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (unsigned i = 0; i < 16; ++i) { out += hex[digest[i] >> 4]; out += hex[digest[i] & 15]; }
    return out;
}

static bool depthHasStencil(MTLPixelFormat f)
{
    return f == MTLPixelFormatDepth24Unorm_Stencil8 || f == MTLPixelFormatDepth32Float_Stencil8;
}

// MSL -> library. Runs on the render thread or a worker, without the mutex.
static void compileLibrary(id<MTLDevice> device, LibraryEntry& e)
{
    @autoreleasepool {
        NSError* error = nil;
        NSString* source = [[NSString alloc] initWithBytes:e.msl.data() length:e.msl.size() encoding:NSUTF8StringEncoding];
        // [CHOSEN, not recovered] D3D9 multipass rendering relies on position invariance: two
        // vertex shaders that compute the position with the same instructions give bit-identical
        // depth, so a later pass with D3DCMP_LESSEQUAL/EQUAL hits the depth the first pass wrote
        // (Hi_Terrain: ZFillLightmap writes depth, LowDiffuse/FullDetail test LESSEQUAL with
        // ZWRITE off). Metal's fast-math compiler may contract the position math differently per
        // shader, so the colour pass failed the depth test and the cleared target showed
        // through (black terrain). Vertex positions are [[invariant]]
        // (Shaders.inc patchMsl), which needs preserveInvariance.
        static MTLCompileOptions* const options = [] {
            MTLCompileOptions* o = [MTLCompileOptions new];
            o.preserveInvariance = !std::getenv("BF2_METAL_NO_INVARIANCE"); // [TEMP] c-shadows A/B
            return o;
        }();
        e.library = source ? [device newLibraryWithSource:source options:options error:&error] : nil;
        if (!e.library) {
            e.error = std::string("MSL compile failed: ") + (error ? error.localizedDescription.UTF8String : "invalid source");
            return;
        }
    }
}

// The pipeline state of a recipe.
static id<MTLRenderPipelineState> makePipeline(id<MTLDevice> device, const PipelineRecipe& r, id<MTLFunction> vertex,
                                               id<MTLFunction> fragment, std::string& error)
{
    @autoreleasepool {
        MTLVertexDescriptor* vd = [MTLVertexDescriptor vertexDescriptor];
        for (const auto& in : r.attributes) {
            auto* a = vd.attributes[in.reg];
            a.format = MTLVertexFormat(in.format); a.offset = in.offset; a.bufferIndex = in.stream + 1;
        }
        for (const auto& s : r.strides) {
            vd.layouts[s.first + 1].stride = s.second;
            vd.layouts[s.first + 1].stepFunction = MTLVertexStepFunctionPerVertex;
        }
        const bool writes = r.writeMask != 0;
        MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
        d.rasterSampleCount = r.samples;
        d.vertexFunction = vertex;
        d.fragmentFunction = writes ? fragment : nil;
        d.vertexDescriptor = vd;
        auto* a = d.colorAttachments[0];
        a.pixelFormat = MTLPixelFormat(r.color);
        a.writeMask = writes ? MTLColorWriteMask(r.writeMask) : MTLColorWriteMaskNone;
        a.blendingEnabled = writes && r.blend;
        if (a.blendingEnabled) {
            a.sourceRGBBlendFactor = MTLBlendFactor(r.factors[0]); a.destinationRGBBlendFactor = MTLBlendFactor(r.factors[1]);
            a.sourceAlphaBlendFactor = MTLBlendFactor(r.factors[2]); a.destinationAlphaBlendFactor = MTLBlendFactor(r.factors[3]);
            a.rgbBlendOperation = MTLBlendOperation(r.factors[4]); a.alphaBlendOperation = MTLBlendOperation(r.factors[5]);
        }
        const MTLPixelFormat depth = MTLPixelFormat(r.depth);
        d.depthAttachmentPixelFormat = depth;
        d.stencilAttachmentPixelFormat = depthHasStencil(depth) ? depth : MTLPixelFormatInvalid;
        NSError* err = nil;
        id<MTLRenderPipelineState> state = [device newRenderPipelineStateWithDescriptor:d error:&err];
        if (!state) error = std::string("render pipeline: ") + (err ? err.localizedDescription.UTF8String : "unknown error");
        return state;
    }
}

// Brings an entry to Ready or Failed: builds it when no thread has started it, waits when another
// thread is building it. The lock is held on entry and on return and released in between. The
// render thread (urgent) lends the building thread its priority and holds back new low-priority
// jobs while it waits.
template<class Entry, class Build>
static void settle(Prebuild* p, std::unique_lock<std::mutex>& lock, Entry& e, bool urgent, Build build)
{
    if (e.state == BuildState::Queued) {
        e.state = BuildState::Building; e.builder = pthread_self();
        build(lock);
        e.state = e.error.empty() ? BuildState::Ready : BuildState::Failed;
        p->done.notify_all();
    } else if (e.state == BuildState::Building) {
        pthread_override_t boost = urgent ? pthread_override_qos_class_start_np(e.builder, QOS_CLASS_USER_INTERACTIVE, 0) : nullptr;
        if (urgent) ++p->waiting;
        p->done.wait(lock, [&] { return e.state != BuildState::Building; });
        if (urgent) { --p->waiting; p->work.notify_all(); }
        if (boost) pthread_override_qos_class_end_np(boost);
    }
}
// The includes of a shader's MSL, and the rest of it. A batch hoists the includes and wraps the rest
// in a namespace, so that equal struct and function names of different shaders do not collide.
static void splitIncludes(const std::string& msl, std::vector<std::string>& includes, std::string& body)
{
    std::size_t at = 0;
    while (at < msl.size()) {
        std::size_t end = msl.find('\n', at);
        if (end == std::string::npos) end = msl.size(); else ++end;
        if (msl.compare(at, 8, "#include") == 0) {
            const std::string line = msl.substr(at, end - at);
            if (std::find(includes.begin(), includes.end(), line) == includes.end()) includes.push_back(line);
        } else body.append(msl, at, end - at);
        at = end;
    }
}
// Compiles shaders as one source, each in the namespace l<hash> and numbered from line 1 of a file
// "l<hash>": a shader's functions get the same name and source positions, and so the same compiled
// code and system shader cache entry for their pipeline states, whether it is compiled alone (at a
// draw) or anywhere in a batch with any other shaders (in the background). A shader compiled bare
// in one start, or at another line of another batch, missed the pipeline cache on the next start.
// False when the compiler refuses the source (each member keeps the error).
static bool compileGroup(id<MTLDevice> device, const std::vector<LibraryEntry*>& group)
{
    @autoreleasepool {
        std::vector<std::string> includes;
        std::string bodies;
        for (LibraryEntry* e : group) {
            bodies += "namespace l" + e->hash + " {\n#line 1 \"l" + e->hash + "\"\n";
            splitIncludes(e->msl, includes, bodies);
            bodies += "\n}\n";
        }
        LibraryEntry all;
        for (const auto& line : includes) { all.msl += line; if (all.msl.back() != '\n') all.msl += '\n'; }
        all.msl += bodies;
        compileLibrary(device, all);
        if (!all.library) { for (LibraryEntry* e : group) e->error = all.error; return false; }
        std::unordered_map<std::string, LibraryEntry*> byName;
        for (LibraryEntry* e : group) byName["l" + e->hash] = e;
        for (NSString* name in all.library.functionNames) {
            const std::string full = name.UTF8String;
            const auto it = byName.find(full.substr(0, full.find("::")));
            if (it == byName.end() || it->second->function) continue;
            it->second->library = all.library;
            it->second->function = [all.library newFunctionWithName:name];
        }
        for (LibraryEntry* e : group) if (!e->function) e->error = "MSL library has no entry point";
        return true;
    }
}
static void settleLibrary(Prebuild* p, std::unique_lock<std::mutex>& lock, LibraryEntry& e, bool urgent)
{
    settle(p, lock, e, urgent, [&](std::unique_lock<std::mutex>& held) {
        held.unlock();
        compileGroup(p->device, {&e});
        held.lock();
        e.msl.clear(); e.msl.shrink_to_fit();
    });
}
static void settlePipeline(Prebuild* p, std::unique_lock<std::mutex>& lock, PipelineEntry& e, bool urgent)
{
    settle(p, lock, e, urgent, [&](std::unique_lock<std::mutex>& held) {
        settleLibrary(p, held, *e.vs, urgent);
        if (e.fs) settleLibrary(p, held, *e.fs, urgent);
        if (e.vs->state != BuildState::Ready) { e.error = "vertex shader " + e.vs->error; return; }
        if (e.fs && e.fs->state != BuildState::Ready) { e.error = "pixel shader " + e.fs->error; return; }
        id<MTLFunction> vertex = e.vs->function, fragment = e.fs ? e.fs->function : nil;
        held.unlock();
        e.object = makePipeline(p->device, e.recipe, vertex, fragment, e.error);
        held.lock();
    });
}

// Compiles the batch's members that no thread has started. The lock is held on entry and on return.
static void buildBatch(Prebuild* p, std::unique_lock<std::mutex>& lock, Prebuild::Batch& batch)
{
    std::vector<std::shared_ptr<LibraryEntry>> todo;
    for (const auto& e : batch.members)
        if (e->state == BuildState::Queued && !e->cancelled) { e->state = BuildState::Building; e->builder = pthread_self(); todo.push_back(e); }
    if (todo.empty()) return;
    lock.unlock();
    const auto started = std::chrono::steady_clock::now();
    std::vector<LibraryEntry*> group;
    for (const auto& e : todo) group.push_back(e.get());
    const bool together = compileGroup(p->device, group);
    // A batch the compiler refused: each shader on its own (one of them is the faulty one).
    if (!together && group.size() > 1) for (LibraryEntry* e : group) { e->error.clear(); compileGroup(p->device, {e}); }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (bf2hitch::enabled()) {
        const std::string detail = "background batch of " + std::to_string(todo.size()) + (together || todo.size() == 1 ? "" : " (one by one)");
        bf2hitch::add(bf2hitch::MetalLibrary, ms, detail.c_str());
    }
    lock.lock();
    for (const auto& e : todo) {
        e->state = e->error.empty() ? BuildState::Ready : BuildState::Failed;
        e->msl.clear(); e->msl.shrink_to_fit();
    }
    p->libraryBackground += unsigned(todo.size()); ++p->batchCount; p->backgroundMs += ms;
    p->done.notify_all();
}
static void prebuildWorker(Prebuild* p)
{
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    pthread_setname_np("bf2 metal prebuild");
    std::unique_lock<std::mutex> lock(p->mutex);
    for (;;) {
        // Wakes up for work, and at intervals to close a batch that stopped growing.
        p->work.wait_for(lock, std::chrono::milliseconds(100), [&] { return p->stop || !p->jobs.empty() || (!p->batches.empty() && !p->waiting); });
        if (p->stop) return;
        if (!p->jobs.empty()) {
            const std::shared_ptr<PipelineEntry> job = p->jobs.front();
            p->jobs.pop_front();
            job->queued = false;
            if (job->state != BuildState::Queued) continue;
            // A draw is missing until this is built: ahead of the batches, at a higher priority.
            pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
            const auto started = std::chrono::steady_clock::now();
            settlePipeline(p, lock, *job, false);
            job->readyFrame = p->presents;
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            ++p->pipelineBackground; p->backgroundMs += ms;
            pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
            lock.unlock();
            bf2hitch::add(bf2hitch::MetalPipeline, ms, "background, for a skipped draw");
            lock.lock();
            continue;
        }
        if (p->waiting) continue;
        std::shared_ptr<Prebuild::Batch> batch;
        if (!p->batches.empty()) { batch = p->batches.front(); p->batches.pop_front(); }
        else {
            const auto now = std::chrono::steady_clock::now();
            for (auto& open : p->open)
                if (open && now - open->last > std::chrono::milliseconds(250)) { batch = open; open.reset(); break; }
        }
        if (batch) buildBatch(p, lock, *batch);
    }
}
static void startWorkers(Prebuild* p)
{
    while (p->threads.size() < p->threadLimit) p->threads.emplace_back(prebuildWorker, p);
}
// A new library joins its stream's open batch. The lock is held.
static void prebuildQueue(Prebuild* p, const std::shared_ptr<LibraryEntry>& e)
{
    if (!p->background) return;
    auto& open = p->open[e->vertex ? 0 : e->variant ? 2 : 1];
    if (!open) open = std::make_shared<Prebuild::Batch>();
    open->members.push_back(e);
    open->last = std::chrono::steady_clock::now();
    if (open->members.size() >= p->batchSize) { p->batches.push_back(open); open.reset(); p->work.notify_one(); }
    startWorkers(p);
}

static Prebuild* prebuild(Frame* f)
{
    if (f->prebuild) return f->prebuild;
    auto* p = new Prebuild;
    p->device = f->device;
    if (const char* e = std::getenv("BF2_METAL_PREBUILD")) p->background = *e != '0';
    const unsigned cores = std::max(2u, std::thread::hardware_concurrency());
    p->threadLimit = std::min(4u, std::max(1u, cores / 2 - 1));
    if (const char* e = std::getenv("BF2_METAL_PREBUILD_THREADS")) p->threadLimit = unsigned(std::max(1, std::atoi(e)));
    if (const char* e = std::getenv("BF2_METAL_PREBUILD_BATCH")) p->batchSize = unsigned(std::max(1, std::atoi(e)));
    f->prebuild = p;
    return p;
}
// BF2_FX_NATIVE_TRACE / BF2_HITCH_TRACE: the counts so far, on stderr.
static void prebuildSummary(Frame* f)
{
    Prebuild* p = f->prebuild;
    if (!p || !(std::getenv("BF2_FX_NATIVE_TRACE") || bf2hitch::enabled())) return;
    std::lock_guard<std::mutex> lock(p->mutex);
    std::size_t waitingLibraries = 0;
    for (const auto& e : p->libraries) if (e.second->state == BuildState::Queued || e.second->state == BuildState::Building) ++waitingLibraries;
    std::vector<unsigned long> missing = p->framesMissing;
    std::sort(missing.begin(), missing.end());
    std::fprintf(stderr, "[metal] prebuild: libraries %zu (ready at first draw %u, waited for %u, built at the draw %u; "
                 "background %u in %u batches, %zu not compiled), pipelines %zu (built at the draw %u, waited for %u, "
                 "shared %u, built for skipped draws %u; skipped draws %u, frames missing median %lu max %lu); "
                 "background build time %.1f s\n",
                 p->libraries.size(), p->libraryReady, p->libraryWaited, p->libraryNow, p->libraryBackground, p->batchCount,
                 waitingLibraries, p->pipelines.size(), p->pipelineNow, p->pipelineWaited, p->pipelineShared, p->pipelineBackground,
                 p->skippedDraws, missing.empty() ? 0ul : missing[missing.size() / 2], missing.empty() ? 0ul : missing.back(),
                 p->backgroundMs / 1000.0);
}
static void destroyPrebuild(Frame* f)
{
    Prebuild* p = f->prebuild;
    if (!p) return;
    {
        std::lock_guard<std::mutex> lock(p->mutex);
        p->stop = true;
        p->work.notify_all();
    }
    for (auto& t : p->threads) t.join();
    prebuildSummary(f);
    delete p;
    f->prebuild = nullptr;
}

// The library of a new program: shared by MSL and queued for a background compile. Render thread.
static std::shared_ptr<LibraryEntry> internLibrary(Frame* f, const ProgramDesc& desc)
{
    Prebuild* p = prebuild(f);
    const std::string hash = mslHash(desc.msl);
    std::lock_guard<std::mutex> lock(p->mutex);
    auto& slot = p->libraries[hash];
    if (slot) {
        ++slot->programs;
        if (slot->cancelled.exchange(false) && slot->state == BuildState::Queued) prebuildQueue(p, slot);
        return slot;
    }
    slot = std::make_shared<LibraryEntry>();
    slot->hash = hash; slot->msl = desc.msl; slot->vertex = desc.vertex; slot->variant = desc.variant; slot->programs = 1;
    const std::shared_ptr<LibraryEntry> entry = slot;
    prebuildQueue(p, entry);
    return entry;
}
// A program died. No background job compiles a library that no program holds any more (a draw
// cannot ask for it either).
static void releaseLibrary(const std::shared_ptr<LibraryEntry>& e)
{
    if (e && --e->programs == 0) e->cancelled = true;
}
