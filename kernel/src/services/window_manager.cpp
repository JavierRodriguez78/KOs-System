#include <services/window_manager.hpp>
#include <graphics/compositor.hpp>
#include <graphics/framebuffer.hpp>
#include <console/logger.hpp>
#include <ui/framework.hpp>
#include <ui/input.hpp>
#include <ui/login_screen.hpp>
#include <ui/process_viewer.hpp>
#include <graphics/terminal.hpp>
#include <graphics/font8x8_basic.hpp>
#include <services/service_manager.hpp>
#include <console/tty.hpp>
#include <drivers/mouse/mouse_stats.hpp>
#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <kernel/globals.hpp>
#include <ui/cursor.hpp>
#include <console/shell.hpp>
#include <console/threaded_shell.hpp>
#include <drivers/ps2/ps2.hpp>
#include <lib/serial.hpp>
#include <lib/stdio.hpp>
#include <drivers/net/e1000/e1000_poll.h>
#include <drivers/gpu/vmsvga.hpp>
#include <process/thread_manager.hpp>
#include <process/scheduler.hpp>
#include <fs/filesystem.hpp>

// Use the canonical kernel global mouse driver pointer declared in `kernel/globals.hpp`.
// Access it as `kos::g_mouse_driver_ptr`.

// New modular system components
#include <input/event_queue.hpp>
#include <ui/window_registry.hpp>
#include <ui/component.hpp>
#include <ui/system_components.hpp>
using namespace kos::services;

namespace kos { namespace services {

static uint32_t t = 0;
static uint32_t s_mouse_win_id = 0; // small diagnostics window for mouse position
static uint32_t s_login_win_id = 0; // login window id during pre-login
static uint32_t s_process_monitor_win_id = 0; // process monitor window id
static bool s_login_active = false;
// 0=never, 1=until first packet (default), 2=always
static uint8_t s_mouse_poll_mode = 1;
static bool s_taskbar_enabled = true;
static uint32_t s_terminal_counter = 1; // for titling Shell N
static uint32_t s_prev_kbd_ev = 0;      // track keyboard event counter
static int s_kbd_evt_flash = 0;         // frames to flash indicator when keys arrive
static bool s_shell_started = false;
static uint32_t s_clock_win_id = 0; // desktop clock/date widget
static uint32_t s_system_hud_win_id = 0; // desktop CPU/MEM/BAT widget
static uint32_t s_hardware_info_win_id = 0; // desktop hardware information window
static constexpr uint32_t kDesktopClockTopInset = 56u; // reserved top strip to avoid overlap with top widgets

// ========== File Browser window state ==========
static uint32_t s_file_browser_win_id = 0;
static kos::ui::ProcessMonitorComponent s_process_monitor_component;
static kos::ui::ClockComponent s_clock_component;
static kos::ui::SystemHudComponent s_system_hud_component;
static kos::ui::MouseDiagnosticComponent s_mouse_component;
static kos::ui::HardwareInfoComponent s_hardware_info_component;
static kos::ui::FileBrowserComponent s_file_browser_component;
static kos::ui::AppMenuComponent s_app_menu_component;

struct DesktopAppEntry {
    char name[48];
    char exec[96];
    bool autostart;
    uint32_t priority;
};

static DesktopAppEntry s_desktop_apps[16];
static uint32_t s_desktop_app_count = 0;
static uint32_t s_app_menu_win_id = 0;

static uint32_t StrLen(const char* s) {
    if (!s) return 0;
    uint32_t n = 0;
    while (s[n]) ++n;
    return n;
}

static bool StrEq(const char* a, const char* b) {
    if (!a || !b) return false;
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == 0 && b[i] == 0;
}

static void StrCopy(char* dst, uint32_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) { dst[0] = 0; return; }
    uint32_t i = 0;
    while (src[i] && i + 1u < dstSize) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void StrTrim(char* s) {
    if (!s) return;
    uint32_t len = StrLen(s);
    uint32_t start = 0;
    while (start < len && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) ++start;
    uint32_t end = len;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) --end;
    if (start > 0) {
        uint32_t i = 0;
        while (start + i < end) { s[i] = s[start + i]; ++i; }
        s[i] = 0;
    } else {
        s[end] = 0;
    }
}

static uint32_t ParseU32(const char* s, uint32_t fallback) {
    if (!s || !*s) return fallback;
    uint32_t v = 0;
    bool any = false;
    for (uint32_t i = 0; s[i]; ++i) {
        if (s[i] < '0' || s[i] > '9') break;
        any = true;
        v = v * 10u + static_cast<uint32_t>(s[i] - '0');
    }
    return any ? v : fallback;
}

static bool ParseDesktopEntryBuffer(const char* text, uint32_t len, DesktopAppEntry& out) {
    out.name[0] = 0;
    out.exec[0] = 0;
    out.autostart = false;
    out.priority = 100u;
    if (!text || len == 0) return false;

    bool inDesktopSection = false;
    uint32_t pos = 0;
    while (pos < len) {
        char line[192];
        uint32_t li = 0;
        while (pos < len && text[pos] != '\n' && li + 1u < sizeof(line)) {
            line[li++] = text[pos++];
        }
        line[li] = 0;
        if (pos < len && text[pos] == '\n') ++pos;

        StrTrim(line);
        if (!line[0] || line[0] == '#') continue;
        if (StrEq(line, "[Desktop Entry]")) {
            inDesktopSection = true;
            continue;
        }
        if (line[0] == '[') {
            inDesktopSection = false;
            continue;
        }
        if (!inDesktopSection) continue;

        char* eq = nullptr;
        for (uint32_t i = 0; line[i]; ++i) {
            if (line[i] == '=') { eq = &line[i]; break; }
        }
        if (!eq) continue;
        *eq = 0;
        char* key = line;
        char* value = eq + 1;
        StrTrim(key);
        StrTrim(value);

        if (StrEq(key, "Name")) {
            StrCopy(out.name, sizeof(out.name), value);
        } else if (StrEq(key, "Exec")) {
            StrCopy(out.exec, sizeof(out.exec), value);
        } else if (StrEq(key, "X-KOS-Autostart")) {
            out.autostart = StrEq(value, "true") || StrEq(value, "1") || StrEq(value, "yes");
        } else if (StrEq(key, "X-KOS-Priority")) {
            out.priority = ParseU32(value, 100u);
        }
    }

    return (out.name[0] != 0 && out.exec[0] != 0);
}

static bool DiscoverDesktopEntryCallback(const kos::fs::DirEntry* entry, void* userdata) {
    (void)userdata;
    if (!entry || entry->isDir || s_desktop_app_count >= 16u) return true;

    char fullPath[160];
    const char* base = "/USR/SHARE/DESKTOP/";
    StrCopy(fullPath, sizeof(fullPath), base);
    uint32_t p = StrLen(fullPath);
    for (uint32_t i = 0; entry->name[i] && p + 1u < sizeof(fullPath); ++i) {
        fullPath[p++] = static_cast<char>(entry->name[i]);
    }
    fullPath[p] = 0;

    uint8_t buf[2048];
    int32_t n = kos::fs::g_fs_ptr ? kos::fs::g_fs_ptr->ReadFile(reinterpret_cast<const int8_t*>(fullPath), buf, sizeof(buf) - 1u) : -1;
    if (n <= 0) return true;
    buf[n] = 0;

    DesktopAppEntry parsed{};
    if (!ParseDesktopEntryBuffer(reinterpret_cast<const char*>(buf), static_cast<uint32_t>(n), parsed)) {
        return true;
    }

    s_desktop_apps[s_desktop_app_count++] = parsed;
    return true;
}

static bool LaunchDesktopApp(const DesktopAppEntry& app) {
    uint32_t pid = 0;
    uint32_t tid = kos::process::ThreadManagerAPI::SpawnProcess(
        app.exec,
        app.name,
        &pid,
        8192,
        kos::process::PRIORITY_NORMAL,
        0);

    if (!tid) {
        kos::lib::serial_write("[WM] app launch failed: ");
        kos::lib::serial_write(app.exec);
        kos::lib::serial_write("\n");
        return false;
    }

    kos::lib::serial_write("[WM] app launched: ");
    kos::lib::serial_write(app.name);
    kos::lib::serial_write(" -> ");
    kos::lib::serial_write(app.exec);
    kos::lib::serial_write("\n");
    return true;
}

static void DiscoverDesktopApplications() {
    s_desktop_app_count = 0;
    if (!kos::fs::g_fs_ptr) return;
    (void)kos::fs::g_fs_ptr->EnumDir(reinterpret_cast<const int8_t*>("/USR/SHARE/DESKTOP"), DiscoverDesktopEntryCallback, nullptr);
}

static void LaunchAutostartDesktopApplications() {
    if (s_desktop_app_count == 0u) return;

    // Stable insertion sort by priority (lower number starts first).
    DesktopAppEntry ordered[16];
    uint32_t count = s_desktop_app_count;
    for (uint32_t i = 0; i < count; ++i) ordered[i] = s_desktop_apps[i];
    for (uint32_t i = 1; i < count; ++i) {
        DesktopAppEntry key = ordered[i];
        int32_t j = static_cast<int32_t>(i) - 1;
        while (j >= 0 && ordered[j].priority > key.priority) {
            ordered[j + 1] = ordered[j];
            --j;
        }
        ordered[j + 1] = key;
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (ordered[i].autostart) {
            (void)LaunchDesktopApp(ordered[i]);
        }
    }
}

static void ShowAppMenuWindow() {
    if (s_desktop_app_count == 0u) return;
    
    // If menu window already exists, just toggle visibility/focus
    if (s_app_menu_win_id != 0) {
        kos::gfx::WindowDesc d;
        if (kos::ui::GetWindowDesc(s_app_menu_win_id, d)) {
            // Window exists, restore and focus it
            if (kos::ui::GetWindowState(s_app_menu_win_id) == kos::ui::WindowState::Minimized) {
                kos::ui::RestoreWindow(s_app_menu_win_id);
            }
            kos::ui::BringToFront(s_app_menu_win_id);
            kos::ui::SetFocusedWindow(s_app_menu_win_id);
            return;
        }
    }
    
    // Create new menu window
    uint32_t menuWin = kos::ui::CreateDialogWindow(0, 280, 180, 0xFF1A1A20u, "Applications");
    if (menuWin == 0) return;
    
    s_app_menu_win_id = menuWin;
    
    // Update component with current app list
    kos::ui::AppMenuComponent::DesktopAppEntry appsCopy[16];
    for (uint32_t i = 0; i < s_desktop_app_count; ++i) {
        appsCopy[i].autostart = s_desktop_apps[i].autostart;
        appsCopy[i].priority = s_desktop_apps[i].priority;
        StrCopy(appsCopy[i].name, sizeof(appsCopy[i].name), s_desktop_apps[i].name);
        StrCopy(appsCopy[i].exec, sizeof(appsCopy[i].exec), s_desktop_apps[i].exec);
    }
    s_app_menu_component.SetAppList(appsCopy, s_desktop_app_count);
    s_app_menu_component.BindWindow(menuWin);
    
    // Register component
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    (void)reg.RegisterComponent(&s_app_menu_component, menuWin);
    (void)kos::ui::SetWindowComponent(menuWin, &s_app_menu_component);
}

static void CloseAppMenuWindow() {
    if (s_app_menu_win_id != 0) {
        kos::ui::MinimizeWindow(s_app_menu_win_id);
        s_app_menu_win_id = 0;
    }
}

static void ProcessAppMenuSelection() {
    if (s_app_menu_win_id == 0) return;
    
    const kos::ui::AppMenuComponent::DesktopAppEntry* selected = s_app_menu_component.GetSelectedApp();
    if (selected && selected->name[0] != 0) {
        DesktopAppEntry appToLaunch;
        appToLaunch.autostart = selected->autostart;
        appToLaunch.priority = selected->priority;
        StrCopy(appToLaunch.name, sizeof(appToLaunch.name), selected->name);
        StrCopy(appToLaunch.exec, sizeof(appToLaunch.exec), selected->exec);
        
        (void)LaunchDesktopApp(appToLaunch);
        CloseAppMenuWindow();
    }
}

static void RegisterClockComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_clock_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_clock_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_clock_component);
}

