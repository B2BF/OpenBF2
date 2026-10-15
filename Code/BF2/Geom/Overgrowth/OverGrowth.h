// Vegetation resources, cell batching, near-instance rendering and shadow collection.
// The owner, its tables and their records are native objects with std containers (issue #7); the
// `// +0xNN` comments keep the recovered retail offsets.

#ifndef BF2_GEOM_OVERGROWTH_H
#define BF2_GEOM_OVERGROWTH_H

#ifdef BF2_IMAGE_RENDDX9
#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include <string>
#include <vector>

namespace dice { namespace hfe {
class Frustum;
namespace dx9rend { class IRenderView; struct EngineTexture; struct TextureAtlas; }
namespace world { class IHeightmap; class IHeightmapCluster; class IObjectTemplate; }
namespace geom {
using dx9rend::IRenderView;
class IGeometryTemplate;

// [INFERRED] names for instruction-proven pointer carriers. These declarations do not claim
// recovered record members beyond the observed strides, or source constness.
struct OverGrowthCell;             // 0x2AC bytes, defined in OverGrowthCell.h (constructor 100D0C90)
struct OverGrowthPatch;            // 0x98-byte entries at owner +0xE0, defined in OverGrowthCell.h
struct LightShadowRecord;          // XpLightManager.h - the LightManager's shadow records


// Per-material vegetation type, including placement weight and preloaded geometry.

struct OverGrowthType
{
    OverGrowthType();

    std::string name;                   // +0x00  addType/setActiveType key (case-sensitive ==)
    std::string geometry;               // +0x1C  OvergrowthType.geometry; addType seeds it with name
    float        probability;            // +0x38  [INFERRED] 1.0f; postLoad: density / sum(density)
    float        density;                // +0x3C  OvergrowthType.density
    float        rotationScale;          // +0x40  OvergrowthType.rotationScale
    Vec2         randomScale;            // +0x44  OvergrowthType.randomScale (1,1); save omits it
    float        minRadiusToSame;        // +0x4C  OvergrowthType.minRadiusToSame
    float        minRadiusToOthers;      // +0x50  OvergrowthType.minRadiusToOthers
    float        normalScale;            // +0x54  OvergrowthType.normalScale
    int          geometryIndex;          // +0x58  -1 until preLoad resolves `geometry` (1015F450)
};


// OverGrowthMaterial - one Overgrowth.addMaterial record, 0x30 bytes in the PE, in the owner's +0x3C
// vector (preLoad/save/getMaterialList divide by 48). Copies and assignment are member-wise (the
// string, +0x1C, then the type vector - 100D0600 / 100D1A70 / 100D0AE0); the destructor releases the
// types, then the name (100D28B0).

struct OverGrowthMaterial
{
    OverGrowthMaterial();

    std::string                name;    // +0x00  setActiveMaterial key (_stricmp)
    unsigned int                id;      // +0x1C  addMaterial's second argument (unique): the
                                         //        Overgrowth.raw value, 0..15 (the coverage slot)
    std::vector<OverGrowthType> types;   // +0x20
};


// One half of a geometry-table entry (0x50 bytes in the PE): a template, its own instance and the
// pool of instances the frame hands out. Construction 1015EB80, copy 1015EC70, destruction 1015EC00
// (OverGrowth.cpp). Field names are [INFERRED].

struct OverGrowthGeometryRecord
{
    explicit OverGrowthGeometryRecord(const std::string& templateName);
    OverGrowthGeometryRecord(const OverGrowthGeometryRecord& other);
    ~OverGrowthGeometryRecord();

    std::string             name;              // +0x00  the template name
    void*                    geometry;          // +0x1C  the template's own instance (createInstance)
    // +0x20 / +0x24 - the vegetation vertices of the non-"tile_" (leaf, [0]) and "tile_" (trunk, [1])
    // materials; +0x28 / +0x2C the matching index counts (postLoadResources).
    unsigned int             vertexCounts[2];
    unsigned int             indexCounts[2];
    world::IObjectTemplate*  objectTemplate;    // +0x30  ObjectTemplateManager getTemplate(name)
    IGeometryTemplate*       geometryTemplate;  // +0x34  GeometryTemplateManager getTemplate(name)
    unsigned int             poolUsed;          // +0x38  pool geometries handed out this frame;
                                                //        NOT constructed (render resets it)
    std::vector<void*>       pool;              // +0x3C  the instances the pool owns
    float                    distanceScale;     // +0x4C  1.0; postLoad clamp((radius * 0.2)^2, 0.3, 1)

private:
    OverGrowthGeometryRecord& operator=(const OverGrowthGeometryRecord&);
};

// The 0xA0-byte geometry-table element. Its copy (0x10160610: +0x00 then +0x50) and its destruction
// (0x10160640 / the range destroy 0x101607F0: +0x50 then +0x00) are the two records' own.
struct OverGrowthGeometryPair
{
    OverGrowthGeometryPair(const std::string& lodName, const std::string& fullName)
        : lod(lodName), full(fullName) {}

