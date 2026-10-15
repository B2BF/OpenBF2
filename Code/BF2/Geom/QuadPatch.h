
// Primary and surrounding terrain patches, detail layers and quadtree culling.
// Each patch interface has a single deleting-destructor slot.

#ifndef GEOM_QUADPATCH_H
#define GEOM_QUADPATCH_H

#include <cstddef>
#include <vector>
#include "../Math/Vec3.h"

namespace dice {
namespace hfe {

class Frustum;
namespace dx9rend { namespace Ra { class RaShader; } }

namespace geom {

class Terrain;
class TerrainRawReader;
struct TerrainCullerNode;

// sub_101C6750 - the eight corners of an axis-aligned box from its (min, max) pair, in the shipped
// order: x fixed at min for the first four (z, y, z), then at max.
inline void aabbCorners(float* corners, const float* minMax)
{
    const float* const lo = minMax;
    const float* const hi = minMax + 3;
    corners[0]  = lo[0]; corners[1]  = lo[1]; corners[2]  = lo[2];
    corners[3]  = lo[0]; corners[4]  = lo[1]; corners[5]  = hi[2];
    corners[6]  = lo[0]; corners[7]  = hi[1]; corners[8]  = lo[2];
    corners[9]  = lo[0]; corners[10] = hi[1]; corners[11] = hi[2];
    corners[12] = hi[0]; corners[13] = lo[1]; corners[14] = lo[2];
    corners[15] = hi[0]; corners[16] = lo[1]; corners[17] = hi[2];
    corners[18] = hi[0]; corners[19] = hi[1]; corners[20] = lo[2];
    corners[21] = hi[0]; corners[22] = hi[1]; corners[23] = hi[2];
}


// The 0x3C-byte axis-aligned box the culler nodes (+0x50), the surrounding patches (+0xCC) and the
// water cell walk (a stack local) all carry. sub_101C5EB0 refreshes the derived fields from the
// (min, max) pair; sub_100E7180 answers corner `index` of the pair in the shipped bit order
// (bit 0 = max.x, bit 1 = max.y, bit 2 = max.z). Names are [INFERRED] from those two bodies.

struct TerrainAabb
{
    Vec3          min;              // +0x00
    Vec3          max;              // +0x0C
    float         cornerLength;     // +0x18  refresh: sqrt(max over corners of |corner|^2)
    float         radius;           // +0x1C  refresh: sqrt(max over corners of |corner - centre|^2)
    unsigned char dirty;            // +0x20  1 until refresh runs
    unsigned char pad21[3];
    Vec3          extent;           // +0x24  max - min
    Vec3          centre;           // +0x30  (min + max) * 0.5

    void         refresh();
    const float* corner(float out[3], int index) const;
};


// One detail layer of a QuadPatch: the index buffer that draws the patch triangles carrying detail
// texture `detailIndex`, plus its per-LOD index tables. 0x3C bytes, no vtable.
// sub_101BB710 (ctor) / sub_101BB7C0 (load, returns the detail index) / sub_101BB750 (dtor).

struct QuadPatchDetailLayer
{
    explicit QuadPatchDetailLayer(Terrain* owner);
    int  load(TerrainRawReader& reader);
    ~QuadPatchDetailLayer();
    // sub_101BB6B0 (thiscall, recursive; `this` is only passed on) - the destructor's pre-0x18 arm:
    // the four children's subtrees when the first link (+0x180) is set, then the record.
    void freeLodTableTree(unsigned char* table);

