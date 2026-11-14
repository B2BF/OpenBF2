// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: rend-deinit-reftrace] Device reference trace for
// the macOS Metal backend. With BF2_METAL_REFTRACE=1 every native object
// that holds IDirect3DDevice9 references (resources, shaders, declarations, queries, effects,
// meshes, render-to-surface helpers) records its kind and creation stack, and every direct device
// AddRef/Release outside those objects records its caller stack. report() prints, before the
// renderer's final device release, who still holds references. Without the variable each entry
// point returns after one cached test. Remove this header, its definitions in Device.cpp and the
// calls in Device.cpp, Resources.inc, Effect.cpp, D3DXMesh.cpp, D3DXTextures.cpp and
// RendDX9Init.cpp once the exit-time release has been clean on every gated exit path for one
// coordinator review.

#ifndef BF2_PLATFORM_GFX_METAL_REFTRACE_H
#define BF2_PLATFORM_GFX_METAL_REFTRACE_H

namespace bf2metal {
namespace reftrace {

typedef unsigned long (*RefsFn)(const void* object);

bool enabled();
// An object that holds device references while refs(object) is non-zero; `kind` is a static name.
void born(const void* object, const char* kind, RefsFn refs);
void died(const void* object);
// A device AddRef (+1) or Release (-1) made directly, not through a traced object's own count.
void deviceRef(int delta);
// Marks the device calls a traced object makes for its own count, so deviceRef skips them.
struct Owned
{
    Owned();
    ~Owned();
};
// Prints the live holders and the direct-reference stacks to stderr ([REFTRACE] lines).
void report(const void* device, unsigned long deviceRefs);

} // namespace reftrace
} // namespace bf2metal

// The renderer's entry: called with the device just before its final release.
void bf2metalRefTraceReport(void* device);

#endif // BF2_PLATFORM_GFX_METAL_REFTRACE_H
