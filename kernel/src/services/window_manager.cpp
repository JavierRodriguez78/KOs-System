#include <services/window_manager.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <ui/framework.hpp>
#include <ui/input.hpp>

using namespace kos::services;

namespace kos { namespace services {

static uint32_t t = 0;

bool WindowManager::Start() {
    if (!kos::gfx::IsAvailable()) {
        kos::console::Logger::Log("WindowManager: framebuffer not available; disabled");
        return false;
    }
    if (!kos::gfx::Compositor::Initialize()) {
        kos::console::Logger::Log("WindowManager: compositor init failed");
        return false;
    }
    kos::ui::InitInput();
    // Center the cursor and set a comfortable sensitivity
    const auto& fb = kos::gfx::GetInfo();
    kos::ui::SetCursorPos((int)(fb.width/2), (int)(fb.height/2));
    kos::ui::SetMouseSensitivity(2, 1); // 2x default sensitivity
    kos::ui::Init();
    // Create a demo window via the UI framework
    kos::ui::CreateWindow(60, 60, 260, 180, 0xFF3B82F6u, "Demo Window");
    kos::console::Logger::Log("WindowManager: started");

    // Draw an initial frame immediately so the window is visible even before the service ticker runs
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(wallpaper);
    kos::ui::RenderAll();
    kos::gfx::Compositor::EndFrame();
    return true;
}

void WindowManager::Tick() {
    if (!kos::gfx::IsAvailable()) return;
    // Process basic mouse input for click-to-focus and dragging
    static bool dragging = false;
    static uint32_t dragWin = 0;
    static int dragOffX = 0, dragOffY = 0;
    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    bool left = (mb & 1u) != 0;
    if (!dragging && left) {
        uint32_t wid; bool onTitle;
        if (kos::ui::HitTest(mx, my, wid, onTitle)) {
            kos::ui::BringToFront(wid);
            if (onTitle) {
                kos::gfx::WindowDesc d; kos::ui::GetWindowDesc(wid, d);
                dragging = true; dragWin = wid; dragOffX = mx - (int)d.x; dragOffY = my - (int)d.y;
            }
        }
    } else if (dragging && left) {
        // drag
        int nx = mx - dragOffX; if (nx < 0) nx = 0;
        int ny = my - dragOffY; if (ny < 0) ny = 0;
        kos::ui::SetWindowPos(dragWin, (uint32_t)nx, (uint32_t)ny);
    } else if (dragging && !left) {
        dragging = false; dragWin = 0;
    }
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(wallpaper);
    // Render all UI windows
    kos::ui::RenderAll();
    // Optional animation: move the demo window horizontally (simple effect)
    // For now, leave static to keep it predictable.
    kos::gfx::Compositor::EndFrame();
}

}} // namespace
