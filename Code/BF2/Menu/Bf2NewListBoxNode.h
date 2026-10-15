// List-box node descriptor and script-facing properties.
#ifndef MENU_BF2NEWLISTBOXNODE_H
#define MENU_BF2NEWLISTBOXNODE_H
#include "BfNewListBoxNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class BfListBoxData;

class Bf2NewListBoxNode : public BfNewListBoxNode {
public:
    explicit Bf2NewListBoxNode(const char* name = nullptr);
    ~Bf2NewListBoxNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                           // 5A5E20, true
    static TypeDescriptor* getClassType();               // 7CDF80, metadata9385A0

    // [CHOSEN, not recovered] Native list-box properties used by the builder.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setDataForHud(BfListBoxData* data);             // 7CDD60, +CC
    BfListBoxData* getDataForHud();
    void bindDataToSelfForHud();                         // 7CDD70, +D4
    // Refuses an index of 5 or more but not a negative one, and answers with
    // the index's low byte rather than a flag - so index 0 reports false.
    bool setFontForHud(FontHandle font, int index);             // 7CDD80, +D8
    bool setSelectActionForHud(Action* action);          // 7CDDA0, +DC -> +48
    bool setFieldE0ForHud(Action* action);               // 7CDDB0, +E0 -> +4C
    bool setFocusActionForHud(Action* action);           // 7CDDC0, +E4 -> +50
    bool setFieldE8ForHud(Action* action);               // 7CDDD0, +E8 -> +54
    Action* getSelectActionForHud();                     // 658420, +EC -> +48
    Action* getFocusActionForHud();                      // 62E4B0, +F0 -> +50
    void setSound070ForHud(int sound);                   // 5C94F0, +FC -> +70
    void setWidthForHud(float width);
    void setSound068ForHud(int sound);
    void setSound06cForHud(int sound);                   // 5C94E0, +108 -> +6C
    void setRowHeightForHud(float height);               // 61B420, +10C -> +18
    void setRowSpacingForHud(float spacing);             // 6CBA10, +110 -> +1C
    void setBorderEnabledForHud(bool enabled);
    void setBorderTopForHud(int value);                  // 5C9560, +11C -> +98
    void setBorderBottomForHud(int value);               // 4B3A90, +120 -> +9C
    void setBorderLeftForHud(int value);                 // 4B3AA0, +124 -> +A0
    void setBorderRightForHud(int value);                // 4B7320, +128 -> +A4
    void setBackgroundRedForHud(float value);            // 4B1C80, +12C -> +A8
    void setBackgroundGreenForHud(float value);          // 4B1C90, +130 -> +AC
    void setBackgroundBlueForHud(float value);           // 4B1910, +134 -> +B0
    void setBackgroundAlphaForHud(float value);          // 4B1920, +138 -> +B4
    void setFrameRedForHud(float value);                 // 4B1930, +13C -> +B8
    void setFrameGreenForHud(float value);               // 4B1CA0, +140 -> +BC
    void setFrameBlueForHud(float value);                // 4B1CB0, +144 -> +C0
    void setFrameAlphaForHud(float value);               // 4B1CC0, +148 -> +C4
    void setSelectionRedForHud(float value);             // 655C80, +14C -> +C8
    void setSelectionGreenForHud(float value);           // 63F810, +150 -> +CC
    void setSelectionBlueForHud(float value);
    void setSelectionAlphaForHud(float value);           // 4B3B20, +158 -> +D4
    void setScrollOffsetForHud(float value);             // 4B19C0, +15C -> +F8
    void setScrollWidthForHud(float value);
    void setScrollbarRedForHud(float value);             // 65C3D0, +164 -> +D8
    void setScrollbarGreenForHud(float value);           // 65C760, +168 -> +DC
    void setScrollbarBlueForHud(float value);            // 65C770, +16C -> +E0
    void setScrollbarAlphaForHud(float value);           // 65C780, +170 -> +E4
    void setScrollbarBackRedForHud(float value);         // 65C3C0, +174 -> +E8
    void setScrollbarBackGreenForHud(float value);       // 7CDE20, +178 -> +EC
    void setScrollbarBackBlueForHud(float value);        // 5ECDD0, +17C -> +F0
    void setScrollbarBackAlphaForHud(float value);       // 4B3B70, +180 -> +F4
    void setField109ForHud(bool value);

protected:
    friend struct Bf2NewListBoxNodeFixture;
    char* m_name;                                        // retail+12C, owned clone
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2NewListBoxNode) == 300, "native Bf2NewListBoxNode storage");
#endif
} }
#endif
#endif
