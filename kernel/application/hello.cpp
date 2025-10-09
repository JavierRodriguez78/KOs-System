#include <sys/api.hpp>
#include <common/types.hpp>
using namespace kos::common;
using namespace kos::sys;
#include "app.hpp"

extern "C" void app_hello() {
    printf((int8_t*)"Hello from %s %d 0x%X!\n", (int8_t*)"application hello", 42, 0xBEEF);
}

// Standalone entry for the .elf binary build
#ifndef APP_EMBED
extern "C" int main() {
    app_hello();
    return 0;
}
#endif
