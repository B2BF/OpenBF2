// [TEMP] [CHOSEN, not recovered] Focused x86 runtime fixture for the recovered
// RendDX9 world-pass queue walk and accumulation-map constant publication.
// The production implementation is included unchanged. External GPU/engine
// services remain outside this fixture; the named non-empty draw frontier is
// observed by a recording provider instead of being allowed to abort.

#include <cmath>
#include <cstdio>
#include <cstring>

#define BF2_IMAGE_RENDDX9 1
#include "../BF2/RendDX9/RenderQueues.cpp"

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool value, const char* label)
{
    ++g_checks;
    if (!value)
    {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

struct FrontierHit
{
    const char* name;
    const void* self;
    unsigned int slot;
};

FrontierHit g_frontierHits[8] = {};
unsigned int g_frontierHitCount = 0;
unsigned int g_unexpectedExternalCalls = 0;

// Typed COM test double. The production helper reaches AddRef/Release through
// IDirect3DBaseTexture9 itself; this fixture never builds or indexes a vtable.
class RecordingBaseTexture : public IDirect3DBaseTexture9
{
public:
    explicit RecordingBaseTexture(ULONG initialRefs)
        : refs(initialRefs), addRefCalls(0), releaseCalls(0)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** value) override
    {
        if (value)
            *value = 0;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        ++addRefCalls;
        return ++refs;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ++releaseCalls;
        return --refs;
    }

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** device) override
    {
        if (device)
            *device = 0;
        return D3DERR_INVALIDCALL;
    }

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, const void*, DWORD, DWORD) override
    {
        return D3DERR_INVALIDCALL;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void*, DWORD*) override
    {
        return D3DERR_INVALIDCALL;
    }

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override
    {
        return D3DERR_INVALIDCALL;
    }

    DWORD STDMETHODCALLTYPE SetPriority(DWORD priority) override
    {
        return priority;
    }

    DWORD STDMETHODCALLTYPE GetPriority() override
    {
        return 0;
    }

    void STDMETHODCALLTYPE PreLoad() override
    {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override
    {
        return D3DRTYPE_TEXTURE;
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD lod) override
    {
        return lod;
    }

    DWORD STDMETHODCALLTYPE GetLOD() override
    {
        return 0;
    }

    DWORD STDMETHODCALLTYPE GetLevelCount() override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override
    {
        return D3D_OK;
    }

    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override
    {
        return D3DTEXF_NONE;
    }

    void STDMETHODCALLTYPE GenerateMipSubLevels() override
    {
    }

    ULONG refs;
    unsigned int addRefCalls;
    unsigned int releaseCalls;
};

template <typename T>
T* zeroObject(unsigned char* storage, size_t storageSize)
{
    std::memset(storage, 0, storageSize);
    return reinterpret_cast<T*>(storage);
}

