// Compiled road geometry loaded from a road template. [INFERRED] header placement.
// The IRoad interface precedes the IGeometry/GeometryInstanceData subobject.
// loadMesh initializes the chunk counts, visibility and file version before rendering;
// the constructor alone does not initialize all of those fields.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_ROADCOMPILED_H
#define BF2_GEOM_ROADCOMPILED_H

#include "RoadCompiledTemplate.h"
#include "GeometryInstanceData.h"
#include "MeshTemplate.h"            // MeshBounds
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"

#include <stddef.h>
#include <vector>

namespace dice {
namespace hfe {

class Frustum;

namespace geom {


// Per-chunk visibility bits (the shipped std::vector<bool>).

typedef std::vector<bool> RoadChunkBits;

class RoadCompiled
{
public:
    explicit RoadCompiled(RoadCompiledTemplate* owner);
    ~RoadCompiled();

    // IRoad, primary table 0x1021E9E0
    int   addRef();                                        // +0x00 0x10103C00  returns the new count
    int   getRef() const;                                  // +0x04
    int   release();                                       // +0x08
    void* queryInterface(unsigned int iid);                // +0x0C
    // +0x10 0x1015AD30 is `ret` - see the table.
    void  updateChunkVisibility(const Frustum* frustum);   // +0x14
    bool  anyChunkVisible() const { return m_anyChunkVisible != 0; }   // +0x18

    // IGeometry bodies this class owns (table 0x1021E8E8, subobject +4)
    void  addToRenderQueue();                              // +0x18 (three ignored args)
    void  setTransform(const Mat4* transform);             // +0x50
    void* destroy(unsigned int flags);                     // +0x34

    // reached directly
    void  loadMesh(const std::string& file);              // Object.geometry.loadMesh
    void  draw();                                          // RoadCompiledRenderer's walk

    // data, every offset asserted below
    void* const*           m_roadVptr;             // +0x000
    GeometryInstanceData   m_geometry;             // +0x004 .. +0x033
    int                    m_refCount;             // +0x034
    RoadCompiledTemplate*  m_template;             // +0x038
    unsigned int           m_vertexCount;          // +0x03C
    int                    m_indexCount;           // +0x040  not written by the constructor
    void*                  m_vertexBuffer;         // +0x044
    void*                  m_indexBuffer;          // +0x048
    Vec3                   m_origin;               // +0x04C
    float                  m_radius;               // +0x058
    MeshBounds             m_worldBounds;          // +0x05C
    MeshBounds             m_localBounds;          // +0x098
    Mat4                   m_transform;            // +0x0D4
    unsigned int           m_chunkCount;           // +0x114  not written by the constructor
    std::vector<unsigned int> m_chunkStarts;       // +0x118
    std::vector<unsigned int> m_chunkIndexCounts;  // +0x128
    std::vector<float>     m_chunkRadii;           // +0x138
    std::vector<Vec3>      m_chunkCentres;         // +0x148
    RoadChunkBits          m_chunkVisible;         // +0x158
    unsigned char          m_anyChunkVisible;      // +0x16C  not written by the constructor
    unsigned char          m_pad16D[3];
    std::string           m_string170;            // +0x170  [UNRESOLVED] no reader or writer
    unsigned short         m_versionMinor;         // +0x18C
    unsigned short         m_versionMajor;         // +0x18E
    int                    m_order;                // +0x190

private:
    RoadCompiled(const RoadCompiled&);
    RoadCompiled& operator=(const RoadCompiled&);
};

#if defined(_M_IX86)
static_assert(offsetof(RoadCompiled, m_geometry) == 0x004, "IGeometry subobject at +0x04");
#endif

// IGeometry's address for an instance - the pointer createInstance answers and the component the
// object holds (IGeometry subobject = object + 4).
inline void* roadCompiledGeometry(RoadCompiled* road)
{
    return road ? static_cast<void*>(&road->m_geometry) : 0;
}

// RoadCompiledRenderer.cpp - 0x10145210, the queue insert addToRenderQueue calls on the singleton.
void roadCompiledRendererAdd(RoadCompiled* road);
// RoadCompiledRenderer.cpp - the singleton's +0x8C view-distance scale (1.3f), which the per-chunk
// visibility reads straight off g_RoadCompiledRenderer (0x1015AEA3).
float roadCompiledRendererViewDistanceScale();
// RoadCompiledRenderer.cpp - the singleton's RenderPathListener subobject (retail +0x24), whose +0x0C
// renderFrame calls directly each frame. Unguarded: the caller tests the singleton.
void* roadCompiledRendererListener();

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ROADCOMPILED_H
#endif // BF2_IMAGE_RENDDX9
