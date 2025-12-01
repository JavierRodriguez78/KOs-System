#include <kernel/multiboot_kernel.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <common/types.hpp>
#include <kernel/boot_options.hpp>
#include <kernel/globals.hpp>

using namespace kos;
using namespace kos::kernel;

MultibootKernel::MultibootKernel(const void* mb_info_, kos::common::uint32_t magic_)
    : mb_info(mb_info_), magic(magic_), mousePollMode(2), memLowerKB(0), memUpperKB(0) {}

void MultibootKernel::Init()
{
    // Let gfx subsystem probe framebuffer info first (it handles MB1/MB2)
    kos::gfx::InitFromMultiboot(mb_info, magic);

    // If a framebuffer is available, clear to a neutral dark wallpaper color.
    // ARGB 0xFF1E1E20 — matches compositor wallpaper and avoids a green flash on boot.
    if (kos::gfx::IsAvailable()) {
        Logger::Log("Framebuffer (32bpp) detected; initializing graphics background to dark gray");
        kos::gfx::Clear32(0xFF1E1E20u);
    }

    // Default mouse poll mode
    mousePollMode = 2;

    // Parse Multiboot v1 mem fields if present and keep mem info
    if (magic == 0x2BADB002 && mb_info) {
        struct MultibootInfoMinimal { kos::common::uint32_t flags; kos::common::uint32_t mem_lower; kos::common::uint32_t mem_upper; kos::common::uint32_t boot_device; kos::common::uint32_t cmdline; };
        const MultibootInfoMinimal* mbi = (const MultibootInfoMinimal*)mb_info;
        if (mbi->flags & 1) { memLowerKB = mbi->mem_lower; memUpperKB = mbi->mem_upper; }
    }

    // Parse boot options (cmdline flags) via BootOptions helper
    BootOptions opts = BootOptions::ParseFromMultiboot(mb_info, magic);
    mousePollMode = opts.MousePollMode();
    // Update global for alternative service registration path
    kos::g_mouse_poll_mode = mousePollMode;
}
