// The local path result's out-of-line members. [INFERRED] file name: the image keeps them in their
// own translation unit (own static initialisation) between MeshImporterExporter's and
// QTSearchNode's.

#include "../../Main/AIPathfindingSearchTypes.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2FE9F0, __tcf_0 0x2FEA50, _GLOBAL__I 0x2FEA80).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2FE870 (C2; C1 0x2FE8C0). No step yet, no island, zero cost.
QTPath::QTPath()
    : m_i0(0)
    , m_island(-1)
    , m_i20(0.0f)
{
}

// ELF 0x2FE910 (D2; D1 0x2FE980).
QTPath::~QTPath()
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
