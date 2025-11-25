#ifndef KOS_GRAPHICS_FRAMEBUFFER_HPP
#define KOS_GRAPHICS_FRAMEBUFFER_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace gfx {

        /*
        *@brief Framebuffer information structure
        */
        struct FramebufferInfo {
            uint64_t addr; // physical address
            uint32_t pitch; // bytes per scanline
            uint32_t width; // pixels
            uint32_t height; // pixels  
            uint8_t bpp; // bits per pixel
            uint8_t type; // 0=indexed,1=RGB,2=EGA text
            // RGB layout (valid when type == 1)
            uint8_t red_pos, red_size;// in bits
            uint8_t green_pos, green_size;// in bits
            uint8_t blue_pos, blue_size;// in bits
        };

        /*
        *@brief Initialize framebuffer from Multiboot info
        *@param mb_info Pointer to Multiboot info structure
        *@param magic Multiboot magic number
        * If Multiboot2 framebuffer tag not found, attempts Multiboot v1 VBE fallback.
        * On success, framebuffer info is stored internally and IsAvailable() will return true.
        */
        void InitFromMultiboot(const void* mb_info, uint32_t magic);


        /*
        *@brief Check if framebuffer is available and initialized
        *@return true if framebuffer is available (RGB 24bpp or 32bpp), false otherwise
        */  
        bool IsAvailable();
        
        /*
        *@brief Get framebuffer information
        *@return Reference to FramebufferInfo structure
        */
        const FramebufferInfo& GetInfo();

        
        /*
        *@brief 32bpp helpers (no-op if unavailable or not 32bpp RGB)
        * @param    rgba 32-bit color value in 0xAARRGGBB format
        */
        void Clear32(uint32_t rgba);

        /*
        *@brief Put a pixel at (x,y) with 32bpp color   
        *@param x X coordinate
        *@param y Y coordinate
        *@param rgba 32-bit color value in 0xAARRGGBB format
        */
        void PutPixel32(uint32_t x, uint32_t y, uint32_t rgba);
    } 
}
#endif // KOS_GRAPHICS_FRAMEBUFFER_HPP
