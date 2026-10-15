// Shared mesh-renderer state. [CHOSEN] Internal layout is local; interface tables retain ABI.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_STATICMESHRENDERER_H
#define BF2_GEOM_STATICMESHRENDERER_H

#include <stddef.h>
#include <string>
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/TextureAtlas/TextureAtlas.h"

namespace dice {
namespace hfe {
namespace geom {

// Retail globals 0x102597E0..0x102597F0. StaticMeshRenderer retains sole creation/release
// ownership; BundledMesh borrows these for its shadow/highlight draws (0x101064A0..0x101069CE).
extern dx9rend::Effect* g_bundledShadowEffect;
extern dx9rend::ShaderParameter* g_bundledShadowAlphaThreshold;
extern dx9rend::ShaderParameter* g_bundledTexLayer0;
extern dx9rend::ShaderParameter* g_bundledOneBoneSkinning;
extern dx9rend::ShaderParameter* g_bundledViewI;


// Owns shared mesh-render resources and ten lightmap texture atlases.

struct StaticMeshRenderer
{
    void setEnabled(unsigned char enabled) { m_enabled = enabled; }
    int getEnabled() const { return m_enabled; }

    const void**  m_vptr;          // +0x00  primary vtable (ABI)
    unsigned char m_enabled;       // +0x04  slots 6/7; the StaticMesh draw's first gate
    unsigned char m_pad05[3];
    std::string  m_name;          // +0x08  slots 8/9 (retail 28 bytes, to +0x23)
    const void**  m_ehVptr;        // +0x24  ECCoreEventHandler subobject vtable (ABI, deferred use)
    int           m_refCount;      // +0x28
    void*         m_decl2C;        // +0x2C  ordinary static/shadow declaration
    void*         m_decl30;        // +0x30  lightmap declaration
    void*         m_decl34;        // +0x34  byte-identical second ordinary declaration
    void*         m_decl38;        // +0x38  FLOAT3 + stream-1 FLOAT2 declaration
    void*         m_decl3C;        // +0x3C  SHORT4 + stream-1 SHORT2 declaration
    void*         m_opaque40;      // +0x40  [UNRESOLVED]
    dx9rend::Effect*          m_shadowEffect;  // +0x44  mods/xpack/Shaders/StaticMesh
    dx9rend::ShaderParameter* m_worldViewProj; // +0x48  WorldViewProjection
    dx9rend::ShaderParameter* m_viewI;         // +0x4C  ViewI
    dx9rend::ShaderParameter* m_paraboloid;    // +0x50  ParaboloidValues
    dx9rend::ShaderParameter* m_paraboloidZ;   // +0x54  ParaboloidZValues
    dx9rend::ShaderParameter* m_parallax;      // +0x58  parallaxscalebias
    // +0x5C: the draw's per-material frustum-cull counter (`++[smr+5Ch]` in 0x100FC7D0 and
    // 0x100FB440); the constructor does not write it.
    unsigned int  m_culledMaterials;
    // +0x60..+0x64, shipped {1,0,1,0,1}, each read by the StaticMesh bodies:
    //   [0] +0x60  cull materials against the frustum before submitting them (the draw)
    //   [1] +0x61  load the per-lod ".lights" vertex-light lists (the lightmap loader)
    //   [2] +0x62  lightmap names carry "=<key>=" (the name builders); off, "_x-y-z"
    //   [3] +0x63  the silhouette draw's fast path (slot +0x1C); off, the slow one
    //   [4] +0x64  object lightmaps come from the atlas; the loader clears it when no .tai loads
    unsigned char m_flags60[5];
    unsigned char m_pad65[3];
    dx9rend::TextureAtlas* m_lightmapAtlases[10];   // +0x68..+0x8C  ten textureAtlasCtor objects
    // +0x90: incremented once per atlas .tai the loader parses.  NOT written by the
    // constructor, so the first increment reads whatever the heap held.  Shipped.
    unsigned int  m_atlasesLoaded;
    unsigned char m_byte94;        // +0x94  ctor 0; the draw forces lod 0 when set
    unsigned char m_pad95[3];
    unsigned int  m_dword98;       // +0x98  ctor 9; the highest lod that still gets a lightmap
    unsigned int  m_dword9C;       // +0x9C  ctor 0 (a byte store)
};

// StaticMeshRenderer_tryLoadLightmapTexture, PE 0x10121F30 (444 B), attested to
// StaticMeshRenderer.cpp:355 by its own diagnostic.  Look an object's lightmap up in the level's
// lightmap atlas: when the first atlas is still empty, parse "<directory>LightmapAtlas.tai" into
// it (failure clears m_flags60[4], logs once per call unless the base's +0x102 byte is set, and
// answers false); then find "<directory><objectName>.dds" and hand back the entry's four floats
// and its page texture, addRef'd.  false when the entry is missing.
bool staticMeshRendererTryLoadLightmapTexture(StaticMeshRenderer* self,
                                              const std::string& directory,
                                              const std::string& objectName,
                                              float outOffset[4],
                                              dx9rend::EngineTexture** outTexture);

// The object-lightmap atlas builders behind `StaticMeshRenderer.loadAllLightMapFiles` and
// `StaticMeshRenderer.loadAllLightMapFilesDLL`. Both list `directory` ('/' appended when it ends in
// neither separator) recursively through the FileManager and take every non-empty file whose name
// contains `extension`, whose base name is exactly its stem plus `extension` and whose stem does not
// contain "LightmapAtlas", as directory + name. The atlas is written under the
// Editor_LevelSavePath setting (output and input root alike) as directory + "LightmapAtlas": its
// .tai and its pages. Both pass no size and no DXT5 override, so the builder packs pages of up to
// 2048 x 2048 in the source files' own format.
//
// The DLL form packs every file into one atlas and answers the builder's result (false for no
// files). [INFERRED] names.
bool staticMeshRendererLoadAllLightMapFilesDll(StaticMeshRenderer* self,
                                               const std::string& directory,
                                               const std::string& extension);
// The batched form packs the files in batches, each sized so that a dice_TextureAtlasTool command
// line naming them would stay under 32300 characters (the line is built, never run), into
// "LightmapAtlas<N>" atlases numbered from 0. False when a batch fails, which includes an empty file
// list; true once the last batch is written. [INFERRED] names.
bool staticMeshRendererLoadAllLightMapFiles(StaticMeshRenderer* self,
                                            const std::string& directory,
                                            const std::string& extension);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_STATICMESHRENDERER_H
#endif // BF2_IMAGE_RENDDX9