    OverGrowthGeometryRecord lod;    // +0x00  "<name>_lod" - the batched cell geometry
    OverGrowthGeometryRecord full;   // +0x50  "<name>" - the near instances

private:
    OverGrowthGeometryPair& operator=(const OverGrowthGeometryPair&);
};


// One placed vegetation instance - 0x60 bytes, a patch's vector entries (100D19E0 divides by 0x60).
// Construction 1015E930 (OverGrowthCells.cpp / OverGrowthLoad.cpp). Field names are [INFERRED].
struct OverGrowthInstance
{
    int           geometryIndex;    // +0x00  the geometry pair, -1
    Mat4          transform;        // +0x04  identity; the translation row is +0x34..+0x3C
    float         scale;            // +0x44  1.0
    float         cullRadius;       // +0x48  the full geometry's bounding radius (times scale)
    float         radiusToSame;     // +0x4C  radius * minRadiusToSame
    float         radiusToOthers;   // +0x50  radius * minRadiusToOthers
    float         shadow;           // +0x54  the shadow-map sample; NOT constructed
    unsigned char generated;        // +0x58  1 when populatePatch placed it, 0 for a level object
    unsigned char pad59[3];
    float         lodFadeState;     // +0x5C  1.0; the lodTimer cross-fade
};

// The instance list carrier of the cell/patch virtuals.
typedef std::vector<OverGrowthInstance> OverGrowthInstanceVector;


// The native host and compatibility image retain this image-owned object. VideoOptions writes the
// quality scale (+0xF4); post-load/draw test the path (+0x88) for emptiness.
// The unpublished base's constructor/cleanup are factored as helpers; no base type name is guessed.
// Data members are public: the Overgrowth translation units' file-local helpers read them.
class OverGrowth
{
public:
    OverGrowth();
    virtual ~OverGrowth();
    virtual void initialize();
    virtual void update(IRenderView* view);
    virtual void setViewDistance(float distance);                   // +0C 100CBA30
    virtual float getViewDistance() const;
    virtual void rebuildCells();
    virtual unsigned int sampleDensity(float x, float z);
    virtual void updateCells(IRenderView* view);                     // +1C 100C9450
    virtual OverGrowthCell* getCell(int x, int z);
    virtual void buildCell(OverGrowthCell*, OverGrowthInstanceVector*);
    virtual void collectCell(OverGrowthCell*, OverGrowthInstanceVector*);
    virtual void reload();                                         // +2C 100CDDC0
    virtual void clear();
    virtual void render(IRenderView* view, float time);
    // +38 the LightManager's directional records (+0x79C), the pulled-back light Frustum it culls
    // patches with, the caster Frustum whose near plane it grows, and the geometry vector it fills
    // (LightManager +0x970).
    virtual void collectShadowGeometry(const std::vector<LightShadowRecord>* records,
                                       const Frustum* lightFrustum, Frustum* casterFrustum,
                                       std::vector<void*>* geometries);
    virtual void updateCell(OverGrowthCell*, OverGrowthInstanceVector*, bool changed); // +3C 100CB080
    virtual void placeInstance(OverGrowthType*, Vec3*, OverGrowthInstance*);
    virtual void populatePatch(OverGrowthPatch*);

    void preLoadResources();                                        // direct 100CC500
    void postLoadResources();                                       // direct 100CD020
    static void operator delete(void* memory) noexcept;


    // The non-virtual members the Overgrowth / OvergrowthType console executors call (thiscall,
    // `mov ecx, dword_10258F38`), and the load path's geometry-table lookup. Names are [INFERRED]
    // from the console spelling; getter constness is [INFERRED]. Every OvergrowthType.* member acts
    // on the active type (+0xEC) and is a no-op / default read when there is none.

