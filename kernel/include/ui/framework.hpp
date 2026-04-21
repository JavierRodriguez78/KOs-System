#ifndef KOS_UI_FRAMEWORK_HPP
#define KOS_UI_FRAMEWORK_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>

namespace kos { 
    namespace ui {

    // Special coordinate value to request automatic placement.
    constexpr uint32_t kAutoCoord = 0xFFFFFFFFu;

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
            WF_Closable    = 1 << 3,
            WF_Frameless   = 1 << 4
        };

        enum class WindowState : uint8_t {
            Normal = 0,
            Minimized,
            Maximized
        };

        enum class WindowRole : uint8_t {
            Normal = 0,
            Dialog,
            Utility,
            Dock
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
            WindowRole role;
            uint32_t parentId; // 0 for none; used by transient/dialog relationships
            uint32_t flags; // WindowFlags
            // Geometry to restore from when un-maximizing
            kos::gfx::Rect restore;
            // Dirty region tracking: set to true when window needs redraw
            // Reset by renderer after redraw. Optimization to avoid full-screen redraws.
            bool needs_redraw = true;
            // Pointer to UI component (if this window is backed by one). Optional.
            class IUIComponent* component = nullptr;
        };

        // Initialize UI framework
        bool Init();
        // Set a global work area (usable desktop region) used by placement, move,
        // resize, and maximize operations.
        void SetWorkArea(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
        // Reset work area to full framebuffer.
        void ResetWorkArea();
        // Query current work area.
        bool GetWorkArea(kos::gfx::Rect& outArea);
        // Create a window and return its id
    // x/y can be kAutoCoord to let the framework choose placement.
    uint32_t CreateWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title,
                  uint32_t flags = (WF_Resizable | WF_Minimizable | WF_Maximizable | WF_Closable));
        // Extended creation with explicit role hint for placement and future policy.
        uint32_t CreateWindowEx(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg, const char* title,
                                WindowRole role,
                                uint32_t flags = (WF_Resizable | WF_Minimizable | WF_Maximizable | WF_Closable),
                                uint32_t parentWindowId = 0);
        // Convenience helper for transient dialogs centered over parent.
        uint32_t CreateDialogWindow(uint32_t parentWindowId,
                                    uint32_t w, uint32_t h, uint32_t bg, const char* title,
                                    uint32_t flags = (WF_Minimizable | WF_Closable));
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
        bool WindowNeedsRedraw(uint32_t windowId);
        bool ConsumeWindowNeedsRedraw(uint32_t windowId);
        bool InvalidateWindow(uint32_t windowId);
        bool SetWindowComponent(uint32_t windowId, class IUIComponent* component);
        class IUIComponent* GetWindowComponent(uint32_t windowId);
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

        // Query role metadata for policy consumers.
        WindowRole GetWindowRole(uint32_t windowId);
        uint32_t GetWindowParent(uint32_t windowId);

    }

}

#endif // KOS_UI_FRAMEWORK_HPP