void testAccumulationProjection()
{
    using namespace dice::hfe::dx9rend;

    alignas(4) unsigned char constantsStorage[sizeof(RaShaderConstants)];
    RaShaderConstants* const constants =
        zeroObject<RaShaderConstants>(constantsStorage, sizeof(constantsStorage));
    g_raShaderConstants = constants;

    g_accumulationProjectionInit = 0;
    for (unsigned int i = 0; i != 4; ++i)
    {
        g_accumulationProjectionScale[i] = 99.0f;
        g_accumulationProjectionOffset[i] = -99.0f;
    }

    RecordingBaseTexture firstBase(10);
    RecordingBaseTexture secondBase(20);

    alignas(4) unsigned char firstTextureStorage[sizeof(EngineTexture)];
    EngineTexture* const firstTexture =
        zeroObject<EngineTexture>(firstTextureStorage, sizeof(firstTextureStorage));
    firstTexture->baseTexture = &firstBase;
    firstTexture->width = 0xCAFE0020u;  // only low 16 bits (32) are consumed
    firstTexture->height = 0xBEEF0040u; // only low 16 bits (64) are consumed

    raShaderConstantsSetAccumulationMap(firstTexture);

    IDirect3DBaseTexture9* const* textureField =
        reinterpret_cast<IDirect3DBaseTexture9* const*>(constantsStorage + 0x284);
    const float* const scale = reinterpret_cast<const float*>(constantsStorage + 0x294);
    const float* const offset = reinterpret_cast<const float*>(constantsStorage + 0x2A8);

    check(*textureField == &firstBase,
          "accumulation raw-texture payload is published at +0x284");
    check(firstBase.refs == 11 && firstBase.addRefCalls == 1 && firstBase.releaseCalls == 0,
          "first accumulation texture gains exactly one COM reference");
    check(g_accumulationProjectionInit == 3u,
          "both lazy projection initialization bits are set");
    check(scale[0] == 0.5f && scale[1] == -0.5f && scale[2] == 1.0f && scale[3] == 1.0f,
          "projection scale is copied to the +0x294 Vec4 payload");
    check(offset[0] == 0.515625f && offset[1] == 0.5078125f
              && offset[2] == 0.0f && offset[3] == 0.0f,
          "half-texel projection offset uses the low-16 width and height");

    raShaderConstantsSetAccumulationMap(firstTexture);
    check(firstBase.refs == 11 && firstBase.addRefCalls == 1 && firstBase.releaseCalls == 0,
          "republishing the identical D3D texture leaves ownership unchanged");

    alignas(4) unsigned char secondTextureStorage[sizeof(EngineTexture)];
    EngineTexture* const secondTexture =
        zeroObject<EngineTexture>(secondTextureStorage, sizeof(secondTextureStorage));
    secondTexture->baseTexture = &secondBase;
    secondTexture->width = 2u;
    secondTexture->height = 4u;

    // Prove the payload is recopied each pass, while its process-lifetime
    // backing values are not recomputed for a replacement target size.
    std::memset(constantsStorage + 0x294, 0xA5, 16);
    std::memset(constantsStorage + 0x2A8, 0x5A, 16);
    raShaderConstantsSetAccumulationMap(secondTexture);

    check(*textureField == &secondBase,
          "replacement accumulation texture is published at the same record");
    check(firstBase.refs == 10 && firstBase.releaseCalls == 1
              && secondBase.refs == 21 && secondBase.addRefCalls == 1,
          "replacement releases the old COM texture before retaining the new one");
    check(scale[0] == 0.5f && scale[1] == -0.5f && scale[2] == 1.0f && scale[3] == 1.0f,
          "projection scale is recopied on a later world pass");
    check(offset[0] == 0.515625f && offset[1] == 0.5078125f
              && offset[2] == 0.0f && offset[3] == 0.0f,
          "projection offset remains cached across a target-size replacement");
    check(g_accumulationProjectionOffset[0] == 0.515625f
              && g_accumulationProjectionOffset[1] == 0.5078125f,
          "process backing offset itself retains the first target dimensions");

    raShaderConstantsSetTexture(0x280u, 0);
    check(*textureField == 0 && secondBase.refs == 20 && secondBase.releaseCalls == 1,
          "clearing the raw record releases the final owned COM reference");
    g_raShaderConstants = 0;
}

// A value-initialized queue (zeroed words, empty entry list and name) holding `count` fake
// entry pointers. Only `enabled` and the entry list are read by the world-pass walk.
dice::hfe::dx9rend::RenderQueue* prepareQueue(unsigned char* storage,
                                              unsigned char enabled,
                                              unsigned int* first,
                                              unsigned int count)
{
    using namespace dice::hfe::dx9rend;
    RenderQueue* const queue = new (storage) RenderQueue();
    queue->enabled = enabled;
    for (unsigned int i = 0; i != count; ++i)
        queue->entries.push_back(reinterpret_cast<RenderQueueEntry*>(first + i));
    return queue;
}

