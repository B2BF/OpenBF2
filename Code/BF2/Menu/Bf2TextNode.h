// Named HUD text node. [INFERRED] Header placement. Native accessors bridge the incomplete Node interface.
#ifndef MENU_BF2TEXTNODE_H
#define MENU_BF2TEXTNODE_H
#include "../External/BFMeme/TextNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2TextNode : public BfTextNode {
public:
    // Default [INFERRED] from factory7C2560, which supplies a null name.
    explicit Bf2TextNode(const char* name = nullptr);
    ~Bf2TextNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;
    static TypeDescriptor* getClassType();                // 7C25C0, metadata93619C

    // [CHOSEN, not recovered] Native text-node properties used by the builder.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setStringDataForHud(Data* data);                 // 7CE250, +C8
    bool setStyleForHud(Style* style);                    // 7CDFC0, +CC
    bool setTypeDataForHud(Data* type);
    Data* getStringDataForHud();                          // 68AD50, +D4
    Style* getStyleForHud();                              // 68E7B0, +D8
protected:
    char* m_name; // +18, owned clone
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2TextNode) == 28, "native Bf2TextNode storage");
#endif
} }
#endif
#endif
