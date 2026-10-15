// Owns grass materials, density cells and the visible patch cache.
// [INFERRED] header placement and non-console method names; preserve the declared virtual order.
// [CHOSEN, not recovered] implementation split into object, load, render and console files.
// Materials contain grass types selected by the Material/Type blocks in Undergrowth.cfg.

#ifndef BF2_GEOM_UNDERGROWTHSYSTEM_H
#define BF2_GEOM_UNDERGROWTHSYSTEM_H

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <string>
#include "../Core/IBase.h"
#include "../Core/Random.h"
#include "../Math/Vec3.h"

#include <cstddef>
#include <new>
#include <vector>

namespace dice {
namespace hfe {

namespace world { class IHeightmap; }

namespace dx9rend {
class Effect;
class ShaderParameter;
struct EngineTexture;
struct TextureAtlas;
}

namespace geom {

class Terrain;
struct MeshBounds;    // MeshTemplate.h - the 60-byte box cellBounds fills
struct UndergrowthPatch;


// 24-byte CPU vertex. Only position is initialized; bytes +0x12..+0x13 are padding.
// Copies preserve the two separate attribute bytes without copying that padding.

struct UndergrowthVertex
{
    UndergrowthVertex() { position.x = 0.0f; position.y = 0.0f; position.z = 0.0f; }

    Vec3          position;            // +0x00  .obj "v", z negated by the loader
    short         texCoord[2];         // +0x0C  atlas u, v * 32767 (__ftol2, low word)
    unsigned char byte10;              // +0x10  [UNRESOLVED] never written by the load path, never
                                       //        read by the render path
    unsigned char colorScale;          // +0x11  (1 - TerrainColorScale) * 255, truncated; buildPatch
                                       //        copies it into the patch vertex
    unsigned char pad12[2];            // +0x12  never written, never copied
    float         sway;                // +0x14  0 at the root, TypeSwayScale at the top

    UndergrowthVertex(const UndergrowthVertex& other)        // member-wise, no padding
        : position(other.position), byte10(other.byte10), colorScale(other.colorScale),
          sway(other.sway)
    {
        texCoord[0] = other.texCoord[0];
        texCoord[1] = other.texCoord[1];
    }
};


// UndergrowthType - 0xB0 bytes: one "Type <name> { ... }" block of Undergrowth.cfg, or one type record
// of Undergrowth.dat. Constructor 0x10148070, copy constructor 0x1014FED0 (member-wise), destructor
// (the three vectors, +0xA0 first, then the three strings, +0x38 first - reverse
// declaration order). Field names follow the cfg keywords that fill them (the parser's stores
// 0x1014C001..0x1014C374, the key string pushed before each).

struct UndergrowthType
{
    UndergrowthType();

    std::string                   name;                     // +0x00  "Type <name>"
    std::string                   mesh;                     // +0x1C  "Mesh"      ../RawData/Undergrowth/<mesh>.obj
    std::string                   texture;                  // +0x38  "Texture"   ../RawData/Undergrowth/<texture>.dds
    Vec2                           size;                     // +0x54  "CrossSize" / "Size" (1, 1); y is the
                                                             //        height factor buildPatch uses
    Vec2                           randomSizeScale;          // +0x5C  "RandomSizeScale" (min, max)  (1, 1)
    Vec2                           slopeRange;               // +0x64  "SlopeRange" - bounds on the terrain
                                                             //        normal's y                  (0, 1)
    float                          density;                  // +0x6C  "Density", clamped [0.01, 10]   0.01
    float                          variation;                // +0x70  "Variation", clamped [-1, 1]    0.2;
                                                             //        < 0 places uniformly at random
    float                          terrainColorScale;        // +0x74  "TerrainColorScale"       1
    float                          typeSwayScale;            // +0x78  "TypeSwayScale"           1
    bool                           skew;                     // +0x7C  "Skew" (int != 0)         false
                                                             // +0x7D..+0x7F padding (0x1014FED0
                                                             //   copies the one byte only)
    std::vector<UndergrowthVertex> vertices;                 // +0x80
    std::vector<unsigned short>    indices;                  // +0x90
    std::vector<int>               materials;                // +0xA0  [UNRESOLVED] the cfg's in-Type
                                                             // The cfg material-index arm is unreachable.
};


// UndergrowthMaterial - 0x34 bytes: one "Material <name> <id> { ... }" block. The default
// constructor is the stack temporary at 0x1014BB0A..0x1014BB37 (name constructed, +0x20 = 0, the
// vector's three pointers zeroed; +0x1C and the vector's allocator word NOT written) and the one
// 0x1014B637 builds for resize. Copy = 0x10151700 (member-wise); destructor = 0x101518E0's per-element
// body (the type vector, then the name). The two density queries (+0x28 / +0x2C) match `id` against
// a density byte and answer `generalHeight`; buildPatch strides the vector by `/ 52` (0x10146E77).

struct UndergrowthMaterial
{
    UndergrowthMaterial() : generalHeight(0.0f) {}

