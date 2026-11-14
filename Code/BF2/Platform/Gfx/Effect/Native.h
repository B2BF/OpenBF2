// [CHOSEN, not recovered] Native (Wine-free) effect compilation for the macOS client:
// vkd3d-shader (LGPL-2.1+, LICENSE.vkd3d) compiles each shader to D3D9 bytecode, the effect
// parser/builder (FxBuild.h) produces the Description, MojoShader translates to MSL.
// Build setup: docs/building.md (macOS).
#pragma once
#include "Cache.h"
#include "FxBuild.h"
#include <functional>
#include <future>
#include <string>

namespace bf2fx {
namespace native {

// Toolchain identity: vkd3d version and patches, MojoShader patches, native sources and the
// acceptance rules, fixed at build time. Requests captured for the native cache use
// dllHash = identityHash() and translator = identity().
const std::string& identity();
const std::string& identityHash();

// D3DX acceptance of an effect request, from the offline refusal table (keys of every attempt
// D3DX was measured on, independent of toolchains).
enum class Known { Accepted, Refused, Unknown };
std::string refusalKey(const Request& request);
Known known(const Request& request);
// Fallback for sources the table does not know (mods): Rule R refuses Ra attempts below
// ps_2_0 (PSMODEL ps_1_x), as D3DX does for most of them; everything else compiles.
bool ruleRefuses(const Request& request);

// Preprocesses the request with vkd3d-shader (macros, include snapshot).
bool preprocess(const Request& request, std::string& text, std::string& log);
// Structure only: parameters, techniques, passes, states and shader objects whose records
// carry no bytecode yet (the deferred compile fills them). False when the source does not
// build (the caller compiles synchronously to get D3DX-like errors).
bool skeleton(const std::string& text, Entry& out, std::string& error);
// Full compile: an EffectEntry, a RefusedEntry (compile failure, with the diagnostic) or a
// ReflectionOnlyEntry (no techniques). key and fx2 are filled.
Entry compile(const Request& request, const std::string* text = nullptr);

// The buffer ID3DXEffectCompiler::CompileEffect returns for a native entry: a tagged request
// envelope that D3DXCreateEffect (and Ra's .cfx disk cache) hands back.
Bytes tag(const Request& request);
bool untag(const void* data, size_t size, Request& request);

// Background compile queue (one worker per core but one). wait() runs a job inline when no
// worker has started it, so a pass that is drawn first never waits behind the queue.
using Done = std::function<void(const Entry&)>;
std::shared_future<Entry> submit(const std::string& key, const Request& request, std::string text, Done done);
Entry wait(const std::string& key, const std::shared_future<Entry>& future);
size_t pending();

// Shader-level memo (Native.cpp): the text a shader job of a D3DX-accepted request compiles,
// reduced to what `entry` reaches, whitespace collapsed. False when the text does not parse or
// has no such function (the job then compiles the full text). Counts are process totals.
bool trimSource(const std::string& text, const std::string& entry, std::string& out);
void memoCounts(unsigned& hits, unsigned& misses);
// One shader job's compile (profile ladder and MojoShader probe) of `source` (text + wrapper).
bool compileJob(const ShaderJob& job, const std::string& source, Bytes& bytecode, std::string& error);

} // namespace native
} // namespace bf2fx
