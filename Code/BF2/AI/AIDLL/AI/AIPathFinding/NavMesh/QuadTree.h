// Navigation quad tree with file loading, polygon lookup and destroyable-map-part state.

#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREE_H
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREE_H

#include <list>
#include <vector>
#include <utility>

#include "../../Main/AIDllTypes.h"      // Vec2, Vec3, DrawPrimitive
#include "QuadTreeNode.h"
#include "../../../../../Math/DoubleVec3.h"

namespace dice {
namespace hfe {

namespace io { class File; }

namespace ai {

class ClusterGraph;
class GTSDataOptimiser;
class MaterialManager;

// Polygon - one navigation-mesh polygon, 20 bytes on disk and in memory. The array is constructed
// before the read overwrites it: the ELF zeroes +8..+19 per element, the PE runs a per-element
// constructor (0x10168E40, 14 B) - the normal below. In retail that zeroing is Vec3's own
// constructor (PE 0x10011130 zeroes x, y, z); this tree's Math::Vec3 is a plain aggregate, so the
// constructor here writes it out. Field names [INFERRED] from their readers.
class Polygon
{
public:
    Polygon() { m_normal.x = 0.0f; m_normal.y = 0.0f; m_normal.z = 0.0f; }

    /* +0x00 */ unsigned char  vertexCount;  // bounds every corner / edge index (0x3068B0, 0x307560)  // name: AIDLL_r assert "p.vertexCount" 0x101871C0 (+0)
    /* +0x01 */ unsigned char  m_material;          // get/setMaterialOfPolygon (0x3081A0 / 0x3081C0)
    /* +0x02 */ unsigned short cluster;             // ClusterGraph::getClusterId (0x314E10)  // name: AIDLL_r assert "p->cluster == cId" (0x10175A40; +2)
    /* +0x04 */ unsigned int   vertexOffset;        // into m_vertexIndices and m_neighbours  // name: AIDLL_r assert "p.vertexOffset" 0x101871C0 (+4)
    /* +0x08 */ Vec3           m_normal;            // getNormalOfPolygon returns its address (0x307760)
};

typedef char PolygonIsTheFileRecord[sizeof(Polygon) == 20 ? 1 : -1];

// DestroyableMapPart - one per process-wide map-part id and per island. The vector holds every
// polygon with at least one vertex inside the part's object-local boundary. The three trailing
// bytes are paired-layout evidence: ELF +0x0C/+0x0D/+0x0E and PE +0x10/+0x11/+0x12, after the
// ports' different vector layouts. Names are [INFERRED]; behavior is recovered below.
class DestroyableMapPart
{
public:
    std::vector<unsigned int> m_polygons;           // [INFERRED] name
    unsigned char m_originalMaterial;               // [INFERRED] name
    bool m_initiallyWalkable;                        // [INFERRED] name
    bool m_destroyed;                                // [INFERRED] name
};

// Double-precision GTS polygon: ELF 0x303316 allocates 32 bytes per element.
class DoublePolygon
{
public:
    DoublePolygon() : m_normal() {}
    /* +0x00 */ unsigned char m_numberOfVertices;
    /* +0x01 */ unsigned char m_material;
    /* +0x02 */ unsigned short m_cluster;
    /* +0x04 */ unsigned int m_firstIndex;
    /* +0x08 */ DoubleVec3 m_normal;
};

typedef char DoublePolygonIsTheFileRecord[sizeof(DoublePolygon) == 32 ? 1 : -1];

class QuadTree
{
public:
    explicit QuadTree(unsigned int island);
    ~QuadTree();

