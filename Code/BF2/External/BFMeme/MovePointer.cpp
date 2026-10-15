// Full operational instructions read from BF2.exe. Construction and descriptor
// remain declaration-only while TransformNode identity is unpublished.
// No raw Node suffix dispatch is introduced.
#include "MovePointer.h"
#if !DEDICATED
#include "TransformNode.h"
#include "System.h"
#include "TypeDescriptor.h"
#include "ColorEffect.h"
#include <string>
namespace dice { namespace meme {
namespace {
// Keep the live x87 intermediate: a double temporary can lose cancellation
// bits when the caller selects extended precision (82B302..82B384).
float moveOffset(float value, float start, float origin)
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld value
        fsub start
        fadd origin
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(static_cast<double>(value) - start + origin);
#endif
}
float outlineThickness(float dimension) // 82B6D9..82B6EF and three repeated arms
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld1
        fsub dimension
        fadd dimension
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(1.0 - dimension + dimension);
#endif
}
TransformNode* asTransform(Node* node)
{
    if (!node) return nullptr;
    TypeDescriptor* expected = TransformNode::getClassType();
    TypeDescriptor* actual = node->getType();
    while (actual) {
        if (actual == expected) return static_cast<TransformNode*>(node);
        actual = actual->getBaseType();
    }
    return nullptr;
}
class SelectTransformVisitor : public NodeVisitor
{
public:
    SelectTransformVisitor(TransformNode** selected, float* position)
        : NodeVisitor(0), m_selected(selected), m_position(position) {}
    void visit(System* system, Node*& node, const Rect* bounds, NodeState* state) override
    {
        if (!node || !(state->target < 0.0f)) return;
        TransformNode* selected = asTransform(node);
        if (selected && selected->getState()->target < 0.0f) {
            m_position[0] = bounds->x;
            m_position[1] = bounds->y;
            const auto& position = selected->getPosition();
            m_position[0] = static_cast<float>(static_cast<double>(position.x) + m_position[0]);
            m_position[1] = static_cast<float>(static_cast<double>(position.y) + m_position[1]);
            *m_selected = selected;
        }
        // The current node is reloaded after type/state callbacks. No new guard.
        SelectTransformVisitor visitor(m_selected, m_position);
        node->visitChildren(system, state, bounds, &visitor);
    }
private:
    TransformNode** m_selected;
    float* m_position;
};
class PaintTransformVisitor : public NodeVisitor
{
public:
    PaintTransformVisitor(TransformNode** selected, bool* next)
        : NodeVisitor(0), m_selected(selected), m_next(next) {}
    void visit(System* system, Node*& node, const Rect* bounds, NodeState* state) override
    {
        if (*m_next) {
            *m_next = false;
            RenderContext context = {nullptr, nullptr, 0, 1, 0, 1, 1};
            {
                Rect outline = *bounds;
                outline.x = static_cast<float>(static_cast<double>(bounds->x) - 1.0);
                outline.y = static_cast<float>(static_cast<double>(bounds->y) - 1.0);
                outline.width = static_cast<float>(static_cast<double>(outline.width) + 2.0);
                outline.height = outlineThickness(outline.height);
                Quad quad(&outline, nullptr);
                drawContextTexture(&context, system, 0, &quad);
            }
            {
                Rect outline = *bounds;
                outline.x = static_cast<float>(static_cast<double>(bounds->x) - 1.0);
                outline.y = static_cast<float>(static_cast<double>(bounds->y) + outline.height);
                outline.width = static_cast<float>(static_cast<double>(outline.width) + 2.0);
                outline.height = outlineThickness(outline.height);
                Quad quad(&outline, nullptr);
                drawContextTexture(&context, system, 0, &quad);
            }
            {
                Rect outline = *bounds;
                outline.x = static_cast<float>(static_cast<double>(bounds->x) - 1.0);
                outline.width = outlineThickness(bounds->width);
                Quad quad(&outline, nullptr);
                drawContextTexture(&context, system, 0, &quad);
            }
            {
                Rect outline = *bounds;
                outline.x = static_cast<float>(static_cast<double>(bounds->x) + bounds->width);
                outline.width = outlineThickness(bounds->width);
                Quad quad(&outline, nullptr);
                drawContextTexture(&context, system, 0, &quad);
            }
            ColorEffect effect(.1f, .1f, .1f, .1f);
            context.effect = &effect;
            Quad quad(bounds, bounds);
            drawContextTexture(&context, system, 0, &quad);
        } else if (node && state->target < 0.0f) {
            if (*m_selected == node) *m_next = true;
            PaintTransformVisitor visitor(m_selected, m_next);
            node->visitChildren(system, state, bounds, &visitor);
        }
    }
private:
    TransformNode** m_selected;
    bool* m_next;
};
}
MovePointer::~MovePointer() {} // 82B8D0, inherited Pointer owns no graphic
TypeDescriptor* MovePointer::getType() { return getClassType(); }
unsigned int MovePointer::getObjectSize() { return sizeof(MovePointer); }
bool MovePointer::paint(System* system)
{
    if (!Pointer::paint(system)) return false;
    if (m_selected && !m_dragging) {
        bool next = false;
        PaintTransformVisitor visitor(&m_selected, &next);
        NodeState* state = system->getNodeState();
        const Rect* bounds = system->getBounds();
        visitor.traverse(system, system->getRootNode(), bounds, state);
    }
    return true;
}
bool MovePointer::setPosition(System* system, float x, float y)
{
    if (!Pointer::setPosition(system, x, y)) return false;
    if (m_dragging) {
        TransformNode* selected = m_selected;
        if (selected) {
            float deltaY = static_cast<float>(static_cast<double>(y) - m_startY);
            if (m_resizing) {
                float width = moveOffset(x, m_startX, m_startWidth);
                const double unroundedHeight = static_cast<double>(deltaY) + m_startHeight;
                float height = static_cast<float>(unroundedHeight);
                if (width < 2.0f) width = 2.0f;
                if (unroundedHeight < 2.0) height = 2.0f;
                auto& size = selected->getSize();
                size.x = width;
                size.y = height;
            } else {
                const float positionX = moveOffset(x, m_startX, m_startPositionX);
                const float positionY = static_cast<float>(static_cast<double>(deltaY) + m_startPositionY);
                auto& position = selected->getPosition();
                position.x = positionX;
                position.y = positionY;
            }
        }
    } else {
        m_selected = nullptr;
        float position[2] = {0, 0};
        SelectTransformVisitor visitor(&m_selected, position);
        NodeState* state = system->getNodeState();
        const Rect* bounds = system->getBounds();
        visitor.traverse(system, system->getRootNode(), bounds, state);
    }
    return true;
}
bool MovePointer::event1(System* system, int first, int, int) // 82B3F0; no enabled guard
{
    system->lockPointer();
    const float* position = system->getPointerPosition();
    m_startX = position[0];
    m_startY = position[1];
    m_selected = nullptr;
    float offset[2] = {0, 0};
    SelectTransformVisitor visitor(&m_selected, offset);
    NodeState* state = system->getNodeState();
    const Rect* bounds = system->getBounds();
    visitor.traverse(system, system->getRootNode(), bounds, state);
    if (m_selected) {
        const auto& selectedPosition = m_selected->getPosition();
        m_startPositionX = selectedPosition.x;
        m_startPositionY = selectedPosition.y;
        const auto& size = m_selected->getSize();
        m_startWidth = size.x;
        m_startHeight = size.y;
    }
    position = system->getPointerPosition();
    m_relativeX = position[0];
    m_relativeY = position[1];
    m_relativeX -= offset[0];
    m_relativeY -= offset[1];
    m_dragging = true;
    m_resizing = first == 4;
    return true;
}
bool MovePointer::event2(System* system, int, int) // 82B940; no enabled guard
{
    system->unlockPointer();
    m_dragging = false;
    if (m_selected) {
        std::string name("NoName");
        if (m_selected->slot_0x24()) name = m_selected->getName();
    }
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(MovePointer) == 64, "Native x86 MovePointer layout");
#endif
} }
#endif
