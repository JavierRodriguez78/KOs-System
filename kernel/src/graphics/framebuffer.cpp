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

        // Multiboot v1 minimal info header (subset we care about)
        struct multiboot1_info {
            uint32_t flags;
            uint32_t mem_lower; uint32_t mem_upper;
            uint32_t boot_device; uint32_t cmdline;
            uint32_t mods_count; uint32_t mods_addr;
            uint32_t syms[4];
            uint32_t mmap_length; uint32_t mmap_addr;
            uint32_t drives_length; uint32_t drives_addr;
            uint32_t config_table; uint32_t boot_loader_name;
            uint32_t apm_table;
            uint32_t vbe_control_info; // phys addr
            uint32_t vbe_mode_info;     // phys addr
            uint16_t vbe_mode;
            uint16_t vbe_interface_seg; uint16_t vbe_interface_off; uint16_t vbe_interface_len;
        } __attribute__((packed));

        // VBE mode info block (subset) – offsets per VBE spec
        struct vbe_mode_info {
            uint16_t attributes;
            uint8_t  winA, winB;
            uint16_t granularity; uint16_t winsize;
            uint16_t segmentA, segmentB;
            uint32_t realFctPtr;
            uint16_t pitch; // bytes per scan line
            uint16_t XResolution; uint16_t YResolution;
            uint8_t  XCharSize; uint8_t YCharSize;
            uint8_t  numberOfPlanes; uint8_t bitsPerPixel;
            uint8_t  numberOfBanks; uint8_t memoryModel;
            uint8_t  bankSize; uint8_t numberOfImagePages; uint8_t reserved1;
            uint8_t  redMaskSize; uint8_t redFieldPosition;
            uint8_t  greenMaskSize; uint8_t greenFieldPosition;
            uint8_t  blueMaskSize; uint8_t blueFieldPosition;
            uint8_t  reservedMaskSize; uint8_t reservedFieldPosition; uint8_t directColorModeInfo;
            // Skip rest up to physbase (we only need physbase)
            uint32_t physbase;
            // The real structure is larger; we ignore remaining fields.
        } __attribute__((packed));

        void InitFromMultiboot(const void* mb_info, uint32_t magic) {
            g_fb_ready = false;
            if (!mb_info) return;
            if (magic != MULTIBOOT2_MAGIC) {
                // Attempt Multiboot v1 VBE fallback (magic 0x2BADB002)
                if (magic == 0x2BADB002) {
                    const multiboot1_info* mbi = (const multiboot1_info*)mb_info;
                    // VBE info present if flags bit 11 set
                    if (mbi->flags & (1u << 11)) {
                        const vbe_mode_info* vmi = (const vbe_mode_info*)(uintptr_t)mbi->vbe_mode_info;
                        if (vmi) {
                            // Only accept 32bpp linear framebuffer
                            if (vmi->bitsPerPixel == 32) {
                                g_fb.addr = vmi->physbase;
                                g_fb.pitch = vmi->pitch;
                                g_fb.width = vmi->XResolution;
                                g_fb.height = vmi->YResolution;
                                g_fb.bpp = vmi->bitsPerPixel;
                                g_fb.type = 1; // RGB
                                // Basic RGB masks (common 8:8:8)
                                g_fb.red_pos = vmi->redFieldPosition; g_fb.red_size = vmi->redMaskSize;
                                g_fb.green_pos = vmi->greenFieldPosition; g_fb.green_size = vmi->greenMaskSize;
                                g_fb.blue_pos = vmi->blueFieldPosition; g_fb.blue_size = vmi->blueMaskSize;
                                if (g_fb.pitch == 0) g_fb.pitch = g_fb.width * 4; // fallback
                                g_fb_ready = true;
                            }
                        }
                    }
                }
                // If not multiboot2 and v1 fallback failed, return.
                if (!g_fb_ready) return;
            }
            if (magic != MULTIBOOT2_MAGIC) return; // already handled v1
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
    

    bool IsAvailable() { return g_fb_ready && g_fb.type == 1 && (g_fb.bpp == 32 || g_fb.bpp == 24); }

        const FramebufferInfo& GetInfo() { return g_fb; }

        void Clear32(uint32_t rgba) {
            if (!IsAvailable()) return;
            uint8_t* base = (uint8_t*)(uint32_t)g_fb.addr;
            if (g_fb.bpp == 32) {
                for (uint32_t y = 0; y < g_fb.height; ++y) {
                    uint32_t* row = (uint32_t*)(base + y * g_fb.pitch);
                    for (uint32_t x = 0; x < g_fb.width; ++x) row[x] = rgba;
                }
            } else if (g_fb.bpp == 24) {
                uint8_t r = (rgba >> 16) & 0xFF;
                uint8_t g = (rgba >> 8) & 0xFF;
                uint8_t b = (rgba) & 0xFF;
                for (uint32_t y = 0; y < g_fb.height; ++y) {
                    uint8_t* row = base + y * g_fb.pitch;
                    for (uint32_t x = 0; x < g_fb.width; ++x) {
                        uint8_t* p = row + x * 3;
                        // Assume VBE 24bpp BGR order
                        p[0] = b; p[1] = g; p[2] = r;
                    }
                }
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
            uint8_t* base = (uint8_t*)(uint32_t)g_fb.addr;
            if (g_fb.bpp == 32) {
                uint32_t* row = (uint32_t*)(base + y * g_fb.pitch);
                row[x] = rgba;
            } else if (g_fb.bpp == 24) {
                uint8_t r = (rgba >> 16) & 0xFF;
                uint8_t g = (rgba >> 8) & 0xFF;
                uint8_t b = (rgba) & 0xFF;
                uint8_t* p = base + y * g_fb.pitch + x * 3;
                p[0] = b; p[1] = g; p[2] = r;
            }
        }

    }
}
