// Terrain resources loaded from terraindata.raw and rendered through the active render path.
// Preserve primary, device-listener and render-path-listener base order and asserted layout.
// [INFERRED] interpretations are marked at declarations; [UNRESOLVED] members retain offsets.
// Missing virtual bodies remain explicit [FRONTIER] boundaries where declared.
#ifndef GEOM_TERRAIN_H
#define GEOM_TERRAIN_H

#include <cstddef>
#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include "../RendDX9/RendDX9Base.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"

namespace dice {
namespace hfe {

class Frustum;                                                        // Math/Frustum.h
namespace world { class IHeightmap; class IHeightmapCluster; class LightSource; }   // Physics/, Game/Objects/Visual/
namespace dx9rend { struct EngineTexture; namespace Ra { class RaShader; class RaStreamDataEditable; } }   // RendDX9/Texture.h, RendDX9/Ra/

namespace geom {

class QuadPatch;
class SurroundingPatch;
class TerrainCuller;


// The terraindata.raw reader: a host stream plus the typed reads the shipped loader family
// (sub_10075DE0..sub_100763A0) performs on it. Every read is "stream->read(dst, n) == n"; the
// results are discarded by every shipped caller, as they are here.

class TerrainRawReader
{
public:
    TerrainRawReader() : m_stream(0) {}
    ~TerrainRawReader() { close(); }

    // Opens a stream with FileManager(path, mode, access, 0). Clears the held pointer
    // first, so it does not release a previously held stream. The fourth argument is
    // unused; its original meaning is [UNRESOLVED].
    void open(const vc71::string& path, int mode, int access, int unusedFourth);
    // sub_10075E00 - release and null.
    void close();

    bool readU32(void* dst)  { return read(dst, 4) == 4; }
    bool readU16(void* dst)  { return read(dst, 2) == 2; }
    bool readByte(void* dst) { return read(dst, 1) == 1; }
    bool readVec3(void* dst) { return read(dst, 12) == 12; }
    bool readVec2(void* dst) { return read(dst, 8) == 8; }
    bool readVec4(void* dst) { return read(dst, 16) == 16; }
    // The writer twins (stream +0x14), by roads step 2 for RoadTemplate::compile
    // (0x1013A260), read in the instruction stream. The 2- and 8-byte values and the wide string
    // (0x10076180 / 0x100761A0 / 0x100761E0 / 0x10076220 / 0x100762A0) are not transcribed until a
    // caller needs them.
    bool writeU32(unsigned int value) { return write(&value, 4) == 4; }
    bool writeF32(float value) { return write(&value, 4) == 4; }
    // sub_10076260 - write(c_str(), size()) == size(); the terminator is not written.
    bool writeString(const vc71::string& text)
    {
        const unsigned int length = text.size();
        return write(text.c_str(), length) == length;
    }
    // sub_100762E0 - answers `write(src, 8) != 0`, ANY non-zero count, not == 8 (shipped; its
    // Vec3 / Vec4 twins 0x10076300 / 0x10076320 share the test).
    bool writeVec2(const void* src) { return write(src, 8) != 0; }
    // TerrainEditable R3: the rest the terraindata.raw writer (0x1010C790) calls. The
    // family mirrors io::File's typed writes in the ELF's order (unsigned int, int, unsigned short,
    // short, unsigned char, char, float, double, bool, string, wide string, Vec2, Vec3, Vec4), so
    // sub_10076140 is the unsigned twin of writeU32 (same body). One byte: sub_100761C0 (unsigned
    // char) and sub_10076240 (bool), both `== 1`. Vec3 / Vec4 answer `!= 0` like writeVec2.
    bool isOpen() const { return m_stream != 0; }
    bool writeByte(unsigned char value) { return write(&value, 1) == 1; }
    bool writeVec3(const void* src) { return write(src, 12) != 0; }
    bool writeVec4(const void* src) { return write(src, 16) != 0; }
    // sub_10075E30 - the raw write: `mov ecx,[ecx]; jmp [eax+14h]`, unguarded as shipped.
    unsigned int write(const void* src, unsigned int n) { return m_stream->write(src, n); }
    unsigned int read(void* dst, unsigned int n);              // sub_10075E20 - the raw read, unguarded
    // sub_10076000 - bytes up to (not including) `delimiter`, at most 0x1000; false on an empty line.
    bool readLine(vc71::string& out, char delimiter);
    int  tell();                                               // sub_10075E50 - the nonlinear +0x34
    void seek(int origin, int offset);                         // sub_10075E40 - the nonlinear +0x30

    ::renddx9abi::HostStream* m_stream;
};

// Per-layer detail-texture settings shared by plain and editable terrain.
// Preserve all 23 virtual slots and by-value string/Vec2 calling conventions.
// The plain implementation exposes only runtime material, tiling and environment-map settings;
// editable subclasses provide names and editing data. Some accessor names are [INFERRED].
// The editable integer at +0x68 remains [UNRESOLVED].
class TerrainDetailTextureBase
{
public:
    virtual ~TerrainDetailTextureBase() {}                           // +0x00  (the base's)
    virtual void          setName(vc71::string name) = 0;             // +0x04
    virtual vc71::string  getName() = 0;                              // +0x08
    virtual void          setColorTextureName(vc71::string name) = 0; // +0x0C
    virtual vc71::string  getColorTextureName() = 0;                  // +0x10
    virtual void          setEditableInt68(int value) = 0;            // +0x14  [UNRESOLVED] the editable record's +0x68
    virtual int           getEditableInt68() = 0;                     // +0x18
    virtual void          setMaterialId(unsigned char id) = 0;        // +0x1C
    virtual unsigned char getMaterialId() = 0;                        // +0x20
    virtual void          setPlaneMap(unsigned char planeMap) = 0;    // +0x24
    virtual unsigned char getPlaneMap() = 0;                          // +0x28
    virtual void          setTopTiling(float tiling) = 0;             // +0x2C
    virtual float         getTopTiling() = 0;                         // +0x30
    virtual void          setSideTiling(Vec2 tiling) = 0;             // +0x34
    virtual Vec2          getSideTiling() = 0;                        // +0x38
    virtual void          setYOffset(float offset) = 0;               // +0x3C
    virtual float         getYOffset() = 0;                           // +0x40
    virtual void          setColorTextureTiling(Vec2 tiling) = 0;     // +0x44  [INFERRED] name
    virtual Vec2          getColorTextureTiling() = 0;                // +0x48
    virtual void          setLowDetailType(int type) = 0;             // +0x4C  [INFERRED] name
    virtual int           getLowDetailType() = 0;                     // +0x50
    virtual void          setEnvMap(bool envMap) = 0;                 // +0x54
    virtual bool          getEnvMap() = 0;                            // +0x58
};

class TerrainDetailTexture : public TerrainDetailTextureBase
{
public:
    // out of line for TerrainEditable::init (0x1010ADC6) and the dead editable
    // record constructor (0x101087F3); the loader inlines the same stores (0x100DD2A3 for its
    // stack record, 0x100DD38F for the heap records). All three store topTiling = 32.0f.
    TerrainDetailTexture();
    virtual ~TerrainDetailTexture();                                  // +0x00  0x100E1730 -> sub_100D8C80
    virtual void          setName(vc71::string name);                 // +0x04
    virtual vc71::string  getName();                                  // +0x08
    virtual void          setColorTextureName(vc71::string name);     // +0x0C
    virtual vc71::string  getColorTextureName();                      // +0x10
    virtual void          setEditableInt68(int value);                // +0x14
    virtual int           getEditableInt68();                         // +0x18
    virtual void          setMaterialId(unsigned char id);            // +0x1C
    virtual unsigned char getMaterialId();                            // +0x20
    virtual void          setPlaneMap(unsigned char planeMap);        // +0x24
    virtual unsigned char getPlaneMap();                              // +0x28
    virtual void          setTopTiling(float tiling);                 // +0x2C
    virtual float         getTopTiling();                             // +0x30
    virtual void          setSideTiling(Vec2 tiling);                 // +0x34
    virtual Vec2          getSideTiling();                            // +0x38
    virtual void          setYOffset(float offset);                   // +0x3C
    virtual float         getYOffset();                               // +0x40
    virtual void          setColorTextureTiling(Vec2 tiling);         // +0x44
    virtual Vec2          getColorTextureTiling();                    // +0x48
    virtual void          setLowDetailType(int type);                 // +0x4C
    virtual int           getLowDetailType();                         // +0x50
    virtual void          setEnvMap(bool envMap);                     // +0x54
    virtual bool          getEnvMap();                                // +0x58

