// Visual effect interface. Method names and source constness are [INFERRED]; preserve virtual order and argument widths.
#ifndef BFMEME_EFFECT_H
#define BFMEME_EFFECT_H
#include "Object.h"
#if !DEDICATED
namespace dice { namespace meme {
struct Color;
struct Quad;
struct RenderContext;

class Effect : public Object
{
public:
    ~Effect() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    // Bf2ColorEffect7C2190 constructs a four-float color from these scalar
    // arguments; 7D1450 saves/restores the nullable +44 color with this slot.
    virtual bool setColor(float r, float g, float b, float a);
    virtual Color* getColor();
    virtual bool update(System*, float delta);
    virtual bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                               Quad*, RenderContext*); // +4C 82D810
    virtual bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*);
    static TypeDescriptor* getClassType();            // 82D860, factory82D8A0
};

// Named free-function spellings are [INFERRED]. Retail82D730/82D790 use ECX
// for context; all callers here are rebuilt together using these typed helpers.
bool drawContextCharacter(RenderContext*, System*, int character, FontHandle font,
                          int elementIndex, Quad*);
bool drawContextTexture(RenderContext*, System*, TextureHandle texture, Quad*);
bool updateEffect(System*, Effect*, float delta);
TypeDescriptor* getEffectClassType();                 // 82D860, Stream adapter
} }
#endif
#endif
