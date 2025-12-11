#ifndef KOS_UI_FRAMEWORK_HPP
#define KOS_UI_FRAMEWORK_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>

namespace kos { 
    namespace ui {

        enum class HitRegion : uint8_t {
            None = 0,
            Client = 1,
            TitleBar = 2,
            ButtonClose = 3,
            ButtonMax = 4,
            ButtonMin = 5,
            // Resizing edges (bitmask style for future):
            ResizeLeft = 10,
            ResizeRight = 11,
            ResizeTop = 12,
            ResizeBottom = 13,
            ResizeTopLeft = 14,
            ResizeTopRight = 15,
            ResizeBottomLeft = 16,
            ResizeBottomRight = 17
        };

        enum WindowFlags : uint32_t {
            WF_None      = 0,
            WF_Resizable = 1 << 0,
            WF_Minimizable = 1 << 1,
            WF_Maximizable = 1 << 2,
            WF_Closable    = 1 << 3
        };

        enum class WindowState : uint8_t {
            Normal = 0,
            Minimized,
            Maximized
        };

        // UI event system for interaction notifications
        enum class UIEventType : uint8_t {
            None = 0,
            WindowFocused,
            WindowClosed,
            WindowMinimized,
            WindowRestored,
            WindowMaximized,
            WindowMoved,
            WindowResized
        };

        struct UIEvent {
            UIEventType type;
            uint32_t windowId;
            // Optional payload
            uint32_t x; // for move/resize new x or width
            uint32_t y; // for move/resize new y or height
        };

        struct UIWindow {
            uint32_t id;
            kos::gfx::WindowDesc desc;
            // Window manager state
            WindowState state;
            uint32_t flags; // WindowFlags
            // Geometry to restore from when un-maximizing
            kos::gfx::Rect restore;
        };

        // Initialize UI framework
        bool Init();
        // Create a window and return its id
    uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title,
                  uint32_t flags = (WF_Resizable | WF_Minimizable | WF_Maximizable | WF_Closable));
        // Render all windows using the compositor
        void RenderAll();

        // Hit-testing: returns topmost window under (x,y). onTitleBar=true when point is on the title bar area
    bool HitTest(int x, int y, uint32_t& outWindowId, bool& onTitleBar);
    // Detailed hit test reporting region (client, title, buttons, resize edges)
    bool HitTestDetailed(int x, int y, uint32_t& outWindowId, HitRegion& region);

        // Bring a window to front (top of z-order)
        bool BringToFront(uint32_t windowId);

        // Set window position
        bool SetWindowPos(uint32_t windowId, uint32_t nx, uint32_t ny);
    bool SetWindowSize(uint32_t windowId, uint32_t nw, uint32_t nh);

        // Query window properties
        bool GetWindowDesc(uint32_t windowId, kos::gfx::WindowDesc& outDesc);
    // Enumerate windows for UI components (like taskbar)
    uint32_t GetWindowCount();
    bool GetWindowAt(uint32_t index, uint32_t& outId, kos::gfx::WindowDesc& outDesc, WindowState& outState, uint32_t& outFlags);

        // Window state changes
        bool CloseWindow(uint32_t windowId);
    bool MinimizeWindow(uint32_t windowId);
    bool RestoreWindow(uint32_t windowId);
        bool ToggleMaximize(uint32_t windowId);
        WindowState GetWindowState(uint32_t windowId);
        uint32_t GetWindowFlags(uint32_t windowId);

        // Focus handling
        void SetFocusedWindow(uint32_t windowId); // set focus if windowId exists
        uint32_t GetFocusedWindow(); // 0 when none

        // Standard title bar metrics to keep compositor and hit-testing in sync
        constexpr uint32_t TitleBarHeight() { return 18; }
        void GetStandardButtonRects(const kos::gfx::WindowDesc& d,
                                    kos::gfx::Rect& outMin,
                                    kos::gfx::Rect& outMax,
                                    kos::gfx::Rect& outClose);

        // Process one-tick input and update window interactions (move/resize/button actions/focus)
        void UpdateInteractions();
        // Poll one UI event from the internal queue; returns false if no events
        bool PollEvent(UIEvent& outEvent);

    }

}

#endif // KOS_UI_FRAMEWORK_HPP
