// Interactive list-box node.
#ifndef MENU_BFNEWLISTBOXNODE_H
#define MENU_BFNEWLISTBOXNODE_H
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/Quad.h"
#if !DEDICATED
#include <list>
namespace dice { namespace meme {
class BfListBoxData;
class Action;
class BfNewListBoxNode : public Node
{
    friend class BfListBoxData;
public:
    BfNewListBoxNode();
    ~BfNewListBoxNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override; // 7C33E0 [FRONTIER]
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override; // 7C40A0 [FRONTIER]
    bool onEvent2(System*, const Rect*, const NodeState*, int, int) override;
    bool onEvent3(System*, const Rect*, const NodeState*, int, int) override; // 7C46D0 [FRONTIER]
    bool enqueueEvent(int type);                      // 7C56C0, native list<int>
    static TypeDescriptor* getClassType();             // 7C3320, metadata936460

    // [CHOSEN, not recovered] Sets list-box selectability through native dispatch.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    void setSelectableForHud(bool selectable);         // 7C39B0, +C8
protected:
    // [INFERRED] Row-navigation helpers used by queued input events.
    bool selectPreviousRow(System*);
    bool selectNextRow(System*, const Rect*);
    bool advanceVisiblePage(System*);
    // 7C2FB0, the fourth of the family and the twin of advanceVisiblePage: one row instead of one
    // page. It takes a System* and never reads it, exactly as selectPreviousRow does, and its
    // clamp arm re-stores the value it just wrote. [INFERRED] name.
    bool scrollDownOneRow(System*);

    int m_firstRow, m_rowCount, m_visibleRows;          // +08..10
    BfListBoxData* m_data;                            // +14, borrowed
    float m_rowHeight, m_spacing, m_borderWidth, m_scrollWidth; // +18..24
    float m_field028, m_field02c, m_field030;
    FontHandle m_fonts[5];                           // +34..44
    union { Action* m_selectAction; FontHandle m_font048; }; // +48: both streamed here in retail
    // Actions invoked on release inside the list and on a press elsewhere. [UNRESOLVED] Role names.
    Action* m_field04c;                               // +4C, run by onEvent1 on release
    Action* m_focusAction;                            // +50
    Action* m_field054;                               // +54, run by onEvent1 on a click elsewhere
    float m_field058, m_field05c, m_field060;
    int m_field064, m_sound068, m_sound06c, m_sound070;
    bool m_field074;
    float m_field078;                                  // +78 pointer-hold poll timer
    bool m_field07c;
    int m_field080;
    std::list<int> m_pendingEvents;                   // retail+84, native8-byte list
    float m_width;                                   // retail+90
    bool m_selectable, m_border;
    int m_top, m_bottom, m_left, m_right;              // retail+98..A4
    // The last two are not what their old names said. The console rows name
    // them: +D8..E4 is the scrollbar colour (row 52) and +E8..F4 the scrollbar
    // background colour (row 53), which the constructor leaves untouched.
    Color m_background, m_frame, m_selection;         // retail+A8..D4
    Color m_scrollbar;                               // retail+D8..E4
    Color m_scrollbarBackground;                     // retail+E8..F4, untouched by constructor
    float m_scrollOffset;                            // retail+F8
    float m_field0fc, m_field100, m_field104;           // +FC timer, +100/+104 pointer snapshot
    bool m_field108, m_field109;
    std::int32_t m_field10c[7];                         // +10C begins hovered row/column pair
    bool m_field128;
};
} }
#endif
#endif
