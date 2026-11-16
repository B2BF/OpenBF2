// Shared folded debug-mesh bodies. [INFERRED] Header placement.
#ifndef BF2_GEOMMISC_DEBUGMESHCOMMON_H
#define BF2_GEOMMISC_DEBUGMESHCOMMON_H

#include "../Geom/GeometryInstanceData.h"
#include "../Math/LocalBoundingBox.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include <cfloat>
#include <cmath>
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif

namespace dice { namespace hfe { namespace geom { namespace debugmesh {
// PE 0x10037D40 / 0x100E13B0 / 0x100E1480 / 0x10083AB0 / 0x10083A80 / 0x10082D20.
inline void RD9_THUNK empty0(void* RD9_THUNK_EDX) {}
inline void RD9_THUNK empty1(void* RD9_THUNK_EDX, unsigned int) {}
inline void RD9_THUNK empty2(void* RD9_THUNK_EDX, unsigned int, unsigned int) {}
inline void RD9_THUNK empty3(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) {}
inline void RD9_THUNK empty4(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int, unsigned int) {}
inline void RD9_THUNK empty6(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int,
                            unsigned int, unsigned int, unsigned int) {}
// PE 0x100ED3B0 / 0x10088060 / 0x10083AC0 / 0x10083AA0.
inline int RD9_THUNK zero0(void* RD9_THUNK_EDX) { return 0; }
inline int RD9_THUNK zero1(void* RD9_THUNK_EDX, unsigned int) { return 0; }
inline int RD9_THUNK zero2(void* RD9_THUNK_EDX, unsigned int, unsigned int) { return 0; }
inline int RD9_THUNK zero3(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 0; }
// PE 0x10083AF0 / 0x10083AD0 / 0x10186100 / 0x10082D40 / 0x10082D50 / 0x10083AE0.
inline bool RD9_THUNK false0(void* RD9_THUNK_EDX) { return false; }
inline bool RD9_THUNK true0(void* RD9_THUNK_EDX) { return true; }
inline int RD9_THUNK one(void* RD9_THUNK_EDX) { return 1; }
inline int RD9_THUNK minusOne(void* RD9_THUNK_EDX) { return -1; }
inline int RD9_THUNK seven(void* RD9_THUNK_EDX) { return 7; }
inline int RD9_THUNK sixteen(void* RD9_THUNK_EDX) { return 16; }
// PE 0x10082D10 / 0x10083A60 / 0x10083A70 / 0x10083A90.
inline float RD9_THUNK minusOneFloat(void* RD9_THUNK_EDX) { return -1.0f; }
inline float RD9_THUNK lodDistance(void* RD9_THUNK_EDX, unsigned int, unsigned int) { return -1.0f; }
inline float RD9_THUNK materialValue(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 15.0f; }
inline float RD9_THUNK quarter(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 0.25f; }
// PE 0x10083B00.
inline Vec3* RD9_THUNK decompress(void* RD9_THUNK_EDX, Vec3* out, const Vec3*)
{ *out = Vec3::getIdentity(); return out; }
// PE 0x10082D30.
inline void RD9_THUNK setName(GeometryInstanceData* s RD9_THUNK_EDX, const std::string& v) { s->m_name = v; }

// PE 0x10037DC0. [INFERRED] Name; tolerance is inclusive and NaNs are rejected.
inline bool equalVector(const Vec3& value, const Vec3& other)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the unspilled x87 subtractions on native Apple.
    using namespace bf2x87;
    return lessEqual(bf2x87::abs(sub(wide(value.x), wide(other.x))), wide(FLT_EPSILON)) &&
           lessEqual(bf2x87::abs(sub(wide(value.y), wide(other.y))), wide(FLT_EPSILON)) &&
           lessEqual(bf2x87::abs(sub(wide(value.z), wide(other.z))), wide(FLT_EPSILON));
#else
    return std::fabs(value.x - other.x) <= FLT_EPSILON &&
           std::fabs(value.y - other.y) <= FLT_EPSILON &&
           std::fabs(value.z - other.z) <= FLT_EPSILON;
#endif
}

} } } }
#endif