    void*          indexBuffer;      // +0x00  u16 indices, `indexCount` of them (writeonly, INDEX16)
    void*          legacyIndices;    // +0x04  the pre-0x18 host copy (freed after the upload)
    unsigned int   indexCount;       // +0x08  file
    unsigned int   fileWord0C;       // +0x0C  file [UNRESOLVED]
    unsigned char  fileBytes10[4];   // +0x10  file: 4 raw bytes (the constructor seeds the word 4) [UNRESOLVED]
    float          selectorX;        // +0x14  (detailIndex % 3 == 2) - the componentselector the diffuse pass sets
    float          selectorY;        // +0x18  (detailIndex % 3 == 1)
    float          selectorZ;        // +0x1C  (detailIndex % 3 == 0)
    int            detailIndex;      // +0x20  file: which detail texture
    Vec3           fileVec24;        // +0x24  file [UNRESOLVED]
    int            fileInt30;        // +0x30  file [UNRESOLVED]
    Terrain*       terrain;          // +0x34
    unsigned char* lodTables;        // +0x38  file (version >= 0x18): N x 400-byte LOD records
};


// QuadPatch - 0x120 bytes, vtable off_1022062C (one slot). sub_1018A0E0 / sub_1018A5B0.

class QuadPatch
{
public:
    explicit QuadPatch(Terrain* owner);
    virtual ~QuadPatch();                                            // slot 0: 0x1018E520 -> body sub_1018A220
    void load(TerrainRawReader& reader);
    void loadDetailLayers(TerrainRawReader& reader);
    void loadDetailMaps();

    // the per-frame bodies. Each takes the visible-list iterator by reference and
    //      consumes every entry that belongs to this patch.
    void draw(TerrainCullerNode**& it);                              // 0x1018BE20  the Hi_/Low_Terrain pass-0 draw (accumulation target)
    void drawFinal(TerrainCullerNode**& it);                         // 0x1018C910  the diffuse detail passes (LOD 0 per layer, then the lower LODs)
    void drawShadow();                                               // 0x1018DD00  pass 7: this patch's gathered shadow ranges
    bool gatherShadowRanges(void** lockedIndices, int* cursor);     // 0x1018BAE0  per shadow record -> sub_1018AC00 [UNREAD, stops]
    void drawWater(TerrainCullerNode**& it, std::vector<TerrainCullerNode*>* list,
                   void* materialParameters, void* objectParameters); // 0x1018D800  the Ra water submit - see the body

    // the light passes' draws over the culler's gathered list
    void drawGatheredNodes(TerrainCullerNode**& it, std::vector<TerrainCullerNode*>* list,
                           int passLod0, int passOther);            // 0x10189210  per node: its pass, the morph constants, one uncracked range
    void drawPointLight(TerrainCullerNode**& it);                    // 0x1018C7F0  "TerrainPointLight": drawGatheredNodes, pass 13 / 1

    // three QuadPatch.cpp bodies only the dormant editable terrain calls
    // TerrainEditable::update (0x1010C630) culls and sorts its patches with the first two
    // (0x1010C733 / 0x1010C740); QuadPatchEditable's VB rebuild (0x1019DF30) runs the third
    // (0x1019E150). The loaded game path never reaches them. [INFERRED] names.
    bool  isVisible(const Frustum* frustum);                         // 0x10188660   29 B  the sphere (+0xA0, +0xAC) against the frustum
    float getSquaredDistance(const Vec3& position);                  // 0x10188BE0   47 B  |position - center|^2
    void  buildNormalVertexBuffer();                                 // 0x10188680 1271 B  per-vertex normals from the heightmap into +0x5C

