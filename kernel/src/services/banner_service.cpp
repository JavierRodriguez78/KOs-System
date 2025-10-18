#include <services/banner_service.hpp>
#include <console/logo.hpp>
#include <console/logger.hpp>
#include <graphics/framebuffer.hpp>

using namespace kos::console;

namespace kos { 
    namespace services {

bool BannerService::Start() {
    Logger::Log("BannerService: rendering boot logo");
    // Prefer framebuffer logo if available, else VGA text block art
    if (kos::gfx::IsAvailable()) {
        PrintLogoFramebuffer32();
    } else {
        PrintLogoBlockArt();
    }
    return true;
}

}} // namespace
