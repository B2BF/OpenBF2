// Path:     [INFERRED] header beside the attested Geom/XpLightManager.cpp implementation.
// Contents: evidence-scoped Win32 view of dice::hfe::geom::LightManager.  The retail primary
//           interface has 189 slots. Recovered operations are ordinary C++ member functions;
//           the still-unknown slots are pure, non-callable position markers for the separately
//           materialized host ABI table in XpLightManager.cpp.

#ifndef GEOM_XPLIGHTMANAGER_H
#define GEOM_XPLIGHTMANAGER_H

#if defined(BF2_IMAGE_RENDDX9)
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#else
#include "../Core/IBase.h"
#include <string>
#endif

#include "../Math/Vec3.h"

// The three light-gather slots take the DLL's own VC7.1 vector layout by reference.  The type is
// only ever named by reference here, so the dedicated build - which never includes the RendDX9Abi
// headers and never calls those slots - needs no more than this declaration. The ground-hemi getter
// (+0x48) returns the renderer's own VC7.1 string by reference on the same terms.
namespace vc71 { struct vector_layout; class string; }
struct IDirect3DSurface9;   // lightManagerSetDepthStencil

namespace dice {
namespace hfe {

namespace world { class LightSource; }
namespace dx9rend { class Effect; struct EngineTexture; }

namespace geom {

// The ground-hemi setter (+0x44) takes the host's string, like HemiMapManager::setBaseHemiMap it
// forwards to (HemiMapName): the retail VC7.1 layout in the compatibility flavour, the rebuilt
// host's std::string in the native one, and std::string in the images that only declare the slot.
#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString LightManagerHostString;
#else
typedef std::string LightManagerHostString;
#endif

#define BF2_LIGHT_RESERVED(offset) virtual void reserved_##offset() = 0

// Light manager interface. Reserved virtuals preserve positions and must not be called.
// The host boundary retains the full explicit table; unknown entries log their offset and abort.
class LightManager : public IBase
{
public:
    void addRef() override;                                                // +0x000
    int getRef() override;                                                 // +0x004
    int release() override;                                                // +0x008
    void* queryInterface(unsigned int iid) const override;                 // +0x00C

    virtual void update(double time);                                      // +0x010
    // +0x014  PE 0x100F2E60.  Every owned light (the +0x1B8 set, in key order) that satisfies
    // every bit of `required` and violates none of `forbidden` (lightMatchesMask in the .cpp),
    // appended to `out`.
    virtual void getLightsByMask(vc71::vector_layout& out, unsigned int required,
                                 unsigned int forbidden);                   // +0x014
    // [INFERRED] Appends visible lights in frame order. required == 0x7FFFFFFF ignores forbidden
    // and selects enabled, nondirectional lights whose templates support emitters.
    virtual void getVisibleLightsByMask(vc71::vector_layout& out, unsigned int required,
                                        unsigned int forbidden);            // +0x018
    // +0x01C  PE 0x100F3890.  The lights of this frame's scratch list (+0x1D0) within `radius` of
    // `position` - each light's bounding radius widening the test - filtered by the two masks,
    // sorted nearest first, ray-tested against the world when `occlusionTest`, and appended to
    // `out` until it holds `maxLights`.
    virtual void gatherLights(const Vec3& position, float radius, vc71::vector_layout& out,
                              unsigned int maxLights, unsigned int required,
                              unsigned int forbidden, bool occlusionTest);  // +0x01C
    // +0x020  PE 0x100F1750.  gatherLights at a geometry instance's transform translation with
    // that instance's bounding radius.  The StaticMesh draw's entry point.
    virtual void gatherLightsForGeometry(void* geometry, vc71::vector_layout& out,
                                         unsigned int maxLights, unsigned int required,
                                         unsigned int forbidden, bool occlusionTest); // +0x020
    virtual void setSkyLight(world::LightSource* light);                   // +0x024
    virtual world::LightSource* getSkyLight() const;                       // +0x028
    virtual void setSkyColor(const Vec3& color);                           // +0x02C
    virtual const Vec3& getSkyColor() const;                               // +0x030
    virtual void setAmbientColor(const Vec3& color);                       // +0x034
    virtual const Vec3& getAmbientColor() const;                           // +0x038
    virtual void setTreeAmbientColor(const Vec3& color);                   // +0x03C
    virtual const Vec3& getTreeAmbientColor() const;                       // +0x040
    // +0x044  PE 0x1015C9E0 (`retn 4`), Ignores `this`: the settings
    // repository's GLSWorldSizeX (default 2048) as the range, origin 0/0/0 and falloff 500, handed to
    // the published HemiMapManager's setBaseHemiMap with no null guard. `LightManager.groundHemi` SET.
    virtual void setGroundHemi(const LightManagerHostString& name);        // +0x044
    // +0x048  the base hemi map's name (HemiMap +0x18), or a function-local static
    // empty string while there is none. `LightManager.groundHemi` GET. Constness [INFERRED].
    virtual const vc71::string& getGroundHemi() const;                     // +0x048

