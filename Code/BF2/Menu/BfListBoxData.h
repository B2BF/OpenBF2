// [INFERRED] path beside the game-specific Menu owner. Class names are
// descriptor-attested at PE9365C4/9365E0.
#ifndef MENU_BFLISTBOXDATA_H
#define MENU_BFLISTBOXDATA_H
#include "../External/BFMeme/Data.h"
#if !DEDICATED
#include "../External/BFMeme/Quad.h"
#include "../Core/IBase.h"
#include <cstdint>
#include <string>
#include <vector>
namespace dice { namespace hfe { namespace hud { class BF2HudManager; class HudChat; class HudVoteMap; class HudCommander; class HudSquad; class HudScoreboard; class HudVoip; } } }
namespace dice { namespace meme {
class Action;
class BfNewListBoxNode;
struct Rect;
struct NodeState;

// Complete32-slot base936388. Names and pointer/reference spelling inferred.
class ListBoxData : public Data
{
public:
    explicit ListBoxData(const char* name = nullptr); // inlined7C3250
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool onCreate(System*) override;                  // 5A1860, true
    int getCharacterElement(System*, int, int) override; // 68C4A0, zero
    int getElementCount(System*) override;            // 4553E0, zero
    virtual void setRow(int);
    virtual void setColumn(int);
    virtual bool activateSelection(System*);
    virtual bool updateSelection(System*, const Rect*, const NodeState*);
    static TypeDescriptor* getClassType();
protected:
    // PE79431E reads the selected column directly while handling the
    // CommanderMenu single-click action. The original owner therefore has
    // source-level access to this base member too.
    friend class dice::hfe::hud::HudCommander;
    // PE 7A45F7 (HudScoreboard::singleClick, the Scoreboard.singleClick row) reads the clicked
    // column of the friendly or enemy list the same way.
    friend class dice::hfe::hud::HudScoreboard;
    // So do the SquadMenu and LevelsList click targets: HudSquad::singleClickNew
    // (791D46 / 791DF2 / 791E29, the member list) and singleClickInvite (791F5F, the invite list),
    // and HudVoteMap::singleClick (785742, the map list).
    friend class dice::hfe::hud::HudSquad;
    friend class dice::hfe::hud::HudVoteMap;
    // BfNewListBoxNode::updateSelf reads the selected row directly before
    // deciding whether to dispatch the typed selectRow callback.
    friend class BfNewListBoxNode;
    int m_row;                                       // +08
    int m_column;                                    // +0C, untouched by constructor
};

// [INFERRED] plain cell/column names. Native STL, never packed retail strings.
struct ListBoxCell
{
    ListBoxCell(std::wstring, float, float, float, float, int);
    // 7C2B70, the narrow twin of the wide constructor above: it assigns `narrow`
    // and leaves `isWide` false. HudVoip's row painter is its only caller.
    ListBoxCell(std::string, float, float, float, float, int);
    // 7C2BF0 / 7C2C80 the positioned twins of the two constructors above.
    // Same object, same defaults, except that imageX/imageY carry the caller's offset instead of
    // being zeroed - so a plain text cell can be nudged without becoming an image cell (isImage
    // stays false and texture stays 0). imageWidth/imageHeight are still zeroed.
    ListBoxCell(std::string, float x, float y, float r, float g, float b, float a, int);
    ListBoxCell(std::wstring, float x, float y, float r, float g, float b, float a, int);
    ListBoxCell(TextureHandle texture, float width, float height, float x, float y,
        float r, float g, float b, float a);
    std::string narrow;                              // retail+00
    std::wstring wide;                               // +1C
    bool isWide;                                     // +38
    Color color;                                     // +3C
    TextureHandle texture;                           // +4C
    float imageWidth, imageHeight, imageX, imageY;    // +50..5C
    bool isImage;                                    // +60
    int field064, field068;                          // +64/+68
    // Highlight state populated by setRowHighlight.
    bool field06c;                                   // +6C, setRowHighlight's flag
    Color highlight;                                 // +70..+7C, setRowHighlight's r,g,b,a
    float highlightField080;                         // +80, its last argument
    float highlightField084;                         // +84, its sixth (float add in paint)
    float highlightField088;                         // +88, its seventh (FLD in paint)
    int resourceIndex, field090;                     // +8C/+90
};
struct ListBoxColumn
{
    ListBoxColumn(int index, int width, bool, bool);   // inlined7C5850
    int index;
    std::vector<ListBoxCell*> cells;
    int width;
    bool enabled, field019, field01a;
    int field01c, field020;
};

// Complete34-slot declaration936880. The visual node remains unpublished;
// this data object's own table, constructor and descriptor are complete.
class BfListBoxData : public ListBoxData
{
public:
    explicit BfListBoxData(const char* name = nullptr);
    ~BfListBoxData() override;                        // leaks columns/cells
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    int getCharacterElement(System*, int, int) override;
    int getElementCount(System*) override;
    bool activateSelection(System*) override;
    bool updateSelection(System*, const Rect*, const NodeState*) override;
    virtual void selectRow(int row, bool notify);
    // Not a virtual: the HUD builder's setListNodeOutline row (796970)
    // calls it directly on the data the node carries. [INFERRED] name from that
    // row; the body is one byte store into +20.
    void setOutlineForHud(bool outline);
    virtual int getFirstVisibleRow();
    // [INFERRED] Selects the active column by unsigned index, or clears it when absent.
    void selectActiveColumnForNode(unsigned int index);
    // Requests the first visible row and posts an update to the attached node. Does nothing without a node.
    void setFirstVisibleRow(int row);
    static TypeDescriptor* getClassType();