    void*         texture;            // +0x04  <name>
    void*         normalTexture;      // +0x08  <name>_normal        (when the file exists)
    void*         sideTexture;        // +0x0C  <name>_side          (when the file exists; else the base pair again)
    void*         sideNormalTexture;  // +0x10  <name>_sideNormal
    unsigned char materialId;         // +0x14  0; not in the file
    unsigned char planeMap;           // +0x15  file: 1 byte - the side-projected layer (pass 5)
    unsigned char pad16[2];
    float         topTiling;          // +0x18  file: 4 bytes (default 32.0f) - neartextiling.z
    Vec2          sideTiling;         // +0x1C  file: 8 bytes (default 2.0f, 2.0f) - neartextiling.xy
    float         yOffset;            // +0x24  file: 4 bytes (default 0) - neartextiling.w
    unsigned char envMap;             // +0x28  file: 1 byte - the environment-mapped layer (pass 11)
    unsigned char pad29[3];
};

// The 0x14-byte water envmap record setupWater builds (sub_101BF0F0 constructs it: three zero
// words, the byte at +0xC and the texture at +0x10).
struct TerrainEnvMapRecord
{
    int           unresolved00;       // +0x00  0 [UNRESOLVED]
    int           unresolved04;       // +0x04  0 [UNRESOLVED]
    int           unresolved08;       // +0x08  0 [UNRESOLVED]
    unsigned char isDefaultEnvMap;    // +0x0C  1 when <level>/Envmaps/EnvMap0 stood in for <level>/Water/envmap
    unsigned char pad0D[3];
    void*         texture;            // +0x10
};


// Fixed-capacity water animation. Loading and sampling do not check bounds.
// Requires at most 16 frames and a valid phase; destruction evicts and releases each texture.

struct TerrainWaterFrameList
{
    TerrainWaterFrameList();              // sub_10014EB0 - Terrain.cpp

    dx9rend::EngineTexture* frames[16];   // +0x00  left as allocated by the constructor
    int                     frameCount;   // +0x40  signed (the destroy loop tests `jle` / `jl`, the
                                          //        second sampler divides with `idiv`)
    float                   phase;        // +0x44  updateWaterConstants' frac(time / (speed * 0.2f)) -
                                          //        in [0,1) only while that quotient is finite and
                                          //        non-negative (a C truncation, not a floor)

    // sub_10014FD0, 353 B - read the list file: pairs of <texture> <number>. The by-value
    // argument is the shipped signature (`ret 1Ch`); the answer is dropped by setupWater.
    bool load(vc71::string path);
    dx9rend::EngineTexture* currentFrame() const;   // sub_10014EC0 - WaterMapFrame0
    dx9rend::EngineTexture* nextFrame() const;      // sub_10014EF0 - WaterMapFrame1
    float                   frameBlend() const;     // sub_10014F30 - WaterCycleTime
    void destroy();                       // sub_10014F70 - Terrain.cpp
};


// Terrain

class Terrain : public IBase, public dx9rend::DeviceLostResetListener, public dx9rend::RenderPathListener
{
public:
    Terrain();
    // sub_100DD6C0, 2,247 B - the destructor body. NOT a slot: slot +0xB0 (deletingDestructor) is
    // the MSVC scalar deleting destructor that calls it. Non-virtual, as no base declares a virtual
    // destructor (IBase's is protected and non-virtual) - a `virtual` here would be a new slot.
    ~Terrain();

    // IBase, primary table slots 0..3
    virtual void  addRef();                                          // +0x00 0x1010DE10  ++refCount
    virtual int   getRef();                                          // +0x04
    virtual int   release();                                         // +0x08 0x100E2800  --refCount; 0 -> slot +0xB0(1)
    virtual void* queryInterface(unsigned int iid) const;            // +0x0C 0x100E2700  1 / 820004 -> this

