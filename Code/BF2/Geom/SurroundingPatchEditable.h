
// Editable patch in the terrain's surrounding ring; owns its buffers and edited textures.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_SURROUNDINGPATCHEDITABLE_H
#define BF2_GEOM_SURROUNDINGPATCHEDITABLE_H

#include "QuadPatch.h"
#include "QuadPatchEditable.h"

#include <cstddef>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; }

namespace geom {

class TerrainEditable;

class SurroundingPatchEditable : public SurroundingPatch
{
public:
    explicit SurroundingPatchEditable(TerrainEditable* owner);
    virtual ~SurroundingPatchEditable();                                    // slot +0x00: 0x101A5F80 -> body 0x101A24E0
    // Slot +0x04 the ring patch's editor draw; TerrainEditable's ring draw (0x101098B0)
    // calls it with 0 after binding the shared ring buffers. The argument is not read.
    virtual void draw(int unused);

    void init(int slot);
    void setupEditorPlacement();
    void computeBounds();
    void buildVertexBuffers();
    // The sample writers buildVertexBuffers runs over the ring's grid blocks (inclusive ranges in
    // secondary-heightmap steps, a running cursor into the locked buffer). `flags` (heights only):
    // bit 0 averages the odd columns of the last row, bit 1 the odd rows of the last column - the
    // crack fix against the next coarser block. [INFERRED] names.
    void heightSubGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                       float* out, int* cursor, int flags);                 // (`retn 24h`)
    void normalSubGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                       Vec3* out, int* cursor, int flags);                  // (`retn 24h`; flags unread)
    void drawForLightmapGeneration();
    // TerrainLightSampling's read: (originX, 0, originZ) .. (endX, 0, endZ); only min and max are
    // written. [INFERRED] name.
    void getWorldBounds(TerrainAabb& box);
    void makeColormapEditCopy();
    void commitColormapEdit();
    void reloadLightmap();
    void paintColor(float x, float z, float radius, int strength, unsigned int color);
    void paintDetail(float x, float z, float radius, int strength, unsigned int color);
    void paintColorFromTexture(float x, float z, float radius, int strength, int layer);
    // The engine-mode switch's follow-up: the colormap edit committed, then the colormap, the lightmap
    // and the low-detail map re-oriented for the slot (skipped for slots 1 and 2).
    void onModeSwitch();
    void rotateTexture(dx9rend::EngineTexture** texture, bool engineMode);  // (`retn 8`)
    // R3: saving. [INFERRED] names.
    void saveLowDetailMap();
    void compileLowDetailMap();
    void saveEditedColormap();
    void burnColormap(const vc71::string& base);                            // (`retn 4`; `base` read only when not ready)

    // Owned editable textures; destruction releases low-detail/overlay resources before colormaps.
    dx9rend::EngineTexture* m_colormapEdit;        // +0x168  the brushes' edit copy (makeColormapEditCopy), committed by alpha
    dx9rend::EngineTexture* m_colormap;            // +0x16C  "<save><editor colormap base>_s%i", else 0xFF00FF00
    dx9rend::EngineTexture* m_lightmap;            // +0x170  "<save><editor lightmap base>_s%i", else white
    TerrainEditable*        editableTerrain;       // +0x174
    dx9rend::EngineTexture* m_texture178;          // +0x178  0 (init); no R2 reader or writer [UNRESOLVED]
    dx9rend::EngineTexture* m_texture17C;          // +0x17C  0 (init); no R2 reader or writer [UNRESOLVED]
    PatchDetailLayers       m_detailLayers;        // +0x180  (no R2 body touches the three sets)
    PatchDetailLayers       m_detailLayersEdit;    // +0x1FC
    MergedPatchDetailLayers m_mergedLayers;        // +0x278
    unsigned char           m_unresolved2F8[2];    // +0x2F8  not touched by the constructor [UNRESOLVED]
    // +0x2FA - set by init once the colormap exists; the colour brushes skip the patch without it.
    unsigned char           m_colormapReady;       // +0x2FA  [INFERRED] name
    // +0x2FB - the draw's pass: 1 selects pass 1. Its only writer is the R3 burn (0x101A5060), which
    // raises it around its own render and restores it. [INFERRED] name.
    unsigned char           m_burnPass;            // +0x2FB

private:
    SurroundingPatchEditable(const SurroundingPatchEditable&);
    SurroundingPatchEditable& operator=(const SurroundingPatchEditable&);
};

#if defined(_M_IX86)
static_assert(sizeof(SurroundingPatchEditable) == 0x2FC, "createSurroundingPatch allocates 0x2FC - see 0x101084F0");
static_assert(offsetof(SurroundingPatchEditable, m_colormapEdit) == 0x168, "+0x168");
static_assert(offsetof(SurroundingPatchEditable, m_colormap) == 0x16C, "+0x16C - init 0x101A4710");
static_assert(offsetof(SurroundingPatchEditable, m_lightmap) == 0x170, "+0x170");
static_assert(offsetof(SurroundingPatchEditable, editableTerrain) == 0x174, "owner at +0x174 - see 0x101A2413");
static_assert(offsetof(SurroundingPatchEditable, m_detailLayers) == 0x180, "+0x180 - see 0x101A2431");
static_assert(offsetof(SurroundingPatchEditable, m_detailLayersEdit) == 0x1FC, "+0x1FC - see 0x101A2455");
static_assert(offsetof(SurroundingPatchEditable, m_mergedLayers) == 0x278, "+0x278 - see 0x101A24A3");
static_assert(offsetof(SurroundingPatchEditable, m_colormapReady) == 0x2FA, "+0x2FA - see 0x101A24C8");
static_assert(offsetof(SurroundingPatchEditable, m_burnPass) == 0x2FB, "+0x2FB - see 0x101A21DF");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_SURROUNDINGPATCHEDITABLE_H
#endif // BF2_IMAGE_RENDDX9