void testWorldQueueCollections()
{
    using namespace dice::hfe::dx9rend;

    // Value-initialized: zeroed words, empty shader map and queue collections.
    alignas(RenderQueueTable) unsigned char tableStorage[sizeof(RenderQueueTable)];
    RenderQueueTable* const table = new (tableStorage) RenderQueueTable();

    alignas(RenderQueue) unsigned char queueStorage[5][sizeof(RenderQueue)];
    RenderQueue* queues[5];

    unsigned int entries[3] = { 11u, 22u, 33u };
    queues[0] = prepareQueue(queueStorage[0], 0, entries, 1);     // disabled, but non-empty
    queues[1] = prepareQueue(queueStorage[1], 1, entries, 0);     // empty (the VC7.1 null-first shape)
    queues[2] = prepareQueue(queueStorage[2], 1, entries, 0);     // empty
    queues[3] = prepareQueue(queueStorage[3], 1, entries, 1);     // enabled and non-empty
    queues[4] = prepareQueue(queueStorage[4], 1, entries + 1, 2); // second collection

    table->worldQueues.assign(queues, queues + 4);
    table->pointLightQueues.assign(queues + 4, queues + 5);
    table->defaultShader = 0;

    g_frontierHitCount = 0;
    g_renderQueues = table;
    renderQueuesFlushWorldPass();

    check(g_frontierHitCount == 2,
          "only enabled non-empty queues reach the recovered draw-walk frontier");
    check(g_frontierHits[0].self == queues[3] && g_frontierHits[1].self == queues[4],
          "the two +0x94/+0xA4 collections are walked in shipped order");
    check(g_frontierHits[0].slot == 0x08u && g_frontierHits[1].slot == 0x08u,
          "world-pass non-empty queues invoke the flushWithArg owner method");
    check(std::strstr(g_frontierHits[0].name, "NON-EMPTY queue flush") != 0
              && std::strstr(g_frontierHits[1].name, "NON-EMPTY queue flush") != 0,
          "both observations stop at the named unrecovered draw walk");

    // Empty collections are legal no-ops, including the never-filled shape
    // installed by createRenderQueues.
    table->worldQueues.clear();
    std::vector<void*>().swap(table->pointLightQueues);
    g_frontierHitCount = 0;
    renderQueuesFlushWorldPass();
    check(g_frontierHitCount == 0,
          "empty and never-filled collections perform no queue flushes");

    g_renderQueues = 0;
    for (unsigned int i = 0; i != 5; ++i)
        queues[i]->~RenderQueue();
    table->~RenderQueueTable();
}

} // anonymous namespace

namespace renddx9abi {

// [TEMP] Recording implementation for the fixture only. This replaces the
// diagnostic transport, not any production queue or constant helper.
void frontier(const char* qualifiedName, const void* self, unsigned int slot)
{
    if (g_frontierHitCount < sizeof(g_frontierHits) / sizeof(g_frontierHits[0]))
    {
        g_frontierHits[g_frontierHitCount].name = qualifiedName;
        g_frontierHits[g_frontierHitCount].self = self;
        g_frontierHits[g_frontierHitCount].slot = slot;
    }
    ++g_frontierHitCount;
}

void logf(const char*, ...)
{
    ++g_unexpectedExternalCalls;
}

unsigned int addr(const void* value)
{
    return reinterpret_cast<unsigned int>(value);
}

} // namespace renddx9abi

namespace dice {
namespace hfe {
namespace dx9rend {

// [TEMP] Link-complete sentinels for services used only by uncalled portions
// of RenderQueues.cpp. Any accidental reach fails the fixture at process end.
void registerDeviceLostResetListener(void*, int)
{
    ++g_unexpectedExternalCalls;
}

void unregisterDeviceLostResetListener(void*)
{
    ++g_unexpectedExternalCalls;
}

void textureDtor(EngineTexture*)
{
    ++g_unexpectedExternalCalls;
}

void textureAddRef(EngineTexture*)
{
    ++g_unexpectedExternalCalls;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

int main()
{
    static_assert(sizeof(void*) == 4, "this fixture must be compiled as x86");

    testAccumulationProjection();
    testWorldQueueCollections();
    check(g_unexpectedExternalCalls == 0,
          "no unmodelled renderer service is reached by the focused helper paths");

    std::printf("RendDX9 detail helpers: %d/%d checks passed "
                "(actual RenderQueues.cpp; named draw frontier recorded).\n",
                g_checks - g_failures, g_checks);
    return g_failures ? 1 : 0;
}
