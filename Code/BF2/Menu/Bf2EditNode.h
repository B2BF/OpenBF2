// Named HUD edit node with an owned name clone. [INFERRED] Header placement.
// Native accessors bridge the incomplete Node virtual interface.
#ifndef MENU_BF2EDITNODE_H
#define MENU_BF2EDITNODE_H
#include "BfEditNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class BfEditData;
class Data;

class Bf2EditNode : public BfEditNode {
public:
    explicit Bf2EditNode(const char* name = nullptr);
    ~Bf2EditNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                           // 5A5E20, true
    static TypeDescriptor* getClassType();               // 7D1000, metadata938DB8

    // [CHOSEN, not recovered] Native edit-node property access.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setEditDataForHud(BfEditData* data);            // 7CDFC0, +C8 -> +08
    bool setStringForHud(Data* string);                  // 7CE250, +CC -> +0C
    // The second argument is the outline flag (+3D), not anything about the
    // font - the console row that drives it is called `setEditNodeFont`.
    bool setFontForHud(FontHandle font, bool outline);          // 7D0F20, +D0 -> +14/+3D
    bool setMaxLengthForHud(int characters);             // 7C21D0, +D4 -> +18
    bool setColorForHud(float r, float g, float b, float a); // 7D0F40, +D8 -> +40..4C

protected:
    friend struct Bf2EditNodeFixture;
    char* m_name;                                        // +50, owned clone
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2EditNode) == 84, "native Bf2EditNode storage");
#endif
} }
#endif
#endif