    std::string                   name;                     // +0x00  "Material <name>"
    int                            id;                       // +0x1C  "Material <name> <id>" - the
                                                             //        Undergrowth.raw byte it paints, and
                                                             //        the index into typeFractions
    float                          generalHeight;            // +0x20  "GeneralHeight"
    std::vector<UndergrowthType>   types;                    // +0x24  0xB0-byte records
};


// UndergrowthCell - 0x68 bytes, m_cells (+0xEC): one per (terrain patch x PatchSubdivide)^2 cell, its
// grid position implicit (row-major over m_cellsPerSide; cellBounds divides the offset by 104).
// Allocated `new[]` with an array cookie (0x1014D78D: 0x68 * n + 4, the count stored first) and freed
// by clear() as sub_1005F370(p - 4) with no element destructor loop - so the element class has an
// empty user-declared destructor (declared here to keep the cookie). The constructor (inlined at
// 0x1014D7C0) writes only +0x00, +0x0C..+0x18, +0x24..+0x63 (16 dwords) and the byte +0x64.

struct UndergrowthCell
{
    UndergrowthCell()
        : patch(0), texScaleU(0.0f), texScaleV(0.0f), texOffsetU(0.0f), texOffsetV(0.0f), byte64(0)
    {
        for (int i = 0; i < 16; ++i)
            typeFractions[i] = 0.0f;
    }
    ~UndergrowthCell() {}

    UndergrowthPatch*       patch;           // +0x00  the cache entry currently holding this cell
    dx9rend::EngineTexture* colorMap;        // +0x04  Terrain +0x44 (getPatchColormap) of the cell's
                                             //        patch - TEXLAYER1, sampled on the simple path
    dx9rend::EngineTexture* lightMap;        // +0x08  Terrain +0x48 (getPatchLightmap) - TEXLAYER2
    float                   texScaleU;       // +0x0C  1 / patch world size - the render path uploads
    float                   texScaleV;       // +0x10  the four as TerrainTexCoordScaleAndOffset
    float                   texOffsetU;      // +0x14  -(patch origin x) * texScaleU
    float                   texOffsetV;      // +0x18  -(patch origin z) * texScaleV
    float                   minHeight;       // +0x1C  Terrain +0x3C, then the heightmap's min over the
                                             //        cell - the cell box's min.y
    float                   maxHeight;       // +0x20  Terrain +0x40 + 1.5, then the heightmap's max + 2
    float                   typeFractions[16]; // +0x24  share of the cell's density texels holding each
                                             //        material id 0..15; 0 makes buildPatch skip it
    unsigned char           byte64;          // +0x64  [UNRESOLVED] zeroed by the constructor
    bool                    noDetail;        // +0x65  Terrain +0x4C (getPatchHasNoDetail); buildPatch
                                             //        returns at once when set
    unsigned char           pad66[2];
};


// UndergrowthPatch - 0x30 bytes, one entry of the patch cache (m_patches, +0xF4). `new[]` with a
// cookie (0x1014D86A: 0x30 * n + 4); the constructor inlined at 0x1014D890 writes +0x00, the byte
// +0x04 and +0x08..+0x24 (8 dwords); +0x28/+0x2C stay unwritten. Destructor 0x10146310.
// rebuildPatches assigns a cell to it (and stamps +0x00/+0x04/+0x08/+0x18..+0x2C), buildPatch fills
// its buffers, render draws it and stamps +0x0C.

struct UndergrowthPatch
{
    UndergrowthPatch()
        : cell(0), built(0), primitiveCount(0), lastDrawnFrame(0), vertexBuffer(0), indexBuffer(0),
          buildMaterial(0), buildType(0), buildRow(0), buildColumn(0)
    {
    }
    ~UndergrowthPatch();

