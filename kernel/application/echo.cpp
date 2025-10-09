#include <sys/api.hpp>
#include <common/types.hpp>
using namespace kos::common;
using namespace kos::sys;
#include "app.hpp"

extern "C" void app_echo() {
    puts((int8_t*)"Echo app says hi.\n");
}

// Standalone entry for the .elf binary build
#ifndef APP_EMBED
extern "C" int main() {
    app_echo();
    return 0;
}
#endif