    void clean();
    void saveToFile(io::File& file);
    void readFromFile(io::File& file);
    void readGTSDataFromFileDoubles(io::File& file);
    void readGTSDataFromFile(io::File& file);
    void optimiseGTSData(ClusterGraph* graph);
    void buildQuadTreeMeshFromOptMesh(GTSDataOptimiser* optimiser);
    void buildQuadTree(const std::vector<unsigned int>& polygons);
    void removeDuplicateVerticesFromOptMesh();
    void buildQuadTreesForDuplicateVertices(std::vector<std::pair<bool, unsigned int> >&,
                                            unsigned int&);
    void initialDivideQuadsAndTestForDuplicateVertices(std::vector<Vec3*>*,
        std::vector<unsigned int>*, std::vector<std::pair<Vec2, Vec2> >&);
    void divideQuadsAndTestForDuplicateVertices(const Vec2&, const Vec2&,
        const std::vector<Vec3*>&, const std::vector<unsigned int>&,
        std::vector<Vec3*>*, std::vector<unsigned int>*, std::vector<std::pair<Vec2, Vec2> >&);
    void recurseQuadTreeForDuplicateVertices(const std::vector<Vec3*>&,
        const std::vector<unsigned int>&, const std::pair<Vec2, Vec2>&, unsigned int,
        std::vector<std::pair<bool, unsigned int> >&, unsigned int&);
    void draw() const;
    void undraw() const;
    void drawPolygonIds() const;
    void drawPolygon(unsigned int polygon, float* height, float time);
    void drawPolygonEdge(unsigned int polygon, unsigned char edge) const;
    void validateNeighbourRelations() const;

    bool findPolygon(const Vec3&, unsigned int&, MaterialManager*, float) const;
    void printPolygonsInNode(const Vec3&, unsigned int&, MaterialManager*, float) const;
    Vec3 getPolygonMidPoint(unsigned int polygon);
    bool isPointInPolygon(const Vec3&, unsigned int, float, MaterialManager*) const;
    void getVerticiesOfPolygon(std::vector<const Vec3*>&, unsigned int) const;
    unsigned char getMaterialOfPolygon(unsigned int polygon) const;
    void setMaterialOfPolygon(unsigned int polygon, unsigned char material);

    // ClusterGraph's GTS importer writes cluster ids through both returned pointers.
    Polygon* getPolygon(unsigned int polygon) const;
    DoublePolygon* getPolygonDouble(unsigned int polygon) const;
    unsigned int getNeighbourOfPolygon(unsigned int polygon, unsigned char edge) const;
    bool isNeighbourOfPolygon(unsigned int polygon, unsigned int neighbour) const;
    unsigned int getSafeNeighbourOfPolygon(unsigned int polygon, unsigned char edge) const;
    const Vec3* getVertexForPolygon(unsigned int polygon, unsigned char corner) const;
    bool isVertexOfPolygon(unsigned int polygon, unsigned int vertex) const;
    const Vec3* getSafeVertexForPolygon(unsigned int polygon, unsigned char corner) const;
    unsigned int getVertexIndexForPolygon(unsigned int polygon, unsigned char corner) const;
    unsigned int getSafeVertexIndexForPolygon(unsigned int polygon, unsigned char corner) const;
    void getCentreOfPolygon(Vec3& centre, unsigned int polygon) const;
    const Vec3* getNormalOfPolygon(unsigned int polygon) const;
    bool getNeighbourIndexForPolygon(unsigned char& edge, unsigned int polygon,
                                     unsigned int neighbour) const;
    // Where the XZ ray from `origin` along `direction` leaves the polygon; see the definition.
    bool getLineIntersectionOnEdgeInPolygon(Vec3& intersection, unsigned int polygon,
                                            unsigned char& edge, unsigned int previousPolygon,
                                            const Vec3& origin, const Vec3& direction,
                                            float* edgeClearance, float*) const;

    // Flood fills from `polygon` over neighbours: the start polygon always, then every polygon a
    // vertex or edge of which lies within the radius (XYZ) / that overlaps the triangle (XZ, with
    // a 2 m height window); see the definitions.
    void getPolygonsInRadius(std::list<unsigned int>& polygons, unsigned int polygon,
                             const Vec3& centre, float radius) const;
    void getPolygonsInTriangle(std::list<unsigned int>& polygons, unsigned int polygon,
                               const Vec3& a, const Vec3& b, const Vec3& c) const;

    void initializeDestroyableMapPart(int, bool, const Vec3&, const Vec3&, const Mat4&,
                                      ClusterGraph*, bool);
    void destroyMapPart(int, ClusterGraph*);
    void restoreMapPart(int, ClusterGraph*);
    void resetDestroyableMapParts(ClusterGraph*);
    void resetOriginalMaterialOfDestroyableMapPart(int, ClusterGraph*);

private:
    friend class GTSDataOptimiser;
    friend class QuadTreeNode;
    friend class QuadTreeManager;
    friend class ClusterGraph;