    // the Terrain surface, slots 4..56
    virtual unsigned int getReflectionTypeId();                      // +0x10
    virtual void  init();                                            // +0x14 0x100D7920  console `terrain.init`; load()'s first call
    virtual void  load(const vc71::string& path);                    // +0x18 0x100DE070  console `terrain.load`
    virtual void  slot07_noop();                                     // +0x1C 0x100D8520  `ret`
    virtual void  setRenderStyle(int style);                         // +0x20 0x1018F9C0  console `forceRenderStyle` set; 0..1 accepted
    virtual int   getRenderStyle();                                  // +0x24 0x1018F9E0  console `forceRenderStyle` get
    virtual void  update();                                          // +0x28 0x100D8550  culler update, then slot +0xE0
    virtual void  slot11_noop();                                     // +0x2C 0x10037D40  shared `ret`
    virtual int   getPatchCount();                                   // +0x30 / +0x40
    virtual int   getPatchSize();                                    // +0x34 / +0x38
    virtual const Vec3* getPrimaryWorldScale();                      // +0x38 / +0x14
    // +0x3C / +0x40: the patch's minimum (+0x9C) and maximum (+0x98) height (
    // R0: the TerrainEditable raw writer 0x1010C790 writes max-over-patches of +0x40 and
    // min-over-patches of +0x3C into the file words the loader reads to +0x30 / +0x34, then each
    // patch's +0x40 / +0x3C value into the patch record's +0x98 / +0x9C words; QuadPatchEditable's
    // bounds body 0x1019E160 computes them as the max / min vertex height). The names keep the
    // offsets because UndergrowthSystemLoad.cpp calls them by these names.
    virtual float getPatchField9C(int index);                        // +0x3C 0x100D8920  patches[i]->minHeight (+0x9C)
    virtual float getPatchField98(int index);                        // +0x40 0x100D8940  patches[i]->maxHeight (+0x98)
    virtual void* getPatchColormap(int index);                       // +0x44 0x100D8960  patches[i]->+0x4C
    virtual void* getPatchLightmap(int index);                       // +0x48 0x100D8980  patches[i]->+0x50
    virtual bool  getPatchHasNoDetail(int index);                    // +0x4C 0x100D89A0  patches[i]->byte +0x10C
    virtual void  setFarSideTiling(float x, float y);                // +0x50 0x100E28B0  +0x324/+0x328 (file: farSideTiling)
    virtual Vec2* getFarSideTiling(Vec2* out);                       // +0x54
    virtual void  setFarTopTilingHi(float v);                        // +0x58 / +0x330
    virtual float getFarTopTilingHi();                               // +0x5C
    virtual void  setFarTopTilingLow(float v);                       // +0x60 / +0x32C
    virtual float getFarTopTilingLow();                              // +0x64
    virtual void  setFarYOffset(float v);                            // +0x68 / +0x334
    virtual float getFarYOffset();                                   // +0x6C
    virtual void  setSunColor(float r, float g, float b);            // +0x70 0x100DADC0  console `sunColor` set: *0.25, clamp [0,1], dirty
    virtual Vec3* getSunColor(Vec3* out);                            // +0x74 0x100DAFC0  console `sunColor` get: stored * 4
    virtual Vec3* getSunColorRaw(Vec3* out);                         // +0x78 0x100E2820  +0x2E4 as stored
    virtual void  setGIColor(float r, float g, float b);             // +0x7C 0x100DAEC0  console `GIColor` set: *0.5, clamp [0,1], dirty
    virtual Vec3* getGIColorTimes2(Vec3* out);                       // +0x80 0x100E2850  console `GIColor` get: stored * 2
    virtual Vec3* getGIColorRaw(Vec3* out);                          // +0x84 0x100E2880  +0x2F0 as stored
    virtual Vec2* getWaterScroll(Vec2* out);                         // +0x88 0x100E28F0  +0x6C8   console `waterScroll` get
    virtual void  setWaterScroll(float x, float y);                  // +0x8C 0x100E2910  console `waterScroll` set
    virtual float getWaterAnimSpeed();                               // +0x90 0x100E27D0  +0x6C4   console `waterAnimSpeed`
    virtual void  setWaterAnimSpeed(float v);                        // +0x94
    virtual vc71::string getWaterNormalMap();                        // +0x98 0x100E27E0  a copy of +0x6D0   console `waterNormalMap` get
    virtual void  setWaterNormalMap(vc71::string name);              // +0x9C 0x100D85E0  +0x6D0 = name, then slot +0xD4 (ret 0x1C: by value)
    virtual Vec4* getWaterSpecularColor(Vec4* out);                  // +0xA0 0x100E2930  materialBlock->+0x70   console `waterSpecularColor` get
    virtual void  setWaterSpecularColor(float r, float g, float b, float a); // +0xA4 0x100E2970  materialBlock->+0x70
    virtual float getWaterSpecularPower();                           // +0xA8 0x100E29A0  materialBlock->+0x84   console `waterSpecularPower` get
    virtual void  setWaterSpecularPower(float v);                    // +0xAC 0x100E29B0  materialBlock->+0x84
    virtual void* deletingDestructor(int flags);                     // +0xB0 0x100E2C60  ~Terrain (sub_100DD6C0), the free when bit 0, returns this (`mov eax, esi`); release() invokes it with 1
    virtual void  drawDepthComplexity(int);                          // +0xB4 0x100DB2F0  the RPDirectX9DepthComplexity debug draw [read, not written]
    virtual void  renderWorldPassBegin();                            // +0xB8 0x100E0E50  Renderer::renderFrame's first Terrain arm
    virtual void  renderWorldPassEnd();                              // +0xBC 0x100E10C0  Renderer::renderFrame's closing Terrain arm
    virtual void  slot48_unread_100DB810(int, int, int);             // +0xC0 0x100DB810  ret 0xC, the Hi_/Low_Terrain technique chooser [UNREAD]
    virtual void  slot49_noop(int);                                  // +0xC4 0x100D85D0  `ret 4`
    virtual const Vec3* getSecondaryWorldScale();                    // +0xC8 / +0x20
    virtual void  buildSurroundingGeometry();                        // +0xCC 0x100DEAB0  the corner/side ring buffers (Terrain_Surrounding / TerrainEditable)
    virtual void  buildLegacyBaseGridVertexTable();                  // +0xD0 (patchSize+1)^2 packed xz bytes into +0xD4 - the pre-0x18 file arm
    virtual void  setupWater();                                      // +0xD4 0x100DA210  load's closing call - water techniques, envmap, constants
    virtual void  drawWater();                                       // +0xD8 0x100E0EA0  the DrawWater bracket: the Ra arm or the grid arm by ShaderVersion
    // +0xDC 0x100DA120, 232 B - [INFERRED] name: the body releases exactly the water resources
    // setupWater acquires (the WaterMap holder's volume texture, the 2D frame list, the envmap
    // record and its texture). The destructor calls it first, directly (0x100DD6DD).
    virtual void  releaseWater();
    virtual void  updateWaterConstants();                            // +0xE0 0x100DBAD0  update()'s second call: the water quality latch and the water constants

    // dx9rend::DeviceLostResetListener, secondary table 0x1021A188
    virtual void onDeviceLost();                                     // +0x00 0x100D8620  `ret`
    virtual void onDeviceReset();                                    // +0x04 0x100D8630  listener+0x418 = Terrain+0x41C = 1
    virtual void onDeviceDestroy();                                  // +0x08 0x100D8640  `ret`

    // dx9rend::RenderPathListener, secondary table 0x1021A170
    virtual void onRenderPathActivated(int path);                    // +0x00 0x100D8650  +0xC = path  (the +8 subobject's +4)
    virtual void onRenderPathDeactivated();                          // +0x04 0x100D8660  `ret`
    virtual void onRenderPathChanged(int path);                      // +0x08 0x100D8670  +0xC = path
    virtual void onRenderPathNotification(int path);                 // +0x0C 0x100D8680  "TBM: Dont use this!" (Terrain.cpp:3313), logged
    virtual int  getCurrentRenderPath() const;                       // +0x10 0x101A9A10  shared getter
    virtual vc71::string getRenderPathListenerName() const;          // +0x14 0x100E2750  "Terrain"

    // non-virtual bodies the load path calls
    void  loadLowDetailTexture();
    void  loadDetailTextures(TerrainRawReader& reader);
    void  loadLodTablesAndBaseGrid(TerrainRawReader& reader);
    void  loadPatches(TerrainRawReader& reader);
    void  loadSurroundingPatches(TerrainRawReader& reader);
    // sub_100DE740 - the ring grid generator buildSurroundingGeometry calls eight times.
    void  generateSurroundingGrid(int x0, int z0, int x1, int z1, int stepX, int stepZ,
                                  vc71::vector_layout& indices, vc71::vector_layout& vertices);
    // The two upload loops 0x100DEAB0 inlines twice each.
    void  uploadSurroundingVertices(void* vertexBuffer, const vc71::vector_layout& vertices);
    void  uploadSurroundingIndices(void* indexBuffer, const vc71::vector_layout& indices);
    // sub_100D8890 (thiscall, recursive) - the destructor's pre-0x1A arm: free a 400-byte LOD record
    // and, when its first child link (+0x180) is set, its four children's subtrees first.
    void  freeLodTableTree(unsigned char* table);

    // the base-grid LOD index buffer
    // sub_100DD110 (241 B) - the three LOD ranges (+0x104..+0x124), the +0xD8 index buffer of their
    // total (+0xDC) indices, filled by createLodIndices per LOD; +0x1A8 = 4. load()'s pre-0x18 arm
    // (0x100DE398) and TerrainEditable::init (0x1010B480) are its callers. [INFERRED] name.
    void  createLodIndexBuffer();
    // sub_100DCDC0 (841 B, `retn 14h`) - one LOD's indices for the (x, z) block at quad-tree depth
    // `depth`: four sub-blocks down to depth 3, where each block's two-triangles-per-quad list is
    // built in +0x470, vertex-cache optimised and appended at `cursor`. The name is the 2006
    // editor's profiler label for its copy (EditorRendDX9 0x1026DC00, "terain_createLodIndices").
    void  createLodIndices(unsigned short*& cursor, int lod, int x, int z, int depth);
    // sub_100DC0E0 (601 B, `retn 4`) - reorder `indices` (the u16 vector at +0x470) for the vertex
    // cache through two D3DX meshes; the positions come from the legacy base-grid table at +0xD4.
    // Name: the 2006 profiler label "terain_optimizeLodIndices" around its call.
    void  optimizeLodIndices(vc71::vector_layout& indices);

