// HUD color effect with typed property operations. [INFERRED] Header placement.
#ifndef MENU_BF2COLOREFFECT_H
#define MENU_BF2COLOREFFECT_H
#include "../External/BFMeme/ColorEffect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2ColorEffect : public ColorEffect {
public:
    // Defaults [INFERRED] from factory7C23B0, which supplies four ones.
    Bf2ColorEffect(float red = 1.0f, float green = 1.0f,
                   float blue = 1.0f, float alpha = 1.0f);
    ~Bf2ColorEffect() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool setColor(float r, float g, float b, float a) override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override;
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    // Names [INFERRED]; the three slots this class adds above Effect's21.
    virtual bool setOwnerNode(void* owner);
    virtual bool setAlphaBinding(const float* alpha);
    virtual bool setRgbBindings(const float* red, const float* green,
                                const float* blue);        // +5C 7C2380
    static TypeDescriptor* getClassType();                 // 7C2440, metadata936080
protected:
    // +14 is not written by the constructor; only slot +54 ever sets it.
    void* m_owner;                    // +14, uninitialised on construction
    const float* m_alphaBinding;      // +18, multiplies the alpha when >= 0
    const float* m_redBinding;        // +1C, replaces the channel when >= 0
    const float* m_greenBinding;      // +20
    const float* m_blueBinding;       // +24
    int m_field28;                    // +28, cleared by the constructor, else unread
    float m_disabledBinding;          // +2C, -1 so every default binding is inactive
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2ColorEffect) == 48, "native Bf2ColorEffect storage");
#endif
} }
#endif
#endif
