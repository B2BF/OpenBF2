// This source code has been donated to the Public Domain.

#ifndef TU_HASH_CAPACITY_H
#define TU_HASH_CAPACITY_H

#include <stdint.h>

namespace tu_util {

// Returns the raw bucket-count bits for a positive signed 32-bit request.
// The table reproduces SwiffPlayer's 24-bit, nearest-rounding arithmetic;
// inherited 53/64-bit precision and directed rounding have different tables.
inline uint32_t hash_raw_capacity(int requested_size)
{
    static_assert(sizeof(int) == 4, "SwiffPlayer capacity requests are 32-bit");
    static const int transitions[] = {
        2, 3, 5, 9, 17, 33, 65, 129, 257, 513, 1025, 2049,
        4097, 8193, 16385, 32769, 65537, 131073, 262145, 524289,
        1048577, 2097153, 4194304, 8388606, 16777210, 33554419,
        67108837, 134217674, 268435350, 536870703, 1073741414, 2147482846
    };
    unsigned lo = 0, hi = sizeof(transitions) / sizeof(transitions[0]);
    while (lo < hi)
    {
        const unsigned mid = lo + (hi - lo) / 2;
        if (requested_size < transitions[mid]) hi = mid;
        else lo = mid + 1;
    }
    return uint32_t(1) << (lo & 31u);
}

} // namespace tu_util

#endif