    // the per-frame bodies - TerrainDraw.cpp
    void  applyFrameConstants();                                     // sub_100D95D0  the +0xB8 arm's constants worker
    void  setMorphConstants(int lod);                                // the morphdeltaselector / nearfarmorphlimits / morphdeltaadder block every draw inlines
    void  lockDynamicIndexBuffer();                                  // sub_100D8570  discard-lock of +0x368 into +0x484
    void  drawMain();                                                // sub_100E0940  Hi_/Low_Terrain pass 0 (and pass 7 shadows) into the accumulation target
    void  drawDetailPass();                                          // sub_100DF990  the ShaderVersion-1 detail/light pass [read - stops, see the body]
    void  drawMaskLights();                                          // sub_100DE5B0  the point-light pass over getVisibleLightsByMask(2313) - LightManager +0x18, the frame's visible lights
    void  reloadForLowQuality();                                     // sub_100DDF90  the Low_Terrain fallback reload
    void  drawFinal();                                               // sub_100DB680  the +0xBC arm's diffuse passes
    void  drawSurrounding();                                         // sub_100E0F80  Shared_SurroundingTerrain over the ring
    void  drawWaterRa(float seaLevel);                               // sub_100DBF50  the ShaderVersion-1 Ra water over the gathered nodes
    void  drawWaterGrid(float seaLevel);                             // sub_100DCA50  the 65x65-grid water, one cell per patch-sized square
    void  drawWaterCell(float x0, float z0, float x1, float z1, void* lightmap);
    dx9rend::Ra::RaShader* pickWaterTechnique(const float* box, const float* cameraPosition);
    void  setWaterRecordConstants();                                 // sub_1018F8D0 on +0x710

