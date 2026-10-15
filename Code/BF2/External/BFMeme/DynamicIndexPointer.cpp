// Path [INFERRED]. Complete client table94E6C0 and all new bodies read in full;
// this UI family has no dedicated ELF counterpart. No synthetic success hook.
#include "DynamicIndexPointer.h"
#if !DEDICATED
#include "System.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
DynamicIndexPointer::DynamicIndexPointer()                // factory837940 inline ctor
    : m_locked(false), m_assignedCount(0), m_lastInputIndex(0)
    , m_field030(0), m_field034(0), m_timer(0.0f)
{
    m_indexOrder[0] = m_indexOrder[1] = m_indexOrder[2] = m_indexOrder[3] = 100;
}
DynamicIndexPointer::~DynamicIndexPointer() {}            // 82B8D0 ->Pointer; no ownership
TypeDescriptor* DynamicIndexPointer::getClassType()
{
    static ClassTypeDescriptor<DynamicIndexPointer, Pointer> type("dice::meme::DynamicIndexPointer");
    return &type;
}
TypeDescriptor* DynamicIndexPointer::getType() { return getClassType(); }
unsigned int DynamicIndexPointer::getObjectSize() { return sizeof(DynamicIndexPointer); }
bool DynamicIndexPointer::visit0x3c(System* system, Stream* stream)
{
    if (!Pointer::visit0x3c(system, stream)) return false;
    for (int i = 0; i < 4; ++i)
        if (!stream->streamInt(&m_indexOrder[i], "Index order")) return false;
    return stream->streamBool(&m_locked, "Is locked");
}
bool DynamicIndexPointer::update(System* system, float delta)
{
    if (m_timer > 0.0f) m_timer -= delta;
    return Pointer::update(system, delta); // direct base call even while disabled
}
bool DynamicIndexPointer::event1(System* system, int first, int index, int second)
{
    m_lastInputIndex = index ? index - 1 : 0;
    if (m_locked && index) {
        // Retail accepts any nonzero index here, with no range check. Retain
        // both the unchecked array access and the signed comparison.
        if (m_indexOrder[index - 1] > m_assignedCount + 1) {
            m_indexOrder[index - 1] = m_assignedCount + 1;
            ++m_assignedCount;
        }
        system->enqueueEvent(1, m_indexOrder[index - 1], static_cast<float>(first), static_cast<float>(second), 0);
    } else {
        m_indexOrder[0] = index == 1 ? 1 : 100;
        m_indexOrder[1] = index == 2 ? 1 : 100;
        m_indexOrder[2] = index == 3 ? 1 : 100;
        m_indexOrder[3] = index == 4 ? 1 : 100;
        system->enqueueEvent(1, 1, static_cast<float>(first), static_cast<float>(second), 0);
    }
    return true; // actual body always enqueues; there is no Pointer enabled guard
}
bool DynamicIndexPointer::event2(System* system, int first, int index)
{
    // Deliberately stronger index test than event1, still no upper bound.
    const int mapped = m_locked && index > 0 ? m_indexOrder[index - 1] : 1;
    system->enqueueEvent(2, mapped, static_cast<float>(first), 0.0f, 0);
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(DynamicIndexPointer) == 60, "client DynamicIndexPointer native layout");
#endif
} }
#endif
