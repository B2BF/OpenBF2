// Hemi-map manager interface; preserve the fourteen-slot virtual declaration order.

#ifndef BF2_GEOM_HEMIMAPMANAGER_HEMIMAPMANAGER_H
#define BF2_GEOM_HEMIMAPMANAGER_HEMIMAPMANAGER_H

#if defined(BF2_IMAGE_RENDDX9)
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#else
#include "../../Core/IBase.h"
#include <string>
#endif

#include "../../Math/Vec3.h"
#include <cstddef>

namespace dice {
namespace hfe {

namespace dx9rend {
struct EngineTexture;
}

namespace geom {

#if defined(BF2_IMAGE_RENDDX9)
// PE 0x1012B600 allocates 0x34 bytes.  This is an ordinary owned data object, not an interface;
// publish the recovered layout so its clients can use the real field instead of growing an
// accessor seam or a positional dispatch facade around it.  The internal name is the renderer's
// std::string.
struct HemiMap
{
    Vec3 position;                         // +0x00
    float range;                           // +0x0C
    float falloff;                         // +0x10
    dx9rend::EngineTexture* texture;       // +0x14
    std::string name;                     // +0x18
};

#if defined(_M_IX86)
static_assert(offsetof(HemiMap, texture) == 0x14, "HemiMap texture offset drift");
static_assert(offsetof(HemiMap, name) == 0x18, "HemiMap name offset drift");
#endif
#else
struct HemiMap;
#endif

// PE 0x1012B2F0 caches these four values at manager +0x4C.  The source field spellings are
// [INFERRED]; their offsets and values are instruction-proven.
struct HemiMapInfo
{
    float positionX;
    float positionZ;
    float range;
    float lerpBias;
};

#if defined(BF2_IMAGE_RENDDX9)
typedef ::renddx9abi::HostString HemiMapName;
#else
typedef std::string HemiMapName;
#endif

// Primary table 0x1021CE84, exactly 14 entries.  The neutral slot names at +0x20/+0x24/+0x28 are
// intentional: their bodies and x86 carriers are recovered, but no symbol or string attests a
// source name.  Keeping them virtual here is still important—the later bias/makeHemiMap methods
// must occupy their retail positions in an ordinary C++ vtable.
class IHemiMapManager : public IBase
{
public:
    virtual void clear() = 0;                                            // +0x10

    // PE 0x1012BF70 destroys the old map, creates the replacement, stores it at +0x08, and
    // returns without explicitly materializing a result.  Its sole retail caller (console
    // executor 0x100C43E0) discards EAX, so this facade correctly makes the operation void rather
    // than treating the helper's incidental EAX as HemiMap*/bool.
    virtual void setBaseHemiMap(const HemiMapName& name, const Vec3& position,
                                float range, float falloff) = 0;          // +0x14
    virtual HemiMap* getBaseHemiMap() const = 0;                          // +0x18
    virtual const HemiMapInfo& getHemiMapInfo(const HemiMap* map) = 0;    // +0x1C
    virtual bool slot20(unsigned int, unsigned int,
                        unsigned int, unsigned int) = 0;                   // +0x20
    virtual bool slot24(unsigned int) = 0;                                // +0x24
    virtual HemiMap* slot28(unsigned int) = 0;                            // +0x28
    virtual void setHemiLerpBias(float bias) = 0;                         // +0x2C
    virtual float getHemiLerpBias() const = 0;                            // +0x30
    virtual bool makeHemiMap(unsigned long resolution, HemiMapName outputName,
                             Vec3 position, float orthoExtent) = 0;       // +0x34
};

extern const unsigned int IID_IHemiMapManager;
extern const unsigned int CID_HemiMapManager;

IBase* __fastcall createHemiMapManager(unsigned int iid, IBase* outer);

// ModuleGeom's singleton publication target. IHemiMapManager has IBase at offset zero, so the
// ClassManager's IBase* write and ordinary typed calls address the same primary subobject.
extern IHemiMapManager* g_HemiMapManager;

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_HEMIMAPMANAGER_HEMIMAPMANAGER_H
