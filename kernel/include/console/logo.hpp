#ifndef KOS_CONSOLE_LOGO_HPP
#define KOS_CONSOLE_LOGO_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace console {

        // Render the KOS logo in VGA text mode using colored block art
        void PrintLogoBlockArt();
        // Variant that lets you control sampling steps (bigger steps => smaller logo)
        void PrintLogoBlockArtScaled(uint32_t xStep, uint32_t yStep);
        // Render the KOS logo to framebuffer in 32-bpp RGB, scaled to 335x212 if available
        void PrintLogoFramebuffer32();
        // Variant that renders to framebuffer at a requested width/height (nearest neighbor)
        void PrintLogoFramebuffer32Scaled(uint32_t targetW, uint32_t targetH);

    }
}

#endif // KOS_CONSOLE_LOGO_HPP