static void RegisterProcessMonitorComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_process_monitor_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_process_monitor_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_process_monitor_component);
}

static void RegisterSystemHudComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_system_hud_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_system_hud_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_system_hud_component);
}

static void RegisterMouseComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_mouse_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_mouse_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_mouse_component);
}

static void RegisterHardwareInfoComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_hardware_info_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_hardware_info_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_hardware_info_component);
}

static void RegisterFileBrowserComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_file_browser_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_file_browser_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_file_browser_component);
}

static void DispatchQueuedInputEvents() {
    kos::input::InputEvent ev{};
    uint32_t processed = 0;
    const uint32_t kMaxPerTick = 128u;

    while (processed < kMaxPerTick && kos::input::InputEventQueue::Instance().Dequeue(ev)) {
        ++processed;

        uint32_t targetWindow = ev.target_window;
        if (ev.type == kos::input::EventType::MouseMove ||
            ev.type == kos::input::EventType::MousePress ||
            ev.type == kos::input::EventType::MouseRelease) {
            bool onTitle = false;
            uint32_t hitWindow = 0;
            if (kos::ui::HitTest(ev.mouse_data.x, ev.mouse_data.y, hitWindow, onTitle)) {
                targetWindow = hitWindow;
            }
        }

        if (targetWindow == 0) {
            targetWindow = kos::ui::GetFocusedWindow();
        }

        if (targetWindow == 0) {
            continue;
        }

        kos::ui::IUIComponent* component = kos::ui::WindowRegistry::Instance().GetComponent(targetWindow);
        if (!component) {
            continue;
        }

        bool handled = component->OnInputEvent(ev);
        if (handled || ev.type == kos::input::EventType::MouseMove ||
            ev.type == kos::input::EventType::MousePress || ev.type == kos::input::EventType::MouseRelease) {
            (void)kos::ui::InvalidateWindow(targetWindow);
        }
    }
}

static void FillCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell = 2);
static bool TryReadBatteryPercent(uint8_t& outPct) {
    int32_t pct = kos::sys::get_battery_percent();
    if (pct < 0 || pct > 100) return false;
    outPct = static_cast<uint8_t>(pct);
    return true;
}

static uint8_t blend8(uint8_t a, uint8_t b, uint32_t t, uint32_t max) {
    if (max == 0) return b;
    return static_cast<uint8_t>((static_cast<uint32_t>(a) * (max - t) + static_cast<uint32_t>(b) * t) / max);
}

