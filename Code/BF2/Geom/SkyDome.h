// Path: [INFERRED] from the registered class; no __FILE__ or Linux witness.
// Sky, cloud and flare rendering, recovered from RendDX9.dll 1.50.
// Retail layout: 0x1D4 bytes; constructor 0x10138CB0, primary table 0x1021D3A8.
// Slot names are [INFERRED]; positions and arities are instruction-proven.

#ifndef BF2_GEOM_SKYDOME_H
#define BF2_GEOM_SKYDOME_H

#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/IBase.h"
#include "../Math/Vec3.h"
#include "../RendDX9/RendDX9Base.h"

namespace dice {
namespace hfe {

namespace world { class LightSource; }

namespace dx9rend {
class Effect;
class ShaderParameter;
struct EngineTexture;
}

namespace geom {

class IGeometryTemplate;
class StaticMeshMaterial;

// Owns sky/cloud geometry and textures; draws the dome through the listener at +0x04.
// Keep IBase first and RenderPathListener second. The constructor deliberately leaves
// m_flareMaterial and m_dword180 untouched; m_hasFlare gates their use.
class SkyDome : public IBase, public dx9rend::RenderPathListener
{
public:
    SkyDome();
    ~SkyDome();                                                      // NOT a slot

    // IBase, +0x00..+0x0C
    virtual void  addRef();                                          // ICF_incrementDwordAt0xC
    virtual int   getRef();                                          // ICF_getDwordAt0xC
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // the 39 own slots, +0x10..+0xA8
    virtual void update(float deltaTime);                            // +0x10
    virtual void setPosition(const Vec3& position);                  // +0x14
    virtual const Vec3& getPosition() const;                         // +0x18
    virtual void setDomeScale(float scale);                          // +0x1C
    virtual float getDomeScale() const;                              // +0x20
    virtual void setDomeRotation(float degrees);                     // +0x24
    virtual float getDomeRotation() const;                           // +0x28
    virtual void setSkyTemplate(const std::string& name);           // +0x2C
    virtual const std::string& getSkyTemplate() const;              // +0x30
    virtual void setSkyTexture(const std::string& name);            // +0x34
    virtual const std::string& getSkyTexture() const;               // +0x38
    virtual void setHasCloudLayer(bool value);                       // +0x3C
    virtual bool getHasCloudLayer() const;                           // +0x40
    virtual void setHasCloudLayer2(bool value);                      // +0x44
    virtual bool getHasCloudLayer2() const;                          // +0x48
    virtual void setCloudTemplate(const std::string& name);         // +0x4C
    virtual const std::string& getCloudTemplate() const;            // +0x50
    virtual void setCloudTexture(const std::string& name);          // +0x54
    virtual const std::string& getCloudTexture() const;             // +0x58
    virtual void setCloudTexture2(const std::string& name);         // +0x5C
    virtual const std::string& getCloudTexture2() const;            // +0x60
    virtual void setScrollDirection(const Vec2& direction);          // +0x64
    virtual const Vec2& getScrollDirection() const;                  // +0x68
    virtual void setScrollDirection2(const Vec2& direction);         // +0x6C
    virtual const Vec2& getScrollDirection2() const;                 // +0x70
    virtual void setFadeCloudsDistances(const Vec2& distances);      // +0x74
    virtual const Vec2& getFadeCloudsDistances() const;              // +0x78
    virtual void setCloudLerpFactors(const Vec2& factors);           // +0x7C
    virtual const Vec2& getCloudLerpFactors() const;                 // +0x80
    virtual void setFlareTemplate(const std::string& name);         // +0x84
    virtual const std::string& getFlareTemplate() const;            // +0x88
    virtual void setFlareTexture(const std::string& name);          // +0x8C
    // NOT const: 0x101382D0 stores 1
    // into m_flareOcclusionReference (+0x1AC) before returning the string, which resets the flare's
    // occlusion ratio to "fully visible" - i.e. it marks the flare state dirty. A getter with a
    // side effect; the console's flareTexture read therefore has an observable effect on rendering.
    virtual const std::string& getFlareTexture();                   // +0x90
    virtual void setCloudLayerCount(int count);                      // +0x94
    virtual void setLightingBlend(float value);                      // +0x98
    virtual float getLightingBlend() const;                          // +0x9C
    virtual void setLightingColor(const Vec3& color);                // +0xA0
    virtual const Vec3& getLightingColor() const;                    // +0xA4
    // Renderer::renderFrame (0x1002F7E0) calls this three times a frame with 0, 1 and 2.
    virtual void drawFlare(int mode);                                // +0xA8

    // dx9rend::RenderPathListener, secondary table 0x1021D390
    virtual void onRenderPathActivated(int path);                    // +0x00
    virtual void onRenderPathDeactivated();                          // +0x04 nullsub_15
    virtual void onRenderPathChanged(int path);                      // +0x08 nullsub_53
    // THE ODD slot. Retail's dome/cloud draw lives in the listener table, not the primary one -
    // it occupies the position RendDX9Base.h calls onRenderPathNotification, and Renderer::
    // renderFrame dispatches the draw pass through exactly that entry. The body takes one stack
    // argument (`retn 4`) and never reads it; the parameter is kept so the arity matches.
    virtual void onRenderPathNotification(int unused);               // +0x0C
    virtual int  getCurrentRenderPath() const;                       // +0x10
    virtual std::string getRenderPathListenerName() const;          // +0x14

private:
    // Retail inlines this bind-and-submit sequence at both of drawFlare's two draw sites
    // (0x10139E4B.. and 0x10139F77..); they are byte-for-byte the same. One helper, two calls.
    void submitFlarePass();

