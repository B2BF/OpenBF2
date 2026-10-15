// Shared compiled-road textures/settings and instance creation.
// [INFERRED] header placement. Keep the GeometryTemplate prefix and virtual order.
// The instance list is used during destruction; roads do not unlink themselves when destroyed,
// so the list may retain dangling pointers. Loading initializes the renderer's priority.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_ROADCOMPILEDTEMPLATE_H
#define BF2_GEOM_ROADCOMPILEDTEMPLATE_H

#include "MeshTemplate.h"          // IGeometryTemplate (44 slots) and the 12-byte GeometryTemplate prefix

#include <stddef.h>                // offsetof - the layout assertions below
#include <list>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; }

namespace geom {

class RoadCompiled;


// The shipped std::list<RoadCompiled*>: template instances and the per-frame compiled-road queue.

typedef std::list<RoadCompiled*> RoadList;


// RoadCompiledTemplate - GeometryTemplate.RoadCompiled. The 44 slots in table order; the ones this
// class does not own are the generic IGeometryTemplate default bodies, read one by one (addresses
// beside each declaration).

class RoadCompiledTemplate : public GeometryTemplate
{
public:
    RoadCompiledTemplate();
    ~RoadCompiledTemplate();

    // IBase
    virtual void  addRef();                                                         // +0x00
    virtual int   getRef();                                                         // +0x04
    virtual int   release();                                                        // +0x08
    virtual void* queryInterface(unsigned int iid) const;                           // +0x0C

    // IGeometryTemplate
    virtual unsigned int getClassId() const;                                        // +0x10
    virtual void setName(const std::string& name);                                 // +0x14
    virtual const std::string& getName() const;                                    // +0x18
    virtual void setPath(const std::string& path);                                 // +0x1C
    virtual const std::string& getPath() const;                                    // +0x20
    virtual void* createInstance();                                                 // +0x24
    virtual bool isLoaded() const;                                                  // +0x28
    virtual bool load();                                                            // +0x2C
    virtual void setLodDistance(int geom, int lod, float distance);                 // +0x30
    virtual float getLodDistance(int geom, int lod);                                // +0x34
    virtual void setMaterialValue(int geom, int lod, unsigned char material,
                                  float value);                                     // +0x38
    virtual float getMaterialValue(int geom, int lod, unsigned char material);      // +0x3C
    virtual void slot_0x40(int a, int b, int c, int d);                             // +0x40
    virtual float slot_0x44(int a, int b, int c);                                   // +0x44
    virtual void slot_0x48(int a);                                                  // +0x48
    virtual bool slot_0x4C();                                                       // +0x4C
    virtual bool isWater() const;                                                   // +0x50
    virtual unsigned int getMemoryUsage(int mask);                                  // +0x54
    virtual int getPrimitiveCount(int geom, int lod, int material);                 // +0x58
    virtual int getVertexCount(int geom, int lod, int material);                    // +0x5C
    virtual void setDoNotGenerateLightmaps(bool value);                             // +0x60
    virtual bool getDoNotGenerateLightmaps() const;                                 // +0x64
    virtual void setLightmapOverride(bool value);                                   // +0x68
    virtual bool getLightmapOverride() const;                                       // +0x6C
    virtual void setLightmapSize(int a, int b, int size);                           // +0x70
    virtual int getLightmapSize(int a, int b);                                      // +0x74
    virtual void setCompressVertexData(bool value);                                 // +0x78
    virtual bool getCompressVertexData() const;                                     // +0x7C
    virtual void slot_0x80(int a);                                                  // +0x80
    virtual bool slot_0x84();                                                       // +0x84
    virtual void setMaxTextureRepeat(int value);                                    // +0x88
    virtual int getMaxTextureRepeat() const;                                        // +0x8C
    virtual void slot_0x90(int a);                                                  // +0x90
    virtual bool slot_0x94();                                                       // +0x94
    virtual void slot_0x98(int a);                                                  // +0x98
    virtual bool slot_0x9C();                                                       // +0x9C
    virtual int getMaterialCount(unsigned int lod);                                 // +0xA0
    virtual int getLodPrimitiveCount(unsigned int lod);                             // +0xA4
    virtual Vec3* decompressPosition(Vec3* out, const Vec3* in);                    // +0xA8
    virtual void releaseDeviceResources();                                          // +0xAC nullsub_15

    // data, in shipped order (GeometryTemplate's +0x04/+0x08 precede)
    int                      m_refCount;       // +0x0C  ctor 1
    unsigned char            m_loaded;         // +0x10  ctor 0
    unsigned char            m_pad11[3];       // +0x11  not written
    RoadList                 m_instances;      // +0x14
    std::string             m_name;           // +0x20
    std::string             m_path;           // +0x3C
    dx9rend::EngineTexture*  m_texture0;       // +0x58  ctor 0
    dx9rend::EngineTexture*  m_texture1;       // +0x5C  ctor 0
    float                    m_blendFactor;    // +0x60  ctor 0
    float                    m_fadeOff[2];     // +0x64  ctor (0, 0)
    int                      m_priority;       // +0x6C  NOT written by the constructor

private:
    RoadCompiledTemplate(const RoadCompiledTemplate&);
    RoadCompiledTemplate& operator=(const RoadCompiledTemplate&);
};

#if defined(_M_IX86)
static_assert(offsetof(RoadCompiledTemplate, m_refCount) == 0x0C, "refCount at +0x0C");
static_assert(offsetof(RoadCompiledTemplate, m_loaded) == 0x10, "loaded at +0x10");
static_assert(offsetof(RoadCompiledTemplate, m_instances) == 0x14, "instance list at +0x14");
#endif

// The reflection id getClassId answers and the Object.geometry.* rows test (0x100B12F0).
// Core/Ids.h defines the same name as an internal-linkage constant; a TU cannot include both (the
//   CID_RoadCompiledRenderer / ModuleGeom.cpp arrangement is the same).
extern const unsigned int CID_RoadCompiledTemplate;

// RefFactory published by ModuleGeom::getClassList.
IBase* __fastcall createRoadCompiledTemplate(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ROADCOMPILEDTEMPLATE_H
#endif // BF2_IMAGE_RENDDX9
