
// Editable terrain patches with six detail-weight maps, brushes and texture export.
// Merged layers borrow source maps until their own maps have been created.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_QUADPATCHEDITABLE_H
#define BF2_GEOM_QUADPATCHEDITABLE_H

#include "QuadPatch.h"
#include <string>

#include <cstddef>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; }

namespace geom {

class TerrainEditable;
class QuadPatchEditable;


// The colour helpers the patch and ring-patch brushes share (QuadPatchEditable.cpp; the ring patch's
// SurroundingPatchEditable.cpp calls the same three bodies). [INFERRED] names; the four floats are
// ordered like the A8R8G8B8 texel's bytes from the low one up (blue, green, red, alpha).
//   pack: each float * 255.0f (flt_101E14C0), chopped to int, the LOW byte kept - no
//                clamp, so 1.2 wraps and a negative wraps through 0xFF; four floats BY value.
//   unpack: each byte * (1 / 255) (flt_101E2B98) - a multiply, not a divide.
//   lerp in place: all four of dst scaled by (1 - t) first, then t * src added.

unsigned int terrainEditPackColor(float c0, float c1, float c2, float c3);
void         terrainEditUnpackColor(float out[4], unsigned int color);
void         terrainEditLerpColor(float dst[4], const float src[4], float t);


// PatchDetailLayer - one detail layer's 8-bit weight map over a patch, `width` x `width` texels
// (Terrain +0x44, the detail-map size). No table. [INFERRED] field names.

struct PatchDetailLayer
{
    PatchDetailLayer();                                   // created = 0, data = 0
    ~PatchDetailLayer();                                  // delete[] data, data = 0
    void init(QuadPatchEditable* owner, int width);       // (inlined by PatchDetailLayers::init)
    void create();
    void assertCreated();                                 // a logged "TBM", never a stop
    unsigned char* at(int position);
    void  clearQuad(int x, int z);                        // one heightmap quad's texels to 0
    float averageWeight(int x, int z);
    bool  anyWeight(int x, int z);                        // the quad and a one-texel border

    unsigned char      created;     // +0x00
    unsigned char      pad01[3];
    // +0x04 - the patch the map belongs to: QuadPatchEditable::init stores `this`. Every body reads the
    // terrain through owner +0x124 (0x1019B04B, 0x1019BB16, 0x1019BC6C). [INFERRED] for the surrounding
    // patches, whose init (0x101A4710) is R2.
    QuadPatchEditable* owner;
    unsigned char*     data;        // +0x08  byteSize bytes, operator new[] (sub_1005F340)
    int                byteSize;    // +0x0C  width * width
    int                width;       // +0x10
};


// PatchDetailLayers - the six detail layers of a patch (the editable terrain draws six). No
// destructor slot in the table: the class has the implicit non-virtual destructor, and the owners'
// destructors run the six record destructors in reverse (0x1019C190 inlines them, 0x101A24E0 calls
// 0x1019B000). Slot names [INFERRED] from the bodies.

class PatchDetailLayers
{
public:
    PatchDetailLayers() {}                                                     // the six record ctors (inlined)

    virtual void          init(QuadPatchEditable* owner, int width);           // +0x00
    virtual bool          isLayerSet(int layer, int x, int z);                  // +0x04
    virtual unsigned char layerMask3(int x, int z);                             // +0x08 at most three layers, 5..3 then 0..2
    virtual unsigned char dominantLayer(int x, int z);                          // +0x0C
    virtual unsigned char layerMask(int x, int z);                              // +0x10
    virtual bool          hasLayer(int layer, int x, int z);                    // +0x14
    virtual void          clearLayerQuad(int layer, int x, int z);              // +0x18
    virtual void          freeAll();                                            // +0x1C
    virtual void          createAll();                                          // +0x20
    virtual void          clearAll();                                           // +0x24
    virtual void          copyFrom(const PatchDetailLayers* other);             // +0x28
    virtual bool          areAllCreated();                                      // +0x2C

    // non-virtual. The guard (`layer < 0 || layer > 6`, signed - as in hasLayer and
    // clearLayerQuad) accepts layer 6, one past the six records, and only logs: the address is
    // answered for any layer.
    PatchDetailLayer* getLayer(int layer);

    PatchDetailLayer records[6];                                                // +0x04
};


// MergedPatchDetailLayers - the patch's merged view (+0x25C): while its own six maps are not all
// created, every query is answered by `source` (the imported set, +0x164); afterwards by its own maps.

class MergedPatchDetailLayers : public PatchDetailLayers
{
public:
    MergedPatchDetailLayers() {}                                                // `source` is NOT initialised (QuadPatchEditable::init sets it)