    Terrain*              terrain;              // +0x004
    std::vector<QuadPatchDetailLayer*> detailLayers;   // +0x008  one per detail texture (+1 slots, null-filled)
    Vec3                  center;               // +0x018  patch origin + half a patch (load)
    // Owned vertex buffers; editable patches populate height and normal streams.
    // Loaded game patches leave these three optional buffers null.
    void*                 vertexBuffer24;       // +0x024  0
    int                   unresolved28;         // +0x028  0
    void*                 legacyVertexTable;    // +0x02C  the pre-0x18 packed xz table
    int                   unresolved30[2];      // +0x030  not touched
    unsigned int          lastDrawnFrame;       // +0x038  0; drawFinal stamps the renderer's frame ordinal
    int                   unresolved3C;         // +0x03C  0
    void*                 detailMap[2];         // +0x040  "<detailmapBase>%02ix%02i_1" / "_2"
    void*                 lowDetailmap;         // +0x048  "<lowDetailmapBase>%02ix%02i" (when the .dds exists; mip class 4)
    void*                 colormap;             // +0x04C  "<colormapBase>%02ix%02i" (mip class 2)
    void*                 lightmap;             // +0x050  "<lightmapBase>%02ix%02i" (mip class 7)
    void*                 heightVertexBuffer;   // +0x054  0 (see +0x024)
    int                   unresolved58;         // +0x058  not touched
    void*                 normalVertexBuffer;   // +0x05C  0 (see +0x024)
    void*                 vertexBuffer;         // +0x060  "QuadPatch_SingleCompact", (patchSize+1)^2 x 20/24 B
    int                   patchIndex;           // +0x064  (gridX + n/2) + (gridZ + n/2) * patchCount
    int                   gridX;                // +0x068  file (signed, centred on 0)
    int                   gridZ;                // +0x06C  file
    int                   worldX;               // +0x070  gridX * patchSize
    int                   worldZ;               // +0x074  gridZ * patchSize
    int                   scaledX;              // +0x078  (int)(worldX * primaryScale.x)
    int                   scaledZ;              // +0x07C  (int)(worldZ * primaryScale.z)
    int                   scaledXEnd;           // +0x080  scaledX + (int)(patchSize * primaryScale.x)
    int                   scaledZEnd;           // +0x084  scaledZ + (int)(patchSize * primaryScale.z)
    float                 worldXf;              // +0x088  worldX * primaryScale.x   (the Vec3 the centre is seeded from)
    float                 zero8C;               // +0x08C  0
    float                 worldZf;              // +0x090  worldZ * primaryScale.z
    float                 zero94;               // +0x094  0
    // +0x098..+0x0AC - the patch's height range and bounding sphere (R0; the
    // tree had +0x98 as an [UNRESOLVED] int). QuadPatchEditable's bounds body (0x1019E160) computes
    // all four - the max and the min of the vertex heights (seeded FLT_MIN / FLT_MAX), then
    // D3DXComputeBoundingSphere over the vertices into +0xA0 / +0xAC - and the TerrainEditable raw
    // writer (0x1010C790) writes them in this file order. Terrain slots +0x40 / +0x3C read the two
    // heights (fld); the water cell walk skips cells whose water lies below the minimum;
    // isVisible tests the sphere.
    float                 maxHeight;            // +0x098  file
    float                 minHeight;            // +0x09C  file
    Vec3                  sphereCentre;         // +0x0A0  file
    float                 sphereRadius;         // +0x0AC  file
    int                   unresolvedB0;         // +0x0B0  not touched by the plain patch; QuadPatchEditable's VB rebuild stores the byte 1 [UNRESOLVED]
    int                   unresolvedB4[4];      // +0x0B4..+0x0C0  0
    int                   currentDetailMap;     // +0x0C4  drawFinal's TEXLAYER2 cache (-1 at its start)
    // +0x0C8..+0x10B: the Ra light record the water submit fills (RaShader.cpp's raLightRecord
    // shape): [3..5] the sky light direction, [6..8] its colour, [14] never seeded, [15] = [16] = 1.
    float                 lightRecord[17];      // +0x0C8
    unsigned char         hasNoDetailLayers;    // +0x10C  1 when no detail layer index came in (Terrain slot +0x4C)
    unsigned char         pad10D[3];
    int                   unresolved110;        // +0x110  0
    int                   unresolved114;        // +0x114  -1
    int                   fileEndOffset;        // +0x118  reader.tell() after the vertex data
    int                   maxDetailLayerIndex;  // +0x11C  -1, then the largest layer index < detailTextureCount
};


// SurroundingPatch - 0x168 bytes, vtable off_10220778 (one slot). sub_1018E7D0 / sub_1018EFB0 /
// Eight of them ring the primary terrain; `slot` (0..7) picks the placement case.

class SurroundingPatch
{
public:
    explicit SurroundingPatch(Terrain* owner);
    virtual ~SurroundingPatch();                                     // slot 0: 0x1018F870 -> body sub_1018E910
    void load(int slot, TerrainRawReader& reader);
    void setupPlacement();
    bool isVisible(int unused);                                      // 0x1018F420  camera inside, far-plane reject, then the slot's edge tests
    void draw(int unused);                                           // 0x1018F7B0  textures, sttransxz, the two streams, pass 0, one draw

