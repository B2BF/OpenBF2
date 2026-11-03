// The cluster record's out-of-line members.

#include "Cluster.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x313DC0, __tcf_0 0x313E20, _GLOBAL__I 0x313E50).
#include <iostream>

#include "../../../../../IO/File/File.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x313D10 (C2; C1 0x313D20). Leaves every field unset; ClusterGraph::readFromFile overwrites
// the array it constructs.
Cluster::Cluster()
{
}

// ELF 0x313D30 (D2; D1 0x313D40).
Cluster::~Cluster()
{
}

// ELF 0x313D50. Reads one cluster's polygon list of a GTS file into `polygons` (count first, then
// the indices) and records where it starts in the graph's list (`first`) and how long it is.
void Cluster::readGTSDataFromFile(unsigned int* polygons, unsigned int& count, unsigned int first,
                                  io::File& file)
{
    file.read(count);
    file.read(polygons, count * sizeof(unsigned int));
    m_firstPolygon = first;
    m_numberOfPolygons = count;
}

} // namespace ai
} // namespace hfe
} // namespace dice
