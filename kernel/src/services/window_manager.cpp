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
static uint32_t s_prev_total_runtime = 0;
static uint32_t s_prev_idle_runtime = 0;
static uint8_t  s_last_cpu_pct = 0;
static constexpr uint32_t kDesktopClockTopInset = 56u; // reserved top strip to avoid overlap with top widgets

// ========== File Browser window state ==========
static uint32_t s_file_browser_win_id = 0;
static char     s_fb_path[128] = { '/', 0 };  // current absolute path
static kos::fs::DirEntry s_fb_entries[64];
static uint32_t s_fb_entry_count = 0;
static uint32_t s_fb_page = 0;
static int32_t  s_fb_selected = -1;           // selected entry index (-1 = none)
static bool     s_fb_needs_refresh = true;
static constexpr uint32_t kFbRowH      = 12u;
static constexpr uint32_t kFbPageSize  = 16u;
static constexpr uint32_t kFbSidebarW  = 80u;
static constexpr uint32_t kFbHeaderH   = 16u;
struct FbBookmark { const char* label; const char* path; };
static const FbBookmark kFbBookmarks[] = {
    { "Root /",   "/" },
    { "/bin",     "/bin" },
    { "/etc",     "/etc" },
    { "/var",     "/var" },
    { "/var/log", "/var/log" },
};
static constexpr uint32_t kFbBookmarkCount = 5u;
static kos::gfx::Rect s_fb_up_btn           = {0,0,0,0};
static kos::gfx::Rect s_fb_sidebar_btns[kFbBookmarkCount];
static kos::gfx::Rect s_fb_item_rects[kFbPageSize];
static kos::gfx::Rect s_fb_prev_btn_r       = {0,0,0,0};
static kos::gfx::Rect s_fb_next_btn_r       = {0,0,0,0};
static uint32_t       s_fb_visible_count    = 0;
static bool           s_fb_pager_visible    = false;

static void RenderFileBrowserContent();
static void RenderHardwareInfoWindowContent();
static void RenderClockWindowContent();
static void RenderSystemHudContent();
static void RenderProcessMonitorWindowContent();

class LegacyRenderComponent : public kos::ui::IUIComponent {
public:
    using RenderFn = void (*)();

    LegacyRenderComponent() : m_window_id(0), m_name("Legacy"), m_render_fn(nullptr) {}

    void Bind(uint32_t windowId, const char* name, RenderFn renderFn) {
        m_window_id = windowId;
        m_name = name;
        m_render_fn = renderFn;
    }

    virtual uint32_t GetWindowId() const override { return m_window_id; }
    virtual void Render() override { if (m_render_fn) m_render_fn(); }
    virtual bool OnInputEvent(const kos::input::InputEvent&) override { return false; }
    virtual const char* GetName() const override { return m_name; }

private:
    uint32_t m_window_id;
    const char* m_name;
    RenderFn m_render_fn;
};

static LegacyRenderComponent s_legacy_components[8];
static uint32_t s_legacy_component_count = 0;
static kos::ui::ClockComponent s_clock_component;
static kos::ui::SystemHudComponent s_system_hud_component;
static kos::ui::MouseDiagnosticComponent s_mouse_component;

static void RegisterLegacyComponent(uint32_t windowId, const char* name, LegacyRenderComponent::RenderFn renderFn) {
    if (windowId == 0 || renderFn == nullptr) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    if (s_legacy_component_count >= 8u) return;

    LegacyRenderComponent& c = s_legacy_components[s_legacy_component_count++];
    c.Bind(windowId, name, renderFn);
    (void)reg.RegisterComponent(&c, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &c);
}

static void RegisterClockComponent(uint32_t windowId) {
    if (windowId == 0) return;
    kos::ui::WindowRegistry& reg = kos::ui::WindowRegistry::Instance();
    if (reg.GetComponent(windowId) != nullptr) return;
    s_clock_component.BindWindow(windowId);
    (void)reg.RegisterComponent(&s_clock_component, windowId);
    (void)kos::ui::SetWindowComponent(windowId, &s_clock_component);
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

struct PciDeviceSummary {
    bool valid;
    uint8_t bus;
    uint8_t dev;
    uint8_t fn;
    uint16_t vendor;
    uint16_t device;
    uint8_t subclass;
};

enum class HardwareTab : uint8_t {
    Overview = 0,
    CPU,
    Graphics,
    Memory,
    Storage,
    Network,
    PCI,
    Controllers,
    Count
};

struct PciListEntry {
    uint8_t bus;
    uint8_t dev;
    uint8_t fn;
    uint8_t cls;
    uint8_t subclass;
    uint8_t progIf;
    uint16_t vendor;
    uint16_t device;
};

struct HardwareSnapshot {
    char cpuVendor[13];
    char cpuBrand[49];
    uint32_t cpuFamily;
    uint32_t cpuModel;
    uint32_t cpuStepping;
    uint32_t cpuLogical;

    uint32_t totalFrames;
    uint32_t freeFrames;
    uint32_t heapSize;
    uint32_t heapUsed;

    PciDeviceSummary display;
    uint32_t displayCount;
    PciDeviceSummary storage;
    uint32_t storageCount;
    PciDeviceSummary network;
    uint32_t networkCount;

    bool vmsvgaReady;
    const char* renderBackend;
    bool keyboardDriverReady;
    bool mouseDriverReady;
    const char* keyboardInput;
    const char* mouseInput;
    uint32_t mousePackets;
    bool filesystemMounted;
};

static HardwareSnapshot s_hw_snapshot{};
static HardwareTab s_hw_active_tab = HardwareTab::Overview;
static char s_hw_lines[24][112];
static uint32_t s_hw_line_count = 0;
static PciListEntry s_pci_entries[96];
static uint32_t s_pci_entry_count = 0;
static uint32_t s_hw_pci_page = 0;
static constexpr uint32_t kPciPageSize = 9u;
static kos::gfx::Rect s_hw_pci_prev_btn = {0,0,0,0};
static kos::gfx::Rect s_hw_pci_next_btn = {0,0,0,0};
static bool s_hw_pci_btns_visible = false;
static uint32_t s_hw_refresh_ticks = 0;
static constexpr uint32_t kHwTabStripH = 18u;
static constexpr uint32_t kHwTabs = static_cast<uint32_t>(HardwareTab::Count);
static const char* s_hw_tab_titles[kHwTabs] = {
    "Overview", "CPU", "Graphics", "Memory", "Storage", "Network", "PCI", "Controllers"
};

static const char* PciVendorName(uint16_t vendor, uint16_t device);

static void FillCheckerRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t cA, uint32_t cB, uint32_t cell = 2);

static inline uint32_t PciCfgRead(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return kos::sys::pci_cfg_read(bus, device, function, offset);
}

static void AppendHwLine(const char* line) {
    if (s_hw_line_count >= 24 || line == nullptr) return;
    char* dst = s_hw_lines[s_hw_line_count++];
    uint32_t i = 0;
    while (line[i] && i < 111u) {
        dst[i] = line[i];
        ++i;
    }
    dst[i] = 0;
}

static void AppendHwKV(const char* key, const char* value) {
    if (!key) key = "";
    if (!value) value = "-";
    char line[112];
    kos::sys::snprintf(line, sizeof(line), "%s\t%s", key, value);
    AppendHwLine(line);
}

static void BufPutChar(char* out, uint32_t outSize, uint32_t& p, char c) {
    if (p + 1u >= outSize) return;
    out[p++] = c;
}

static void BufPutStr(char* out, uint32_t outSize, uint32_t& p, const char* s) {
    if (!s) return;
    while (*s) {
        if (p + 1u >= outSize) break;
        out[p++] = *s++;
    }
}

static void BufPutDec(char* out, uint32_t outSize, uint32_t& p, uint32_t v) {
    char rev[12];
    uint32_t ri = 0;
    if (v == 0u) rev[ri++] = '0';
    while (v && ri < sizeof(rev)) {
        rev[ri++] = static_cast<char>('0' + (v % 10u));
        v /= 10u;
    }
    while (ri) BufPutChar(out, outSize, p, rev[--ri]);
}

static void BufPutHexFixed(char* out, uint32_t outSize, uint32_t& p, uint32_t v, uint32_t digits) {
    static const char* kHex = "0123456789ABCDEF";
    if (digits == 0u) return;
    for (int32_t i = static_cast<int32_t>(digits) - 1; i >= 0; --i) {
        uint32_t shift = static_cast<uint32_t>(i) * 4u;
        BufPutChar(out, outSize, p, kHex[(v >> shift) & 0xFu]);
    }
}

static void FormatPciLocation(char* out, uint32_t outSize, uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t p = 0;
    BufPutHexFixed(out, outSize, p, bus, 2);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, fn);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

static void FormatPciId(char* out, uint32_t outSize, uint16_t vendor, uint16_t device) {
    uint32_t p = 0;
    BufPutHexFixed(out, outSize, p, vendor, 4);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, device, 4);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

static void FormatPciSummary(char* out, uint32_t outSize, const char* vendorName,
                             uint8_t bus, uint8_t dev, uint8_t fn,
                             uint16_t vendor, uint16_t device) {
    uint32_t p = 0;
    BufPutStr(out, outSize, p, vendorName);
    BufPutStr(out, outSize, p, " | pci ");
    BufPutHexFixed(out, outSize, p, bus, 2);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, fn);
    BufPutStr(out, outSize, p, " | ");
    BufPutHexFixed(out, outSize, p, vendor, 4);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, device, 4);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

static void FormatPciClassLine(char* out, uint32_t outSize,
                               uint16_t vendor, uint16_t device,
                               uint8_t cls, uint8_t subclass, uint8_t progIf,
                               const char* vendorName) {
    uint32_t p = 0;
    BufPutHexFixed(out, outSize, p, vendor, 4);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, device, 4);
    BufPutStr(out, outSize, p, "  cls=");
    BufPutHexFixed(out, outSize, p, cls, 2);
    BufPutChar(out, outSize, p, '/');
    BufPutHexFixed(out, outSize, p, subclass, 2);
    BufPutStr(out, outSize, p, " if=");
    BufPutHexFixed(out, outSize, p, progIf, 2);
    BufPutStr(out, outSize, p, "  ");
    BufPutStr(out, outSize, p, vendorName);
    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

