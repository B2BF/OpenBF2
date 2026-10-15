// Terrain editing interface returned by queryInterface(820005).
// [CHOSEN, not recovered] shared header usable by renderer and editor images.
// Method names and getter constness are [INFERRED]; const getters must remain distinct from
// Terrain's primary-interface getters. Preserve the 90-slot 1.50 virtual order.
// [CHOSEN, not recovered] base-name getters return HostString by value.
#ifndef BF2_GEOM_ITERRAINEDITABLE_H
#define BF2_GEOM_ITERRAINEDITABLE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"

#if defined(BF2_IMAGE_RENDDX9)
#include "../../RendDX9Abi/PluginAbi.h"       // renddx9abi::HostString
#else
#include <string>
#endif

namespace dice {
namespace hfe {
namespace geom {

class TerrainDetailTextureBase;               // Geom/Terrain.h - the 23-slot detail-texture record
class PatchDetailLayers;                      // Geom/QuadPatchEditable.h

// The string type every host-callable slot carries (see the header note).
#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString TerrainEditableString;
#else
typedef std::string TerrainEditableString;
#endif

// The base-name getters' result (see the above).
typedef TerrainEditableString TerrainEditableNameResult;          // [CHOSEN] native: by value

// IID_ITerrainEditable (Core/Ids.h) - dword_1021AE1C, the id TerrainEditable::queryInterface answers
// with this + 0x768.
const unsigned int kIID_ITerrainEditable = 820005u;


// Editable-terrain interface. Preserve the virtual order after the IBase quartet.

class ITerrainEditable : public IBase
{
public:
    // saving and baking (R3 / R2)
    // save: `path` is relative to the setting Editor_LevelSavePath (a '/' is appended when missing).
    // flags: 4 terraindata.raw + the detail / low-detail compiles (0x10 adds the colormap burns),
    // 1 Terrain.con, 2 the editor maps + buildMaterialMap, 8 +0x18; 0 = all, burns included.
    // False only when the Terrain.con write comes up short.
    virtual bool save(const TerrainEditableString& path, int flags) = 0;                  // +0x10
    virtual void burnAllColorMaps() = 0;                                                  // +0x14 0x101074F0  [INFERRED] name - burnColorMaps (+0x124) jumps here
    virtual void saveEditedColorMaps() = 0;                                               // +0x18 0x10107580  [INFERRED]
    virtual void saveLightmaps() = 0;                                                     // +0x1C 0x10107670  [INFERRED]
    virtual void buildMaterialMap() = 0;                                                  // +0x20 0x10109860  binding `buildMaterialMap`