    // Both bodies ignore this and call the published HemiMapManager
    // singleton without a null guard. ModuleGeom publication order is therefore observable.
    virtual void setHemiLerpBias(float bias);                              // +0x04C
    virtual float getHemiLerpBias() const;                                 // +0x050
    virtual void setLightCullDistance(float distance);                     // +0x054
    virtual float getLightCullDistance() const;                            // +0x058
    virtual void setMaxLightsPerBundledMesh(int quality);                  // +0x05C
    virtual int getMaxLightsPerBundledMesh() const;                        // +0x060
    virtual void setMaxLightsPerStaticMesh(int quality);                   // +0x064
    virtual int getMaxLightsPerStaticMesh() const;                         // +0x068
    virtual void setMaxLightsPerSkinnedMesh(int quality);                  // +0x06C
    virtual int getMaxLightsPerSkinnedMesh() const;                        // +0x070
    virtual void setMaxLightsPerTerrainPatch(int quality);                 // +0x074
    virtual int getMaxLightsPerTerrainPatch() const;                       // +0x078
    // an unclamped float at +0x1B4. The console executor
    // 0x100BF5D3 names the property skinnedMeshShaderLodDistance and proves both call shapes.
    // Getter constness follows this interface's read-only accessor convention [INFERRED].
    virtual void setSkinnedMeshShaderLodDistance(float distance);           // +0x07C
    virtual float getSkinnedMeshShaderLodDistance() const;                  // +0x080
    // The luminance range (constructor 0.1 .. 0.5, field +0x1FC) the mesh light sampler's colour
    // packing maps onto the lightmap's alpha. `LightManager.lightMapAmbientAlphaCutoffRange` GET and
    // SET; in this pair the getter precedes the setter. Getter constness [INFERRED].
    virtual const Vec2& getLightMapAmbientAlphaCutoffRange() const;         // +0x084
    virtual void setLightMapAmbientAlphaCutoffRange(const Vec2& range);     // +0x088
    virtual void addLight(world::LightSource* light);                      // +0x08C
    virtual void removeLight(world::LightSource* light);                   // +0x090
    virtual void enableLight(world::LightSource* light);                   // +0x094
    virtual void disableLight(world::LightSource* light);                  // +0x098
    BF2_LIGHT_RESERVED(09C); BF2_LIGHT_RESERVED(0A0);
    BF2_LIGHT_RESERVED(0A4); BF2_LIGHT_RESERVED(0A8);
    BF2_LIGHT_RESERVED(0AC); BF2_LIGHT_RESERVED(0B0);
    BF2_LIGHT_RESERVED(0B4); BF2_LIGHT_RESERVED(0B8);
    BF2_LIGHT_RESERVED(0BC); BF2_LIGHT_RESERVED(0C0);
    virtual void setShadowStaticFeature(bool enabled);                     // +0x0C4
    virtual bool getShadowStaticFeature() const;                           // +0x0C8
    BF2_LIGHT_RESERVED(0CC); BF2_LIGHT_RESERVED(0D0);
    virtual void setShadowIntensityBias(float value);                      // +0x0D4
    virtual float getShadowIntensityBias() const;                          // +0x0D8
    virtual void setLightMapIntensityBias(float value);                    // +0x0DC
    virtual float getLightMapIntensityBias() const;                        // +0x0E0
    BF2_LIGHT_RESERVED(0E4); BF2_LIGHT_RESERVED(0E8);
    virtual void setDynamicShadowsEnabled(bool enabled);                   // +0x0EC
    virtual bool getDynamicShadowsEnabled() const;                         // +0x0F0
    BF2_LIGHT_RESERVED(0F4); BF2_LIGHT_RESERVED(0F8);
    BF2_LIGHT_RESERVED(0FC); BF2_LIGHT_RESERVED(100);
    BF2_LIGHT_RESERVED(104); BF2_LIGHT_RESERVED(108);
    BF2_LIGHT_RESERVED(10C); BF2_LIGHT_RESERVED(110);
    BF2_LIGHT_RESERVED(114); BF2_LIGHT_RESERVED(118);
    BF2_LIGHT_RESERVED(11C); BF2_LIGHT_RESERVED(120);
    virtual void setShadowUpdatesEnabled(bool enabled);                    // +0x124
    virtual bool getShadowUpdatesEnabled() const;                          // +0x128
    BF2_LIGHT_RESERVED(12C); BF2_LIGHT_RESERVED(130);
    BF2_LIGHT_RESERVED(134); BF2_LIGHT_RESERVED(138);
    BF2_LIGHT_RESERVED(13C); BF2_LIGHT_RESERVED(140);
    BF2_LIGHT_RESERVED(144); BF2_LIGHT_RESERVED(148);
    BF2_LIGHT_RESERVED(14C); BF2_LIGHT_RESERVED(150);
    BF2_LIGHT_RESERVED(154); BF2_LIGHT_RESERVED(158);
    BF2_LIGHT_RESERVED(15C); BF2_LIGHT_RESERVED(160);
    BF2_LIGHT_RESERVED(164); BF2_LIGHT_RESERVED(168);
    BF2_LIGHT_RESERVED(16C); BF2_LIGHT_RESERVED(170);
    BF2_LIGHT_RESERVED(174); BF2_LIGHT_RESERVED(178);
    BF2_LIGHT_RESERVED(17C);
    virtual void setShadowTSMFocus(float value);                           // +0x180
    virtual float getShadowTSMFocus() const;                               // +0x184
    BF2_LIGHT_RESERVED(188); BF2_LIGHT_RESERVED(18C);
    BF2_LIGHT_RESERVED(190); BF2_LIGHT_RESERVED(194);
    BF2_LIGHT_RESERVED(198);
    virtual void renderShadowMaps();                                       // +0x19C

