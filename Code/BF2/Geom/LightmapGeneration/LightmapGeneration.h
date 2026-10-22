// The renderer's lightmap generator. The implementation's path,
// "...\Code\BF2\Geom\LightmapGeneration\LightmapGeneration.cpp", is attested by the image; the header
// placement and the class and member names are [INFERRED]. Field names follow the 2006 editor's
// console bindings where one exists.
//
// The generator renders, from every sample's position, one small view per light into a 1024 x 1024
// target (the occluders drawn as silhouettes), reads the target back and turns the unshadowed pixel
// counts into the sample's channels, which the sampler (ILightSampling) packs into the lightmap.
// 1.50 publishes no console object and never creates the singleton; createLightmapGenerationSingleton
// is the shipped creator, kept callable for the editor flavour's console (LightmapGenerationConsole.cpp).

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_LIGHTMAPGENERATION_LIGHTMAPGENERATION_H
#define BF2_GEOM_LIGHTMAPGENERATION_LIGHTMAPGENERATION_H

#include "ILightmapGeneration.h"
#include "ILightSampling.h"
#include "../QuadPatch.h"                 // TerrainAabb
#include "../UndergrowthSystem.h"         // Vc71Vec - the renderer's owning VC7.1-layout vector
#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include "../../../RendDX9Abi/Vc71String.h"

#include <stddef.h>

struct IUnknown;
struct IDirect3DSurface9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;

namespace dice {
namespace hfe {

class Frustum;
namespace world { class IObject; class LightSource; }
namespace dx9rend { struct EngineTexture; class Effect; class IRenderView; }

namespace geom {

class ILightSampleable;

// A light's per-triangle back-face bits: VC7.1 std::vector<bool>, the bit count ahead of the words.
// [CHOSEN, not recovered] a local copyable form (Vc71BoolVector.h disables copying, and the light
// records are copied by value).
struct LightFacingBits
{
    unsigned int           size;     // +0x00  _Mysize - the destructor zeroes it
    Vc71Vec<unsigned int>  words;    // +0x04  _Myvec

    LightFacingBits() : size(0) {}
    LightFacingBits(const LightFacingBits& other) : size(other.size), words(other.words) {}
    ~LightFacingBits() { size = 0; }

    // Resizes to `count` bits; new bits are false.
    void resize(unsigned int count);
    bool test(unsigned int index) const { return (words[index >> 5] & (1u << (index & 31u))) != 0; }
    void set(unsigned int index)        { words[index >> 5] |= 1u << (index & 31u); }
    void clear(unsigned int index)      { words[index >> 5] &= ~(1u << (index & 31u)); }

private:
    LightFacingBits& operator=(const LightFacingBits&);
};

// One light of a sample block. The `light.*` bindings of the 2006 editor edited the last one; the
// generator appends the scene lights and the GI ring per block and trims back to the persistent
// count afterwards.
struct LightInfo
{
    LightInfo();                                    // the debug defaults listed beside the fields
    LightInfo(const LightInfo& other);              // the render view is copied without a reference
    ~LightInfo();                                   // releases the render view

    // Out-of-line accessors that 1.50 keeps without a caller; the 2006 editor's `light.*` bindings
    // used them on the last light.
    void        setDirection(const Vec3& value);    // stored, then normalised in place
    const Vec3& getDirection() const;
    void  setShadowIntensity(float value);
    float getShadowIntensity() const;
    void  setIntensity(float value);
    float getIntensity() const;
    void  setFov(float value);
    float getFov() const;
    void  setSelfShadowing(bool value);
    bool  getSelfShadowing() const;
    void  setObjectShadows(bool value);
    bool  getObjectShadows() const;
    void  setViewDistance(float value);
    float getViewDistance() const;
    void  setSoftness(float value);
    float getSoftness() const;
    void  setMaxOccluders(int value);
    int   getMaxOccluders() const;

