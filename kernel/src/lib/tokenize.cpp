#include <lib/tokenize.hpp>

namespace kos {
namespace lib {

int split_csv(int8_t* buf, const char** arr, int maxItems) {
    if (!buf) return 0;
    int n = 0; int8_t* p = buf;
    // trim leading spaces
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    while (*p && n < maxItems) {
        // mark start
        arr[n++] = (const char*)p;
        // scan to comma or end
        while (*p && *p != ',' && *p != '\n' && *p != '\r') ++p;
        if (!*p) break;
        *p++ = 0;
        // skip spaces
        while (*p == ' ' || *p == '\t') ++p;
    }
    return n;
}

int split_args(int8_t* line, const int8_t** argv, int maxArgs) {
    int argc = 0;
    int8_t* p = line;
    // Trim leading spaces
    while (*p == ' ' || *p == '\t') ++p;
    while (*p && argc < maxArgs) {
        argv[argc++] = p;
        // advance to next space or end
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (!*p) break;
        *p++ = 0;
        while (*p == ' ' || *p == '\t') ++p; // skip spaces
    }
    return argc;
}

} // namespace lib
} // namespace kos
