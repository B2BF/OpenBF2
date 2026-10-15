// [TEMP] [CHOSEN, not recovered] Calls the actual production pre/post-load bodies with
// observed BufferManager, settings, allocation, TextureAtlas and singleton boundaries.
// No constructor, concrete particle-instance, D3D, map-load or gameplay acceptance is claimed.
#include "../BF2/Geom/ParticleSystem/ParticleSystemManager.cpp"
#include <cstdio>
#include <vector>
#include <string>
#include <cstdarg>

using namespace dice::hfe;
using namespace dice::hfe::geom;

namespace {
int checks = 0, failures = 0;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, #e); } } while (0)
struct Allocation { void* pointer; unsigned int size; };
struct Buffer { unsigned int count, stride, usage, format; std::vector<unsigned short> indices; };
std::vector<Allocation> allocations;
std::vector<Buffer*> buffers;
std::vector<std::string> events;
int priority = 17, pool = 1, lightReads = 0, applyCalls = 0, atlasCalls = 0, singletonCalls = 0;
bool useAtlas = false;
ParticleSystemManager* currentManager = 0;
void* const atlasSentinel = reinterpret_cast<void*>(0x17171717u);
void* const heightmapSentinel = reinterpret_cast<void*>(0x18181818u);

float __fastcall lightRead(void*, void*) { ++lightReads; events.push_back("light"); return 0.5f; }
IBase* __fastcall singletonRead(void*, void*, const renddx9abi::HostString& name)
{
    ++singletonCalls;
    CHECK(name == "HeightmapCluster");
    return static_cast<IBase*>(heightmapSentinel);
}
void __fastcall applyProfile(void* receiver, void*, const ParticleSystemSettings* settings)
{
    CHECK(receiver == currentManager);
    CHECK(priority == 5);
    CHECK(settings->distanceFactor == 500.0f && settings->poolCount04 == 1000);
    CHECK(settings->poolCount08 == 700 && settings->poolCount0C == 600);
    CHECK(settings->nearRatio == 0.4f && settings->farRatio == 0.85f);
    CHECK(settings->shaderQuality == 3 && settings->qualityLevel == 2);
    ++applyCalls;
    events.push_back("apply");
    currentManager->m_settingsApplied = 1;
}

void checkChain(void* backing, void* head, unsigned int count, unsigned int stride)
{
    CHECK(backing == head);
    unsigned char* item = static_cast<unsigned char*>(head);
    for (unsigned int i = 0; i < count; ++i)
    {
        CHECK(item == static_cast<unsigned char*>(backing) + i * stride);
        item = *reinterpret_cast<unsigned char**>(item);
    }
    CHECK(item == 0);
}

void verifyBuffers()
{
    CHECK(buffers.size() == 6);
    CHECK(buffers[0]->count == 18 && buffers[0]->stride == 64 && buffers[0]->usage == 0x208);
    CHECK(buffers[1]->count == 36 && buffers[1]->stride == 72 && buffers[1]->usage == 0x208);
    CHECK(buffers[2]->count == 30 && buffers[2]->stride == 68 && buffers[2]->usage == 0x208);
    for (unsigned int side = 1; side <= 3; ++side)
    {
        const Buffer* b = buffers[2 + side];
        CHECK(b->count == 24u * side && b->usage == 8 && b->format == 101);
        const unsigned short start[] = {0,1,static_cast<unsigned short>(2*side+1),
                                       0,static_cast<unsigned short>(2*side+1),
                                       static_cast<unsigned short>(2*side)};
        CHECK(std::memcmp(b->indices.data(), start, sizeof(start)) == 0);
        const unsigned int final = 10u * side;
        CHECK(b->indices[b->count - 1] == final - 2u);
        const Vec3* radial = psmRendererField<Vec3*>(currentManager->m_trailRenderer, 0x8C8 + 4u*side);
        CHECK(radial != 0);
        for (unsigned int i = 0; i < side; ++i)
        {
            CHECK(std::memcmp(radial + i*2, radial + i*2 + 1, sizeof(Vec3)) == 0);
            CHECK(radial[i*2].z == 0.0f);
            CHECK(std::fabs(radial[i*2].x * radial[i*2].x + radial[i*2].y * radial[i*2].y - 1.0f) < 0.000001f);
        }
    }
}

void verifyPools()
{
    ParticleSystemManager& m = *currentManager;
    checkChain(m.m_type0PoolBacking, m.m_type0FreeList, 3, 148);
    checkChain(m.m_type4PoolBacking, m.m_type4FreeList, 6, 200);
    checkChain(m.m_type1PoolBacking, m.m_type1FreeList, 4, 192);
    checkChain(m.m_type2PoolBacking, m.m_type2FreeList, 5, 168);
    checkChain(m.m_type3PoolBacking, m.m_type3FreeList, 400, 176);
    const unsigned int* p0 = static_cast<unsigned int*>(m.m_type0PoolBacking);
    const unsigned int* p1 = static_cast<unsigned int*>(m.m_type1PoolBacking);
    const unsigned int* p2 = static_cast<unsigned int*>(m.m_type2PoolBacking);
    const unsigned int* p3 = static_cast<unsigned int*>(m.m_type3PoolBacking);
    const unsigned int* p4 = static_cast<unsigned int*>(m.m_type4PoolBacking);
    CHECK(p0[3] == 0 && p0[27] == 0 && p0[1] == 0xA5A5A5A5u && p0[28] == 0xA5A5A5A5u);
    CHECK(p1[1] == 0x3F800000 && p1[6] == 0x3F800000 && p1[16] == 0x3F800000);
    CHECK(p1[23] == 0xA5A5A5A5u && p1[44] == 0 && p1[45] == 0xA5A5A5A5u);
    CHECK(p2[29] == 0 && p2[30] == 0xA5A5A5A5u && p2[37] == 0);
    CHECK(p3[1] == 0x3F800000 && p3[23] == 0xA5A5A5A5u && p3[39] == 0);
    CHECK(p4[1] == 0 && p4[7] == 0xA5A5A5A5u && p4[37] == 0xA5A5A5A5u && p4[49] == 0);
}

void verifyOrder(bool applied, bool atlas)
{
    std::vector<std::string> expected;
    expected.push_back("priority");
    if (applied) expected.push_back("apply");
    const char* sequence[] = {
        "light", "light", "pool", "ParticleSystemRenderer", "pool",
        "pool", "NSAParticleSystemRenderer", "pool", "pool", "TrailRenderer",
        "pool", "ib", "pool", "lock", "unlock",
        "pool", "ib", "pool", "lock", "unlock",
        "pool", "ib", "pool", "lock", "unlock", "pool" };
    expected.insert(expected.end(), sequence, sequence + sizeof(sequence)/sizeof(sequence[0]));
    if (atlas) expected.push_back("atlas");
    expected.push_back("priority");
    CHECK(events == expected);
    const unsigned int sizes[] = {444,12,12,1200,24,768,16,840,20,24,48,72,70400};
    CHECK(allocations.size() == sizeof(sizes)/sizeof(sizes[0]));
    for (unsigned int i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i)
        CHECK(allocations[i].size == sizes[i]);
}

void cleanup()
{
    for (const Allocation& allocation : allocations) std::free(allocation.pointer);
    allocations.clear();
    for (Buffer* buffer : buffers) delete buffer;
    buffers.clear();
    events.clear();
}
}

