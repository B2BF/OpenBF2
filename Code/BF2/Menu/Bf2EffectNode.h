// Named HUD effect node. [INFERRED] Header placement.
#ifndef MENU_BF2EFFECTNODE_H
#define MENU_BF2EFFECTNODE_H
#include "../External/BFMeme/EffectNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2EffectNode : public EffectNode {
public:
    // Defaults [INFERRED] from factory7C2080, which supplies two nulls.
    Bf2EffectNode(const char* name = nullptr, Effect* effect = nullptr);
    ~Bf2EffectNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    static TypeDescriptor* getClassType();               // 7C20D0, metadata935FE0

    // [CHOSEN, not recovered] Returns a reference to the borrowed effect pointer.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    Effect*& getEffectForHud() { return m_effect; }
protected:
    char* m_name; // +0C, owned clone
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2EffectNode) == 16, "native Bf2EffectNode storage");
#endif
} }
#endif
#endif
