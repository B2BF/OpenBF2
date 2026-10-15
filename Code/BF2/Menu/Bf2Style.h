// Script-visible styles for HUD nodes.
#ifndef MENU_BF2STYLE_H
#define MENU_BF2STYLE_H
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/Quad.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2Style : public Style {
public:
    // Defaults [INFERRED] from factory7C1530, which supplies a null name and 0.
    Bf2Style(const char* name = nullptr, FontHandle font = 0);
    ~Bf2Style() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    void drawText(System*, Data*, int elementIndex, const Rect* bounds,
                  const Rect* clip, RenderContext*) override;
    // Names [INFERRED]; the three slots this class adds above Style's 23.
    virtual bool setShadowFont(FontHandle font);                // +5C 7C1490
    virtual bool setShadowOffset(float x, float y);
    virtual bool setTexture(TextureHandle texture);
    static TypeDescriptor* getClassType();               // 7C1B20, metadata935E90
protected:
    // [INFERRED] spelling for the shadow and highlight passes every drawText
    // body performs once per glyph; retail inlines the identical code at all
    // three sites. Nonvirtual, so the published table is unchanged.
    bool paintGlyph(System*, int character, int position, Quad& glyphQuad,
                    RenderContext*, bool highlight);
    bool m_wordWrap;                // +18, true from the constructor
    unsigned char m_padding19[3];   // untouched
    FontHandle m_shadowFont;        // +1C, zero means reuse the ordinary font
    TextureHandle m_texture;        // +20, copied into every glyph quad when set
    float m_shadowOffsetX;          // +24
    float m_shadowOffsetY;          // +28
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2Style) == 44, "native Bf2Style storage");
#endif

// 7D1CE0 / table939250. Walks the string backwards from its last character,
// decreasing the pen, so the text ends at the right edge of the bounds.
class Bf2RightAlignedStyle : public Bf2Style {
public:
    Bf2RightAlignedStyle(const char* name = nullptr, FontHandle font = 0);
    ~Bf2RightAlignedStyle() override;                    // 7C26A0, shared body
    TypeDescriptor* getType() override;
    void drawText(System*, Data*, int elementIndex, const Rect* bounds,
                  const Rect* clip, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7D21B0, metadata9392C0
};
// 7C2600 / table936258. Measures each line first and starts the pen at the
// centre of the bounds.
class Bf2CenterAlignedStyle : public Bf2Style {
public:
    Bf2CenterAlignedStyle(const char* name = nullptr, FontHandle font = 0);
    ~Bf2CenterAlignedStyle() override;                   // 7C26A0, shared body
    TypeDescriptor* getType() override;
    void drawText(System*, Data*, int elementIndex, const Rect* bounds,
                  const Rect* clip, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7C2B10, metadata9362C8
};
} }
#endif
#endif
