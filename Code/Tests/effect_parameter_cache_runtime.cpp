// [TEMP] [CHOSEN, not recovered] Real Effect cache and diagnostic-logging experiment.
// No renderer/device initialization or gameplay. The unrecovered Effect destructor
// is deliberately not called; process exit reclaims these component-owned objects.
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#ifndef BF2_EFFECT_SOURCE
#define BF2_EFFECT_SOURCE "../BF2/RendDX9/Effect.cpp"
#endif
#define private public
#include BF2_EFFECT_SOURCE
#undef private
#ifndef BF2_TEST_EXPECT_LOOKUP_TRACE
#define BF2_TEST_EXPECT_LOOKUP_TRACE 0
#endif

using namespace dice::hfe::dx9rend;
namespace {
int checks = 0, failures = 0;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
unsigned long long logBytes()
{
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExA("RendDX9Rebuilt.log", GetFileExInfoStandard, &data)) return 0;
    return (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}
}
int main()
{
    renddx9abi::logOpen();
    renddx9abi::logf("[TEST] actual shader-parameter cache experiment");
    check(logBytes() != 0, "actual diagnostic logger opens and flushes output");
    Effect* effect = createEffectObject();
    if (!effect) return 2;
    // Names used by NameTagRenderer::drawIcons, constructed before measurements.
    const vc71::string names[] = { "FadeOut", "HealthBarTrans", "IconTexOffset",
        "IconRotation", "CrossFadeValue", "ColorIndex1", "ColorIndex2", "IconFlashTexScaleOffset" };
    const unsigned int count = sizeof(names) / sizeof(names[0]);
    ShaderParameter* handles[count];
    vc71::map_node* nodes[count];
    for (unsigned int i = 0; i != count; ++i)
    {
        handles[i] = effectGetShaderParameterBorrowedTyped(effect, names[i]);
        nodes[i] = vc71::mapFind(effect->m_parameters, names[i].c_str());
        check(handles[i] && nodes[i], "first lookup creates cached parameter and registry entry");
        check(handles[i] && handles[i]->getRef() == 1, "borrowed miss retains only cache reference");
        for (unsigned int j = 0; j != i; ++j)
            check(handles[i] != handles[j], "distinct semantic names have distinct handles");
    }
    const int ownerRefs = g_parameterOwner->getRef();
    check(effect->m_parameters.size == count, "one registry node per distinct semantic");
    LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
    unsigned int mismatches = 0;
    for (unsigned int iterations : { 1000u, 10000u })
    {
        LARGE_INTEGER begin, end;
        const unsigned long long before = logBytes();
        QueryPerformanceCounter(&begin);
        for (unsigned int i = 0; i != iterations; ++i)
        {
            const unsigned int index = i % count;
            if (effectGetShaderParameterBorrowedTyped(effect, names[index]) != handles[index]) ++mismatches;
        }
        QueryPerformanceCounter(&end);
        const double milliseconds = (end.QuadPart - begin.QuadPart) * 1000.0 / frequency.QuadPart;
        const unsigned long long addedBytes = logBytes() - before;
        std::printf("LOOKUP iterations=%u milliseconds=%.6f log_bytes=%llu\n",
            iterations, milliseconds, addedBytes);
#if BF2_TEST_EXPECT_LOOKUP_TRACE
        check(addedBytes != 0, "explicit baseline mode observes per-lookup trace writes");
#else
        check(addedBytes == 0, "warm lookup emits no per-call diagnostic output");
#endif
    }
    check(mismatches == 0, "all warm lookups preserve handle identity");
    check(effect->m_parameters.size == count, "warm lookups allocate no new cache entries");
    check(g_parameterOwner->getRef() == ownerRefs, "warm lookups create no new owners or parameter references");
    for (unsigned int i = 0; i != count; ++i)
    {
        check(vc71::mapFind(effect->m_parameters, names[i].c_str()) == nodes[i], "warm lookups retain original registry nodes");
        check(handles[i]->getRef() == 1, "borrowed hits do not increment reference counts");
    }
    ShaderParameter* owned = effectGetShaderParameterTyped(effect, names[0]);
    check(owned == handles[0] && owned->getRef() == 2, "owned wrapper still adds its caller reference");
    check(owned->release() == 1, "owned caller release preserves cache reference");
    const vc71::string failedName("cachedNull");
    vc71::map_node* failedNode = vc71::mapInsert(effect->m_parameters, 0x30, failedName.c_str());
    vc71::mapNodeValueRef(failedNode) = 0; // Test-owned prior allocation-failure state.
    check(effectGetShaderParameterBorrowedTyped(effect, failedName) == 0,
        "cached null allocation result remains null without replacement");
    check(effect->m_parameters.size == count + 1 && vc71::mapFind(effect->m_parameters, failedName.c_str()) == failedNode,
        "cached null lookup leaves the original registry entry intact");
    renddx9abi::logf("[TEST] actual diagnostic logger remains callable");
    renddx9abi::logClose();
    std::printf("Effect parameter cache: %d/%d checks passed; final_log_bytes=%llu\n",
        checks - failures, checks, logBytes());
    return failures ? 1 : 0;
}