    UndergrowthCell* cell;              // +0x00  0 = free
    unsigned char    built;             // +0x04  buildPatch sets it when the whole cell is in (a byte)
    unsigned char    pad05[3];
    unsigned int     primitiveCount;    // +0x08  indexCount / 3, set with `built`
    unsigned int     lastDrawnFrame;    // +0x0C  Renderer +0x54 at the last draw - the LRU key
    void*            vertexBuffer;      // +0x10  BufferManager VertexBuffer wrapper (16 / 24 stride)
    void*            indexBuffer;       // +0x14  BufferManager IndexBuffer wrapper (INDEX16)
    unsigned int     buildMaterial;     // +0x18  buildPatch's resumable cursor: the material index,
    unsigned int     buildType;         // +0x1C    the type index within it,
    unsigned int     buildRow;          // +0x20    the placement row,
    unsigned int     buildColumn;       // +0x24    the placement column
    unsigned int     vertexCount;       // +0x28  vertices written so far - NOT constructed
    unsigned int     indexCount;        // +0x2C  indices written so far - NOT constructed
};

// The atlas rectangle the mesh builders map texture coordinates through: TextureAtlasEntry's first
// four floats, copied into a stack record at 0x1014923A (defaults 1, 1, 0, 0). [INFERRED] name.
struct UndergrowthAtlasRect
{
    float width;                       // +0x00
    float height;                      // +0x04
    float uOffset;                     // +0x08
    float vOffset;                     // +0x0C
};


// The two vertex formats buildPatch writes into a patch's vertex buffer (stride from 0x10147804:
// `setnz cl; lea ecx, [ecx*8+10h]`). Positions are shorts relative to the cell box's centre, scaled by
// 131.068 (flt_1021DB44 = 32767/250). Field names [INFERRED] from what writes them.

struct UndergrowthPatchVertex               // 16 bytes - simpleShaderEnable off
{
    short         position[4];              // +0x00  x, y (type-local height), z, ground height
    unsigned int  texCoord;                 // +0x08  UndergrowthVertex::texCoord, both shorts at once
    unsigned char swayIndex;                // +0x0C  (random >> 22) & 15 - a SwayOffset row
    unsigned char swayAmount;               // +0x0D  heightByte * UndergrowthVertex::sway
    unsigned char heightByte;               // +0x0E  scale * size.y * 85
    unsigned char colorScale;               // +0x0F  UndergrowthVertex::colorScale
};

struct UndergrowthPatchVertexSimple         // 24 bytes - simpleShaderEnable on
{
    UndergrowthPatchVertex base;            // +0x00
    unsigned int           colorSample;     // +0x10  the cell's colour map at the vertex
    unsigned int           lightSample;     // +0x14  the cell's light map at the vertex
};

// The 8-byte element of m_drawOrder (+0x2E4): a sort key (squared distance) and the cell.
struct UndergrowthCellDistance
{
    float            key;                   // +0x00
    UndergrowthCell* cell;                  // +0x04
};

// One of the sixteen 16-byte records at +0x1CC - the "SwayOffset" rows the shader reads. update()
// rewrites x/y every frame and scales z/w in place (see UndergrowthSystem.cpp - a compounding
// multiply, as shipped).
struct UndergrowthWindWave
{
    float x, y, z, w;
    UndergrowthWindWave() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}   // the ctor's zeroing loop
};


// UndergrowthSystem - 0x598 bytes, primary vtable 0x1021DD48 (30 slots). Members at retail offsets
// (pinned by UndergrowthLayoutCheck below).

class UndergrowthSystem : public IBase
{
public:
    UndergrowthSystem();

