// Cluster record used by the navigation graph. Loading initializes its stored data.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H

namespace dice {
namespace hfe {

namespace io { class File; }

namespace ai {

class Cluster
{
public:
    Cluster();
    ~Cluster();

    // Reads the cluster's polygon list of a GTS file; see the definition.
    void readGTSDataFromFile(unsigned int* polygons, unsigned int& count, unsigned int first,
                             io::File& file);

    /* +0x00 */ unsigned int   m_firstPolygon;        // [INFERRED] name: into ClusterGraph::m_clusterPolygons
    /* +0x04 */ unsigned int   m_numberOfPolygons;    // [INFERRED] name: resetTempMemberSizes copies it
    /* +0x08 */ unsigned short m_firstPortal;         // getClusterPortal's base
    /* +0x0A */ unsigned short m_numberOfPortals;     // getClusterPortal's stride
    /* +0x0C */ unsigned short m_firstPortalCost;     // [INFERRED] name: getCostBetweenClustersPortals' base
    /* +0x0E */ unsigned char  m_walkable;            // [INFERRED] name: map-part walkability state
    /* +0x0F */ unsigned char  m_unread0F;
};

typedef char ClusterIsTheFileRecord[sizeof(Cluster) == 16 ? 1 : -1];

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H
