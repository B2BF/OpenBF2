// Named HUD text node; drawing and streaming use the inherited text-node implementation. [INFERRED] Source placement.
#include "Bf2TextNode.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
char* cloneTextNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseTextNodeName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

Bf2TextNode::Bf2TextNode(const char* name) : m_name(nullptr)
{
    // The base has already cleared these three and built the shadow effect;
    // retail re-clears them anyway before the inlined setName.
    m_style = nullptr;
    m_string = nullptr;
    m_type = nullptr;
    releaseTextNodeName(m_name);
    m_name = cloneTextNodeName(name);
}
Bf2TextNode::~Bf2TextNode() { releaseTextNodeName(m_name); }

TypeDescriptor* Bf2TextNode::getClassType()
{
    // Complete eight-slot metadata93619C; independent counter.
    static ClassTypeDescriptor<Bf2TextNode, BfTextNode> type("dice::meme::Bf2TextNode");
    return &type;
}
TypeDescriptor* Bf2TextNode::getType() { return getClassType(); }
unsigned int Bf2TextNode::getObjectSize() { return sizeof(Bf2TextNode); }
namespace {
TypeDescriptor* const registeredBf2TextNodeType = Bf2TextNode::getClassType();
}

const char* Bf2TextNode::getName() { return m_name; }
bool Bf2TextNode::setName(const char* name)
{
    releaseTextNodeName(m_name); // the shipped free-before-copy aliasing defect
    m_name = cloneTextNodeName(name);
    return true;
}
bool Bf2TextNode::slot_0x24() { return true; }

// native +C8..+D8 accessors
bool Bf2TextNode::setStringDataForHud(Data* data)
{ m_string = data; return true; }
bool Bf2TextNode::setStyleForHud(Style* style)
{ m_style = style; return true; }
bool Bf2TextNode::setTypeDataForHud(Data* type)
{ m_type = type; return true; }
Data* Bf2TextNode::getStringDataForHud() { return m_string; }
Style* Bf2TextNode::getStyleForHud() { return m_style; }

#if defined(_M_IX86)
static_assert(sizeof(Bf2TextNode) == 28, "native Bf2TextNode storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2TextNode, BfTextNode>) == 16,
    "Bf2TextNode metadata");
#endif
} }
#endif