static void FillCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell) {
    if (cell == 0) cell = 1;
    // Optimized: group horizontal segments of the same color
    for (uint32_t j = 0; j < h; ++j) {
        uint32_t i = 0;
        while (i < w) {
            uint32_t tx = (x + i) / cell;
            uint32_t ty = (y + j) / cell;
            uint32_t color = ((tx + ty) & 1u) ? cA : cB;
            uint32_t seg_len = 1;
            while (i + seg_len < w) {
                uint32_t next_tx = (x + i + seg_len) / cell;
                uint32_t next_color = ((next_tx + ty) & 1u) ? cA : cB;
                if (next_color != color) break;
                ++seg_len;
            }

            kos::gfx::Compositor::FillRect(x + i, y + j, seg_len, 1, color);
            i += seg_len;
        }
    }
}

static void RenderLoginBackdrop() {
    const auto& fb = kos::gfx::GetInfo();
    if (fb.width == 0 || fb.height == 0) return;

    constexpr uint32_t kTop = 0xFF140A3Eu;
    constexpr uint32_t kBottom = 0xFF2C1A68u;
    constexpr uint32_t kGlowA = 0xFF2E6BAAu;
    constexpr uint32_t kGlowB = 0xFF4A8DD2u;

    for (uint32_t y = 0; y < fb.height; ++y) {
        uint8_t r = blend8(static_cast<uint8_t>((kTop >> 16) & 0xFFu), static_cast<uint8_t>((kBottom >> 16) & 0xFFu), y, fb.height - 1);
        uint8_t g = blend8(static_cast<uint8_t>((kTop >> 8) & 0xFFu), static_cast<uint8_t>((kBottom >> 8) & 0xFFu), y, fb.height - 1);
        uint8_t b = blend8(static_cast<uint8_t>(kTop & 0xFFu), static_cast<uint8_t>(kBottom & 0xFFu), y, fb.height - 1);
        r = static_cast<uint8_t>(r & 0xE0u);
        g = static_cast<uint8_t>(g & 0xE0u);
        b = static_cast<uint8_t>(b & 0xC0u);
        if (y & 1u) {
            if (r > 8u) r = static_cast<uint8_t>(r - 8u);
            if (g > 8u) g = static_cast<uint8_t>(g - 8u);
            if (b > 8u) b = static_cast<uint8_t>(b - 8u);
        }
        kos::gfx::Compositor::FillRect(0, y, fb.width, 1, 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
                                                      (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b));
    }

    uint32_t glowY = (fb.height * 4u) / 5u;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t w = (fb.width / 6u) + i * (fb.width / 18u);
        uint32_t h = 10u + i * 6u;
        uint32_t x = (fb.width > w) ? ((fb.width - w) / 2u) : 0u;
        uint32_t y = (glowY > (i * 5u)) ? (glowY - i * 5u) : 0u;
        uint32_t color = (i < 2) ? kGlowB : kGlowA;
        kos::gfx::Compositor::FillRect(x, y, w, h, color);
    }

    for (uint32_t i = 0; i < 40; ++i) {
        uint32_t x = (i * 131u + 17u) % fb.width;
        uint32_t y = (i * 67u + 29u) % (fb.height > 120 ? fb.height - 120 : fb.height);
        kos::gfx::Compositor::FillRect(x, y, 1, 1, 0x66C7DDFFu);
    }
}

static void deferred_shell_thread_entry() {
    kos::console::ThreadedShellAPI::StartShell();
}

class LoginKeyboardHandler : public kos::drivers::keyboard::KeyboardEventHandler {
public:
    virtual void OnKeyDown(int8_t c) override {
        kos::ui::LoginScreen::OnKeyDown(c);
    }

    virtual void OnKeyUp(int8_t) override {}
};

static LoginKeyboardHandler s_login_handler;

static constexpr uint32_t kTaskbarSpawnBtnId = 0xFFFFFFFFu;
static constexpr uint32_t kTaskbarAppBtnId = 0xFFFFFFFDu;
static constexpr uint32_t kTaskbarRebootBtnId = 0xFFFFFFFEu;

extern "C" void app_reboot() __attribute__((weak));
extern "C" void app_shutdown() __attribute__((weak));

static constexpr uint32_t kTaskbarHeight = 22;
static constexpr uint32_t kTaskbarPad = 4;
static constexpr uint32_t kTaskButtonW = 120;
static constexpr uint32_t kTaskButtonH = 18;

static void TaskbarRebootSystem() {
    if (app_reboot) {
        app_reboot();
        return;
    }

    auto io_outb = [](uint16_t port, uint8_t val) {
        __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
    };
    auto io_inb = [](uint16_t port) -> uint8_t {
        uint8_t ret;
        __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    };

    while (io_inb(0x64) & 0x02u) { }
    io_outb(0x64, 0xFE);
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ __volatile__("lidt %0\n\tint $0x03" : : "m"(null_idt));
    for (;;) { __asm__ __volatile__("hlt"); }
}

static void TaskbarShutdownSystem() {
    if (app_shutdown) {
        app_shutdown();
        return;
    }
    for (;;) { __asm__ __volatile__("cli; hlt"); }
}

struct TaskbarButtonGeom { uint32_t x,y,w,h; uint32_t winId; bool minimized; bool focused; };
static TaskbarButtonGeom s_taskButtons[16];
static uint32_t s_taskButtonCount = 0;

static void BuildTaskbarButtons() {
    s_taskButtonCount = 0;
    if (!s_taskbar_enabled) return;
    const auto& fb = kos::gfx::GetInfo();
    const uint32_t spawnW = 20;
    const uint32_t appW = 20;
    const uint32_t rebootW = 28;
    uint32_t x = kTaskbarPad + spawnW + 4 + appW + 4;
    uint32_t y = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    for (uint32_t i = 0; i < kos::ui::GetWindowCount() && s_taskButtonCount < 16; ++i) {
        uint32_t wid; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        if (wid == s_mouse_win_id || wid == s_clock_win_id || wid == s_system_hud_win_id) continue;
        TaskbarButtonGeom& b = s_taskButtons[s_taskButtonCount++];
        b.x = x;
        b.y = y;
        b.w = kTaskButtonW;
        b.h = kTaskButtonH;
        b.winId = wid;
        b.minimized = (st == kos::ui::WindowState::Minimized);
        b.focused = (wid == kos::ui::GetFocusedWindow());
        x += kTaskButtonW + 4;
        if (x + kTaskButtonW + kTaskbarPad + rebootW + 4 > fb.width) break;
    }
}

