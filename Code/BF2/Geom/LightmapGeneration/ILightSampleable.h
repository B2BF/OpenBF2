// Light-sampler factory interface (IID_ILightSampleable, Core/Ids.h).
// [INFERRED] header placement: no header path is attested. TerrainEditable implements it at +0x774
// and StaticMesh's explicit table at +0x30 has the same three slots; LightmapGeneration finds it by
// queryInterface on an object's geometry or on the terrain. Slot names are [INFERRED].

#ifndef BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLEABLE_H
#define BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLEABLE_H

#include "../../Core/IBase.h"

namespace dice {
namespace hfe {
namespace geom {

class ILightSampling;

class ILightSampleable : public IBase
{
public:
    // True when this object takes part in lightmap generation (both implementations answer true).
    virtual bool isLightSampleable() = 0;                                       // +0x10
    // Creates a sampler for the current geometry. `resolutionModifier` scales the lightmap size
    // (LightmapGeneration +0x54, 1.0 by default; the terrain sampler stores it at +0x1C).
    // Returns null when no sampler could be made.
    virtual ILightSampling* createLightSampler(float resolutionModifier) = 0;    // +0x14
    // Releases `sampler` (which writes its lightmap). TerrainEditable answers false for null;
    // StaticMesh reloads the instance's lightmap and answers true.
    virtual bool destroyLightSampler(ILightSampling* sampler) = 0;              // +0x18

protected:
    ~ILightSampleable() {}
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLEABLE_H
