// [INFERRED] Menu path. Primary930088, metadata935FE0, constructor7C2040,
// destructor773550, factory7C2080, getName68AD50 and setName8385B0 read in
// full. Everything else is EffectNode's inherited surface, including the paint
// and updateSelf bodies, so no new Node suffix caller stop appears here.
#include "Bf2EffectNode.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
char* cloneEffectNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseEffectNodeName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

Bf2EffectNode::Bf2EffectNode(const char* name, Effect* effect)
    : EffectNode(nullptr), m_name(nullptr)
{
    // setName inlined: release the just-cleared field, then clone. The borrowed
    // effect is stored last, after the name.
    releaseEffectNodeName(m_name);
    m_name = cloneEffectNodeName(name);
    m_effect = effect;
}
Bf2EffectNode::~Bf2EffectNode() { releaseEffectNodeName(m_name); }

TypeDescriptor* Bf2EffectNode::getClassType()
{
    // Complete eight-slot metadata935FE0; independent counter A221B8.
    static ClassTypeDescriptor<Bf2EffectNode, EffectNode> type("dice::meme::Bf2EffectNode");
    return &type;
}
TypeDescriptor* Bf2EffectNode::getType() { return getClassType(); }
unsigned int Bf2EffectNode::getObjectSize() { return sizeof(Bf2EffectNode); }
namespace {
TypeDescriptor* const registeredBf2EffectNodeType = Bf2EffectNode::getClassType();
}

const char* Bf2EffectNode::getName() { return m_name; }
bool Bf2EffectNode::setName(const char* name)
{
    releaseEffectNodeName(m_name); // the shipped free-before-copy aliasing defect
    m_name = cloneEffectNodeName(name);
    return true;
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2EffectNode) == 16, "native Bf2EffectNode storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2EffectNode, EffectNode>) == 16,
    "Bf2EffectNode metadata");
#endif
} }
#endif
