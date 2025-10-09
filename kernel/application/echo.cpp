#include <console/tty.hpp>
#include <common/types.hpp>
#include "app.hpp"

using namespace kos::console;
using namespace kos::common;

extern "C" void app_echo() {
    TTY tty;
    tty.Write((int8_t*)"Echo app says hi.\n");
}

// Standalone entry for the .elf binary build
#ifndef APP_EMBED
extern "C" int main() {
    app_echo();
    return 0;
}
#endif