static uint32_t TaskbarHitTest(int mx, int my) {
    if (!s_taskbar_enabled) return 0;
    const auto& fb = kos::gfx::GetInfo();
    if ((uint32_t)my < fb.height - kTaskbarHeight) return 0;

    const uint32_t spawnX = kTaskbarPad;
    const uint32_t spawnY = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    const uint32_t spawnW = 20;
    const uint32_t spawnH = kTaskButtonH;
    if ((uint32_t)mx >= spawnX && (uint32_t)mx < spawnX + spawnW && (uint32_t)my >= spawnY && (uint32_t)my < spawnY + spawnH) {
        return kTaskbarSpawnBtnId;
    }

    const uint32_t appX = spawnX + spawnW + 4;
    const uint32_t appW = 20;
    if ((uint32_t)mx >= appX && (uint32_t)mx < appX + appW && (uint32_t)my >= spawnY && (uint32_t)my < spawnY + spawnH) {
        return kTaskbarAppBtnId;
    }

    const uint32_t rebootW = 28;
    const uint32_t rebootX = fb.width - kTaskbarPad - rebootW;
    const uint32_t rebootY = spawnY;
    if ((uint32_t)mx >= rebootX && (uint32_t)mx < rebootX + rebootW && (uint32_t)my >= rebootY && (uint32_t)my < rebootY + spawnH) {
        return kTaskbarRebootBtnId;
    }

    for (uint32_t i = 0; i < s_taskButtonCount; ++i) {
        const auto& b = s_taskButtons[i];
        if ((uint32_t)mx >= b.x && (uint32_t)mx < b.x + b.w && (uint32_t)my >= b.y && (uint32_t)my < b.y + b.h) {
            return b.winId;
        }
    }
    return 0;
}