    // IBase, +0x00..+0x0C
    virtual void  addRef();                                          // ICF_incrementDwordAt0x4
    virtual int   getRef();                                          // ICF_getDwordAt0x4
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // own slots, +0x10..+0x74
    virtual void  load(const std::string& levelPath);               // +0x10
    virtual bool  isLoaded() const;                                  // +0x14
    virtual void  reload();                                          // +0x18
    virtual void  unload();                                          // +0x1C
    virtual void  render(float time);                                // +0x20
    virtual void  update(float time);                                // +0x24
    virtual float getTypeHeightAt(float x, float z);                 // +0x28 (retn 8)
    virtual float getGrassHeightAbove(const Vec3& position,
                                      const Vec3& viewer);           // +0x2C (retn 8)
    virtual void  setEnabled(bool enabled);                          // +0x30
    virtual bool  getEnabled() const;                                // +0x34
    virtual void  setDynamicShadowsEnable(bool enable);              // +0x38
    virtual bool  getDynamicShadowsEnable() const;                   // +0x3C
    virtual void  setSimpleShaderEnable(bool enable);                // +0x40
    virtual bool  getSimpleShaderEnable() const;                     // +0x44
    virtual void  setZPassPatchCount(unsigned int count);            // +0x48
    virtual unsigned int getZPassPatchCount() const;                 // +0x4C
    // +0x50..+0x60: empty in this class (shared nullsubs); the editor's derived class fills them.
    // Arity from the shared bodies (`retn`, `retn 4`, `retn 10h`); the +0x5C argument types from the
    // editor brush's one call (x, z, 2.0f, 0). Names [INFERRED].
    virtual void  editorHook50();                                    // +0x50 nullsub_15
    virtual void  editorHook54();                                    // +0x54 nullsub_15 (0x101A6560)
    virtual void  editorHook58(int value);                           // +0x58 nullsub_53
    virtual void  editorPaint(float x, float z, float radius, int type); // +0x5C nullsub_32
    virtual void  editorHook60();                                    // +0x60 nullsub_15
    virtual float getViewDistance() const;                           // +0x64
    virtual void  setViewDistanceScale(float scale);                 // +0x68
    virtual float getViewDistanceScale() const;                      // +0x6C
    virtual ~UndergrowthSystem();                                    // +0x70
    virtual bool  isEditable() const;                                // +0x74 0x10083AF0 `return 0`

    // The engine heap: the factory allocates with engineAlloc and the deleting destructor frees
    // with engineFree (0x10151FB0 -> sub_1005F360).
    static void operator delete(void* memory);

    // Console direct fields (no slot exists for any of them - the rows read and write the members).
    unsigned int testValue() const { return m_test; }
    void         setTestValue(unsigned int value) { m_test = value; }
    float        test2Value() const { return m_test2; }
    void         setTest2Value(float value) { m_test2 = value; }
    float        viewDistanceFade() const { return m_viewDistanceFade; }
    void         setViewDistanceFadeField(float value) { m_viewDistanceFade = value; }

private:
    UndergrowthSystem(const UndergrowthSystem&);
    UndergrowthSystem& operator=(const UndergrowthSystem&);

    // UndergrowthSystem.cpp
    void clear();
    // the density byte under a world position (0 outside the map).
    int  densityAt(float x, float z);
    // the four density bytes around a world position, and the cell fractions.
    void densityCornersAt(float x, float z, int ids[4], float* fracX, float* fracZ);

    // UndergrowthSystemLoad.cpp
    void initSwayTables();
    bool loadDat(const std::string& levelPath);                     // 0x1014B5B0   826 B  retn 4
    void saveDat(const std::string& savePath,
                 const std::string& levelPath);                     // 0x10146610 1,281 B  retn 8
    void buildAtlas(const std::string& savePath,
                    const std::string& levelPath);                  // 0x10147C00 1,134 B  retn 8
    bool buildMeshes(bool secondPass);                               // 0x10148FC0 2,238 B  retn 4
    void loadMesh(UndergrowthType& type, const UndergrowthAtlasRect& rect); // 0x10148100 1,857 B retn 8
    void computeCellTypeFractions(UndergrowthCell* cell);            // 0x10146360   686 B  retn 4