    virtual void          init(QuadPatchEditable* owner, int width);           // +0x00 refuses (logs QuadPatchEditable.h:107)
    virtual bool          isLayerSet(int layer, int x, int z);                  // +0x04
    virtual unsigned char layerMask3(int x, int z);                             // +0x08
    virtual unsigned char dominantLayer(int x, int z);                          // +0x0C
    virtual unsigned char layerMask(int x, int z);                              // +0x10
    virtual bool          hasLayer(int layer, int x, int z);                    // +0x14
    virtual void          clearLayerQuad(int layer, int x, int z);              // +0x18

    // non-virtual, hides the base's: the source set's record while this set's maps are
    // not all created, else this set's own. (Its one caller, 0x1010B575, is an R2 brush.)
    PatchDetailLayer* getLayer(int layer);

    PatchDetailLayers* source;                                                  // +0x7C
};


// QuadPatchEditable - one primary-heightmap patch of the editable terrain.

class QuadPatchEditable : public QuadPatch
{
public:
    explicit QuadPatchEditable(TerrainEditable* owner);
    virtual ~QuadPatchEditable();                                          // slot +0x00: 0x101A1E60 -> body 0x1019C190
    // Slot +0x04 the patch's editor draw, one call per visible patch from
    // TerrainEditable::drawPatches. The argument is pushed as 0 by every caller and never read.
    virtual void draw(int unused);

    void init(int gridX, int gridZ);
    void computeBounds();                                                  // heights, sphere
    void rebuildVertexBuffers();                                           // heights (+0x54), normals (+0x5C)
    void importAllLayers();
    void importLayer(int layer);                                           // detail map channel -> weight map
    void rebuildLayerIndexBuffer(int layer);
    void rebuildLowDetailMap();
    void debugTest(int value);                                             // (nullsub_12, `retn 4`)
    // The technique overlays TerrainEditable::setShaderTechnique creates and releases (techniques 4 / 5 /
    // 6 / 8). The three creates make a patchSize^2 A8R8G8B8 texture cleared to 0.
    void createUndergrowthOverlay();                                       // (+0x2E4)
    void releaseUndergrowthOverlay();
    void createOvergrowthOverlay();                                        // (+0x2E8)
    void releaseOvergrowthOverlay();
    void createOvergrowthShadowOverlay();                                  // (+0x2EC)
    void releaseOvergrowthShadowOverlay();
    void createMaterialOverlay();                                          // (R2)
    void releaseMaterialOverlay();                                         // (+0x2F0)

    // R2: the editing bodies TerrainEditable's slots reach
    void makeColormapEditCopy();                                           // (+0x120 from the colormap)
    void releaseColormapEdit();
    void clearDetailLayer(int layer);
    void clearAllDetailLayers();
    void clearColormapEdits();
    void brushHeightmap(float x, float z, float radius, float strength);   // (nullsub_11, `retn 10h`)
    void writeLayersBack(int x0, int x1, int z0, int z1, int layer);       // (the layer is not read)
    void paintDetail(float x, float z, float radius, int strength, int layer);
    void drawForLightmapGeneration();
    void importBaseLayers(const std::string& directory);
    void importLayerTile(int layer, const std::string& directory);
    void importColormapTile(const std::string& directory);
    void commitColormapEdit();
    void revertLayer(int layer);
    void revertAllLayers();
    void releaseRoadMask();
    void reloadLightmap();
    void paintColor(float x, float z, float radius, int strength, unsigned int color);
    // The two TerrainLightSampling reads. [INFERRED] names.
    // getFileIndices: (heightmapSize / patchSize) / 2 + grid position per axis, the "%02ix%02i" pair.
    // getWorldBounds: the patch's world box at height 0; only min and max are written.
    void getFileIndices(int* x, int* z);
    void getWorldBounds(TerrainAabb& bounds);
    void renderRoadMask(int size);                                         // (+0x2F4)
    void burnRoadsInUndergrowth();
    void buildMaterialMap();
    void paintColorFromTexture(float x, float z, float radius, int strength, int layer);
    void commitLayer(int layer);                                           // (the layer is not read)
    void commitAllLayers();

