// [TEMP] [CHOSEN, not recovered] Actual ParticleSystemManager source and virtual table;
// only the camera, allocator and pre-load collaborators are fixtures. No level, concrete
// particle, embedded renderer constructor, D3D or production startup acceptance is claimed.
#ifdef BF2_PSM_FIXTURE_OLD_SOURCE
#include "particle_update_old_source.inc"
#else
#include "../BF2/Geom/ParticleSystem/ParticleSystemManager.cpp"
#endif
#include <cstddef>
#include <cstdio>
#include <vector>
#include <string>
#include <limits>

using namespace dice::hfe;
using namespace dice::hfe::geom;

namespace {
int checks = 0, failures = 0;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, #e); } } while (0)
ParticleSystemManager* currentManager = 0;
// A byte image of a manager. Each std container's bytes are its storage pointers and size, so an
// unchanged image also means every runtime vector kept its backing storage and capacity.
struct ManagerImage { unsigned char bytes[sizeof(ParticleSystemManager)]; };
void capture(ManagerImage& image, const ParticleSystemManager& manager)
{ std::memcpy(image.bytes, &manager, sizeof(image.bytes)); }
ManagerImage frontierSnapshot;
std::vector<std::string> calls;
std::vector<void*> allocations;
std::vector<unsigned int> allocationSizes;
struct Buffer { std::vector<unsigned short> indices; };
std::vector<Buffer*> buffers;
void* viewTable[0x9C / 4] = {};
void* viewObject = viewTable;
Mat4 cameraMatrix;
float fovValues[4] = {1.0f, 1.2f, 1.4f, 1.6f};
float aspectValue = 1.75f;
unsigned int previousDeltaBits = 0;
int fovReads = 0, priority = 17, pool = 1;
bool expectedFrontier = false;
const void* expectedConstructorFrontierSelf = 0;

unsigned int bits(float value)
{ unsigned int result; std::memcpy(&result, &value, sizeof(result)); return result; }
float fromBits(unsigned int value)
{ float result; std::memcpy(&result, &value, sizeof(result)); return result; }
void observe(const char* name)
{
    CHECK(bits(currentManager->m_deltaTime) == previousDeltaBits);
    CHECK(currentManager->m_pendingClassification.empty());
    CHECK(currentManager->m_pendingDestroy.empty());
    calls.push_back(name);
}
Mat4* __fastcall matrixRead(void* self, void*)
{ CHECK(self == &viewObject); observe("matrix"); return &cameraMatrix; }
float __fastcall fovRead(void* self, void*)
{
    CHECK(self == &viewObject); observe("fov"); CHECK(fovReads < 4);
    return fovValues[fovReads++];
}
float __fastcall aspectRead(void* self, void*)
{ CHECK(self == &viewObject); observe("aspect"); return aspectValue; }
float __fastcall lightRead(void*, void*) { return 0.5f; }

// An empty runtime vector that already owns storage, so a reallocation or release shows up.
void setEmptyCapacity(ParticleSystemInstanceVector& v) { v.reserve(2); }

void verifyUpdate(ParticleSystemManager& manager, float dt)
{
    currentManager = &manager;
    previousDeltaBits = bits(manager.m_deltaTime);
    calls.clear(); fovReads = 0;
    static ManagerImage before, comparable;
    capture(before, manager);
    reinterpret_cast<IParticleSystemManager*>(&manager)->update(dt);
    const char* expected[] = {"view", "matrix", "fov", "fov", "aspect", "fov", "fov"};
    CHECK(calls == std::vector<std::string>(expected, expected + 7));
    CHECK(fovReads == 4 && bits(manager.m_deltaTime) == bits(dt));
    // All runtime vectors retain their backing/capacity. Pool bytes, initialization
    // flag, paused flag and the unused matrix cells must not become frame-loop scratch storage.
    capture(comparable, manager);
    std::memcpy(comparable.bytes + offsetof(ParticleSystemManager, m_deltaTime),
                before.bytes + offsetof(ParticleSystemManager, m_deltaTime), sizeof(float));
    CHECK(std::memcmp(comparable.bytes, before.bytes, sizeof(before.bytes)) == 0);
}

void cleanup()
{
    for (void* p : allocations) std::free(p);
    for (Buffer* p : buffers) delete p;
    allocations.clear(); allocationSizes.clear(); buffers.clear();
}
}