    // UndergrowthSystemRender.cpp
    // (retn 0Ch) - a cell's world box: x/z from the cell index and m_cellSize, y from
    // minHeight / maxHeight, grown by flt_10239CF8 when `margin`. The loader calls it too (0x1014DCA0).
    void cellBounds(const UndergrowthCell* cell, MeshBounds* bounds, bool margin) const;
    // (retn 18h) - world x/z -> density texel (through m_invHeightmapScale and half the map
    // size, clamped to 0 outside) plus the fractional parts.
    void worldToDensityCell(float x, float z, int* col, int* row, float* fracX, float* fracZ);
    void rebuildPatches(bool force, float viewDistance);             // (retn 8)
    void buildPatch(UndergrowthCell* cell, bool unusedFlag);         // (retn 8)

    // data, in shipped order
    int                       m_refCount;                 // +0x004
    unsigned int              m_test;                     // +0x008  console `test` - a direct field
    float                     m_test2;                    // +0x00C  console `test2` - a direct field
    bool                      m_loaded;                   // +0x010
    bool                      m_loadStarted;              // +0x011  load(): if set, clear(); then set
    bool                      m_enabled;                  // +0x012  console `enabled`
    unsigned char             m_pad013;
    std::string              m_levelPath;                // +0x014  load()'s argument, reload()'s
    float                     m_viewDistance;             // +0x030  "ViewDistance"
    float                     m_viewDistanceFadeScale;    // +0x034  "ViewDistanceFadeScale"
    float                     m_viewDistanceHeightScale;  // +0x038  "ViewDistanceHeightScale"
    float                     m_viewDistanceHeight2Scale; // +0x03C  "ViewDistanceHeight2Scale"
    float                     m_viewDistanceStreamingScale; // +0x040 "ViewDistanceStreamingScale"
    // +0x044 is always the constructor's 15 (0x1014AD88, its only writer in the TU): the .cfg parser
    // reads the "PatchSubdivide" value through its integer reader and discards the answer
    // (0x1014C69F..0x1014C6A7). Shipped; the loader uses +0x044 for the grid (0x1014D758, `fild` plus
    // the 2^32 fix-up - unsigned) and the cell size (0x1014D827).
    unsigned int              m_patchSubdivide;           // +0x044
    float                     m_swaySpeed;                // +0x048  "SwaySpeed"
    float                     m_swaySpeedDiff;            // +0x04C  "SwaySpeedDiff"
    float                     m_swayScale;                // +0x050  "SwayScale"
    float                     m_swayScaleDiff;            // +0x054  "SwayScaleDiff"
    unsigned int              m_patchVertexCount;         // +0x058  a patch buffer's vertex capacity
    unsigned int              m_patchIndexCount;          // +0x05C  a patch buffer's index capacity
    unsigned char*            m_densityMap;               // +0x060  Undergrowth.raw
    dx9rend::EngineTexture*   m_atlasTexture;             // +0x064  the atlas's first page - TEXLAYER0
    dx9rend::Effect*          m_effect;                   // +0x068  "Shaders/Undergrowth"
    dx9rend::ShaderParameter* m_paramTexLayer0;           // +0x06C  "TEXLAYER0"
    dx9rend::ShaderParameter* m_paramTexLayer1;           // +0x070  "TEXLAYER1"
    dx9rend::ShaderParameter* m_paramTexLayer2;           // +0x074  "TEXLAYER2"
    dx9rend::ShaderParameter* m_param078;                 // +0x078  never assigned by load; released
                                                          //         by the destructor, NOT by clear
    dx9rend::ShaderParameter* m_paramWorldViewProjection; // +0x07C
    dx9rend::ShaderParameter* m_paramWorldView;           // +0x080
    dx9rend::ShaderParameter* m_paramPosOffsetAndScale;   // +0x084
    dx9rend::ShaderParameter* m_paramTerrainTexCoordScaleAndOffset; // +0x088
    dx9rend::ShaderParameter* m_paramCameraPos;           // +0x08C
    dx9rend::ShaderParameter* m_paramSwayOffset;          // +0x090
    dx9rend::ShaderParameter* m_paramFadeAndHeightScaleOffset; // +0x094
    dx9rend::ShaderParameter* m_paramSunColor;            // +0x098
    dx9rend::ShaderParameter* m_paramGIColor;             // +0x09C
    dx9rend::ShaderParameter* m_paramStaticPointColor;    // +0x0A0
    dx9rend::ShaderParameter* m_paramFogRange;            // +0x0A4
    dx9rend::ShaderParameter* m_paramFogColor;            // +0x0A8
    dx9rend::ShaderParameter* m_paramAmbientIlluminationRange; // +0x0AC
    dx9rend::ShaderParameter* m_paramWaterFogRange;       // +0x0B0
    dx9rend::ShaderParameter* m_paramWaterFogColor;       // +0x0B4  never set by the render path
    dx9rend::ShaderParameter* m_paramPointLightPosAtten;  // +0x0B8
    dx9rend::ShaderParameter* m_paramPointLightColor;     // +0x0BC
    dx9rend::ShaderParameter* m_paramAlphaRef;            // +0x0C0
    dx9rend::ShaderParameter* m_paramTransparencyX8;      // +0x0C4  "TRANSPARENCY_X8"
    dx9rend::ShaderParameter* m_paramCameraToWater;       // +0x0C8
    dx9rend::ShaderParameter* m_paramShadowMap;           // +0x0CC  "SHADOWMAP"
    dx9rend::ShaderParameter* m_paramShadowProjMatrix;    // +0x0D0
    dx9rend::ShaderParameter* m_paramShadowTrapMatrix;    // +0x0D4
    Terrain*                  m_terrain;                  // +0x0D8  getSingletonAs("Terrain", 820004)
    float                     m_lightingScale;            // +0x0DC  "LightingScale"
    unsigned int              m_cellsPerSide;             // +0x0E0  terrain patches per side * subdivide
    unsigned int              m_cellCount;                // +0x0E4  m_cellsPerSide squared
    float                     m_cellSize;                 // +0x0E8  world units per cell side
    UndergrowthCell*          m_cells;                    // +0x0EC
    unsigned int              m_patchCount;               // +0x0F0  (2r + 1)^2 + 4
    UndergrowthPatch*         m_patches;                  // +0x0F4
    std::vector<UndergrowthMaterial> m_materials;         // +0x0F8
    float                     m_alphaRef;                 // +0x108  "AlphaRef"
    float                     m_windSpeed[16];            // +0x10C  initSwayTables; NOT constructed
    float                     m_windAmplitude[16];        // +0x14C  NOT constructed
    float                     m_windPhase[16];            // +0x18C  NOT constructed; update() advances
    UndergrowthWindWave       m_windWaves[16];            // +0x1CC  "SwayOffset" (16 x float4)
    dx9rend::TextureAtlas*    m_atlas;                    // +0x2CC  allocated by the constructor
    Random                    m_random;                   // +0x2D0  seed 0x759DC150 (OverGrowth +0x18
                                                          //         carries the same); buildPatch
                                                          //         reseeds it per cell
    std::vector<UndergrowthCell*> m_visibleCells;         // +0x2D4  rebuildPatches
    std::vector<UndergrowthCellDistance> m_drawOrder;     // +0x2E4
    Vec3                      m_lastRebuildPosition;      // +0x2F4  the camera at the last rebuild
    // +0x300: the "Undergrowth" profiler stat (sub_10047F20(this+0x300, "Undergrowth") /
    // ~sub_10032E40) - stat-only scaffolding the tree does not reproduce (RenderQueues.cpp's
    // precedent). The stat is 0x268 bytes: its constructor's last store is this+0x264.
    unsigned char             m_profilerStat[0x268];      // +0x300
    world::IHeightmap*        m_heightmap;                // +0x568  the cluster's primary heightmap;
                                                          //         NOT constructed
    Vec3                      m_invHeightmapScale;        // +0x56C  1 / getScale(), per axis
    int                       m_densityWidth;             // +0x578  getSize x; NOT constructed
    int                       m_densityHeight;            // +0x57C  getSize z; NOT constructed
    bool                      m_dynamicShadowsEnable;     // +0x580  console `dynamicShadowsEnable`
    bool                      m_simpleShaderEnable;       // +0x581  selects the 24-byte vertex stride
    unsigned char             m_pad582[2];
    unsigned int              m_zPassPatchCount;          // +0x584  console `zPassPatchCount`
    float                     m_viewDistanceScale;        // +0x588  console `viewDistanceScale`
    float                     m_viewDistanceFade;         // +0x58C  console `viewDistanceFade`
    bool                      m_editorMode;               // +0x590  [INFERRED] only writer: the
                                                          //         orphan 0x10145E10
    unsigned char             m_pad591[7];

