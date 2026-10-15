// Client system adapter for the menu runtime. [INFERRED] Header placement.
#ifndef MENU_REF2SYSTEM_H
#define MENU_REF2SYSTEM_H

#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../External/BFMeme/System.h"
#include "../IO/Input/GameInput.h"
#include "../Core/IBase.h"
#include "../GeomMisc/MenuRend/IMemeTexture.h"
#include <map>
#include <string>

namespace dice { namespace hfe { namespace menu {
class Font;                                        // [INFERRED] map payload name

// [INFERRED] concrete name. Embedded table935368 differs from OStream9352A0
// only at write+8C (7D3760); it retains OStream's descriptor and four-byte size.
class Ref2OutputStream : public meme::OStream
{
public:
    int write(const void* buffer, int count) override;
};

class Ref2System : public meme::System
{
public:
    Ref2System(float width, float height);
    ~Ref2System() override;
    meme::TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool init() override;                        // +40 7AD030, real Style/System lifecycle
    bool close() override;                       // +44 7AEF10, separate from destructor
    void setSize(float, float) override;          // +48 5A59F0, real empty override
    bool update(float delta) override;           // +4C 7AD100 ->82A1A0
    bool paint() override;
    meme::OStream* getOutputStream() override;
    meme::IStream* createInputStream(const char*) override; // +5C 7AD510
    meme::OStream* createOutputStream(const char*) override;
    // Font handles are Font*, texture handles IMemeTexture* (pointer-sized meme handles).
    bool drawCharacter(int, meme::FontHandle, meme::Quad*) override;
    bool getGlyphMetrics(int, meme::FontHandle, meme::GlyphMetrics*) override;
    float getKerning(int, int, meme::FontHandle) override;
    float getFontHeight(meme::FontHandle) override; // +7C 7AE8E0
    meme::FontHandle loadFont(const char*) override;
    const char* getFontName(meme::FontHandle) override;
    bool drawTexture(meme::TextureHandle, meme::Quad*) override;
    meme::TextureHandle loadTexture(const char*, bool) override; // +8C 7AD240
    void releaseTexture(meme::TextureHandle) override;
    const char* getTextureName(meme::TextureHandle) override;
    float getTextureWidth(meme::TextureHandle) override;
    float getTextureHeight(meme::TextureHandle) override; // +9C 7AD300
    void beginPaint() override;                   // +B0 7AE6B0
    void endPaint() override;                     // +B4 7AD3B0
    bool preUpdate(float delta) override;         // +B8 7AD8B0
    void sizeChanged(const float* oldSize, const float* newSize) override; // +C0 7AD410
    IBase* getRenderView() { return m_renderView; } // nonvirtual7AD390, complete MOV EAX,[ECX+678];RET
    IBase* getPreviousRenderView() { return m_previousRenderView; } // nonvirtual7AD3A0, complete MOV EAX,[ECX+67C];RET
    void setInput(const io::GameInput& input, int user); // nonvirtual7AD360, complete320-byte copy

private:
    // [INFERRED] nonvirtual helper names, read complete at 0x7AD450/0x7AD6D0.
    void prepareRenderView();
    void prepareViewMatrix();

    std::map<std::string, Font*> m_fonts;           // retail +58, native STL
    geom::IMemeTexture* m_textureList;            // +64; texture link uses +28
    geom::IMemeTexture* m_blankTexture;           // +68, untouched by ctor
    Ref2OutputStream m_output;                    // +6C, an embedded object
    io::GameInput m_inputs[4];                    // +70, each 0x140
    float m_inputStates[4][16];                   // +570, untouched by ctor; [INFERRED] name
    float m_renderWidth, m_renderHeight;          // +670/+674
    IBase* m_renderView;                          // +678, untouched by ctor
    IBase* m_previousRenderView;                  // +67C, borrowed
    IBase* m_renderer;                            // +680, singleton, borrowed
    IBase* m_field684;                            // released only by destructor
    unsigned char m_textureFlags;                // +688
    std::uint32_t m_field68c;
};

} } }
#endif
#endif
