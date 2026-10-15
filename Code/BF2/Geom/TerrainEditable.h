// Terrain editing, rendering and serialization of 1.50 terrain data.
// [INFERRED] header placement. Preserve the shared Terrain prefix and interface base order.
// Game-image behavior retains the legacy missing grid/culler initialization, uninitialized
// parameter handles, unchecked patch allocation and patch ownership leaks.
// [CHOSEN, not recovered] editor-only avoidance of those hazards is gated by BF2_IMAGE_EDITOR.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_TERRAINEDITABLE_H
#define BF2_GEOM_TERRAINEDITABLE_H

#include "ITerrainEditable.h"
#include "Terrain.h"
#include "LightmapGeneration/ILightSampleable.h"
#include "../Game/IEventListener.h"
#include "../IO/Console/IConsoleSaveable.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include <string>

#include <stddef.h>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; }

namespace geom {

class QuadPatchEditable;
class SurroundingPatchEditable;

// The two ids queryInterface tests besides 1 / 820004 / 820005 (dword_10208DC0 / dword_1021AE6C).
const unsigned int kIID_IConsoleSaveable_TE = 0x3709A4FFu;    // Core/Ids.h IID_IConsoleSaveable
const unsigned int kIID_ILightSampleable_TE = 0xEBC881FBu;    // Core/Ids.h IID_ILightSampleable


// TerrainDetailTextureEditable - the 0x78-byte detail record TerrainEditable::init builds six of
// (the constructor is inlined at 0x1010ADC6; its dead out-of-line copy is 0x101087F0). Table
// the plain record's 23 slots with eleven overridden (Terrain.h has the table).
// [INFERRED] class name.

class TerrainDetailTextureEditable : public TerrainDetailTexture
{
public:
    TerrainDetailTextureEditable();
    virtual ~TerrainDetailTextureEditable();                                // +0x00 (deleting)
    virtual void          setName(std::string name);                       // +0x04
    virtual std::string  getName();                                        // +0x08
    virtual void          setColorTextureName(std::string name);           // +0x0C
    virtual std::string  getColorTextureName();                            // +0x10
    virtual void          setEditableInt68(int value);                      // +0x14
    virtual int           getEditableInt68();                               // +0x18
    virtual void          setColorTextureTiling(Vec2 tiling);               // +0x44
    virtual Vec2          getColorTextureTiling();                          // +0x48
    virtual void          setLowDetailType(int type);                       // +0x4C
    virtual int           getLowDetailType();                               // +0x50

