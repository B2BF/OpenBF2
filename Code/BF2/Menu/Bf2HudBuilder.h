// Creates and configures HUD node trees. [INFERRED] Header placement and field names.
#ifndef MENU_BF2HUDBUILDER_H
#define MENU_BF2HUDBUILDER_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <cstdint>
#include <string>
namespace dice { namespace meme { class System; class Object; class Node; class Bf2MenuMapNode; } }
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;

class Bf2HudBuilder
{
public:
    Bf2HudBuilder();
    ~Bf2HudBuilder();                               // 794E90, nonvirtual
    virtual void setSystem(meme::System*);
    bool createSplitNode(std::string parent, std::string name);
    bool createMapNode(std::string parent, std::string name);   // 796530, stores the map at +08
    bool createPictureNode(std::string parent, std::string name,
                           int x, int y, int width, int height);
    bool setPictureNodeTexture(std::string texture);
    bool setPictureNodeVariableTexture(std::string variable);
    bool setPictureNodeRotateVariable(std::string variable);
    bool setPictureNodeRotation(int degrees);
    bool setPictureNodeCenterPoint(int x, int y);
    bool setPictureNodeBorder(int top, int bottom, int left, int right);
    bool setPictureNodeBorderColor(float r, float g, float b, float a);
    bool createBarNode(std::string parent, std::string name, int direction,
                       int x, int y, int width, int height);
    bool setBarNodeTexture(int index, std::string texture);
    bool setBarNodeVariableTexture(int index, std::string variable);
    bool setBarNodeBorder(int top, int bottom, int left, int right);
    bool setBarNodeSnap(int snap);
    bool setBarNodeSnapDir(bool towardsEnd);
    bool setBarNodeValueVariable(std::string variable);
    bool createCompassNode(std::string parent, std::string name, int direction,
                           int x, int y, int width, int height,
                           bool snap, bool wrap);
    bool setCompassNodeTexture(int index, std::string texture);
    bool setCompassNodeBorder(int top, int bottom, int left, int right);
    bool setCompassNodeSnapOffset(int a, int b, int c, int d);
    bool setCompassNodeSnapTexture(bool primary, std::string texture);
    bool setCompassNodeValueVariable(std::string variable);
    bool setCompassNodeOffset(int offset);
    bool setCompassNodeTextureSize(int width, int height);
    bool createTextNode(std::string parent, std::string name,
                        int x, int y, int width, int height);
    bool setTextNodeString(std::string text);
    bool setTextNodeStyle(std::string font, int alignment);
    bool setTextNodeOutLineOffset(float x, float y);
    bool setTextNodeStringVariable(std::string variable);
    bool setNodeShowVariable(std::string name);
    bool setNodeLogicShowVariable(std::string operation, std::string variable, int value);
    bool setNodeColor(float r, float g, float b, float a);
    bool setNodeInTime(float seconds);
    bool setNodeOutTime(float seconds);
    bool addNodeAlphaShowEffect();
    bool addNodeBlendEffect(int source, int destination);
    bool addNodeMoveShowEffect(float direction, int length);
    bool setNodeAlphaVariable(std::string variable);
    bool setNodeRGBVariables(std::string red, std::string green,
                             std::string blue);
    bool setNodePosVariable(int index, std::string variable);
    bool setNodePos(int x, int y);
    bool setNodeOffset(int x, int y);
    bool setNodeSize(int width, int height);
    bool newLayer();
    bool createHoverNode(std::string parent, std::string name,
                        int x, int y, int width, int height);
    bool setHoverInMiddlePos(int x, int y);
    bool setHoverMaxValue(float value);
    bool setHoverWidthLength(float width, float length);
    // Family M, the six button rows the stock scripts call. Rows 119
    // `setButtonNodeMouseOverColor` and 122 `setButtonNodeFunction` exist in the
    // table but no shipped script reaches them, so they are not written here.
    bool createButtonNode(std::string parent, std::string name,
                         int x, int y, int width, int height);
    bool setButtonNodeTexture(int index, std::string texture);
    bool setButtonNodeMouseArea(int x, int y, int width, int height);
    bool setButtonNodeConCmd(std::string command, int index);
    bool setButtonNodeAltConCmd(std::string command, int index);
    bool setButtonNodeDebug(bool enabled);
    // Family N, the thirteen list-box rows.
    bool createListNode(std::string parent, std::string name, int x, int y,
                       int width, int height, int rowHeight, bool selectable);
    bool setListNodeData(int selector);
    bool setListNodeConCmd(int which, std::string command);
    bool setListNodeBackgroundColor(float r, float g, float b, float a);
    bool setListNodeSelectColor(float r, float g, float b, float a);
    bool setListNodeBorderColor(float r, float g, float b, float a);
    bool setListNodeBorder(int top, int bottom, int left, int right);
    bool setListNodeScrollbar(int width, int offset);
    bool setListNodeRowSpacing(int spacing);
    bool setListNodeScrollbarColor(float r, float g, float b, float a);
    bool setListNodeScrollbarBackgroundColor(float r, float g, float b, float a);
    bool setListNodeFont(std::string font, int index);
    bool setListNodeOutline(bool enabled);
    // Family O, the six edit-box rows.
    bool createEditNode(std::string parent, std::string name,
                       int x, int y, int width, int height);
    bool setEditNodeData(int selector);
    bool setEditNodeString(int selector);
    bool setEditNodeMaxLength(int characters);
    bool setEditNodeColor(float r, float g, float b, float a);
    bool setEditNodeFont(std::string font, bool outline);
    // Family P, the single mini-map row.
    bool createMiniMapNode(std::string parent, std::string name,
                          int x, int y, int width, int height);
    // Family Q, the four transform-list rows.
    bool createTransformListNode(std::string parent, std::string name,
                                int x, int y, int width, int height,
                                int count);
    bool addTransformListNode(std::string name);
    bool setTranformListNodePosVariable(int index, std::string variable);
    bool setTranformListNodeOffset(int x, int y);
    // Family G, the three slider rows. The create row's four floats are the
    // slider's range, not geometry: minimum, maximum, thumb span and step. The
    // stock script follows them with setNodePos/setNodeSize.
    bool createSliderNode(std::string parent, std::string name, float minimum,
                         float maximum, float thumbSpan, float step);
    bool setSliderNodeData(std::string variable);
    bool setSliderNodeChild(std::string name);
    bool createObjectSelectionNode(std::string parent, std::string name,
                                  int x, int y, int width, int height);
    bool setObjectSelectionNodePointerSize(int width, int height);
    bool createOccupiedNode(std::string parent, std::string name, int seat,
                           int x, int y, int width, int height);
    bool setOccupiedNodeData(int selector);
    bool setOccupiedNodePosVariable(int index, std::string variable);
    bool createTransformNode(std::string parent, std::string name,
                            int x, int y, int width, int height);
    bool createObjectMarkerNode(std::string parent, std::string name,
                               int x, int y, int width, int height);
    bool setObjectMarkerNodeTexture(int index, std::string texture);
    bool setObjectMarkerNodeTextureSize(int index, int width, int height);
    bool setObjectMarkerNodeObjects(int objects);
    bool setObjectMarkerNodeLockText(bool enabled, std::string prefix);
    bool setObjectMarkerNodeLockTextOffset(int x, int y);
    bool addObjectMarkerNodeLockTextNode(std::string name);
    bool setObjectMarkerNodeLockOnType(int type);
    bool setObjectMarkerNodeWeapon(int weapon);
    // Selects the active builder object by parent and node name.
    bool setActiveObject(std::string parent, std::string name);

private:
    // Helper names [INFERRED]; by-value strings and typed child operations are
    // evidenced by the full client PE bodies, without a dedicated ELF counterpart.
    meme::Node* findNodePredecessor(std::string parent, std::string name);
    meme::Node* findLastNode(std::string parent);
    meme::Node* findNode(std::string name);
    bool insertNode(std::string parent, meme::Node* node, int mode);
    bool registerNode(std::string parent, meme::Node* node, meme::Object* data, int mode);
    friend class BF2HudManager;
    friend class HudConsoleBinding; // PE75F0F0 availability checks the native map handle
    meme::System* m_system;                         // retail+04
    // HUD initialization identifies the first command handle as its map node.
    meme::Bf2MenuMapNode* m_field008;                // 7583E1 -> HUD's map field
    meme::Object* m_field00c;                      // insertion cursor, Object cast794ED0
    meme::Object* m_field010;                      // alternate insertion cursor
    meme::Object* m_field014;                      // selected registry object79552E
    std::string m_field018, m_field034, m_field050, m_field06c;
    bool m_field088;
    // Not initialized by794E30. Native alignment supplies the three untouched
    // bytes after the flag; no packed retail STL. Retail +8C is not an
    // unresolved payload word: `setButtonNodeTexture` (798D20) parks the handle
    // it just loaded there on its way to the node, and nothing ever reads it
    // back. The remaining three are still [UNRESOLVED].
    // The two handle words are pointer-sized; uintptr_t is uint32_t on x86.
    std::uintptr_t m_buttonTexture;                 // retail+8C
    std::uintptr_t m_field090;                      // retail+90, last loaded text-node font
    std::uint32_t m_field094, m_field098;
};
} } }
#endif
#endif
