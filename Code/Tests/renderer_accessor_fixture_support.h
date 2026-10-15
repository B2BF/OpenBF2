// [TEMP] Scaffolding for x86 renderer fixtures, not recovered source. RendDX9Base.h's Renderer
// accessors (issue #7) map a retail x86 offset to the native field; these fixtures model the Renderer
// as retail x86 bytes behind g_Renderer, so each accessor here is the x86 identity mapping - the same
// address the raw reads used before the accessors existed. Include once, after g_Renderer is defined.
#ifndef TESTS_RENDERER_ACCESSOR_FIXTURE_SUPPORT_H
#define TESTS_RENDERER_ACCESSOR_FIXTURE_SUPPORT_H
#if !defined(_M_IX86)
#error "renderer_accessor_fixture_support.h models the retail x86 Renderer layout"
#endif
namespace dice { namespace hfe { namespace dx9rend {
inline unsigned char* fixtureRendererBytes() { return reinterpret_cast<unsigned char*>(g_Renderer); }
unsigned char& rendererRenderSwitch(unsigned int retailOffset) { return fixtureRendererBytes()[retailOffset]; }
unsigned char& rendererTailByte(unsigned int retailOffset) { return fixtureRendererBytes()[retailOffset]; }
float& rendererTailFloat(unsigned int retailOffset)
{
    return *reinterpret_cast<float*>(fixtureRendererBytes() + retailOffset);
}
double& rendererTailDouble(unsigned int retailOffset)
{
    return *reinterpret_cast<double*>(fixtureRendererBytes() + retailOffset);
}
unsigned int rendererRaPath() { return *reinterpret_cast<unsigned int*>(fixtureRendererBytes() + 0x6CC); }
void* rendererFrameTimer() { return *reinterpret_cast<void**>(fixtureRendererBytes() + 0x5DC); }
// Effect +0x08, the D3DX effect handle (RendDX9/Effect.cpp's accessor).
void* effectD3dxHandle(void* effect) { return *reinterpret_cast<void**>(static_cast<unsigned char*>(effect) + 8); }
} } }
#endif
