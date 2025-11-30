#include <kernel/boot_options.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <common/panic.hpp>
#include <common/types.hpp>

using namespace kos;
using namespace kos::kernel;
using namespace kos::lib;
using namespace kos::common;

namespace {
    constexpr uint32_t MB1_MAGIC = 0x2BADB002u;
    constexpr uint32_t MB2_MAGIC = 0x36d76289u;

    constexpr uint8_t MOUSE_POLL_NEVER = 0;
    constexpr uint8_t MOUSE_POLL_ONCE  = 1;
    constexpr uint8_t MOUSE_POLL_ALWAYS = 2;


    static uint8_t parseMousePollMode(const char* opt, uint8_t def) {
        if (!opt) return def;
        if (String::strcmp((const int8_t*)opt, (const int8_t*)"mouse_poll=", 11) != 0) return def;
        const char* val = opt + 11;
        if (String::strcmp((const int8_t*)val, (const int8_t*)"never", 5) == 0) return MOUSE_POLL_NEVER; 
        if (String::strcmp((const int8_t*)val, (const int8_t*)"once", 4) == 0) return MOUSE_POLL_ONCE;
        if (String::strcmp((const int8_t*)val, (const int8_t*)"always", 6) == 0) return MOUSE_POLL_ALWAYS;
        return def;
    }

    static uint8_t applyCmdlineFlags(const char* cmd, uint8_t currentMouseMode) {
        if (!cmd) return currentMouseMode;
        if (String::strstr(cmd, "debug") || String::strstr(cmd, "log=debug")) {
            Logger::SetDebugEnabled(true);
            Logger::Log("Debug mode enabled via boot param");
        }
        if (String::strstr(cmd, "panic=reboot") || String::strstr(cmd, "reboot_on_panic")) {
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
        struct MultibootInfoMinimal { 
            uint32_t flags; 
            uint32_t mem_lower; 
            uint32_t mem_upper; 
            uint32_t boot_device; 
            uint32_t cmdline; 
        };
        const MultibootInfoMinimal* mbi = (const MultibootInfoMinimal*)mb_info;
        if (mbi->flags & (1u << 2)) {
            const char* cmd = (const char*)(uintptr_t)mbi->cmdline;
            if (cmd) {
                opts.mousePollMode = applyCmdlineFlags(cmd, opts.mousePollMode);
                // Parse display mode: look for "mode=text" or "mode=graphics"
                if (String::strstr(cmd, "mode=text")) opts.mode = DisplayMode::Text;
                else if (String::strstr(cmd, "mode=graphics")) opts.mode = DisplayMode::Graphics;
            }
        }
        return opts;
    }

    // Multiboot v2: walk tags until cmdline (type 1)
    if (magic == MB2_MAGIC && mb_info) {
        const uint8_t* p = (const uint8_t*)mb_info;
        p += 8; // skip total_size + reserved
        while (true) {
            struct Tag { uint32_t type; uint32_t size; };
            const Tag* tag = (const Tag*)p;
            if (tag->type == 0) break;
            if (tag->type == 1) {
                const char* cmd = (const char*)(p + sizeof(Tag));
                if (cmd) {
                    opts.mousePollMode = applyCmdlineFlags(cmd, opts.mousePollMode);
                    if (String::strstr(cmd, "mode=text")) opts.mode = DisplayMode::Text;
                    else if (String::strstr(cmd, "mode=graphics")) opts.mode = DisplayMode::Graphics;
                }
                break;
            }
            uint32_t size = tag->size;
            p += (size + 7) & ~7u;
        }
    }

    return opts;
}