    Vec3                     position;              // +0x00  (1360, 118, 700) [INFERRED debug default]
    float                    radius;                // +0x0C  10
    unsigned char            isLocal;               // +0x10  a point or spot light from the scene
    unsigned char            pad11[3];
    world::LightSource*      source;                // +0x14  the scene light, or null
    Vec3                     direction;             // +0x18  normalise(0, 0.3, 1); toward the light
    float                    intensity;             // +0x24  1
    float                    shadowIntensity;       // +0x28  0.8
    float                    fov;                   // +0x2C  8 (degrees)
    float                    viewDistance;          // +0x30  150
    unsigned char            selfShadowing;         // +0x34  1
    unsigned char            objectShadows;         // +0x35  0 - occluders are drawn for this light
    unsigned char            pad36[2];
    float                    softness;              // +0x38  0
    int                      maxOccluders;          // +0x3C  100 [INFERRED name] - the occluder cap per sample
    unsigned char            isGI;                  // +0x40  a GI ring or GI top light
    unsigned char            pad41[3];
    int                      colorRed;              // +0x44  (int)(source colour.x + 0.5) [UNRESOLVED role]
    dx9rend::IRenderView*    renderView;            // +0x48  created per block by the trace
    world::LightSource*      spotSource;            // +0x4C  the point/spot light again [UNRESOLVED role]
    LightFacingBits          facing;                // +0x50  per-triangle back-face bits

private:
    LightInfo& operator=(const LightInfo&);
};

// One occluder or traced object: the object's placement and its geometry's world-space box.
struct OccluderRecord
{
    OccluderRecord();                               // zero position and box, identity transformation

    world::IObject*  object;                        // +0x00
    Vec3             position;                      // +0x04  the absolute position
    Mat4             transform;                     // +0x10  the absolute transformation
    Vec3             centre;                        // +0x50  the geometry's box centre in world space
    float            radius;                        // +0x5C  its radius, capped at 20000
    float            corners[24];                   // +0x60  the eight box corners in world space
    void*            geometry;                      // +0xC0  the object's IGeometry (+0x38)
};

// A quadtree node over the occluder records.
struct OccluderNode
{
    OccluderNode();                                 // an empty dirty box, no children, no records
    ~OccluderNode();                                // children (array form), then the list

    TerrainAabb                    bounds;          // +0x00
    float                          corners[24];     // +0x3C  the eight corners of the bounds
    Vec3                           centre;          // +0x9C  the bounds' centre (refreshed)
    float                          radius;          // +0xA8  the bounds' radius (refreshed)
    OccluderNode*                  children;        // +0xAC  new[4], or null for a leaf
    Vc71Vec<OccluderRecord*>       records;         // +0xB0  (first at +0xB4) the leaf's records

private:
    OccluderNode(const OccluderNode&);
    OccluderNode& operator=(const OccluderNode&);
};

class LightmapGeneration : public ILightmapGeneration
{
public:
    // Creates the two render targets, their read-back copies, the depth stencil, the light marker,
    // the quad and the effects the trace draws with, and loads the statistics font.
    LightmapGeneration();
    ~LightmapGeneration();

    // IBase
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();                        // destroys and frees at zero
    virtual void* queryInterface(unsigned int iid) const;            // IBase and ILightmapGeneration

    // ILightmapGeneration
    virtual bool traceAllObjects();
    virtual bool traceActiveObject();
    virtual bool traceObject(const LightmapGenerationString& name);
    virtual bool traceTerrain(int patch);
    virtual void setProgressCallback(LightmapProgressCallback callback, void* user);

    // Traces the active object, reporting it as object `index` of `count` in the progress text
    // (-1, -1: not part of a list). The 2006 binding traceActiveObjectProgress.
    bool traceActiveObjectProgress(int index, int count);

    // Out-of-line accessors that 1.50 keeps without a caller (the 2006 console's surface).
    void  setOnlyNearest(bool value);
    bool  getOnlyNearest() const;
    void  setFirstBlock(int value);
    int   getFirstBlock() const;
    void  setBlockSize(int value);
    int   getBlockSize() const;
    void  setSampleOffset(float value);
    float getSampleOffset() const;
    void  setShadowsNearCamera(float value);
    float getShadowsNearCamera() const;
    // Store, then clamp each component to [0, 1].
    void  setSkyColor(const Vec3& value);
    void  setAmbientColor(const Vec3& value);
    void  setGILightColor(const Vec3& value);

