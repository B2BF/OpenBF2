// Strategic status grid. Evidence: libaidll.so (ia-32) 0x366000..0x3663E8, a TU of its own
// without static initialisation between ScalarMap.cpp and SAIMapSquare.cpp. Its placement in
// AIStrategicMap is [INFERRED] from that address range; the header stays in AIStrategic.

#include "../AIStrategic/StatusMap.h"

#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x366000 (C2; C1 0x366160).
StatusMap::StatusMap(int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_statuses = checkPointer(new Status[width * height]);
}

// ELF 0x3662C0 (D2; D1 0x3662F0).
StatusMap::~StatusMap()
{
    if (m_statuses)
        delete[] m_statuses;
}

// ELF 0x366320.
void StatusMap::getOwnedSquares(std::vector<int>& squares) const
{
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
            if (m_statuses[y * m_width + x] == Owned)
                squares.push_back(x + y * m_width);
}

} // namespace ai
} // namespace hfe
} // namespace dice
