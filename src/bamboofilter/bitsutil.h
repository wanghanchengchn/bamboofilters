
#pragma once

#include <stdint.h>

// inspired from
// http://www-graphics.stanford.edu/~seander/bithacks.html#ZeroInWord

#define haszero12(x) (((x)-0x001001001001ULL) & (~(x)) & 0x800800800800ULL)
#define hasvalue12(x, n) (haszero12((x) ^ (0x001001001001ULL * (n))))

inline uint64_t upperpower2(uint64_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}
