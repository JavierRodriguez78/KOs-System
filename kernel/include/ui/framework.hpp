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

        // Hit-testing: returns topmost window under (x,y). onTitleBar=true when point is on the title bar area
        bool HitTest(int x, int y, uint32_t& outWindowId, bool& onTitleBar);

        // Bring a window to front (top of z-order)
        bool BringToFront(uint32_t windowId);

        // Set window position
        bool SetWindowPos(uint32_t windowId, uint32_t nx, uint32_t ny);

        // Query window properties
        bool GetWindowDesc(uint32_t windowId, kos::gfx::WindowDesc& outDesc);

    }

}

#endif // KOS_UI_FRAMEWORK_HPP
