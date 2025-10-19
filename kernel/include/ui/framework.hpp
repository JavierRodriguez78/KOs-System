#ifndef KOS_UI_FRAMEWORK_HPP
#define KOS_UI_FRAMEWORK_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>

namespace kos { 
    namespace ui {

        struct UIWindow {
            uint32_t id;
            kos::gfx::WindowDesc desc;
        };

        // Initialize UI framework
        bool Init();
        // Create a window and return its id
        uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title);
        // Render all windows using the compositor
        void RenderAll();

    }

}

#endif // KOS_UI_FRAMEWORK_HPP
