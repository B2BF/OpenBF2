// Named HUD split node. [INFERRED] Source placement.
#include "Bf2SplitNode.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. The separate counterA227E4 has only the
// add/sub/get references below; neither construction nor destruction adjusts it.
int bf2SplitNodeInstances;
class Bf2SplitNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2SplitNodeTypeDescriptor() : TypeDescriptor("dice::meme::Bf2SplitNode") {}
    TypeDescriptor* getBaseType() override { return SplitNode::getClassType(); }
    RuntimeObject* create() override // 7CB130, inlines the null-name constructor
    {
        // [CHOSEN, not recovered] Constructs the available native Node prefix.
        // [RETIRE-WHEN: node-native-prefix-publication] Replace after the full Node interface and descriptor path are validated.
        void* allocation = ::operator new(sizeof(Bf2SplitNode));
        return allocation ? new(allocation) Bf2SplitNode(nullptr) : nullptr;
    }
    // Inherited destructor/description are the actual no-unlink/null bodies.
    void addInstance() override { ++bf2SplitNodeInstances; }
    void removeInstance() override { --bf2SplitNodeInstances; } // 7CB050, no zero guard
    int getInstanceCount() override { return bf2SplitNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2SplitNode); }
};

char* cloneSplitNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
}
Bf2SplitNode::Bf2SplitNode(const char* name) : m_name(nullptr)
{
    ::operator delete(m_name); // 737EE0 clears the initially null name before cloning.
    m_name = nullptr;
    m_name = cloneSplitNodeName(name);
}
Bf2SplitNode::~Bf2SplitNode()
{ ::operator delete(m_name); }
unsigned int Bf2SplitNode::getObjectSize() { return sizeof(Bf2SplitNode); }
const char* Bf2SplitNode::getName() { return m_name; }
bool Bf2SplitNode::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneSplitNodeName(name); // Retail free-before-copy aliasing defect retained.
    return true;
}
bool Bf2SplitNode::slot_0x24() { return true; }

TypeDescriptor* Bf2SplitNode::getClassType()
{
    static Bf2SplitNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2SplitNode::getType() { return getClassType(); }
namespace {
// CRT96DA68 ->86B8A0 eagerly constructs the same descriptorA227E8 and stores
// its pointer atA227FC. Native C++ provides the guard/teardown spelling; the
// original eager initializer sets its guard before the base constructor.
TypeDescriptor* const registeredBf2SplitNodeType = Bf2SplitNode::getClassType();
}
#if defined(_M_IX86)
static_assert(sizeof(Bf2SplitNodeTypeDescriptor) == 16, "client BF2 split metadata");
static_assert(sizeof(Bf2SplitNode) == 16, "client BF2 named SplitNode");
#endif
} }
#endif