    bool addColumn(int width, bool, bool);
    bool addIconColumn(int width, int height, bool);   // 7C5900, full-width height and byte flag
    bool addHiddenColumn();                          // 7C59B0, width0 and disabled
    std::string getSelectedString(int column);       // 7C48D0, current row
    bool appendWideCell(int column, std::wstring, float, float, float, float, int);
    // 7C8090, the narrow twin of appendWideCell. Like its wide twin it returns
    // true unconditionally - a column index past the end, a null column and a
    // failed allocation are all silent - and a failed allocation still appends a
    // null cell to the column.
    bool appendCell(int column, std::string, float, float, float, float, int);
    // 7C81C0 and 7C8300 are pure forwarders: they re-push every argument (the
    // string re-copied) and call the two above. Kept as separate entries because
    // HudVoip's row painter calls these addresses, not the targets.
    bool appendCell0x7c81c0(int column, std::string, float, float, float, float, int);
    // Appends a positioned cell to a valid column. Returns true even if no cell is added.
    // A failed allocation may append null, matching the unpositioned variants.
    bool appendPositionedCell(int column, std::string, float x, float y,
        float r, float g, float b, float a, int);
    bool appendPositionedWideCell(int column, std::wstring, float x, float y,
        float r, float g, float b, float a, int);
    // pure forwarders to the two above, every argument re-pushed and the string
    // re-copied - the same relationship 7C81C0 / 7C8300 have to appendCell / appendImageCell.
    // HudScoreboard::updateFinalList calls these addresses, not the targets.
    bool appendPositionedCell0x7c8210(int column, std::string, float x, float y,
        float r, float g, float b, float a, int);
    bool appendPositionedWideCell0x7c8470(int column, std::wstring, float x, float y,
        float r, float g, float b, float a, int);
    bool appendImageCell(int column, TextureHandle texture, float, float, float, float,
        float, float, float, float);
    bool appendImageCell0x7c8300(int column, TextureHandle texture, float, float, float, float,
        float, float, float, float);
    bool setImageCell(int column, int row, TextureHandle texture, float, float, float, float,
        float, float, float, float, int extra);
    bool setWideCell(int column, int row, std::wstring, float, float, float, float, int, int);
    bool setWideCell0x7c87f0(int, int, std::wstring, float, float, float, float, int, int);
    // Sets a positioned cell from narrow text; the wide-text overload has the same behavior.
    bool setPositionedCell(int column, int row, std::string, float x, float y,
        float r, float g, float b, float a, int, int);
    bool setPositionedCell0x7c8640(int column, int row, std::string, float x, float y,
        float r, float g, float b, float a, int, int);
    std::wstring getWideCell(int column, int row);
    int getRowCount();                                // no first-column guard
    // Sets row background across all columns and returns true even when row is absent.
    // [UNRESOLVED] Meaning of the final three styling parameters.
    bool setRowHighlight(int row, float r, float g, float b, float a,
        float, float, float);
    bool setRowHighlight0x7c4ba0(int row, float r, float g, float b, float a,
        float, float, float);
    bool clearRows();                                 // 7C89B0, preserves columns
    bool clearCell(int column, int row);               // 7C4C40, named resource+20 seam
    bool clearRow(int row);
    bool copyCell(int column, int row, const ListBoxCell*);
    bool copyRow(int from, int to);
    bool moveRow(int from, int to);
    int getColumnRightEdge(int column);
    int getAvailableWidth(int alignment);             // 7C4EB0, active column
    int getAvailableWidth(int column, int alignment);
    int measureText(System*, std::wstring, int column, int row, int* fontMode);
    bool insertCell(int column, int row, std::string, float, float, float, float);
    bool insertWideCell(int column, int row, std::wstring, float, float, float, float);
private:
    friend class dice::hfe::hud::BF2HudManager;
    friend class dice::hfe::hud::HudChat;
    friend class dice::hfe::hud::HudVoteMap;
    friend class dice::hfe::hud::HudCommander;
    friend class dice::hfe::hud::HudSquad;
    friend class dice::hfe::hud::HudScoreboard;
    friend class dice::hfe::hud::HudVoip;
    friend class BfNewListBoxNode;
    // 7CDD70 stores the node's back-pointer into +10 directly, so the node had
    // access in the original source.
    friend class Bf2NewListBoxNode;
    BfNewListBoxNode* m_node;                         // +10
    Action* m_createAction;                           // +14
    Data* m_createString;                             // +18
    Data* m_profileName;                              // +1C
    // +20 is the outline flag - console row 55 setListNodeOutline writes it.
    bool m_outline, m_field021, m_field022, m_field023;
    float m_field024;
    int m_field028;                                    // +28, signed pending first row
    int m_visibleRows;                                // +2C
    float m_field030;
    int m_field034, m_field038, m_field03c;
    bool m_field040;
    int m_field044;
    dice::hfe::IBase* m_resource;                      // +48, release+08 known
    std::uint32_t m_field04c;
    std::vector<ListBoxColumn*> m_columns;             // retail+50, native vector
    int m_columnCount;                                // retail+60
    ListBoxColumn* m_activeColumn;                    // retail+64
};
} }
#endif
#endif