    std::string            name;                // +0x2C
    std::string            colorTextureName;    // +0x48
    dx9rend::EngineTexture* colorTexture;        // +0x64  the paint-from-texture source
    int                     editableInt68;       // +0x68  [UNRESOLVED]
    Vec2                    colorTextureTiling;  // +0x6C  (1, 1)
    int                     lowDetailType;       // +0x74  0 none, 1 -> R, 2 -> B (QuadPatchEditable::rebuildLowDetailMap)
};


// TerrainEditable

class TerrainEditable : public Terrain,
                        public ITerrainEditable,                     // +0x768
                        public ::dice::hfe::io::IConsoleSaveable,    // +0x76C
                        public IEventListener,                       // +0x770
                        public ILightSampleable                      // +0x774
{
public:
    TerrainEditable();
    // the destructor body (non-virtual, like ~Terrain): slot +0xB0 calls it.
    ~TerrainEditable();

    // IBase - every subobject's quartet (the adjustor thunks are the compiler's)
    virtual void  addRef();                                                // P+0x00 0x1010DE10 (ICF, Terrain's)
    virtual int   getRef();                                                // P+0x04 0x1010DE20 (ICF)
    virtual int   release();                                               // P+0x08 0x1010DE30 `jmp` Terrain::release
    virtual void* queryInterface(unsigned int iid) const;                  // P+0x0C 0x1010DBF0

    // geom::Terrain primary slots TerrainEditable overrides
    virtual unsigned int getReflectionTypeId();                            // P+0x10 0x101A5FB0  821005 (GetSets)
    virtual void  init();                                                  // P+0x14 0x1010ADA0  replaces Terrain::init (does not call it)
    virtual void  update();                                                // P+0x28 0x1010C630
    virtual void  slot11_noop();                                           // P+0x2C 0x1010AD50  THE editor draw (Terrain's +0x2C is a `ret`)
    virtual float getPatchField9C(int index);                              // P+0x3C 0x10108390  patches[i]->minHeight
    virtual float getPatchField98(int index);                              // P+0x40 0x101083B0  patches[i]->maxHeight
    virtual void* getPatchColormap(int index);                             // P+0x44 0x101083D0
    virtual void* getPatchLightmap(int index);                             // P+0x48 0x101083F0
    virtual bool  getPatchHasNoDetail(int index);                          // P+0x4C 0x10108410
    virtual void* deletingDestructor(int flags);                           // P+0xB0 0x1010E310
    virtual void  renderWorldPassBegin();                                  // P+0xB8 0x1010DC60  -> R+0xC -> P+0x2C
    virtual void  renderWorldPassEnd();                                    // P+0xBC 0x10037D40  the shared `ret`
    virtual void  buildSurroundingGeometry();                              // P+0xCC 0x1010CEB0
    // Builds the 16-byte base-grid vertex buffer at +0xD0; the base class instead builds a table.
    virtual void  buildLegacyBaseGridVertexTable();
    // R+0x0C 0x101076D0 - `(this - 8)->P+0x2C()`: the render-path notification draws.
    virtual void  onRenderPathNotification(int path);

    // the primary slots TerrainEditable adds (+0xE4..+0x114; E06 names where a binding exists) --
    virtual void  burnRoadsInUndergrowth();                                // P+0xE4  0x101087B0  binding (R2, written)
    virtual void  burnRoadsInUndergrowthNeerest();                         // P+0xE8  0x10109CA0  binding (R2, written; shipped spelling)
    virtual std::string getColormapDirectory();                           // P+0xEC  0x101A6080  [INFERRED]
    virtual std::string getDetailmapDirectory();                          // P+0xF0  0x101A61F0
    virtual std::string getEditorDetailmapBaseName();                     // P+0xF4  0x101A63A0
    virtual std::string getEditorDetailmapDirectory();                    // P+0xF8  0x101A6410
    virtual std::string getEditorColormapBaseName();                      // P+0xFC  0x101A6480
    virtual std::string getEditorColormapDirectory();                     // P+0x100 0x101A64F0
    virtual std::string getEditorLightmapBaseName();                      // P+0x104 0x101A6160
    virtual std::string getEditorLightmapDirectory();                     // P+0x108 0x101A60F0
    virtual std::string getEditorLowDetailmapBaseName();                  // P+0x10C 0x101A62C0
    virtual std::string getEditorLowDetailmapDirectory();                 // P+0x110 0x101A6330
    // P+0x114 0x101076E0 (R2, written): the per-patch brush it reaches (0x1019B780) is empty in 1.50.
    // [UNRESOLVED] the fourth argument's type - it is forwarded as a raw dword and never read.
    virtual void  brushHeightmapPatches(float x, float z, float radius, float strength);

    // ITerrainEditable (+0x768)
    virtual bool  save(const TerrainEditableString& path, int flags);
    virtual void  burnAllColorMaps();
    virtual void  saveEditedColorMaps();
    virtual void  saveLightmaps();
    virtual void  buildMaterialMap();
    virtual void  setLodSwitchDistance(float distance);
    virtual float getLodSwitchDistance() const;
    virtual void  setLowDetailTexture(const TerrainEditableString& path);
    virtual void  setPrimaryWorldScale(const Vec3& scale);
    virtual const Vec3& getPrimaryWorldScale() const;
    virtual void  setSecondaryWorldScale(const Vec3& scale);
    virtual const Vec3& getSecondaryWorldScale() const;
    virtual void  setUnresolved2C(float value);
    virtual float getUnresolved2C() const;
    virtual void  setPatchSize(int size);
    virtual int   getPatchSize() const;
    virtual void  setSubdividePatches(bool subdivide);
    virtual bool  getSubdividePatches() const;
    virtual void  setPatchColormapSize(int size);
    virtual int   getPatchColormapSize() const;
    virtual void  setLowDetailmapSize(int size);
    virtual int   getLowDetailmapSize() const;
    virtual void  setColormapBaseName(const TerrainEditableString& name);
    virtual TerrainEditableNameResult getColormapBaseName() const;
    virtual void  setLightmapBaseName(const TerrainEditableString& name);
    virtual TerrainEditableNameResult getLightmapBaseName() const;
    virtual void  setDetailmapBaseName(const TerrainEditableString& name);
    virtual TerrainEditableNameResult getDetailmapBaseName() const;
    virtual void  setLowDetailmapBaseName(const TerrainEditableString& name);
    virtual TerrainEditableNameResult getLowDetailmapBaseName() const;
    virtual void  setShaderTechnique(int technique);
    virtual int   getShaderTechnique() const;
    virtual TerrainEditableString getLevelSavePath() const;
    virtual void  setDetailTextureName(int layer, const TerrainEditableString& name);
    virtual unsigned char getDetailMaterial(int layer) const;
    virtual void  setDetailMaterial(int layer, unsigned char material);
    virtual TerrainDetailTextureBase* getDetailTexture(unsigned int layer);
    virtual void  paintDetail(float x, float z, float radius, int strength, int layer);
    virtual void  paintColor(float x, float z, float radius, int strength, unsigned int color);
    virtual void  paintColorFromTexture(float x, float z, float radius, int strength, int layer);
    virtual void  renderForLightmapGeneration();
    virtual void  importBaseDetailLayers(TerrainEditableString tga);
    virtual void  importDetailLayer(int layer, TerrainEditableString tga);
    virtual void  importAllDetailLayers(TerrainEditableString tga);
    virtual void  clearDetailLayer(int layer);
    virtual void  clearAllDetailLayers();
    virtual void  commitDetailLayers();
    virtual void  revertDetailLayers();
    virtual void  importColorMap(TerrainEditableString tga);
    virtual void  clearColorMapEdits();
    virtual void  commitColorMapEdits();
    virtual void  debugTest(int value);
    virtual void  setDrawDetailLayerAsColor(int layer);
    virtual PatchDetailLayers* getPatchMergedDetailLayers(int patchIndex) const;
    virtual void* getPatchUndergrowthOverlay(int patchIndex) const;
    virtual void* getPatchOvergrowthOverlay(int patchIndex) const;
    virtual void* getPatchOvergrowthShadowOverlay(int patchIndex) const;
    virtual void  runNvdxt(TerrainEditableString file, TerrainEditableString outputDirectory,
                           TerrainEditableString options);
    virtual void  convertToDxt1(TerrainEditableString file, TerrainEditableString outputDirectory);
    virtual void  makeUncompDDS(TerrainEditableString file, TerrainEditableString outputDirectory);
    virtual void  makeUncompDDSAgain(TerrainEditableString file, TerrainEditableString outputDirectory);
    virtual void  convertToU565(TerrainEditableString file, TerrainEditableString outputDirectory);
    virtual void  removeSurroundingPatch(int slot);
    virtual void  createSurroundingPatch(int slot);
    virtual void  removeSurroundingAllPatchs();
    virtual void  createSurroundingAllPatchs();
    virtual void  setSurroundingTerrainEngineMode(bool engineMode);
    virtual bool  getSurroundingTerrainEngineMode() const;
    virtual void  rebuildTerrain();
    virtual void  burnColorMaps();
    virtual void  setUseCompDetailMaps(bool use);
    virtual bool  getUseCompDetailMaps() const;
    virtual void  makeCompDetailMaps();
    virtual void  removeCompDetailMaps();
    virtual void  setTerrainWaterColor(const Vec3& colour);
    virtual const Vec3& getTerrainWaterColor() const;
    virtual void  setColormapSettings(TerrainEditableString settings);
    virtual const TerrainEditableString& getColormapSettings() const;
    virtual void  setLowDetailmapSettings(TerrainEditableString settings);
    virtual const TerrainEditableString& getLowDetailmapSettings() const;
    virtual void  setDetailmapSettings(TerrainEditableString settings);
    virtual const TerrainEditableString& getDetailmapSettings() const;
    virtual void  setSurroundingColormapSettings(TerrainEditableString settings);
    virtual const TerrainEditableString& getSurroundingColormapSettings() const;
    virtual void  setSurroundingLowDetailmapSettings(TerrainEditableString settings);
    virtual const TerrainEditableString& getSurroundingLowDetailmapSettings() const;

    // io::IConsoleSaveable (+0x76C)
    virtual bool  makeScript(::dice::hfe::io::IStream* stream);           // 0x10107A00 -> false

    // IEventListener (+0x770)
    virtual void  handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data);