static void BufPadTo(char* out, uint32_t outSize, uint32_t& p, uint32_t col) {
    while (p < col) BufPutChar(out, outSize, p, ' ');
}

static void FormatPciTableRow(char* out, uint32_t outSize, const PciListEntry& e) {
    uint32_t p = 0;
    // BDF column (starts at 0)
    BufPutHexFixed(out, outSize, p, e.bus, 2);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, e.dev, 2);
    BufPutChar(out, outSize, p, '.');
    BufPutDec(out, outSize, p, e.fn);

    // VID:DID column (starts at 9)
    BufPadTo(out, outSize, p, 9u);
    BufPutHexFixed(out, outSize, p, e.vendor, 4);
    BufPutChar(out, outSize, p, ':');
    BufPutHexFixed(out, outSize, p, e.device, 4);

    // CLS column (starts at 19)
    BufPadTo(out, outSize, p, 19u);
    BufPutHexFixed(out, outSize, p, e.cls, 2);
    BufPutChar(out, outSize, p, '/');
    BufPutHexFixed(out, outSize, p, e.subclass, 2);

    // IF column (starts at 25)
    BufPadTo(out, outSize, p, 25u);
    BufPutHexFixed(out, outSize, p, e.progIf, 2);

    // VENDOR column (starts at 30)
    BufPadTo(out, outSize, p, 30u);
    BufPutStr(out, outSize, p, PciVendorName(e.vendor, e.device));

    if (outSize) out[p < outSize ? p : outSize - 1u] = 0;
}

static uint32_t PciClassRowColor(uint8_t cls) {
    switch (cls) {
        case 0x01: return 0xFFFFD27Au; // Storage
        case 0x02: return 0xFF8BF7B2u; // Network
        case 0x03: return 0xFF8EC8FFu; // Display
        case 0x0C: return 0xFF9EE5E7u; // Serial bus
        default:   return 0xFFEAF4FFu;
    }
}

static const char* PciClassName(uint8_t cls, uint8_t subclass) {
    if (cls == 0x01) {
        if (subclass == 0x01) return "IDE";
        if (subclass == 0x06) return "SATA";
        if (subclass == 0x08) return "NVMe";
        return "Storage";
    }
    if (cls == 0x02) return "Network";
    if (cls == 0x03) return "Display";
    if (cls == 0x0C) return "SerialBus";
    return "Other";
}

static const char* PciVendorName(uint16_t vendor, uint16_t device) {
    if (vendor == 0x8086u) {
        if (device == 0x100Eu) return "Intel e1000";
        return "Intel";
    }
    if (vendor == 0x10ECu) {
        if (device == 0x8169u || device == 0x8168u) return "Realtek RTL816x";
        if (device == 0x8139u) return "Realtek RTL8139";
        if (device == 0xB822u || device == 0xB828u) return "Realtek RTL8822BE";
        return "Realtek";
    }
    if (vendor == 0x15ADu) return "VMware";
    if (vendor == 0x1234u) return "QEMU";
    if (vendor == 0x80EEu) return "VirtualBox";
    return "Unknown";
}

static void ScanPciClass(uint8_t wantedClass, PciDeviceSummary& first, uint32_t& count) {
    first.valid = false;
    first.bus = first.dev = first.fn = 0;
    first.vendor = first.device = 0;
    first.subclass = 0;
    count = 0;

    for (uint8_t bus = 0; bus < 8; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            uint8_t headerType = static_cast<uint8_t>(PciCfgRead(bus, dev, 0, 0x0E));
            uint8_t functions = (headerType & 0x80u) ? 8u : 1u;
            for (uint8_t fn = 0; fn < functions; ++fn) {
                uint16_t vendor = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x00));
                if (vendor == 0xFFFFu) {
                    if (fn == 0u) break;
                    continue;
                }
                uint8_t cls = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0B));
                if (cls != wantedClass) continue;
                uint16_t device = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x02));
                uint8_t subclass = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0A));
                ++count;
                if (!first.valid) {
                    first.valid = true;
                    first.bus = bus;
                    first.dev = dev;
                    first.fn = fn;
                    first.vendor = vendor;
                    first.device = device;
                    first.subclass = subclass;
                }
            }
        }
    }
}

static void ScanPciAllDevices() {
    s_pci_entry_count = 0;
    for (uint8_t bus = 0; bus < 8 && s_pci_entry_count < 96u; ++bus) {
        for (uint8_t dev = 0; dev < 32 && s_pci_entry_count < 96u; ++dev) {
            uint8_t headerType = static_cast<uint8_t>(PciCfgRead(bus, dev, 0, 0x0E));
            uint8_t functions = (headerType & 0x80u) ? 8u : 1u;
            for (uint8_t fn = 0; fn < functions && s_pci_entry_count < 96u; ++fn) {
                uint16_t vendor = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x00));
                if (vendor == 0xFFFFu) {
                    if (fn == 0u) break;
                    continue;
                }
                PciListEntry& e = s_pci_entries[s_pci_entry_count++];
                e.bus = bus;
                e.dev = dev;
                e.fn = fn;
                e.vendor = vendor;
                e.device = static_cast<uint16_t>(PciCfgRead(bus, dev, fn, 0x02));
                e.cls = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0B));
                e.subclass = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x0A));
                e.progIf = static_cast<uint8_t>(PciCfgRead(bus, dev, fn, 0x09));
            }
        }
    }
}

static void CpuidRegs(uint32_t eaxIn, uint32_t ecxIn, uint32_t regs[4]) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(eaxIn), "c"(ecxIn));
}

static const char* InputSourceName(uint8_t src) {
    if (src == 1) return "IRQ";
    if (src == 2) return "POLL";
    return "none";
}

static void RefreshHardwareSnapshot() {
    uint32_t regs[4] = {0, 0, 0, 0};
    for (int i = 0; i < 13; ++i) s_hw_snapshot.cpuVendor[i] = 0;
    for (int i = 0; i < 49; ++i) s_hw_snapshot.cpuBrand[i] = 0;

    CpuidRegs(0, 0, regs);
    uint32_t maxBasic = regs[0];
    reinterpret_cast<uint32_t*>(s_hw_snapshot.cpuVendor)[0] = regs[1];
    reinterpret_cast<uint32_t*>(s_hw_snapshot.cpuVendor)[1] = regs[3];
    reinterpret_cast<uint32_t*>(s_hw_snapshot.cpuVendor)[2] = regs[2];
    s_hw_snapshot.cpuVendor[12] = 0;

    CpuidRegs(0x80000000u, 0, regs);
    uint32_t maxExt = regs[0];
    if (maxExt >= 0x80000004u) {
        uint32_t* p = reinterpret_cast<uint32_t*>(s_hw_snapshot.cpuBrand);
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
            CpuidRegs(leaf, 0, regs);
            *p++ = regs[0]; *p++ = regs[1]; *p++ = regs[2]; *p++ = regs[3];
        }
    }

    s_hw_snapshot.cpuFamily = 0;
    s_hw_snapshot.cpuModel = 0;
    s_hw_snapshot.cpuStepping = 0;
    s_hw_snapshot.cpuLogical = 1;
    if (maxBasic >= 1) {
        CpuidRegs(1, 0, regs);
        uint32_t eax = regs[0];
        uint32_t ebx = regs[1];
        s_hw_snapshot.cpuStepping = eax & 0xFu;
        s_hw_snapshot.cpuModel = (eax >> 4) & 0xFu;
        s_hw_snapshot.cpuFamily = (eax >> 8) & 0xFu;
        uint32_t extModel = (eax >> 16) & 0xFu;
        uint32_t extFamily = (eax >> 20) & 0xFFu;
        if (s_hw_snapshot.cpuFamily == 0xFu) s_hw_snapshot.cpuFamily += extFamily;
        if (s_hw_snapshot.cpuFamily == 0x6u || s_hw_snapshot.cpuFamily == 0xFu) s_hw_snapshot.cpuModel += (extModel << 4);
        s_hw_snapshot.cpuLogical = (ebx >> 16) & 0xFFu;
        if (s_hw_snapshot.cpuLogical == 0) s_hw_snapshot.cpuLogical = 1;
    }

    const char* cpuBrand = s_hw_snapshot.cpuBrand;
    while (*cpuBrand == ' ') ++cpuBrand;
    if (*cpuBrand == 0) {
        const char* unk = "Unknown CPU";
        uint32_t i = 0;
        for (; unk[i] && i < sizeof(s_hw_snapshot.cpuBrand) - 1; ++i) s_hw_snapshot.cpuBrand[i] = unk[i];
        s_hw_snapshot.cpuBrand[i] = 0;
    }

    s_hw_snapshot.totalFrames = 0;
    s_hw_snapshot.freeFrames = 0;
    s_hw_snapshot.heapSize = 0;
    s_hw_snapshot.heapUsed = 0;
    if (kos::sys::table()) {
        if (kos::sys::table()->get_total_frames) s_hw_snapshot.totalFrames = kos::sys::table()->get_total_frames();
        if (kos::sys::table()->get_free_frames) s_hw_snapshot.freeFrames = kos::sys::table()->get_free_frames();
        if (kos::sys::table()->get_heap_size) s_hw_snapshot.heapSize = kos::sys::table()->get_heap_size();
        if (kos::sys::table()->get_heap_used) s_hw_snapshot.heapUsed = kos::sys::table()->get_heap_used();
    }
    ScanPciClass(0x03u, s_hw_snapshot.display, s_hw_snapshot.displayCount);
    ScanPciClass(0x01u, s_hw_snapshot.storage, s_hw_snapshot.storageCount);
    ScanPciClass(0x02u, s_hw_snapshot.network, s_hw_snapshot.networkCount);
    ScanPciAllDevices();

    s_hw_snapshot.vmsvgaReady = kos::drivers::gpu::vmsvga::IsReady();
    s_hw_snapshot.renderBackend = kos::gfx::Compositor::BackendName();
    s_hw_snapshot.keyboardDriverReady = (::kos::g_keyboard_driver_ptr != nullptr);
    s_hw_snapshot.mouseDriverReady = (::kos::g_mouse_driver_ptr != nullptr);
    s_hw_snapshot.keyboardInput = InputSourceName(::kos::g_kbd_input_source);
    s_hw_snapshot.mouseInput = InputSourceName(::kos::g_mouse_input_source);
    s_hw_snapshot.mousePackets = ::kos::drivers::mouse::g_mouse_packets;
    s_hw_snapshot.filesystemMounted = (::kos::fs::g_fs_ptr != nullptr);
}

