#include <graphics/framebuffer.hpp>

using namespace kos::common;

namespace kos { 
    namespace gfx {

        static FramebufferInfo g_fb = {};
        static bool g_fb_ready = false;

        // Multiboot2 constants
        static const uint32_t MULTIBOOT2_MAGIC = 0x36d76289;
        static const uint32_t MB2_TAG_TYPE_END = 0;
        static const uint32_t MB2_TAG_TYPE_FRAMEBUFFER = 8;

        // Multiboot2 tag header
        struct mb2_tag { uint32_t type; uint32_t size; };

        // We only need the initial part of framebuffer tag
        struct mb2_tag_framebuffer {
            uint32_t type; // =8
            uint32_t size;
            uint64_t framebuffer_addr;
            uint32_t framebuffer_pitch;
            uint32_t framebuffer_width;
            uint32_t framebuffer_height;
            uint8_t framebuffer_bpp;
            uint8_t framebuffer_type; // 0 palette,1 RGB,2 text
            uint16_t reserved;
        };

        struct mb2_tag_framebuffer_rgb {
            uint8_t red_pos, red_size;
            uint8_t green_pos, green_size;
            uint8_t blue_pos, blue_size;
        };

        void InitFromMultiboot(const void* mb_info, uint32_t magic) {
            g_fb_ready = false;
            if (!mb_info) return;
            if (magic != MULTIBOOT2_MAGIC) {
                // Could add Multiboot1 framebuffer parsing later
                return;
            }
            const uint8_t* p = (const uint8_t*)mb_info;
            // First 8 bytes: total size and reserved; then tags
            uint32_t total_size = *(const uint32_t*)p; (void)total_size;
            p += 8;
            while (true) {
                const mb2_tag* tag = (const mb2_tag*)p;
                if (tag->type == MB2_TAG_TYPE_END) break;
                if (tag->type == MB2_TAG_TYPE_FRAMEBUFFER) {
                    const mb2_tag_framebuffer* fb = (const mb2_tag_framebuffer*)p;
                    g_fb.addr = fb->framebuffer_addr;
                    g_fb.pitch = fb->framebuffer_pitch;
                    g_fb.width = fb->framebuffer_width;
                    g_fb.height = fb->framebuffer_height;
                    g_fb.bpp = fb->framebuffer_bpp;
                    g_fb.type = fb->framebuffer_type;
                    g_fb.red_pos = g_fb.red_size = g_fb.green_pos = g_fb.green_size = g_fb.blue_pos = g_fb.blue_size = 0;
                    if (g_fb.type == 1) {
                        // RGB: next bytes are RGB layout
                        const mb2_tag_framebuffer_rgb* rgb = (const mb2_tag_framebuffer_rgb*)(fb + 1);
                        g_fb.red_pos = rgb->red_pos; g_fb.red_size = rgb->red_size;
                        g_fb.green_pos = rgb->green_pos; g_fb.green_size = rgb->green_size;
                        g_fb.blue_pos = rgb->blue_pos; g_fb.blue_size = rgb->blue_size;
                    }
                    g_fb_ready = true;
                    break;
                }
                // advance to next tag (8-byte aligned)
                uint32_t size = tag->size;
                p += (size + 7) & ~7u;
            }
        }
    

        bool IsAvailable() { return g_fb_ready && g_fb.type == 1 && g_fb.bpp == 32; }

        const FramebufferInfo& GetInfo() { return g_fb; }

        void Clear32(uint32_t rgba) {
            if (!IsAvailable()) return;
            // 32-bit kernel: physical address fits in 32 bits
            uint8_t* base = (uint8_t*)(uint32_t)g_fb.addr;
            for (uint32_t y = 0; y < g_fb.height; ++y) {
                uint32_t* row = (uint32_t*)(base + y * g_fb.pitch);
                for (uint32_t x = 0; x < g_fb.width; ++x) row[x] = rgba;
            }
        }

        static inline uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b) {
            // Assume 8:8:8:8 ARGB little-endian; place into fb based on positions if needed
            // For now, write as 0xAARRGGBB; many VBE framebuffers are X8R8G8B8
            return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | (uint32_t(b));
        }

        void PutPixel32(uint32_t x, uint32_t y, uint32_t rgba) {
            if (!IsAvailable()) return;
            if (x >= g_fb.width || y >= g_fb.height) return;
            // 32-bit kernel: physical address fits in 32 bits
            uint8_t* base = (uint8_t*)(uint32_t)g_fb.addr;
            uint32_t* row = (uint32_t*)(base + y * g_fb.pitch);
            row[x] = rgba;
        }

    }
}
