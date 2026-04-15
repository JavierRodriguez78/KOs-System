#include <services/banner_service.hpp>
#include <console/logo.hpp>
#include <console/logger.hpp>
#include <graphics/framebuffer.hpp>
#include <kernel/globals.hpp>

using namespace kos::console;

namespace kos { 
    namespace services {

bool BannerService::Start() {
    Logger::Log("BannerService: rendering boot logo");
    // In graphics mode the WindowManager/Compositor owns the framebuffer.
    // Skip the legacy direct-framebuffer banner path here because it can block
    // or fault after the compositor has initialized its own mapped access.
    if (kos::g_display_mode == kos::kernel::DisplayMode::Graphics && kos::gfx::IsAvailable()) {
        Logger::Log("BannerService: skipped in graphics mode");
        return true;
    }

    // Text mode keeps the legacy banner behavior.
    if (kos::gfx::IsAvailable()) {
        PrintLogoFramebuffer32();
    } else {
        PrintLogoBlockArt();
    }
    return true;
}

}} // namespace
