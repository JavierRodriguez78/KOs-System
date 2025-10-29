#include <console/logo.hpp>
#include <console/tty.hpp>
#include <common/logo.h>
#include <graphics/framebuffer.hpp>
#include <process/scheduler.hpp>
#include <drivers/vga/vga.hpp>

using namespace kos::console;
using namespace kos::process;
using namespace kos::drivers::vga;


namespace kos { 
    namespace console {

        // Direct decode helper at (sx,sy) without advancing a data pointer
        static inline void DecodePixelAt(uint32_t sx, uint32_t sy, uint8_t out[3]) {
            // Each pixel uses 4 bytes in header_data
            const char* p = KOS_LOGO_DATA + 4u * (sy * (uint32_t)KOS_LOGO_WIDTH + sx);
            int32_t a = (int32_t)p[0] - 33;
            int32_t b = (int32_t)p[1] - 33;
            int32_t c = (int32_t)p[2] - 33;
            int32_t d = (int32_t)p[3] - 33;
            out[0] = (uint8_t)(((a << 2) | ((b >> 4) & 0x3)) & 0xFF);
            out[1] = (uint8_t)(((((b) & 0xF) << 4) | ((c >> 2) & 0xF)) & 0xFF);
            out[2] = (uint8_t)(((((c) & 0x3) << 6) | (d & 0x3F)) & 0xFF);
        }

        // Internal helper to render a downsampled color block logo in VGA text mode
        static void PrintLogoBlockArtImpl(uint32_t xStep, uint32_t yStep) {
            // Defensive: if logo has no data, skip
            if (!KOS_LOGO_DATA || KOS_LOGO_WIDTH == 0 || KOS_LOGO_HEIGHT == 0) {
                TTY::Write("Error: Logo data not available or invalid dimensions\n");
                return;
            }

            TTY::Write("Starting logo rendering...\n");

            // VGA 16-color palette approximation (RGB 0..255)
            struct RGB { uint8_t r,g,b; };
            static const RGB vga16[16] = {
                {0,0,0},        // 0 Black
                {0,0,170},      // 1 Blue
                {0,170,0},      // 2 Green
                {0,170,170},    // 3 Cyan
                {170,0,0},      // 4 Red
                {170,0,170},    // 5 Magenta
                {170,85,0},     // 6 Brown
                {170,170,170},  // 7 Light gray
                {85,85,85},     // 8 Dark gray
                {85,85,255},    // 9 Light blue (approx)
                {85,255,85},    // 10 Light green (approx)
                {85,255,255},   // 11 Light cyan (approx)
                {255,85,85},    // 12 Light red (approx)
                {255,85,255},   // 13 Light magenta (approx)
                {255,255,85},   // 14 Yellow (light brown)
                {255,255,255}   // 15 White
            };

            auto nearestVGAColor = [&](uint8_t r, uint8_t g, uint8_t b) -> uint8_t {
                uint32_t best = 0xFFFFFFFFu;
                uint8_t bestIdx = 7; // default light gray
                for (uint8_t i = 0; i < 16; ++i) {
                    int16_t dr = (int16_t)r - (int16_t)vga16[i].r;
                    int16_t dg = (int16_t)g - (int16_t)vga16[i].g;
                    int16_t db = (int16_t)b - (int16_t)vga16[i].b;
                    uint32_t d = (uint32_t)(dr*dr) + (uint32_t)(dg*dg) + (uint32_t)(db*db);
                    if (d < best) { best = d; bestIdx = i; }
                }
                return bestIdx;
            };

            const uint8_t defaultAttr = 0x07; // light gray on black

            // We'll decode sequentially using HEADER_PIXEL which advances a data pointer
            const char* data = KOS_LOGO_DATA;
            uint8_t pix[3];

            // Iterate image rows
            for (uint32_t y = 0; y < KOS_LOGO_HEIGHT; ++y) {
                bool printRow = (y % yStep) == 0;
                uint32_t xInRow = 0;
                if (printRow) {
                    // Optionally center: compute columns for this row
                    uint32_t cols = (KOS_LOGO_WIDTH + xStep - 1) / xStep;
                    if (cols < 80) {
                        uint32_t pad = (80 - cols) / 2;
                        // Left padding spaces
                        TTY::SetAttr(defaultAttr);
                        for (uint32_t s = 0; s < pad; ++s) TTY::PutChar(' ');
                    }
                }
                // Walk pixels in this row, decoding sequentially
                for (uint32_t x = 0; x < KOS_LOGO_WIDTH; ++x) {
                    KOS_LOGO_HEADER_PIXEL(data, pix); // fills pix[0..2] and advances data
                    if (printRow) {
                        if ((x % xStep) == 0) {
                            uint8_t fg = nearestVGAColor(pix[0], pix[1], pix[2]);
                            // Set foreground color; background black
                            TTY::SetAttr(VGA::MakeAttr(fg, 0));
                            TTY::PutChar((int8_t)0xDB); // full block (CP437 219)
                            ++xInRow;
                        }
                    }
                }
                if (printRow) { 
                    TTY::PutChar('\n'); 
                    // Yield CPU every few rows to allow other threads to run
                    if ((y % (yStep * 5)) == 0) {
                        // Safe to call unconditionally; API checks scheduler presence
                        kos::process::SchedulerAPI::SleepThread(1);
                    }
                }
            }

            // Restore default attribute
            TTY::SetAttr(defaultAttr);
            TTY::Write("Logo rendering completed.\n");
        }

