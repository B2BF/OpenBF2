// [TEMP] [CHOSEN, not recovered] Shared collaborators for whole-source particle fixtures.
// These observe constructor requests and GPU submissions. No real device or pixels are produced.
#ifndef PARTICLE_RENDERER_FIXTURE_SUPPORT_H
#define PARTICLE_RENDERER_FIXTURE_SUPPORT_H
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

namespace particle_renderer_fixture {
bool allowConstruction = false;
bool failDeclaration = false;
std::vector<std::string> constructionEvents;
std::vector<std::vector<D3DVERTEXELEMENT9>> declarations;
std::vector<dice::hfe::dx9rend::DeviceLostResetListener*> listeners;
std::vector<int> listenerPriorities;
bool allowGpu = false;
unsigned int gpuStride = 64;
unsigned int failGpuLockAt = 0;
struct GpuLock { unsigned int offset, count, flags; };
struct GpuDraw { unsigned int count, type, offset; std::vector<unsigned char> bytes; };
struct GpuIndexedDraw {
    unsigned int count, type; int base; unsigned int minimum, vertices, start;
    std::vector<unsigned char> bytes;
};
std::vector<GpuLock> gpuLocks;
std::vector<GpuDraw> gpuDraws;
std::vector<GpuIndexedDraw> gpuIndexedDraws;
std::vector<void*> gpuIndexBuffers;
std::vector<std::pair<unsigned int, unsigned int>> gpuStates;
std::vector<unsigned char> gpuBytes;
std::vector<std::string> gpuEvents;
void requireGpu() { if (!allowGpu) std::abort(); }
void requireConstruction() { if (!allowConstruction) std::abort(); }
}
namespace dice { namespace hfe { namespace geom { IHemiMapManager* g_HemiMapManager = 0; } } }
namespace dice { namespace hfe { namespace dx9rend {
// These fixtures never unload a level or release device buffers. Stop if that scope changes.
void textureAtlasReleaseAll(TextureAtlas*) { std::abort(); }
void vertexBufferRelease(void*) { std::abort(); }
void indexBufferRelease(void*) { std::abort(); }
void* g_rendererBase = 0;
void* g_raShaderConstants = 0;
// The x86 mapping of RenderQueues.cpp's accessor: the retail offset into the constants block.
unsigned char* raShaderConstantsAt(unsigned int retailByteOffset)
{
    return static_cast<unsigned char*>(g_raShaderConstants) + retailByteOffset;
}
void* g_shaderManager = reinterpret_cast<void*>(0x51515151u);
void* vertexDeclarationCreate(const void* input)
{
    using namespace particle_renderer_fixture;
    requireConstruction();
    constructionEvents.push_back("declaration");
    const D3DVERTEXELEMENT9* item = static_cast<const D3DVERTEXELEMENT9*>(input);
    std::vector<D3DVERTEXELEMENT9> copy;
    for (unsigned int i = 0; i < 32; ++i) {
        copy.push_back(item[i]);
        if (item[i].Stream == 0xFF) break;
        if (i == 31) std::abort();
    }
    declarations.push_back(copy);
    return failDeclaration ? 0 : reinterpret_cast<void*>(0x61000000u + 0x100u * declarations.size());
}
void* shaderManagerGetEffect(void* manager, const char* path)
{
    using namespace particle_renderer_fixture;
    requireConstruction(); if (manager != g_shaderManager) std::abort();
    constructionEvents.push_back(std::string("effect:") + path);
    return reinterpret_cast<void*>(0x62000000u + 0x100u * constructionEvents.size());
}
ShaderParameter* effectGetShaderParameterTyped(Effect* effect, const std::string& name)
{
    using namespace particle_renderer_fixture;
    requireConstruction(); if (!effect) std::abort();
    constructionEvents.push_back(std::string("parameter:") + name.c_str());
    return reinterpret_cast<ShaderParameter*>(0x63000000u + 0x100u * constructionEvents.size());
}
void registerDeviceLostResetListener(DeviceLostResetListener* listener, int priority)
{
    using namespace particle_renderer_fixture;
    requireConstruction(); constructionEvents.push_back("listener");
    listeners.push_back(listener); listenerPriorities.push_back(priority);
}
void stateManagerPush() { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("push"); }
void stateManagerPop() { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("pop"); }
void stateManagerSetRenderState(unsigned int state, unsigned int value)
{
    particle_renderer_fixture::requireGpu();
    particle_renderer_fixture::gpuStates.push_back(std::make_pair(state,value));
}
bool effectSetTechniqueByName(void*, const std::string& name)
{
    particle_renderer_fixture::requireGpu();
    particle_renderer_fixture::gpuEvents.push_back(std::string("technique:")+name.c_str()); return true;
}
unsigned int effectBegin(void*) { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("begin"); return 1; }
void effectBeginPass(void*, unsigned int pass)
{ particle_renderer_fixture::requireGpu(); if (pass) std::abort(); particle_renderer_fixture::gpuEvents.push_back("pass"); }
void effectEnd(void*) { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("end"); }
void* vertexBufferLock(void*, unsigned int offset, unsigned int count, unsigned int flags)
{
    using namespace particle_renderer_fixture;
    requireGpu(); gpuLocks.push_back({offset,count,flags}); gpuEvents.push_back("lock");
    if (failGpuLockAt && gpuLocks.size() == failGpuLockAt) return 0;
    gpuBytes.assign(count*gpuStride,0xA5); return gpuBytes.data();
}
bool vertexBufferUnlock(void*) { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("unlock"); return true; }
void vertexBufferBindStream(void*, unsigned int stream)
{ particle_renderer_fixture::requireGpu(); if(stream) std::abort(); particle_renderer_fixture::gpuEvents.push_back("stream"); }
void vertexDeclarationBind(void*) { particle_renderer_fixture::requireGpu(); particle_renderer_fixture::gpuEvents.push_back("declaration"); }
void indexBufferBind(void* buffer)
{
    particle_renderer_fixture::requireGpu();
    particle_renderer_fixture::gpuIndexBuffers.push_back(buffer);
    particle_renderer_fixture::gpuEvents.push_back("indices");
}
void rendererDrawIndexedPrimitive(unsigned int count, unsigned int type, int base,
    unsigned int minimum, unsigned int vertices, unsigned int start)
{
    using namespace particle_renderer_fixture;
    requireGpu(); if (vertices*gpuStride > gpuBytes.size()) std::abort();
    GpuIndexedDraw draw = {count,type,base,minimum,vertices,start,
        {gpuBytes.begin(),gpuBytes.begin()+vertices*gpuStride}};
    gpuIndexedDraws.push_back(draw); gpuEvents.push_back("drawIndexed");
}
void rendererDrawPrimitive(unsigned int count, unsigned int type, unsigned int offset)
{
    using namespace particle_renderer_fixture;
    requireGpu(); if (count*3*gpuStride > gpuBytes.size()) std::abort();
    GpuDraw draw = {count,type,offset,{gpuBytes.begin(),gpuBytes.begin()+count*3*gpuStride}};
    gpuDraws.push_back(draw); gpuEvents.push_back("draw");
}
} } }

