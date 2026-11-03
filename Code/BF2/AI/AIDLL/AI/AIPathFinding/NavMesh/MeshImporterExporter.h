// GTS mesh serialization. [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_MESHIMPORTEREXPORTER_H
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_MESHIMPORTEREXPORTER_H

#include "../../Main/IAIPathfinding.h"
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace dice { namespace hfe { namespace ai {

class MeshImporterExporter
{
public:
    // Writes a binary GTS mesh beneath the current level's GTSData/Meshes directory.
    // Returns false if opening fails; individual write results are discarded.
    static bool exportMesh(const std::vector<Vec3>& vertices,
                           const std::vector<std::pair<int, int>>& edges,
                           const std::list<triple<int, int, int>>& faces,
                           const std::string& name);
};

} } }

#endif