    Terrain*              terrain;              // +0x004
    void*                 xzBytes;              // +0x008  file: 4 x vertexCount bytes (packed xz per vertex)
    int                   unresolved0C[2];      // +0x00C  not touched
    void*                 lowDetailmap;         // +0x014  "<lowDetailmapBase>_s%i"
    void*                 colormap;             // +0x018  "<colormapBase>_s%i"
    void*                 xzVertexBuffer;       // +0x01C  "SurroundingPatch", vertexCount x 4 B
    void*                 heightVertexBuffer;   // +0x020  "SurroundingPatch", vertexCount x 8 (half) / 12 (float) B
    int                   originX;              // +0x024  placement, in world units
    int                   originZ;              // +0x028
    int                   endX;                 // +0x02C  origin + span
    int                   endZ;                 // +0x030
    int                   secondaryScaleX;      // +0x034  (int) secondaryWorldScale.x
    int                   span;                 // +0x038  (int)primaryScale.x * (heightmapSizeX - 1)
    int                   spanPlusOne;          // +0x03C
    int                   sideCount;            // +0x040  span / secondaryScaleX + 1
    int                   sideCountSquared;     // +0x044
    int                   unresolved48;         // +0x048  not touched
    Vec3                  center;               // +0x04C  origin + span * 0.5 (x, z)
    float                 originXf;             // +0x058  the origin as (x, 0, z, 0) - the Vec3 the centre is seeded from
    float                 zero5C;               // +0x05C  0
    float                 originZf;             // +0x060
    float                 zero64;               // +0x064  0
    float                 fileMaxY;             // +0x068  file [INFERRED] - the top of the bounds
    float                 fileMinY;             // +0x06C  file [INFERRED] - the bottom of the bounds
    Vec3                  boundsCenter;         // +0x070  setupPlacement: the box centre
    float                 boundsRadius;         // +0x07C  setupPlacement: half the box diagonal
    int                   unresolved80;         // +0x080  not touched
    int                   slot;                 // +0x084  0..7
    int                   isCorner;             // +0x088  1 for the four corner placements, 0 for the sides
    float                 placement[16];        // +0x08C  identity, then the placement rows (sub_10079460); the draw's sttransxz
    TerrainAabb           bounds;               // +0x0CC  (originX, minY, originZ) .. (endX, maxY, endZ); dirty stays 1 - nothing refreshes it
    float                 corners[24];          // +0x108  the eight bounds corners (aabbCorners of +0xCC)
};


// Quadtree nodes use array-order children 1+4k..4+4k. Patch-level nodes reference root
// LOD records; descendants reference subpatch LODs. Every node carries its owning patch.

struct TerrainCullerNode
{
    TerrainCullerNode();
    // the occlusion query's reference, then ~vector on +0x08. Inlined, too, into the
    // node array's vector deleting destructor (sub_100E75B0).
    ~TerrainCullerNode();

