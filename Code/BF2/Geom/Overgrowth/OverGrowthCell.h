// Internal overgrowth grid-cell, patch and batched-stream declarations.
#ifndef BF2_GEOM_OVERGROWTHCELL_H
#define BF2_GEOM_OVERGROWTHCELL_H

#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "../MeshTemplate.h"                   // MeshBounds
#include "../../Math/Mat4.h"
#include "../../RendDX9/Ra/RaShader.h"         // RaObjectParameters
#include "../../RendDX9/Ra/RaStreamDataEditable.h"

namespace dice { namespace hfe { namespace geom {


// One patch of the countX * countZ grid at OverGrowth +0xE0 - 0x98 bytes in the PE, allocated with
// operator new[] (the element count ahead of the first element). Construction sub_100CAFF0: position
// and the vector zeroed, the bounds zeroed with their dirty byte set; the coverage is NOT constructed.
// The destructor is its vector's. Field names are [INFERRED].
struct OverGrowthPatch
{
    OverGrowthPatch();

    Vec3                     position;       // +0x00  the patch corner, y the terrain height
    OverGrowthInstanceVector instances;      // +0x0C
    float                    coverage[16];   // +0x1C  share of the patch's texels per material id
    MeshBounds               bounds;         // +0x5C  60 bytes, ends at +0x98
};


// Cell vertex stream plus object parameters. Extra buffer-management slots extend the
// current base declaration while retaining compatibility positions.

class OverGrowthCellMesh : public dx9rend::Ra::RaStreamDataEditable
{
public:
    OverGrowthCellMesh(unsigned int vertexCapacity, unsigned int indexCapacity,
                       const std::vector<const dx9rend::Ra::RaVertexElementDesc*>& elements);

    void* destroy(unsigned char flags) override;                     // +0x00
    virtual void releaseBuffers();                                    // +0x30  10048D60 [FRONTIER]
    virtual unsigned int getFreeVertexCount() const;                  // +0x34
    virtual unsigned int getVertexCapacity() const;                   // +0x38
    virtual unsigned int getIndexCapacity() const;                    // +0x3C
    virtual void createBuffers();                                     // +0x40  10048C70 [FRONTIER]

    // +0x1A4 in the PE: the Ra object block, constructed in place by sub_100458E0.
    alignas(dx9rend::Ra::RaObjectParameters)
    unsigned char m_objectParameters[sizeof(dx9rend::Ra::RaObjectParameters)];

    dx9rend::Ra::RaObjectParameters& objectParameters()
    { return *reinterpret_cast<dx9rend::Ra::RaObjectParameters*>(m_objectParameters); }
};


// Visible grid cell with bounds, camera fade, instance list, and leaf/trunk streams.
// The eight-slot interface overlays its abstract base (0x2AC bytes in the PE). Field names are
// [INFERRED].

struct OverGrowthCell
{
    OverGrowthCell();                                                 // 100D0C90 (+ base 1015E6D0)

    virtual void* destroy(unsigned char flags);                       // +0x00
    virtual bool  appendGeometry(void* geometryTemplate, const Mat4* transform, float scale,
                                 int unused, float shadow);           // +0x04
    virtual void  lock();                                             // +0x08
    virtual void  unlock();                                           // +0x0C
    virtual void  draw(IRenderView* view, const Vec3* position, float fade); // +0x10
    virtual unsigned int getVertexCapacity();                         // +0x14
    virtual unsigned int getIndexCapacity();                          // +0x18
    virtual void  clearFarList();                                     // +0x1C

    unsigned char      visible;           // +0x04  in range and built this frame
    unsigned char      built;             // +0x05  its streams hold the current cell
    unsigned char      empty;             // +0x06  1 - no far instance to draw
    unsigned char      pad07;
    Vec3               corner;            // +0x08  the cell's corner, y the terrain height
    float              size;              // +0x14  the cell edge
    float              fadeFactor;        // +0x18  the distance / height fade
    float              cameraDistance;    // +0x1C  the camera's distance to the centre
    MeshBounds         bounds;            // +0x20
    int                cellX;             // +0x5C  world cell, 0x2CEE9D7 when unplaced
    int                cellZ;             // +0x60
    unsigned char      hasNear;           // +0x64  some instances are drawn as full meshes
    unsigned char      pad65[3];
    // +0x68 - the base's Ra object block (sub_100458E0 / sub_1004D900).
    alignas(dx9rend::Ra::RaObjectParameters)
    unsigned char      objectParameters[sizeof(dx9rend::Ra::RaObjectParameters)];
    std::vector<OverGrowthInstance*> farList;   // +0x28C  the instances batched into the streams
    unsigned int       unresolved29C;     // +0x29C  zeroed by the constructor and rebuildCells [UNRESOLVED]
    OverGrowthPatch*   lastPatch;         // +0x2A0  the patch updateCell last took
    OverGrowthCellMesh* leaf;             // +0x2A4  the non-"tile_" materials
    OverGrowthCellMesh* trunk;            // +0x2A8  the "tile_" materials
};

// The allocation postLoadResources makes per grid slot (0x100CD6DE..0x100CD6FB): sub_1005F320(0x2AC), then
// the constructor when the heap answered (OverGrowthCells.cpp).
OverGrowthCell* createOverGrowthCell();

// sub_1015EEB0 - the abstract base's updateCells: the camera ring of N x N cells and each cell's build.
// OverGrowth::updateCells calls it first; render calls it directly when +0xD5 is clear
// (OverGrowthCells.cpp).
void overGrowthBaseUpdateCells(OverGrowth* self, IRenderView* view);

} } }
#endif
#endif