    virtual void setSunDirection(Vec3 direction);                          // +0x1A0
    virtual Vec3 getSunDirection() const;                                  // +0x1A4
    virtual void setSunColor(Vec3 color);                                  // +0x1A8
    virtual Vec3 getSunColor() const;                                      // +0x1AC
    virtual void setSunSpecColor(Vec3 color);                              // +0x1B0
    virtual Vec3 getSunSpecColor() const;                                  // +0x1B4
    virtual void setSunEnabled(bool enabled);                              // +0x1B8
    virtual bool getSunEnabled() const;                                    // +0x1BC
    BF2_LIGHT_RESERVED(1C0);
    virtual void setTreeSunColor(const Vec3& color);                       // +0x1C4
    virtual const Vec3& getTreeSunColor() const;                           // +0x1C8
    virtual void setTreeSkyColor(const Vec3& color);                       // +0x1CC
    virtual const Vec3& getTreeSkyColor() const;                           // +0x1D0
    virtual void setStaticSunColor(const Vec3& color);                     // +0x1D4
    virtual const Vec3& getStaticSunColor() const;                         // +0x1D8
    virtual void setStaticSpecularColor(const Vec3& color);                // +0x1DC
    virtual const Vec3& getStaticSpecularColor() const;                    // +0x1E0
    virtual void setStaticSkyColor(const Vec3& color);                     // +0x1E4
    virtual const Vec3& getStaticSkyColor() const;                         // +0x1E8
    virtual void setSinglePointColor(const Vec3& color);                   // +0x1EC
    virtual const Vec3& getSinglePointColor() const;                       // +0x1F0
    virtual void setDynamicPointColor(const Vec3& color);                  // +0x1F4
    virtual const Vec3& getDynamicPointColor() const;                      // +0x1F8
    BF2_LIGHT_RESERVED(1FC); BF2_LIGHT_RESERVED(200);
    BF2_LIGHT_RESERVED(204); BF2_LIGHT_RESERVED(208);
    BF2_LIGHT_RESERVED(20C); BF2_LIGHT_RESERVED(210);
    BF2_LIGHT_RESERVED(214); BF2_LIGHT_RESERVED(218);
    BF2_LIGHT_RESERVED(21C); BF2_LIGHT_RESERVED(220);
    BF2_LIGHT_RESERVED(224); BF2_LIGHT_RESERVED(228);
    BF2_LIGHT_RESERVED(22C); BF2_LIGHT_RESERVED(230);
    BF2_LIGHT_RESERVED(234); BF2_LIGHT_RESERVED(238);
    BF2_LIGHT_RESERVED(23C); BF2_LIGHT_RESERVED(240);
    BF2_LIGHT_RESERVED(244); BF2_LIGHT_RESERVED(248);
    BF2_LIGHT_RESERVED(24C); BF2_LIGHT_RESERVED(250);
    BF2_LIGHT_RESERVED(254); BF2_LIGHT_RESERVED(258);
    virtual void setEffectSunColor(const Vec3& color);                     // +0x25C
    virtual const Vec3& getEffectSunColor() const;                         // +0x260
    virtual void setEffectShadowColor(const Vec3& color);                  // +0x264
    virtual const Vec3& getEffectShadowColor() const;                      // +0x268
    virtual void setDefaultEffectLightAffectionFactor(float factor);       // +0x26C
    virtual float getDefaultEffectLightAffectionFactor() const;            // +0x270
    BF2_LIGHT_RESERVED(274); BF2_LIGHT_RESERVED(278);
    BF2_LIGHT_RESERVED(27C);
    // +0x280  PE 0x1015BF40, `mov eax, [ecx+154h]; retn` - the field the constructor seeds with 1
    // and +0x27C sets.  The StaticMesh shadow draw renders a caster with this lod (clamped to the
    // instance's lod count) and restores afterwards.  [INFERRED] name.
    virtual int getStaticMeshShadowLod() const;                            // +0x280
    virtual void setBundledMeshShadowLod(int lod);                         // +0x284
    virtual int getBundledMeshShadowLod() const;                          // +0x288
    // PE 0x1015BE30/0x1015BE40, field +0x15C: the skinned shadow draw's LOD, without clamping.
    // Name [INFERRED] from that caller; getter constness follows the read-only accessor convention.
    virtual void setSkinnedMeshShadowLod(int lod);                         // +0x28C
    virtual int getSkinnedMeshShadowLod() const;                           // +0x290
    BF2_LIGHT_RESERVED(294); BF2_LIGHT_RESERVED(298);
    // [INFERRED] name: height probes cast upward by this unclamped float, field +0x164.
    virtual void setShadowRoofRayLength(float length);                   // +0x29C
    virtual float getShadowRoofRayLength() const;                        // +0x2A0
    virtual void setShadowSunScalePower(float power);                     // +0x2A4
    virtual float getShadowSunScalePower() const;                         // +0x2A8
    BF2_LIGHT_RESERVED(2AC); BF2_LIGHT_RESERVED(2B0);
    virtual void setShadowUseSweepTest(bool enabled);                      // +0x2B4
    virtual bool getShadowUseSweepTest() const;                            // +0x2B8
    virtual void setShadowSunHeightOverFade(const Vec2& range);            // +0x2BC
    virtual const Vec2& getShadowSunHeightOverFade() const;                // +0x2C0
    // [INFERRED] sampler names follow SimpleShadowmap.raw / SimpleShadowmapIO.raw. Float
    // coordinate/return types and integer texel arguments are PE-proven; constness is inferred
    // from these read-only bodies. The filtered methods retain four virtual texel queries.
    virtual float sampleSimpleShadowMap(float x, float z) const;           // +0x2C4
    virtual float sampleSimpleShadowMapIO(float x, float z) const;         // +0x2C8
    virtual float sampleSimpleShadowMapIOTexel(int x, int z) const;        // +0x2CC
    virtual float sampleSimpleShadowMapTexel(int x, int z) const;          // +0x2D0
    BF2_LIGHT_RESERVED(2D4);
    // +0x2D8  PE 0x1015C590, `mov eax, [ecx+820h]; retn` - the field the constructor seeds with
    // 16 and +0x2D4 sets.  The StaticMesh shadow draw merges a lod's index ranges into one draw
    // only while the shadow lod is at or below it.  [INFERRED] name.
    virtual int getShadowMergeLodLimit() const;                            // +0x2D8
    virtual void setDynamicLightRangeScale(float scale);                   // +0x2DC
    virtual float getDynamicLightRangeScale() const;                       // +0x2E0
    virtual void setOccludedSkyNightIntensity(float intensity);            // +0x2E4
    virtual float getOccludedSkyNightIntensity() const;                    // +0x2E8
    // [INFERRED] name; PE 100F3B20 loads both SimpleShadowmap raw files at post-load.
    virtual void loadSimpleShadowMaps();                                  // +0x2EC
    virtual void registerShadowEffect(int reflectionTypeId,
                                      dx9rend::Effect* effect);            // +0x2F0