static void BuildHardwareInfoLinesForTab(HardwareTab tab) {
    s_hw_line_count = 0;
    char buf[112];

    auto appendPciPrimary = [&](const char* key, const PciDeviceSummary& dev, uint32_t count, const char* fallback) {
        if (!dev.valid) {
            AppendHwKV(key, fallback);
            return;
        }
        FormatPciSummary(buf, sizeof(buf), PciVendorName(dev.vendor, dev.device),
                         dev.bus, dev.dev, dev.fn, dev.vendor, dev.device);
        AppendHwKV(key, buf);
        kos::sys::snprintf(buf, sizeof(buf), "%u detected", count);
        AppendHwKV("Count", buf);
    };

    uint32_t usedFrames = (s_hw_snapshot.totalFrames >= s_hw_snapshot.freeFrames) ? (s_hw_snapshot.totalFrames - s_hw_snapshot.freeFrames) : 0u;
    uint32_t totalMiB = s_hw_snapshot.totalFrames / 256u;
    uint32_t freeMiB = s_hw_snapshot.freeFrames / 256u;
    uint32_t usedMiB = usedFrames / 256u;

    switch (tab) {
        case HardwareTab::Overview:
            AppendHwLine("System Hardware Dashboard");
            AppendHwKV("CPU", s_hw_snapshot.cpuBrand);
            kos::sys::snprintf(buf, sizeof(buf), "%s | %u logical cores", s_hw_snapshot.cpuVendor, s_hw_snapshot.cpuLogical);
            AppendHwKV("CPU Detail", buf);
            appendPciPrimary("Graphics", s_hw_snapshot.display, s_hw_snapshot.displayCount, "No PCI display adapter");
            kos::sys::snprintf(buf, sizeof(buf), "%u/%u MiB used  (%u MiB free)", usedMiB, totalMiB, freeMiB);
            AppendHwKV("Memory", buf);
            appendPciPrimary("Storage", s_hw_snapshot.storage, s_hw_snapshot.storageCount, "No PCI storage controller");
            appendPciPrimary("Network", s_hw_snapshot.network, s_hw_snapshot.networkCount, "No PCI network adapter");
            kos::sys::snprintf(buf, sizeof(buf), "render=%s  vmsvga=%s", s_hw_snapshot.renderBackend,
                               s_hw_snapshot.vmsvgaReady ? "ready" : "off");
            AppendHwKV("Drivers", buf);
            break;
        case HardwareTab::CPU:
            AppendHwLine("CPU Information");
            AppendHwKV("Model", s_hw_snapshot.cpuBrand);
            AppendHwKV("Vendor", s_hw_snapshot.cpuVendor);
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.cpuFamily);
            AppendHwKV("Family", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.cpuModel);
            AppendHwKV("Model ID", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.cpuStepping);
            AppendHwKV("Stepping", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.cpuLogical);
            AppendHwKV("Logical CPUs", buf);
            break;
        case HardwareTab::Graphics:
            AppendHwLine("Graphics Adapter");
            kos::sys::snprintf(buf, sizeof(buf), "%s", s_hw_snapshot.renderBackend);
            AppendHwKV("Render Backend", buf);
            AppendHwKV("VMSVGA", s_hw_snapshot.vmsvgaReady ? "Ready" : "Disabled");
            if (s_hw_snapshot.display.valid) {
                kos::sys::snprintf(buf, sizeof(buf), "%s", PciVendorName(s_hw_snapshot.display.vendor, s_hw_snapshot.display.device));
                AppendHwKV("Primary Adapter", buf);
                FormatPciLocation(buf, sizeof(buf), s_hw_snapshot.display.bus, s_hw_snapshot.display.dev, s_hw_snapshot.display.fn);
                AppendHwKV("PCI Location", buf);
                FormatPciId(buf, sizeof(buf), s_hw_snapshot.display.vendor, s_hw_snapshot.display.device);
                AppendHwKV("PCI ID", buf);
            } else {
                AppendHwKV("Primary Adapter", "No PCI display adapter");
            }
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.displayCount);
            AppendHwKV("Adapters Detected", buf);
            break;
        case HardwareTab::Memory:
            AppendHwLine("Memory State");
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", totalMiB);
            AppendHwKV("Physical Total", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", usedMiB);
            AppendHwKV("Physical Used", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u MiB", freeMiB);
            AppendHwKV("Physical Free", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u KiB", s_hw_snapshot.heapSize / 1024u);
            AppendHwKV("Heap Total", buf);
            kos::sys::snprintf(buf, sizeof(buf), "%u KiB", s_hw_snapshot.heapUsed / 1024u);
            AppendHwKV("Heap Used", buf);
            break;
        case HardwareTab::Storage:
            AppendHwLine("Storage Controllers");
            if (s_hw_snapshot.storage.valid) {
                AppendHwKV("Class", PciClassName(0x01u, s_hw_snapshot.storage.subclass));
                AppendHwKV("Controller", PciVendorName(s_hw_snapshot.storage.vendor, s_hw_snapshot.storage.device));
                FormatPciLocation(buf, sizeof(buf), s_hw_snapshot.storage.bus, s_hw_snapshot.storage.dev, s_hw_snapshot.storage.fn);
                AppendHwKV("PCI Location", buf);
                FormatPciId(buf, sizeof(buf), s_hw_snapshot.storage.vendor, s_hw_snapshot.storage.device);
                AppendHwKV("PCI ID", buf);
            } else {
                AppendHwKV("Controller", "No PCI storage controller");
            }
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.storageCount);
            AppendHwKV("Controllers", buf);
            break;
        case HardwareTab::Network:
            AppendHwLine("Network Adapters");
            if (s_hw_snapshot.network.valid) {
                AppendHwKV("Primary Adapter", PciVendorName(s_hw_snapshot.network.vendor, s_hw_snapshot.network.device));
                FormatPciLocation(buf, sizeof(buf), s_hw_snapshot.network.bus, s_hw_snapshot.network.dev, s_hw_snapshot.network.fn);
                AppendHwKV("PCI Location", buf);
                FormatPciId(buf, sizeof(buf), s_hw_snapshot.network.vendor, s_hw_snapshot.network.device);
                AppendHwKV("PCI ID", buf);
            } else {
                AppendHwKV("Primary Adapter", "No PCI network adapter");
            }
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.networkCount);
            AppendHwKV("Adapters", buf);
            break;
        case HardwareTab::PCI: {
            AppendHwLine("PCI Device Inventory");
            uint32_t pageCount = (s_pci_entry_count + kPciPageSize - 1u) / kPciPageSize;
            if (pageCount == 0u) pageCount = 1u;
            if (s_hw_pci_page >= pageCount) s_hw_pci_page = pageCount - 1u;
            kos::sys::snprintf(buf, sizeof(buf), "%u device(s)  page %u/%u", s_pci_entry_count, s_hw_pci_page + 1u, pageCount);
            AppendHwKV("Summary", buf);
            AppendHwLine("BDF      VID:DID   CLS   IF   VENDOR");
            AppendHwLine("--------------------------------------------");
            uint32_t start = s_hw_pci_page * kPciPageSize;
            uint32_t end = start + kPciPageSize;
            if (end > s_pci_entry_count) end = s_pci_entry_count;
            for (uint32_t i = start; i < end; ++i) {
                const PciListEntry& e = s_pci_entries[i];
                FormatPciTableRow(buf, sizeof(buf), e);
                AppendHwLine(buf);
            }
            if (s_pci_entry_count == 0u) {
                AppendHwKV("Info", "No PCI devices detected");
            }
            break;
        }
        case HardwareTab::Controllers:
            AppendHwLine("Controllers and Runtime Drivers");
            AppendHwKV("Keyboard Driver", s_hw_snapshot.keyboardDriverReady ? "Loaded" : "Not loaded");
            AppendHwKV("Mouse Driver", s_hw_snapshot.mouseDriverReady ? "Loaded" : "Not loaded");
            AppendHwKV("Keyboard Input", s_hw_snapshot.keyboardInput);
            AppendHwKV("Mouse Input", s_hw_snapshot.mouseInput);
            kos::sys::snprintf(buf, sizeof(buf), "%u", s_hw_snapshot.mousePackets);
            AppendHwKV("Mouse Packets", buf);
            AppendHwKV("Filesystem", s_hw_snapshot.filesystemMounted ? "Mounted" : "Not mounted");
            break;
        default:
            break;
    }
}

static bool GetHardwareTabLayout(const kos::gfx::WindowDesc& d,
                                 uint32_t& cx, uint32_t& cy, uint32_t& cw, uint32_t& ch,
                                 uint32_t& tabX, uint32_t& tabY, uint32_t& tabW) {
    const uint32_t flags = kos::ui::GetWindowFlags(s_hardware_info_win_id);
    const bool frameless = (flags & kos::ui::WF_Frameless) != 0u;
    cx = d.x + 1u;
    cy = d.y + (frameless ? 1u : (kos::ui::TitleBarHeight() + 1u));
    cw = (d.w > 2u) ? d.w - 2u : d.w;
    if (frameless) {
        ch = (d.h > 2u) ? d.h - 2u : d.h;
    } else {
        const uint32_t chrome = kos::ui::TitleBarHeight() + 2u;
        ch = (d.h > chrome) ? (d.h - chrome) : 0u;
    }
    if (cw < 120u || ch < 80u) return false;
    tabX = cx + 6u;
    tabY = cy + 4u;
    uint32_t availW = (cw > 12u) ? (cw - 12u) : 0u;
    if (availW < (kHwTabs * 40u)) return false;
    tabW = availW / kHwTabs;
    if (tabW < 44u) tabW = 44u;
    return true;
}