    void getPolygonsInRadiusInternal(std::list<unsigned int>& polygons, unsigned int polygon,
                                     const Vec3& centre, float radius) const;
    void getPolygonsInTriangleInternal(std::list<unsigned int>& polygons, unsigned int polygon,
                                       const Vec3& a, const Vec3& b, const Vec3& c) const;
    bool withinPolygon(Polygon*, const Vec3&, float&, float) const;
    bool pointWithinPolygonEdges(Polygon*, const Vec3&) const;
    void getPolygonsWithinBoundary(const Vec3&, const Vec3&, const Mat4&,
                                   std::vector<unsigned int>&) const;
    void getPolygonsWithinBoundaryDoubles(const Vec3&, const Vec3&, const Mat4&,
                                          std::vector<unsigned int>&) const;

    // Left-hand offsets are the ELF's (sizeof 196; `operator new(0xC4)` in loadQuadTrees).
    // [UNRESOLVED] roles of the scratch vectors except m_vector18, which accumulates leaf
    // polygon ids during tree construction. The doubles store min X/Z, max X/Z and extent;
    // GTS-only arrays hold double vertices and polygons. clean frees these arrays.
    /* +0x00 */ std::vector<Vec3>         m_vector00;          // 12-byte elements, trivial destructor
    /* +0x0C */ std::vector<unsigned int> m_vector0C;
    /* +0x18 */ std::vector<unsigned int> m_vector18;
    /* +0x24 */ std::vector<unsigned int> m_vector24;
    // Set by QuadTreeManager::loadGTSData; construction and clean leave these untouched.
    /* +0x30 */ int m_minPolygonsInLeaf;  // [INFERRED] names from the map setup parameters
    /* +0x34 */ int m_maxDepthInTree;
    /* +0x38 */ int m_gtsBuildValue;      // [INFERRED] name; maximum-depth stop applies only at or above this depth
    /* +0x3C */ Vec2           m_min;                // [INFERRED] the island's bounds - Dalian Infantry
    /* +0x44 */ Vec2           m_max;                //   (-351.75, -466.75) .. (382.65, 340.53)
    /* +0x4C */ float          m_size;               // [INFERRED] the root square's edge - 807.28 there, the larger extent
    /* +0x50 */ unsigned int   m_island;             // the constructor's argument
    /* +0x54 */ unsigned int   m_numberOfNodes;
    /* +0x58 */ unsigned int   m_numberOfPolygons;   // getPolygon's bound (0x3069A0)
    /* +0x5C */ unsigned int   m_numberOfNodePolygons;
    /* +0x60 */ unsigned int   m_vertexCount;   // name: AIDLL_r assert 0x1018C4A0 (PE +0x70)
    /* +0x64 */ unsigned int   m_vertexIndexCount;   // name: AIDLL_r assert 0x1018CF30 (PE +0x74)
    /* +0x68 */ unsigned int   m_numberOfNeighbours;
    /* +0x6C */ QuadTreeNode*  m_nodes;
    /* +0x70 */ Polygon*       m_polygons;
    /* +0x74 */ unsigned int*  m_nodePolygons;       // [INFERRED] the leaves' polygon lists (71,907 entries on Dalian Infantry)
    /* +0x78 */ Vec3*          m_vertices;
    /* +0x7C */ unsigned int*  m_vertexIndices;
    /* +0x80 */ unsigned int*  m_neighbours;
    /* +0x84 */ DoubleVec3*    m_array84;            // GTS-only double-precision vertex array
    /* +0x88 */ DoublePolygon* m_array88;
    /* +0x8C */ double         m_double8C;
    /* +0x94 */ double         m_double94;
    /* +0x9C */ double         m_double9C;
    /* +0xA4 */ double         m_doubleA4;
    /* +0xAC */ double         m_doubleAC;            // GTS-computed maximum X/Z extent; ctor skips it
    /* +0xB4 */ std::vector<DestroyableMapPart*> m_destroyableMapParts;
    // [CHOSEN] mutable: the ELF's undraw is const (`_ZNK`) and still hands this to
    // IDPManager::deleteMesh by reference, which nulls it.
    /* +0xC0 */ mutable DrawPrimitive* m_debugMesh;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QUADTREE_H