    friend struct UndergrowthLayoutCheck;
};

// Offsets the bodies depend on, pinned where the class is complete. The file and vertex formats
// hold on every architecture (Undergrowth.dat stores UndergrowthVertex records as one block of
// count * 24 bytes; the patch vertex buffers take the 16 / 24-byte strides; SwayOffset uploads
// float4 rows); the rest is the retail x86 layout.
struct UndergrowthLayoutCheck
{
    static_assert(sizeof(UndergrowthVertex) == 0x18, "0x1014F9C0 / 0x10150340 stride 0x18");
    static_assert(offsetof(UndergrowthVertex, texCoord) == 0x0C, "u/v at +0x0C");
    static_assert(offsetof(UndergrowthVertex, byte10) == 0x10, "+0x10 byte");
    static_assert(offsetof(UndergrowthVertex, colorScale) == 0x11, "+0x11 byte");
    static_assert(offsetof(UndergrowthVertex, sway) == 0x14, "sway at +0x14");
    static_assert(sizeof(UndergrowthPatchVertex) == 16, "the non-simple vertex stride");
    static_assert(sizeof(UndergrowthPatchVertexSimple) == 24, "the simple vertex stride");
    static_assert(sizeof(UndergrowthWindWave) == 16, "the SwayOffset rows are float4");
#if defined(_M_IX86)
    static_assert(sizeof(UndergrowthCell) == 0x68, "0x1014D78D: 0x68 * n + 4");
    static_assert(offsetof(UndergrowthCell, texScaleU) == 0x0C, "+0x0C");
    static_assert(offsetof(UndergrowthCell, minHeight) == 0x1C, "+0x1C");
    static_assert(offsetof(UndergrowthCell, typeFractions) == 0x24, "+0x24");
    static_assert(offsetof(UndergrowthCell, byte64) == 0x64, "+0x64");
    static_assert(offsetof(UndergrowthCell, noDetail) == 0x65, "+0x65");
    static_assert(sizeof(UndergrowthPatch) == 0x30, "0x1014D86A: 0x30 * n + 4");
    static_assert(offsetof(UndergrowthPatch, lastDrawnFrame) == 0x0C, "+0x0C");
    static_assert(offsetof(UndergrowthPatch, vertexBuffer) == 0x10, "+0x10");
    static_assert(offsetof(UndergrowthPatch, indexBuffer) == 0x14, "+0x14");
    static_assert(offsetof(UndergrowthPatch, buildMaterial) == 0x18, "+0x18");
    static_assert(offsetof(UndergrowthPatch, vertexCount) == 0x28, "+0x28");
    static_assert(sizeof(UndergrowthAtlasRect) == 0x10, "four floats");
    static_assert(sizeof(UndergrowthCellDistance) == 8, "m_drawOrder's size() is `sar 3`");

    static_assert(offsetof(UndergrowthSystem, m_test) == 0x008, "+0x08");
    static_assert(offsetof(UndergrowthSystem, m_loaded) == 0x010, "+0x10");
    static_assert(offsetof(UndergrowthSystem, m_loadStarted) == 0x011, "+0x11");
    static_assert(offsetof(UndergrowthSystem, m_levelPath) == 0x014, "+0x14");
#endif
};

// dword_1024B8F0. ModuleGeom.cpp owns the definition; the renderer, VideoOptions and the console
// rows read it.
extern IBase* g_UndergrowthSystem;

// The factory registered under "dice.hfe.geom.UndergrowthSystem" (0x100B0B20).
IBase* __fastcall createUndergrowthSystem(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // BF2_GEOM_UNDERGROWTHSYSTEM_H
