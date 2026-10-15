// Editable HUD node with descriptor registration and event handling. [INFERRED] Source placement.
#ifndef BF2_MENU_BFEDITNODE_H
#define BF2_MENU_BFEDITNODE_H
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/Quad.h"
#if !DEDICATED
namespace dice { namespace meme {
class BfEditData;
class Data;
class Action;
class BfEditNode : public Node
{
    friend class BfEditData;
public:
    BfEditNode();                                      // 7AAE90; no owned resources
    TypeDescriptor* getType() override;               // 7AC340 [FRONTIER] Node suffix
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override; // 7AAEF0 [FRONTIER]
    bool onEvent1(System*, const Rect*, const NodeState*, int first, int index, int second) override; // 7AB5F0 [FRONTIER]
    bool onEvent4(System*, const Rect*, const NodeState*, int first, int index, int second) override;
    void updateVisibleStart(System*, const Rect*);
    static TypeDescriptor* getClassType();             // 7AC340, metadata934DA0
protected:
    friend class Bf2EditNode;
    BfEditData* m_editData;                            // +08, borrowed
    Data* m_string;                                   // +0C, borrowed
    Action* m_selectAction;                           // +10, borrowed
    FontHandle m_font;                                // +14, opaque font handle
    int m_maxCharacters;                              // +18
    int m_firstVisible, m_cursor;                     // +1C,+20
    float m_blinkTime;                                // +24
    bool m_focused;                                   // +28; following alignment bytes untouched
    int m_pendingKey, m_pendingCharacter;             // +2C,+30; untouched by constructor
    int m_pendingInput;                               // +34
    bool m_ignoreClick, m_flag39, m_slowBlink, m_skipCharacter; // +38..3B
    bool m_limitWidth, m_outline;                     // +3C,+3D
    Color m_color;                                    // +40; initialized blue/opaque
};
} }
#endif
#endif