    // R3: saving. Strings by reference (`retn 4`). [INFERRED] names.
    void burnColormap(const std::string& base);                           // (`base` read only when not edited)
    void saveDetailMaps(const std::string& suffix);
    void saveLightmap(const std::string& base);
    void removeCompDetailMaps();
    void saveEditedColormap();
    void makeCompDetailMaps();

    // Owned colormap edit copy, used by brushes and technique 3 until committed and released.
    // [INFERRED] name; the texture is patchColormapSize squared, A8R8G8B8.
    dx9rend::EngineTexture* m_colormapEdit;          // +0x120
    TerrainEditable*        editableTerrain;         // +0x124  the owner, typed (QuadPatch::terrain holds the same object)
    // +0x128..+0x13C - the checkerboard fill's state (D3DXFillTexture's pData, 0x1019F889): the last
    // texel coordinate, the last cell start and the cell counters per axis.
    float                   m_checkerLastU;          // +0x128
    float                   m_checkerLastV;          // +0x12C
    float                   m_checkerCellU;          // +0x130
    float                   m_checkerCellV;          // +0x134
    int                     m_checkerCountU;         // +0x138
    int                     m_checkerCountV;         // +0x13C
    int                     m_fileIndexX;            // +0x140  gridX + patchCount / 2 - the "%02ix%02i" file name
    int                     m_fileIndexZ;            // +0x144
    std::string            m_fileName;              // +0x148  "%02ix%02i"
    PatchDetailLayers       m_detailLayers;          // +0x164  the imported weight maps
    PatchDetailLayers       m_detailLayersEdit;      // +0x1E0  the edited working copy (R2) [INFERRED]
    MergedPatchDetailLayers m_mergedLayers;          // +0x25C  source = +0x164 until its own maps exist
    dx9rend::EngineTexture* m_compDetailMap[2];      // +0x2DC  the compressed detail maps (R3's 0x101A0310 / 0x1019F120; never released here)
    dx9rend::EngineTexture* m_undergrowthOverlay;    // +0x2E4  technique 4
    dx9rend::EngineTexture* m_overgrowthOverlay;     // +0x2E8  technique 5
    dx9rend::EngineTexture* m_overgrowthShadowOverlay; // +0x2EC technique 6
    dx9rend::EngineTexture* m_materialOverlay;       // +0x2F0  technique 8
    // Downsampled orthographic road mask, consumed and released by material/undergrowth updates.
    dx9rend::EngineTexture* m_roadMask;              // +0x2F4
    void*                   m_layerIndexBuffer[6];   // +0x2F8  6 * patchSize^2 u16 each, the layer's triangles
    int                     m_layerTriangleCount[6]; // +0x310
    unsigned char           m_detailMapsFromFile;    // +0x328  a "<base>_1/_2" detail map was loaded (NOT set by the ctor)
    unsigned char           m_importPending;         // +0x329  update() imports the layers while set
    unsigned char           m_colormapEdited;        // +0x32A  the colormap came from the editor path or is a fresh checkerboard
                                                     //         (every brush and commitLayer require it - R2)
    unsigned char           m_pad32B;

private:
    // [CHOSEN, not recovered] one body for the weight arithmetic the detail brush (0x1019D5D0) and the
    // tile import (0x1019E650) each inline: `amount` added to `layer` in the edit set at `position`,
    // the other edit layers rescaled, then the merged set rebuilt from the edit set or the imported
    // set. The import only ever passes a positive amount. See the body.
    void addLayerWeight(int layer, int position, int amount);

    QuadPatchEditable(const QuadPatchEditable&);
    QuadPatchEditable& operator=(const QuadPatchEditable&);
};

#if defined(_M_IX86)
static_assert(sizeof(PatchDetailLayer) == 0x14, "a weight map record is 20 bytes - see 0x1019B1A0");
static_assert(offsetof(PatchDetailLayer, data) == 0x08, "record data at +0x08");
static_assert(offsetof(PatchDetailLayer, byteSize) == 0x0C, "record size at +0x0C");
static_assert(sizeof(PatchDetailLayers) == 0x7C, "six records behind a vptr");
static_assert(offsetof(PatchDetailLayers, records) == 0x04, "records at +0x04");
static_assert(sizeof(MergedPatchDetailLayers) == 0x80, "the merged set's source at +0x7C");
static_assert(offsetof(MergedPatchDetailLayers, source) == 0x7C, "source at +0x7C - QuadPatchEditable::init (0x1019F530) stores +0x2D8");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_QUADPATCHEDITABLE_H
#endif // BF2_IMAGE_RENDDX9