    // settings
    virtual void  setLodSwitchDistance(float distance) = 0;                               // +0x24 0x1010DC70  binding `lodSwitchDistance`
    virtual float getLodSwitchDistance() const = 0;                                       // +0x28
    virtual void  setLowDetailTexture(const TerrainEditableString& path) = 0;             // +0x2C 0x101097B0  [INFERRED] name
    virtual void  setPrimaryWorldScale(const Vec3& scale) = 0;                            // +0x30 0x101A6600  binding `primaryWorldScale`
    virtual const Vec3& getPrimaryWorldScale() const = 0;                                 // +0x34
    virtual void  setSecondaryWorldScale(const Vec3& scale) = 0;                          // +0x38 0x101A6630  binding `secondaryWorldScale`
    virtual const Vec3& getSecondaryWorldScale() const = 0;                               // +0x3C
    virtual void  setUnresolved2C(float value) = 0;                                       // +0x40 0x101A5FE0  [UNRESOLVED] Terrain +0x2C, no binding
    virtual float getUnresolved2C() const = 0;                                            // +0x44
    virtual void  setPatchSize(int size) = 0;                                             // +0x48 0x101A6000  binding `patchSize`
    virtual int   getPatchSize() const = 0;                                               // +0x4C
    virtual void  setSubdividePatches(bool subdivide) = 0;                                // +0x50 0x101A6020  binding `subdividePatches`
    virtual bool  getSubdividePatches() const = 0;                                        // +0x54
    virtual void  setPatchColormapSize(int size) = 0;                                     // +0x58 0x101A6040  binding `patchColormapSize`
    virtual int   getPatchColormapSize() const = 0;                                       // +0x5C
    virtual void  setLowDetailmapSize(int size) = 0;                                      // +0x60 0x1010DC90  binding `lowDetailmapSize`
    virtual int   getLowDetailmapSize() const = 0;                                        // +0x64
    virtual void  setColormapBaseName(const TerrainEditableString& name) = 0;             // +0x68 0x101A6060  binding `colormapBaseName`
    virtual TerrainEditableNameResult getColormapBaseName() const = 0;                    // +0x6C
    virtual void  setLightmapBaseName(const TerrainEditableString& name) = 0;             // +0x70 0x101A60D0  binding `lightmapBaseName`
    virtual TerrainEditableNameResult getLightmapBaseName() const = 0;                    // +0x74
    virtual void  setDetailmapBaseName(const TerrainEditableString& name) = 0;            // +0x78 0x101A61D0  binding `detailmapBaseName`
    virtual TerrainEditableNameResult getDetailmapBaseName() const = 0;                   // +0x7C
    virtual void  setLowDetailmapBaseName(const TerrainEditableString& name) = 0;         // +0x80 0x101A6240  binding `lowDetailmapBaseName`
    virtual TerrainEditableNameResult getLowDetailmapBaseName() const = 0;                // +0x84
    virtual void  setShaderTechnique(int technique) = 0;                                  // +0x88 0x101A6560  binding `shaderTechnique`
    virtual int   getShaderTechnique() const = 0;                                         // +0x8C
    virtual TerrainEditableString getLevelSavePath() const = 0;                           // +0x90 0x101A6660  "Editor_LevelSavePath" + "/"
    virtual void  setDetailTextureName(int layer, const TerrainEditableString& name) = 0; // +0x94 0x101A6700  [INFERRED]
    virtual unsigned char getDetailMaterial(int layer) const = 0;                         // +0x98 0x101A6780  [INFERRED] record byte +0x14
    virtual void  setDetailMaterial(int layer, unsigned char material) = 0;               // +0x9C 0x101A67A0  [INFERRED]
    virtual TerrainDetailTextureBase* getDetailTexture(unsigned int layer) = 0;           // +0xA0 0x1010ACB0  accepts layer <= 6 (shipped)

    // Brush strength is an integer: positive paints, negative erases.
    virtual void paintDetail(float x, float z, float radius, int strength, int layer) = 0;            // +0xA4 0x1010A830  binding `paintDetail`
    virtual void paintColor(float x, float z, float radius, int strength, unsigned int color) = 0;    // +0xA8 0x10107930  [INFERRED]
    virtual void paintColorFromTexture(float x, float z, float radius, int strength, int layer) = 0;  // +0xAC 0x1010A930  [INFERRED]
    virtual void renderForLightmapGeneration() = 0;                                       // +0xB0 0x1010A5F0  [INFERRED] "lightmapGeneration"

    // layers and imports (R2)
    virtual void importBaseDetailLayers(TerrainEditableString tga) = 0;                   // +0xB4 0x1010A9E0  binding; by value (`retn 1Ch`)
    virtual void importDetailLayer(int layer, TerrainEditableString tga) = 0;             // +0xB8 0x1010AAD0  binding; `retn 20h`
    virtual void importAllDetailLayers(TerrainEditableString tga) = 0;                    // +0xBC 0x10107CA0  binding; logs "dont use this functions!"
    virtual void clearDetailLayer(int layer) = 0;                                         // +0xC0 0x10107D20  binding
    virtual void clearAllDetailLayers() = 0;                                              // +0xC4 0x10107D80  binding
    virtual void commitDetailLayers() = 0;                                                // +0xC8 0x10107E90  [INFERRED]
    virtual void revertDetailLayers() = 0;                                                // +0xCC 0x10107EE0  [INFERRED]
    virtual void importColorMap(TerrainEditableString tga) = 0;                           // +0xD0 0x1010ABC0  binding; `retn 1Ch`
    virtual void clearColorMapEdits() = 0;                                                // +0xD4 0x10107DD0  [INFERRED]
    virtual void commitColorMapEdits() = 0;                                               // +0xD8 0x10107E20  [INFERRED]
    virtual void debugTest(int value) = 0;                                                // +0xDC 0x101081B0  binding; per-patch body is empty
    virtual void setDrawDetailLayerAsColor(int layer) = 0;                                // +0xE0 0x1010DDA0  binding `drawDetailLayerAsColor`; -1 = off
    virtual PatchDetailLayers* getPatchMergedDetailLayers(int patchIndex) const = 0;      // +0xE4 0x10108430  [INFERRED] patch +0x25C
    virtual void* getPatchUndergrowthOverlay(int patchIndex) const = 0;                   // +0xE8 0x10108450  [INFERRED] patch +0x2E4 (technique 4)
    virtual void* getPatchOvergrowthOverlay(int patchIndex) const = 0;                    // +0xEC 0x10108470  [INFERRED] patch +0x2E8 (technique 5)
    virtual void* getPatchOvergrowthShadowOverlay(int patchIndex) const = 0;              // +0xF0 0x10108490  [INFERRED] patch +0x2EC (technique 6)

