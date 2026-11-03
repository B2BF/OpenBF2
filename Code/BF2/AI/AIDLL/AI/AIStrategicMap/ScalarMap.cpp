// Scalar grid smoothing and ranking. Evidence: libaidll.so (ia-32) 0x364640..0x365FF8; the
// smoothing error names AI/AIDLL/AI/AIStrategicMap/ScalarMap.cpp. The TU has no static
// initialisation in the image.

#include "ScalarMap.h"

#include <queue>
#include <vector>

#include "../AIUtils/checkPointer.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF: 8-byte heap element {index, value}. std::less<PQElem> (the instantiated comparator) orders
// by descending value, so the priority queue's top is the smallest kept value.
struct PQElem
{
    PQElem(int index, float value) : m_index(index), m_value(value) {}
    bool operator<(const PQElem& other) const { return m_value > other.m_value; }

    int m_index;
    float m_value;
};

} // namespace

// ELF 0x364640 (C2; C1 0x3647A0).
ScalarMap::ScalarMap(int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_values = checkPointer(new float[width * height]);
}

// ELF 0x364900 (D2; D1 0x364930).
ScalarMap::~ScalarMap()
{
    if (m_values)
        delete[] m_values;
}

// ELF 0x364960. The result is indexed with its own width for the row and this map's width for the
// right column, as shipped. The update-time timer is read once.
bool ScalarMap::smooth(ScalarMap& result, double deadline, int& row) const
{
    if (m_height * m_width == 1)
        return true;

    Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicMap/ScalarMap.cpp", 45, "") << "Does not work";

    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const float* const v = m_values;
    if (row >= 0)
    {
        int y = row ? row : 1;
        for (; y < m_height - 1; ++y)
        {
            if (timer->getUpdateTime() >= deadline)
            {
                row = y;
                return false;
            }
            for (int x = 1; x < m_width - 1; ++x)
            {
                float& out = result.m_values[y * result.m_width + x];
                out = v[(y - 1) * m_width + x - 1];
                out += v[y * m_width + x - 1];
                out += v[(y + 1) * m_width + x - 1];
                out += v[(y - 1) * m_width + x];
                out += v[y * m_width + x];
                out += v[(y + 1) * m_width + x];
                out += v[(y - 1) * m_width + x + 1];
                out += v[y * m_width + x + 1];
                out += v[(y + 1) * m_width + x + 1];
                out *= 0.11111111f;
            }
        }
        row = -1;
        if (timer->getUpdateTime() >= deadline)
            return false;
    }

    // Top row.
    for (int x = 1; x < m_width - 1; ++x)
    {
        float& out = result.m_values[x];
        out = v[x - 1];
        out += v[m_width + x - 1];
        out += v[x];
        out += v[m_width + x];
        out += v[x + 1];
        out += v[m_width + x + 1];
        out *= 0.16666667f;
    }
    // Bottom row.
    for (int x = 1; x < m_width - 1; ++x)
    {
        float& out = result.m_values[(m_height - 1) * result.m_width + x];
        out = v[(m_height - 2) * m_width + x - 1];
        out += v[(m_height - 1) * m_width + x - 1];
        out += v[(m_height - 2) * m_width + x];
        out += v[(m_height - 1) * m_width + x];
        out += v[(m_height - 2) * m_width + x + 1];
        out += v[(m_height - 1) * m_width + x + 1];
        out *= 0.16666667f;
    }
    // Left column.
    for (int y = 1; y < m_height - 1; ++y)
    {
        float& out = result.m_values[y * result.m_width];
        out = v[(y - 1) * m_width];
        out += v[y * m_width];
        out += v[(y + 1) * m_width];
        out += v[(y - 1) * m_width + 1];
        out += v[y * m_width + 1];
        out += v[(y + 1) * m_width + 1];
        out *= 0.16666667f;
    }
    // Right column.
    for (int y = 1; y < m_height - 1; ++y)
    {
        float& out = result.m_values[y * result.m_width + m_width - 1];
        out = v[(y - 1) * m_width + m_width - 2];
        out += v[y * m_width + m_width - 2];
        out += v[(y + 1) * m_width + m_width - 2];
        out += v[(y - 1) * m_width + m_width - 1];
        out += v[y * m_width + m_width - 1];
        out += v[(y + 1) * m_width + m_width - 1];
        out *= 0.16666667f;
    }
    // Corners.
    {
        float& out = result.m_values[0];
        out = v[0];
        out += v[m_width];
        out += v[1];
        out += v[m_width + 1];
        out *= 0.25f;
    }
    {
        float& out = result.m_values[m_width - 1];
        out = v[m_width - 2];
        out += v[2 * m_width - 2];
        out += v[m_width - 1];
        out += v[2 * m_width - 1];
        out *= 0.25f;
    }
    {
        float& out = result.m_values[(m_height - 1) * result.m_width];
        out = v[(m_height - 2) * m_width];
        out += v[(m_height - 1) * m_width];
        out += v[(m_height - 2) * m_width + 1];
        out += v[(m_height - 1) * m_width + 1];
        out *= 0.25f;
    }
    {
        float& out = result.m_values[(m_height - 1) * result.m_width + m_width - 1];
        out = v[(m_height - 2) * m_width + m_width - 2];
        out += v[(m_height - 1) * m_width + m_width - 2];
        out += v[(m_height - 2) * m_width + m_width - 1];
        out += v[(m_height - 1) * m_width + m_width - 1];
        out *= 0.25f;
    }
    return true;
}

// ELF 0x365890. The queue starts with count placeholders {-1, -1e20}; a value above the smallest
// kept one replaces it. The queue is then emptied smallest first from the back of indices.
void ScalarMap::sortLargest(int count, int* indices) const
{
    std::priority_queue<PQElem> queue;
    PQElem element(-1, -1e20f);
    float threshold = -1e20f;
    for (int i = 0; i < count; ++i)
        queue.push(element);
    for (int i = 0; i < m_width * m_height; ++i)
    {
        if (m_values[i] > threshold)
        {
            queue.pop();
            element.m_index = i;
            element.m_value = m_values[i];
            queue.push(element);
            threshold = queue.top().m_value;
        }
    }
    for (int i = count - 1; i >= 0; --i)
    {
        indices[i] = queue.top().m_index;
        queue.pop();
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
