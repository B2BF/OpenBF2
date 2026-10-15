// Client PE instructions retained in system_lifecycle_evidence_20260905.
// No dedicated ELF counterpart; field/method spellings are [INFERRED].
#include "Pointer.h"
#if !DEDICATED
#include "Node.h"
#include "Style.h"
#include "System.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
Pointer::Pointer() : m_width(20.0f), m_height(20.0f), m_graphic(nullptr), m_enabled(false) {}
Pointer::~Pointer() {}
TypeDescriptor* Pointer::getClassType()
{
    static ClassTypeDescriptor<Pointer, Object> type("dice::meme::Pointer");
    return &type;
}
TypeDescriptor* Pointer::getType() { return getClassType(); }
unsigned int Pointer::getObjectSize() { return sizeof(Pointer); }
bool Pointer::paint(System* system)
{
    if (m_graphic && m_enabled) {
        const Rect bounds = {system->m_pointerX - m_width * 0.5f,
            system->m_pointerY - m_height * 0.5f, m_width, m_height, 0};
        const NodeState state = {1.0f, 1.0f};
        RenderContext context = {nullptr, nullptr, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
        m_graphic->paint(system, &state, &bounds, nullptr, &context);
    }
    return true;
}
bool Pointer::update(System* system, float delta)
{
    // Retail copies the coordinates even when this direct call is disabled.
    // The separate guarded wrapper skips the entire call when disabled.
    system->m_previousPointerX = system->m_pointerX;
    system->m_previousPointerY = system->m_pointerY;
    if (m_graphic && m_enabled) {
        const Rect bounds = {system->m_pointerX - m_width * 0.5f,
            system->m_pointerY - m_height * 0.5f, m_width, m_height, 0};
        const NodeState state = {1.0f, 1.0f};
        return m_graphic->update(system, &state, &bounds, delta);
    }
    return true;
}
bool Pointer::setPosition(System* system, float x, float y)
{
    if (m_enabled) {
        system->m_pointerY = y;
        system->m_pointerX = x;
        system->pointerPositionChanged(&x, &y); // Return and changed locals ignored.
    }
    return true;
}
bool Pointer::event1(System* system, int first, int index, int second)
{
    if (m_enabled) system->enqueueEvent(1, index, static_cast<float>(first), static_cast<float>(second), 0);
    return true;
}
bool Pointer::event2(System* system, int first, int index)
{
    if (m_enabled) system->enqueueEvent(2, index, static_cast<float>(first), 0.0f, 0);
    return true;
}
bool Pointer::event4(System* system, int first, int index, int second)
{
    if (m_enabled) system->enqueueEvent(4, index, static_cast<float>(first), static_cast<float>(second), 0);
    return true;
}
bool Pointer::event5(System* system, int first, int index)
{
    if (m_enabled) system->enqueueEvent(5, index, static_cast<float>(first), 0.0f, 0);
    return true;
}
void Pointer::setGraphic(System* system, Node* node)
{
    deleteObject(system, reinterpret_cast<Object*&>(m_graphic));
    m_graphic = node;
}
bool paintPointer(System* s, Pointer* p) { return p && p->isEnabled() ? p->paint(s) : true; }
bool updatePointer(System* s, Pointer* p, float d) { return p && p->isEnabled() ? p->update(s, d) : true; }
bool positionPointer(System* s, Pointer* p, float x, float y) { return p && p->isEnabled() ? p->setPosition(s, x, y) : true; }
bool pointerEvent1(System* s, Pointer* p, int a, int i, int b) { return p && p->isEnabled() ? p->event1(s, a, i, b) : true; }
bool pointerEvent2(System* s, Pointer* p, int a, int i) { return p && p->isEnabled() ? p->event2(s, a, i) : true; }
#if defined(_M_IX86)
static_assert(sizeof(Pointer) == 20, "Pointer has no STL-dependent members");
#endif
} }
#endif