    // ILightSampleable (+0x774)
    virtual bool   isLightSampleable();
    // A TerrainLightSampler over every patch and ring patch (TerrainLightSampling.h), after
    // forcing the ring patches out of engine mode. Null when the sampler's init fails.
    virtual ILightSampling* createLightSampler(float resolutionModifier);
    virtual bool   destroyLightSampler(ILightSampling* sampler);

    // the TU's non-virtual bodies
    void reloadLowDetailMaps();                                            // 0x10107490  update's one-shot latch
    void applyEditorFrameConstants();
    void drawPatches();
    void drawEditorSurrounding();                                          // 0x101098B0  [INFERRED] name
    void drawEditorWater();                                                // 0x10109D80  [INFERRED] name
    void rebuildPatchesInRect(const float* rect);                          // 0x10109B40  world rect -> bounds + VBs
    void rebuildSurroundingInRect(const float* rect);
    void boundsAfterPrimaryScale();                                        // 0x101084B0  every patch's bounds
    std::string getLowDetailmapDirectory();                               // (GetSets; R3's compiles call it)
    // The technique helpers setShaderTechnique runs (0x10107F30 .. 0x10108160): each walks the patch
    // grid (z outer, x inner, patches[z * n + x]) and calls one QuadPatchEditable overlay body.
    void createUndergrowthOverlays();
    void releaseUndergrowthOverlays();
    void createOvergrowthOverlays();
    void releaseOvergrowthOverlays();
    void createOvergrowthShadowOverlays();
    void releaseOvergrowthShadowOverlays();
    void createMaterialOverlays();                                         // 0x10108110 -> 0x101A1AD0 (R2)
    void releaseMaterialOverlays();
    // R2: the external-tool launchers and the road-mask pass, non-virtual primary bodies.
    // The strings are by value (`retn 38h`), as shipped.
    void runTool(std::string executable, std::string arguments);          // 0x10109F10  CreateProcessA + wait (Esc aborts)
    void tgasplit(std::string tga, std::string outputDirectory);          // 0x1010A0B0  "tgasplit" 512 x 512 tiles
    void renderRoadMasks(int size);                                        // 0x10108760  every patch's 0x101A0680
    // R3: save()'s writers. Strings by reference (`retn 4` / `retn 8`). [INFERRED] names.
    void saveEditorMaps(const std::string& unused);                       // 0x101075E0  flag 2 (the argument is not read)
    void writeTerrainData(const std::string& folder, int flags);          // 0x1010C790  flag 4: terraindata.raw (+ 0x10 burns)
    void writePatchHeights(TerrainRawReader& raw, QuadPatchEditable* patch);           // 0x10108DB0  heights + morph deltas
    void writePatchLayersAndCompile(TerrainRawReader& raw, QuadPatchEditable* patch);  // 0x1010B4C0  index lists, DDS compiles

