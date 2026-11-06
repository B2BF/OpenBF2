// Navigation cluster graph with GTS construction, optimisation fix-ups and portal queries.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H

#include <list>
#include <set>
#include <map>
#include <utility>

#include "../../Main/AIDllTypes.h"      // Vec3
#include "../../Main/IAIPathfinding.h"   // ai::triple
#include "Cluster.h"

namespace dice {
namespace hfe {

namespace io { class File; }

namespace ai {

class QuadTree;
class OptPolygon;
class MaterialManager;

// Portal record used by the cluster graph and its serialized format.
class ClusterPortal
{
public:
    ClusterPortal() { m_position.x = 0.0f; m_position.y = 0.0f; m_position.z = 0.0f; }

    /* +0x00 */ Vec3           m_position;          // [INFERRED] name
    /* +0x0C */ unsigned short m_neighbourCluster;  // [INFERRED] name; areNeighbours compares it
    /* +0x0E */ unsigned char  m_unread0E[2];       // [UNRESOLVED] - see the note above
    /* +0x10 */ unsigned int   m_polygon;           // [INFERRED] name: ClusterNodePath::getPoint hands it out
};

typedef char ClusterPortalIsTheFileRecord[sizeof(ClusterPortal) == 20 ? 1 : -1];

class ClusterGraph
{
public:
    struct ClusterPortalBuildInfo
    {
        // Field names are inferred from the portal builder; ia-32 offsets are 0, 4 and 8.
        unsigned int firstVertex;
        unsigned int secondVertex;
        unsigned short neighbour;
    };
    typedef std::map<unsigned int, std::map<unsigned short, unsigned int> > VertexClusterEdges;
    typedef std::map<unsigned short, std::list<std::pair<unsigned int, unsigned int> > > ClusterEdges;
    typedef std::map<unsigned short, std::list<ClusterPortalBuildInfo> > PortalBuildMap;
    typedef std::map<unsigned int, std::pair<unsigned int, bool> > DynamicPolygonMap;
    typedef std::map<unsigned short, std::list<unsigned int> > ClusterPolygonMap;

    ClusterGraph();
    ~ClusterGraph();

    void clean();
    void readFromFile(io::File& file);
    void readGTSDataFromFile(QuadTree* quadTree, io::File& file);
    void createPortals(int map, QuadTree* quadTree, float radius);
    void buildClusterEdges(const Cluster&, unsigned short, QuadTree*, VertexClusterEdges&, ClusterEdges&);
    void buildPortalsFromClusterEdges(unsigned short, QuadTree*, const VertexClusterEdges&,
                                      const ClusterEdges&, PortalBuildMap&);
    bool calculateMiddleVertex(unsigned short, const ClusterPortalBuildInfo&, QuadTree*,
                                unsigned short, MaterialManager*, Vec3&, unsigned int&) const;
    void adjustPolygonsInClusters();
    void movePolygonBetweenClusters(unsigned short, unsigned short, unsigned int);
    void addPolygonsToNewCluster(const std::set<unsigned int>&, std::list<OptPolygon*>&);
    void updateClustersAfterOptimisation(const DynamicPolygonMap&, std::list<OptPolygon*>&, ClusterPolygonMap&);
    void clearEmptyClusters(std::list<OptPolygon*>&);
    void splitClustersCutByDeepWater(const std::list<OptPolygon*>&);
    void moveDynamicObjectPolygonsToSeparateClusters(const std::list<OptPolygon*>&,
                                                     const DynamicPolygonMap&, ClusterPolygonMap&);
    void fixClusterGraphConnectivity(std::list<OptPolygon*>&, bool);
    void floodFillPolygonsInCluster(OptPolygon*, std::set<OptPolygon*>&, bool) const;
    void getAllDeepWaterAndInvalidPolygonsInCluster(unsigned short, const std::list<OptPolygon*>&,
                                                   std::set<OptPolygon*>&) const;
    void setPolygonIndices(const ClusterPolygonMap&);
    void recurseFloodFill(OptPolygon*, OptPolygon*, std::set<unsigned int>&) const;
    bool validateClusterConnectivityFromPolygon(unsigned short, OptPolygon*, unsigned int,
                                                 std::set<unsigned int>&, bool) const;
    void saveToFile(io::File& file) const;
    // Recomputes every portal-to-portal cost of the map's island with blocking searches.
    void calculatePortalCosts(int map, unsigned int island);
    void resetTempMemberSizes();
    bool allNeighboursOfClusterInSameCluster(QuadTree* quadTree, unsigned short cluster) const;
    bool checkClusterCondition(QuadTree* quadTree, unsigned short cluster) const;
    bool validateClusterGraphConnectivity(QuadTree* quadTree, bool verbose);
    void printClusterInfo(unsigned short cluster, QuadTree* quadTree, bool verbose) const;
    void drawCluster(unsigned short cluster, int costType) const;

