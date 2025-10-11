#ifndef KOS_GRAPHICS_FRAMEBUFFER_HPP
#define KOS_GRAPHICS_FRAMEBUFFER_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace gfx {

        struct FramebufferInfo {
            uint64_t addr; // physical address
            uint32_t pitch;
            uint32_t width;
            uint32_t height;
            uint8_t bpp;
            uint8_t type; // 0=indexed,1=RGB,2=EGA text
            // RGB layout (valid when type == 1)
            uint8_t red_pos, red_size;
            uint8_t green_pos, green_size;
            uint8_t blue_pos, blue_size;
        };

        // Initialize framebuffer info from multiboot magic and info pointer. Supports Multiboot2 framebuffer tag.
        void InitFromMultiboot(const void* mb_info, uint32_t magic);

        bool IsAvailable();
        const FramebufferInfo& GetInfo();

        // 32bpp helpers (no-op if unavailable or not 32bpp RGB)
        void Clear32(uint32_t rgba);
        void PutPixel32(uint32_t x, uint32_t y, uint32_t rgba);
    } 
}
#endif // KOS_GRAPHICS_FRAMEBUFFER_HPP
