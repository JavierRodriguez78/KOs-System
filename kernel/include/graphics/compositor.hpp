#ifndef KOS_GRAPHICS_COMPOSITOR_HPP
#define KOS_GRAPHICS_COMPOSITOR_HPP

#include <common/types.hpp>
#include <graphics/framebuffer.hpp>

namespace kos { 
    namespace gfx {

        struct Rect { uint32_t x, y, w, h; };

        // Very simple window descriptor (no input yet)
        struct WindowDesc {
            uint32_t x, y, w, h;
            uint32_t bg;   // ARGB
            const char* title; // optional
        };

        class Compositor {
        public:
            static bool Initialize();
            static void Shutdown();

            // Begin a new frame (clears backbuffer with wallpaper color)
            static void BeginFrame(uint32_t wallpaper);
            // Draw a simple window (solid fill + border + optional title bar)
            static void DrawWindow(const WindowDesc& win);
            // Present backbuffer to the real framebuffer
            static void EndFrame();

            // Draw a single 8x8 glyph into backbuffer at x,y with fg/bg colors (ARGB)
            static void DrawGlyph8x8(uint32_t x, uint32_t y, const uint8_t glyph[8], uint32_t fgARGB, uint32_t bgARGB);

            // Fill a rectangle in backbuffer with a solid color (clamped to screen)
            static void FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

        private:
            static bool s_ready;
            static uint32_t* s_backbuf; // backbuffer mapped to system RAM
            static uint32_t s_pitchBytes; // bytes per row in framebuffer
            static uint32_t s_width, s_height;
        };

    }

}

#endif // KOS_GRAPHICS_COMPOSITOR_HPP
