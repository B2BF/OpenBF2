// Transform-list node descriptor.
#ifndef MENU_BF2TRANSFORMLISTNODE_H
#define MENU_BF2TRANSFORMLISTNODE_H
#include "../External/BFMeme/Node.h"
#if !DEDICATED
#include <cstdint>
#include <list>
namespace dice { namespace meme {
class Bf2TransformNode;

class Bf2TransformListNode : public Node {
public:
    // Defaults from the real factory 7D1CB0, which pushes four zeros. The two
    // middle arguments are floats: the console row FILDs its x and y before the
    // call, and the fourth is the row's plain integer count.
    Bf2TransformListNode(const char* name = nullptr, float x = 0.0f, float y = 0.0f,
                         int count = 0);
    ~Bf2TransformListNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                            // 5A5E20, true
    bool update(System*, const NodeState*, const Rect*, float) override;
    static TypeDescriptor* getClassType();                // 7D18B0, metadata9390B0

    // [CHOSEN, not recovered] Native transform-list property access.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setPositionForHud(float x, float y);             // 7D1840, +C8 -> +38/+3C
    bool setOffsetForHud(int x, int y);                   // 7D1BE0, +CC -> +28/+2C
    // Two bindings, not an array: index 0 lands at +1C and every other index -
    // including a negative one - lands at +20. A null binding is refused.
    bool setPositionBindingForHud(int index, int* binding);
    // 7D1C60, +D4. A null child is refused; the list borrows what it is given -
    // the destructor frees the name and the links, never the children.
    bool addChildForHud(Bf2TransformNode* child);

protected:
    friend struct Bf2TransformListNodeFixture;
    std::list<Bf2TransformNode*> m_children;              // retail+08..13, native 8
    char* m_name;                                         // +14, owned clone
    std::uint32_t m_field018;                             // +18, constructor0
    int* m_positionBinding0;                              // +1C, index 0
    int* m_positionBinding1;                              // +20, every other index
    int m_count;                                          // +24, the fourth argument
    int m_offsetX, m_offsetY;                             // +28/+2C
    float m_field030;                                     // +30, horizontal direction, constructor0
    float m_field034;                                     // +34, constructor 1.0
    float m_positionX, m_positionY;                       // +38/+3C
};
static_assert(sizeof(Bf2TransformListNode) == 60, "native Bf2TransformListNode storage");
} }
#endif
#endif