    void                setPath(const std::string& path);
    const std::string& getPath() const;
    void                preLoad();
    std::string        getMaterialList() const;
    bool                addMaterial(const std::string& name, unsigned int id);
    bool                removeActiveMaterial();
    void                setActiveMaterial(const std::string& name);
    std::string        getActiveMaterialName() const;
    std::string        getTypeList() const;
    bool                addType(const std::string& name);
    bool                removeActiveType();
    void                setActiveType(const std::string& name);
    void                save();                                             // 100CD7E0 (editor)
    std::string        getGeometryList() const;
    float               getLodDistance() const;
    void                generateAtlas();                                    // 100CCD20 (editor)
    void                setEditorEnable(bool enable);
    bool                getEditorEnable() const;

    // Direct fields the retail executors read and write with no call at all ([CHOSEN] typed
    // accessors in the SkyDome manner; the executor addresses are in OverGrowthConsole.cpp).
    float getViewDistanceScale() const;       void setViewDistanceScale(float scale);     // +0x28
    float getCloseDistPercentage() const;     void setCloseDistPercentage(float value);   // +0xC8
    float getCloseDistFade() const;           void setCloseDistFade(float value);         // +0xCC
    float getLodTimer() const;                void setLodTimer(float value);              // +0xF8
    // +0xF4 - the vegetation quality scale VideoOptions::apply stores (0.4 / 0.7 / 1.0 per quality
    // level); updateCells multiplies it into the near distance. [CHOSEN] typed accessors for the
    // field VideoOptions wrote by offset.
    float getQualityScale() const;            void setQualityScale(float scale);          // +0xF4

    // The active type's properties - OvergrowthType.*. Each setter writes the field and, once the
    // level is loaded (+0xA4, set by postLoadResources), regenerates everything through reload().
    void         setTypeGeometry(const std::string& geometry);  std::string getTypeGeometry() const;
    void         setTypeDensity(float value);           float getTypeDensity() const;
    void         setTypeNormalScale(float value);       float getTypeNormalScale() const;
    void         setTypeRotationScale(float value);     float getTypeRotationScale() const;
    void         setTypeMinRadiusToSame(float value);   float getTypeMinRadiusToSame() const;
    void         setTypeMinRadiusToOthers(float value); float getTypeMinRadiusToOthers() const;
    void         setTypeRandomScale(Vec2 value);        Vec2  getTypeRandomScale() const;

    // the geometry table's find-or-add: the index of the 0xA0 record whose "_lod" (+0x00)
    // or full (+0x50) name is `name`; else a new record built from `name + "_lod"` and `name`, kept
    // only when both templates gave a geometry; -1 for an empty name or a dropped record. preLoad,
    // initialize, the static-object adoption and the editor's populatePatch call it.
    int findOrAddGeometry(const std::string& name);


    // The editor's painting (OverGrowth.cpp). x / z / radius are world metres on the primary
    // heightmap, centred on the origin; the painters write the +0x80 material map / +0x84 shadow map
    // directly and then refresh the TerrainEditable overlay of the painted rectangle when the terrain
    // shows it (technique 5 / 6). The last argument is an 8-bit value: both shipped builds read only
    // its low byte (1.50 `cmp byte ptr` / `movzx` / `mov r8`; the 2006 editor's executor narrows the
    // console's int to a byte before the call). [INFERRED] names for the members without a console row.

    void paintMaterial(float x, float z, float radius, unsigned char material);    // 100CA130 Overgrowth.paintMaterial
    void paintShadowmap(float x, float z, float radius, unsigned char value);      // 100C9670 Overgrowth.paintShadowmap
    void exportToTerrainTexture();                                                 // 100C8F40 Overgrowth.exportToTerrainTexture
    void exportShadowmapToTerrainTexture();                                        // 100C9250 TerrainEditable technique 6
    // The rectangle exporters: x0..x1 / z0..z1 are primary-heightmap texels, divided unsigned by the
    // terrain's patch size into an inclusive patch range.
    void exportMaterialRectToTerrainTexture(unsigned int x0, unsigned int x1,
                                            unsigned int z0, unsigned int z1);
    void exportShadowmapRectToTerrainTexture(unsigned int x0, unsigned int x1,
                                             unsigned int z0, unsigned int z1);

