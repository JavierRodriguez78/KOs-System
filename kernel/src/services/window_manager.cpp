#include <services/window_manager.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <ui/framework.hpp>

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
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(wallpaper);
    // Render all UI windows
    kos::ui::RenderAll();
    // Optional animation: move the demo window horizontally (simple effect)
    // For now, leave static to keep it predictable.
    kos::gfx::Compositor::EndFrame();
}

}} // namespace