    // [CHOSEN name] Returns the stored cluster count used by QuadTreeManager.
    unsigned int getNumberOfClusters() const { return m_numberOfClusters; }

    unsigned short getClusterId(QuadTree* quadTree, unsigned int polygon) const;
    bool areNeighbours(unsigned short cluster, unsigned short neighbour) const;
    // Portal queries. A cluster's portals are stored once per cost type: portal `index` of cost
    // type `costType` is m_portals[first + costType * count + index].
    ClusterPortal* getClusterPortal(unsigned short cluster, unsigned short costType,
                                    unsigned short index) const;
    unsigned short getPortalForNeighbour(unsigned short cluster, unsigned short costType,
                                         unsigned short neighbour) const;
    unsigned short getCorrespongingPortalInNeighbour(ClusterPortal* portal,
                                                     unsigned short costType) const;
    float getCostBetweenClustersPortals(unsigned short cluster, unsigned short first,
                                        unsigned short second, unsigned short costType) const;
    void getClusterPortals(unsigned short cluster, unsigned short costType,
                           std::list<ClusterPortal*>& portals) const;
    unsigned short findClusterPortalWithNeighbour(unsigned short cluster, unsigned short costType,
                                                  unsigned short neighbour) const;
    // ELF 0x31CE30. The setter twin of getCostBetweenClustersPortals (same pair index).
    void setCostBetweenClustersPortals(unsigned short cluster, unsigned short first,
                                       unsigned short second, unsigned short costType, float cost);
    bool updatePortalCosts(unsigned int& resource, triple<unsigned short, unsigned short,
                                                          unsigned short>& portal,
                           int map, unsigned int island, unsigned short costType);

private:
    friend class GTSDataOptimiser;
    friend class ClusterNodeManager;
    friend class QuadTree;
    friend class QuadTreeManager;
    /* +0x00 */ unsigned int*  m_clusterPolygons;           // [INFERRED] polygon indices grouped by cluster
    /* +0x04 */ unsigned int   m_polygonCount;   // (Dalian Infantry: 18,512, every polygon once)  // name: AIDLL_r assert 0x10179F30 (+4)
    /* +0x08 */ Cluster*       m_clusters;                  // getClusterPortal
    /* +0x0C */ unsigned int   m_numberOfClusters;          // loadQuadTrees' empty walk bound
    /* +0x10 */ unsigned int   m_numberOfPortals;
    /* +0x14 */ ClusterPortal* m_portals;                   // getClusterPortal
    /* +0x18 */ std::set<unsigned int>   m_set18;          // [INFERRED] removed polygon indices
    /* +0x24 */ unsigned int*            m_tempMemberSizes; // [INFERRED] name: resetTempMemberSizes
                                                             // fills it with every cluster's polygon
                                                             // count; clean frees and nulls it
    /* +0x28 */ unsigned int   m_numberOfPortalCosts;
    // float: the PE allocates it through its own checkPointer instantiation (, not the
    // uint one at the +0x00 list uses), and the shipped values are plainly floats
    // (Dalian Infantry opens 31.67, 24.51, 14.41, 56.92). Name [INFERRED] from
    // QuadTreeManager::recalculatePortalCosts -> ClusterGraph::calculatePortalCosts.
    /* +0x2C */ float*         m_portalCosts;
    /* +0x30 */ std::set<unsigned short> m_set30;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H
