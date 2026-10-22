// Editable-road settings and writer for compiled road .con/.dat files.
// [INFERRED] header placement. Indexed setters do not check bounds: use texture layers 0/1
// and LOD 0. The integer at +0x5C is [UNRESOLVED]; the float at +0x60 is [UNRESOLVED].

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_ROADTEMPLATE_H
#define BF2_GEOM_ROADTEMPLATE_H

#include "IRoadTemplate.h"             // IRoadTemplate (table 0x1021D610), shared with the editor
#include "UndergrowthSystem.h"        // Vc71Vec<T>
#include "Vc71BoolVector.h"
#include "../Math/Vec3.h"             // Vec2
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"   // renddx9abi::HostString

#include <stddef.h>

namespace dice {
namespace hfe {
namespace geom {


class RoadTemplate : public IRoadTemplate
{
public:
    RoadTemplate();
    ~RoadTemplate();

    // IBase
    virtual void  addRef();                                                     // +0x00
    virtual int   getRef();                                                     // +0x04
    virtual int   release();                                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;                       // +0x0C

    // IRoadTemplate
    virtual void setTextureFile(int index, const ::renddx9abi::HostString& file);
    virtual const ::renddx9abi::HostString& getTextureFile(int index);
    virtual void setTextureScale(int index, const Vec2& scale);
    virtual const Vec2& getTextureScale(int index);
    virtual void setMirror(unsigned int index, bool mirror);
    virtual bool getMirror(unsigned int index);
    virtual void setWidth(float width);
    virtual float getWidth();
    virtual void setMaxTesselatedError(int lod, float error);
    virtual float getMaxTesselatedError(int lod);
    virtual void setBlendFactor(float factor);
    virtual float getBlendFactor();
    virtual void setFadeOffDistance(const Vec2& distance);
    virtual const Vec2& getFadeOffDistance();
    virtual void setPriority(int priority);                                         // (ICF)
    virtual int getPriority();                                                      // (ICF)
    virtual void setMaterial(int material);                                         // (ICF)
    virtual int getMaterial();                                                      // (ICF)
    virtual void setName(const ::renddx9abi::HostString& name);
    virtual ::renddx9abi::HostString getName();
    virtual bool compile(const ::renddx9abi::HostString& path);
    virtual void setUnknown5C(int value);
    virtual int getUnknown5C();
    virtual void setUnknown60(float value);
    virtual float getUnknown60();

    // data, in shipped order
    int                                   m_refCount;              // +0x04
    Vc71Vec< ::renddx9abi::HostString>    m_textureFiles;          // +0x08
    Vc71Vec<Vec2>                         m_textureScales;         // +0x18
    Vc71BoolVector                        m_mirrors;               // +0x28
    Vc71Vec<float>                        m_maxTesselatedErrors;   // +0x3C
    float                                 m_blendFactor;           // +0x4C
    Vec2                                  m_fadeOffDistance;       // +0x50
    float                                 m_width;                 // +0x58
    int                                   m_unknown5C;             // +0x5C
    float                                 m_unknown60;             // +0x60
    int                                   m_material;              // +0x64
    ::renddx9abi::HostString              m_name;                  // +0x68
    int                                   m_priority;              // +0x84

private:
    RoadTemplate(const RoadTemplate&);
    RoadTemplate& operator=(const RoadTemplate&);
};

// The retail flavour's layout is the shipped one. In the native flavour HostString is the host's
// std::string, so the name (and the texture-file elements) take that type's size instead - the
// object never crosses the boundary by layout, only through its slots.
#if defined(_M_IX86) && !BF2_PLUGIN_ABI_NATIVE
static_assert(offsetof(RoadTemplate, m_refCount) == 0x04, "refCount at +0x04");
static_assert(offsetof(RoadTemplate, m_textureFiles) == 0x08, "texture files at +0x08");
static_assert(offsetof(RoadTemplate, m_textureScales) == 0x18, "texture scales at +0x18");
static_assert(offsetof(RoadTemplate, m_mirrors) == 0x28, "mirror flags at +0x28");
static_assert(offsetof(RoadTemplate, m_maxTesselatedErrors) == 0x3C, "max errors at +0x3C");
static_assert(offsetof(RoadTemplate, m_blendFactor) == 0x4C, "blend factor at +0x4C");
static_assert(offsetof(RoadTemplate, m_fadeOffDistance) == 0x50, "fade-off at +0x50");
static_assert(offsetof(RoadTemplate, m_width) == 0x58, "width at +0x58");
static_assert(offsetof(RoadTemplate, m_material) == 0x64, "material at +0x64");
static_assert(offsetof(RoadTemplate, m_name) == 0x68, "name at +0x68");
static_assert(offsetof(RoadTemplate, m_priority) == 0x84, "priority at +0x84");
static_assert(sizeof(RoadTemplate) == 0x88, "the factory 0x100B099A allocates 0x88");
#endif

// RefFactory published by ModuleGeom::getClassList.
IBase* __fastcall createRoadTemplate(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ROADTEMPLATE_H
#endif // BF2_IMAGE_RENDDX9