static bool HandleHardwareInfoTabClick(int mx, int my) {
    if (!s_hardware_info_win_id) return false;

    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != s_hardware_info_win_id) {
        return false;
    }

    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_hardware_info_win_id, d)) return false;

    uint32_t cx, cy, cw, ch, tabX, tabY, tabW;
    if (!GetHardwareTabLayout(d, cx, cy, cw, ch, tabX, tabY, tabW)) return false;

    uint32_t ux = static_cast<uint32_t>(mx);
    uint32_t uy = static_cast<uint32_t>(my);
    if (ux < tabX || ux >= tabX + tabW * kHwTabs || uy < tabY || uy >= tabY + kHwTabStripH) return false;

    uint32_t idx = (ux - tabX) / tabW;
    if (idx >= kHwTabs) return false;
    if (static_cast<HardwareTab>(idx) != s_hw_active_tab) {
        s_hw_pci_page = 0;
    }
    s_hw_active_tab = static_cast<HardwareTab>(idx);
    BuildHardwareInfoLinesForTab(s_hw_active_tab);
    return true;
}

static bool HandleHardwareInfoPciPagerClick(int mx, int my) {
    if (s_hw_active_tab != HardwareTab::PCI || !s_hw_pci_btns_visible) return false;

    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != s_hardware_info_win_id) {
        return false;
    }

    auto inRect = [&](const kos::gfx::Rect& r) -> bool {
        uint32_t ux = static_cast<uint32_t>(mx);
        uint32_t uy = static_cast<uint32_t>(my);
        return (ux >= r.x && ux < r.x + r.w && uy >= r.y && uy < r.y + r.h);
    };

    uint32_t pageCount = (s_pci_entry_count + kPciPageSize - 1u) / kPciPageSize;
    if (pageCount == 0u) pageCount = 1u;

    if (inRect(s_hw_pci_prev_btn) && s_hw_pci_page > 0u) {
        --s_hw_pci_page;
        BuildHardwareInfoLinesForTab(HardwareTab::PCI);
        return true;
    }
    if (inRect(s_hw_pci_next_btn) && (s_hw_pci_page + 1u) < pageCount) {
        ++s_hw_pci_page;
        BuildHardwareInfoLinesForTab(HardwareTab::PCI);
        return true;
    }
    return false;
}

// ========== File Browser helpers ==========

static uint32_t FbStrLen(const char* s) {
    uint32_t n = 0; while (s[n]) ++n; return n;
}