    SkyDome(const SkyDome&);
    SkyDome& operator=(const SkyDome&);

    // data, in shipped order
    int                  m_renderPath;              // +0x008
    int                  m_refCount;                // +0x00C
    Vec3                 m_position;                // +0x010
    float                m_domeScale;               // +0x01C
    float                m_domeRotation;            // +0x020
    std::string         m_skyTemplate;             // +0x024
    std::string         m_flareTemplate;           // +0x040
    std::string         m_skyTexture;              // +0x05C
    unsigned char        m_hasCloudLayer;           // +0x078
    unsigned char        m_hasCloudLayer2;          // +0x079
    unsigned char        m_pad07A[2];
    int                  m_cloudLayerCount;         // +0x07C
    std::string         m_cloudTemplate;           // +0x080
    std::string         m_cloudTexture;            // +0x09C
    std::string         m_cloudTexture2;           // +0x0B8
    Vec2                 m_scrollDirection;         // +0x0D4
    Vec2                 m_scrollDirection2;        // +0x0DC
    IGeometryTemplate*   m_skyTemplatePtr;          // +0x0E4
    IGeometryTemplate*   m_flareTemplatePtr;        // +0x0E8
    IGeometryTemplate*   m_cloudTemplatePtr;        // +0x0EC
    dx9rend::EngineTexture* m_skyTextureTex;        // +0x0F0
    dx9rend::EngineTexture* m_cloudTextureTex;      // +0x0F4
    dx9rend::EngineTexture* m_cloudTexture2Tex;     // +0x0F8
    dx9rend::Effect*     m_effect;                  // +0x0FC
    dx9rend::ShaderParameter* m_paramTexLayer0;     // +0x100
    dx9rend::ShaderParameter* m_paramTexLayer1;     // +0x104
    dx9rend::ShaderParameter* m_paramTexLayer2;     // +0x108
    dx9rend::ShaderParameter* m_paramTexOffset;     // +0x10C
    dx9rend::ShaderParameter* m_paramTexOffset2;    // +0x110
    dx9rend::ShaderParameter* m_paramCloudsFadeOutDist;  // +0x114
    dx9rend::ShaderParameter* m_paramCloudLerpFactors;   // +0x118
    dx9rend::ShaderParameter* m_paramWorldViewProjection;// +0x11C
    dx9rend::ShaderParameter* m_paramFlareParams;   // +0x120
    dx9rend::ShaderParameter* m_paramFogColor;      // +0x124
    dx9rend::ShaderParameter* m_paramLightingBlend; // +0x128
    dx9rend::ShaderParameter* m_paramLightingColor; // +0x12C
    void*                m_vertexDeclClouds;        // +0x130
    void*                m_vertexDeclNoClouds;      // +0x134
    float                m_time;                    // +0x138
    Vec2                 m_texOffset;               // +0x13C
    Vec2                 m_texOffset2;              // +0x144
    Vec2                 m_fadeCloudsDistances;     // +0x14C
    Vec2                 m_cloudLerpFactors;        // +0x154
    StaticMeshMaterial*  m_skyMaterial;             // +0x15C
    StaticMeshMaterial*  m_flareMaterial;           // +0x160  uninitialised, as shipped
    int                  m_dword164;                // +0x164  [UNRESOLVED]
    void*                m_flareQuery;              // +0x168
    float                m_flareFadeAdd;            // +0x16C
    float                m_flareFadeMul;            // +0x170
    float                m_flareAngleFadeAdd;       // +0x174
    float                m_flareAngleFadeMul;       // +0x178
    dx9rend::EngineTexture* m_flareTextureTex;      // +0x17C
    int                  m_dword180;                // +0x180  uninitialised, [UNRESOLVED]
    std::string         m_flareTextureName;        // +0x184
    Vec3                 m_flareDirection;          // +0x1A0
    int                  m_flareOcclusionReference; // +0x1AC
    int                  m_flareOcclusionVisible;   // +0x1B0
    unsigned char        m_flareOcclusionPending;   // +0x1B4
    unsigned char        m_hasFlare;                // +0x1B5
    unsigned char        m_pad1B6[2];
    int                  m_flareOcclusionFrame;     // +0x1B8
    unsigned int         m_flareOcclusionResultFrame; // +0x1BC
    int                  m_flareOcclusionInterval;  // +0x1C0
    float                m_lightingBlend;           // +0x1C4
    Vec3                 m_lightingColor;           // +0x1C8

public:
    // Typed accessors for fields used directly by the console; these add no vtable slots.
    const Vec3& flareDirection() const { return m_flareDirection; }
    void setFlareDirectionField(const Vec3& value) { m_flareDirection = value; }
    float flareFadeAdd() const { return m_flareFadeAdd; }
    void setFlareFadeAddField(float value) { m_flareFadeAdd = value; }
    float flareFadeMul() const { return m_flareFadeMul; }
    void setFlareFadeMulField(float value) { m_flareFadeMul = value; }
};

// dword_1024B918 in RendDX9.dll. ModuleGeom.cpp owns the definition; SkyDomeConsole.cpp and the
// renderer's three guarded calls read it.
extern IBase* g_SkyDome;

// The factory registered under "dice.hfe.geom.SkyDome" (0x100B0958).
IBase* __fastcall createSkyDome(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_SKYDOME_H
