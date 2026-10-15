// Menu system lifetime, object registry, and event dispatch.
#include "System.h"
#if !DEDICATED
#include "Data.h"
#include "NamedObjectRegistry.h"
#include "TypeDescriptor.h"
#include "IStream.h"
#include "EventQueue.h"
#include "Node.h"
#include "NodeState.h"
#include "Pointer.h"
#include "Style.h"
#include "NamedInputStream.h"
#include "InputNodeVisitors.h"
#include "../../Core/Debug.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

namespace dice { namespace meme {

namespace {
// System descriptor creates instances with zero float parameters. Instance-counter operations are unguarded.
class SystemTypeDescriptor final : public TypeDescriptor
{
public:
    SystemTypeDescriptor() : TypeDescriptor("dice::meme::System") {}
    TypeDescriptor* getBaseType() override { return Object::getClassType(); }
    RuntimeObject* create() override
    {
        void* allocation = ::operator new(sizeof(System));
        return allocation ? new(allocation) System(0.0f, 0.0f) : nullptr;
    }
    const char* getDescription() override { return nullptr; }
    void addInstance() override { ++instanceCount; }
    void removeInstance() override { --instanceCount; }
    int getInstanceCount() override { return instanceCount; }
    unsigned int getInstanceSize() override { return sizeof(System); } // 7CDCD0, retail 0x58
private:
    static int instanceCount;
};
int SystemTypeDescriptor::instanceCount = 0;
#if defined(_M_IX86)
static_assert(sizeof(SystemTypeDescriptor) == 16, "System metadata stores its counter separately");
#endif

// A3970C is a plain dword whose bit zero is set before construction. Retail has no lock and no
// exception rollback here; avoid the compiler's C++11 thread-safe function-static guard.
alignas(SystemTypeDescriptor) unsigned char systemTypeStorage[sizeof(SystemTypeDescriptor)] = {};
unsigned int systemTypeGuard = 0;

SystemTypeDescriptor* systemTypeDescriptor()
{
    return reinterpret_cast<SystemTypeDescriptor*>(systemTypeStorage);
}

void destroySystemTypeDescriptor()
{
    systemTypeDescriptor()->~SystemTypeDescriptor();
}
}

TypeDescriptor* System::getClassType() // 82A9B0, same body used by the virtual getter
{
    if ((systemTypeGuard & 1u) == 0)
    {
        systemTypeGuard |= 1u;
        new(systemTypeStorage) SystemTypeDescriptor;
        std::atexit(&destroySystemTypeDescriptor);
    }
    return systemTypeDescriptor();
}
TypeDescriptor* System::getType() { return getClassType(); }

System::System(float width, float height)
    : m_originX(width * -0.5f), m_originY(height * -0.5f)
    , m_width(width), m_height(height), m_field014(0), m_field018(nullptr)
    , m_field01c(0), m_field020(nullptr), m_field024(nullptr)
    // 0x838E70(2, 2, true): second component=2, first=-(2+1).
    , m_field028(-(2.0f + 1.0f)), m_field02c(2.0f), m_lockCount(0)
    , m_registry(nullptr), m_pointerX(0), m_pointerY(0), m_previousPointerX(0)
    , m_previousPointerY(0), m_eventQueue(nullptr), m_field04c(0), m_field050(0), m_field054(0)
{
}

// 0x829EB0 delegates directly to Object's empty destructor. It does not close
// the UI system or free its runtime collaborators; close is a separate virtual.
System::~System() {}
bool System::visit0x3c(System* system, Stream* stream)
{
    return Object::visit0x3c(system, stream)
        && stream->streamInt(&m_lockCount, "Lock count")
        && stream->streamObject(this, m_field020, "Pointer", Pointer::getClassType())
        && stream->streamNode(this, m_field018, "Start node", nullptr);
}
bool System::init() // 82A5C0, real named Style/queue/Pointer lifecycle
{
    void* storage = ::operator new(sizeof(NameTreeRoot));
    m_registry = storage ? new(storage) NameTreeRoot : nullptr;
    storage = ::operator new(sizeof(Style));
    Style* style = storage ? new(storage) Style("System", 0) : nullptr;
    m_field024 = registerObject(style, nullptr);
    storage = ::operator new(sizeof(EventQueue));
    m_eventQueue = storage ? new(storage) EventQueue(10) : nullptr;
    createObject(this, m_field020, Pointer::getClassType());
    return true; // Shipped code ignores register/create failure here.
}
bool System::close()
{
    deleteObject(this, m_field020);
    deleteObject(this, reinterpret_cast<Object*&>(m_field018));
    if (m_registry) {
        m_registry->close(this);
        delete m_registry; // Reload after callback, including its null check.
        m_registry = nullptr;
    }
    delete m_eventQueue;
    m_eventQueue = nullptr;
    m_field024 = nullptr;
    return true;
}
bool System::setPointerPosition(float x, float y)
{ return positionPointer(this, static_cast<Pointer*>(m_field020), x, y); }
bool System::update(float delta)
{
    if (!preUpdate(delta)) return false;
    Rect* bounds = getBounds();
    NodeState* state = getNodeState();
    updateNodeState(state, this, nullptr, nullptr, bounds, delta, true, true);
    if (m_field018 && !m_field018->update(this, state, bounds, delta)) return false;
    if (!m_eventQueue->process(this)) return false;
    if (!updatePointer(this, static_cast<Pointer*>(m_field020), delta)) return false;
    return postUpdate(delta);
}
void System::setSize(float width, float height)
{
    const float oldSize[2] = {m_width, m_height};
    m_originX = width * -0.5f;
    m_originY = height * -0.5f;
    m_width = width;
    m_height = height;
    sizeChanged(oldSize, &m_width);
}
bool System::paint()
{
    beginPaint();
    if (m_field018) {
        RenderContext context = {nullptr, nullptr, 0, 1, 0, 1, 1};
        // These native field groups are the live NodeState at+28 and five-word
        // Rect at+04. Preserve their addresses across the callback, including
        // the zero-initialized raw fifth Rect word at+14.
        m_field018->paint(this, reinterpret_cast<const NodeState*>(&m_field028),
            reinterpret_cast<const Rect*>(&m_originX), nullptr, &context);
    }
    if (!paintPointer(this, static_cast<Pointer*>(m_field020)))
        return false; // Retail skips endPaint after a failed pointer callback.
    endPaint();
    return true;
}
unsigned int System::getObjectSize() { return sizeof(System); }
OStream* System::getOutputStream() { return nullptr; }
IStream* System::createInputStream(const char*) { return nullptr; }
OStream* System::createOutputStream(const char*) { return nullptr; }
void System::deleteOutputStream(OStream* stream) { delete stream; }
void System::deleteInputStream(IStream* stream) { delete stream; }
bool System::pointerPositionChanged(float*, float*) { return true; }
bool System::drawCharacter(int, FontHandle, Quad*) { return false; }
bool System::getGlyphMetrics(int, FontHandle, GlyphMetrics*) { return false; }
float System::getKerning(int, int, FontHandle) { return 0.0f; }
float System::getFontHeight(FontHandle) { return 0.0f; }
FontHandle System::loadFont(const char*) { return 0; }
const char* System::getFontName(FontHandle) { return nullptr; }
bool System::drawTexture(TextureHandle, Quad*) { return false; }
TextureHandle System::loadTexture(const char*, bool) { return 0; }
void System::releaseTexture(TextureHandle) {} // 5AB2A0, real empty default
const char* System::getTextureName(TextureHandle) { return nullptr; }
float System::getTextureWidth(TextureHandle) { return 0.0f; }
float System::getTextureHeight(TextureHandle) { return 0.0f; }
void System::setDefaultTexture(TextureHandle texture) { m_field054 = texture; }
bool System::playSound(int, float) { return false; } // 455420, real default
int System::loadSound(const char*) { return 0; }
const char* System::getSoundName(int) { return nullptr; }
void System::beginPaint() {} // 83D270, real empty default
void System::endPaint() {} // 83D270, real empty default
bool System::preUpdate(float) { return true; }
bool System::postUpdate(float) { return true; }
void System::sizeChanged(const float*, const float*) {}
void System::setPointerOwner(Node* owner) { m_field01c = owner; }
Node* System::getPointerOwner() { return m_field01c; }

Object* System::registerObject(Object* object, const char* container)
{
    const char* const file = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\External\\BFMeme\\System.cpp";
    if (!object) {
        hfe::Debug(hfe::DebugTypeError, file, 79, "")
            << "Meme: Error: Cannot register null pointers\r\n";
        return nullptr;
    }
    if (!object->getName()) {
        hfe::Debug(hfe::DebugTypeError, file, 83, "")
            << "Meme: Error: Cannot register objects without name\r\n";
        return nullptr;
    }
    if (!object->onCreate(this)) {
        hfe::Debug(hfe::DebugTypeError, file, 87, "")
            << "Meme: Error: register onCreate returned false (" << object->getName()
            << " : " << container << ")";
        return nullptr;
    }
    if (!m_registry->insert(this, object, container)) {
        hfe::Debug(hfe::DebugTypeError, file, 91, "")
            << "Meme: Error: Adding named object to nodetree (" << object->getName()
            << " : " << container << ")";
        return nullptr;
    }
    object->getType()->addInstance();
    return object;
}
Data* System::registerData(Data* data, const char* container)
{ return static_cast<Data*>(registerObject(data, container)); }
IStream* System::openNamedInputStream(const char* path)
{
    if (m_field050) {
        void* allocation = ::operator new(sizeof(TeeIStream));
        if (!allocation) return nullptr;
        IStream* input = createInputStream(path);
        OStream* output = m_field050; // Reload after the virtual call.
        return new(allocation) TeeIStream(input, output);
    }
    if (m_field04c) {
        void* allocation = ::operator new(sizeof(RelativeIStream));
        if (!allocation) return nullptr;
        return new(allocation) RelativeIStream(m_field04c);
    }
    return createInputStream(path);
}
Object* System::findNamedObject(const char* path)
{ return m_registry->findObject(this, path); }
namespace {
// Table94990C: the three NodeVisitor slots, no virtual destructor. Unlike the
// state visitor, this visitor continues through every node after dispatch.
class EventNodeVisitor : public NodeVisitor
{
public:
    explicit EventNodeVisitor(const NodeInputEvent& event)
        : NodeVisitor(0), m_event(event) {}
    void visit(System* system, Node*& node, const Rect* rect, NodeState* state) override
    {
        if (node) {
            dispatchNodeEvent(system, node, rect, state, &m_event);
            // Reload after callback; retail has no second null guard.
            node->visitChildren(system, state, rect, this);
        }
    }
private:
    NodeInputEvent m_event;
};
#if defined(_M_IX86)
static_assert(sizeof(EventNodeVisitor) == 28, "client generic event visitor");
#endif

Node* castMenuNode(Object* object)
{
    if (!object) return nullptr;
    TypeDescriptor* expected = Node::getClassType();
    TypeDescriptor* type = object->getType();
    while (type) {
        if (type == expected) return static_cast<Node*>(object);
        type = type->getBaseType();
    }
    return nullptr;
}
}
bool System::loadMenu(const char* mainPath, const char* cachePath)
{
    if (cachePath) {
        m_field04c = createInputStream(cachePath);
        if (m_field04c) {
            systemPrintf(this, "Meme: Using cache file '%s' for loading\r\n", cachePath);
        } else {
            m_field050 = createOutputStream(cachePath);
            if (m_field050)
                systemPrintf(this, "Meme: Creating cache file '%s'\r\n", cachePath);
            else
                systemPrintf(this, "Meme: Cannot create cache file '%s', using standard reading\r\n", cachePath);
        }
    }
    Object* object = nullptr;
    bool result = loadNamedObject(this, object, mainPath);
    if (!m_field018) {
        m_field018 = castMenuNode(object);
        if (m_field018) {
            const NodeInputEvent first = {17, 0, 0.0f, 0.0f, 0};
            EventNodeVisitor firstVisitor(first);
            firstVisitor.traverse(this, m_field018, getBounds(), getNodeState());
            const NodeInputEvent second = {13, 0, 0.0f, 0.0f, 0};
            EventNodeVisitor secondVisitor(second);
            secondVisitor.traverse(this, m_field018, getBounds(), getNodeState());
            const NodeInputEvent third = {14, 0, 0.0f, 0.0f, 0};
            EventNodeVisitor thirdVisitor(third);
            thirdVisitor.traverse(this, m_field018, getBounds(), getNodeState());
        }
    }
    if (!update(0.0f)) result = false;
    // Direct virtual deletion, not the deleteInput/OutputStream hooks. Retail
    // leaves both fields unchanged and reloads +50 after the input destructor.
    IStream* input = m_field04c;
    if (input) delete input;
    OStream* output = m_field050;
    if (output) delete output;
    return result;
}
bool System::assignObjectPath(Object* object, const char* path)
{
    if (!object) return false;
    if (!object->getName() && !object->setName(nullptr)) return false;
    const char* oldName = object->getName();
    const char* leaf = path;
    char container[256];
    container[0] = 0;
    const char* separator = std::strrchr(path, '/');
    const char* backslash = std::strrchr(path, '\\');
    if (backslash && (!separator || backslash > separator)) separator = backslash;
    if (separator) {
        const unsigned int length = static_cast<unsigned int>(separator - path);
        std::memcpy(container, path, length); // Retail has no256-byte bound check.
        container[length] = 0;
        leaf = separator + 1;
    }
    if (!leaf) return oldName == nullptr; // After the two unguarded strrchr calls.
    NameTreeNode* where = m_registry->createContainer(this, container);
    if (!where) where = m_registry;
    NameTreeNode* existing = where->findChild(leaf);
    if (existing) return existing->getObject() == object;
    if (oldName) m_registry->eraseObject(this, object); // 82A320 wrapper
    object->setName(leaf);
    where->insert(this, object, nullptr);
    return true; // Retail ignores erase/setName/insert results.
}

void System::enqueueEvent(int type, int index, float first, float second, std::uint32_t field14)
{
    const QueuedEvent event(type, index, first, second, field14);
    m_eventQueue->enqueue(this, event); // 82A3D0, no queue-null guard
}

void systemPrintf(System* system, const char* format, ...)
{
    OStream* stream = system->getOutputStream();
    if (stream) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        std::vsprintf(buffer, format, args); // Shipped unbounded formatting.
        va_end(args);
        stream->write(buffer, static_cast<int>(std::strlen(buffer)));
    }
}

#if defined(_M_IX86)
static_assert(sizeof(Object) == 4, "BF2 client Object prefix size");
static_assert(sizeof(OStream) == 4, "Ref2System embeds a four-byte OStream");
static_assert(sizeof(System) == 0x58, "System has no STL-dependent members");
#endif

} }
#endif
