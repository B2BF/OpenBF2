// [TEMP] Platform/X87Compat.h:42 (game frontier closure): compiled with __LDBL_MANT_DIG__ forced to 53,
// as on a binary64 long-double port, X87Compat must select the soft extended Real instead of the
// former abort, and that Real must keep x87 precision (1 + 2^-60 survives, which binary64 cannot).
#include "../BF2/Platform/X87Compat.h"
#include <type_traits>

bool x87NarrowLongDoubleSelectsSoftExtended()
{
#if !defined(BF2X87_SOFT_EXTENDED)
    return false;
#else
    if (!std::is_same<bf2x87::Real, bf2x87::ExtendedFloat>::value) return false;
    const bf2x87::Real one = bf2x87::wide(1.0);
    const bf2x87::Real tiny = bf2x87::div(one, bf2x87::wide(1152921504606846976.0));   // 2^60
    const bf2x87::Real sum = bf2x87::add(one, tiny);
    return bf2x87::greater(sum, one) && bf2x87::store(sum) == 1.0f;
#endif
}
