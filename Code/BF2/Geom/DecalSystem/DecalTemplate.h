// Renderer-side decal appearance and animation settings, owned by DecalManager.
// [INFERRED] header placement and class name. Texture references release on replacement/destruction.
// The two HostString fields are std::string, so the native offsets after +0x3C differ from retail's;
// callers must use typed accessors. The Vec2 at +0x2C is [UNRESOLVED], zeroed and unread.

#ifndef BF2_GEOM_DECALSYSTEM_DECALTEMPLATE_H
#define BF2_GEOM_DECALSYSTEM_DECALTEMPLATE_H

#ifdef BF2_IMAGE_RENDDX9

#include "IDecalTemplate.h"

#include <stddef.h>

namespace dice {
namespace hfe {

namespace dx9rend {
struct EngineTexture;
struct TextureAtlasEntry;
}

namespace geom {

class DecalTemplate : public IDecalTemplate
{
public:
    explicit DecalTemplate(const DecalTemplateString& name);
    virtual ~DecalTemplate();

    // IDecalTemplate, table 0x10221928
    virtual const DecalTemplateString& getName() const override;             // +0x00
    virtual void  refresh() override;                                        // +0x04
    virtual void  setSize(float size) override;                              // +0x08
    virtual float getSize() const override;                                  // +0x0C
    virtual void  setRandomSize(float randomSize) override;                  // +0x10
    virtual float getRandomSize() const override;                            // +0x14
    virtual void  setRandomColorIntensity(float intensity) override;         // +0x18
    virtual float getRandomColorIntensity() const override;                  // +0x1C
    virtual void  setColor(const Vec3& color) override;                      // +0x20
    virtual Vec3  getColor() const override;                                 // +0x24
    virtual void  setIsStatic(bool isStatic) override;                       // +0x28
    virtual bool  getIsStatic() const override;                              // +0x2C
    virtual void  setUsedOnTransparentMaterials(bool used) override;         // +0x30
    virtual bool  getUsedOnTransparentMaterials() const override;            // +0x34
    virtual void  setUvExtent(const Vec2& extent) override;                  // +0x38
    virtual Vec2  getUvExtent() const override;                              // +0x3C
    virtual void  setRandomRotation(float randomRotation) override;          // +0x40
    virtual float getRandomRotation() const override;                        // +0x44
    virtual void  setRotation(float rotation) override;                      // +0x48
    virtual float getRotation() const override;                              // +0x4C
    virtual void  setDecalTextureName(const DecalTemplateString& name) override; // +0x50
    virtual const DecalTemplateString& getDecalTextureName() const override; // +0x54
    virtual void  setAnimationEnable(bool enable) override;                  // +0x58
    virtual bool  getAnimationEnable() const override;                       // +0x5C
    virtual void  setAnimationRandomizedStartFrame(bool randomized) override;// +0x60
    virtual bool  getAnimationRandomizedStartFrame() const override;         // +0x64
    virtual void  setAnimationSpeed(float speed) override;                   // +0x68
    virtual float getAnimationSpeed() const override;                        // +0x6C
    virtual void  setAnimationFrameWidth(unsigned int width) override;       // +0x70
    virtual unsigned int getAnimationFrameWidth() const override;            // +0x74
    virtual void  setAnimationFrameHeight(unsigned int height) override;     // +0x78
    virtual unsigned int getAnimationFrameHeight() const override;           // +0x7C
    virtual void  setAnimationFrameWidthRelative(float width) override;      // +0x80
    virtual float getAnimationFrameWidthRelative() const override;           // +0x84
    virtual void  setAnimationFrameHeightRelative(float height) override;    // +0x88
    virtual float getAnimationFrameHeightRelative() const override;          // +0x8C
    virtual void  setAnimationFrameCount(unsigned int count) override;       // +0x90
    virtual unsigned int getAnimationFrameCount() const override;            // +0x94
    virtual void  setAnimationFrameCountX(unsigned int count) override;      // +0x98
    virtual unsigned int getAnimationFrameCountX() const override;           // +0x9C