namespace renddx9abi {
void* engineAlloc(unsigned int size)
{ void* p = std::malloc(size); std::memset(p, 0xA5, size); allocations.push_back(p); allocationSizes.push_back(size); return p; }
void engineFree(void* p)
{
    for (unsigned int i = 0; i < allocations.size(); ++i)
        if (allocations[i] == p) { allocations[i] = 0; break; }
    std::free(p);
}
void logf(const char*, ...) {}
unsigned int addr(const void* p) { return reinterpret_cast<unsigned int>(p); }
void frontier(const char* name, const void* self, unsigned int slot)
{
    if (expectedConstructorFrontierSelf && expectedConstructorFrontierSelf == self && slot == 0 &&
        std::strcmp(name,"dice::hfe::geom::ParticleSystemRenderer::ParticleSystemRenderer "
                         "(Debug type 6, ParticleSystemRenderer.cpp:113: Failed to create declspec)") == 0) {
        std::printf("EXPECTED [FRONTIER] %s\n",name); std::fflush(stdout); std::_Exit(86);
    }
    // A non-returning fixture trap replaces the process abort, never a production continuation.
    // Exit 86 is accepted only for a correctly named, mutation-free, pre-camera frontier.
    const bool correct = expectedFrontier && self == currentManager && slot == 0x28 &&
        std::strcmp(name, "dice::hfe::geom::ParticleSystemManager::update (non-empty runtime state)") == 0 &&
        calls.empty() &&
        std::memcmp(currentManager, frontierSnapshot.bytes, sizeof(frontierSnapshot.bytes)) == 0;
    std::printf("%s [FRONTIER] %s\n", correct ? "EXPECTED" : "UNEXPECTED", name);
    std::fflush(stdout);
    std::_Exit(correct ? 86 : 87);
}
}

namespace dice { namespace hfe {
IClassManager* classManager = 0;
namespace geom {
LightManager* g_LightManager = 0; IBase* g_ParticleSystemManager = 0;
void* constructParticleSystemTemplateType0(void*,const void*) { std::abort(); }
#ifndef BF2_PSM_TYPE1_DESCRIPTOR_LINKED
// Type-1 descriptor fixtures link the production implementation.
void* constructParticleSystemType1(void*,const void*) { std::abort(); }
#endif
void* constructParticleSystemType2(void*,const void*) { std::abort(); }
void* constructParticleSystemType3(void*,const void*) { std::abort(); }
void* constructParticleSystemType4(void*,const void*) { std::abort(); }
#ifndef BF2_PSM_DRAW_FIXTURE
void* staticMeshRootParent(void*) { std::abort(); }
#endif
}
namespace dx9rend {
unsigned int rendererFrameOrdinal() { return 100; }
#ifndef BF2_PSM_DRAW_FIXTURE
int readIntSetting(const char*,int) { std::abort(); }
IVideoOptions* rendererGetVideoOptions() { std::abort(); }
TextureAtlas* textureAtlasCtor(void*) { std::abort(); }
IRenderView* rendererGetRenderViewDup()
{ observe("view"); return reinterpret_cast<IRenderView*>(&viewObject); }
#endif
int resourceManagerSwapDefaultPriority(int value) { int old = priority; priority = value; return old; }
int bufferManagerSwapPool(int value) { int old = pool; pool = value; return old; }
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*)
{ CHECK(priority == 5 && pool == 0); Buffer* b = new Buffer; buffers.push_back(b); return b; }
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage, unsigned int format)
{
    CHECK(priority == 5 && pool == 0 && usage == 8 && format == 101);
    Buffer* b = new Buffer; b->indices.resize(count); buffers.push_back(b); return b;
}
void* indexBufferLock(void* raw, unsigned int offset, unsigned int count, unsigned int flags)
{ Buffer* b = static_cast<Buffer*>(raw); CHECK(offset == 0 && count == b->indices.size() && flags == 0); return b->indices.data(); }
bool indexBufferUnlock(void*) { return true; }
bool readBoolSetting(const char* name, bool fallback)
{ CHECK(std::strcmp(name, "GSUseEffectTextureAtlas") == 0 && !fallback); return false; }
bool textureAtlasLoadTAI(TextureAtlas*, const std::string&) { std::abort(); }
TextureAtlasEntry* textureAtlasFind(TextureAtlas*, const std::string&) { std::abort(); }
unsigned int textureAtlasEntryCount(const TextureAtlas*) { std::abort(); }
}
} }

#include "particle_renderer_fixture_support.h"