    // Appends a default LightInfo (the 2006 binding light.create).
    void pushDefaultLight();

    // The trace internals, in the order the generator calls them.
    // Roads and vegetation answer true untraced; otherwise the intensity trace, or the colour trace
    // when enableIntensityLightmaps is off. `record` null traces the terrain.
    bool traceRecord(OccluderRecord* record, int objectIndex, int objectCount, int block);
    // Every (geom, lod) of the record's geometry, last first, through traceRecord, bracketed by the
    // resume log; false when one of them failed.
    bool traceObjectRecord(OccluderRecord* record, int objectIndex, int objectCount, int block);
    // The intensity trace (the shipped default). Answers the sampler's destroyLightSampler result.
    bool traceIntensity(OccluderRecord* record, int objectIndex, int objectCount, int block);
    // The colour trace (enableIntensityLightmaps 0): RGB lightmaps from the colours each light's
    // views see, over the ambient colour. Answers the sampler's destroyLightSampler result; false
    // when the terrain, its ITerrainEditable or the light sampler is missing.
    bool traceColour(OccluderRecord* record, int objectIndex, int objectCount, int block);
    // The resume log. `starting`: true before a record is traced (answers false when the log
    // already holds it as done), false afterwards to append done!/failed!.
    bool updateResumeLog(OccluderRecord* record, bool succeeded, bool starting);
    // Appends the scene lights touching `bounds` and the GI lights for a block.
    void gatherLights(TerrainAabb* bounds);
    // Every registered object's record, the pointer list, and the quadtree over it.
    void rebuildOccluders();
    // Appends `object`'s record unless it (or its root) moves.
    void addOccluder(world::IObject* object, Vc71Vec<OccluderRecord>* records);
    // A new root bounding every record's sphere, split over m_occluderList.
    void buildOccluderTree(Vc71Vec<OccluderRecord*>* records);
    // Quarters `node` while it holds more than 16 records, 11 levels deep at most.
    void splitOccluderNode(const Vc71Vec<OccluderRecord*>& records, OccluderNode* node, int depth);
    // Every record under `node` whose subtree the frustum may see.
    void queryOccluderNode(const Frustum& frustum, OccluderNode* node,
                           Vc71Vec<OccluderRecord*>* out, int unused);
    // The visible records of the quadtree, each object once.
    void occludersInFrustum(const Frustum& frustum, Vc71Vec<OccluderRecord*>* out);
    // Builds a distance-ordered map of `records` and drops it again (shipped: the list is never
    // trimmed to `maxCount`).
    void sortOccluders(Vc71Vec<OccluderRecord*>* records, const Vec3& position, int maxCount);