    // Invokes nvdxt with file, output directory, and option text passed by value.
    virtual void runNvdxt(TerrainEditableString file, TerrainEditableString outputDirectory,
                          TerrainEditableString options) = 0;                             // +0xF4 0x1010A130  [INFERRED]; three by value (`retn 54h`)
    virtual void convertToDxt1(TerrainEditableString file, TerrainEditableString outputDirectory) = 0;      // +0xF8 0x10108210  [INFERRED] "-dxt1c"
    virtual void makeUncompDDS(TerrainEditableString file, TerrainEditableString outputDirectory) = 0;      // +0xFC 0x10108270  binding; "-u8888"
    virtual void makeUncompDDSAgain(TerrainEditableString file, TerrainEditableString outputDirectory) = 0; // +0x100 0x101082D0 [UNRESOLVED] the slot 1.50 inserted; "-u8888"
    virtual void convertToU565(TerrainEditableString file, TerrainEditableString outputDirectory) = 0;      // +0x104 0x10108330 [INFERRED] "-u565"

    // surrounding patches (R2)
    virtual void removeSurroundingPatch(int slot) = 0;                                    // +0x108 0x10108560 binding
    virtual void createSurroundingPatch(int slot) = 0;                                    // +0x10C 0x101084F0 binding
    virtual void removeSurroundingAllPatchs() = 0;                                        // +0x110 0x101085B0 binding (shipped spelling)
    virtual void createSurroundingAllPatchs() = 0;                                        // +0x114 0x10108590 binding
    virtual void setSurroundingTerrainEngineMode(bool engineMode) = 0;                    // +0x118 0x101085D0 binding
    virtual bool getSurroundingTerrainEngineMode() const = 0;                             // +0x11C
    virtual void rebuildTerrain() = 0;                                                    // +0x120 0x10108640 binding
    virtual void burnColorMaps() = 0;                                                     // +0x124 0x1010DDC0 binding; `jmp [vtbl+14h]`
    virtual void setUseCompDetailMaps(bool use) = 0;                                      // +0x128 0x101086C0 binding
    virtual bool getUseCompDetailMaps() const = 0;                                        // +0x12C
    virtual void makeCompDetailMaps() = 0;                                                // +0x130 0x101086E0 binding
    virtual void removeCompDetailMaps() = 0;                                              // +0x134 0x10108720 binding

    // water colour and the nvdxt option strings
    virtual void setTerrainWaterColor(const Vec3& colour) = 0;                            // +0x138 0x1010DE40 binding `terrainWaterColor`
    virtual const Vec3& getTerrainWaterColor() const = 0;                                 // +0x13C
    virtual void setColormapSettings(TerrainEditableString settings) = 0;                 // +0x140 0x1010DCC0 binding; by value (`retn 1Ch`)
    virtual const TerrainEditableString& getColormapSettings() const = 0;                 // +0x144
    virtual void setLowDetailmapSettings(TerrainEditableString settings) = 0;             // +0x148 0x1010DCF0 binding
    virtual const TerrainEditableString& getLowDetailmapSettings() const = 0;             // +0x14C
    virtual void setDetailmapSettings(TerrainEditableString settings) = 0;                // +0x150 0x1010DD20 binding
    virtual const TerrainEditableString& getDetailmapSettings() const = 0;                // +0x154
    virtual void setSurroundingColormapSettings(TerrainEditableString settings) = 0;      // +0x158 0x1010DD50 binding `surroundingcolormapSettings`
    virtual const TerrainEditableString& getSurroundingColormapSettings() const = 0;      // +0x15C (ICF)
    virtual void setSurroundingLowDetailmapSettings(TerrainEditableString settings) = 0;  // +0x160 0x1010DD70 binding `surroundinglowDetailmapSettings`
    virtual const TerrainEditableString& getSurroundingLowDetailmapSettings() const = 0;  // +0x164

protected:
    ~ITerrainEditable() {}                    // never deleted through the interface - release() is
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ITERRAINEDITABLE_H