namespace {
// Explicit ABI fixture objects, never production runtime instances. Byte flags and virtual calls
// model the contracts consumed by the actual manager; all scheduler/container code is production.
struct TestParticle { void** table; unsigned char data[0x23C]; };
void* testParticleTable[51] = {};
std::vector<int> particleCalls;
int __fastcall testType(void* self, void*) { return psmRuntimeField<int>(self, 0x220); }
bool __fastcall testWater(void* self, void*) { return psmRuntimeField<unsigned char>(self, 0x224) != 0; }
bool __fastcall testNotVisible(void*, void*) { return false; }
int __fastcall testRefs(void* self, void*) { return psmRuntimeField<int>(self, 0x228); }
world::IEffectController* __fastcall testController(void* self, void*)
{ particleCalls.push_back(49); return psmRuntimeField<world::IEffectController*>(self, 12); }
void __fastcall testSetController(void* self, void*, world::IEffectController* controller)
{ particleCalls.push_back(48); psmRuntimeField<world::IEffectController*>(self, 12) = controller; }
int __fastcall testStop(void*, void*) { particleCalls.push_back(4); return 0; }
int __fastcall testDetach(void*, void*) { particleCalls.push_back(8); return 0; }
void* __fastcall testDelete(void* self, void*, int flags)
{ CHECK(flags == 1); particleCalls.push_back(1000 + psmRuntimeField<int>(self, 0x22C)); return self; }
void __fastcall testUpdate(void* self, void*, float dt)
{ CHECK(dt == 0.25f); particleCalls.push_back(psmRuntimeField<int>(self, 0x22C)); }
struct TestController : world::IEffectController {
    int starts = 0, stops = 0;
    void addNumEmittersPlaying() override { ++starts; }
    void decNumEmittersPlaying() override { ++stops; }
    world::IObject* getObject() const override { return 0; }
};
IParticleSystemInstance* initializeTest(TestParticle& raw, int type, int id, bool water = false)
{
    std::memset(&raw, 0, sizeof(raw)); raw.table = testParticleTable;
    psmRuntimeField<int>(&raw, 0x220) = type;
    psmRuntimeField<unsigned char>(&raw, 0x224) = water;
    psmRuntimeField<int>(&raw, 0x22C) = id;
    return reinterpret_cast<IParticleSystemInstance*>(&raw);
}
// Releases a runtime vector's storage, leaving it empty.
void freeList(ParticleSystemInstanceVector& list) { ParticleSystemInstanceVector().swap(list); }
void verifyScheduler()
{
    testParticleTable[0] = reinterpret_cast<void*>(&testDelete);
    testParticleTable[1] = reinterpret_cast<void*>(&testType);
    testParticleTable[4] = reinterpret_cast<void*>(&testStop);
    testParticleTable[8] = reinterpret_cast<void*>(&testDetach);
    testParticleTable[10] = reinterpret_cast<void*>(&testNotVisible);
    testParticleTable[39] = reinterpret_cast<void*>(&testRefs);
    testParticleTable[41] = reinterpret_cast<void*>(&testUpdate);
    testParticleTable[48] = reinterpret_cast<void*>(&testSetController);
    testParticleTable[49] = reinterpret_cast<void*>(&testController);
    testParticleTable[50] = reinterpret_cast<void*>(&testWater);
    ParticleSystemManager manager = {};
    manager.m_vptr = const_cast<const void**>(kVtable);
    manager.m_poolsInitialized = 1;
    TestParticle raw[20];
    IParticleSystemInstance* buckets[10][2];
    for (int bucket = 0; bucket < 10; ++bucket)
    {
        const int type = bucket == 0 || bucket == 8 ? 4 : bucket == 3 || bucket == 4 || bucket == 9 ? 1 : bucket == 5 || bucket == 6 ? 2 : 0;
        for (int i = 0; i < 2; ++i) buckets[bucket][i] = initializeTest(raw[bucket * 2 + i], type, bucket * 10 + i);
        manager.m_renderBuckets[bucket].assign(buckets[bucket], buckets[bucket] + 2);
    }
    psmRuntimeField<unsigned char>(buckets[7][1], 5) = 1;
    currentManager = &manager; previousDeltaBits = bits(manager.m_deltaTime);
    calls.clear(); particleCalls.clear(); fovReads = 0;
    reinterpret_cast<IParticleSystemManager*>(&manager)->update(0.25f);
    const int expected[] = {70,91,90,81,80,1,0,11,10,31,30,51,50,21,20,41,40,61,60};
    CHECK(particleCalls == std::vector<int>(expected, expected + 19));
    CHECK(fovReads == 4 && manager.m_deltaTime == 0.25f);
    for (int bucket = 0; bucket < 10; ++bucket) freeList(manager.m_renderBuckets[bucket]);

    TestParticle one;
    IParticleSystemInstance* instance = initializeTest(one, 0, 111, true);
    TestController controller;
    psmRuntimeField<world::IEffectController*>(instance, 12) = &controller;
    psmAddActive(&manager, instance, true);
    CHECK(controller.starts == 1 && manager.m_renderBuckets[7][0] == instance);
    CHECK(psmRuntimeField<unsigned char>(instance, 4) == 1);
    psmRuntimeField<unsigned char>(instance, 7) = 0;
    psmReclassification(&manager).insert(instance);
    psmReclassify(&manager);
    CHECK(controller.stops == 1 && controller.starts == 2);
    CHECK(manager.m_renderBuckets[7].size() == 0 && manager.m_renderBuckets[2].size() == 1);
    CHECK(psmReclassification(&manager).empty());
    psmRuntimeField<int>(instance, 0x228) = 2;
    particleCalls.clear(); psm_removeInstance(&manager, 0, instance);
    const int referenced[] = {49,49,8,4};
    CHECK(particleCalls == std::vector<int>(referenced, referenced + 4));
    CHECK(controller.stops == 2 && manager.m_pendingDestroy.size() == 0);
    psmRuntimeField<int>(instance, 0x228) = 0;
    psmPush(manager.m_instances, instance);
    psm_removeInstance(&manager, 0, instance);
    CHECK(manager.m_pendingClassification.size() == 1 && manager.m_pendingDestroy.size() == 1);
    CHECK(psmRuntimeField<world::IEffectController*>(instance, 12) == 0);
    psmRemoveActive(&manager, false);
    CHECK(!psmRuntimeField<unsigned char>(instance, 4) && manager.m_renderBuckets[2].size() == 0);
    psmPush(manager.m_pendingDestroy, instance); // duplicate destruction queue entries delete once
    particleCalls.clear(); psmDrainDestroy(&manager);
    CHECK(particleCalls.size() == 1 && particleCalls[0] == 1111 && manager.m_instances.size() == 0);
    freeList(manager.m_instances); freeList(manager.m_pendingDestroy); freeList(manager.m_pendingClassification);
    for (int i = 0; i < 10; ++i) freeList(manager.m_renderBuckets[i]);
}
}

