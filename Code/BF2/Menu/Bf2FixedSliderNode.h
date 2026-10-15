// Fixed-range slider node descriptor.
#ifndef MENU_BF2FIXEDSLIDERNODE_H
#define MENU_BF2FIXEDSLIDERNODE_H
#include "BfFixedSliderNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;

class Bf2FixedSliderNode : public BfFixedSliderNode
{
public:
    // Defaults from the real factory 7CDC80, which inlines this constructor
    // with a null name and four zeroes. The four floats are not geometry:
    // the console row spells them minimum, maximum, thumb span and step, and
    // `setNodePos`/`setNodeSize` supply the geometry afterwards.
    Bf2FixedSliderNode(const char* name = nullptr, float minimum = 0.0f,
                       float maximum = 0.0f, float thumbSpan = 0.0f,
                       float step = 0.0f);
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                            // 5A5E20, true
    static TypeDescriptor* getClassType();                // 7CDBD0, metadata938218

    // [CHOSEN, not recovered] Assigns the inherited slider data pointer, accepting null.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setDataForHud(Data* data);

protected:
    friend struct Bf2FixedSliderNodeFixture;
    char* m_name;                                         // +58, owned clone, never freed
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2FixedSliderNode) == 92, "retail and native Bf2FixedSliderNode storage");
#endif
} }
#endif
#endif
