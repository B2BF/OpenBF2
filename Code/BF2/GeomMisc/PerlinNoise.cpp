// Procedural noise generation. Arithmetic order is significant for floating-point results;
// transcendental calculations use double precision before conversion.


#ifdef BF2_IMAGE_RENDDX9

#include "PerlinNoise.h"

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"          // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/ModuleCommon.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <new>

namespace dice {
namespace hfe {
namespace geom {

// dword_1020AF00 - 81035, read as data by getClassList (0x1007DEC7).
extern const unsigned int CID_PerlinNoise = 81035u;

namespace {

// dword_1020AE74 (80035, IPerlinNoise) and dword_10208DC0 (0x3709A4FF, io::IConsoleSaveable) - the
// two ids queryInterface tests after IBase's 1; both read as data.
const unsigned int kIID_IPerlinNoise      = 80035u;
const unsigned int kIID_IConsoleSaveable  = 0x3709A4FFu;

// flt_1020CF68 / flt_1020CFD0 / flt_1020CFCC (2^-24, 2^-25, 2^-26) - the scales that turn the 24-bit
// draw into [0, 1), [0, 1/2) and [0, 1/4) of the range; flt_1020CF58 = 3.1415927f.
const float kTwoPowMinus24 = 5.9604644775390625e-08f;
const float kTwoPowMinus25 = 2.98023223876953125e-08f;
const float kTwoPowMinus26 = 1.490116119384765625e-08f;
const float kPi            = 3.1415927f;

// One Schrage half of lcgrand: x' = a * x mod (2^31 - 1) in 16-bit halves, with the image's signed
// shifts (`sar`) - every state it is handed is already in [0, 2^31 - 1).
int lcgrandHalf(int x, int factor)
{
    const int lo = (x & 0xFFFF) * factor;
    int hi = (x >> 16) * factor;
    hi += lo >> 16;
    const int folded = ((hi & 0x7FFF) << 16) + (hi >> 15);
    int result = folded + (lo & 0xFFFF) - 0x7FFFFFFF;
    if (result < 0)
        result += 0x7FFFFFFF;
    return result;
}

// The 24-bit draw every sampler makes: `or al, 80h; sar eax, 7; add eax, 1` - [1, 2^24], exact in a
// float.
float draw24(int value)
{
    return static_cast<float>(((value | 0x80) >> 7) + 1);
}

} // anonymous namespace


// Construction, reference counting, identity.


// The stores in shipped order after the three vptr writes: refCount 1, then the lcgrand
// state from the seed word before the seed is written - the allocator's leftover +0x10 masked to 31
// bits (shipped; read through memcpy so the stale bytes are copied, not interpreted early) - then the
// seed from time(0), and the six defaults.
PerlinNoise::PerlinNoise()
{
    m_refCount = 1;
    int stale;
    std::memcpy(&stale, &m_seed, sizeof(stale));
    m_state = stale & 0x7FFFFFFF;
    m_seed = static_cast<int>(std::time(0));
    m_dimension = 0;
    m_smoothing = 0;
    m_interpolation = 2;
    m_octaves = 1;
    m_persistence = 0.25f;
    m_range.x = -1.0f;
    m_range.y = 1.0f;
}

void PerlinNoise::addRef()
{
    ++m_refCount;                                                            // 0x1001A010 (shared)
}

int PerlinNoise::getRef()
{
    return m_refCount;                                                       // 0x10051870 (shared)
}

// 0x1009F6A0: `--ref`; at zero the (trivial) destructor restores both vptrs and the block goes back to
// the engine heap. The new count is the answer.
int PerlinNoise::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~PerlinNoise();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// 0x1009F630: 1 and 80035 answer `this`; 0x3709A4FF answers the IConsoleSaveable subobject when
// `this` is non-null; anything else 0.
void* PerlinNoise::queryInterface(unsigned int iid) const
{
    PerlinNoise* const self = const_cast<PerlinNoise*>(this);
    if (iid == RD9_IID_IBASE || iid == kIID_IPerlinNoise)
        return static_cast<IPerlinNoise*>(self);
    if (iid == kIID_IConsoleSaveable)
        return static_cast< ::dice::hfe::io::IConsoleSaveable*>(self);       // null-tested, `add eax, 4`
    return 0;
}

// 0x1009F460 (`xor al, al; retn 4`): the noise writes no console script.
bool PerlinNoise::makeScript(::dice::hfe::io::IStream* /*stream*/)
{
    return false;
}


// The accessors, 0x1009F000..0x1009F0C0 and 0x1009F470 - one store or one load each.

void  PerlinNoise::setSeed(int seed)                  { m_seed = seed; }
int   PerlinNoise::getSeed()                          { return m_seed; }
void  PerlinNoise::setDimension(int dimension)        { m_dimension = dimension; }
int   PerlinNoise::getDimension()                     { return m_dimension; }
void  PerlinNoise::setSmoothing(int mode)             { m_smoothing = mode; }
int   PerlinNoise::getSmoothing()                     { return m_smoothing; }
void  PerlinNoise::setInterpolation(int mode)         { m_interpolation = mode; }
int   PerlinNoise::getInterpolation()                 { return m_interpolation; }
void  PerlinNoise::setOctaves(int octaves)            { m_octaves = octaves; }
int   PerlinNoise::getOctaves()                       { return m_octaves; }
void  PerlinNoise::setPersistence(float persistence)  { m_persistence = persistence; }
float PerlinNoise::getPersistence()                   { return m_persistence; }
const Vec2& PerlinNoise::getOutputRange()             { return m_range; }

// y through the FPU, x as a dword - a plain Vec2 copy.
void PerlinNoise::setOutputRange(const Vec2& range)
{
    m_range.y = range.y;
    m_range.x = range.x;
}


// The generator and the three samplers.


// lcgrand: two Lehmer steps (24112, then 26143) on +0x0C; the new state is the answer.
int PerlinNoise::nextRandom()
{
    m_state = lcgrandHalf(lcgrandHalf(m_state, 24112), 26143);
    return m_state;
}

// Reseeds each coordinate so output depends only on seed, coordinate, range, and smoothing mode.
// Mode 0 draws once; mode 1 weights neighboring samples 1/4, 1/2, 1/4. Unknown modes report an error and return zero.
float PerlinNoise::baseNoise(int n)
{
    const int seed = m_seed + n;
    switch (m_smoothing)
    {
    case 0:
    {
        m_state = seed & 0x7FFFFFFF;
        const float lo = m_range.x;
        const float hi = m_range.y;
        const float r = draw24(nextRandom());
        return r * (hi - lo) * kTwoPowMinus24 + lo;
    }
    case 1:
    {
        m_state = seed & 0x7FFFFFFF;
        const float lo = m_range.x;
        const float hi = m_range.y;
        const float r1 = draw24(nextRandom());
        m_state = (seed - 1) & 0x7FFFFFFF;
        const float a = r1 * (hi - lo) * kTwoPowMinus25 + lo * 0.5f;
        const float r2 = draw24(nextRandom());
        m_state = (seed + 1) & 0x7FFFFFFF;
        const float b = r2 * (m_range.y - m_range.x) * kTwoPowMinus26 + m_range.x * 0.25f;
        const float sum = b + a;
        const float r3 = draw24(nextRandom());
        const float c = r3 * (m_range.y - m_range.x) * kTwoPowMinus26 + m_range.x * 0.25f;
        return c + sum;
    }
    default:
        // sub_10074300(6, __FILE__, 151, "") << "Unknown param"; sub_10074690 - dice's debug stream.
        ::renddx9abi::logf("PerlinNoise.cpp:151: Unknown param");
        return 0.0f;
    }
}

// Interpolates neighboring samples linearly, with cosine smoothing, or cubically.
// Preserve repeated cubic samples and their order because each call changes generator state.
float PerlinNoise::interpolatedNoise(int n, float t)
{
    switch (m_interpolation)
    {
    case 0:
    {
        const float a = baseNoise(n + 1) * t;
        return (1.0f - t) * baseNoise(n) + a;
    }
    case 1:
    {
        // `fmul pi` rounds; fcos does not; `fsubr 1.0` rounds once.
        const float angle = t * kPi;
        const float f = static_cast<float>(1.0 - std::cos(static_cast<double>(angle))) * 0.5f;
        const float a = baseNoise(n + 1) * f;
        return (1.0f - f) * baseNoise(n) + a;
    }
    case 2:
    {
        const float v0 = baseNoise(n - 1);
        const float d01 = v0 - baseNoise(n);                                 // P - Q's shared term
        const float v3 = baseNoise(n + 2);
        const float p = (v3 - baseNoise(n + 1)) - d01;                       // the cubic coefficient
        const float v2 = baseNoise(n + 1);
        const float r = (v2 - baseNoise(n - 1)) * t;                         // the linear term
        float result = baseNoise(n) + r;
        // _CIpow(t, 2.0) and (t, 3.0) (dbl_101DDDE8, dbl_1020D038): double results, each rounded by
        // the multiply that consumes it.
        result = result + static_cast<float>(std::pow(static_cast<double>(t), 2.0) *
                                              static_cast<double>(d01 - p));
        result = result + static_cast<float>(std::pow(static_cast<double>(t), 3.0) *
                                              static_cast<double>(p));
        return result;
    }
    default:
        ::renddx9abi::logf("PerlinNoise.cpp:175: Unknown param");
        return 0.0f;
    }
}

// Sums octave samples weighted by persistence. Nonpositive octave counts return zero.
float PerlinNoise::octaveSum(float x)
{
    float total = 0.0f;
    for (int i = 0; i < m_octaves; ++i)
    {
        const float persistence = m_persistence;
        // `shl edx, cl` - the hardware masks the count to five bits.
        const float scaled = static_cast<float>(static_cast<int>(1u << (i & 31))) * x;
        const int whole = static_cast<int>(scaled);                          // fistp, truncating
        const float fraction = scaled - static_cast<float>(whole);
        const float value = interpolatedNoise(whole, fraction);
        total = static_cast<float>(static_cast<double>(value) *
                                   std::pow(static_cast<double>(persistence),
                                            static_cast<double>(i))) + total;
    }
    return total;
}

// 0x1009F5A0 (`retn 10h`) - only the one-dimensional path exists: any non-zero +0x14 logs "Not
// implemented" (line 106) and answers 0. y, z and w are never read.
float PerlinNoise::sample(float x, float /*y*/, float /*z*/, float /*w*/)
{
    if (m_dimension != 0)
    {
        ::renddx9abi::logf("PerlinNoise.cpp:106: Not implemented");
        return 0.0f;
    }
    return octaveSum(x);
}


// RefFactory 0x1007CEF0: refuse aggregation, 0x30 bytes, construct, QI with no null test on the
// object (shipped), release on refusal.

IBase* __fastcall createPerlinNoise(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(PerlinNoise));    // 0x30 on x86
    PerlinNoise* self = 0;
    if (storage)
        self = new (storage) PerlinNoise();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [edx+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
