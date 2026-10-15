// Editable-road settings and writer for compiled road .con/.dat files.
// [INFERRED] header placement. Indexed setters do not check bounds: use texture layers 0/1
// and LOD 0. The integer at +0x5C is [UNRESOLVED]; the float at +0x60 is [UNRESOLVED].

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_ROADTEMPLATE_H
#define BF2_GEOM_ROADTEMPLATE_H

#include "IRoadTemplate.h"             // IRoadTemplate (table 0x1021D610), shared with the editor
#include "../Math/Vec3.h"             // Vec2
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"   // renddx9abi::HostString

#include <stddef.h>
#include <vector>

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
    std::vector< ::renddx9abi::HostString> m_textureFiles;         // +0x08
    std::vector<Vec2>                     m_textureScales;         // +0x18
    std::vector<bool>                     m_mirrors;               // +0x28
    std::vector<float>                    m_maxTesselatedErrors;   // +0x3C
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

// The object never crosses the boundary by layout, only through its slots.

// RefFactory published by ModuleGeom::getClassList.
IBase* __fastcall createRoadTemplate(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ROADTEMPLATE_H
#endif // BF2_IMAGE_RENDDX9
