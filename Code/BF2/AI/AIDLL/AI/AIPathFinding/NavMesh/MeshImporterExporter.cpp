// [INFERRED] Source placement from the translation unit's static initializer symbol.
#include "MeshImporterExporter.h"

// static init: ELF 0x2FE770, __tcf_0 0x2FE7D0, _GLOBAL__I 0x2FE800.
#include <iostream>

#include "../../Main/CommonSingletonServer.h"
#include "../../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../../../IO/File/File.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x2FE320.
bool MeshImporterExporter::exportMesh(const std::vector<Vec3>& vertices,
                                      const std::vector<std::pair<int, int>>& edges,
                                      const std::list<triple<int, int, int>>& faces,
                                      const std::string& name)
{
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();
    io::File file;
    if (!file.open(levelPath + "/GTSData/Meshes/" + name + ".gts",
                   static_cast<io::FileMode>(0), static_cast<io::FileAccess>(1), 0))
        return false;

    file.write(static_cast<unsigned int>(vertices.size()));
    file.write(static_cast<unsigned int>(edges.size()));
    file.write(static_cast<unsigned int>(faces.size()));

    for (std::vector<Vec3>::const_iterator it = vertices.begin(); it != vertices.end(); ++it)
        file.write(0);
    for (std::vector<Vec3>::const_iterator it = vertices.begin(); it != vertices.end(); ++it)
    {
        file.write(static_cast<double>(it->x));
        file.write(static_cast<double>(it->y));
        file.write(static_cast<double>(it->z));
    }

    for (std::vector<std::pair<int, int>>::const_iterator it = edges.begin(); it != edges.end(); ++it)
        file.write(0);
    for (std::vector<std::pair<int, int>>::const_iterator it = edges.begin(); it != edges.end(); ++it)
    {
        file.write(it->first);
        file.write(it->second);
    }

    for (std::list<triple<int, int, int>>::const_iterator it = faces.begin(); it != faces.end(); ++it)
        file.write(0);
    for (std::list<triple<int, int, int>>::const_iterator it = faces.begin(); it != faces.end(); ++it)
    {
        file.write(it->first);
        file.write(it->second);
        file.write(it->third);
    }
    file.close();
    return true;
}

} } }
