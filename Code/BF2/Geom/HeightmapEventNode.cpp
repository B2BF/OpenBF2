// Path:     [INFERRED] - see Geom/HeightmapEventNode.h.
// Contents: the whole translation unit, (ELF 0x84A8900..0x84A8A5F, PE
//           bf2_w32ded 0x756520 / 0x756550). Engine code: the dedicated server links it too.

// [CHOSEN, not recovered] The native static renderer uses the host's identical
// implementation and RTTI for this shared event type. Windows DLLs keep their own copy.
#if !(defined(BF2_STATIC_PLUGINS) && defined(BF2_IMAGE_RENDDX9))
#include "HeightmapEventNode.h"

#include <iostream>     // the TU's static initialiser (ELF 0x84A89B0) is the <iostream> guard

namespace dice {
namespace hfe {
namespace geom {

// ELF 0x84A8930 (C1) / 0x84A8900 (C2), 47 B; PE 0x756520. EventNode's m_next = 0, the vptr, the
// type, then the four area floats zeroed. RendDX9's own copy: PE 0x101C8460 (vtable 0x10223094).
HeightmapEventNode::HeightmapEventNode(unsigned int type)
    : SimpleEventNode(type)
{
    m_affectedArea.x = 0.0f;
    m_affectedArea.y = 0.0f;
    m_affectedArea.z = 0.0f;
    m_affectedArea.w = 0.0f;
}

// ELF 0x84A8A40 (D1) / 0x84A8A50 (D0): nothing of its own - the vptr goes back to EventNode's.
// RendDX9's scalar deleting form, PE 0x101C84C0, is slot 0 of both the EventNode table
// (0x10223088) and this class's (0x10223094).
HeightmapEventNode::~HeightmapEventNode()
{
}

// ELF 0x84A8960, 64 B; PE 0x756550. A straight four-float copy. RendDX9's copy: PE 0x101C8490
// (posted by the road tessellator, 0x1014067D).
void HeightmapEventNode::setAffectedArea(const Vec4& area)
{
    m_affectedArea = area;
}


const Vec4& HeightmapEventNode::getAffectedArea() const
{
    return m_affectedArea;
}

} // namespace geom
} // namespace hfe
} // namespace dice
#endif
