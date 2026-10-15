// [INFERRED] path and concrete class name. Complete table94EBA0 and every
// differing operation plus recursive link destructor839750 read. This is the
// real typed collection embedded by ActionListAction; no STL overlay is used.
#include "ObjectList.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
struct ObjectList::Link
{
    Link* next;
    Link* previous;
    Object* object;
    explicit Link(Object* value) : next(nullptr), previous(nullptr), object(value) {}
    ~Link() { delete next; }                             // 839750, reverse free order, no payload deletion
    static Link* allocate(Object* object)
    {
#if defined(_M_IX86)
        static_assert(sizeof(Link) == 12, "client ObjectList link");
#endif
        void* memory = ::operator new(sizeof(Link));
        return memory ? new(memory) Link(object) : nullptr;
    }
};
ObjectList::ObjectList(TypeDescriptor* type) : m_first(nullptr), m_last(nullptr),
    m_cursor(nullptr), m_type(type) {}
ObjectList::~ObjectList() { delete m_first; }             // 8397B0, payloads remain borrowed here
bool ObjectList::clear(System* system)
{
    for (Link* link = m_first; link; link = link->next)
        if (link->object && !deleteObject(system, link->object)) return false;
    delete m_first;
    m_first = nullptr; // Retail does not clear m_last or m_cursor.
    return true;
}
ObjectCollectionPosition* ObjectList::first()
{ return reinterpret_cast<ObjectCollectionPosition*>(m_first); }
ObjectCollectionPosition* ObjectList::last()
{ return reinterpret_cast<ObjectCollectionPosition*>(m_last); }
ObjectCollectionPosition* ObjectList::next(ObjectCollectionPosition* position)
{
    Link* link = reinterpret_cast<Link*>(position);
    return link ? reinterpret_cast<ObjectCollectionPosition*>(link->next) : nullptr;
}
ObjectCollectionPosition* ObjectList::previous(ObjectCollectionPosition* position)
{
    Link* link = reinterpret_cast<Link*>(position);
    return link ? reinterpret_cast<ObjectCollectionPosition*>(link->previous) : nullptr;
}
int ObjectList::size()
{
    int count = 0;
    for (Link* link = m_first; link; link = link->next) ++count;
    return count;
}
Object* ObjectList::getObject(ObjectCollectionPosition* position)
{ return position ? reinterpret_cast<Link*>(position)->object : nullptr; }
bool ObjectList::setObject(ObjectCollectionPosition* position, Object* object)
{
    if (!position) return false;
    reinterpret_cast<Link*>(position)->object = object;
    return true;
}
TypeDescriptor* ObjectList::getObjectType() { return m_type; }
bool ObjectList::append(System*, Object* object)
{
    if (!object) return false;
    TypeDescriptor* type = getObjectType();
    if (!object->getType()->isDerivedFrom(type)) return false;
    if (!m_first)
    {
        Link* link = Link::allocate(object);
        m_first = link;
        m_cursor = link;
        m_last = link;
        return true; // Retail even returns true for null allocation on an empty list.
    }
    Link* last = m_first; // Deliberately walks from head, not cached m_last.
    while (last->next) last = last->next;
    Link* link = Link::allocate(object);
    last->next = link;
    link->previous = last; // Retail missing allocation-null guard.
    m_last = last->next;
    return true;
}
bool ObjectList::insert(System* system, ObjectCollectionPosition* position, Object* object)
{
    if (!object) return false;
    TypeDescriptor* type = getObjectType();
    if (!object->getType()->isDerivedFrom(type)) return false;
    if (!position) return append(system, object); // Virtual, repeats type checks as shipped.
    Link* at = reinterpret_cast<Link*>(position);
    Link* link = Link::allocate(object);
    link->next = at; // No allocation-null guard.
    link->previous = at->previous;
    if (at->previous)
    {
        if (m_first == at) m_first = link;
        at->previous->next = link;
        at->previous = link;
    }
    else
    {
        m_first = link;
        at->previous = link;
    }
    return true;
}
void ObjectList::remove(System*, ObjectCollectionPosition* position)
{
    Link* link = reinterpret_cast<Link*>(position);
    if (!link) return;
    if (link == m_first) m_first = link->next;
    if (link == m_last) m_last = link->previous;
    if (link == m_cursor) m_cursor = m_first;
    if (link->previous) link->previous->next = link->next;
    if (link->next) link->next->previous = link->previous;
    link->next = nullptr;
    delete link; // Only this detached link, never its payload.
}
bool ObjectList::readObjects(System* system, IStream* stream)
{
    if (!ObjectCollection::readObjects(system, stream)) return false;
    m_cursor = m_first;
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(ObjectList) == 20, "client concrete ObjectList");
#endif
} }
#endif
