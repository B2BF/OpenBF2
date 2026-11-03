// Owns navigation quad trees and loads their per-map data.

#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREEMANAGER_H
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREEMANAGER_H

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "../../Main/AIDllTypes.h"      // DrawPrimitive, Vec3, Mat4

namespace dice {
namespace hfe {
namespace ai {

class ClusterGraph;
class ClusterPortal;
class MaterialManager;
class QuadTree;

class QuadTreeManager
{
public:
    // One debug mesh: up to 0x5555 polygons of the islands in one static mesh. 40 bytes on the
    // ELF (operator new(0x28)), no virtual members. Field names [INFERRED] from buildDebugMeshes,
    // which allocates and fills every array, and draw, which hands them to createStaticMesh.
    class DebugMesh
    {
    public:
        DebugMesh();
        void lockSize();

        /* +0x00 */ int            m_numberOfVertices;
        /* +0x04 */ int            m_numberOfPolygons;
        /* +0x08 */ int            m_numberOfIndices;
        /* +0x0C */ unsigned int*  m_firstIndices;      // per polygon: where its indices start
        /* +0x10 */ unsigned int*  m_vertexCounts;      // per polygon
        /* +0x14 */ Vec3*          m_vertices;
        /* +0x18 */ unsigned int*  m_indices;
        /* +0x1C */ unsigned char* m_materials;         // per polygon
        /* +0x20 */ unsigned int*  m_colours;           // per polygon, the mesh's colour argument
        /* +0x24 */ DrawPrimitive* m_mesh;              // the static mesh handle while drawn
    };

    QuadTreeManager();
    ~QuadTreeManager();

    // the island accessors - the only query surface that needs nothing else
    QuadTree* getIsland(unsigned int) const;
    ClusterGraph* getClusterGraph(unsigned int) const;
    unsigned int getNumberOfIslands() const;

    // everything below forwards into QuadTree / ClusterGraph, or builds debug geometry
    void destroyQuadTree(QuadTree *);
    void destroyQuadTrees();
    void saveQuadTrees(const std::string&);
    void loadQuadTrees(const std::string&);
    bool loadGTSData(const std::string&, int, int, int, float);
    void recalculatePortalCosts(int);
    void draw() const;
    void undraw() const;
    void drawCluster();
    void undrawCluster() const;
    void drawClusterNeighbourIndices(const Vec3&);
    void buildDebugMeshes(const std::string&);
    void buildDebugClusterMeshes(const std::string&);
    void loadGTSDebugMeshes(const std::string&);
    void unloadDebugMeshes();
    void unloadDebugClusterMeshes();
    void triangulateDebugMeshes(std::list<DebugMesh*>&);
    // Full-width island index, or UINT_MAX. Return type comes from callers; Itanium does not
    // mangle it. Both methods are non-const (`_ZN`, not `_ZNK`) in the ELF.
    unsigned int findPolygon(const Vec3&, unsigned int&, MaterialManager*, float);
    unsigned int findPolygon(const Vec3&, unsigned int&, unsigned int, unsigned int,
                             MaterialManager*, float);
    void printPolygonsInNode(const Vec3&, unsigned int&, MaterialManager *, float);
    unsigned short getCluster(unsigned int, unsigned int) const;
    void getClusterPortals(unsigned int, unsigned short, unsigned short, std::list<ClusterPortal*>&) const;
    void getPolygonsInRadius(std::list<unsigned int>&, unsigned int, unsigned int, const Vec3&, float) const;
    void getPolygonsInTriangle(std::list<unsigned int>&, unsigned int, unsigned int, const Vec3&, const Vec3&, const Vec3&) const;
    void initializeDestroyableMapPart(int, bool, const Vec3&, const Vec3&, const Mat4&);
    void destroyMapPart(int);
    void restoreMapPart(int);
    void resetOriginalMaterialOfDestroyableMapPart(int);
    void resetDestroyableMapParts();
    void validateNeighbourRelations() const;

    // Offsets describe the original 32-byte ELF object; STL layouts vary by compiler.

    // One quad-tree/cluster-graph pointer pair per island; each original x86 pair is eight bytes.
    /*  +0 */ std::vector<std::pair<QuadTree*, ClusterGraph*> > m_islands;
    // Island count stored in the file, initialized by load or save. Runtime count queries
    // use m_islands.size() instead; construction leaves this field unset.
    /* +12 */ int m_numberOfIslands;
    /* +16 */ std::list<DebugMesh*> m_debugMeshes;               // node 12
    /* +20 */ std::list<DebugMesh*> m_debugClusterMeshes;        // node 12
    /* +24 */ std::list<std::pair<DrawPrimitive*, DrawPrimitive*> > m_drawnPairs;   // node 16 [INFERRED]
    /* +28 */ std::list<DrawPrimitive*> m_drawn;                 // node 12 [INFERRED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREEMANAGER_H
