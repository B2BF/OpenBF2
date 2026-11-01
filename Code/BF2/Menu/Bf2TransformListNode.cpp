// HUD transform-list node properties. [INFERRED] Source placement.
#include "Bf2TransformListNode.h"
#if !DEDICATED
#include "Bf2TransformNode.h"
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {
namespace {
char* cloneTransformListName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseTransformListName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

// The name slot is released before it is ever written - the shipped
// body zeroes the surrounding members first, then releases +14 and clones into
// it - so that release always frees a null pointer.
Bf2TransformListNode::Bf2TransformListNode(const char* name, float x, float y, int count)
    : Node(), m_children(), m_name(nullptr), m_field018(0),
      m_positionBinding0(nullptr), m_positionBinding1(nullptr),
      m_count(count), m_offsetX(0), m_offsetY(0), m_field030(0), m_field034(1.0f),
      m_positionX(0.0f), m_positionY(0.0f)
{
    releaseTransformListName(m_name);
    m_name = cloneTransformListName(name);
    m_positionX = x;
    m_positionY = y;
}

// 7D1C00 -> 7D1C30. A plain operator delete on the name, not the nulling
// helper, and the list's elements are never freed: they are borrowed nodes the
// registry owns.
Bf2TransformListNode::~Bf2TransformListNode()
{
    ::operator delete(m_name);
}

TypeDescriptor* Bf2TransformListNode::getClassType()       // 7D18B0, singletonA23C8C
{
    static ClassTypeDescriptor<Bf2TransformListNode, Node>
        type("dice::meme::Bf2TransformListNode");
    return &type;
}
TypeDescriptor* Bf2TransformListNode::getType() { return getClassType(); }
unsigned int Bf2TransformListNode::getObjectSize() { return sizeof(Bf2TransformListNode); }
const char* Bf2TransformListNode::getName() { return m_name; }
bool Bf2TransformListNode::setName(const char* name)
{
    releaseTransformListName(m_name);
    m_name = cloneTransformListName(name);
    return true;
}
bool Bf2TransformListNode::slot_0x24() { return true; }

// Arrange the borrowed transforms before the ordinary Node update traversal.
bool Bf2TransformListNode::update(System* system, const NodeState* state,
                                  const Rect* bounds, float delta)
{
    if (m_positionBinding0) m_positionX = static_cast<float>(*m_positionBinding0);
    if (m_positionBinding1) m_positionY = static_cast<float>(*m_positionBinding1);
    m_positionX += static_cast<float>(m_offsetX);
    m_positionY += static_cast<float>(m_offsetY);

    float width = 0.0f;
    float height = 0.0f;
    const float gapY = static_cast<float>(m_count) * m_field034;
    const float gapX = static_cast<float>(m_count) * m_field030;
    for (Bf2TransformNode* child : m_children) {
        if (m_field030 > 0.0f) {
            width += child->getSize().x;
            if (height < child->getSize().y) height = child->getSize().y;
        } else {
            height += child->getSize().y;
            if (width < child->getSize().x) width = child->getSize().x;
        }
        width += gapX;
        height += gapY;
    }

    // The far edges are captured before either near-edge clamp. Even an
    // oversized list therefore follows the original two independent tests.
    const float right = width + m_positionX;
    const float bottom = height + m_positionY;
    if (system->getWidth() * 0.5f + bounds->x > m_positionX)
        m_positionX = system->getWidth() * 0.5f + bounds->x;
    if (system->getWidth() * 0.5f + bounds->x + bounds->width < right)
        m_positionX = system->getWidth() * 0.5f + bounds->x + bounds->width - width;
    if (system->getHeight() * 0.5f + bounds->y > m_positionY)
        m_positionY = system->getHeight() * 0.5f + bounds->y;
    if (system->getHeight() * 0.5f + bounds->height + bounds->y < bottom)
        m_positionY = system->getHeight() * 0.5f + bounds->height + bounds->y - height;

    float x = m_positionX;
    float y = m_positionY;
    for (Bf2TransformNode* child : m_children) {
        child->setPositionForHud(x, y);
        const float childStepY = child->getSize().y * m_field034;
        x = m_field030 * child->getSize().x + x + gapX;
        y = childStepY + y + gapY;
    }
    return Node::update(system, state, bounds, delta);
}

namespace {
TypeDescriptor* const registeredBf2TransformListNodeType = Bf2TransformListNode::getClassType();
}

// 7D1840, +C8. The same pair the constructor's second and third arguments reach.
bool Bf2TransformListNode::setPositionForHud(float x, float y)
{ m_positionX = x; m_positionY = y; return true; }

// 7D1BE0, +CC. No range test of any kind.
bool Bf2TransformListNode::setOffsetForHud(int x, int y)
{ m_offsetX = x; m_offsetY = y; return true; }

// 7C8A90, +D0. not an array: the shipped body tests the index against zero and
// sends everything else - negatives included - to the second slot.
bool Bf2TransformListNode::setPositionBindingForHud(int index, int* binding)
{
    if (!binding) return false;
    if (index == 0) { m_positionBinding0 = binding; return true; }
    m_positionBinding1 = binding;
    return true;
}

// 7D1C60, +D4. The shipped body splices a fresh link in before the list head,
// which is push_back; the child is borrowed.
bool Bf2TransformListNode::addChildForHud(Bf2TransformNode* child)
{
    if (!child) return false;
    m_children.push_back(child);
    return true;
}
} }
#endif
