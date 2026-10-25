// Fractal noise interface (IID 80035, class 81035): the edge noise of an editable road.
// [CHOSEN, not recovered] shared header usable by the renderer and editor images; the slot order is
// the 1.50 table's. Method names are [INFERRED] from their bodies and the retail editor's Road rows.
#ifndef BF2_GEOMMISC_IPERLINNOISE_H
#define BF2_GEOMMISC_IPERLINNOISE_H

#include "../Core/IBase.h"
#include "../Math/Vec3.h"                     // Vec2

namespace dice {
namespace hfe {
namespace geom {

// Defaults: seed time(0), octaves 1, persistence 0.25, output range (-1, 1). A non-zero dimension makes
// every sample 0 (only the one-dimensional path exists). Below x = 0 the blend extrapolates beyond the
// range (shipped).
class IPerlinNoise : public IBase
{
public:
    virtual void  setSeed(int seed) = 0;                                        // +0x10
    virtual int   getSeed() = 0;                                                // +0x14
    virtual void  setDimension(int dimension) = 0;                              // +0x18
    virtual int   getDimension() = 0;                                           // +0x1C
    virtual void  setSmoothing(int mode) = 0;                                   // +0x20
    virtual int   getSmoothing() = 0;                                           // +0x24
    virtual void  setInterpolation(int mode) = 0;                               // +0x28
    virtual int   getInterpolation() = 0;                                       // +0x2C
    virtual void  setOctaves(int octaves) = 0;                                  // +0x30
    virtual int   getOctaves() = 0;                                             // +0x34
    virtual void  setPersistence(float persistence) = 0;                        // +0x38
    virtual float getPersistence() = 0;                                         // +0x3C
    virtual void  setOutputRange(const Vec2& range) = 0;                        // +0x40
    virtual const Vec2& getOutputRange() = 0;                                   // +0x44
    // Four floats; only the first is read. RoadEditable calls it as sample(x, 0, 0, 0).
    virtual float sample(float x, float y, float z, float w) = 0;               // +0x48
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_IPERLINNOISE_H