        void PrintLogoBlockArt() {
            // Auto-fit to ~70 columns and ~20 rows
            uint32_t targetCols = 70, targetRows = 20;
            uint32_t xStep = (KOS_LOGO_WIDTH + targetCols - 1) / targetCols; if (xStep == 0) xStep = 1;
            uint32_t yStep = (KOS_LOGO_HEIGHT + targetRows - 1) / targetRows; if (yStep == 0) yStep = 1;
            PrintLogoBlockArtImpl(xStep, yStep);
        }

        void PrintLogoBlockArtScaled(uint32_t xStep, uint32_t yStep) {
            if (xStep == 0) xStep = 1;
            if (yStep == 0) yStep = 1;
            PrintLogoBlockArtImpl(xStep, yStep);
        }

        // Render the KOS logo to framebuffer in 32-bpp RGB, scaled to 335x212 if available
        void PrintLogoFramebuffer32() {
            PrintLogoFramebuffer32Scaled(335, 212);
        }

        void PrintLogoFramebuffer32Scaled(uint32_t targetW, uint32_t targetH) {
            if (!KOS_LOGO_DATA || KOS_LOGO_WIDTH == 0 || KOS_LOGO_HEIGHT == 0) {
                TTY::Write("Error: Logo data not available for framebuffer\n");
                return;
            }
            if (!kos::gfx::IsAvailable()) {
                TTY::Write("Error: Framebuffer not available\n");
                return;
            }
            
            TTY::Write("Starting framebuffer logo rendering...\n");
            const uint32_t srcW = KOS_LOGO_WIDTH;
            const uint32_t srcH = KOS_LOGO_HEIGHT;

            // Compute scale using nearest-neighbor to fixed size 335x212
            // Center the image on screen if larger framebuffer
            const auto& fb = kos::gfx::GetInfo();
            uint32_t startX = 0, startY = 0;
            if (fb.width > targetW) startX = (fb.width - targetW) / 2;
            if (fb.height > targetH) startY = (fb.height - targetH) / 2;

            for (uint32_t y = 0; y < targetH; ++y) {
                // Map to source y (use 32-bit math to avoid libgcc 64-bit div)
                uint32_t sy = (y * srcH) / targetH;
                for (uint32_t x = 0; x < targetW; ++x) {
                    uint32_t sx = (x * srcW) / targetW;
                    uint8_t pix[3];
                    DecodePixelAt(sx, sy, pix);
                    uint8_t r = pix[0];
                    uint8_t g = pix[1];
                    uint8_t b = pix[2];
                    // Pack into 0xAARRGGBB
                    uint32_t rgba = (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | (uint32_t(b));
                    kos::gfx::PutPixel32(startX + x, startY + y, rgba);
                }
                
                // Yield CPU every few rows to allow other threads to run
                if ((y % 20) == 0) {
                    // Safe to call unconditionally; API checks scheduler presence
                    kos::process::SchedulerAPI::SleepThread(1);
                }
            }
            
            TTY::Write("Framebuffer logo rendering completed.\n");

        } 
    }
}