// [TEMP]
// Unused mesh-particle dependencies report and abort; these fixtures do not draw mesh particles.
namespace particle_renderer_fixture {
[[noreturn]] void meshParticleUnreached(const char* name)
{ std::fprintf(stderr, "[TEMP] particle fixture: unexpected %s\n", name); std::fflush(stderr); std::abort(); }
}
namespace dice { namespace hfe {
namespace dx9rend {
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&)
{ particle_renderer_fixture::meshParticleUnreached("dx9rend::effectGetShaderParameterBorrowedTyped"); }
}
namespace geom {
MeshParticleMeshEffectState g_meshParticleMeshEffectState = {};
void MeshParticleMesh::draw(int, bool)
{ particle_renderer_fixture::meshParticleUnreached("geom::MeshParticleMesh::draw"); }
void MeshParticleMesh::setAgeAndAlpha(float, float, int)
{ particle_renderer_fixture::meshParticleUnreached("geom::MeshParticleMesh::setAgeAndAlpha"); }
#ifndef BF2_PSM_TYPE1_DESCRIPTOR_LINKED   // the type-1 fixture links ParticleSystemTemplateType1.cpp
void particleSystemType1ResolveGeometry(void*)
{ particle_renderer_fixture::meshParticleUnreached("geom::particleSystemType1ResolveGeometry"); }
#endif
}
} }
#endif
