// Style interface. Method names, constness, and pointer/reference spellings are [INFERRED].
// Preserve virtual order and scalar argument widths.
#ifndef BFMEME_STYLE_H
#define BFMEME_STYLE_H
#include "Object.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;
struct RenderContext;

// Field names [INFERRED]; five float outputs written by client font7D50F0.
struct GlyphMetrics
{
    float leftBearing, width, rightBearing, height, verticalOffset;
};
// Five-float drawing bounds, read by Style838900 and Quad ctor7732D0.
struct Rect
{
    float x, y, width, height, z;
};

class Style : public Object
{
public:
    Style(const char* name = 0, FontHandle font = 0);
    ~Style() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool streamProperties(System*, Stream*) override;
    virtual float getCharacterWidth(System*, int character);
    virtual float getDataWidth(System*, Data*, int elementIndex);
    virtual float getStringWidth(System*, const char* text);
    virtual void drawText(System*, Data*, int elementIndex, const Rect* bounds,
                          const Rect* clip, RenderContext*);         // +4C 838900
    virtual void prepareFont();
    virtual void positionGlyph(Rect* output, const GlyphMetrics*,
                               float* x, float* y);
    virtual void positionGlyphInt(Rect* output, const GlyphMetrics*,
                                  int* x, int* y);
    static TypeDescriptor* getClassType();         // 838C90, descriptor factory838D40

protected:
    FontHandle m_font;                            // +04, integer handle
    FontHandle m_shadowFont;                      // +08, not written by ctor
    char* m_name;                                 // +0C, owned clone
    float m_scale, m_reciprocalScale;              // +10/+14, not written by ctor
};
} }
#endif
#endif
