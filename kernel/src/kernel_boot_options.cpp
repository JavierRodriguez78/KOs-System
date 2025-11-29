#include <kernel/boot_options.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <common/panic.hpp>

using namespace kos;
using namespace kos::kernel;
using namespace kos::lib;

namespace {
constexpr kos::common::uint32_t MB1_MAGIC = 0x2BADB002u;
constexpr kos::common::uint32_t MB2_MAGIC = 0x36d76289u;

constexpr kos::common::uint8_t MOUSE_POLL_NEVER = 0;
constexpr kos::common::uint8_t MOUSE_POLL_ONCE  = 1;
constexpr kos::common::uint8_t MOUSE_POLL_ALWAYS = 2;

static bool containsSubstring(const char* hay, const char* needle) {
    if (!hay || !needle) return false;
    kos::common::uint32_t nlen = String::strlen((const int8_t*)needle);
    for (kos::common::uint32_t i = 0; hay[i]; ++i) {
        if (String::strcmp((const int8_t*)(hay + i), (const int8_t*)needle, nlen) == 0) return true;
    }
    return false;
}

static kos::common::uint8_t parseMousePollMode(const char* opt, kos::common::uint8_t def) {
    if (!opt) return def;
    if (String::strcmp((const int8_t*)opt, (const int8_t*)"mouse_poll=", 11) != 0) return def;
    const char* val = opt + 11;
    if (String::strcmp((const int8_t*)val, (const int8_t*)"never", 5) == 0) return MOUSE_POLL_NEVER;
    if (String::strcmp((const int8_t*)val, (const int8_t*)"once", 4) == 0) return MOUSE_POLL_ONCE;
    if (String::strcmp((const int8_t*)val, (const int8_t*)"always", 6) == 0) return MOUSE_POLL_ALWAYS;
    return def;
}

static kos::common::uint8_t applyCmdlineFlags(const char* cmd, kos::common::uint8_t currentMouseMode) {
    if (!cmd) return currentMouseMode;
    if (containsSubstring(cmd, "debug") || containsSubstring(cmd, "log=debug")) {
        Logger::SetDebugEnabled(true);
        Logger::Log("Debug mode enabled via boot param");
    }
    if (containsSubstring(cmd, "panic=reboot") || containsSubstring(cmd, "reboot_on_panic")) {
        kos::kernel::SetPanicReboot(true);
        Logger::Log("Panic: reboot-on-panic enabled via boot param");
    }
    for (const char* p = cmd; *p; ++p) {
        if (*p == 'm') currentMouseMode = parseMousePollMode(p, currentMouseMode);
    }
    switch (currentMouseMode) {
        case MOUSE_POLL_NEVER:  Logger::Log("Boot param: mouse_poll=never"); break;
        case MOUSE_POLL_ONCE:   Logger::Log("Boot param: mouse_poll=once"); break;
        case MOUSE_POLL_ALWAYS: Logger::Log("Boot param: mouse_poll=always"); break;
    }
    return currentMouseMode;
}

} // anonymous

BootOptions::BootOptions() : mousePollMode(MOUSE_POLL_ALWAYS), debugEnabled(false), rebootOnPanic(false) {}

BootOptions BootOptions::ParseFromMultiboot(const void* mb_info, kos::common::uint32_t magic) {
    BootOptions opts;
    opts.mode = DisplayMode::Graphics; // default to graphics if available later

    // Multiboot v1: parse minimal info struct for cmdline
    if (magic == MB1_MAGIC && mb_info) {
        struct MultibootInfoMinimal { kos::common::uint32_t flags; kos::common::uint32_t mem_lower; kos::common::uint32_t mem_upper; kos::common::uint32_t boot_device; kos::common::uint32_t cmdline; };
        const MultibootInfoMinimal* mbi = (const MultibootInfoMinimal*)mb_info;
        if (mbi->flags & (1u << 2)) {
            const char* cmd = (const char*)(uintptr_t)mbi->cmdline;
            if (cmd) {
                opts.mousePollMode = applyCmdlineFlags(cmd, opts.mousePollMode);
                // Parse display mode: look for "mode=text" or "mode=graphics"
                if (containsSubstring(cmd, "mode=text")) opts.mode = DisplayMode::Text;
                else if (containsSubstring(cmd, "mode=graphics")) opts.mode = DisplayMode::Graphics;
            }
        }
        return opts;
    }

    // Multiboot v2: walk tags until cmdline (type 1)
    if (magic == MB2_MAGIC && mb_info) {
        const uint8_t* p = (const uint8_t*)mb_info;
        p += 8; // skip total_size + reserved
        while (true) {
            struct Tag { kos::common::uint32_t type; kos::common::uint32_t size; };
            const Tag* tag = (const Tag*)p;
            if (tag->type == 0) break;
            if (tag->type == 1) {
                const char* cmd = (const char*)(p + sizeof(Tag));
                if (cmd) {
                    opts.mousePollMode = applyCmdlineFlags(cmd, opts.mousePollMode);
                    if (containsSubstring(cmd, "mode=text")) opts.mode = DisplayMode::Text;
                    else if (containsSubstring(cmd, "mode=graphics")) opts.mode = DisplayMode::Graphics;
                }
                break;
            }
            kos::common::uint32_t size = tag->size;
            p += (size + 7) & ~7u;
        }
    }

    return opts;
}
