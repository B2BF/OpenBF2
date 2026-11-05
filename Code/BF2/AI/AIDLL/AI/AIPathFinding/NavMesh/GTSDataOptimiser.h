// Navigation mesh optimisation records.
#ifndef AI_AIDLL_GTS_DATA_OPTIMISER_H
#define AI_AIDLL_GTS_DATA_OPTIMISER_H

#include <list>
#include <vector>
#include <map>
#include <set>
#include "../../../../../Math/DoubleVec3.h"

namespace dice { namespace hfe {
// Two-double record used by the optimiser's projected edge operations; member names [INFERRED].
struct DoubleVec2 { double x, y; };
namespace io { class File; }
namespace ai {

class ClusterGraph;
class QuadTree;
class DoublePolygon;
class DestroyableMapPart;

class OptPolygon
{
public:
    OptPolygon();
    ~OptPolygon();
    void deleteMembers();
    void calculateNormal() const;
    const DoubleVec3* getNormal() const;
    unsigned char findFirstNeighbourIndex(OptPolygon*) const;
    void changeVertex(const DoubleVec3&, const DoubleVec3&);
    void changeNeighbour(const DoubleVec3&, OptPolygon*, OptPolygon*);
    void printPolygon(io::File&, int) const;
    bool checkForDuplicateVertices() const;

    // Field names are inferred from the optimiser and QuadTree readers; offsets are ia-32.
    /* +0x00 */ unsigned char m_numberOfVertices;
    /* +0x04 */ DoubleVec3* m_vertices;
    /* +0x08 */ OptPolygon** m_neighbours;
    /* +0x0C */ unsigned char m_material;
    /* +0x0D */ unsigned char m_originalMaterial;
    /* +0x10 */ mutable DoubleVec3* m_normal;
    /* +0x14 */ unsigned int m_index;
    /* +0x18 */ unsigned short m_cluster;
};

class GTSDataOptimiser
{
public:
    explicit GTSDataOptimiser(ClusterGraph* graph);
    ~GTSDataOptimiser();
    void destroyPolygon(OptPolygon* polygon);
    void buildOptMeshFromTriangles(DoublePolygon*, DoubleVec3*, unsigned int*, unsigned int*,
                                   unsigned int, QuadTree*);
    void optimise(const std::vector<DestroyableMapPart*>&);
    OptPolygon* initPolygonIteration();
    OptPolygon* getNextPolygon();
    unsigned int getNumberOfVertices() const;
    typedef std::map<OptPolygon*, std::vector<const DoubleVec3*> > VertexRemovalMap;
    void removeVerticesAndRebuildPolygon(const VertexRemovalMap&);
    OptPolygon* createPolygon() const;
    void removeTooThinTriangles();
    void setPolygonIndices(const std::vector<DestroyableMapPart*>&);
    bool twoToOneMerge(std::set<OptPolygon*>&);
    bool threeToTwoMerge();
    void cleanPolygon(OptPolygon*, unsigned int, unsigned int);
    DoubleVec2& getVectorToXZDouble(const DoubleVec3&, const DoubleVec3&, DoubleVec2&);
    DoubleVec2& getNormalDouble(const DoubleVec2&, DoubleVec2&);
    bool lineIntersect(const DoubleVec3&, const DoubleVec3&, const DoubleVec3&,
                       const DoubleVec3&, DoubleVec3&);
    bool isNeighbourOfPolygon(OptPolygon*, OptPolygon*) const;
    void validateNeighbours(OptPolygon*) const;
    void validateNeighbourRelations(const std::set<OptPolygon*>&);

    /* +0x00 */ std::list<OptPolygon*> m_polygons;
    /* +0x04 */ std::list<OptPolygon*>::iterator m_current;
    /* +0x08 */ ClusterGraph* m_clusterGraph;   // name: AIDLL_r assert 0x101F74E0 (PE +0x10)
};

} } }
#endif