    // data
    int            m_renderPath;            // +0x00C  the RenderPathListener subobject's +4: the active path
    int            m_refCount;              // +0x010  1
    Vec3           m_primaryWorldScale;     // +0x014  (2, 0.0025, 2)      file
    Vec3           m_secondaryWorldScale;   // +0x020  (8, 1, 8)           file
    int            m_fileWord2C;            // +0x02C  file (the editor writes 0xCDCDCDCD) [UNRESOLVED]; TerrainEditable's E+40/44 pair treats it as a float (fld)
    // +0x030 / +0x034 - the maximum and minimum height over the patches:
    // the TerrainEditable raw writer (0x1010C790) writes the max over Terrain slot +0x40 and the
    // min over slot +0x3C into exactly the two file words the loader reads here, and
    // TerrainEditable's heightmap handler (0x10109E30) recomputes them: FLT_MIN / FLT_MAX seeds
    // (0x10109E73 / 0x10109E7D), then the max of every patch's +0x98 and the min of its +0x9C. Floats.
    float          m_maxHeight;             // +0x030  0                   file
    float          m_minHeight;             // +0x034  0                   file
    int            m_patchSize;             // +0x038  128                 file
    unsigned char  m_subdividePatches;      // +0x03C  1                   file
    unsigned char  m_pad3D[3];
    int            m_patchCount;            // +0x040  patches per side    file
    // Texture dimensions for detail maps, colormaps, low-detail maps, and surrounding patches.
    int            m_patchDetailmapSize;    // +0x044  512                 file  [INFERRED] name
    int            m_patchColormapSize;     // +0x048  512                 file
    int            m_lowDetailmapSize;      // +0x04C  512
    int            m_surroundingTextureSize;// +0x050  512                        [INFERRED] name
    vc71::string   m_colormapBaseName;      // +0x054  file line 1  ("Levels/<l>/Colormaps/tx")
    vc71::string   m_lightmapBaseName;      // +0x070  file line 4  ("Levels/<l>/Lightmaps/tx")
    vc71::string   m_detailmapBaseName;     // +0x08C  file line 2  ("Levels/<l>/Detailmaps/tx")
    vc71::string   m_lowDetailmapBaseName;  // +0x0A8  file line 3  ("Levels/<l>/LowDetailmaps/tx")
    Vec3           m_terrainWaterColor;     // +0x0C4  file (after the GI colour)
    void*          m_baseGridVertexBuffer;  // +0x0D0  "Terrain_BaseGrid", (patchSize+1)^2 x 4 B   (0x100DA9C0)
    // +0x0D4: written only by slot +0xD0 (0x100D8700) and the constructor. optimizeLodIndices reads it
    // for its mesh positions; TerrainEditable overrides slot +0xD0 with its own grid builder
    // (0x10107A10, which fills +0xD0 instead), so on a TerrainEditable this stays null.
    void*          m_legacyBaseGridTable;   // +0x0D4  the pre-0x1A packed xz table               (0x100D8700)
    void*          m_lodIndexBuffer;        // +0x0D8  the LOD index buffer, m_lodIndexCount u16    (0x100DA9C0 / createLodIndexBuffer; the 2006 editor's m_pLODIndices)
    unsigned int   m_lodIndexCount;         // +0x0DC  file (0x18+), else createLodIndexBuffer's total
    // The surrounding ring's shared geometry (0x100DEAB0): the corner grid and the side grid, each a
    // vertex/index buffer pair, plus a third vertex buffer that is created and never filled.
    void*          m_sideVertexBuffer;      // +0x0E0  "Terrain_Surrounding" - the side strips
    void*          m_cornerVertexBuffer;    // +0x0E4  "TerrainEditable" - the corner blocks
    void*          m_sideIndexBuffer;       // +0x0E8
    void*          m_cornerIndexBuffer;     // +0x0EC
    void*          m_sideVertexBuffer2;     // +0x0F0  "Terrain_Surrounding", 16/8-byte stride, never uploaded; the ring draw binds it as stream 2
    unsigned int   m_sideIndexCount;        // +0x0F4
    unsigned int   m_cornerIndexCount;      // +0x0F8
    unsigned int   m_sideVertexCount;       // +0x0FC  SurroundingPatch::load reads +0xFC + 4 * isCorner
    unsigned int   m_cornerVertexCount;     // +0x100
    // Per-LOD base-grid index start, index count, and triangle count.
    int            m_lodRangeStart[3];         // +0x104  0
    int            m_lodRangeIndexCount[3];    // +0x110  0
    int            m_lodRangeTriangleCount[3]; // +0x11C  0
    unsigned char  m_gap128[0x1A8 - 0x128]; // +0x128..+0x1A8  not touched by the constructor
    int            m_primitiveType;         // +0x1A8  4 = D3DPT_TRIANGLELIST - every patch draw's primitive type (createLodIndexBuffer stores it again)
    unsigned char* m_lodTables;             // +0x1AC  85 x 400-byte LOD records (0x100DA9C0), file
    void*          m_effect;                // +0x1B0  ShaderManager "Shaders/TerrainShader"
    // +0x1B4..+0x2A0: the shader parameter handles init() looks up, in the shipped lookup order.
    void*          m_hScaleBaseUV;          // +0x1B4  scalebaseuv
    void*          m_hScaleTransXZ;         // +0x1B8  scaletransxz
    void*          m_hScaleTransY;          // +0x1BC  scaletransy
    void*          m_hStScaleTransY;        // +0x1C0  stscaletransy
    void*          m_hTexScale;             // +0x1C4  texscale
    void*          m_hYPlaneTexScaleAndFarTile; // +0x1C8  yplanetexscaleandfartile
    void*          m_hStTexScale;           // +0x1CC  sttexscale
    void*          m_hMatViewProj;          // +0x1D0  matVIEWPROJ
    void*          m_hMatView;              // +0x1D4  matVIEW
    void*          m_hTexLayer[7];          // +0x1D8..+0x1F0  TEXLAYER0..6
    void*          m_hShadowMap;            // +0x1F4  shadowmap
    void*          m_hMultipleShadowMap;    // +0x1F8  multipleshadowmap
    void*          m_hComponentSelector;    // +0x1FC  componentselector
    void*          m_hTexProjOffset;        // +0x200  texprojoffset
    void*          m_hTexProjScale;         // +0x204  texprojscale
    void*          m_hCameraPos;            // +0x208  camerapos
    void*          m_hSunColor;             // +0x20C  suncolor
    void*          m_hGIColor;              // +0x210  gicolor
    void*          m_hAmbientColor;         // +0x214  ambientcolor
    void*          m_hSinglePointColor1X;   // +0x218  SINGLEPOINTCOLOR_1X
    void*          m_hDynamicPointColor;    // +0x21C  dynamicpointcolor
    void*          m_hFogRange;             // +0x220  fogRange
    void*          m_unresolved224;         // +0x224  not written by the constructor or init() [UNRESOLVED]
    void*          m_hFogColor;             // +0x228  fogColor
    void*          m_hAmbientIlluminationRange; // +0x22C  AmbientIlluminationRange
    void*          m_hAmbientIlluminationColor; // +0x230  AmbientIlluminationColor
    void*          m_hWaterFogRange;        // +0x234  WaterFogRange
    void*          m_hWaterFogColor;        // +0x238  WaterFogColor
    void*          m_hMorphDeltaSelector;   // +0x23C  morphdeltaselector
    void*          m_hMorphDeltaAdder;      // +0x240  morphdeltaadder
    void*          m_hNearFarMorphLimits;   // +0x244  nearfarmorphlimits
    void*          m_hFarTexTiling;         // +0x248  fartextiling
    void*          m_hStFarTexTiling;       // +0x24C  stfartextiling
    void*          m_hNearTexTiling;        // +0x250  neartextiling
    void*          m_hLightViewProj;        // +0x254  lightviewproj
    void*          m_hLightViewProjOrtho;   // +0x258  lightviewprojortho
    void*          m_hViewportMap;          // +0x25C  viewportmap
    void*          m_hStTransXZ;            // +0x260  sttransxz
    void*          m_hPointLight;           // +0x264  pointlight
    void*          m_hSpotLight;            // +0x268  spotlight
    void*          m_hNightTimeMode;        // +0x26C  NightTimeMode
    void*          m_hNightOccludedSkyIntensity; // +0x270  NightOccludedSkyIntensity
    void*          m_hLightPos1;            // +0x274  LIGHTPOS1
    void*          m_hLightAttSqrInv1;      // +0x278  LIGHTATTSQRINV1
    void*          m_unresolved27C;         // +0x27C  never written [UNRESOLVED]
    void*          m_hWaterHeight;          // +0x280  WaterHeight
    void*          m_hCameraToWater;        // +0x284  CameraToWater
    void*          m_hTerrainWaterColor;    // +0x288  TerrainWaterColor
    void*          m_hColorLightTex;        // +0x28C  colorlighttex
    void*          m_hCausticsMap;          // +0x290  CausticsMap
    void*          m_hCausticsMapFrame0;    // +0x294  CausticsMapFrame0
    void*          m_hCausticsMapFrame1;    // +0x298  CausticsMapFrame1
    void*          m_hWaterScroll;          // +0x29C  WaterScroll
    void*          m_hWaterCycleTime;       // +0x2A0  WaterCycleTime
    void*          m_surroundingDeclaration;     // +0x2A4  init: word_10219AF8 / AD0 (high style), word_10219AB0 (low)
    void*          m_surroundingFullDeclaration; // +0x2A8  init: word_10219A80 - the ring draw's declaration
    // [UNRESOLVED] Legacy index-generation options. Their setters rebuild indices,
    // but the current generator does not consume the values.
    unsigned char  m_flag2AC;               // +0x2AC  1 [UNRESOLVED]
    unsigned char  m_flag2AD;               // +0x2AD  1 [UNRESOLVED]
    unsigned char  m_pad2AE[2];
    // Three std::maps with 0x18-byte nodes (_Isnil at +0x15): 4-byte key, 4-byte trivially destroyed
    // value (the erase sub_100E1800 frees nodes only). Nothing in this class inserts into them -
    // only the constructor (0x100DF367) and the destructor (0x100DDF1D..) touch +0x2B0..+0x2D3.
    vc71::map_layout m_map2B0;              // +0x2B0  std::map, 0x18-byte nodes (sub_100E1760) [UNRESOLVED key/value]
    vc71::map_layout m_map2BC;              // +0x2BC  std::map, 0x18-byte nodes
    vc71::map_layout m_map2C8;              // +0x2C8  std::map, 0x18-byte nodes
    void*          m_lowDetailTexture;      // +0x2D4  loadLowDetailTexture's texture - TEXLAYER4
    int            m_unresolved2D8;         // +0x2D8  not touched
    ::dice::hfe::world::IHeightmapCluster* m_heightmapCluster;   // +0x2DC  0; applyFrameConstants caches the "HeightmapCluster" singleton
    float          m_cameraHeightAboveTerrain; // +0x2E0  0; applyFrameConstants: |camera.y - terrain height under it|; the walk adds it to every LOD distance
    Vec3           m_sunColor;              // +0x2E4  (1,1,1); file, stored * 0.25
    Vec3           m_giColor;               // +0x2F0  (0x3ECECCCD, 0x3EB53333, 0x3E9B999A); file, stored * 0.5
    Vec3           m_ambientColor;          // +0x2FC  0 - ambientcolor; console `ambientColor` [INFERRED from the constant it feeds]
    int            m_unresolved308[6];      // +0x308..+0x31C  0
    int            m_detailTextureCount;    // +0x320  file
    Vec2           m_farSideTiling;         // +0x324  (2, 2); file
    float          m_farTopTilingLow;       // +0x32C  32.0; file (version >= 0x17, else = Hi)
    float          m_farTopTilingHi;        // +0x330  16.0; file
    float          m_farYOffset;            // +0x334  0; file
    vc71::vector_layout m_detailTextures;   // +0x338  TerrainDetailTexture* per record (first at +0x33C)
    int            m_currentPass;           // +0x348  0; drawFinal's pass cache (the +0xBC arm resets it)
    ::dice::hfe::world::IHeightmap* m_primaryHeightmap; // +0x34C  init: the cluster's primary heightmap
    int            m_heightmapSizeX;        // +0x350  init: its size
    int            m_heightmapSizeZ;        // +0x354
    unsigned char  m_drawMain;              // +0x358  1  the +0xB8 arm's main draw gate, the +0xBC arm's diffuse gate
    unsigned char  m_flag359;               // +0x359  1  the +0xBC arm's ring gate (with +0x35C)
    unsigned char  m_flag35A;               // +0x35A  1  the +0xB8 arm's slot +0xD8 (water) gate
    unsigned char  m_flag35B;               // +0x35B  0  drawFinal: "makeDynamicLightmap is obsolete!" when set
    unsigned char  m_surroundingBuilt;      // +0x35C  0; loadSurroundingPatches raises it after slot +0xCC; the +0xBC arm's third gate
    unsigned char  m_flag35D;               // +0x35D  0
    unsigned char  m_lowQualityB;           // +0x35E  0; load: 1 when the Terrain quality option is 1 (the detail-layer loader and the draws test it)
    unsigned char  m_lowQuality;            // +0x35F  0; load: 1 when the Terrain quality option is 1 (the texture loaders and the technique choice test it)
    int            m_drawDebugFlags;        // +0x360  0; drawFinal: bit 1 skips the lower-LOD merge, bit 2 enables the LOD-0 merge [UNRESOLVED writer]
    unsigned char  m_flag364;               // +0x364  0
    unsigned char  m_pad365[3];
    void*          m_indexBuffer;           // +0x368  49152 x u16, usage 520, format 101 (load's tail) - the dynamic shadow-range index buffer
    // +0x36C: the shadow-range records the gather fills (24 bytes each: start, count, shadow record,
    // node, +0x10, patch); the pass-7 draw walks them per patch. The alloc word (+0x36C) is untouched.
    vc71::vector_layout m_shadowRanges;     // +0x36C  (first at +0x370)
    int            m_unresolved37C;         // +0x37C  not touched
    QuadPatch**    m_patches;               // +0x380  patchCount^2 pointers (load)
    SurroundingPatch* m_surrounding[8];     // +0x384..+0x3A0  the eight surrounding patches (load)
    int            m_renderStyle;           // +0x3A4  2 (= choose from the caps in init); 0 hi, 1 low
    unsigned char  m_halfFloatSurrounding;  // +0x3A8  0  the ring's FLOAT16 vertex format switch
    unsigned char  m_pad3A9[3];
    unsigned short m_rawVersion;            // +0x3AC  file header word 0 (0x1A on 1.50 content) - not initialised by the constructor
    unsigned short m_rawHasVersion;         // +0x3AE  file header word 1 (1)                   - likewise; only load() writes both
    void*          m_gridDeclaration;       // +0x3B0  init: word_10219B38 (high style) / word_10219B20 (low)
    void*          m_vertexDeclaration;     // +0x3B4  the patch declaration (unk_10219A30 compact / word_10219A58)
    unsigned char  m_flag3B8;               // +0x3B8  0
    unsigned char  m_pad3B9[3];
    TerrainCuller* m_culler;                // +0x3BC  new 0x7C in init()
    int            m_hiddenDetailLayer;     // +0x3C0  -1; drawFinal skips this one layer index (a debug hide) [INFERRED]
    unsigned char  m_mergeDrawRanges;       // +0x3C4  1; the pass-0 and water draws merge contiguous LOD ranges ("TerrainLightmap"); 0 draws per node ("TerrainMorphing") [INFERRED]
    unsigned char  m_pad3C5[3];
    float          m_viewProj[16];          // +0x3C8  identity; applyFrameConstants: the view-projection product -> matVIEWPROJ
    float          m_cameraPosition[4];     // +0x408  0; applyFrameConstants: the camera as a Vec4, w = height above the terrain + 1 -> camerapos
    unsigned char  m_deviceResetPending;    // +0x418  0 in the ctor; [UNRESOLVED] - the reset store once read as this byte lands on +0x41C (the listener is Terrain+4); the name is stale
    unsigned char  m_pad419[3];
    unsigned char  m_lightingDirty;         // +0x41C  1; the sun/GI setters and the culler's LOD-distance change raise it; applyFrameConstants clears it
    unsigned char  m_pad41D[3];
    int            m_currentDetailLayer;    // +0x420  -1; drawFinal's per-frame layer cache (TEXLAYER3/6, neartextiling, componentselector)
    int            m_currentLod;            // +0x424  -1; setMorphConstants' cache; applyFrameConstants resets it with the lighting
    float          m_morphLimits[6];        // +0x428  0; applyFrameConstants: nearfarmorphlimits per LOD (two floats each, LOD 0..2)
    float          m_unresolved440;         // +0x440  -1.0f [UNRESOLVED] (the LOD-3 pair's first word, never set by a draw)
    int            m_unresolved444;         // +0x444  not touched
    // +0x448: the patches whose shadow gather produced ranges this frame; the pass-7 draw walks it.
    vc71::vector_layout m_shadowPatches;    // +0x448  (first at +0x44C)
    unsigned int   m_indexBufferCount;      // +0x458  49152
    float          m_shadowGatherThreshold; // +0x45C  4.0f; drawMain gathers only when > 0.1 [INFERRED]
    float          m_shadowExtrusionScale;  // +0x460  4.0f; drawMain: threshold * (1.643 + sunDirection.y), handed to the gather [INFERRED]
    int            m_unresolved464;         // +0x464  512
    unsigned char  m_lodTablesFromFile;     // +0x468  load: 1 on version >= 0x1A content (the constructor does not initialise it; the destructor picks its LOD-table free by it)
    unsigned char  m_pad469[3];
    int            m_unresolved46C;         // +0x46C  0
    // Reused ushort index buffer for one block of base-grid LOD generation.
    vc71::vector_layout m_lodBlockIndices;  // +0x470  (first at +0x474)
    unsigned char  m_indexBufferLocked;     // +0x480  0; lockDynamicIndexBuffer raises it
    unsigned char  m_pad481[3];
    void*          m_indexBufferBits;       // +0x484  0; the discard lock of +0x368
    unsigned char  m_flag488;               // +0x488  not touched
    unsigned char  m_flag489;               // +0x489  0
    unsigned char  m_compactVertices;       // +0x48A  0; file (version >= 0x19): the 20-byte "QuadPatch_SingleCompact" format
    unsigned char  m_pad48B;
    int            m_lowDetailTextureMode;  // +0x48C  0; loadLowDetailTexture: 1 low, 2 hi, 3 plain, 4 Default.dds
    dx9rend::Ra::RaStreamDataEditable* m_waterGridGeometry;      // +0x490  0; drawWaterCell builds the 65x65 water grid once (RaStreamDataEditable)
    dx9rend::Ra::RaStreamDataEditable* m_waterGridGeometryTemp;  // +0x494  0; its construction slot, nulled once moved into +0x490
    void*          m_materialParameters;    // +0x498  the 0x150-byte Ra material block (raMaterialParametersConstruct); +0x14C holds the water technique
    unsigned char  m_objectParameters[0x224]; // +0x49C  the 0x224-byte Ra object block (raObjectParametersConstruct)
    TerrainEnvMapRecord* m_waterEnvMap;     // +0x6C0  setupWater's envmap record (null when neither file exists)
    float          m_waterAnimSpeed;        // +0x6C4  10.0
    Vec2           m_waterScroll;           // +0x6C8  (0.1, 0.1)
    vc71::string   m_waterVolumeName;       // +0x6D0  "watervolume"; console `waterNormalMap`
    int            m_unresolved6EC[4];      // +0x6EC..+0x6F8  0
    int            m_unresolved6FC;         // +0x6FC  not touched
    float          m_unresolved700;         // +0x700  0.5f
    int            m_waterQuality;          // +0x704  -1; setupWater caches quality option 0 here unless byte_10258F70 latched it
    int            m_waterQualityApplied;   // +0x708  0; updateWaterConstants: the quality setupWater last ran for
    float          m_waterTechniqueMargin;  // +0x70C  40.0f; pickWaterTechnique: the camera must lie within this of the cell for the near technique
    // +0x710: new 0x1C: (1,1,1,0.8,0.5,0.5,0.5) - WaterColor, transparency/reflectivity above and
    // below water. An owning member [INFERRED auto_ptr-shaped]: the constructor's exchange frees the
    // previous value whenever it differs (auto_ptr::reset), and the compiler's member teardown
    // deletes it - no null test - between the +0x740 and +0x6D0 strings (0x100DDE5F).
    float*         m_waterRecord;           // +0x710
    dx9rend::Ra::RaShader* m_waterTechnique;            // +0x714  "Water2D" / "Water3D" / "WaterHighEnd3D"
    dx9rend::Ra::RaShader* m_waterDistantTechnique;     // +0x718  "WaterDistant2D" / "...3D"  (the constructor seeds "Water2D")
    dx9rend::Ra::RaShader* m_waterDistantTechnique2;    // +0x71C  the same again
    dx9rend::Ra::RaShader* m_waterSurroundingTechnique; // +0x720  "WaterSurrounding2D" / "...3D"
    TerrainWaterFrameList* m_waterTextureList; // +0x724  0; setupWater: new 0x48 list of "common/textures/<volume>.txt" frames; updateWaterConstants' 2D arm samples it (its only reader); releaseWater destroys it
    unsigned char  m_water3D;               // +0x728  0; setupWater: 1 for the 3D water styles
    unsigned char  m_pad729[3];
    float          m_waterCullHeightOffset; // +0x72C  0; drawWaterGrid: a cell is skipped when sea + this lies below the patch's minimum height [INFERRED]
    int            m_waterObjectRingCursor; // +0x730  0; drawWaterGrid resets it, drawWaterCell takes ring entry cursor++ (no bound, as shipped)
    unsigned char* m_waterObjectRing;       // +0x734  0; drawWaterCell: 256 RaObjectParameters behind a count word
    float          m_waterAlphaHeightMul;   // +0x738  0; applyFrameConstants: WaterColor.w = (20 - camera.y + sea) * 0.05 * this + m_waterAlphaAdd
    float          m_waterAlphaAdd;         // +0x73C  0.32f
    vc71::string   m_levelPath;             // +0x740  load's argument
    int            m_fileOffsetDetail;      // +0x75C  reader.tell() before the detail records
    int            m_fileOffsetPatches;     // +0x760  reader.tell() before the patches
    int            m_unresolved764;         // +0x764  not touched
};

#if defined(_M_IX86)
static_assert(sizeof(Terrain) == 0x768, "RefFactory_geom_Terrain allocates 0x768 - see 0x100B0544");
static_assert(offsetof(Terrain, m_renderPath) == 0x00C, "render path at +0x0C");
static_assert(offsetof(Terrain, m_refCount) == 0x010, "Terrain::m_refCount at +0x10");
static_assert(offsetof(Terrain, m_primaryWorldScale) == 0x014, "primary scale at +0x14");
static_assert(offsetof(Terrain, m_maxHeight) == 0x030, "max height at +0x30 - see 0x10109E73");
static_assert(offsetof(Terrain, m_minHeight) == 0x034, "min height at +0x34 - see 0x10109E7D");
static_assert(offsetof(Terrain, m_patchSize) == 0x038, "patchSize at +0x38");
static_assert(offsetof(Terrain, m_patchDetailmapSize) == 0x044, "detail-map size at +0x44 - see 0x1019FB79");
static_assert(offsetof(Terrain, m_patchColormapSize) == 0x048, "patchColormapSize at +0x48 - see 0x101A6046");
static_assert(offsetof(Terrain, m_lowDetailmapSize) == 0x04C, "lowDetailmapSize at +0x4C - see 0x1010DC96");
static_assert(offsetof(Terrain, m_surroundingTextureSize) == 0x050, "surrounding texture size at +0x50 - see 0x101A48FA");
static_assert(offsetof(Terrain, m_colormapBaseName) == 0x054, "colormap base name at +0x54");
static_assert(offsetof(Terrain, m_lowDetailmapBaseName) == 0x0A8, "low-detail base name at +0xA8");
static_assert(offsetof(Terrain, m_terrainWaterColor) == 0x0C4, "terrain water colour at +0xC4");
static_assert(offsetof(Terrain, m_baseGridVertexBuffer) == 0x0D0, "base grid VB at +0xD0");
static_assert(offsetof(Terrain, m_lodIndexCount) == 0x0DC, "LOD index count at +0xDC");
static_assert(offsetof(Terrain, m_sideVertexCount) == 0x0FC, "side vertex count at +0xFC");
static_assert(offsetof(Terrain, m_lodRangeStart) == 0x104, "LOD range starts at +0x104 - see 0x100DD135");
static_assert(offsetof(Terrain, m_lodRangeIndexCount) == 0x110, "LOD range index counts at +0x110 - see 0x100DD163");
static_assert(offsetof(Terrain, m_lodRangeTriangleCount) == 0x11C, "LOD range triangle counts at +0x11C - see 0x100DD159");
static_assert(offsetof(Terrain, m_primitiveType) == 0x1A8, "primitive type at +0x1A8");
static_assert(offsetof(Terrain, m_lodTables) == 0x1AC, "LOD tables at +0x1AC");
static_assert(offsetof(Terrain, m_effect) == 0x1B0, "effect at +0x1B0");
static_assert(offsetof(Terrain, m_hNightTimeMode) == 0x26C, "NightTimeMode handle at +0x26C");
static_assert(offsetof(Terrain, m_hWaterHeight) == 0x280, "WaterHeight handle at +0x280");
static_assert(offsetof(Terrain, m_hWaterCycleTime) == 0x2A0, "WaterCycleTime handle at +0x2A0");
static_assert(offsetof(Terrain, m_map2B0) == 0x2B0, "first map at +0x2B0");
static_assert(offsetof(Terrain, m_lowDetailTexture) == 0x2D4, "low-detail texture at +0x2D4");
static_assert(offsetof(Terrain, m_heightmapCluster) == 0x2DC, "cluster cache at +0x2DC");
static_assert(offsetof(Terrain, m_sunColor) == 0x2E4, "sun colour at +0x2E4");
static_assert(offsetof(Terrain, m_giColor) == 0x2F0, "GI colour at +0x2F0");
static_assert(offsetof(Terrain, m_detailTextureCount) == 0x320, "detail texture count at +0x320");
static_assert(offsetof(Terrain, m_farSideTiling) == 0x324, "far side tiling at +0x324");
static_assert(offsetof(Terrain, m_farYOffset) == 0x334, "far Y offset at +0x334");
static_assert(offsetof(Terrain, m_detailTextures) == 0x338, "detail texture vector at +0x338");
static_assert(offsetof(Terrain, m_currentPass) == 0x348, "pass cache at +0x348");
static_assert(offsetof(Terrain, m_primaryHeightmap) == 0x34C, "primary heightmap at +0x34C");
static_assert(offsetof(Terrain, m_drawMain) == 0x358, "draw gate at +0x358");
static_assert(offsetof(Terrain, m_indexBuffer) == 0x368, "index buffer at +0x368");
static_assert(offsetof(Terrain, m_shadowRanges) == 0x36C, "shadow ranges at +0x36C");
static_assert(offsetof(Terrain, m_patches) == 0x380, "patch table at +0x380");
static_assert(offsetof(Terrain, m_renderStyle) == 0x3A4, "render style at +0x3A4");
static_assert(offsetof(Terrain, m_rawVersion) == 0x3AC, "raw version at +0x3AC");
static_assert(offsetof(Terrain, m_gridDeclaration) == 0x3B0, "grid declaration at +0x3B0");
static_assert(offsetof(Terrain, m_vertexDeclaration) == 0x3B4, "declaration at +0x3B4");
static_assert(offsetof(Terrain, m_culler) == 0x3BC, "culler at +0x3BC");
static_assert(offsetof(Terrain, m_viewProj) == 0x3C8, "view-projection at +0x3C8");
static_assert(offsetof(Terrain, m_cameraPosition) == 0x408, "camera position at +0x408");
static_assert(offsetof(Terrain, m_deviceResetPending) == 0x418, "device-reset byte at +0x418");
static_assert(offsetof(Terrain, m_lightingDirty) == 0x41C, "lighting dirty byte at +0x41C");
static_assert(offsetof(Terrain, m_morphLimits) == 0x428, "morph limits at +0x428");
static_assert(offsetof(Terrain, m_shadowPatches) == 0x448, "shadow patches at +0x448");
static_assert(offsetof(Terrain, m_indexBufferCount) == 0x458, "index count at +0x458");
static_assert(offsetof(Terrain, m_lodTablesFromFile) == 0x468, "LOD-from-file byte at +0x468");
static_assert(offsetof(Terrain, m_lodBlockIndices) == 0x470, "the +0x470 vector (first at +0x474) - see 0x100DDE85");
static_assert(offsetof(Terrain, m_indexBufferBits) == 0x484, "index lock pointer at +0x484");
static_assert(offsetof(Terrain, m_compactVertices) == 0x48A, "compact flag at +0x48A");
static_assert(offsetof(Terrain, m_lowDetailTextureMode) == 0x48C, "low-detail mode at +0x48C");
static_assert(offsetof(Terrain, m_waterGridGeometry) == 0x490, "water grid at +0x490");
static_assert(offsetof(Terrain, m_materialParameters) == 0x498, "material block at +0x498");
static_assert(offsetof(Terrain, m_objectParameters) == 0x49C, "object block at +0x49C");
static_assert(offsetof(Terrain, m_waterEnvMap) == 0x6C0, "water envmap record at +0x6C0");
static_assert(offsetof(Terrain, m_waterAnimSpeed) == 0x6C4, "water anim speed at +0x6C4");
static_assert(offsetof(Terrain, m_waterVolumeName) == 0x6D0, "water volume name at +0x6D0");
static_assert(offsetof(Terrain, m_waterQuality) == 0x704, "water quality at +0x704");
static_assert(offsetof(Terrain, m_waterTechnique) == 0x714, "water technique at +0x714");
static_assert(offsetof(Terrain, m_waterTextureList) == 0x724, "2D water frame list at +0x724");
static_assert(sizeof(TerrainWaterFrameList) == 0x48, "setupWater allocates 0x48 for the frame list - see 0x100DA5FD");
static_assert(offsetof(TerrainWaterFrameList, frameCount) == 0x40, "frame count at +0x40 - see sub_10014F70");
static_assert(offsetof(TerrainWaterFrameList, phase) == 0x44, "phase at +0x44 - see sub_10014EC0 and 0x100DBBE8");
static_assert(offsetof(Terrain, m_waterCullHeightOffset) == 0x72C, "water cull offset at +0x72C");
static_assert(offsetof(Terrain, m_waterObjectRing) == 0x734, "water object ring at +0x734");
static_assert(offsetof(Terrain, m_levelPath) == 0x740, "level path at +0x740");
static_assert(offsetof(Terrain, m_fileOffsetDetail) == 0x75C, "detail file offset at +0x75C");
static_assert(sizeof(TerrainDetailTexture) == 0x2C, "the detail record is 0x2C bytes - see 0x100DD280");
static_assert(offsetof(TerrainDetailTexture, texture) == 0x04, "detail record texture at +0x04");
static_assert(offsetof(TerrainDetailTexture, materialId) == 0x14, "detail record material byte at +0x14 - see 0x100E1446");
static_assert(offsetof(TerrainDetailTexture, planeMap) == 0x15, "detail record plane-map byte at +0x15 - see 0x100E1456");
static_assert(offsetof(TerrainDetailTexture, topTiling) == 0x18, "detail record top tiling at +0x18 - see 0x100D8C61");
static_assert(offsetof(TerrainDetailTexture, sideTiling) == 0x1C, "detail record side tiling at +0x1C - see 0x100E14B9");
static_assert(offsetof(TerrainDetailTexture, yOffset) == 0x24, "detail record y offset at +0x24 - see 0x101AFB56");
static_assert(offsetof(TerrainDetailTexture, envMap) == 0x28, "detail record last file byte at +0x28");
static_assert(sizeof(TerrainEnvMapRecord) == 0x14, "the envmap record is 0x14 bytes - see 0x100DA210");
#endif

// The published singleton - ModuleGeom.cpp's g_Terrain (retail dword_1024B930), typed. Terrain
// derives from IBase first, so the interface subobject sits at offset zero and this adjusts nothing.
extern IBase* g_Terrain;
inline Terrain* terrain()
{
    return static_cast<Terrain*>(g_Terrain);
}

// Terrain.cpp - sub_100E11B0: copies its argument into a guarded file static and returns the
// static's address. The QuadPatch and SurroundingPatch loaders seed their centres through it, and
// applyFrameConstants the camera position it hands the heightmap cluster.
const Vec3* terrainOriginScratch(const Vec3& in);

// ModuleGeom.cpp - the `terrain.create <class>` verb's body (0x100B118C): drop any existing
// "Terrain" singleton, look up "dice.hfe.geom." + className, and register the singleton that
// publishes into g_Terrain (iid 820004, flags 2 = construct now).
void registerTerrainSingleton(const vc71::string& className);

// The reflection factory (0x100B0544) ModuleGeom::getClassList publishes for class id 821004.
IBase* __fastcall createTerrain(unsigned int iid, IBase* outer);

// TerrainConsole.cpp
bool registerTerrainConsoleObjects();
bool unregisterTerrainConsoleObjects();

// Terrain.cpp - byte_10258F70, the water quality latch updateWaterConstants writes and setupWater reads.
// Only updateWaterConstants writes it (1 at 0x100DBAEE; 0 at 0x100DBAF7 on a frame
//   that still finds +0x704 at -1). The destructor and the next constructor leave it alone, so it
//   outlives a map change and every second map's setupWater takes the 2D arm.
unsigned char& terrainWaterQualityLatched();
// Terrain.cpp - the owner-side texture release every teardown body here open-codes:
// `if (t && !--t->refCount) { sub_10014E20(t); sub_1005F360(t); }`. Unlike textureReleaseRef it
// neither evicts from the TextureManager's cache nor stores to the holder.
void terrainReleaseTexture(void* texture);
// TerrainDraw.cpp - the live "EnvMapManager" singleton (retail dword_10267A34),
// see the note at its definition. QuadPatch::drawFinal reads its +0x64 texture for pass 11.
IBase* terrainEnvMapManager();

// TerrainDraw.cpp - the shared helpers QuadPatch.cpp and SurroundingPatch.cpp draw through.

// The renderer's frame statistics block - Renderer vslot +0x6C answers base + 264; the terrain
// draws bump the six counters at +612..+632 of it.
unsigned int& terrainRendererStat(unsigned int byteOffset);
// LightManager +0x79C: the dynamic-shadow records (1176 bytes each; a Frustum sits at +16, the
// lightviewprojortho matrix at +760 and lightviewproj at +952). The tree's LightManager never fills
// it, so every reader below finds it empty; the offset is the shipped one (sub_1015C550).
vc71::vector_layout& terrainShadowRecords();
unsigned int terrainShadowRecordCount();
// sub_101C7980 - true unless one of the six frustum planes has the box entirely in front of it
// (the plane's outcode picks the box corner to test).
bool terrainFrustumIntersectsBox(const Frustum* frustum, const float* minMax);
// The sky light's direction and colour - primary slots +0x17C and +0x198 of the world::LightSource
// LightManager::getSkyLight() answers. Shape instruction-proven (no arguments, three floats at the
// answer); slot identity [UNRESOLVED] - see SkyDome.cpp's skyLightGetDirection.
const float* terrainSkyLightDirection(world::LightSource* light);
const float* terrainSkyLightColor(world::LightSource* light);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_TERRAIN_H
