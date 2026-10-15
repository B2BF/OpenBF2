// HUD list-box node properties. [INFERRED] Source placement.
#include "Bf2NewListBoxNode.h"
#if !DEDICATED
#include "BfListBoxData.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {
namespace {
// 737DC0/737EE0, the shared clone and release the other Bf2 node units each
// spell locally.
char* cloneListBoxName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseListBoxName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}
}

// Copies name into owned storage after initializing the name slot to null.
Bf2NewListBoxNode::Bf2NewListBoxNode(const char* name) : BfNewListBoxNode()
{
    m_name = nullptr;
    releaseListBoxName(m_name);
    m_name = cloneListBoxName(name);
}


Bf2NewListBoxNode::~Bf2NewListBoxNode() { releaseListBoxName(m_name); }

TypeDescriptor* Bf2NewListBoxNode::getClassType()
{
    static ClassTypeDescriptor<Bf2NewListBoxNode, BfNewListBoxNode>
        type("dice::meme::Bf2NewListBoxNode");
    return &type;
}
TypeDescriptor* Bf2NewListBoxNode::getType() { return getClassType(); }
unsigned int Bf2NewListBoxNode::getObjectSize() { return sizeof(Bf2NewListBoxNode); }
const char* Bf2NewListBoxNode::getName() { return m_name; }
bool Bf2NewListBoxNode::setName(const char* name)
{
    releaseListBoxName(m_name);
    m_name = cloneListBoxName(name);
    return true;
}
bool Bf2NewListBoxNode::slot_0x24() { return true; }

namespace {
TypeDescriptor* const registeredBf2NewListBoxNodeType = Bf2NewListBoxNode::getClassType();
}

// The published added slots. Every one is a single store; the shipped bodies are
// scattered across the image because MSVC folded identical setters across every
// class that has a member at the same offset - which is why so many of these
// addresses sit nowhere near the rest of this class.

bool Bf2NewListBoxNode::setDataForHud(BfListBoxData* data)       // 7CDD60, +CC
{ m_data = data; return true; }
BfListBoxData* Bf2NewListBoxNode::getDataForHud() { return m_data; }
// 7CDD70, +D4: the data's own back-pointer to this node. No null guard.
void Bf2NewListBoxNode::bindDataToSelfForHud() { m_data->m_node = this; }

// 7CDD80, +D8. Two shipped quirks: a negative index is accepted and writes
// before the array, and the return value is the index's low byte - so index 0
// reports false even though the write happened.
bool Bf2NewListBoxNode::setFontForHud(FontHandle font, int index)
{
    if (index < 5) m_fonts[index] = font;
    return index != 0;
}

bool Bf2NewListBoxNode::setSelectActionForHud(Action* action)    // 7CDDA0, +DC
{ m_selectAction = action; return true; }
bool Bf2NewListBoxNode::setFieldE0ForHud(Action* action)         // 7CDDB0, +E0
{ m_field04c = action; return true; }
bool Bf2NewListBoxNode::setFocusActionForHud(Action* action)     // 7CDDC0, +E4
{ m_focusAction = action; return true; }
bool Bf2NewListBoxNode::setFieldE8ForHud(Action* action)         // 7CDDD0, +E8
{ m_field054 = action; return true; }
// The two getters are crossed against the setter order: +EC answers the +48
// member and +F0 the +50 one, where the setters run +48, +4C, +50, +54.
Action* Bf2NewListBoxNode::getSelectActionForHud() { return m_selectAction; }
Action* Bf2NewListBoxNode::getFocusActionForHud() { return m_focusAction; }

void Bf2NewListBoxNode::setSound070ForHud(int sound) { m_sound070 = sound; }
void Bf2NewListBoxNode::setWidthForHud(float width) { m_width = width; }
void Bf2NewListBoxNode::setSound068ForHud(int sound) { m_sound068 = sound; }
void Bf2NewListBoxNode::setSound06cForHud(int sound) { m_sound06c = sound; }
void Bf2NewListBoxNode::setRowHeightForHud(float h) { m_rowHeight = h; }
void Bf2NewListBoxNode::setRowSpacingForHud(float s) { m_spacing = s; }
void Bf2NewListBoxNode::setBorderEnabledForHud(bool e) { m_border = e; }
void Bf2NewListBoxNode::setBorderTopForHud(int v) { m_top = v; }
void Bf2NewListBoxNode::setBorderBottomForHud(int v) { m_bottom = v; }
void Bf2NewListBoxNode::setBorderLeftForHud(int v) { m_left = v; }
void Bf2NewListBoxNode::setBorderRightForHud(int v) { m_right = v; }

void Bf2NewListBoxNode::setBackgroundRedForHud(float v) { m_background.r = v; }
void Bf2NewListBoxNode::setBackgroundGreenForHud(float v) { m_background.g = v; }
void Bf2NewListBoxNode::setBackgroundBlueForHud(float v) { m_background.b = v; }
void Bf2NewListBoxNode::setBackgroundAlphaForHud(float v) { m_background.a = v; }
void Bf2NewListBoxNode::setFrameRedForHud(float v) { m_frame.r = v; }
void Bf2NewListBoxNode::setFrameGreenForHud(float v) { m_frame.g = v; }
void Bf2NewListBoxNode::setFrameBlueForHud(float v) { m_frame.b = v; }
void Bf2NewListBoxNode::setFrameAlphaForHud(float v) { m_frame.a = v; }
void Bf2NewListBoxNode::setSelectionRedForHud(float v) { m_selection.r = v; }
void Bf2NewListBoxNode::setSelectionGreenForHud(float v) { m_selection.g = v; }
void Bf2NewListBoxNode::setSelectionBlueForHud(float v) { m_selection.b = v; }
void Bf2NewListBoxNode::setSelectionAlphaForHud(float v) { m_selection.a = v; }
void Bf2NewListBoxNode::setScrollOffsetForHud(float v) { m_scrollOffset = v; }
void Bf2NewListBoxNode::setScrollWidthForHud(float v) { m_scrollWidth = v; }
void Bf2NewListBoxNode::setScrollbarRedForHud(float v) { m_scrollbar.r = v; }
void Bf2NewListBoxNode::setScrollbarGreenForHud(float v) { m_scrollbar.g = v; }
void Bf2NewListBoxNode::setScrollbarBlueForHud(float v) { m_scrollbar.b = v; }
void Bf2NewListBoxNode::setScrollbarAlphaForHud(float v) { m_scrollbar.a = v; }
void Bf2NewListBoxNode::setScrollbarBackRedForHud(float v) { m_scrollbarBackground.r = v; }
void Bf2NewListBoxNode::setScrollbarBackGreenForHud(float v) { m_scrollbarBackground.g = v; }
void Bf2NewListBoxNode::setScrollbarBackBlueForHud(float v) { m_scrollbarBackground.b = v; }
void Bf2NewListBoxNode::setScrollbarBackAlphaForHud(float v) { m_scrollbarBackground.a = v; }
void Bf2NewListBoxNode::setField109ForHud(bool v) { m_field109 = v; }
} }
#endif
