// HUD button behavior and event handling. [INFERRED] Source placement.
#include "Bf2ButtonNode.h"
#if !DEDICATED
#include "../External/BFMeme/Action.h"
#include "../External/BFMeme/Effect.h"     // Effect::setColor (+40), drawContextTexture (82D790)
#include "../External/BFMeme/Quad.h"       // Quad, Color, colorQuad (771FA0), setQuadBounds (7AA7F0)
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstdio>
#include <cstdlib>
namespace dice { namespace meme {
namespace {
// 737EE0, the shared release the other Bf2 node units each spell locally.
void releaseButtonName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

// x and y are accepted and never read: only width and height reach
// storage, as the mouse area's size. The position half stays (0,0) until a
// setMouseArea row supplies one. +90/+94 are zeroed twice, before and after
// the two FILDs, and +B4/+B8 are never written at all.
Bf2ButtonNode::Bf2ButtonNode(const char* name, int, int, int width, int height)
    : Bf2PictureNode(),
      m_normalTexture(0), m_pressedTexture(0), m_highlightTexture(0), m_currentTexture(0),
      m_actions(), m_altActions(),
      m_mouseOverColor(),
      m_mouseOverColorSet(false), m_debug(false), m_pointerHeld(false),
      m_pointerReleased(true), m_fieldbc(false)
{
    m_mouseAreaPosition.x = 0.0f;
    m_mouseAreaPosition.y = 0.0f;
    m_mouseOverColor.r = 0.0f; m_mouseOverColor.g = 0.0f;
    m_mouseOverColor.b = 0.0f; m_mouseOverColor.a = 0.0f;
    m_mouseAreaSize.x = static_cast<float>(width);
    m_mouseAreaSize.y = static_cast<float>(height);
    m_mouseAreaPosition.x = 0.0f;                        // the shipped repeat
    m_mouseAreaPosition.y = 0.0f;
    for (int i = 0; i != 4; ++i) { m_actions[i] = nullptr; m_altActions[i] = nullptr; }
    setName(name);                                       // 7CDFD0, the inherited clone
}

// The name is released here and then again by ~Bf2PictureNode; the
// shared helper nulls the slot, so the second release frees a null pointer.
Bf2ButtonNode::~Bf2ButtonNode()
{
    releaseButtonName(m_name);
}

TypeDescriptor* Bf2ButtonNode::getClassType()
{
    static ClassTypeDescriptor<Bf2ButtonNode, Bf2PictureNode> type("dice::meme::Bf2ButtonNode");
    return &type;
}
TypeDescriptor* Bf2ButtonNode::getType() { return getClassType(); }
unsigned int Bf2ButtonNode::getObjectSize() { return sizeof(Bf2ButtonNode); }

namespace {
// CRT registration mirrors the other Bf2 nodes; the factory7D0CB0 supplies a
// null name and 20 for all four integers.
TypeDescriptor* const registeredBf2ButtonNodeType = Bf2ButtonNode::getClassType();
}

// The chosen accessors for the original +C0 and +EC..+110 slots.

// 7D0A10, the +C0 slot. Three arguments, none of them read.
bool Bf2ButtonNode::releasePointerForHud()
{
    m_pointerHeld = false;
    m_pointerReleased = true;
    return true;
}

// 7D0650, +EC. A zero texture is refused before the index is even looked at,
// index 0 and anything above 3 are refused, and the index-1 arm answers with the
// index's low byte instead of a literal 1 (the same value, reproduced as read).
bool Bf2ButtonNode::setTextureForHud(int index, TextureHandle texture)
{
    if (!texture) return false;
    if (index == 1) {
        m_texture = texture;                             // PictureNode's own handle
        m_normalTexture = texture;
        return index != 0;                               // MOV AL,DL at 7D0668
    }
    if (index == 2) { m_pressedTexture = texture; return true; }
    if (index == 3) { m_highlightTexture = texture; return true; }
    return false;
}

// 7D0B50/7D0B70, +F0/+F4. No range test at all - the console rows do it.
bool Bf2ButtonNode::setActionForHud(Action* action, int index)
{ m_actions[index] = action; return true; }
bool Bf2ButtonNode::setAltActionForHud(Action* action, int index)
{ m_altActions[index] = action; return true; }

// 7D0B90/7D0BA0, +F8/+FC.
Action* Bf2ButtonNode::getActionForHud(int index) { return m_actions[index]; }
Action* Bf2ButtonNode::getAltActionForHud(int index) { return m_altActions[index]; }

// 7D0690, +100. The size pair is stored before the position pair.
bool Bf2ButtonNode::setMouseAreaForHud(int x, int y, int width, int height)
{
    m_mouseAreaSize.x = static_cast<float>(width);
    m_mouseAreaSize.y = static_cast<float>(height);
    m_mouseAreaPosition.x = static_cast<float>(x);
    m_mouseAreaPosition.y = static_cast<float>(y);
    return true;
}

// 7D0BC0, +104. The four floats arrive as one 16-byte by-value block, which the
// shipped body copies to a local and then out again; the flag goes up last.
bool Bf2ButtonNode::setMouseOverColorForHud(Color color)
{
    m_mouseOverColor = color;
    m_mouseOverColorSet = true;
    return true;
}


bool Bf2ButtonNode::setDebugForHud(bool enabled) { m_debug = enabled; return true; }

// 7D0C30/7D0C50, +10C/+110: both return their pair by value through the hidden
// pointer, which is why the shipped bodies write two dwords and answer it.
hfe::Vec2 Bf2ButtonNode::getMouseAreaPositionForHud() { return m_mouseAreaPosition; }
hfe::Vec2 Bf2ButtonNode::getMouseAreaSizeForHud() { return m_mouseAreaSize; }

// The three event overrides. All three first ask the System who owns the pointer;
// only the node that owns it acts. The event they hand an action is a local
// NodeInputEvent whose first word is 1 and whose remaining four are zero.

namespace {
// The 20-byte stack record at [ebp-0x14] in7D0840/7D0910: one word set to 1 and
// four zeroed. [INFERRED] that this is the NodeInputEvent the actions receive.
struct ButtonActionEvent { int kind; int words[4]; };
}

// 7D0840, +74. first==1 is the ordinary press, first==0x15 the alternate one.
// Only the press arm updates the displayed texture, and it does so after the
// action has already run.
bool Bf2ButtonNode::onEvent1(System* system, const Rect* bounds, const NodeState* state,
                             int first, int, int)
{
    if (system->getPointerOwner() != this) return false;
    if (first == 1) {
        ButtonActionEvent event = { first, { 0, 0, 0, 0 } };
        if (m_actions[0])
            m_actions[0]->execute(system, bounds, state,
                                  reinterpret_cast<const NodeInputEvent*>(&event));
        if (m_highlightTexture) m_currentTexture = m_highlightTexture;
        return true;
    }
    if (first == 0x15) {
        ButtonActionEvent event = { 1, { 0, 0, 0, 0 } };
        if (m_altActions[0])
            m_altActions[0]->execute(system, bounds, state,
                                     reinterpret_cast<const NodeInputEvent*>(&event));
        return true;
    }
    return false;
}

// 7D0910, +78. Unlike onEvent1 this one always forwards to Node::onEvent2,
// and it refuses only when another node owns the pointer.
bool Bf2ButtonNode::onEvent2(System* system, const Rect* bounds, const NodeState* state,
                             int first, int index)
{
    if (system->getPointerOwner() != this) return false;
    if (first == 1) {
        if (m_pressedTexture) m_currentTexture = m_pressedTexture;
        if (m_actions[3]) {
            ButtonActionEvent event = { 1, { 0, 0, 0, 0 } };
            m_actions[3]->execute(system, bounds, state,
                                  reinterpret_cast<const NodeInputEvent*>(&event));
        }
    } else if (first == 0x15) {
        if (m_altActions[3]) {
            ButtonActionEvent event = { 1, { 0, 0, 0, 0 } };
            m_altActions[3]->execute(system, bounds, state,
                                     reinterpret_cast<const NodeInputEvent*>(&event));
        }
    }
    return Node::onEvent2(system, bounds, state, first, index);
}

// 7D09E0, +7C. No pointer-owner test on this one, and the base always runs.
bool Bf2ButtonNode::onEvent3(System* system, const Rect* bounds, const NodeState* state,
                             int first, int index)
{
    if (first == 1 && m_normalTexture) m_currentTexture = m_normalTexture;
    return Node::onEvent3(system, bounds, state, first, index);
}

// Hit-tests the painted rectangle and runs the press or release action.
// Points on the rectangle edges are outside. Returns true after processing the frame.
bool Bf2ButtonNode::updateSelf(System* system, const NodeState* state, const Rect* rect, float)
{
    if (!m_currentTexture)
        m_currentTexture = m_normalTexture;

    const float* const pointer = system->getPointerPosition();   // 682BA0, &m_pointerX
    const float pointerX = pointer[0];
    const float pointerY = pointer[1];

    if (pointerX > rect->x && pointerX < rect->x + rect->width
        && pointerY > rect->y && pointerY < rect->y + rect->height)
    {
        if (m_pressedTexture
            && m_currentTexture != m_pressedTexture
            && m_currentTexture != m_highlightTexture)
            m_currentTexture = m_pressedTexture;

        system->setPointerOwner(this);                   // +0xC4

        if (!m_pointerHeld)
        {
            system->setPointerOwner(this);               // again, as shipped
            if (m_actions[1])
            {
                NodeInputEvent input;
                input.type    = 1;
                input.index   = 0;
                input.first   = 0.0f;
                input.second  = 0.0f;
                input.field10 = 0;
                m_actions[1]->execute(system, rect, state, &input);   // +0x40
                m_pointerHeld = true;                    // only when an action exists
            }
        }
        m_pointerReleased = false;
        return true;
    }

    if (m_currentTexture == m_pressedTexture
        || m_currentTexture == m_highlightTexture
        || !m_pointerReleased)
    {
        if (m_actions[2] && !m_pointerReleased)
        {
            NodeInputEvent input;
            input.type    = 1;
            input.index   = 0;
            input.first   = 0.0f;
            input.second  = 0.0f;
            input.field10 = 0;
            m_actions[2]->execute(system, rect, state, &input);
        }
        m_pointerHeld     = false;
        m_pointerReleased = true;
        m_currentTexture  = m_normalTexture;
    }
    return true;
}

// Draws the optional debug overlay, then the button picture. The hover color is unused.
void Bf2ButtonNode::paintSelf(System* system, const NodeState* state, const Rect* rect,
                              const Rect* clip, RenderContext* context)
{
    if (m_debug)                                        // +B1
    {
        Quad quad(rect, clip);
        setQuadBounds(&quad,
                      m_mouseAreaPosition.x + rect->x,
                      m_mouseAreaPosition.y + rect->y,
                      m_mouseAreaSize.x,
                      m_mouseAreaSize.y);

        Color tint;
        if (m_currentTexture == m_pressedTexture)
        {
            tint.r = 1.0f; tint.g = 0.0f; tint.b = 0.0f; tint.a = 0.7f;
        }
        else
        {
            tint.r = 1.0f; tint.g = 1.0f; tint.b = 1.0f; tint.a = 0.4f;
        }
        colorQuad(&quad, &tint);

        Effect effect;
        RenderContext debugContext;
        debugContext.parent  = 0;
        debugContext.effect  = &effect;
        debugContext.field08 = 0.0f;
        debugContext.field0c = 1.0f;
        debugContext.field10 = 0.0f;
        debugContext.field14 = 1.0f;
        debugContext.field18 = 1.0f;

        if (m_currentTexture == m_pressedTexture)       // tested again
            effect.setColor(1.0f, 0.0f, 0.0f, 0.7f);    // +0x40
        else
            effect.setColor(1.0f, 1.0f, 1.0f, 0.4f);

        drawContextTexture(&debugContext, system, 0, &quad);   // 82D790, texture 0
    }

    if (m_currentTexture)
    {
        m_texture = m_currentTexture;                   // PictureNode +08
        Bf2PictureNode::paintSelf(system, state, rect, clip, context);
    }
}
} }
#endif
