#include <common/types.hpp>

using namespace kos::common;

namespace {

static uint64_t udiv64_core(uint64_t n, uint64_t d, uint64_t* rem_out) {
    if (d == 0) {
        if (rem_out) *rem_out = 0;
        return 0;
    }

    uint64_t q = 0;
    uint64_t r = 0;

    // Classic restoring division: no dependency on libgcc helper routines.
    for (int i = 63; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1ULL);
        if (r >= d) {
            r -= d;
            q |= (1ULL << i);
        }
    }

    if (rem_out) *rem_out = r;
    return q;
}

static uint64_t abs_to_u64(int64_t v) {
    uint64_t uv = (uint64_t)v;
    if (v < 0) {
        uv = (~uv) + 1ULL;
    }
    return uv;
}

} // namespace

extern "C" uint64_t __udivdi3(uint64_t n, uint64_t d) {
    return udiv64_core(n, d, nullptr);
}

extern "C" int64_t __divdi3(int64_t a, int64_t b) {
    if (b == 0) return 0;

    bool neg = ((a < 0) != (b < 0));
    uint64_t ua = abs_to_u64(a);
    uint64_t ub = abs_to_u64(b);

    uint64_t uq = udiv64_core(ua, ub, nullptr);
    int64_t q = (int64_t)uq;

    if (neg) {
        uint64_t u = (uint64_t)q;
        u = (~u) + 1ULL;
        q = (int64_t)u;
    }

    return q;
}

extern "C" uint64_t __umoddi3(uint64_t n, uint64_t d) {
    uint64_t r = 0;
    (void)udiv64_core(n, d, &r);
    return r;
}

extern "C" int64_t __moddi3(int64_t a, int64_t b) {
    if (b == 0) return 0;

    uint64_t ua = abs_to_u64(a);
    uint64_t ub = abs_to_u64(b);
    uint64_t ur = 0;
    (void)udiv64_core(ua, ub, &ur);

    int64_t r = (int64_t)ur;
    if (a < 0) {
        uint64_t u = (uint64_t)r;
        u = (~u) + 1ULL;
        r = (int64_t)u;
    }

    return r;
}
