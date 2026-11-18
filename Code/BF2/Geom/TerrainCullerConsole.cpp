// [INFERRED] console placement; the retail rows belong to ModuleGeom's initializer list.
#ifdef BF2_IMAGE_RENDDX9

#include "QuadPatch.h"
#include "Terrain.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif

namespace dice {
namespace hfe {
namespace geom {
namespace {

const char kTerrainCullerConsoleTag[] = "geom.terrainCuller";

// PE 0x100B0CFE.
TerrainCuller* consoleTerrainCuller()
{
    return g_Terrain ? static_cast<Terrain*>(g_Terrain)->m_culler : 0;
}

} // namespace

// PE 0x100E2EF0. Also called directly by VideoOptions' three terrain-quality arms (100, 80, 64).
void RD9_THUNK setCullerLodSwitchDistance(TerrainCuller* self RD9_THUNK_EDX, int value)
{
    if (value < self->param14 || value > 1000)
        return;
    self->terrain->m_lightingDirty = 1;
    self->param18 = value;
}

namespace {

// PE 0x100E2F10.
void RD9_THUNK setCullerLod0Start(TerrainCuller* self RD9_THUNK_EDX, float value)
{
    self->lodFactor20 = value;
    self->terrain->m_lightingDirty = 1;
}

// PE 0x100E2F30.
void RD9_THUNK setCullerLod0Stop(TerrainCuller* self RD9_THUNK_EDX, float value)
{
    self->lodFactor24 = value;
    self->terrain->m_lightingDirty = 1;
}

// PE 0x100E2F50.
void RD9_THUNK setCullerLod1Start(TerrainCuller* self RD9_THUNK_EDX, float value)
{
    self->lodFactor28 = value;
    self->terrain->m_lightingDirty = 1;
}

// PE 0x100E2F70.
void RD9_THUNK setCullerLod1Stop(TerrainCuller* self RD9_THUNK_EDX, float value)
{
    self->lodFactor2C = value;
    self->terrain->m_lightingDirty = 1;
}

// PE 0x100E2F90.
void RD9_THUNK setCullerLod2Start(TerrainCuller* self RD9_THUNK_EDX, float value)
{
    self->lodFactor30 = value;
    self->terrain->m_lightingDirty = 1;
}

// PE 0x100E3400. Shipped defect: the initial node is dereferenced without a null check.
void RD9_THUNK updateCullerBaseCells(TerrainCuller* self RD9_THUNK_EDX, TerrainCullerNode* node)
{
    do
    {
        if (node->bounds.dirty)
            node->bounds.refresh();
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] FUCOM compares the integer without a binary32 narrowing.
        node->fileByteEE = bf2x87::wideExact(node->bounds.extent.x) == bf2x87::wideExact(self->param14);
#else
        node->fileByteEE = node->bounds.extent.x == self->param14;
#endif
        if (node->child[0])
            updateCullerBaseCells(self RD9_THUNK_ARG, node->child[0]);
        if (node->child[1])
            updateCullerBaseCells(self RD9_THUNK_ARG, node->child[1]);
        if (node->child[2])
            updateCullerBaseCells(self RD9_THUNK_ARG, node->child[2]);
        node = node->child[3];
    } while (node);
}

// PE 0x100E3480.
void RD9_THUNK setCullerBaseCellSize(TerrainCuller* self RD9_THUNK_EDX, int value)
{
    if (value == self->param14 || value < self->param10)
        return;
    Terrain* const owner = self->terrain;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep the retained x87 product and FCOM unordered result.
    const bf2x87::Real maximum = bf2x87::mul(bf2x87::wide(owner->m_patchSize),
                                           bf2x87::wide(owner->m_primaryWorldScale.x));
    if (bf2x87::less(maximum, bf2x87::wideExact(value)))
        return;
#else
    if (owner->m_patchSize * owner->m_primaryWorldScale.x < value)
        return;
#endif
    if (value != 512 && value != 256 && value != 128
        && value != 64 && value != 32 && value != 16)
        return;
    TerrainCullerNode* const nodes = self->nodes;
    self->param14 = value;
    updateCullerBaseCells(self RD9_THUNK_ARG, nodes);
}

// The getter (`mov al, [ecx+0x78]`, versionFlag78) is read inline by the useStitchedLods binding below.
// PE 0x100E2FB0, 0x100E2FE0: the setter, then the getter.
void RD9_THUNK setCullerStitchedLods(TerrainCuller* self RD9_THUNK_EDX, bool value)
{
    Terrain* const owner = self->terrain;
    if (owner->m_rawHasVersion >= 1 && owner->m_rawVersion >= 0x1A)
        self->versionFlag78 = value;
}

// Console setter adapters keep the executable's repeated owner lookup at execution time.
void consoleLodSwitchDistance(int value) { setCullerLodSwitchDistance(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleLod0Start(float value) { setCullerLod0Start(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleLod0Stop(float value) { setCullerLod0Stop(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleLod1Start(float value) { setCullerLod1Start(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleLod1Stop(float value) { setCullerLod1Stop(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleLod2Start(float value) { setCullerLod2Start(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleBaseCellSize(int value) { setCullerBaseCellSize(consoleTerrainCuller() RD9_THUNK_ARG, value); }
void consoleStitchedLods(bool value) { setCullerStitchedLods(consoleTerrainCuller() RD9_THUNK_ARG, value); }

BF2_CONSOLE_OBJECT("terrainCuller", 2, 0)

// PE 0x100B6AB8.
// PE 0x100B69FA, 0x100B6A6B
BF2_CONSOLE_PROPERTY_FN(lodSwitchDistance, int, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->param18, consoleLodSwitchDistance, kTerrainCullerConsoleTag)
// PE 0x100B6C15 / 0x100B6D18 / 0x100B6E1B / 0x100B6F1E / 0x100B7021.
// PE 0x100B6AFA, 0x100B6BC8
BF2_CONSOLE_PROPERTY_FN(lod0SwitchStart, float, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->lodFactor20, consoleLod0Start, kTerrainCullerConsoleTag)
// PE 0x100B6C5A, 0x100B6CCB
BF2_CONSOLE_PROPERTY_FN(lod0SwitchStop, float, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->lodFactor24, consoleLod0Stop, kTerrainCullerConsoleTag)
// PE 0x100B6D5D, 0x100B6DCE
BF2_CONSOLE_PROPERTY_FN(lod1SwitchStart, float, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->lodFactor28, consoleLod1Start, kTerrainCullerConsoleTag)
// PE 0x100B6E60, 0x100B6ED1
BF2_CONSOLE_PROPERTY_FN(lod1SwitchStop, float, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->lodFactor2C, consoleLod1Stop, kTerrainCullerConsoleTag)
// PE 0x100B6F63, 0x100B6FD4
BF2_CONSOLE_PROPERTY_FN(lod2Start, float, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->lodFactor30, consoleLod2Start, kTerrainCullerConsoleTag)
// PE 0x100B7124 / 0x100B721F.
// PE 0x100B7066, 0x100B70D7
BF2_CONSOLE_FIELD(detailCullMaxLevel, int, consoleTerrainCuller(), param68, kTerrainCullerConsoleTag)
// PE 0x100B7161, 0x100B71D2
BF2_CONSOLE_FIELD(cullerType, int, consoleTerrainCuller(), param6C, kTerrainCullerConsoleTag)
// PE 0x100B731A.
// PE 0x100B725C, 0x100B72CD
BF2_CONSOLE_PROPERTY_FN(baseCellSize, int, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->param14, consoleBaseCellSize, kTerrainCullerConsoleTag)
// PE 0x100B741A / 0x100B7515 / 0x100B761C.
// PE 0x100B735C, 0x100B73CD
BF2_CONSOLE_FIELD(updateLodLevel, bool, consoleTerrainCuller(), flag70, kTerrainCullerConsoleTag)
// PE 0x100B7457, 0x100B74C8
BF2_CONSOLE_FIELD(checkInside, bool, consoleTerrainCuller(), flag71, kTerrainCullerConsoleTag)
// PE 0x100B7552, 0x100B75CF
BF2_CONSOLE_PROPERTY_FN(useStitchedLods, bool, consoleTerrainCuller() != 0,
                        consoleTerrainCuller()->versionFlag78 != 0, consoleStitchedLods, kTerrainCullerConsoleTag)

} // anonymous namespace

bool registerTerrainCullerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(consoleBindingList(kTerrainCullerConsoleTag));
}

bool unregisterTerrainCullerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kTerrainCullerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice
#endif // BF2_IMAGE_RENDDX9
