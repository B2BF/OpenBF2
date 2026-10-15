// [TEMP] [CHOSEN, not recovered] Focused constructor/lifecycle fixture.
// Uses production OverGrowth methods, authored untouched-byte sentinels and process-local heaps.
// Members are read by name: the native object holds std containers (issue #7), so retail offsets
// after the first container no longer apply.
#include "../BF2/Geom/Overgrowth/OverGrowth.h"
#include "../BF2/Geom/XpLightManager.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
int checks;
int failures;
int frees;
int nullFrees;
// The raw word of a member the constructor leaves as allocated (0xCC fill).
template<class T> unsigned int bits(const T& member)
{
    static_assert(sizeof(T) == 4, "word-sized member");
    unsigned int value;
    std::memcpy(&value, &member, 4);
    return value;
}
void check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); }
}
}
namespace renddx9abi {
void* engineAlloc(unsigned int size)
{
    void* p = std::malloc(size);
    if (p) std::memset(p, 0xCC, size);
    return p;
}
void engineFree(void* p) { if (p) ++frees; else ++nullFrees; std::free(p); }
}
namespace dice { namespace hfe { namespace dx9rend {
void* g_OverGrowth;
void overGrowthPreLoadResources();
void overGrowthPostLoadResources();
} } }

int main(int argc, char** argv)
{
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    using dice::hfe::geom::OverGrowth;
    using namespace dice::hfe::dx9rend;
    OverGrowth* p = dice::hfe::geom::createOverGrowth();
    g_OverGrowth = p;
    if (argc > 1 && std::strcmp(argv[1], "nonempty") == 0)
    {
        p->m_path = "authored/nonempty";
        overGrowthPreLoadResources(); // Must diagnose and abort; never return fabricated success.
        return 99;
    }
    check(p != 0, "owner allocated");
    check(p->m_path.empty() && p->m_path.size() == 0,
          "real constructed empty path has zero size");
    check(p->m_randomSeed == 0x759DC150 && p->m_gridSize == 10
              && p->m_viewDistance == 500.0f && p->m_qualityScale == 1.0f,
          "base and derived scalar defaults match the PE");
    check(bits(p->m_originX) == 0xCCCCCCCC
              && bits(p->m_patchCountX) == 0xCCCCCCCC
              && bits(p->m_streamVertexCounts[0]) == 0xCCCCCCCC,
          "constructor preserves unwritten coordinates, grid and tail");
    void** table = *reinterpret_cast<void***>(p);
    typedef float (__thiscall* Getter)(void*);
    typedef void (__thiscall* Draw)(void*, void*, float);
    typedef void (__thiscall* Update)(void*, void*);
    check(reinterpret_cast<Getter>(table[0x10 / 4])(p) == 500.0f,
          "actual primary +10 getter returns its float through ST0");
    overGrowthPreLoadResources();
    overGrowthPostLoadResources();
    reinterpret_cast<Draw>(table[0x34 / 4])(p, 0, 12.0f);
    reinterpret_cast<Update>(table[0x1C / 4])(p, 0);
    check(bits(p->m_patchCountX) == 0xCCCCCCCC && p->m_patches == 0,
          "empty direct lifecycle and actual draw slots touch no resource/view fields");
    {
        typedef void (__thiscall* ShadowQuery)(void*, void*, void*, void*, void*);
        const ShadowQuery shadowQuery = reinterpret_cast<ShadowQuery>(table[0x38 / 4]);
        std::vector<dice::hfe::geom::LightShadowRecord> records;
        std::vector<void*> geometry;
        unsigned char patchSentinel = 0;

        // The rebuilt client's OverGrowth: never loaded, patch array +0xE0 still the ctor's null.
        shadowQuery(p, &records, 0, 0, &geometry);
        check(geometry.empty() && p->m_patches == 0,
              "+38 on the never-loaded owner returns at the patch-array test, touching nothing");

        // Each later guard, one at a time.
        p->m_patches = reinterpret_cast<dice::hfe::geom::OverGrowthPatch*>(&patchSentinel);
        p->m_drawNear = 0;
        shadowQuery(p, &records, 0, 0, &geometry);
        check(geometry.empty(), "+38 returns at the +D4 byte");
        p->m_drawNear = 1;
        shadowQuery(p, &records, 0, 0, &geometry);
        check(geometry.empty(), "+38 returns on an empty records vector (100CE200)");
        records.resize(1);
        const int gridSize = p->m_gridSize;
        p->m_gridSize = 0;
        shadowQuery(p, &records, 0, 0, &geometry);
        check(geometry.empty(), "+38 returns on a non-positive grid size (signed +10)");
        p->m_gridSize = gridSize;
        if (argc > 1 && std::strcmp(argv[1], "shadowwalk") == 0)
        {
            shadowQuery(p, &records, 0, 0, &geometry); // Must name the patch walk and abort.
            return 98;
        }
        p->m_patches = 0;                              // clear() must see the ctor's state again
    }
    const int oldFrees = frees;
    const int oldNullFrees = nullFrees;
    typedef void* (__thiscall* Delete)(void*, unsigned int);
    reinterpret_cast<Delete>(table[0])(p, 1);
    g_OverGrowth = 0;
    check(frees == oldFrees + 1 && nullFrees == oldNullFrees + 4,
          "scalar deleting slot frees owner after both ordered base cleanup passes");
    std::printf("overgrowth runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