    // data, in shipped order (the four interface vptrs precede, +0x768..+0x774)
    dx9rend::EngineTexture*   m_gridTexture;              // +0x778  "BF2Editor/Content/Terrain/Grid" (technique 1's TEXLAYER1)
    dx9rend::EngineTexture*   m_grid3Texture;             // +0x77C  ".../Grid3" (technique 2)
    int                       m_shaderTechnique;          // +0x780  index into the technique table
    QuadPatchEditable**       m_editablePatches;          // +0x784  patchCount^2 (the 2006 m_QuadPatches)
    SurroundingPatchEditable* m_editableSurrounding[8];   // +0x788
    float                     m_lodSwitchDistanceCache;   // +0x7A8  0; the frame constants' change test
    float                     m_lodSwitchDistance;        // +0x7AC  75.0f
    float                     m_morphLimitScale;          // +0x7B0  1 / (far - near) - NOT initialised by the ctor
    float                     m_morphLimitOffset;         // +0x7B4  near * that       - NOT initialised by the ctor
    int                       m_drawDetailLayerAsColor;   // +0x7B8  -1
    unsigned char             m_surroundingTerrainEngineMode; // +0x7BC 0
    unsigned char             m_useCompDetailMaps;        // +0x7BD  0 (also blocks every brush)
    unsigned char             m_pad7BE[2];
    int                       m_roadSamplingSize;         // +0x7C0  4 [INFERRED] (E06 binding roadSamplingSize)
    unsigned char             m_showHeightmap;            // +0x7C4  0 [INFERRED] (E06 binding showHeightmap)
    unsigned char             m_pad7C5[3];
    // The nvdxt option strings. HostString: the getters hand the host a reference (see ITerrainEditable.h).
    TerrainEditableString     m_colormapSettings;               // +0x7C8 "-dxt1c -Kaiser -sharpenMethod SharpenMedium"
    TerrainEditableString     m_lowDetailmapSettings;           // +0x7E4 "-dxt1c -prescale 256 256 -Box"
    TerrainEditableString     m_detailmapSettings;              // +0x800 "-u565 -prescale 256 256 -Box"
    TerrainEditableString     m_surroundingColormapSettings;    // +0x81C as the colormap's
    TerrainEditableString     m_surroundingLowDetailmapSettings;// +0x838 as the low-detail map's

private:
    TerrainEditable(const TerrainEditable&);
    TerrainEditable& operator=(const TerrainEditable&);
};

#if defined(_M_IX86)
static_assert(offsetof(TerrainDetailTextureEditable, name) == 0x2C, "record name at +0x2C - see 0x1010DB47");
#endif
// The five settings strings are the host's std::string; the object never crosses the boundary by
// layout.

// RefFactory published by ModuleGeom::getClassList (class-list row 821005, the id
// dword_1021AEB0 that the reflection getter 0x101A5FB0 also reads).
IBase* __fastcall createTerrainEditable(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_TERRAINEDITABLE_H
#endif // BF2_IMAGE_RENDDX9