static void RenderWindowContentsByZOrder() {
    uint32_t count = kos::ui::GetWindowCount();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t wid = 0; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl = 0;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        if (st == kos::ui::WindowState::Minimized) continue;

        // Global content validator: clip all window content to its client area.
        uint32_t cx = d.x + 1;
        uint32_t cy = d.y + ((fl & kos::ui::WF_Frameless) ? 1u : (kos::ui::TitleBarHeight() + 1u));
        uint32_t cw = (d.w > 2 ? d.w - 2 : 0);
        uint32_t ch = 0;
        if (fl & kos::ui::WF_Frameless) {
            ch = (d.h > 2 ? d.h - 2 : 0);
        } else {
            uint32_t chrome = kos::ui::TitleBarHeight() + 2;
            ch = (d.h > chrome ? d.h - chrome : 0);
        }
        kos::gfx::Compositor::SetClipRect(cx, cy, cw, ch);

        if (kos::ui::IUIComponent* component = kos::ui::WindowRegistry::Instance().GetComponent(wid)) {
            component->Render();
            (void)kos::ui::ConsumeWindowNeedsRedraw(wid);
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (s_login_active && wid == s_login_win_id) {
            kos::ui::LoginScreen::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (!s_login_active && kos::gfx::Terminal::IsActive() && wid == kos::gfx::Terminal::GetWindowId()) {
            kos::gfx::Terminal::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        kos::gfx::Compositor::ClearClipRect();
    }
}

static void CreatePostLoginWindows() {
    if (s_clock_win_id == 0) {
        const auto& fb = kos::gfx::GetInfo();
        // "HH:MM:SS" = 8 chars * 16px (2x) = 128px + 14px padding
        // height: 4 + 16 + 4 + 8 + 4 = 36px
        uint32_t cw = 142, ch = 36;
        uint32_t cx = (fb.width > cw + 8u) ? fb.width - cw - 8u : 0u;
        uint32_t cy = 8u;
        s_clock_win_id = kos::ui::CreateWindowEx(cx, cy, cw, ch, 0xFF020A02u, "Clock",
                             kos::ui::WindowRole::Utility,
                             kos::ui::WF_Frameless);
    }

    if (s_system_hud_win_id == 0) {
        const auto& fb = kos::gfx::GetInfo();
        const uint32_t hudW = 176u, hudH = 44u;
        const uint32_t clockW = 142u;
        uint32_t hx = 8u;
        if (fb.width > (clockW + hudW + 24u)) {
            hx = fb.width - clockW - hudW - 16u;
        }
        s_system_hud_win_id = kos::ui::CreateWindowEx(hx, 8u, hudW, hudH, 0xFF020A02u, "SystemHUD",
                                  kos::ui::WindowRole::Utility,
                                  kos::ui::WF_Frameless);
    }

    // Reserve a top strip so auto-positioned windows won't overlap the clock widget.
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }

    if (s_mouse_win_id == 0) {
        uint32_t w = 280, h = 56;
        s_mouse_win_id = kos::ui::CreateWindowEx(kos::ui::kAutoCoord, kos::ui::kAutoCoord, w, h, 0xFF0F0F10u, "Mouse",
                             kos::ui::WindowRole::Utility,
                             kos::ui::WF_Minimizable | kos::ui::WF_Closable);
        if (s_mouse_win_id) {
            kos::ui::BringToFront(s_mouse_win_id);
        }
    }

    // Desktop tile layout: Process Monitor on top, Hardware Info on bottom.
    // This avoids default overlap and keeps both panes visible at startup.
    kos::gfx::Rect area;
    kos::ui::GetWorkArea(area);
    const uint32_t pad = 8u;
    const uint32_t gap = 8u;
    uint32_t contentX = area.x + pad;
    uint32_t contentY = area.y + pad;
    uint32_t contentW = (area.w > pad * 2u) ? (area.w - pad * 2u) : area.w;
    uint32_t contentH = (area.h > pad * 2u) ? (area.h - pad * 2u) : area.h;

    uint32_t hwH = (contentH > 380u) ? 220u : (contentH > 220u ? (contentH / 3u) : (contentH > 80u ? contentH - 80u : contentH));
    if (hwH < 96u) hwH = 96u;
    if (hwH + gap >= contentH) hwH = (contentH > gap + 64u) ? (contentH - gap - 64u) : contentH;
    uint32_t pmH = (contentH > hwH + gap) ? (contentH - hwH - gap) : 64u;
    if (pmH < 64u) pmH = 64u;

    uint32_t pmX = contentX;
    uint32_t pmY = contentY;
    uint32_t pmW = contentW;

    uint32_t hwX = contentX;
    uint32_t hwY = contentY + pmH + gap;
    uint32_t hwW = contentW;
    const bool smallDesktop = (contentH < 560u) || (contentW < 980u);

    if (s_process_monitor_win_id == 0) {
        s_process_monitor_win_id = kos::ui::CreateWindowEx(pmX, pmY, pmW, pmH, 0xFF1F1F2E, "System Process Monitor",
                                 kos::ui::WindowRole::Normal,
                                 kos::ui::WF_Resizable | kos::ui::WF_Minimizable |
                                 kos::ui::WF_Maximizable | kos::ui::WF_Closable);
        if (s_process_monitor_win_id) {
            if (smallDesktop) {
                kos::ui::MinimizeWindow(s_process_monitor_win_id);
            }
        }
    } else {
        kos::ui::SetWindowPos(s_process_monitor_win_id, pmX, pmY);
        kos::ui::SetWindowSize(s_process_monitor_win_id, pmW, pmH);
        if (smallDesktop && kos::ui::GetWindowState(s_process_monitor_win_id) != kos::ui::WindowState::Minimized) {
            kos::ui::MinimizeWindow(s_process_monitor_win_id);
        }
    }

    if (s_hardware_info_win_id == 0) {
        s_hardware_info_win_id = kos::ui::CreateWindowEx(
            hwX,
            hwY,
            hwW,
            hwH,
            0xFF0F1420u,
            "System Hardware Info",
            kos::ui::WindowRole::Normal,
            kos::ui::WF_Resizable | kos::ui::WF_Minimizable | kos::ui::WF_Maximizable | kos::ui::WF_Closable);
    } else {
        kos::ui::SetWindowPos(s_hardware_info_win_id, hwX, hwY);
        kos::ui::SetWindowSize(s_hardware_info_win_id, hwW, hwH);
    }

    if (smallDesktop && s_hardware_info_win_id) {
        kos::ui::BringToFront(s_hardware_info_win_id);
        kos::ui::SetFocusedWindow(s_hardware_info_win_id);
    }

    // File Browser window: right column if wide enough, else floating minimized
    if (s_file_browser_win_id == 0) {
        constexpr uint32_t fbW = 360u, fbH = 280u;
        uint32_t fbX = kos::ui::kAutoCoord, fbY = kos::ui::kAutoCoord;
        // Try to place it to the right of the hw info window if there's room
        if (contentW > hwW + fbW + gap * 2u) {
            fbX = hwX + hwW + gap;
            fbY = hwY;
        }
        s_file_browser_win_id = kos::ui::CreateWindowEx(
            fbX, fbY, fbW, fbH,
            0xFF0F1320u,
            "File Browser",
            kos::ui::WindowRole::Normal,
            kos::ui::WF_Resizable | kos::ui::WF_Minimizable |
            kos::ui::WF_Maximizable | kos::ui::WF_Closable);
        if (s_file_browser_win_id) {
            // Minimize on small desktops to avoid clutter
            if (smallDesktop) {
                kos::ui::MinimizeWindow(s_file_browser_win_id);
            }
        }
    }

    RegisterProcessMonitorComponent(s_process_monitor_win_id);
    RegisterMouseComponent(s_mouse_win_id);
    RegisterClockComponent(s_clock_win_id);
    RegisterSystemHudComponent(s_system_hud_win_id);
    RegisterHardwareInfoComponent(s_hardware_info_win_id);
    RegisterFileBrowserComponent(s_file_browser_win_id);

    DiscoverDesktopApplications();
    LaunchAutostartDesktopApplications();
}

uint32_t WindowManager::SpawnTerminal() {
    if (!kos::gfx::IsAvailable()) return 0;
    // Terminal geometry: 80x25 8x8 cells + chrome
    uint32_t termW = 80 * 8 + 16; // padding
    uint32_t termH = 25 * 8 + 18 + 8; // title + padding
    char title[32];
    // First window is "Shell", subsequent are "Shell N"
    if (s_terminal_counter <= 1) {
        title[0]='S'; title[1]='h'; title[2]='e'; title[3]='l'; title[4]='l'; title[5]=0;
    } else {
        // Simple decimal append
        const char base[] = "Shell ";
        int bi=0; while (base[bi]) { title[bi]=base[bi]; ++bi; }
        uint32_t n = s_terminal_counter;
        // convert n to decimal
        char rev[10]; int ri=0; if (n==0){ rev[ri++]='0'; }
        while(n && ri<10){ rev[ri++] = char('0' + (n%10)); n/=10; }
        while(ri){ title[bi++] = rev[--ri]; }
        title[bi]=0;
    }
    uint32_t newId = kos::ui::CreateWindow(kos::ui::kAutoCoord, kos::ui::kAutoCoord, termW, termH, 0xFF000000u, title);
    if (!newId) return 0;

    // Close previous terminal window if any, then rebind renderer to new window.
    // Ensure the terminal backend is initialized before rendering/writing to it.
    uint32_t oldId = kos::gfx::Terminal::GetWindowId();
    if (!kos::gfx::Terminal::IsActive()) {
        if (!kos::gfx::Terminal::Initialize(newId, 80, 25, true)) {
            kos::ui::CloseWindow(newId);
            return 0;
        }
    } else {
        kos::gfx::Terminal::SetWindow(newId);
    }
    kos::ui::BringToFront(newId);
    kos::ui::SetFocusedWindow(newId);
    // Ensure a clean buffer and fresh prompt (skip boot logs)
    kos::console::TTY::Clear();
    kos::console::TTY::DiscardPreinitBuffer();
    kos::gfx::Terminal::Clear();
    kos::console::TTY::SetColor(10, 0);
    kos::console::TTY::Write((const int8_t*)"[GUI SHELL] ");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::Write((const int8_t*)"Mismo prompt y comandos que el entorno texto\n");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::SetColor(14, 0);
    kos::console::TTY::Write((const int8_t*)"kos[00]:/");
    kos::console::TTY::SetColor(11, 0);
    kos::console::TTY::Write((const int8_t*)"");
    kos::console::TTY::SetAttr(0x07);
    kos::console::TTY::Write((const int8_t*)"$ ");

    // If there was a previous terminal window, close it to avoid an empty placeholder
    if (oldId && oldId != newId) {
        kos::ui::CloseWindow(oldId);
    }
    ++s_terminal_counter;
    return newId;
}

void WindowManager::SetMousePollMode(uint8_t mode) {
    if (mode > 2) mode = 2;
    s_mouse_poll_mode = mode;
}

bool WindowManager::Start() {
    if (!kos::gfx::IsAvailable()) {
        kos::console::Logger::Log("WindowManager: framebuffer not available; disabled");
        return false;
    }
    // Direct LFB smoke test before compositor init. If this pattern is visible,
    // framebuffer writes reach the host and the issue is in higher layers.
    {
        kos::console::Logger::Log("WindowManager: direct framebuffer smoke test");
        const auto& fb = kos::gfx::GetInfo();
        kos::gfx::Clear32(0xFF202020u);
        const uint32_t diag = (fb.width < fb.height) ? fb.width : fb.height;
        for (uint32_t i = 0; i < diag; ++i) {
            kos::gfx::PutPixel32(i, i, 0xFFFF00FFu); // magenta diagonal
            kos::gfx::PutPixel32(fb.width - 1u - i, i, 0xFF00FFFFu); // cyan diagonal
        }
        for (uint32_t x = 0; x < fb.width; ++x) {
            kos::gfx::PutPixel32(x, 0, 0xFFFFFF00u);
            if (fb.height > 1) kos::gfx::PutPixel32(x, fb.height - 1u, 0xFFFFFF00u);
        }
        kos::console::Logger::Log("WindowManager: direct framebuffer smoke done");
    }
    if (!kos::gfx::Compositor::Initialize()) {
        kos::console::Logger::Log("WindowManager: compositor init failed");
        return false;
    }
    kos::console::Logger::Log("WindowManager: compositor ready");
    kos::console::Logger::LogKV("WindowManager: render backend", kos::gfx::Compositor::BackendName());
    // Suppress further log writes to TTY now; we'll unmute later in Tick() so
    // ServiceManager's status lines don't clobber the initial prompt.
    kos::console::Logger::MuteTTY(true);
    // Input subsystem was initialized earlier at BootStage::InputInit
    kos::ui::Init();
    // Define desktop work area (reserve bottom strip for taskbar/panel).
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = s_login_active ? 0u : kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }
    // Create a login window first, before any terminal. This is blocking.
    {
        uint32_t w = 320, h = 380;
        s_login_win_id = kos::ui::CreateWindowEx(kos::ui::kAutoCoord, kos::ui::kAutoCoord, w, h, 0xFF101012u, "Login",
                     kos::ui::WindowRole::Dialog, kos::ui::WF_Frameless, 0);
        if (s_login_win_id) {
            s_login_active = true;
            kos::ui::BringToFront(s_login_win_id);
            kos::ui::SetFocusedWindow(s_login_win_id);
            kos::ui::LoginScreen::Initialize(s_login_win_id);
            // Swap keyboard handler to capture login input - use global override for reliability
            ::kos::g_keyboard_handler_override = &s_login_handler;
            
            // Also try to set via driver if available
            if (::kos::g_keyboard_driver_ptr) {
                ::kos::g_keyboard_driver_ptr->SetHandler(&s_login_handler);
            }
        }
    }
    // Desktop windows are created only after successful login.
    s_mouse_win_id = 0;
    s_process_monitor_win_id = 0;
    s_clock_win_id = 0;
    s_system_hud_win_id = 0;
    s_hardware_info_win_id = 0;
    s_file_browser_win_id = 0;

    // Draw an initial frame immediately so the window is visible even before the service ticker runs
    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(s_login_active ? 0xFF16061Eu : wallpaper);
    if (s_login_active) RenderLoginBackdrop();
    kos::ui::RenderAll();
    // Render each window client strictly by z-order to keep stacking coherent.
    RenderWindowContentsByZOrder();
    
    // *** STARTUP TEST: Write to serial to confirm it's working ***
    static bool startup_logged = false;
    if (!startup_logged) {
        kos::lib::serial_init();
        kos::lib::serial_write("\n[WM] Window manager initialized\n");
        startup_logged = true;
    }
    
    // DEBUG OVERLAY (early): show fb + window stats if no windows visible
    if (!s_login_active) {
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t winCount = kos::ui::GetWindowCount();
        bool term = kos::gfx::Terminal::IsActive();
        char buf[160];
        int bi=0; auto put=[&](char c){ if(bi<159) buf[bi++]=c; };
        auto writeStr=[&](const char* s){ while(*s) put(*s++); };
        auto writeDec=[&](uint32_t v){ char tmp[16]; int n=0; if(v==0){tmp[n++]='0';} else { char r[16]; int ri=0; while(v&&ri<16){ r[ri++]=char('0'+(v%10)); v/=10;} while(ri) tmp[n++]=r[--ri]; } for(int i=0;i<n;++i) put(tmp[i]); };
        writeStr("FB "); writeDec(fbinfo.width); put('x'); writeDec(fbinfo.height); put(' '); writeDec(fbinfo.bpp); writeStr("bpp  win:"); writeDec(winCount); writeStr(" term:"); writeStr(term?"Y":"N"); buf[bi]=0;
        // Draw line at top-left
        for (int i=0; buf[i]; ++i) {
            char ch = buf[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 4, glyph, 0xFFFFFFFFu, wallpaper);
        }
        // Second line: input sources (kbd/mouse) and brief key indicator
        char buf2[160]; bi=0;
        auto writeStr2 = [&](const char* s){ while(*s) buf2[bi++]=*s++; };
        auto writeSrc=[&](const char* label, uint8_t src){ writeStr2(label); writeStr2(":"); if(src==1) writeStr2("IRQ"); else if(src==2) writeStr2("POLL"); else writeStr2("-"); writeStr2("  "); };
        writeSrc("kbd", ::kos::g_kbd_input_source);
        writeSrc("mouse", ::kos::g_mouse_input_source);
        // Update keyboard event flash
        if (::kos::g_kbd_events != s_prev_kbd_ev) { s_kbd_evt_flash = 15; s_prev_kbd_ev = ::kos::g_kbd_events; }
        else if (s_kbd_evt_flash > 0) { --s_kbd_evt_flash; }
        if (s_kbd_evt_flash > 0) { writeStr2("[KEY]"); }
        buf2[bi]=0;
        for (int i=0; buf2[i]; ++i) {
            char ch = buf2[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 14, glyph, 0xFFFFFFFFu, wallpaper);
        }
    }
    kos::gfx::Compositor::EndFrame();
    return true;
}

