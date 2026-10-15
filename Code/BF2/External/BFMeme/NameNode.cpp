// Named node implementation; unresolved Node suffix operations stop at their callers.
#include "NameNode.h"
#if !DEDICATED
#include "TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] concrete descriptor class name. Eight-slot table94E688,
// getter8376F0 and eager initializer871820 read in full. The instance count
// is separate zero-initialized storage (A3A108), not an object-lifetime count.
int nameNodeInstances;
class NameNodeTypeDescriptor final : public TypeDescriptor
{
public:
    NameNodeTypeDescriptor() : TypeDescriptor("dice::meme::NameNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // PE8376A0. [RETIRE-WHEN: node-native-prefix-publication]
        // Allocation/constructor are real; later suffix dispatch stops at its
        // known caller until the complete Node interface can be declared.
        void* allocation = ::operator new(sizeof(NameNode));
        return allocation ? new(allocation) NameNode(nullptr) : nullptr;
    }
    // Inherited destructor/description are the recovered base no-unlink/null bodies.
    void addInstance() override { ++nameNodeInstances; }
    void removeInstance() override { --nameNodeInstances; }
    int getInstanceCount() override { return nameNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(NameNode); }
};

char* cloneNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
}
TypeDescriptor* NameNode::getClassType()
{
    static NameNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* NameNode::getType() { return getClassType(); }
namespace {
// CRT96E1F8 ->871820 constructs the same descriptor before normal lookup.
// Source spelling [INFERRED]; native C++ owns its static guard and teardown.
TypeDescriptor* const registeredNameNodeType = NameNode::getClassType();
}
NameNode::NameNode(const char* name) : m_name(nullptr)
{
    ::operator delete(m_name); // 737EE0 clears the initially null name before cloning.
    m_name = nullptr;
    m_name = cloneNodeName(name);
}
NameNode::~NameNode()
{
    ::operator delete(m_name);
    m_name = nullptr;
    // The inherited successor remains borrowed and is not deleted.
}
unsigned int NameNode::getObjectSize() { return sizeof(NameNode); }
const char* NameNode::getName() { return m_name; }
bool NameNode::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneNodeName(name); // Preserve the shipped free-before-copy aliasing defect.
    return true;
}
bool NameNode::slot_0x20() { return true; }
bool NameNode::slot_0x24() { return true; }
#if defined(_M_IX86)
static_assert(sizeof(NameNode) == 12, "client BFMeme NameNode");
static_assert(sizeof(NameNodeTypeDescriptor) == 16, "client NameNode descriptor");
#endif
} }
#endif