    QuadPatch*            patch;                // +0x000  patches[fileByte] or null (0xFF)
    unsigned char*        lodTable;             // +0x004  the terrain's LOD table (leaf) or the parent's child table (load)
    std::vector<unsigned char*> layerLodTables; // +0x008  per detail layer of `patch`: the layer's LOD table (leaf) or the parent's child table
    // +0x018: a dx9rend Query (QueryManager.cpp) - proven by the pre-0x18 node builder
    // sub_100E4690, which stores QueryManager::createQuery's answer here (0x100E4E9E..0x100E4EA3),
    // and by the destructor, which releases it through the Query's inline --ref dance around its
    // destructor sub_10003FA0. Nodes loaded from 0x18+ files never get one (0 from the
    // constructor), so on 1.50 content the release is never taken.
    void*                 occlusionQuery;       // +0x018  0
    TerrainCullerNode*    child[4];             // +0x01C  quad-tree children (null on the last level)
    TerrainCullerNode*    neighbour[8];         // +0x02C  from eight u16 file indices (0xFFFF = none)
    unsigned char         allNeighbours;        // +0x04C  1 unless a neighbour index was 0xFFFF
    unsigned char         pad4D[3];
    TerrainAabb           bounds;               // +0x050  file (min, max); refreshed lazily by the walks (dirty = 1 after load)
    float                 corners[24];          // +0x08C  the eight bounds corners (aabbCorners of +0x50)
    unsigned char         fileByteEC;           // +0x0EC  file: take the LOD tables from the parent
    unsigned char         hasPatchList;         // +0x0ED  file: a patch-level node - take the LOD tables from the terrain and the patch's layers
    unsigned char         fileByteEE;           // +0x0EE  file: the walk pushes this node into the visible list instead of descending
    unsigned char         padEF;
    int                   childrenWalked;       // +0x0F0  0; the walk counts the children it took through the LOD block
    unsigned char         visible;              // +0x0F4  0; the walk's frustum answer
    unsigned char         padF5[3];
    int                   lod;                  // +0x0F8  2 (constructor), 0 (load); the walk's 0..2
    unsigned char         cameraInside;         // +0x0FC  0; the camera's xz lies inside the bounds
    unsigned char         padFD[3];
    int                   crackMask;            // +0x100  0; the crack fixup's neighbour bits (1, 2, 4, 8) that pick the LOD table column
    int                   unresolved104;        // +0x104  -1
    int                   unresolved108;        // +0x108  not touched
    int                   unresolved10C;        // +0x10C  0
};

class TerrainCuller
{
public:
    TerrainCuller();
    // the nodes (delete[] of the file's array on 0x18+ content, else the pre-0x18
    // per-node tree), then ~vector on +0x44 and +0x34.
    ~TerrainCuller();
    void init(Terrain* owner, int a, int b);                         // (+0x4, +0x10, +0x14)
    // sub_100E3D10 (thiscall, recursive) - the pre-0x18 arm: a node's four subtrees when its first
    // child is set, then the node (0x100E3CC0) and its free.
    void deleteNodeTree(TerrainCullerNode* node);
    void load(TerrainRawReader& reader);
    void loadNode(TerrainRawReader& reader, TerrainCullerNode* node);
    std::vector<TerrainCullerNode*>* visiblePatches()  { return &visible; }
    std::vector<TerrainCullerNode*>* gatheredPatches() { return &gathered; }

    // the per-frame bodies
    void update();                                                   // 0x100E7080  slot +0x28 of Terrain calls it: LOD distance, the walk, the crack fixup
    void walkRoot(TerrainCullerNode* node);                          // 0x100E6D50  the root's sphere/corner tests, then the mode dispatch
    void walkMode3(TerrainCullerNode* parent);                       // 0x100E59A0  mode 3 (the shipped default): per child - tests, LOD, push or descend
    static void setLod(TerrainCullerNode* node, int lod);            // 0x100E2D00  the node and its visible children; 3+ clamps to 2
    void fixCracks();                                                // 0x100E34E0  the neighbour-LOD mask per visible node
    void gatherSphere(const float* centre, float radius);            // 0x100E6D10  every node with a patch within `radius` into `gathered`
    void gatherNode(TerrainCullerNode* node);                        // 0x100E45B0  its recursive walk
    void gatherVisibleSphere(const float* centre, float radius);     // 0x100E6C70  the light passes' variant: the visible leaves within `radius` into `gathered`
    void gatherVisibleNode(TerrainCullerNode* node);                 // 0x100E4370  its recursive walk

