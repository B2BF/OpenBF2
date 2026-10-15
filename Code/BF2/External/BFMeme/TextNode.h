// Text node with inherited Node ordering and five specialized virtual handlers.
#ifndef BFMEME_TEXTNODE_H
#define BFMEME_TEXTNODE_H
#include "Node.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;
class Style;
class ColorEffect;

class BfTextNode : public Node
{
public:
    BfTextNode();                                          // 7D3AC0, factory7D3B50
    ~BfTextNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    void paintSelf(System*, const NodeState*, const Rect* bounds,
                   const Rect* clip, RenderContext*) override;
    static TypeDescriptor* getClassType();                 // 7D3B70, metadata939B48
protected:
    // Member names [INFERRED] from the three stream property labels and paintSelf.
    Style* m_style;        // +08, streamed as "Style", borrowed
    Data* m_string;        // +0C, streamed as "String", borrowed
    Data* m_type;          // +10, streamed as "Type", borrowed; gates the paint
    ColorEffect* m_shadow; // +14, owned; the constructor allocates a black one
};
#if defined(_M_IX86)
static_assert(sizeof(BfTextNode) == 24, "native BfTextNode storage");
#endif

// A free __fastcall helper, not a member: the System and the Style sit
// in ECX/EDX and the rest on the stack. Name [INFERRED].
void drawStyledText(System*, Style*, const Rect* bounds, const Rect* clip,
                    RenderContext*, Data*, int elementIndex);
} }
#endif
#endif
