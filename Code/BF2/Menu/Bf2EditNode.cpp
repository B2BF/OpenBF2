// HUD edit-node properties. [INFERRED] Source placement.
#include "Bf2EditNode.h"
#if !DEDICATED
#include "BfEditData.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {
namespace {
// 737DC0/737EE0, the shared clone and release the other Bf2 node units each
// spell locally.
char* cloneEditNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseEditNodeName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

// Initializes an owned name clone. The initial release receives null.
Bf2EditNode::Bf2EditNode(const char* name) : BfEditNode()
{
    m_name = nullptr;
    releaseEditNodeName(m_name);
    m_name = cloneEditNodeName(name);
}


Bf2EditNode::~Bf2EditNode() { releaseEditNodeName(m_name); }

TypeDescriptor* Bf2EditNode::getClassType()
{
    static ClassTypeDescriptor<Bf2EditNode, BfEditNode> type("dice::meme::Bf2EditNode");
    return &type;
}
TypeDescriptor* Bf2EditNode::getType() { return getClassType(); }
unsigned int Bf2EditNode::getObjectSize() { return sizeof(Bf2EditNode); }
const char* Bf2EditNode::getName() { return m_name; }
bool Bf2EditNode::setName(const char* name)
{
    releaseEditNodeName(m_name);
    m_name = cloneEditNodeName(name);
    return true;
}
bool Bf2EditNode::slot_0x24() { return true; }

namespace {
TypeDescriptor* const registeredBf2EditNodeType = Bf2EditNode::getClassType();
}

// The five added slots. Each is a single store answering a hard true; none has a
// null test or a range test of its own - the console rows carry what guards exist.
bool Bf2EditNode::setEditDataForHud(BfEditData* data)     // 7CDFC0, +C8
{ m_editData = data; return true; }
bool Bf2EditNode::setStringForHud(Data* string)           // 7CE250, +CC
{ m_string = string; return true; }
// Two members from one call, and the flag is the outline bit at +3D.
bool Bf2EditNode::setFontForHud(FontHandle font, bool outline)
{ m_font = font; m_outline = outline; return true; }
bool Bf2EditNode::setMaxLengthForHud(int characters)      // 7C21D0, +D4
{ m_maxCharacters = characters; return true; }
bool Bf2EditNode::setColorForHud(float r, float g, float b, float a) // 7D0F40, +D8
{
    m_color.r = r;
    m_color.g = g;
    m_color.b = b;
    m_color.a = a;
    return true;
}
} }
#endif