static void FbStrCopy(char* dst, uint32_t dstSize, const char* src) {
    uint32_t i = 0;
    while (src[i] && i + 1u < dstSize) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

static void FbGoUp() {
    uint32_t len = FbStrLen(s_fb_path);
    if (len <= 1u) return;  // already at root "/"
    int32_t i = (int32_t)len - 1;
    while (i > 0 && s_fb_path[i] != '/') --i;
    if (i == 0) { s_fb_path[1] = 0; }  // go to root
    else        { s_fb_path[i] = 0; }
    s_fb_needs_refresh = true;
    s_fb_page = 0;
    s_fb_selected = -1;
}

static void FbNavigateTo(const char* path) {
    FbStrCopy(s_fb_path, sizeof(s_fb_path), path);
    s_fb_needs_refresh = true;
    s_fb_page = 0;
    s_fb_selected = -1;
}

static void FbNavigateInto(const int8_t* entryName) {
    uint32_t baseLen = FbStrLen(s_fb_path);
    char newPath[128];
    for (uint32_t i = 0; i < baseLen && i < 127u; ++i) newPath[i] = s_fb_path[i];
    uint32_t p = baseLen;
    if (p > 0 && newPath[p-1] != '/' && p + 1u < 128u) newPath[p++] = '/';
    for (uint32_t i = 0; entryName[i] && p + 1u < 128u; ++i)
        newPath[p++] = (char)entryName[i];
    newPath[p] = 0;
    FbNavigateTo(newPath);
}

static bool FbEnumCb(const kos::fs::DirEntry* entry, void* userdata) {
    uint32_t* count = reinterpret_cast<uint32_t*>(userdata);
    if (*count < 64u) { s_fb_entries[(*count)++] = *entry; }
    return *count < 64u;
}

static void FbRefreshEntries() {
    s_fb_entry_count = 0;
    if (!kos::fs::g_fs_ptr) { s_fb_needs_refresh = false; return; }
    kos::fs::g_fs_ptr->EnumDir(
        reinterpret_cast<const int8_t*>(s_fb_path),
        FbEnumCb,
        &s_fb_entry_count);
    s_fb_needs_refresh = false;
}

static void RenderFileBrowserContent() {
    if (!s_file_browser_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_file_browser_win_id, d)) return;

    if (s_fb_needs_refresh) FbRefreshEntries();

    constexpr uint32_t kBgA      = 0xFF0F1320u;
    constexpr uint32_t kBgB      = 0xFF121728u;
    constexpr uint32_t kHdrBg    = 0xFF141C30u;
    constexpr uint32_t kSideBg   = 0xFF0C1022u;
    constexpr uint32_t kSideAct  = 0xFF1E3158u;
    constexpr uint32_t kPanel    = 0xFF111928u;
    constexpr uint32_t kSelBg    = 0xFF1A3A6Au;
    constexpr uint32_t kBorderHi = 0xFF4A6A9Au;
    constexpr uint32_t kBorderLo = 0xFF1E2E50u;
    constexpr uint32_t kFgDir    = 0xFFFFB84Du;   // amber  for folders
    constexpr uint32_t kFgFile   = 0xFFB8CDE8u;   // steel blue for files
    constexpr uint32_t kFgPath   = 0xFF7AAED8u;
    constexpr uint32_t kFgText   = 0xFFE0EAFFu;
    constexpr uint32_t kShadow   = 0xFF04070Fu;
    constexpr uint32_t kBtn      = 0xFF1B3258u;

    // Client area
    const uint32_t flags = kos::ui::GetWindowFlags(s_file_browser_win_id);
    const bool frameless  = (flags & kos::ui::WF_Frameless) != 0u;
    const uint32_t cx = d.x + 1u;
    const uint32_t cy = d.y + (frameless ? 1u : (kos::ui::TitleBarHeight() + 1u));
    const uint32_t cw = (d.w > 2u) ? d.w - 2u : d.w;
    const uint32_t ch = frameless
        ? ((d.h > 2u) ? d.h - 2u : d.h)
        : ((d.h > kos::ui::TitleBarHeight() + 2u) ? (d.h - kos::ui::TitleBarHeight() - 2u) : 0u);
    if (cw < 120u || ch < 80u) return;

    // Background checker - use optimized FillCheckerRect instead of pixel-by-pixel loop
    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

    // Outer border
    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy+ch-1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx+cw-1, cy, 1, ch, kBorderLo);

    // --- Header bar ---
    const uint32_t hdrX = cx + 1u;
    const uint32_t hdrY = cy + 2u;
    const uint32_t hdrW = (cw > 2u) ? cw - 2u : cw;
    kos::gfx::Compositor::FillRect(hdrX, hdrY, hdrW, kFbHeaderH, kHdrBg);
    kos::gfx::Compositor::FillRect(hdrX, hdrY + kFbHeaderH, hdrW, 1, kBorderLo);

    // Up button
    const uint32_t upBtnW = 24u, upBtnH = kFbHeaderH - 2u;
    const uint32_t upBtnX = hdrX + 2u, upBtnY = hdrY + 1u;
    s_fb_up_btn = {upBtnX, upBtnY, upBtnW, upBtnH};
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY, upBtnW, upBtnH, kBtn);
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY, upBtnW, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(upBtnX, upBtnY+upBtnH-1, upBtnW, 1, kBorderLo);
    {
        const char* lbl = "^ Up";
        for (uint32_t i = 0; lbl[i]; ++i) {
            char c = lbl[i];
            kos::gfx::Compositor::DrawGlyph8x8(upBtnX + 2u + i*8, upBtnY + 1u,
                kos::gfx::kFont8x8Basic[c-32], kFgText, 0);
        }
    }

    // Path breadcrumb
    const uint32_t pathX = upBtnX + upBtnW + 4u;
    const uint32_t pathY = hdrY + 4u;
    const uint32_t pathMaxChars = (hdrX + hdrW > pathX + 8u) ? ((hdrX + hdrW - pathX) / 8u) : 0u;
    for (uint32_t i = 0; s_fb_path[i] && i < pathMaxChars; ++i) {
        char c = s_fb_path[i]; if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c-32];
        kos::gfx::Compositor::DrawGlyph8x8(pathX + i*8 + 1, pathY + 1, glyph, kShadow, 0);
        kos::gfx::Compositor::DrawGlyph8x8(pathX + i*8,     pathY,     glyph, kFgPath, 0);
    }

    // --- Body ---
    const uint32_t bodyY = hdrY + kFbHeaderH + 1u;
    const uint32_t bodyH = (cy + ch > bodyY + 2u) ? (cy + ch - bodyY - 2u) : 0u;
    if (bodyH == 0u) return;

    // Sidebar
    const uint32_t sideW = (cw > kFbSidebarW + 60u) ? kFbSidebarW : (cw / 4u);
    const uint32_t sideX = cx + 1u;
    kos::gfx::Compositor::FillRect(sideX, bodyY, sideW, bodyH, kSideBg);
    kos::gfx::Compositor::FillRect(sideX + sideW, bodyY, 1, bodyH, kBorderLo);

    // "PLACES" header in sidebar
    {
        const char* lbl = "PLACES";
        for (uint32_t i = 0; lbl[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(sideX + 4u + i*8, bodyY + 3u,
                kos::gfx::kFont8x8Basic[lbl[i]-32], kBorderHi, 0);
    }
    const uint32_t sideItemY = bodyY + 13u;
    const uint32_t sideMaxChars = (sideW > 8u) ? (sideW - 8u) / 8u : 0u;
    for (uint32_t bi = 0; bi < kFbBookmarkCount; ++bi) {
        const uint32_t bY = sideItemY + bi * (kFbRowH + 2u);
        s_fb_sidebar_btns[bi] = {sideX, bY, sideW, kFbRowH + 2u};
        // Check if active
        const char* bp = kFbBookmarks[bi].path;
        uint32_t bpLen = FbStrLen(bp), fpLen = FbStrLen(s_fb_path);
        bool active = (fpLen == bpLen);
        if (active) {
            for (uint32_t ci = 0; ci < fpLen; ++ci)
                if (s_fb_path[ci] != bp[ci]) { active = false; break; }
        }
        if (active) {
            kos::gfx::Compositor::FillRect(sideX, bY, sideW, kFbRowH+2u, kSideAct);
            kos::gfx::Compositor::FillRect(sideX, bY, 2, kFbRowH+2u, kFgDir);
        }
        const char* lbl = kFbBookmarks[bi].label;
        for (uint32_t i = 0; lbl[i] && i < sideMaxChars; ++i) {
            char c = lbl[i];
            kos::gfx::Compositor::DrawGlyph8x8(sideX + 6u + i*8, bY + 2u,
                kos::gfx::kFont8x8Basic[c-32], active ? kFgText : kFgPath, 0);
        }
    }

    // --- Main panel ---
    const uint32_t mainX = sideX + sideW + 1u;
    const uint32_t mainW = (cx + cw > mainX + 4u) ? (cx + cw - mainX - 2u) : 0u;
    if (mainW < 20u) return;
    FillCheckerRect(mainX, bodyY, mainW, bodyH, kPanel, 0xFF0E1828u, 2);
    kos::gfx::Compositor::FillRect(mainX, bodyY, mainW, 1, kBorderHi);

    // Pagination
    s_fb_pager_visible = false;
    s_fb_prev_btn_r = {0,0,0,0};
    s_fb_next_btn_r = {0,0,0,0};
    uint32_t footerH = 0u;
    uint32_t pageCount = (s_fb_entry_count + kFbPageSize - 1u) / kFbPageSize;
    if (pageCount == 0u) pageCount = 1u;
    if (s_fb_page >= pageCount) s_fb_page = pageCount - 1u;
    if (pageCount > 1u) {
        footerH = 14u;
        constexpr uint32_t btnW = 50u, btnH = 12u;
        const uint32_t by = (bodyY + bodyH > btnH + 2u) ? (bodyY + bodyH - btnH - 2u) : bodyY;
        const uint32_t nextX = (mainX + mainW > btnW + 6u) ? (mainX + mainW - btnW - 4u) : mainX;
        const uint32_t prevX = (nextX > btnW + 6u)         ? (nextX - btnW - 4u)         : mainX;
        s_fb_prev_btn_r = {prevX, by, btnW, btnH};
        s_fb_next_btn_r = {nextX, by, btnW, btnH};
        s_fb_pager_visible = true;
        FillCheckerRect(prevX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        FillCheckerRect(nextX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        kos::gfx::Compositor::FillRect(prevX, by, btnW, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(nextX, by, btnW, 1, kBorderHi);
        const char* pl = "< Prev", *nl = "Next >";
        for (uint32_t i = 0; pl[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(prevX + 3u + i*8, by + 2u,
                kos::gfx::kFont8x8Basic[pl[i]-32], kFgText, 0);
        for (uint32_t i = 0; nl[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(nextX + 3u + i*8, by + 2u,
                kos::gfx::kFont8x8Basic[nl[i]-32], kFgText, 0);
        // Page indicator
        char pgBuf[16]; uint32_t pp = 0;
        BufPutDec(pgBuf, sizeof(pgBuf), pp, s_fb_page + 1u);
        BufPutChar(pgBuf, sizeof(pgBuf), pp, '/');
        BufPutDec(pgBuf, sizeof(pgBuf), pp, pageCount);
        pgBuf[pp] = 0;
        uint32_t pgIndX = (prevX > pp * 8u + 8u) ? (prevX - pp * 8u - 8u) : mainX;
        for (uint32_t i = 0; pgBuf[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(pgIndX + i*8, by + 2u,
                kos::gfx::kFont8x8Basic[pgBuf[i]-32], kFgPath, 0);
    }

    // Entry list
    const uint32_t listY    = bodyY + 2u;
    const uint32_t listH    = (bodyH > footerH + 4u) ? (bodyH - footerH - 4u) : 0u;
    uint32_t maxVisRows = listH / kFbRowH;
    if (maxVisRows > kFbPageSize) maxVisRows = kFbPageSize;
    const uint32_t eStart = s_fb_page * kFbPageSize;
    const uint32_t eEnd   = (eStart + maxVisRows < s_fb_entry_count) ? eStart + maxVisRows : s_fb_entry_count;
    s_fb_visible_count = eEnd - eStart;

    // Name column (leave ~56px on right for size)
    const uint32_t kSizeW    = 56u;
    const uint32_t maxNameCh = (mainW > kSizeW + 30u) ? ((mainW - kSizeW - 30u) / 8u) : 0u;

    for (uint32_t ri = 0; ri < maxVisRows; ++ri) {
        const uint32_t ei = eStart + ri;
        const uint32_t ry = listY + ri * kFbRowH;
        if (ei >= s_fb_entry_count) {
            s_fb_item_rects[ri] = {0,0,0,0};
            continue;
        }
        const bool isSelected = (s_fb_selected == (int32_t)ei);
        const bool isDir      = s_fb_entries[ei].isDir;
        const uint32_t rowBg  = isSelected ? kSelBg : (isDir ? 0xFF141E35u : 0xFF0F1928u);
        kos::gfx::Compositor::FillRect(mainX, ry, mainW, kFbRowH, rowBg);
        kos::gfx::Compositor::FillRect(mainX, ry + kFbRowH - 1, mainW, 1, 0xFF1A2640u);
        s_fb_item_rects[ri] = {mainX, ry, mainW, kFbRowH};

        // Folder/file icon
        const char* iconStr  = isDir ? "[>]" : "[ ]";
        const uint32_t iconC = isDir ? kFgDir : 0xFF4A6A9Au;
        for (uint32_t i = 0; iconStr[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 2u + i*8, ry + 2u,
                kos::gfx::kFont8x8Basic[iconStr[i]-32], iconC, 0);

        // Name
        const int8_t* name   = s_fb_entries[ei].name;
        const uint32_t fgCol = isDir ? kFgDir : kFgFile;
        for (uint32_t i = 0; name[i] && i < maxNameCh; ++i) {
            const char c = (char)name[i]; if (c < 32 || c > 127) continue;
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c-32];
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 28u + i*8 + 1, ry + 2u + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 28u + i*8,     ry + 2u,     glyph, fgCol, 0);
        }

        // File size (right-aligned)
        if (!isDir && s_fb_entries[ei].size > 0) {
            char szBuf[12]; uint32_t sp = 0;
            const uint32_t sz = s_fb_entries[ei].size;
            if (sz >= 1024u * 1024u) {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz / (1024u * 1024u));
                BufPutStr(szBuf, sizeof(szBuf), sp, "MB");
            } else if (sz >= 1024u) {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz / 1024u);
                BufPutChar(szBuf, sizeof(szBuf), sp, 'K');
            } else {
                BufPutDec(szBuf, sizeof(szBuf), sp, sz);
                BufPutChar(szBuf, sizeof(szBuf), sp, 'B');
            }
            szBuf[sp] = 0;
            const uint32_t szTW = sp * 8u;
            const uint32_t szX  = (mainX + mainW > szTW + 4u) ? (mainX + mainW - szTW - 4u) : (mainX + mainW);
            for (uint32_t i = 0; szBuf[i]; ++i)
                kos::gfx::Compositor::DrawGlyph8x8(szX + i*8, ry + 2u,
                    kos::gfx::kFont8x8Basic[szBuf[i]-32], 0xFF6A8AA8u, 0);
        }
    }

    // Empty / no-fs message
    if (!kos::fs::g_fs_ptr || s_fb_entry_count == 0) {
        const char* msg = kos::fs::g_fs_ptr ? "Empty directory" : "No filesystem mounted";
        for (uint32_t i = 0; msg[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 8u + i*8, listY + 4u,
                kos::gfx::kFont8x8Basic[msg[i]-32], 0xFF8AA0C0u, 0);
    }

    // Footer item count
    {
        char infoBuf[32]; uint32_t ip = 0;
        BufPutDec(infoBuf, sizeof(infoBuf), ip, s_fb_entry_count);
        BufPutStr(infoBuf, sizeof(infoBuf), ip, " item(s)");
        infoBuf[ip] = 0;
        const uint32_t infoY = (bodyY + bodyH > 10u) ? (bodyY + bodyH - 10u) : bodyY;
        for (uint32_t i = 0; infoBuf[i]; ++i)
            kos::gfx::Compositor::DrawGlyph8x8(mainX + 4u + i*8, infoY,
                kos::gfx::kFont8x8Basic[infoBuf[i]-32], kBorderHi, 0);
    }
}

static bool HandleFileBrowserClick(int mx, int my) {
    if (!s_file_browser_win_id) return false;
    uint32_t hitWin = 0;
    kos::ui::HitRegion hitRegion = kos::ui::HitRegion::None;
    if (!kos::ui::HitTestDetailed(mx, my, hitWin, hitRegion) || hitWin != s_file_browser_win_id)
        return false;

    auto inRect = [](int px, int py, const kos::gfx::Rect& r) -> bool {
        return (px >= (int)r.x && px < (int)(r.x + r.w) &&
                py >= (int)r.y && py < (int)(r.y + r.h));
    };

    // Up button
    if (inRect(mx, my, s_fb_up_btn)) {
        FbGoUp();
        FbRefreshEntries();
        return true;
    }

    // Sidebar bookmarks
    for (uint32_t bi = 0; bi < kFbBookmarkCount; ++bi) {
        if (inRect(mx, my, s_fb_sidebar_btns[bi])) {
            FbNavigateTo(kFbBookmarks[bi].path);
            FbRefreshEntries();
            return true;
        }
    }

    // Pager buttons
    if (s_fb_pager_visible) {
        uint32_t pc = (s_fb_entry_count + kFbPageSize - 1u) / kFbPageSize;
        if (pc == 0u) pc = 1u;
        if (inRect(mx, my, s_fb_prev_btn_r) && s_fb_page > 0u) {
            --s_fb_page; s_fb_selected = -1; return true;
        }
        if (inRect(mx, my, s_fb_next_btn_r) && s_fb_page + 1u < pc) {
            ++s_fb_page; s_fb_selected = -1; return true;
        }
    }

    // Entry rows: single click = select, second click on same dir = navigate in
    const uint32_t eStart = s_fb_page * kFbPageSize;
    for (uint32_t ri = 0; ri < s_fb_visible_count; ++ri) {
        if (s_fb_item_rects[ri].w == 0) continue;
        if (inRect(mx, my, s_fb_item_rects[ri])) {
            const uint32_t ei = eStart + ri;
            if (s_fb_selected == (int32_t)ei) {
                // Second click: open directory
                if (s_fb_entries[ei].isDir) {
                    FbNavigateInto(s_fb_entries[ei].name);
                    FbRefreshEntries();
                }
                s_fb_selected = -1;
            } else {
                s_fb_selected = (int32_t)ei;
            }
            return true;
        }
    }
    return false;
}

static void RenderHardwareInfoWindowContent() {
    if (!s_hardware_info_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_hardware_info_win_id, d)) return;

    if (s_hw_refresh_ticks == 0 || ++s_hw_refresh_ticks >= 45u) {
        RefreshHardwareSnapshot();
        BuildHardwareInfoLinesForTab(s_hw_active_tab);
        s_hw_refresh_ticks = 1;
    }

    constexpr uint32_t kBgA = 0xFF0B1220u;
    constexpr uint32_t kBgB = 0xFF0E1628u;
    constexpr uint32_t kPanel = 0xFF101D35u;
    constexpr uint32_t kBorderHi = 0xFF6EA1EAu;
    constexpr uint32_t kBorderLo = 0xFF163863u;
    constexpr uint32_t kFg = 0xFFEAF4FFu;
    constexpr uint32_t kLabel = 0xFF9FC0E8u;
    constexpr uint32_t kShadow = 0xFF04070Fu;
    constexpr uint32_t kBtn = 0xFF1B3258u;

    uint32_t cx, cy, cw, ch, tabX, tabY, tabW;
    if (!GetHardwareTabLayout(d, cx, cy, cw, ch, tabX, tabY, tabW)) return;

    // Use optimized FillCheckerRect for background instead of pixel-by-pixel loop
    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderLo);

    for (uint32_t ti = 0; ti < kHwTabs; ++ti) {
        uint32_t tx = tabX + ti * tabW;
        bool active = (ti == static_cast<uint32_t>(s_hw_active_tab));
        uint32_t a = active ? 0xFF2F5CA0u : 0xFF1A2A49u;
        uint32_t b = active ? 0xFF3A6DB8u : 0xFF1D3258u;
        FillCheckerRect(tx, tabY, tabW - 1u, kHwTabStripH, a, b, 2);
        kos::gfx::Compositor::FillRect(tx, tabY, tabW - 1u, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(tx, tabY + kHwTabStripH - 1u, tabW - 1u, 1, kBorderLo);

        const char* name = s_hw_tab_titles[ti];
        uint32_t len = 0; while (name[len]) ++len;
        uint32_t nameX = tx + ((tabW > len * 8u) ? ((tabW - len * 8u) / 2u) : 2u);
        uint32_t nameY = tabY + 5u;
        for (uint32_t c = 0; name[c]; ++c) {
            char chv = name[c]; if (chv < 32 || chv > 127) chv = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[chv - 32];
            kos::gfx::Compositor::DrawGlyph8x8(nameX + c*8 + 1, nameY + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(nameX + c*8, nameY, glyph, active ? 0xFFFFFFFFu : 0xFFC7DCF7u, 0);
        }
    }

    uint32_t panelX = cx + 6u;
    uint32_t panelY = tabY + kHwTabStripH + 6u;
    uint32_t panelW = (cw > 12u) ? (cw - 12u) : 0u;
    uint32_t panelH = (cy + ch > panelY + 6u) ? ((cy + ch) - panelY - 6u) : 0u;
    if (panelW < 10u || panelH < 10u) return;
    FillCheckerRect(panelX, panelY, panelW, panelH, kPanel, 0xFF13233Fu, 2);
    kos::gfx::Compositor::FillRect(panelX, panelY, panelW, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(panelX, panelY + panelH - 1u, panelW, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(panelX, panelY, 1, panelH, kBorderHi);
    kos::gfx::Compositor::FillRect(panelX + panelW - 1u, panelY, 1, panelH, kBorderLo);

    s_hw_pci_btns_visible = false;
    s_hw_pci_prev_btn = {0,0,0,0};
    s_hw_pci_next_btn = {0,0,0,0};
    if (s_hw_active_tab == HardwareTab::PCI) {
        uint32_t btnW = 54u;
        uint32_t btnH = 12u;
        uint32_t by = (panelY + panelH > btnH + 3u) ? (panelY + panelH - btnH - 3u) : panelY;
        uint32_t nextX = (panelX + panelW > btnW + 6u) ? (panelX + panelW - btnW - 6u) : panelX;
        uint32_t prevX = (nextX > btnW + 6u) ? (nextX - btnW - 6u) : panelX;
        s_hw_pci_prev_btn = {prevX, by, btnW, btnH};
        s_hw_pci_next_btn = {nextX, by, btnW, btnH};
        s_hw_pci_btns_visible = true;

        FillCheckerRect(prevX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        FillCheckerRect(nextX, by, btnW, btnH, kBtn, 0xFF25406Du, 2);
        kos::gfx::Compositor::FillRect(prevX, by, btnW, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(prevX, by + btnH - 1u, btnW, 1, kBorderLo);
        kos::gfx::Compositor::FillRect(nextX, by, btnW, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(nextX, by + btnH - 1u, btnW, 1, kBorderLo);

        const char* prev = "< Prev";
        const char* next = "Next >";
        for (uint32_t i = 0; prev[i]; ++i) {
            char c = prev[i]; if (c < 32 || c > 127) c='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(prevX + 4u + i*8, by + 2u, glyph, kFg, 0);
        }
        for (uint32_t i = 0; next[i]; ++i) {
            char c = next[i]; if (c < 32 || c > 127) c='?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(nextX + 4u + i*8, by + 2u, glyph, kFg, 0);
        }

        // Class color legend (left footer)
        auto drawLegendText = [&](uint32_t x, uint32_t y, const char* text, uint32_t color) {
            for (uint32_t i = 0; text[i]; ++i) {
                char c = text[i]; if (c < 32 || c > 127) c='?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
                kos::gfx::Compositor::DrawGlyph8x8(x + i*8 + 1u, y + 1u, glyph, kShadow, 0);
                kos::gfx::Compositor::DrawGlyph8x8(x + i*8, y, glyph, color, 0);
            }
            uint32_t len = 0; while (text[len]) ++len;
            return x + len * 8u;
        };
        uint32_t lx = panelX + 6u;
        uint32_t ly = by + 2u;
        lx = drawLegendText(lx, ly, "S", 0xFFFFD27Au);
        lx = drawLegendText(lx, ly, "=Storage  ", kLabel);
        lx = drawLegendText(lx, ly, "N", 0xFF8BF7B2u);
        lx = drawLegendText(lx, ly, "=Network  ", kLabel);
        lx = drawLegendText(lx, ly, "D", 0xFF8EC8FFu);
        lx = drawLegendText(lx, ly, "=Display  ", kLabel);
        (void)drawLegendText(lx, ly, "U=Serial", 0xFF9EE5E7u);
    }

    uint32_t textX = panelX + 6u;
    uint32_t textY = panelY + 6u;
    uint32_t valueX = panelX + (panelW > 280u ? panelW / 3u : 98u);
    uint32_t rowH = 10u;
    uint32_t footerReserve = (s_hw_active_tab == HardwareTab::PCI) ? 16u : 0u;
    uint32_t maxRows = (panelH > (12u + footerReserve)) ? ((panelH - 12u - footerReserve) / rowH) : 0u;
    uint32_t maxLabelChars = (valueX > textX + 8u) ? ((valueX - textX - 8u) / 8u) : 0u;
    uint32_t maxValueChars = (panelX + panelW > valueX + 8u) ? ((panelX + panelW - valueX - 8u) / 8u) : 0u;

    for (uint32_t row = 0; row < s_hw_line_count && row < maxRows; ++row) {
        const char* text = s_hw_lines[row];
        const char* split = nullptr;
        for (const char* p = text; *p; ++p) {
            if (*p == '\t') { split = p; break; }
        }
        uint32_t y = textY + row * rowH;
        if (!split) {
            uint32_t rowColor = 0xFFFFFFFFu;
            if (s_hw_active_tab == HardwareTab::PCI) {
                if (row == 2u) {
                    rowColor = kLabel;
                } else if (row == 3u) {
                    rowColor = 0xFF6F88A7u;
                } else if (row >= 4u) {
                    uint32_t start = s_hw_pci_page * kPciPageSize;
                    uint32_t idx = start + (row - 4u);
                    if (idx < s_pci_entry_count) {
                        rowColor = PciClassRowColor(s_pci_entries[idx].cls);
                    }
                }
            }
            for (uint32_t i = 0; text[i] && i < maxValueChars; ++i) {
                char c = text[i]; if (c < 32 || c > 127) c = '?';
                const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
                kos::gfx::Compositor::DrawGlyph8x8(textX + i*8 + 1, y + 1, glyph, kShadow, 0);
                kos::gfx::Compositor::DrawGlyph8x8(textX + i*8, y, glyph, rowColor, 0);
            }
            continue;
        }

        uint32_t labelLen = static_cast<uint32_t>(split - text);
        uint32_t valueLen = 0; while (split[1 + valueLen]) ++valueLen;
        if (labelLen > maxLabelChars) labelLen = maxLabelChars;
        if (valueLen > maxValueChars) valueLen = maxValueChars;

        for (uint32_t i = 0; i < labelLen; ++i) {
            char c = text[i]; if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(textX + i*8 + 1, y + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(textX + i*8, y, glyph, kLabel, 0);
        }
        for (uint32_t i = 0; i < valueLen; ++i) {
            char c = split[1 + i]; if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(valueX + i*8 + 1, y + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(valueX + i*8, y, glyph, kFg, 0);
        }
    }
}

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
            
            // Find how many consecutive pixels have the same color
            uint32_t segLen = 1;
            while (i + segLen < w) {
                uint32_t nextTx = (x + i + segLen) / cell;
                uint32_t nextColor = ((nextTx + ty) & 1u) ? cA : cB;
                if (nextColor != color) break;
                ++segLen;
            }
            
            // Draw the segment
            kos::gfx::Compositor::FillRect(x + i, y + j, segLen, 1, color);
            i += segLen;
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
        // Posterize channels to evoke a limited 8-bit palette.
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
// File-scope keyboard handler that forwards keys to LoginScreen
class LoginKeyboardHandler : public kos::drivers::keyboard::KeyboardEventHandler {
public:
    virtual void OnKeyDown(int8_t c) override { 
        kos::ui::LoginScreen::OnKeyDown(c); 
    }
    virtual void OnKeyUp(int8_t) override {}
};
static LoginKeyboardHandler s_login_handler;

// Special taskbar hit ids
static constexpr uint32_t kTaskbarSpawnBtnId = 0xFFFFFFFFu; // sentinel for "+" button
static constexpr uint32_t kTaskbarRebootBtnId = 0xFFFFFFFEu; // sentinel for reboot button

extern "C" void app_reboot() __attribute__((weak));
extern "C" void app_shutdown() __attribute__((weak));

// Taskbar layout constants
static constexpr uint32_t kTaskbarHeight = 22; // bottom panel height
static constexpr uint32_t kTaskbarPad = 4;
static constexpr uint32_t kTaskButtonW = 120; // fixed width per window button
static constexpr uint32_t kTaskButtonH = 18;  // inside bar

static void TaskbarRebootSystem() {
    // Prefer the registered reboot app entrypoint used by the text shell.
    if (app_reboot) {
        app_reboot();
        return;
    }

    // Fallback: keyboard controller reset + triple-fault if needed.
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
    // Leave space for a small "+" spawn button at the far left and reboot button at the far right.
    const uint32_t spawnW = 20;
    const uint32_t rebootW = 28;
    uint32_t x = kTaskbarPad + spawnW + 4;
    uint32_t y = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    for (uint32_t i=0;i<kos::ui::GetWindowCount() && s_taskButtonCount < 16; ++i) {
        uint32_t wid; kos::gfx::WindowDesc d; kos::ui::WindowState st; uint32_t fl;
        if (!kos::ui::GetWindowAt(i, wid, d, st, fl)) continue;
        // Skip non-closable utility windows (Mouse diag, Clock widget, System HUD)
        if (wid == s_mouse_win_id || wid == s_clock_win_id || wid == s_system_hud_win_id) { continue; }
        TaskbarButtonGeom& b = s_taskButtons[s_taskButtonCount++];
        b.x = x; b.y = y; b.w = kTaskButtonW; b.h = kTaskButtonH; b.winId = wid; b.minimized = (st == kos::ui::WindowState::Minimized); b.focused = (wid == kos::ui::GetFocusedWindow());
        x += kTaskButtonW + 4;
        if (x + kTaskButtonW + kTaskbarPad + rebootW + 4 > fb.width) break; // no more space
    }
}

static uint32_t TaskbarHitTest(int mx, int my) {
    if (!s_taskbar_enabled) return 0;
    const auto& fb = kos::gfx::GetInfo();
    if ((uint32_t)my < fb.height - kTaskbarHeight) return 0; // outside bar
    // Check spawn '+' button at far left
    const uint32_t spawnX = kTaskbarPad;
    const uint32_t spawnY = fb.height - kTaskbarHeight + (kTaskbarHeight - kTaskButtonH)/2;
    const uint32_t spawnW = 20;
    const uint32_t spawnH = kTaskButtonH;
    if ((uint32_t)mx >= spawnX && (uint32_t)mx < spawnX + spawnW && (uint32_t)my >= spawnY && (uint32_t)my < spawnY + spawnH) {
        return kTaskbarSpawnBtnId;
    }
    // Reboot button at far right
    const uint32_t rebootW = 28;
    const uint32_t rebootX = fb.width - kTaskbarPad - rebootW;
    const uint32_t rebootY = spawnY;
    if ((uint32_t)mx >= rebootX && (uint32_t)mx < rebootX + rebootW && (uint32_t)my >= rebootY && (uint32_t)my < rebootY + spawnH) {
        return kTaskbarRebootBtnId;
    }
    for (uint32_t i=0;i<s_taskButtonCount;++i) {
        const auto& b = s_taskButtons[i];
        if ((uint32_t)mx >= b.x && (uint32_t)mx < b.x + b.w && (uint32_t)my >= b.y && (uint32_t)my < b.y + b.h) {
            return b.winId;
        }
    }
    return 0;
}

// Draw a single 8x8 glyph at integer scale using FillRect per source pixel.
static void DrawGlyphScaled(uint32_t x, uint32_t y, const uint8_t* glyph,
                            uint32_t fg, uint32_t shadow, uint32_t scale) {
    if (scale == 0) scale = 1;
    for (uint32_t row = 0; row < 8u; ++row) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < 8u; ++col) {
            if (bits & (1u << col)) {
                uint32_t px = x + col * scale;
                uint32_t py = y + row * scale;
                if (shadow) kos::gfx::Compositor::FillRect(px + 1, py + 1, scale, scale, shadow);
                kos::gfx::Compositor::FillRect(px, py, scale, scale, fg);
            }
        }
    }
}

static void RenderClockWindowContent() {
    if (!s_clock_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_clock_win_id, d)) return;

    uint16_t year = 0; uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    kos::sys::get_datetime(&year, &month, &day, &hour, &minute, &second);

    auto w2 = [](char* buf, int& p, uint8_t v) {
        buf[p++] = char('0' + (v / 10u));
        buf[p++] = char('0' + (v % 10u));
    };
    auto w4 = [](char* buf, int& p, uint16_t v) {
        buf[p++] = char('0' + ((v / 1000u) % 10u));
        buf[p++] = char('0' + ((v / 100u)  % 10u));
        buf[p++] = char('0' + ((v / 10u)   % 10u));
        buf[p++] = char('0' + (v % 10u));
    };

    const char sep = ((second & 1u) == 0u) ? ':' : ' ';
    char timeBuf[12]; int tp = 0;
    w2(timeBuf, tp, hour); timeBuf[tp++] = sep;
    w2(timeBuf, tp, minute); timeBuf[tp++] = sep;
    w2(timeBuf, tp, second); timeBuf[tp] = 0;

    char dateBuf[16]; int dp = 0;
    w4(dateBuf, dp, year); dateBuf[dp++] = '-';
    w2(dateBuf, dp, month); dateBuf[dp++] = '-';
    w2(dateBuf, dp, day); dateBuf[dp] = 0;

    constexpr uint32_t kPhosphor       = 0xFF39FF14u;
    constexpr uint32_t kPhosphorDim    = 0xFF1A7A08u;
    constexpr uint32_t kPhosphorShadow = 0xFF041400u;
    constexpr uint32_t kBgA = 0xFF020A02u;
    constexpr uint32_t kBgB = 0xFF030C03u;
    constexpr uint32_t kBorderTop = 0xFF22AA22u;
    constexpr uint32_t kBorderBot = 0xFF001000u;
    constexpr uint32_t kScale = 2u;
    constexpr uint32_t kGlyphW2 = 8u * kScale;

    uint32_t cx = d.x + 1, cy = d.y + 1;
    uint32_t cw = d.w > 2 ? d.w - 2 : d.w;
    uint32_t ch = d.h > 2 ? d.h - 2 : d.h;

    // Use optimized FillCheckerRect for background instead of pixel-by-pixel loop
    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderTop);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderBot);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderTop);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderBot);

    const uint32_t padY = 4u;
    uint32_t timeLen = static_cast<uint32_t>(tp);
    uint32_t timeRowW = timeLen * kGlyphW2;
    uint32_t timeX = (cw > timeRowW) ? cx + (cw - timeRowW) / 2u : cx + 4u;
    uint32_t timeY = cy + padY;
    for (uint32_t i = 0; i < timeLen; ++i) {
        char c = timeBuf[i];
        if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
        DrawGlyphScaled(timeX + i * kGlyphW2, timeY, glyph, kPhosphor, kPhosphorShadow, kScale);
    }

    uint32_t dateLen = static_cast<uint32_t>(dp);
    uint32_t dateRowW = dateLen * 8u;
    uint32_t dateX = (cw > dateRowW) ? cx + (cw - dateRowW) / 2u : cx + 4u;
    uint32_t dateY = timeY + kScale * 8u + 4u;
    for (uint32_t i = 0; i < dateLen; ++i) {
        char c = dateBuf[i];
        if (c < 32 || c > 127) c = '?';
        const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
        kos::gfx::Compositor::DrawGlyph8x8(dateX + i*8 + 1, dateY + 1, glyph, kPhosphorShadow, 0);
        kos::gfx::Compositor::DrawGlyph8x8(dateX + i*8, dateY, glyph, kPhosphorDim, 0);
    }
}

static void RenderSystemHudContent() {
    if (!s_system_hud_win_id) return;
    kos::gfx::WindowDesc d;
    if (!kos::ui::GetWindowDesc(s_system_hud_win_id, d)) return;

    constexpr uint32_t kGreen    = 0xFF39FF14u;
    constexpr uint32_t kYellow   = 0xFFF2E85Cu;
    constexpr uint32_t kRed      = 0xFFFF5C5Cu;
    constexpr uint32_t kFgDim    = 0xFF1A7A08u;
    constexpr uint32_t kShadow   = 0xFF041400u;
    constexpr uint32_t kBgA      = 0xFF020A02u;
    constexpr uint32_t kBgB      = 0xFF030C03u;
    constexpr uint32_t kBorderHi = 0xFF22AA22u;
    constexpr uint32_t kBorderLo = 0xFF001000u;
    constexpr uint32_t kBarBg    = 0xFF0A170Au;

    uint32_t cx = d.x + 1, cy = d.y + 1;
    uint32_t cw = d.w > 2 ? d.w - 2 : d.w;
    uint32_t ch = d.h > 2 ? d.h - 2 : d.h;

    // Use optimized FillCheckerRect for background instead of pixel-by-pixel loop
    FillCheckerRect(cx, cy, cw, ch, kBgA, kBgB, 2);

    kos::gfx::Compositor::FillRect(cx, cy, cw, 1, kBorderHi);
    kos::gfx::Compositor::FillRect(cx, cy + ch - 1, cw, 1, kBorderLo);
    kos::gfx::Compositor::FillRect(cx, cy, 1, ch, kBorderHi);
    kos::gfx::Compositor::FillRect(cx + cw - 1, cy, 1, ch, kBorderLo);

    uint32_t totalRuntime = 0;
    uint32_t idleRuntime = 0;
    if (kos::process::g_scheduler) {
        for (uint32_t tid = 1; tid <= 128; ++tid) {
            kos::process::Thread* task = kos::process::g_scheduler->FindTask(tid);
            if (!task || task->state == kos::process::TASK_TERMINATED) continue;
            totalRuntime += task->total_runtime;
            if (task->priority == kos::process::PRIORITY_IDLE) {
                idleRuntime += task->total_runtime;
            } else if (task->name && task->name[0] == 'i' && task->name[1] == 'd') {
                idleRuntime += task->total_runtime;
            }
        }
    }

    uint32_t dTotal = totalRuntime - s_prev_total_runtime;
    uint32_t dIdle = idleRuntime - s_prev_idle_runtime;
    if (dTotal > 0) {
        uint32_t busy = (dIdle <= dTotal) ? (dTotal - dIdle) : 0u;
        uint32_t pct = (busy * 100u) / dTotal;
        if (pct > 100u) pct = 100u;
        s_last_cpu_pct = static_cast<uint8_t>(pct);
    }
    s_prev_total_runtime = totalRuntime;
    s_prev_idle_runtime = idleRuntime;

    uint32_t totalFrames = 0;
    uint32_t freeFrames = 0;
    uint32_t heapSize = 0;
    uint32_t heapUsed = 0;
    if (kos::sys::table()) {
        if (kos::sys::table()->get_total_frames) totalFrames = kos::sys::table()->get_total_frames();
        if (kos::sys::table()->get_free_frames) freeFrames = kos::sys::table()->get_free_frames();
        if (kos::sys::table()->get_heap_size) heapSize = kos::sys::table()->get_heap_size();
        if (kos::sys::table()->get_heap_used) heapUsed = kos::sys::table()->get_heap_used();
    }
    uint32_t usedFrames = (totalFrames >= freeFrames) ? (totalFrames - freeFrames) : 0u;
    uint32_t memPct = (totalFrames > 0) ? ((usedFrames * 100u) / totalFrames) : 0u;

    auto colorByLoad = [&](uint32_t pct) -> uint32_t {
        if (pct >= 80u) return kRed;
        if (pct >= 50u) return kYellow;
        return kGreen;
    };

    uint8_t batPct = 0;
    bool hasBattery = TryReadBatteryPercent(batPct);
    if (batPct > 100u) batPct = 100u;

    auto drawText = [&](uint32_t x, uint32_t y, const char* text, uint32_t fg) {
        for (uint32_t i = 0; text[i]; ++i) {
            char c = text[i];
            if (c < 32 || c > 127) c = '?';
            const uint8_t* glyph = kos::gfx::kFont8x8Basic[c - 32];
            kos::gfx::Compositor::DrawGlyph8x8(x + i*8 + 1, y + 1, glyph, kShadow, 0);
            kos::gfx::Compositor::DrawGlyph8x8(x + i*8, y, glyph, fg, 0);
        }
    };

    auto drawBar = [&](uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pct, uint32_t fg) {
        if (w < 2u || h < 2u) return;
        if (pct > 100u) pct = 100u;
        kos::gfx::Compositor::FillRect(x, y, w, h, kBarBg);
        kos::gfx::Compositor::FillRect(x, y, w, 1, kBorderHi);
        kos::gfx::Compositor::FillRect(x, y + h - 1, w, 1, kBorderLo);
        kos::gfx::Compositor::FillRect(x, y, 1, h, kBorderHi);
        kos::gfx::Compositor::FillRect(x + w - 1, y, 1, h, kBorderLo);
        uint32_t fillW = (w > 2u) ? ((w - 2u) * pct) / 100u : 0u;
        if (fillW > 0u) {
            kos::gfx::Compositor::FillRect(x + 1u, y + 1u, fillW, h - 2u, fg);
        }
    };

    auto appendDec = [](char* out, int& p, uint32_t v) {
        char rev[12];
        int ri = 0;
        if (v == 0) rev[ri++] = '0';
        while (v && ri < 12) { rev[ri++] = char('0' + (v % 10u)); v /= 10u; }
        while (ri) out[p++] = rev[--ri];
    };

    char line1[40]; int p1 = 0;
    line1[p1++] = 'C'; line1[p1++] = 'P'; line1[p1++] = 'U'; line1[p1++] = ':'; line1[p1++] = ' ';
    appendDec(line1, p1, s_last_cpu_pct); line1[p1++] = '%'; line1[p1] = 0;

    char line2[64]; int p2 = 0;
    line2[p2++] = 'M'; line2[p2++] = 'E'; line2[p2++] = 'M'; line2[p2++] = ':'; line2[p2++] = ' ';
    appendDec(line2, p2, memPct); line2[p2++] = '%';
    line2[p2++] = ' '; line2[p2++] = 'H'; line2[p2++] = ':';
    appendDec(line2, p2, heapUsed / 1024u); line2[p2++] = '/'; appendDec(line2, p2, heapSize / 1024u); line2[p2++] = 'K';
    line2[p2] = 0;

    char line3[24]; int p3 = 0;
    line3[p3++] = 'B'; line3[p3++] = 'A'; line3[p3++] = 'T'; line3[p3++] = ':'; line3[p3++] = ' ';
    if (hasBattery) {
        appendDec(line3, p3, batPct); line3[p3++] = '%';
    } else {
        line3[p3++] = 'N'; line3[p3++] = '/'; line3[p3++] = 'A';
    }
    line3[p3] = 0;

    uint32_t tx = cx + 4u;
    uint32_t cpuColor = colorByLoad(s_last_cpu_pct);
    uint32_t memColor = colorByLoad(memPct);
    uint32_t batColor = hasBattery ? colorByLoad(batPct) : kFgDim;

    drawText(tx, cy + 2u, line1, cpuColor);
    drawBar(tx + 72u, cy + 3u, 96u, 6u, s_last_cpu_pct, cpuColor);

    drawText(tx, cy + 14u, line2, memColor);
    drawBar(tx + 72u, cy + 15u, 96u, 6u, memPct, memColor);

    drawText(tx, cy + 26u, line3, batColor);
    drawBar(tx + 72u, cy + 27u, 96u, 6u, hasBattery ? static_cast<uint32_t>(batPct) : 0u, batColor);
}

static void RenderProcessMonitorWindowContent() {
    kos::ui::ProcessViewer::Render();
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

        if (wid == s_process_monitor_win_id) {
            kos::ui::ProcessViewer::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (s_login_active && wid == s_login_win_id) {
            kos::ui::LoginScreen::Render();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_clock_win_id) {
            RenderClockWindowContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_system_hud_win_id) {
            RenderSystemHudContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_hardware_info_win_id) {
            RenderHardwareInfoWindowContent();
            kos::gfx::Compositor::ClearClipRect();
            continue;
        }

        if (wid == s_file_browser_win_id) {
            RenderFileBrowserContent();
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
            kos::ui::ProcessViewer::Initialize(s_process_monitor_win_id);
            kos::ui::ProcessViewer::RefreshProcessList();
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
        if (s_hardware_info_win_id) {
            s_hw_refresh_ticks = 0;
            s_hw_pci_page = 0;
            RefreshHardwareSnapshot();
            BuildHardwareInfoLinesForTab(s_hw_active_tab);
        }
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
            s_fb_path[0] = '/'; s_fb_path[1] = 0;
            s_fb_needs_refresh = true;
            // Minimize on small desktops to avoid clutter
            if (smallDesktop) {
                kos::ui::MinimizeWindow(s_file_browser_win_id);
            }
        }
    }

    RegisterLegacyComponent(s_process_monitor_win_id, "ProcessMonitor", &RenderProcessMonitorWindowContent);
    RegisterMouseComponent(s_mouse_win_id);
    RegisterClockComponent(s_clock_win_id);
    RegisterSystemHudComponent(s_system_hud_win_id);
    RegisterLegacyComponent(s_hardware_info_win_id, "HardwareInfo", &RenderHardwareInfoWindowContent);
    RegisterLegacyComponent(s_file_browser_win_id, "FileBrowser", &RenderFileBrowserContent);
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
                if (!s_prev_left) {
                    (void)HandleHardwareInfoTabClick(mx, my);
                    (void)HandleHardwareInfoPciPagerClick(mx, my);
                    (void)HandleFileBrowserClick(mx, my);
                }
            }
        } else {
            kos::ui::UpdateInteractions();
        }
    }

    // Dispatch keyboard/mouse events that were captured by real drivers.
    DispatchQueuedInputEvents();

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

    // Refresh process list every ~30 ticks (for smooth updates)
    if (s_process_monitor_win_id) {
        static uint32_t refresh_counter = 0;
        if (++refresh_counter >= 30) {
            kos::ui::ProcessViewer::RefreshProcessList();
            refresh_counter = 0;
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
