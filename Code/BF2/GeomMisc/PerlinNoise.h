// One-dimensional fractal noise for editable road edges. [INFERRED] header placement.
// Supports plain/smoothed sampling and linear, cosine or cubic interpolation.
// The dimension selector is [INFERRED]; nonzero values remain unsupported.
// Keep IPerlinNoise before IConsoleSaveable. Each base sample reseeds the generator.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOMMISC_PERLINNOISE_H
#define BF2_GEOMMISC_PERLINNOISE_H

#include "IPerlinNoise.h"                  // IPerlinNoise, shared with the editor
#include "../../RendDX9Abi/HfeAbi.h"
#include "../IO/Console/IConsoleSaveable.h"
#include "../Math/Vec3.h"                  // Vec2 - the output range (two floats, 0x1009F470)

#include <stddef.h>

namespace dice {
namespace hfe {
namespace geom {


class PerlinNoise : public IPerlinNoise, public ::dice::hfe::io::IConsoleSaveable
{
public:
    PerlinNoise();

    // IBase (the primary copies; the IConsoleSaveable thunks are the compiler's)
    virtual void  addRef();                                                     // +0x00
    virtual int   getRef();                                                     // +0x04
    virtual int   release();                                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;                       // +0x0C

    // IPerlinNoise
    virtual void  setSeed(int seed);
    virtual int   getSeed();
    virtual void  setDimension(int dimension);
    virtual int   getDimension();
    virtual void  setSmoothing(int mode);
    virtual int   getSmoothing();
    virtual void  setInterpolation(int mode);
    virtual int   getInterpolation();
    virtual void  setOctaves(int octaves);
    virtual int   getOctaves();
    virtual void  setPersistence(float persistence);
    virtual float getPersistence();
    virtual void  setOutputRange(const Vec2& range);
    virtual const Vec2& getOutputRange();
    virtual float sample(float x, float y, float z, float w);

    // Io::IConsoleSaveable
    virtual bool makeScript(::dice::hfe::io::IStream* stream);                  // 0x1009F460 -> false

    // The TU's own members (non-virtual in the image)
    int   nextRandom();                                                         // 0x1009EF30 lcgrand
    float baseNoise(int n);
    float interpolatedNoise(int n, float t);
    float octaveSum(float x);

    // Data, in shipped order (the two vptrs precede)
    int         m_refCount;        // +0x08
    int         m_state;           // +0x0C
    int         m_seed;            // +0x10
    int         m_dimension;       // +0x14
    int         m_smoothing;       // +0x18
    int         m_interpolation;   // +0x1C
    int         m_octaves;         // +0x20
    float       m_persistence;     // +0x24
    Vec2 m_range;           // +0x28

private:
    PerlinNoise(const PerlinNoise&);
    PerlinNoise& operator=(const PerlinNoise&);
};

#if defined(_M_IX86)
static_assert(offsetof(PerlinNoise, m_refCount) == 0x08, "refCount at +0x08");
static_assert(offsetof(PerlinNoise, m_state) == 0x0C, "lcgrand state at +0x0C");
static_assert(offsetof(PerlinNoise, m_seed) == 0x10, "seed at +0x10");
static_assert(offsetof(PerlinNoise, m_octaves) == 0x20, "octaves at +0x20");
static_assert(offsetof(PerlinNoise, m_persistence) == 0x24, "persistence at +0x24");
static_assert(offsetof(PerlinNoise, m_range) == 0x28, "output range at +0x28");
static_assert(sizeof(PerlinNoise) == 0x30, "the factory 0x1007CEF0 allocates 0x30");
#endif

// RefFactory 0x1007CEF0 - published by ModuleGeomMisc::getClassList.
IBase* __fastcall createPerlinNoise(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOMMISC_PERLINNOISE_H
#endif // BF2_IMAGE_RENDDX9
