// Timed debug mesh. Evidence: libaidll.so (ia-32) 0x37D850..0x37E61F.
// [INFERRED] Source placement.

#include "DPStaticMesh.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37E570, __tcf_0 0x37E5D0, _GLOBAL__I 0x37E600).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIRandom.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37D850 (C2; C1 0x37DB20). The random base is drawn before the loop whether or not
// `colours` is given; the low-bit term is drawn per entry only on the null-`colours` path.
DPStaticMesh::DPStaticMesh(float lifeTime, unsigned int* colours, Vec3* vertices,
                           unsigned int* indices, int count, bool filled)
    : DrawPrimitive(lifeTime, colours ? colours[0] : 0xFFFFFFFFu)
    , m_vertices(vertices)
    , m_indices(indices)
    , m_count(count)
    , m_filled(filled)
{
    m_colours = checkPointer(new unsigned int[m_count]);
    m_ownsArrays = false;
    const unsigned int base = AIRandom::m_instance->next() & 0xE0E0E0E0u;
    for (int i = 0; i < m_count; ++i)
    {
        if (colours)
            m_colours[i] = colours[i];
        else
            m_colours[i] = (AIRandom::m_instance->next() & 0x1F1F1F1Fu) + base;
    }
}

// ELF 0x37DDF0 (C2; C1 0x37E0C0). Identical to the overload above apart from the ignored
// polygon pointer.
DPStaticMesh::DPStaticMesh(float lifeTime, unsigned int* colours, Polygon* /*polygons*/,
                           Vec3* vertices, unsigned int* indices, int count, bool filled)
    : DrawPrimitive(lifeTime, colours ? colours[0] : 0xFFFFFFFFu)
    , m_vertices(vertices)
    , m_indices(indices)
    , m_count(count)
    , m_filled(filled)
{
    m_colours = checkPointer(new unsigned int[m_count]);
    m_ownsArrays = false;
    const unsigned int base = AIRandom::m_instance->next() & 0xE0E0E0E0u;
    for (int i = 0; i < m_count; ++i)
    {
        if (colours)
            m_colours[i] = colours[i];
        else
            m_colours[i] = (AIRandom::m_instance->next() & 0x1F1F1F1Fu) + base;
    }
}

// ELF 0x37E390 (D2; D1 0x37E410, D0 0x37E490).
DPStaticMesh::~DPStaticMesh()
{
    delete[] m_colours;
    if (m_ownsArrays)
    {
        delete[] m_vertices;
        delete[] m_indices;
    }
}

// ELF 0x37E520. IDebugRenderer slot 9 (drawMesh).
bool DPStaticMesh::draw(IDebugRenderer* renderer)
{
    renderer->drawMesh(m_vertices, m_indices, m_count, m_colours, m_filled);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
