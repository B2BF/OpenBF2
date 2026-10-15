// Path/type names [INFERRED]. PE tables 9497F0/949850/9498B0/94ECE0;
// all implemented instructions read.
#include "NamedObjectRegistry.h"
#if !DEDICATED
#include "Object.h"
#include "Stream.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
char* copyName(const char* name) // 737DC0, ordinary allocation; no null guard on allocation
{
    if (!name) return nullptr;
    const unsigned int size = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(size));
    std::memcpy(copy, name, size);
    return copy;
}
const char* separator(const char* name)
{
    if (name) for (; *name; ++name) if (*name == '/' || *name == '\\') return name;
    return nullptr;
}
}
NameTreeNode::~NameTreeNode() { delete next; }
const char* NameTreeNode::getName() { return nullptr; }
bool NameTreeNode::setName(const char*) { return false; }
Object* NameTreeNode::getObject() { return nullptr; }
void NameTreeNode::setPayload(void*) {}
bool NameTreeNode::isContainer() { return false; }
NameTreeNode* NameTreeNode::getChildren() { return nullptr; }
void NameTreeNode::setChildren(NameTreeNode*) {}
bool NameTreeNode::save(System*) { return false; }
bool NameTreeNode::contains(Object*) { return false; }
bool NameTreeNode::insert(System*, Object*, const char*) { return false; }
NameTreeNode* NameTreeNode::createContainer(System*, const char*) { return nullptr; }
bool NameTreeNode::eraseObject(System*, Object*) { return false; }
bool NameTreeNode::eraseNode(System*, NameTreeNode*) { return false; }
bool NameTreeNode::preClose(System*) { return false; }
bool NameTreeNode::close(System*) { return false; }
Object* NameTreeNode::findObject(System*, const char*) { return nullptr; }
NameTreeNode* NameTreeNode::findEntry(System*, Object*) { return nullptr; }
NameTreeNode* NameTreeNode::findChild(const char*) { return nullptr; }
NameTreeNode* NameTreeNode::findParent(Object*) { return nullptr; }
bool NameTreeNode::objectPath(Object*, char*, int) { return true; }
bool NameTreeNode::nodePath(NameTreeNode*, char*, int) { return true; }
bool NameTreeNode::stream(System*, Stream*) { return false; }

NameTreeContainer::NameTreeContainer(const char* name)
    : m_children(nullptr), m_name(copyName(name)) {}