    // data, in shipped order (the vptr is +0x00)
    int                        m_refCount;                 // +0x004  1
    Vc71Vec<OccluderRecord>    m_occluders;                // +0x008  (first at +0x00C)
    Vc71Vec<OccluderRecord*>   m_occluderList;             // +0x018  (first at +0x01C)
    OccluderNode*              m_occluderTree;             // +0x028  rebuilt by a trace when null
    Vc71Vec<LightInfo>         m_lights;                   // +0x02C  (first at +0x030)
    Vec3                       m_skyColor;                 // +0x03C  0.3
    Vec3                       m_ambientColor;             // +0x048  0.3
    float                      m_resolutionModifier;       // +0x054  1
    unsigned char              m_renderTerrain;            // +0x058  1 - the terrain is drawn as an occluder
    unsigned char              pad059[3];
    float                      m_shadowsNearCamera;        // +0x05C  -1 (off)
    unsigned char              m_onlyNearest;              // +0x060  0 - trace only the block around the camera
    unsigned char              pad061[3];
    // +0x064 - 1; an all-blocks trace starts at block m_firstBlock - 1. No 2006 binding names it.
    int                        m_firstBlock;               // +0x064  [INFERRED] name
    int                        m_blockSize;                // +0x068  16 - one sample's view, in pixels
    float                      m_sampleOffset;             // +0x06C  0.02 - views start this far along the normal
    unsigned char              m_noPhongAngle;             // +0x070  0
    unsigned char              pad071[3];
    int                        m_generationMethod;         // +0x074  6 - a bit set, see the traces
    int                        m_phongAngleMode;           // +0x078  0 [INFERRED name]
    float                      m_globalSoftness;           // +0x07C  0
    float                      m_directlyLightedThreshold; // +0x080  0.01
    float                      m_cameraNearPlane;          // +0x084  0.001
    IBase*                     m_font;                     // +0x088  a Font::Ported (FontSystem.cpp), Fonts/BF1942.font
    dx9rend::EngineTexture*    m_targetTexture;            // +0x08C  1024 x 1024 A8R8G8B8 render target
    dx9rend::EngineTexture*    m_targetTexture2;           // +0x090
    dx9rend::EngineTexture*    m_sysMemTargetTexture;      // +0x094  the read-back copy
    dx9rend::EngineTexture*    m_sysMemTargetTexture2;     // +0x098
    IDirect3DSurface9*         m_depthStencil;             // +0x09C  1024 x 1024 D24S8
    IUnknown*                  m_lightMesh;                // +0x0A0  the D3DX sphere (ID3DXMesh), radius 0.1
    dx9rend::Effect*           m_lightEffect;              // +0x0A4  shaders/LightGeom
    IUnknown*                  m_unresolvedA8;             // +0x0A8  0, Release()d by the destructor [UNRESOLVED] type
    dx9rend::Effect*           m_quadEffect;               // +0x0AC  shaders/QuadGeom
    IDirect3DVertexBuffer9*    m_lightMeshVertices;        // +0x0B0
    IDirect3DIndexBuffer9*     m_lightMeshIndices;         // +0x0B4
    unsigned int               m_lightMeshFaces;           // +0x0B8
    unsigned int               m_lightMeshVertexCount;     // +0x0BC
    unsigned int               m_quadStride;               // +0x0C0  8
    unsigned int               m_quadVertexCount;          // +0x0C4  6
    unsigned int               m_quadBytes;                // +0x0C8  48
    unsigned int               m_unwrittenCC;              // +0x0CC  never written
    void*                      m_quadVertexBuffer;         // +0x0D0  "LightmapGeneration", 6 x Vec2
    LightmapProgressCallback   m_progressCallback;         // +0x0D4
    void*                      m_progressUser;             // +0x0D8
    unsigned int               m_unwrittenDC;              // +0x0DC  never written
    unsigned int               m_numberOfGILights;         // +0x0E0  0
    Vec3                       m_giLightColor;             // +0x0E4  0.3
    Vec3                       m_giLightsBaseDir;          // +0x0F0  (0, 0.5, -0.5)
    float                      m_giLightsIntensity;        // +0x0FC  -1 (1)
    float                      m_giLightsShadowIntensity;  // +0x100  -1 (1)
    float                      m_giLightsFov;              // +0x104  -1 (100)
    float                      m_giLightsViewDistance;     // +0x108  50
    unsigned char              m_useGITopLight;            // +0x10C  0
    unsigned char              pad10D[3];
    float                      m_giTopLightIntensity;      // +0x110  0.1
    float                      m_sunLightIntensity;        // +0x114  -1 (the light's own)
    float                      m_sunLightShadowIntensity;  // +0x118  -1 (the light's own)
    float                      m_sunLightFov;              // +0x11C  8
    unsigned char              m_enableIntensityLightmaps; // +0x120  1 - selects the intensity trace
    unsigned char              m_enableBackfaceCulling;    // +0x121  1
    unsigned char              pad122[2];
    int                        m_lightmapFormat;           // +0x124  1 = DXT1, else DXT5
    float                      m_skyWhite;                 // +0x128  0
    vc71::string               m_logPath;                  // +0x12C  the resume log ("" = none), 2006 `useLog`
    // +0x148 - 0.8 from the constructor; each intensity trace resets it and a terrain trace raises it
    // to the largest sun channel of a sample whose normal is straight up (the terrain sampler gives
    // water exactly that normal). The 2006 editor read the same field through its
    // getWaterSunIntensity binding (+0x14C there); 1.50 keeps no reader. [INFERRED] name.
    float                      m_waterSunIntensity;

private:
    LightmapGeneration(const LightmapGeneration&);
    LightmapGeneration& operator=(const LightmapGeneration&);
};

#if defined(_M_IX86)
static_assert(sizeof(LightInfo) == 100, "a light record is 100 bytes - see the /100 at 0x100A7150");
static_assert(offsetof(LightInfo, source) == 0x14, "light source at +0x14");
static_assert(offsetof(LightInfo, direction) == 0x18, "light direction at +0x18");
static_assert(offsetof(LightInfo, maxOccluders) == 0x3C, "occluder cap at +0x3C");
static_assert(offsetof(LightInfo, renderView) == 0x48, "light render view at +0x48");
static_assert(offsetof(LightInfo, facing) == 0x50, "back-face bits at +0x50");
static_assert(sizeof(OccluderRecord) == 196, "an occluder record is 196 bytes - see 0x100A7D00");
static_assert(offsetof(OccluderRecord, centre) == 0x50, "record centre at +0x50");
static_assert(offsetof(OccluderRecord, geometry) == 0xC0, "record geometry at +0xC0");
static_assert(sizeof(OccluderNode) == 0xC0, "a quadtree node is 0xC0 bytes - see 0x100A78C3");
static_assert(offsetof(OccluderNode, centre) == 0x9C, "node centre at +0x9C");
static_assert(offsetof(OccluderNode, children) == 0xAC, "node children at +0xAC");
static_assert(offsetof(LightmapGeneration, m_occluders) == 0x008, "+0x008");
static_assert(offsetof(LightmapGeneration, m_occluderList) == 0x018, "+0x018");
static_assert(offsetof(LightmapGeneration, m_occluderTree) == 0x028, "+0x028");
static_assert(offsetof(LightmapGeneration, m_lights) == 0x02C, "+0x02C");
static_assert(offsetof(LightmapGeneration, m_resolutionModifier) == 0x054, "resolution modifier at +0x54 - see 0x100AB507");
static_assert(offsetof(LightmapGeneration, m_blockSize) == 0x068, "+0x068");
static_assert(offsetof(LightmapGeneration, m_generationMethod) == 0x074, "+0x074");
static_assert(offsetof(LightmapGeneration, m_font) == 0x088, "+0x088");
static_assert(offsetof(LightmapGeneration, m_quadVertexBuffer) == 0x0D0, "+0x0D0");
static_assert(offsetof(LightmapGeneration, m_progressCallback) == 0x0D4, "+0x0D4 - see 0x100B04C9");
static_assert(offsetof(LightmapGeneration, m_numberOfGILights) == 0x0E0, "+0x0E0");
static_assert(offsetof(LightmapGeneration, m_sunLightFov) == 0x11C, "+0x11C");
static_assert(offsetof(LightmapGeneration, m_lightmapFormat) == 0x124, "+0x124 - see 0x10199BD5");
static_assert(offsetof(LightmapGeneration, m_logPath) == 0x12C, "+0x12C");
static_assert(offsetof(LightmapGeneration, m_waterSunIntensity) == 0x148, "+0x148");
static_assert(sizeof(LightmapGeneration) == 0x14C, "the creator allocates 0x14C - see 0x100A870F");
#endif

// The singleton the creator makes; the terrain sampler's lightmap writer reads its format.
extern LightmapGeneration* g_LightmapGeneration;

// Creates the singleton unless one exists and registers it as the ClassManager simple singleton
// "LightmapGeneration". 1.50 RendDX9 has no caller; the editor flavour's `lightmapGeneration.init`
// binding calls it.
void createLightmapGenerationSingleton();
// Destroys the singleton (null-safe) and clears the global. The simple-singleton registration is
// left in place, as shipped. The shipped caller is the renderer's Direct3D subsystem shutdown, which
// Renderer::shutdown (BF2Render.cpp, still a [FRONTIER]) runs; until that body exists, nothing calls
// this.
void destroyLightmapGenerationSingleton();

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_LIGHTMAPGENERATION_LIGHTMAPGENERATION_H
#endif // BF2_IMAGE_RENDDX9
