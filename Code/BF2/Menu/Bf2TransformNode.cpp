// HUD transform-node position and bounds. [INFERRED] Source placement.
// Higher accessors use the documented native interface where original Node slots remain incomplete.
#include "Bf2TransformNode.h"
#if !DEDICATED
#include "Bf2ButtonNode.h"
#include "../External/BFMeme/NodeState.h"
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
void (*editorHudNodeUpdated)(Bf2TransformNode*, float, float, float, float) = nullptr;
#endif
namespace {
bool sumNotLess(float first, float second, float limit) // 7C1ED1..7C1EEE, unordered passes
{
    bool result;
    __asm {
        fld first
        fadd second
        fcomp limit
        fnstsw ax
        test ah, 5
        setp result
    }
    return result;
}
bool stateEnabled(float value) // 7C1EF3..7C1F24; decoded value stays in x87
{
    const float one = 1.0f;
    bool result;
    __asm {
        fld value
        ftst
        fnstsw ax
        test ah, 5
        jp decoded
        fld1
        faddp st(1), st(0)
        fchs
    decoded:
        fcomp one
        fnstsw ax
        test ah, 5
        setp result
    }
    return result;
}

// [INFERRED] descriptor class name. Counter A22194 has only the add/sub/get
// references below; construction and destruction do not adjust it.
int bf2TransformNodeInstances;
class Bf2TransformNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2TransformNodeTypeDescriptor() : TypeDescriptor("dice::meme::Bf2TransformNode") {}
    TypeDescriptor* getBaseType() override { return TransformNode::getClassType(); }
    RuntimeObject* create() override // operator new(0x3C), null name and zero geometry
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Allocation and construction are real; publication retains Node33.
        void* allocation = ::operator new(sizeof(Bf2TransformNode));
        return allocation ? new(allocation) Bf2TransformNode(nullptr, 0, 0, 0, 0) : nullptr;
    }
    void addInstance() override { ++bf2TransformNodeInstances; }
    void removeInstance() override { --bf2TransformNodeInstances; } // 7C1CB0, no zero guard
    int getInstanceCount() override { return bf2TransformNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2TransformNode); }
};

char* cloneTransformNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}
// 737EE0 takes the address of the pointer: it frees the clone and nulls it.
void freeTransformNodeName(char*& name)
{
    ::operator delete(name);
    name = nullptr;
}
}

// The zero stores hit +24..+38 before the name is cloned; the four
// geometry ints are fild'd in the shipped order width, height, x, y.
Bf2TransformNode::Bf2TransformNode(const char* name, int x, int y, int width, int height)
    : m_name(nullptr), m_field028(0), m_offsetX(0), m_offsetY(0), m_positionX(nullptr), m_positionY(nullptr)
{
    freeTransformNodeName(m_name);          // clears the initially null clone, as shipped
    m_name = cloneTransformNodeName(name);
    m_size.x = static_cast<float>(width);   // +14
    m_size.y = static_cast<float>(height);  // +18
    m_position.x = static_cast<float>(x);   // +0C
    m_position.y = static_cast<float>(y);   // +10
}
Bf2TransformNode::~Bf2TransformNode()                    // 7C1BD0, then TransformNode's destructor
{ freeTransformNodeName(m_name); }
unsigned int Bf2TransformNode::getObjectSize() { return sizeof(Bf2TransformNode); }
const char* Bf2TransformNode::getName() { return m_name; }
bool Bf2TransformNode::setName(const char* name)
{
    freeTransformNodeName(m_name);
    m_name = cloneTransformNodeName(name); // free-before-copy aliasing defect retained
    return true;
}
bool Bf2TransformNode::slot_0x24() { return true; }

// Native transform properties bridge the still-untyped Node virtual slots.
bool Bf2TransformNode::setSizeForHud(float width, float height) // 7C1C00, +C8
{ m_size.x = width; m_size.y = height; return true; }
bool Bf2TransformNode::setPositionForHud(float x, float y)      // 7C1C20, +CC
{ m_position.x = x; m_position.y = y; return true; }
bool Bf2TransformNode::setPositionBindingForHud(int index, int* value)
{
    // A null pointer is refused; otherwise index 0 selects x and every other
    // index - not just 1 - selects y.
    if (!value) return false;
    if (index == 0) m_positionX = value;
    else m_positionY = value;
    return true;
}
bool Bf2TransformNode::setOffsetForHud(int x, int y)            // 77A8B0, +D4
{ m_offsetX = x; m_offsetY = y; return true; }

// Updates bound position and size, then handles button-related pointer behavior.
bool Bf2TransformNode::update(System* system, const NodeState* state,
                              const Rect* bounds, float delta)
{
    if (m_positionX) m_position.x = static_cast<float>(*m_positionX);
    if (m_positionY) m_position.y = static_cast<float>(*m_positionY);

    // These additions write back to the object. With no live binding a
    // nonzero offset therefore accumulates on every update, exactly as in PE.
    m_position.x += static_cast<float>(m_offsetX);
    m_position.y += static_cast<float>(m_offsetY);
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    if (editorHudNodeUpdated)
        editorHudNodeUpdated(this, bounds->x + m_position.x, bounds->y + m_position.y, m_size.x, m_size.y);
#endif

    hfe::Vec2 position = m_position;
    hfe::Vec2 size = m_size;
    Node* target = m_child;
    while (target) {
        // Retail asks for the button descriptor anew at each node, before it
        // asks that node for its runtime type.
        TypeDescriptor* buttonType = Bf2ButtonNode::getClassType();
        TypeDescriptor* type = target->getType();
        while (type && type != buttonType)
            type = type->getBaseType();
        if (type == buttonType) {
            Bf2ButtonNode* button = static_cast<Bf2ButtonNode*>(target);

            // Reload the position after the type walk: the PE does not retain
            // the snapshot made before those virtual callbacks.
            position = m_position;
            const hfe::Vec2 mousePosition = button->getMouseAreaPositionForHud();
            position.x += mousePosition.x;
            position.y += mousePosition.y;
            size = button->getMouseAreaSizeForHud();
            break;
        }

        // getChild returns a Node*&. The shipped body deliberately dispatches
        // it twice for every non-null successor; preserve that observable order.
        if (!target->getChild(0)) break;
        target = target->getChild(0);
    }

    const Rect transformed = {bounds->x + position.x, bounds->y + position.y,
        size.x, size.y, bounds->z};
    bool inside = false;
    if (state->target < 0.0f) {
        const float* pointer = system->getPointerPosition();
        inside = !(pointer[0] < transformed.x) && !(pointer[1] < transformed.y)
            && sumNotLess(transformed.width, transformed.x, pointer[0])
            && sumNotLess(transformed.height, transformed.y, pointer[1]);
    }

    updateNodeState(&m_state, system, m_child, this, &transformed, delta,
                    inside, stateEnabled(state->target));
    if (m_child && !m_child->update(system, &m_state, &transformed, delta))
        return false;
    return Node::update(system, state, bounds, delta);
}

TypeDescriptor* Bf2TransformNode::getClassType()
{
    static Bf2TransformNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2TransformNode::getType() { return getClassType(); }
namespace {
// The CRT initializer eagerly constructs the same descriptor (A22198 stores its
// pointer); native C++ supplies the guard/teardown spelling.
TypeDescriptor* const registeredBf2TransformNodeType = Bf2TransformNode::getClassType();
}
static_assert(sizeof(Bf2TransformNodeTypeDescriptor) == 16, "client BF2 transform metadata");
static_assert(sizeof(Bf2TransformNode) == 0x3C, "client BF2 named TransformNode");
} }
#endif