    int                   lodDistance;          // +0x000  not touched by the constructor; update: the current LOD distance
    Terrain*              terrain;              // +0x004
    TerrainCullerNode*    nodes;                // +0x008  nodeCount x 272 B (a count word sits ahead of them)
    int                   unresolved0C;         // +0x00C  not touched
    int                   param10;              // +0x010  1; init: 32
    int                   param14;              // +0x014  1; init: 64 - the LOD distance floor
    int                   param18;              // +0x018  100 - the LOD distance per unit of zoom (x 0.75)
    int                   lastLodDistance;      // +0x01C  not touched by the constructor; update dirties the terrain lighting when it changes
    float                 lodFactor20;          // +0x020  (0.4039..)  morph limit factors, see Terrain::applyFrameConstants
    float                 lodFactor24;          // +0x024  0.9f
    float                 lodFactor28;          // +0x028  2.0f
    float                 lodFactor2C;          // +0x02C  2.7f
    float                 lodFactor30;          // +0x030  3.0f - LOD 2 beyond lodDistance * this
    std::vector<TerrainCullerNode*> visible;    // +0x034  the visible node list the draws walk
    std::vector<TerrainCullerNode*> gathered;   // +0x044  the sphere gathers' node list
    Frustum*              frustum;              // +0x054  update: the render view's frustum
    const float*          cameraPosition;       // +0x058  update: row 3 of the render view's +0x64 matrix
    const float*          gatherCentre;         // +0x05C  gatherSphere's centre
    float                 gatherRadius;         // +0x060
    float                 gatherRadiusSq;       // +0x064
    int                   param68;              // +0x068  2
    int                   param6C;              // +0x06C  3 - the walk mode (1..6); only 3 is written
    unsigned char         flag70;               // +0x070  1 - run the crack fixup
    unsigned char         flag71;               // +0x071  1 - test the camera against the root's box
    unsigned char         pad72[2];
    int                   nodeCount;            // +0x074  file
    unsigned char         versionFlag78;        // +0x078  1 when the file version >= 0x1A
    unsigned char         pad79[3];
};

#if defined(_M_IX86)
static_assert(sizeof(TerrainAabb) == 0x3C, "the box is 0x3C bytes - see 0x101C5EB0");
static_assert(offsetof(TerrainAabb, dirty) == 0x20, "box dirty byte at +0x20");
static_assert(offsetof(TerrainAabb, centre) == 0x30, "box centre at +0x30");
static_assert(sizeof(QuadPatchDetailLayer) == 0x3C, "detail layer is 0x3C bytes - see 0x1018A3A0");
static_assert(offsetof(QuadPatchDetailLayer, terrain) == 0x34, "detail layer terrain at +0x34");
static_assert(offsetof(QuadPatch, terrain) == 0x04, "QuadPatch terrain at +0x04 - one vptr ahead of it");
static_assert(offsetof(QuadPatch, detailLayers) == 0x08, "QuadPatch detail layer vector at +0x08");
static_assert(sizeof(SurroundingPatch) == 0x168, "SurroundingPatch is 0x168 bytes - see 0x100D8BA0");
static_assert(offsetof(SurroundingPatch, terrain) == 0x04, "SurroundingPatch terrain at +0x04");
static_assert(offsetof(SurroundingPatch, originXf) == 0x58, "SurroundingPatch origin vector at +0x58");
static_assert(offsetof(SurroundingPatch, slot) == 0x84, "SurroundingPatch slot at +0x84");
static_assert(offsetof(SurroundingPatch, placement) == 0x8C, "SurroundingPatch transform at +0x8C");
static_assert(offsetof(SurroundingPatch, bounds) == 0xCC, "SurroundingPatch bounds at +0xCC");
static_assert(offsetof(SurroundingPatch, corners) == 0x108, "SurroundingPatch corners at +0x108");
static_assert(offsetof(TerrainCullerNode, layerLodTables) == 0x08, "node layer tables at +0x08");
static_assert(offsetof(TerrainCuller, visible) == 0x34, "visible list at +0x34");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_QUADPATCH_H