void WindowManager::Tick() {
    // Poll E1000 for received packets
    e1000_rx_poll();
    
    if (!kos::gfx::IsAvailable()) return;
    // Keep work area in sync with current resolution/panel reservation.
    {
        const auto& fb = kos::gfx::GetInfo();
        uint32_t workY = s_login_active ? 0u : kDesktopClockTopInset;
        uint32_t workH = (fb.height > workY ? fb.height - workY : 0u);
        if (s_taskbar_enabled && workH > kTaskbarHeight) workH -= kTaskbarHeight;
        kos::ui::SetWorkArea(0, workY, fb.width, workH);
    }
    // One-time delayed logger unmute so boot logs and ServiceManager's status
    // don't clobber the initial prompt line in the GUI terminal.
    static bool s_unmuted = false;
    static uint32_t s_unmute_delay = 0;
    if (!s_unmuted) {
        if (++s_unmute_delay >= 2) { // wait ~2 ticks (~66ms) after Start
            kos::console::Logger::MuteTTY(false);
            s_unmuted = true;
        }
    }
    int mx, my; uint8_t mb; kos::ui::GetMouseState(mx, my, mb);
    // Ensure focus exists: prefer login when active, else terminal
    if (kos::ui::GetFocusedWindow() == 0) {
        if (s_login_active && s_login_win_id) kos::ui::SetFocusedWindow(s_login_win_id);
        else if (kos::gfx::Terminal::IsActive()) kos::ui::SetFocusedWindow(kos::gfx::Terminal::GetWindowId());
    }
    bool left = (mb & 1u) != 0;
    static bool s_prev_left = false;
    // Blocking login mode: no desktop interaction until auth succeeds.
    if (s_login_active) {
        if (s_login_win_id) {
            kos::ui::BringToFront(s_login_win_id);
            kos::ui::SetFocusedWindow(s_login_win_id);
        }
        if (left && !s_prev_left) {
            kos::ui::LoginScreen::OnPointerDown(mx, my);
        }
        // Let login issue power actions
        kos::ui::LoginAction la = kos::ui::LoginScreen::ConsumePendingAction();
        if (la == kos::ui::LoginAction::Reboot) {
            TaskbarRebootSystem();
        } else if (la == kos::ui::LoginAction::Shutdown) {
            TaskbarShutdownSystem();
        }
    } else {
        // Taskbar gets first dibs on clicks; otherwise defer to UI interactions
        if (left) {
            uint32_t tbWin = TaskbarHitTest(mx, my);
            if (tbWin == kTaskbarSpawnBtnId) {
                WindowManager::SpawnTerminal();
            } else if (tbWin == kTaskbarAppBtnId) {
                ShowAppMenuWindow();
            } else if (tbWin == kTaskbarRebootBtnId) {
                TaskbarRebootSystem();
            } else if (tbWin) {
                if (kos::ui::GetWindowState(tbWin) == kos::ui::WindowState::Minimized) {
                    kos::ui::RestoreWindow(tbWin);
                    kos::ui::BringToFront(tbWin);
                    kos::ui::SetFocusedWindow(tbWin);
                } else {
                    if (kos::ui::GetFocusedWindow() == tbWin) {
                        kos::ui::MinimizeWindow(tbWin);
                    } else {
                        kos::ui::BringToFront(tbWin);
                        kos::ui::SetFocusedWindow(tbWin);
                    }
                }
            } else {
                kos::ui::UpdateInteractions();
            }
        } else {
            kos::ui::UpdateInteractions();
        }
    }

    // Dispatch keyboard/mouse events that were captured by real drivers.
    DispatchQueuedInputEvents();
    
    // Check if app menu selection was made
    if (s_app_menu_component.IsSelectionPending()) {
        ProcessAppMenuSelection();
        s_app_menu_component.ClearSelectionPending();
    }

    s_prev_left = left;
    // Ensure login keyboard handler stays active while login is shown
    // Set handler EVERY tick to ensure it stays correct even if something else changes it
    if (s_login_active) {
        // Always set the global override - this is the most reliable method
        ::kos::g_keyboard_handler_override = &s_login_handler;
        if (::kos::g_keyboard_driver_ptr) {
            ::kos::g_keyboard_driver_ptr->SetHandler(&s_login_handler);
        }
    }
    // Consume a few UI events for diagnostics (kept lightweight to avoid spam)
    {
        kos::ui::UIEvent ev; int processed = 0; const int kMaxPerTick = 4;
        while (processed < kMaxPerTick && kos::ui::PollEvent(ev)) {
            ++processed;
            switch (ev.type) {
                case kos::ui::UIEventType::WindowClosed:
                    kos::console::Logger::Log("UI: window closed");
                    break;
                case kos::ui::UIEventType::WindowMinimized:
                    kos::console::Logger::Log("UI: window minimized");
                    break;
                case kos::ui::UIEventType::WindowRestored:
                    kos::console::Logger::Log("UI: window restored");
                    break;
                case kos::ui::UIEventType::WindowMaximized:
                    kos::console::Logger::Log("UI: window maximized");
                    break;
                case kos::ui::UIEventType::WindowFocused:
                case kos::ui::UIEventType::WindowMoved:
                case kos::ui::UIEventType::WindowResized:
                default:
                    // Skip verbose logs for frequent events
                    break;
            }
        }
    }
    // Poll mouse as a fallback only. Avoid mixing IRQ + poll streams because that can desync pointer motion.
    static uint32_t s_no_pkt_ticks = 0;
    static bool s_notified_no_irq = false;
    static bool s_seen_irq_mouse = false;
    static uint32_t s_ticks_since_irq = 0;
    if (::kos::g_mouse_driver_ptr) {
        if (::kos::g_mouse_input_source == 1) {
            s_seen_irq_mouse = true;
            s_ticks_since_irq = 0;
        } else if (s_ticks_since_irq < 0xFFFFFFFFu) {
            ++s_ticks_since_irq;
        }

        bool needPoll = false;
        if (s_mouse_poll_mode == 2) {
            // Always mode: use poll until IRQ is seen, then only if IRQ has been silent for a while.
            needPoll = (!s_seen_irq_mouse) || (s_ticks_since_irq > 8);
        } else if (s_mouse_poll_mode == 1) {
            // Compatibility mode: poll only until IRQ is seen.
            needPoll = !s_seen_irq_mouse;
        } else {
            // Never mode
            needPoll = false;
        }

        if (needPoll) {
            for (int i = 0; i < 16; ++i) {
                uint32_t before = drivers::mouse::g_mouse_packets;
                ::kos::g_mouse_driver_ptr->PollOnce();
                auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
                uint8_t status = ps2.ReadStatus();
                if ((status & 0x01) == 0 || (status & 0x20) == 0) {
                    break;
                }
                if (drivers::mouse::g_mouse_packets != before) {
                    break;
                }
            }
        }
        if (drivers::mouse::g_mouse_packets == 0) {
            if (!s_notified_no_irq) {
                if (++s_no_pkt_ticks >= 60) { // ~2 seconds at 33ms/tick
                    kos::console::Logger::Log("WINMAN: no mouse IRQ packets yet; polling fallback active");
                    s_notified_no_irq = true;
                }
            }
        } else {
            s_no_pkt_ticks = 0; s_notified_no_irq = true; // we have packets; suppress notice
        }
    }
    // Keyboard fallback polling in graphics mode: avoid mixing IRQ + poll paths.
    // Use polling only until IRQ input is seen, or if IRQ goes silent for a while.
    static bool s_seen_irq_kbd = false;
    static uint32_t s_ticks_since_kbd_irq = 0;
    if (::kos::g_kbd_input_source == 1) {
        s_seen_irq_kbd = true;
        s_ticks_since_kbd_irq = 0;
    } else if (s_ticks_since_kbd_irq < 0xFFFFFFFFu) {
        ++s_ticks_since_kbd_irq;
    }

    bool needKbdPoll = (!s_seen_irq_kbd) || (s_ticks_since_kbd_irq > 8);
    if (::kos::g_keyboard_driver_ptr && needKbdPoll) {
        for (int i = 0; i < 16 && ::kos::g_keyboard_driver_ptr->PollOnce(); ++i) {
            // Drain buffered keyboard bytes while in fallback mode.
        }
    }

    // Input watchdog: if both keyboard and mouse remain idle for several seconds,
    // re-assert PS/2 enable/reporting commands to recover from lost controller state.
    {
        static uint32_t s_prev_kbd_ev_wd = 0;
        static uint32_t s_prev_mouse_pk_wd = 0;
        static uint32_t s_idle_ticks = 0;

        const uint32_t curK = ::kos::g_kbd_events;
        const uint32_t curM = ::kos::drivers::mouse::g_mouse_packets;
        if (curK == s_prev_kbd_ev_wd && curM == s_prev_mouse_pk_wd) {
            ++s_idle_ticks;
        } else {
            s_idle_ticks = 0;
            s_prev_kbd_ev_wd = curK;
            s_prev_mouse_pk_wd = curM;
        }

        if (s_idle_ticks >= 150) { // ~5 seconds at 30Hz
            s_idle_ticks = 0;
            auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();

            // Ensure both PS/2 ports are enabled
            ps2.WaitWrite(); ps2.WriteCommand(0xAE); // enable keyboard port
            ps2.WaitWrite(); ps2.WriteCommand(0xA8); // enable mouse port

            // Ensure controller command byte keeps IRQ1/IRQ12 and both clocks enabled
            ps2.WaitWrite(); ps2.WriteCommand(0x20);
            ps2.WaitRead();
            uint8_t cfg = ps2.ReadData();
            cfg |= 0x03;    // IRQ1 + IRQ12
            cfg &= ~0x30u;  // enable clocks for ports 1 and 2
            ps2.WaitWrite(); ps2.WriteCommand(0x60);
            ps2.WaitWrite(); ps2.WriteData(cfg);

            // Re-enable keyboard scanning
            ps2.WaitWrite(); ps2.WriteData(0xF4);
            for (int i = 0; i < 50000; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    (void)ps2.ReadData();
                    break;
                }
            }

            // Re-enable mouse reporting
            ps2.WaitWrite(); ps2.WriteCommand(0xD4);
            ps2.WaitWrite(); ps2.WriteData(0xF4);
            for (int i = 0; i < 50000; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    (void)ps2.ReadData();
                    break;
                }
            }
        }
    }

    const uint32_t wallpaper = 0xFF1E1E20u; // dark gray background
    kos::gfx::Compositor::BeginFrame(s_login_active ? 0xFF16061Eu : wallpaper);
    if (s_login_active) RenderLoginBackdrop();
    // Render frame and contents in z-order
    kos::ui::RenderAll();
    RenderWindowContentsByZOrder();

    // Login transition (state logic kept after rendering)
    if (s_login_active) {
        // If authenticated, transition to normal terminal UI
        if (kos::ui::LoginScreen::Authenticated()) {
            // Close login window
            if (s_login_win_id) { kos::ui::CloseWindow(s_login_win_id); s_login_win_id = 0; }
            s_login_active = false;
            // Restore keyboard handler to shell - clear global override first
            ::kos::g_keyboard_handler_override = nullptr;
            if (!s_shell_started) {
                if (kos::console::ThreadedShellAPI::InitializeShell()) {
                    uint32_t shellTid = kos::process::ThreadManagerAPI::CreateSystemThread(
                        (void*)deferred_shell_thread_entry,
                        kos::process::THREAD_SYSTEM_SERVICE, 8192, kos::process::PRIORITY_NORMAL, "shell");
                    if (shellTid) {
                        s_shell_started = true;
                        kos::console::ThreadedShellAPI::ProcessKeyInput('\n');
                    }
                }
            }
            static kos::console::ShellKeyboardHandler s_shell_handler;
            if (::kos::g_keyboard_driver_ptr) {
                ::kos::g_keyboard_driver_ptr->SetHandler(&s_shell_handler);
            }
            // Create desktop windows only after successful authentication.
            CreatePostLoginWindows();
            // Spawn terminal now
            WindowManager::SpawnTerminal();
        }
    }
    // Build taskbar model and render taskbar only after login.
    if (s_taskbar_enabled && !s_login_active) {
        BuildTaskbarButtons();
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t barY = fbinfo.height - kTaskbarHeight;
        // Retro 8-bit taskbar palette.
        FillCheckerRect(0, barY, fbinfo.width, kTaskbarHeight, 0xFF122A52u, 0xFF153160u, 2);
        kos::gfx::Compositor::FillRect(0, barY, fbinfo.width, 1, 0xFF8FC2FFu);
        kos::gfx::Compositor::FillRect(0, barY + kTaskbarHeight - 1, fbinfo.width, 1, 0xFF00183Cu);
        // Spawn '+' button at far left
        {
            uint32_t x = kTaskbarPad;
            uint32_t y = barY + (kTaskbarHeight - kTaskButtonH)/2;
            uint32_t w = 20, h = kTaskButtonH;
            uint32_t base = 0xFF1E3E78u;
            FillCheckerRect(x, y, w, h, base, 0xFF22488Au, 2);
            // Outline
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF00183Cu);
            // Draw '+' centered using 8x8 glyphs: '+' is ASCII 0x2B
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['+' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx + 1, gy + 1, glyph, 0xFF0A1020u, base);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFE26Fu, base);
        }
        // App launcher button ('A') next to '+'
        {
            uint32_t x = kTaskbarPad + 20 + 4;
            uint32_t y = barY + (kTaskbarHeight - kTaskButtonH)/2;
            uint32_t w = 20, h = kTaskButtonH;
            uint32_t base = 0xFF1E3E78u;
            FillCheckerRect(x, y, w, h, base, 0xFF22488Au, 2);
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF00183Cu);
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['A' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx + 1, gy + 1, glyph, 0xFF0A1020u, base);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFE26Fu, base);
        }
        // Reboot button at far right
        {
            uint32_t w = 28, h = kTaskButtonH;
            uint32_t x = fbinfo.width - kTaskbarPad - w;
            uint32_t y = barY + (kTaskbarHeight - h)/2;
            uint32_t base = 0xFF1E3E78u;
            FillCheckerRect(x, y, w, h, base, 0xFF22488Au, 2);
            // Outline
            kos::gfx::Compositor::FillRect(x, y, w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(x, y, 1, h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, 0xFF00183Cu);
            // Draw "R" centered.
            const uint8_t* glyph = kos::gfx::kFont8x8Basic['R' - 32];
            uint32_t gx = x + (w/2) - 4;
            uint32_t gy = y + (h/2) - 4;
            kos::gfx::Compositor::DrawGlyph8x8(gx + 1, gy + 1, glyph, 0xFF0A1020u, base);
            kos::gfx::Compositor::DrawGlyph8x8(gx, gy, glyph, 0xFFFFE26Fu, base);
        }
        // Render buttons
        for (uint32_t i=0;i<s_taskButtonCount;++i) {
            const auto& b = s_taskButtons[i];
            uint32_t base = b.focused ? 0xFF2E5AA8u : (b.minimized ? 0xFF1A2D4Eu : 0xFF1E3E78u);
            uint32_t alt = b.focused ? 0xFF3364B8u : (b.minimized ? 0xFF1D345Au : 0xFF22488Au);
            FillCheckerRect(b.x, b.y, b.w, b.h, base, alt, 2);
            // Outline
            kos::gfx::Compositor::FillRect(b.x, b.y, b.w, 1, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(b.x, b.y + b.h - 1, b.w, 1, 0xFF00183Cu);
            kos::gfx::Compositor::FillRect(b.x, b.y, 1, b.h, 0xFF8FC2FFu);
            kos::gfx::Compositor::FillRect(b.x + b.w - 1, b.y, 1, b.h, 0xFF00183Cu);
            // Title glyphs truncated
            kos::gfx::WindowDesc wd; kos::ui::GetWindowDesc(b.winId, wd);
            const char* title = wd.title ? wd.title : "(untitled)";
            uint32_t maxChars = (b.w - 8) / 8; // padding left=4 right=4
            uint32_t tx = b.x + 4; uint32_t ty = b.y + (b.h/2) - 4;
            for (uint32_t ci=0; title[ci] && ci < maxChars; ++ci) {
                char ch = title[ci]; if (ch < 32 || ch > 127) ch='?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
                kos::gfx::Compositor::DrawGlyph8x8(tx + ci*8 + 1, ty + 1, glyph, 0xFF0A1020u, base);
                kos::gfx::Compositor::DrawGlyph8x8(tx + ci*8, ty, glyph, 0xFFFFE26Fu, base);
            }
        }
    }
    // DEBUG OVERLAY (live)
    if (!s_login_active) {
        const auto& fbinfo = kos::gfx::GetInfo();
        uint32_t winCount = kos::ui::GetWindowCount();
        bool term = kos::gfx::Terminal::IsActive();
        char buf[160];
        int bi=0; auto put=[&](char c){ if(bi<159) buf[bi++]=c; };
        auto writeStr=[&](const char* s){ while(*s) put(*s++); };
        auto writeDec=[&](uint32_t v){ char tmp[16]; int n=0; if(v==0){tmp[n++]='0';} else { char r[16]; int ri=0; while(v&&ri<16){ r[ri++]=char('0'+(v%10)); v/=10;} while(ri) tmp[n++]=r[--ri]; } for(int i=0;i<n;++i) put(tmp[i]); };
        writeStr("FB "); writeDec(fbinfo.width); put('x'); writeDec(fbinfo.height); put(' '); writeDec(fbinfo.bpp); writeStr(" win:"); writeDec(winCount); writeStr(" term:"); writeStr(term?"Y":"N"); writeStr(" foc:"); writeDec(kos::ui::GetFocusedWindow()); buf[bi]=0;
        for (int i=0; buf[i]; ++i) {
            char ch = buf[i]; if (ch < 32 || ch > 127) ch='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[ch - 32];
            kos::gfx::Compositor::DrawGlyph8x8(4 + i*8, 4, glyph, 0xFFFFFFFFu, wallpaper);
        }
    }
    // Optional animation: move the demo window horizontally (simple effect)
    // For now, leave static to keep it predictable.
    // Render cursor (style selectable) after all content
    kos::ui::RenderCursor();
    kos::gfx::Compositor::EndFrame();
}

}} // namespace
