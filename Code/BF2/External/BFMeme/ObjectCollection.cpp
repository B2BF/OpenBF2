// Path/name [INFERRED]. Full instructions83C470..83C8B2 and concrete list
// table94EBA0 read; no concrete collection factory or guessed list storage.
#include "ObjectCollection.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
ObjectCollection::ObjectCollection() {} // 83C470, only vptr
ObjectCollection::~ObjectCollection() {} // 83C480, no clear/ownership work
bool ObjectCollection::clear(System* system)
{
    ObjectCollectionPosition* position = first();
    while (position) {
        Object* object = getObject(position);
        remove(system, position); // Removal precedes the null check/deletion.
        if (!object) break;
        if (!deleteObject(system, object)) return false;
        position = first(); // Restart after callbacks, not next(oldPosition).
    }
    return true;
}
ObjectCollectionPosition* ObjectCollection::first() { return nullptr; }
ObjectCollectionPosition* ObjectCollection::last()
{
    ObjectCollectionPosition* position = first();
    ObjectCollectionPosition* successor = next(position); // Even when first is null.
    while (successor) {
        position = successor;
        successor = next(position);
    }
    return position;
}
ObjectCollectionPosition* ObjectCollection::next(ObjectCollectionPosition*) { return nullptr; }
ObjectCollectionPosition* ObjectCollection::previous(ObjectCollectionPosition* requested)
{
    ObjectCollectionPosition* predecessor = nullptr;
    ObjectCollectionPosition* position = first();
    if (position == requested) return predecessor;
    do {
        predecessor = position;
        position = next(position);
        if (!position) return nullptr;
    } while (position != requested);
    return predecessor;
}
int ObjectCollection::size() // null payload, not null position, terminates.
{
    int count = 0;
    ObjectCollectionPosition* position = first();
    while (getObject(position)) {
        ++count;
        position = next(position);
    }
    return count;
}
int ObjectCollection::countBefore(ObjectCollectionPosition* requested)
{
    if (!requested) return 0;
    int count = 0;
    ObjectCollectionPosition* position = first();
    while (position != requested) { ++count; position = next(position); }
    return count; // No missing-position termination in retail.
}
int ObjectCollection::countAfter(ObjectCollectionPosition* position)
{
    if (!position) return 0;
    int count = 0;
    position = next(position);
    while (getObject(position)) { ++count; position = next(position); }
    return count;
}
Object* ObjectCollection::getObject(ObjectCollectionPosition*) { return nullptr; }
bool ObjectCollection::setObject(ObjectCollectionPosition*, Object*) { return false; }
Object* ObjectCollection::getObjectAt(int index) { return getObject(positionAt(index)); }
int ObjectCollection::indexOf(ObjectCollectionPosition* requested)
{
    if (!requested) return -1;
    int index = 0;
    ObjectCollectionPosition* position = first();
    while (position) {
        if (position == requested) return index;
        ++index;
        position = next(position);
    }
    return -1;
}
ObjectCollectionPosition* ObjectCollection::positionAt(int index)
{
    if (index < 0) return nullptr;
    ObjectCollectionPosition* position = first();
    while (index) {
        position = next(position);
        if (!position) return nullptr;
        --index;
    }
    return position;
}
ObjectCollectionPosition* ObjectCollection::find(Object* requested)
{
    ObjectCollectionPosition* position = first();
    while (position) {
        if (getObject(position) == requested) return position;
        position = next(position);
    }
    return nullptr;
}
TypeDescriptor* ObjectCollection::getObjectType() { return Object::getClassType(); }
bool ObjectCollection::append(System*, Object*) { return false; }
bool ObjectCollection::insert(System*, ObjectCollectionPosition*, Object*) { return false; }
void ObjectCollection::remove(System*, ObjectCollectionPosition*) {} // Retail empty default.
const char* ObjectCollection::getObjectName(System*, ObjectCollectionPosition* position)
{
    Object* object = getObject(position);
    if (!object) return "<empty>";
    if (object->getName()) return object->getName(); // Repeated virtual read.
    return object->getType()->getShortName();
}
bool ObjectCollection::read(System* system, IStream* input) { return readObjects(system, input); }
bool ObjectCollection::write(System* system, OStream* output) { return writeObjects(system, output); }
bool ObjectCollection::readObjects(System* system, IStream* input)
{
    Object* object = nullptr;
    if (!readObject(system, object, input)) return false;
    while (object) {
        append(system, object); // Ignored failure, no deletion/rollback.
        object = nullptr;
        if (!readObject(system, object, input)) return false;
    }
    return true;
}
bool ObjectCollection::writeObjects(System* system, OStream* output)
{
    ObjectCollectionPosition* position = first();
    Object* object = getObject(position);
    while (object) {
        if (!writeObject(system, object, output)) return false;
        position = next(position);
        object = getObject(position);
    }
    object = nullptr;
    return writeObject(system, object, output); // Always emit the null terminator.
}
#if defined(_M_IX86)
static_assert(sizeof(ObjectCollection) == 4, "BFMeme collection base layout");
#endif
} }
#endif