namespace renddx9abi {
void* engineAlloc(unsigned int size)
{
    CHECK(priority == 5);
    void* p = std::malloc(size);
    std::memset(p, 0xA5, size);
    allocations.push_back({p,size});
    return p;
}
void engineFree(void*) { CHECK(false); }
void logf(const char*, ...) { }
void frontier(const char* name, const void*, unsigned int)
{ std::printf("UNEXPECTED FRONTIER %s\n", name); std::abort(); }
unsigned int addr(const void* p) { return reinterpret_cast<unsigned int>(p); }
}

namespace dice { namespace hfe {
IClassManager* classManager = 0;
namespace geom {
LightManager* g_LightManager = 0; IBase* g_ParticleSystemManager = 0;
// The whole production TU is compiled. These unrelated constructor references must link but
// must never run in this fixture; any accidental reach is a hard failure, not a fake object.
void* constructParticleSystemTemplateType0(void*,const void*) { std::abort(); }
void* constructParticleSystemType1(void*,const void*) { std::abort(); }
void* constructParticleSystemType2(void*,const void*) { std::abort(); }
void* constructParticleSystemType3(void*,const void*) { std::abort(); }
void* constructParticleSystemType4(void*,const void*) { std::abort(); }
// [TEMP] Draw/descriptor lookup are outside this pre/post-load fixture's exercised path.
void* staticMeshRootParent(void*) { std::abort(); }
}
namespace dx9rend {
unsigned int rendererFrameOrdinal() { std::abort(); } // Instance start is outside this fixture.
int readIntSetting(const char*,int) { std::abort(); }
IRenderView* rendererGetRenderViewDup() { std::abort(); }
IVideoOptions* rendererGetVideoOptions() { std::abort(); }
TextureAtlas* textureAtlasCtor(void*) { std::abort(); }
TextureAtlasEntry* textureAtlasFind(TextureAtlas*, const std::string&) { std::abort(); }
unsigned int textureAtlasEntryCount(const TextureAtlas*) { std::abort(); }
int resourceManagerSwapDefaultPriority(int value)
{ const int old = priority; priority = value; events.push_back("priority"); return old; }
int bufferManagerSwapPool(int value)
{ const int old = pool; pool = value; events.push_back("pool"); return old; }
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride, unsigned int usage, const char* name)
{
    CHECK(pool == 0 && priority == 5);
    Buffer* b = new Buffer{count,stride,usage,0,{}};
    buffers.push_back(b); events.push_back(name); return b;
}
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage, unsigned int format)
{
    CHECK(pool == 0 && priority == 5);
    Buffer* b = new Buffer{count,0,usage,format,std::vector<unsigned short>(count,0xEEEE)};
    buffers.push_back(b); events.push_back("ib"); return b;
}
void* indexBufferLock(void* raw, unsigned int offset, unsigned int count, unsigned int flags)
{
    Buffer* b = static_cast<Buffer*>(raw);
    CHECK(offset == 0 && count == b->count && flags == 0);
    events.push_back("lock"); return b->indices.data();
}
bool indexBufferUnlock(void*) { events.push_back("unlock"); return true; }
bool readBoolSetting(const char* name, bool fallback)
{ CHECK(std::strcmp(name,"GSUseEffectTextureAtlas") == 0 && !fallback); return useAtlas; }
bool textureAtlasLoadTAI(TextureAtlas* atlas, const std::string& path)
{
    CHECK(atlas == atlasSentinel && priority == 5 && currentManager->m_poolsInitialized == 0);
    CHECK(std::strcmp(path.c_str(), "Objects/Effects/Textures/Atlas/ParticleAtlas.tai") == 0);
    events.push_back("atlas"); ++atlasCalls; return false; // shipped caller ignores load failure
}
}
} }