    // data, in shipped order (the vptr is +0x00). Names are [INFERRED] except the console's
    // (viewDistance, viewDistanceScale, closeDistPercentage, closeDistFade, lodTimer, editorEnable,
    // path). The constructor writes only the fields the PE writes; the rest stay as allocated.
    Vec3                                m_cameraPosition;       // +0x04  baseUpdateCells: the camera
    int                                 m_gridSize;             // +0x10  N, the N x N camera cells (10)
    float                               m_cellSize;             // +0x14
    unsigned int                        m_randomSeed;           // +0x18  0x759DC150, then per cell/patch
    unsigned char                       m_enabled;              // +0x1C  1
    unsigned char                       m_unresolved1D;         // +0x1D  0 [UNRESOLVED]
    unsigned char                       m_initialized;          // +0x1E  initialize sets it
    unsigned char                       m_pad1F;
    float                               m_fadeBand;             // +0x20  0.3; postLoad 0
    float                               m_viewDistance;         // +0x24  Overgrowth.viewDistance
    float                               m_viewDistanceScale;    // +0x28  Overgrowth.viewDistanceScale
    float                               m_heightFadeDistance;   // +0x2C  1.0
    float                               m_minimumHeightFade;    // +0x30  0.4; postLoad 1.0
    float                               m_cellFadeDistance;     // +0x34  12.0
    unsigned char                       m_reseatCells;          // +0x38  rebuildCells sets it
    unsigned char                       m_pad39[3];
    std::vector<OverGrowthMaterial>     m_materials;            // +0x3C
    std::vector<OverGrowthGeometryPair> m_geometries;           // +0x4C
    float                               m_extentX;              // +0x5C  map width * scale.x
    float                               m_extentZ;              // +0x60  map height * scale.z
    float                               m_originX;              // +0x64  -extentX / 2; NOT constructed
    float                               m_originZ;              // +0x68  -extentZ / 2; NOT constructed
    Vec3                                m_patchOrigin;          // +0x6C  (originX, 0, originZ)
    unsigned int                        m_mapWidth;             // +0x78  the primary heightmap's size
    unsigned int                        m_mapHeight;            // +0x7C
    unsigned char*                      m_densityMap;           // +0x80  Overgrowth.raw - material ids
    unsigned char*                      m_shadowMap;            // +0x84  OvergrowthShadowmap.raw
    std::string                        m_path;                 // +0x88  Overgrowth.path
    unsigned char                       m_loaded;               // +0xA4  postLoadResources sets it
    unsigned char                       m_padA5[3];
    world::IHeightmap*                  m_heightmap;            // +0xA8  NOT constructed
    world::IHeightmapCluster*           m_cluster;              // +0xAC  NOT constructed
    unsigned char                       m_reloading;            // +0xB0  reload() brackets with it
    unsigned char                       m_padB1[3];
    // +0xB4 - a vector the constructor empties and the destructor frees; nothing else touches it.
    // [UNRESOLVED] element type.
    std::vector<void*>                  m_unresolvedB4;
    float                               m_closeDistance;        // +0xC4  updateCells; NOT constructed
    float                               m_closeDistPercentage;  // +0xC8  Overgrowth.closeDistPercentage
    float                               m_closeDistFade;        // +0xCC  Overgrowth.closeDistFade
    unsigned int                        m_unresolvedD0;         // +0xD0  0 [UNRESOLVED]
    unsigned char                       m_drawNear;             // +0xD4  1 - render and the shadow walk
    unsigned char                       m_updateBeforeRender;   // +0xD5  1 - render calls update()
    unsigned char                       m_padD6[2];
    int                                 m_patchCountX;          // +0xD8  setViewDistance; NOT constructed
    int                                 m_patchCountZ;          // +0xDC
    OverGrowthPatch*                    m_patches;              // +0xE0  new[] of countX * countZ
    OverGrowthCell**                    m_cells;                // +0xE4  N x N
    OverGrowthMaterial*                 m_activeMaterial;       // +0xE8
    OverGrowthType*                     m_activeType;           // +0xEC
    unsigned char                       m_editorEnable;         // +0xF0  Overgrowth.editorEnable
    unsigned char                       m_unresolvedF1;         // +0xF1  0 [UNRESOLVED]
    unsigned char                       m_padF2[2];
    float                               m_qualityScale;         // +0xF4  1.0; VideoOptions writes it
    float                               m_lodTimer;             // +0xF8  Overgrowth.lodTimer
    dx9rend::TextureAtlas*              m_atlas;                // +0xFC
    dx9rend::EngineTexture*             m_atlasPage;            // +0x100 the atlas's first page
    // +0x104 / +0x108 - the largest per-patch vertex count of the leaf [0] and trunk [1] stream;
    // +0x10C / +0x110 the matching index counts. NOT constructed.
    unsigned int                        m_streamVertexCounts[2];
    unsigned int                        m_streamIndexCounts[2];
    float                               m_materialCoverageMax[16]; // +0x114 NOT constructed

private:
    OverGrowth(const OverGrowth&);
    OverGrowth& operator=(const OverGrowth&);
};

OverGrowth* createOverGrowth();
} } }
#endif
#endif