NameTreeContainer::~NameTreeContainer() // name, children, then base-owned next
{ ::operator delete(m_name); m_name = nullptr; delete m_children; }
const char* NameTreeContainer::getName() { return m_name; }
bool NameTreeContainer::setName(const char* name)
{
    if (!name) return false;
    ::operator delete(m_name); m_name = nullptr;
    m_name = copyName(name);
    return true;
}
void NameTreeContainer::setPayload(void* payload) { m_children = static_cast<NameTreeNode*>(payload); }
bool NameTreeContainer::isContainer() { return true; }
NameTreeNode* NameTreeContainer::getChildren() { return m_children; }
void NameTreeContainer::setChildren(NameTreeNode* children) { m_children = children; }
bool NameTreeContainer::save(System* system) // next is visited even after child failure
{
    bool result = true;
    if (m_children) result = m_children->save(system);
    if (next && !next->save(system)) return false;
    return result;
}
bool NameTreeContainer::contains(Object* object)
{
    if (!object) return false;
    if (m_children && m_children->contains(object)) return true;
    return next ? next->contains(object) : false;
}
bool NameTreeContainer::insert(System* system, Object* object, const char* container)
{
    if (!object) return false;
    const char* name = object->getName();
    if (!name) return false;
    if (container && *container) {
        NameTreeNode* node = createContainer(system, container);
        return node ? node->insert(system, object, nullptr) : false;
    }
    NameTreeNode* previous = nullptr;
    NameTreeNode* current = m_children;
    while (current) {
        if (current->getObject() && std::strcmp(current->getName(), name) > 0) break;
        previous = current; current = current->next;
    }
    void* allocation = ::operator new(sizeof(NameTreeObjectEntry));
    NameTreeNode* entry = allocation ? new(allocation) NameTreeObjectEntry(object) : nullptr;
    // The retail null-allocation branch still dereferences entry here.
    if (previous) { entry->next = current; previous->next = entry; }
    else { entry->next = m_children; m_children = entry; }
    return true;
}
NameTreeNode* NameTreeContainer::createContainer(System* system, const char* path)
{
    if (!path || !*path) return nullptr;
    const char* rest = separator(path);
    char name[256];
    if (rest) {
        const unsigned int count = static_cast<unsigned int>(rest - path);
        std::memcpy(name, path, count); name[count] = 0; ++rest;
    } else std::strcpy(name, path);
    NameTreeNode* previous = nullptr;
    NameTreeNode* current = m_children;
    while (current) {
        const int comparison = std::strcmp(current->getName(), name);
        if (!comparison) {
            if (!current->isContainer()) return nullptr;
            return rest ? current->createContainer(system, rest) : current;
        }
        if (comparison > 0 || current->getObject()) break;
        previous = current; current = current->next;
    }
    void* allocation = ::operator new(sizeof(NameTreeContainer));
    NameTreeNode* node = allocation ? new(allocation) NameTreeContainer(name) : nullptr;
    if (previous) { node->next = current; previous->next = node; }
    else { node->next = m_children; m_children = node; }
    return rest ? node->createContainer(system, rest) : node;
}
bool NameTreeContainer::eraseObject(System* system, Object* object)
{
    NameTreeNode* previous = nullptr;
    for (NameTreeNode* node = m_children; node; previous = node, node = node->next) {
        if (node->getObject() == object) {
            if (previous) previous->next = node->next; else m_children = node->next;
            node->next = nullptr; delete node; return true;
        }
        if (node->getChildren() && node->eraseObject(system, object)) return true;
    }
    return false;
}
bool NameTreeContainer::eraseNode(System* system, NameTreeNode* target)
{
    NameTreeNode* previous = nullptr;
    for (NameTreeNode* node = m_children; node; previous = node, node = node->next) {
        if (node == target) {
            if (previous) previous->next = node->next; else m_children = node->next;
            node->next = nullptr; delete node; return true;
        }
        if (node->getChildren() && node->eraseNode(system, target)) return true;
    }
    return false;
}
bool NameTreeContainer::preClose(System* system)
{
    if (m_children && !m_children->preClose(system)) return false;
    if (next && !next->preClose(system)) return false;
    return true;
}
bool NameTreeContainer::close(System* system)
{
    if (m_children && !m_children->close(system)) return false;
    if (next && !next->close(system)) return false;
    return true;
}
Object* NameTreeContainer::findObject(System* system, const char* path)
{
    if (!path) return nullptr;
    if (m_children) {
        if (!m_name) {
            Object* object = m_children->findObject(system, path);
            if (object) return object;
        } else {
            const char* rest = separator(path);
            if (rest) {
                char name[128];
                const unsigned int count = static_cast<unsigned int>(rest - path);
                std::memcpy(name, path, count); name[count] = 0;
                if (!std::strcmp(m_name, name)) {
                    Object* object = m_children->findObject(system, rest + 1);
                    if (object) return object;
                }
            }
        }
    }
    return next ? next->findObject(system, path) : nullptr;
}
NameTreeNode* NameTreeContainer::findEntry(System* system, Object* object)
{
    if (m_children) {
        NameTreeNode* found = m_children->findEntry(system, object);
        if (found) return found;
    }
    return next ? next->findEntry(system, object) : nullptr;
}
NameTreeNode* NameTreeContainer::findChild(const char* name)
{
    for (NameTreeNode* node = m_children; node; node = node->next)
        if (!std::strcmp(node->getName(), name)) return node;
    return nullptr;
}
NameTreeNode* NameTreeContainer::findParent(Object* object)
{
    for (NameTreeNode* node = m_children; node; node = node->next) {
        if (node->isContainer()) {
            NameTreeNode* found = node->findParent(object);
            if (found) return found;
        } else if (node->getObject() == object) return this;
    }
    return nullptr;
}
bool NameTreeContainer::objectPath(Object* object, char* buffer, int capacity)
{
    if (m_children) {
        const unsigned int length = static_cast<unsigned int>(std::strlen(buffer));
        if (m_name) { std::strcpy(buffer + length, m_name); std::strcat(buffer + length, "/"); }
        if (m_children->objectPath(object, buffer, capacity)) return true;
        buffer[length] = 0;
    }
    return next ? next->objectPath(object, buffer, capacity) : false;
}
bool NameTreeContainer::nodePath(NameTreeNode* node, char* buffer, int capacity)
{
    if (node == this) { std::strcat(buffer, m_name); return true; }
    if (m_children) {
        const unsigned int length = static_cast<unsigned int>(std::strlen(buffer));
        if (m_name) { std::strcpy(buffer + length, m_name); std::strcat(buffer + length, "/"); }
        if (m_children->nodePath(node, buffer, capacity)) return true;
        buffer[length] = 0;
    }
    return next ? next->nodePath(node, buffer, capacity) : false;
}
bool NameTreeContainer::stream(System* system, Stream* output)
{
    if (m_children && !m_children->stream(system, output)) return false;
    if (next && !next->stream(system, output)) return false;
    return true;
}
bool NameTreeRoot::close(System* system) // 83AC90, two passes before destroying anything
{
    if (m_children && !m_children->preClose(system)) return false;
    if (next && !next->preClose(system)) return false;
    if (m_children && !m_children->close(system)) return false;
    if (next && !next->close(system)) return false;
    return true;
}
bool NameTreeRoot::objectPath(Object* object, char* buffer, int capacity)
{
    if (!buffer || !capacity) return false;
    buffer[0] = 0;
    if (!object) return false;
    return NameTreeContainer::objectPath(object, buffer, capacity);
}
bool NameTreeRoot::nodePath(NameTreeNode* node, char* buffer, int capacity)
{
    if (!buffer) return false;
    buffer[0] = 0; // unlike objectPath, this happens even when capacity is zero
    if (!capacity || !node) return false;
    return NameTreeContainer::nodePath(node, buffer, capacity);
}
const char* NameTreeObjectEntry::getName() { return m_object->getName(); }
bool NameTreeObjectEntry::setName(const char* name) // 83A220, ignores object return
{ if (!name) return false; m_object->setName(name); return true; }
Object* NameTreeObjectEntry::getObject() { return m_object; }
void NameTreeObjectEntry::setPayload(void* object) { m_object = static_cast<Object*>(object); }
bool NameTreeObjectEntry::contains(Object* object) // 83A240 includes null equality
{ return m_object == object || (next ? next->contains(object) : false); }
bool NameTreeObjectEntry::eraseObject(System* system, Object* object)
{ return next ? next->eraseObject(system, object) : false; }
bool NameTreeObjectEntry::eraseNode(System* system, NameTreeNode* node)
{ return next ? next->eraseNode(system, node) : false; }
bool NameTreeObjectEntry::close(System* system)
{
    if (m_object && !destroyRuntimeObject(reinterpret_cast<RuntimeObject*&>(m_object))) return false;
    if (next && !next->close(system)) return false;
    return true;
}
Object* NameTreeObjectEntry::findObject(System* system, const char* path)
{
    if (!path || separator(path)) return nullptr;
    const int comparison = std::strcmp(m_object->getName(), path);
    if (!comparison) return m_object;
    if (comparison > 0) return nullptr;
    return next ? next->findObject(system, path) : nullptr;
}
NameTreeNode* NameTreeObjectEntry::findEntry(System* system, Object* object)
{ return m_object == object ? this : (next ? next->findEntry(system, object) : nullptr); }
bool NameTreeObjectEntry::objectPath(Object* object, char* buffer, int capacity)
{
    if (m_object == object) { std::strcat(buffer, object->getName()); return true; }
    return next ? next->objectPath(object, buffer, capacity) : false;
}
bool NameTreeObjectEntry::nodePath(NameTreeNode* node, char* buffer, int capacity)
{
    if (this == node) { std::strcat(buffer, m_object->getName()); return true; }
    return next ? next->nodePath(node, buffer, capacity) : false;
}
bool NameTreeObjectEntry::save(System* system) // 83A270, next visited after failure
{
    bool result = true;
    if (m_object && !saveNamedObject(system, m_object)) result = false;
    if (next && !next->save(system)) return false;
    return result;
}
bool NameTreeObjectEntry::preClose(System* system)
{
    if (m_object) {
        DeletionStream deletion(true);
        if (!deletion.streamObject(system, m_object, "Object", nullptr)) return false;
    }
    if (next && !next->preClose(system)) return false;
    return true;
}
bool NameTreeObjectEntry::stream(System* system, Stream* output)
{
    if (m_object && !output->streamObject(system, m_object, "Object", nullptr)) return false;
    return next ? next->stream(system, output) : true;
}
// [FRONTIER] saveNamedObject82AB60 and specialized visitor descriptor defaults
// are still deeper dependencies. No registry/System factory is published.
#if defined(_M_IX86)
static_assert(sizeof(NameTreeNode) == 8, "name-tree node layout");
static_assert(sizeof(NameTreeContainer) == 16, "name-tree container layout");
static_assert(sizeof(NameTreeRoot) == 16, "name-tree root layout");
static_assert(sizeof(NameTreeObjectEntry) == 12, "name-tree object entry layout");
#endif
} }
#endif