    // non-virtual, in the image's order
    void reset();
    void updateUv();
    bool loadTexture(const DecalTemplateString& name);
    // 0x101AFAE0 - `loadTexture(m_decalTextureName); jmp [vtbl+4]`. NO reference in the image
    // (no call, no table, no data word holds its address); kept because it is shipped code.
    void reloadTexture();

    // typed accessors for the other decal code (DecalManager / D1's DecalObject::add and its
    //      clipping helpers / the render path). Retail's DecalObject.cpp reads these fields RAW
    //      (e.g. 0x101AC920: +0x34, +0x38, +0x88, +0x94..+0xA4; 0x101AE4B0: +0x08..+0x10, +0x20,
    //      +0x58, +0x78, +0x7C, +0x80); the native flavour moves every field after +0x3C, so those
    //      reads must come through here. Plain field loads - no virtual call - like the originals.
    unsigned short getIndex() const                  { return m_index; }                 // +0x58
    void           setIndex(unsigned short index)    { m_index = index; }
    dx9rend::EngineTexture*     getTexture() const   { return m_texture; }               // +0x78
    dx9rend::TextureAtlasEntry* getAtlasEntry() const{ return m_atlasEntry; }            // +0x7C
    const Vec4&    colorVec4() const                 { return m_color; }                 // +0x08
    bool           isStaticField() const             { return m_isStatic; }              // +0x20
    const Vec2&    uvOffset() const                  { return m_uvOffset; }              // +0x34
    bool           randomizedStartFrameField() const { return m_animationRandomizedStartFrame; } // +0x80
    bool           animationEnableField() const      { return m_animationEnable; }       // +0x88
    unsigned int   frameCountField() const           { return m_animationFrameCount; }   // +0x94
    unsigned int   frameCountXField() const          { return m_animationFrameCountX; }  // +0x98
    float          frameCountXReciprocal() const     { return m_frameCountXReciprocal; } // +0x9C
    const Vec2&    uvExtent() const                  { return m_uvExtent; }              // +0xA0

private:
    DecalTemplate(const DecalTemplate&);
    DecalTemplate& operator=(const DecalTemplate&);

    float                       m_randomColorIntensity;          // +0x04
    Vec4                        m_color;                         // +0x08
    float                       m_size;                          // +0x18
    float                       m_randomSize;                    // +0x1C
    bool                        m_isStatic;                      // +0x20
    bool                        m_usedOnTransparentMaterials;    // +0x21
    unsigned char               m_pad22[2];
    float                       m_randomRotation;                // +0x24
    float                       m_rotation;                      // +0x28
    Vec2                        m_vec2C;                         // +0x2C  [UNRESOLVED]
    Vec2                        m_uvOffset;                      // +0x34
    DecalTemplateString         m_decalTextureName;              // +0x3C
    unsigned short              m_index;                         // +0x58
    unsigned char               m_pad5A[2];
    DecalTemplateString         m_name;                          // +0x5C
    dx9rend::EngineTexture*     m_texture;                       // +0x78
    dx9rend::TextureAtlasEntry* m_atlasEntry;                    // +0x7C
    bool                        m_animationRandomizedStartFrame; // +0x80
    unsigned char               m_pad81[3];
    float                       m_animationSpeed;                // +0x84
    bool                        m_animationEnable;               // +0x88
    unsigned char               m_pad89[3];
    float                       m_animationFrameWidthRelative;   // +0x8C
    float                       m_animationFrameHeightRelative;  // +0x90
    unsigned int                m_animationFrameCount;           // +0x94
    unsigned int                m_animationFrameCountX;          // +0x98
    float                       m_frameCountXReciprocal;         // +0x9C
    Vec2                        m_uvExtent;                      // +0xA0

    friend struct DecalTemplateLayoutCheck;
};

// Retail's object is 0xA8 bytes (DecalManager::addTemplate's `push 0A8h`, 0x10124C81). The native
// object's two strings are std::string, so no retail size or offset after +0x3C is asserted.
struct DecalTemplateLayoutCheck
{
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // BF2_GEOM_DECALSYSTEM_DECALTEMPLATE_H