    // Defined only in XpLightManager.cpp. These adapters are the one binary-boundary concession
    // required by the unrecovered tail of the host table; source code never calls them.
    struct HostAbi;
};

#undef BF2_LIGHT_RESERVED

inline LightManager* asLightManager(IBase* base)
{
    return static_cast<LightManager*>(base);
}

#if defined(BF2_IMAGE_RENDDX9)

extern const unsigned int CID_LightManager;
extern const unsigned int IID_ILightManager;

// Direct XpLightManager.cpp frame coordinator at PE 0x100EFB50.  Renderer calls this against a
// TU-static coordinator rather than through LightManager; keeping it a free seam preserves that
// distinction.
void prepareLightFrame(double time);

// Renderer::onRenderPathActivated and the LightManager device-reset listener call
// this direct body rather than a primary-interface slot.
bool createShadowResources(LightManager* manager);

// PE 0x100F1F60. `Lightmanager.init` invokes this direct body rather than a primary-interface
// slot. It rebuilds the three debug meshes and creates/configures the special sunlight at +0x840.
void initializeLightManager(LightManager* manager);

IBase* __fastcall createLightManager(unsigned int iid, IBase* outer);

// XpLightManager.cpp's renderer-target helpers, published for the terrain draws
// (Geom/TerrainDraw.cpp): sub_10005920 with a texture argument, and sub_10028070.
void lightManagerSetRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture);
void lightManagerRestoreRendererTarget0();
// Binds `surface` as the renderer's depth stencil; null binds the renderer's own (+0x770).
// Published for LightmapGeneration.cpp.
void lightManagerSetDepthStencil(IDirect3DSurface9* surface);

#endif // BF2_IMAGE_RENDDX9

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_XPLIGHTMANAGER_H
