#include <console/tty.hpp>
#include <common/types.hpp>
#include "app.hpp"

using namespace kos::console;
using namespace kos::common;

extern "C" void app_hello() {
    TTY tty;
    tty.Write((int8_t*)"Hello from application hello!\n");
}

// Standalone entry for the .elf binary build
#ifndef APP_EMBED
extern "C" int main() {
    app_hello();
    return 0;
}
#endif
