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

        private:
            static bool s_ready;
            static uint32_t* s_backbuf; // backbuffer mapped to system RAM
            static uint32_t s_pitchBytes; // bytes per row in framebuffer
            static uint32_t s_width, s_height;
        };

    }

}

#endif // KOS_GRAPHICS_COMPOSITOR_HPP
