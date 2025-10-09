#include <sys/api.hpp>
#include <common/types.hpp>
using namespace kos::common;
using namespace kos::sys;
#include "app.hpp"

extern "C" void app_ls() {
    puts((int8_t*)"Listing root directory:\n");
    listroot();
}

// Standalone entry for the .elf binary build
#ifndef APP_EMBED
extern "C" int main() {
    app_ls();
    return 0;
}
#endif
