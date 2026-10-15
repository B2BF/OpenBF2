// Shows a vehicle's displacement from its hover position.
#ifndef MENU_BF2HOVERNODE_H
#define MENU_BF2HOVERNODE_H
#include "../External/BFMeme/Node.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme {
class Bf2HoverNode : public Node {
public:
    // The System argument is accepted and ignored - the constructor stores it
    // nowhere and reaches the information layer through the global instead.
    // Defaults [INFERRED] from factory7CB540, which supplies two nulls.
    Bf2HoverNode(System* system = nullptr, const char* name = nullptr);
    ~Bf2HoverNode() override;                            // 7CB330, frees the name
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                           // 5A5E20, true
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    static TypeDescriptor* getClassType();               // 7CB560, metadata9379B0

    // [CHOSEN, not recovered] Native properties for builder-created hover nodes.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setMaxValueForHud(float value);                  // 7CE240, +C8
    // crossed: the first argument lands at +18 and the second at +14.
    bool setWidthLengthForHud(float width, float length); // 7CB180, +CC
    bool setMiddlePositionForHud(int x, int y);
    // The create row's four geometry integers: x, y, then w, h. All four are fild'd
    // into +4C/+50/+44/+48, and the raw width/height integers are kept at +3C/+38.
    bool setBoundsForHud(int x, int y, int width, int height); // 7CB1C0, +D4

protected:
    friend struct Bf2HoverNodeFixture;
    char* m_name;                        // +08, owned clone
    // +0C is a float (7CB370: `fadd`/`fstp dword [esi+0Ch]`): the frame delta summed while
    // the indicator is shown, reset to 0 when it hides. Nothing in this image reads it.
    // Name [INFERRED]; zeroed by the constructor.
    float m_shownTime;                   // +0C
    float m_maxValue;                    // +10, starts 0.5
    float m_length;                      // +14, the second argument of +CC
    float m_width;                       // +18, the first argument of +CC
    // +1C/+20 are floats: the vehicle's absolute X and Z captured the frame the indicator
    // appears (7CB3F7..7CB40D). Names [INFERRED]; zeroed by the constructor.
    float m_anchorX;                     // +1C
    float m_anchorZ;                     // +20
    std::uint32_t m_field024;            // +24 [UNRESOLVED], never written
    // The three values this node publishes for other HUD elements to read.
    int m_xPos;                          // +28, registered "<name>XPos"
    int m_yPos;                          // +2C, registered "<name>YPos"
    int m_middleX, m_middleY;            // +30/+34, the two ints of +D0
    // +38/+3C: the RAW integer height and width that +D4 receives (7CB1E1/7CB1E4).
    // The pair runs in the opposite order to its float copy: height at +38, width at
    // +3C, where the floats keep width at +44 and height at +48. 7CB370 divides them
    // by +14 and +18 to scale world metres into HUD units. Names [INFERRED].
    int m_boundsHeightInt;               // +38, zeroed
    int m_boundsWidthInt;                // +3C, zeroed
    bool m_show;                         // +40, registered "<name>Show"
    float m_boundsWidth, m_boundsHeight; // +44/+48, arguments 3 and 4 of +D4
    float m_boundsX, m_boundsY;          // +4C/+50, arguments 1 and 2 of +D4
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2HoverNode) == 84, "native Bf2HoverNode storage");
#endif
} }
#endif
#endif
