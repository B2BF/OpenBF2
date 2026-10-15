// Geometry-template operations shared by the engine and renderer.
#ifndef BF2_GEOM_IGEOMETRYTEMPLATE_H
#define BF2_GEOM_IGEOMETRYTEMPLATE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"

#include <string>

namespace dice { namespace hfe { namespace geom {

// Common 44-slot geometry-template interface. Loader-chain virtuals belong to MeshTemplate
// itself and must not extend this shared interface.

class IGeometryTemplate : public ::dice::hfe::IBase
{
public:
    virtual unsigned int getClassId() const = 0;                                    // +0x10
    virtual void setName(const std::string& name) = 0;                             // +0x14
    virtual const std::string& getName() const = 0;                                // +0x18
    virtual void setPath(const std::string& path) = 0;                             // +0x1C
    virtual const std::string& getPath() const = 0;                                // +0x20
    virtual void* createInstance() = 0;                                             // +0x24  base: 0
    virtual bool isLoaded() const = 0;                                              // +0x28
    virtual bool load() = 0;                                                        // +0x2C  base: 0
    virtual void setLodDistance(int geom, int lod, float distance) = 0;             // +0x30
    virtual float getLodDistance(int geom, int lod) = 0;                            // +0x34  base: -1
    virtual void setMaterialValue(int geom, int lod, unsigned char material,
                                  float value) = 0;                                 // +0x38
    virtual float getMaterialValue(int geom, int lod, unsigned char material) = 0;  // +0x3C  base: 15
    virtual void slot_0x40(int a, int b, int c, int d) = 0;                         // +0x40  nullsub_32
    virtual float slot_0x44(int a, int b, int c) = 0;                               // +0x44  0.25
    virtual void slot_0x48(int a) = 0;                                              // +0x48  nullsub_53
    virtual bool slot_0x4C() = 0;                                                   // +0x4C  0
    virtual bool isWater() const = 0;                                               // +0x50  base: 0
    virtual unsigned int getMemoryUsage(int mask) = 0;                              // +0x54  base: 0
    virtual int getPrimitiveCount(int geom, int lod, int material) = 0;             // +0x58  base: 0
    virtual int getVertexCount(int geom, int lod, int material) = 0;                // +0x5C  base: 0
    virtual void setDoNotGenerateLightmaps(bool value) = 0;                         // +0x60
    virtual bool getDoNotGenerateLightmaps() const = 0;                             // +0x64  base: 0
    virtual void setLightmapOverride(bool value) = 0;                               // +0x68
    virtual bool getLightmapOverride() const = 0;                                   // +0x6C  base: 0
    virtual void setLightmapSize(int a, int b, int size) = 0;                       // +0x70
    virtual int getLightmapSize(int a, int b) = 0;                                  // +0x74  base: 0
    virtual void setCompressVertexData(bool value) = 0;                             // +0x78
    virtual bool getCompressVertexData() const = 0;                                 // +0x7C  base: 1
    virtual void slot_0x80(int a) = 0;                                              // +0x80  nullsub_53
    virtual bool slot_0x84() = 0;                                                   // +0x84  0
    virtual void setMaxTextureRepeat(int value) = 0;                                // +0x88
    virtual int getMaxTextureRepeat() const = 0;                                    // +0x8C  base: 16
    virtual void slot_0x90(int a) = 0;                                              // +0x90  nullsub_53
    virtual bool slot_0x94() = 0;                                                   // +0x94  0
    virtual void slot_0x98(int a) = 0;                                              // +0x98  nullsub_53
    virtual bool slot_0x9C() = 0;                                                   // +0x9C  0
    virtual int getMaterialCount(unsigned int lod) = 0;                             // +0xA0  base: 0
    virtual int getLodPrimitiveCount(unsigned int lod) = 0;                         // +0xA4  base: 0
    virtual Vec3* decompressPosition(Vec3* out, const Vec3* in) = 0;                // +0xA8  base: (1,1,1)
    // Despite its legacy name, preloads device resources into video memory; releases nothing.
    virtual void releaseDeviceResources() = 0;                                      // +0xAC
    // The interface ends here (44 slots). +0xB0..+0xEC belong to MeshTemplate - see the header note.
};

} } } // namespace dice::hfe::geom

#endif // BF2_GEOM_IGEOMETRYTEMPLATE_H