#include "particle_renderer_fixture_support.h"

int main()
{
    void* lightTable[0x274 / 4] = {};
    lightTable[0x270 / 4] = reinterpret_cast<void*>(&lightRead);
    void* lightObject = lightTable;
    g_LightManager = reinterpret_cast<LightManager*>(&lightObject);
    void* classTable[0x4C / 4] = {};
    classTable[0x48 / 4] = reinterpret_cast<void*>(&singletonRead);
    void* classObject = classTable;
    classManager = reinterpret_cast<IClassManager*>(&classObject);
    void* managerTable[0x40 / 4] = {};
    managerTable[0x3C / 4] = reinterpret_cast<void*>(&applyProfile);

    for (int pass = 0; pass < 2; ++pass)
    {
        ParticleSystemManager manager = {};
        currentManager = &manager;
        manager.m_vptr = const_cast<const void**>(managerTable);
        manager.m_settingsApplied = pass != 0;
        manager.m_type0PoolCount = 3; manager.m_type1PoolCount = 4;
        manager.m_type2PoolCount = 5; manager.m_type4PoolCount = 6;
        manager.m_cachedLightValue = -1.0f;
        manager.m_lodScales[0] = 0.3f; manager.m_lodScales[1] = 0.6f; manager.m_lodScales[2] = 1.0f;
        manager.m_nearRatio = 0.4f; manager.m_farRatio = 0.85f; manager.m_effectiveViewDistance = 500.0f;
        manager.m_textureAtlasHelper = atlasSentinel;
        useAtlas = pass != 0;
        lightReads = 0;
        particleSystemManagerPreLoadLevel(reinterpret_cast<IParticleSystemManager*>(&manager));
        CHECK(priority == 17 && pool == 1 && manager.m_poolsInitialized == 1);
        CHECK(lightReads == 2 && applyCalls == 1 && atlasCalls == pass);
        CHECK(events.front() == "priority" && events.back() == "priority");
        CHECK(manager.m_baseDistances[2] == 500.0f && manager.m_nearDistances[2] == 200.0f);
        CHECK(manager.m_type3PoolCount == 400);
        verifyPools(); verifyBuffers(); verifyOrder(pass == 0, useAtlas);
        particleSystemManagerPostLoadLevel(reinterpret_cast<IParticleSystemManager*>(&manager));
        CHECK(manager.m_heightmapCluster == heightmapSentinel);
        // Retail preLoad has no initialized guard and does not delete prior backing. Verify
        // repeated calls allocate fresh chains while leaving the earlier allocations intact.
        void* const previousBacking = manager.m_type0PoolBacking;
        const size_t beforeAllocations = allocations.size();
        const int beforeApplyCalls = applyCalls;
        useAtlas = false;
        particleSystemManagerPreLoadLevel(reinterpret_cast<IParticleSystemManager*>(&manager));
        CHECK(manager.m_type0PoolBacking != previousBacking);
        CHECK(allocations.size() == 2u * beforeAllocations);
        CHECK(applyCalls == beforeApplyCalls && priority == 17 && pool == 1);
        CHECK(static_cast<unsigned int*>(previousBacking)[3] == 0);
        checkChain(manager.m_type0PoolBacking, manager.m_type0FreeList, 3, 148);
        cleanup();
    }
    CHECK(singletonCalls == 2);
    std::printf("Particle pre/post-load: %d/%d checks passed\n", checks-failures, checks);
    return failures ? 1 : 0;
}