int main(int argc, char** argv)
{
    viewTable[0x20 / 4] = reinterpret_cast<void*>(&fovRead);
    viewTable[0x44 / 4] = reinterpret_cast<void*>(&aspectRead);
    viewTable[0x64 / 4] = reinterpret_cast<void*>(&matrixRead);
    ParticleSystemManager manager = {};
    manager.m_vptr = const_cast<const void**>(kVtable);
    manager.m_deltaTime = -123.0f;
    currentManager = &manager;
    setEmptyCapacity(manager.m_instances);
    setEmptyCapacity(manager.m_pendingDestroy);
    setEmptyCapacity(manager.m_pendingClassification);
    for (int i = 0; i < 10; ++i) setEmptyCapacity(manager.m_renderBuckets[i]);
    verifyUpdate(manager, 0.125f);
    manager.m_poolsInitialized = 1;
    manager.m_paused = 1; // update has no global pause or pool-initialization early return.
    verifyUpdate(manager, -0.0f);
    for (int i = 0; i < 4; ++i) fovValues[i] = std::numeric_limits<float>::quiet_NaN();
    verifyUpdate(manager, fromBits(0x7FC12345u));
    for (int i = 0; i < 4; ++i) fovValues[i] = 0.0f;
    verifyUpdate(manager, -2.5f); // Shipped empty-camera arithmetic has no FOV sanitization guard.
    for (int i = 0; i < 4; ++i) fovValues[i] = 1.0f + 0.2f * i;

    // This second object reaches initialized state solely via the actual pre-load body.
    // No fixture writes or clears its m_poolsInitialized flag.
    ParticleSystemManager loaded = {};
    loaded.m_vptr = const_cast<const void**>(kVtable);
    loaded.m_settingsApplied = 1;
    loaded.m_type0PoolCount = loaded.m_type1PoolCount = loaded.m_type2PoolCount = loaded.m_type4PoolCount = 2;
    loaded.m_lodScales[0] = 0.3f; loaded.m_lodScales[1] = 0.6f; loaded.m_lodScales[2] = 1.0f;
    loaded.m_nearRatio = 0.4f; loaded.m_farRatio = 0.85f; loaded.m_effectiveViewDistance = 500.0f;
    void* lightTable[0x274 / 4] = {};
    lightTable[0x270 / 4] = reinterpret_cast<void*>(&lightRead);
    void* lightObject = lightTable;
    g_LightManager = reinterpret_cast<LightManager*>(&lightObject);
    currentManager = &loaded;
    particleSystemManagerPreLoadLevel(reinterpret_cast<IParticleSystemManager*>(&loaded));
    CHECK(loaded.m_poolsInitialized == 1 && priority == 17 && pool == 1);
    CHECK(loaded.m_type0PoolBacking && loaded.m_type1PoolBacking && loaded.m_type2PoolBacking &&
          loaded.m_type3PoolBacking && loaded.m_type4PoolBacking);
    CHECK(allocations.size() == 13 && buffers.size() == 6);
    verifyUpdate(loaded, 0.03125f);
    verifyUpdate(loaded, 0.0625f);
    verifyScheduler();
    cleanup();
    std::printf("Particle manager update/queues: %d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
