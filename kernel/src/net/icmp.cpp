#include <net/icmp.hpp>

using namespace kos::common;

namespace kos { namespace net { namespace icmp {

static inline uint16_t add16(uint32_t sum) {
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)sum;
}

uint16_t checksum(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        uint16_t w = (uint16_t)((p[0] << 8) | p[1]);
        sum += w;
        p += 2; len -= 2;
    }
    if (len == 1) {
        sum += (uint16_t)(p[0] << 8);
    }
    uint16_t s = add16(sum);
    return (uint16_t)~s;
}

uint32_t build_echo_request(uint16_t ident,
                            uint16_t seq,
                            const void* payload,
                            uint16_t plen,
                            void* out,
                            uint32_t outMax) {
    uint32_t hdrLen = sizeof(EchoHeader);
    uint32_t total = hdrLen + (uint32_t)plen;
    if (outMax < total) return 0;
    EchoHeader* h = (EchoHeader*)out;
    h->type = ICMP_ECHO_REQUEST;
    h->code = 0;
    h->checksum = 0;
    h->ident = ident;
    h->seq = seq;
    uint8_t* dst = ((uint8_t*)out) + hdrLen;
    const uint8_t* src = (const uint8_t*)payload;
    for (uint16_t i = 0; i < plen; ++i) dst[i] = src ? src[i] : 0;
    // Compute checksum over header+payload
    h->checksum = checksum(out, total);
    return total;
}

}}}
