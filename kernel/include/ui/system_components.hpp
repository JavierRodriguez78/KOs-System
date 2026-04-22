#ifndef KOS_UI_SYSTEM_COMPONENTS_HPP
#define KOS_UI_SYSTEM_COMPONENTS_HPP

#include <ui/component.hpp>
#include <fs/filesystem.hpp>
#include <graphics/compositor.hpp>

namespace kos { namespace ui {

/**
 * Base class for built-in system UI components.
 * Provides common functionality for components like Clock, HUD, FileManager.
 */
class BaseSystemComponent : public IUIComponent {
public:
    explicit BaseSystemComponent(const char* name) : name_(name) {}
    virtual ~BaseSystemComponent() = default;
    
    uint32_t GetWindowId() const override final { return window_id_; }
    const char* GetName() const override final { return name_; }
    
    void InvalidateContent() override final {
        needs_redraw_ = true;
    }
    
    bool NeedsRedraw() const { return needs_redraw_; }
    void ClearRedrawFlag() { needs_redraw_ = false; }
    void BindWindow(uint32_t id) { SetWindowId(id); }
    
protected:
    void SetWindowId(uint32_t id) { window_id_ = id; }
    
private:
    uint32_t window_id_ = 0;
    const char* name_ = "Component";
    bool needs_redraw_ = true;
};

/**
 * Wrapper component for the system Clock widget.
 */
class ClockComponent : public BaseSystemComponent {
public:
    ClockComponent() : BaseSystemComponent("Clock") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    
private:
    // Component state, if needed
};

/**
 * Wrapper component for the System HUD (CPU/Memory stats).
 */
class SystemHudComponent : public BaseSystemComponent {
public:
    SystemHudComponent() : BaseSystemComponent("SystemHUD") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    
private:
    uint32_t prev_total_runtime_ = 0;
    uint32_t prev_idle_runtime_ = 0;
    uint8_t last_cpu_pct_ = 0;
};

/**
 * Wrapper component for Hardware Info (tabbed dashboard).
 */
class HardwareInfoComponent : public BaseSystemComponent {
public:
    HardwareInfoComponent() : BaseSystemComponent("HardwareInfo") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    void OnWindowResized(uint32_t width, uint32_t height) override;
    
private:
    struct PciDeviceSummary {
        bool valid;
        uint8_t bus;
        uint8_t dev;
        uint8_t fn;
        uint16_t vendor;
        uint16_t device;
        uint8_t subclass;
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

    static constexpr uint32_t kPciPageSize = 9u;
    static constexpr uint32_t kHwTabStripH = 18u;
    static constexpr uint32_t kHwTabs = static_cast<uint32_t>(HardwareTab::Count);

    HardwareSnapshot snapshot_{};
    HardwareTab active_tab_ = HardwareTab::Overview;
    char lines_[24][112]{};
    uint32_t line_count_ = 0;
    PciListEntry pci_entries_[96]{};
    uint32_t pci_entry_count_ = 0;
    uint32_t pci_page_ = 0;
    kos::gfx::Rect pci_prev_btn_ = {0, 0, 0, 0};
    kos::gfx::Rect pci_next_btn_ = {0, 0, 0, 0};
    bool pci_btns_visible_ = false;
    uint32_t refresh_ticks_ = 0;

    static const char* kTabTitles[kHwTabs];
    uint32_t PciCfgRead(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) const;
    void AppendLine(const char* line);
    void AppendKV(const char* key, const char* value);
    void FormatPciLocation(char* out, uint32_t outSize, uint8_t bus, uint8_t dev, uint8_t fn) const;
    void FormatPciId(char* out, uint32_t outSize, uint16_t vendor, uint16_t device) const;
    void FormatPciSummary(char* out, uint32_t outSize, const char* vendorName,
                          uint8_t bus, uint8_t dev, uint8_t fn,
                          uint16_t vendor, uint16_t device) const;
    void FormatPciTableRow(char* out, uint32_t outSize, const PciListEntry& e) const;
    const char* PciClassName(uint8_t cls, uint8_t subclass) const;
    const char* PciVendorName(uint16_t vendor, uint16_t device) const;
    uint32_t PciClassRowColor(uint8_t cls) const;
    void ScanPciClass(uint8_t wantedClass, PciDeviceSummary& first, uint32_t& count);
    void ScanPciAllDevices();
    void CpuidRegs(uint32_t eaxIn, uint32_t ecxIn, uint32_t regs[4]) const;
    const char* InputSourceName(uint8_t src) const;
    void RefreshSnapshot();
    void BuildLinesForTab(HardwareTab tab);
    bool GetTabLayout(const kos::gfx::WindowDesc& d,
                      uint32_t& cx, uint32_t& cy, uint32_t& cw, uint32_t& ch,
                      uint32_t& tabX, uint32_t& tabY, uint32_t& tabW) const;
    bool HandleTabClick(int mx, int my);
    bool HandlePciPagerClick(int mx, int my);
};

/**
 * Wrapper component for File Browser.
 */
class FileBrowserComponent : public BaseSystemComponent {
public:
    FileBrowserComponent() : BaseSystemComponent("FileBrowser") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    void OnWindowResized(uint32_t width, uint32_t height) override;
    
private:
    static constexpr uint32_t kPageSize = 16u;
    static constexpr uint32_t kSidebarWidth = 80u;
    static constexpr uint32_t kHeaderHeight = 16u;
    static constexpr uint32_t kRowHeight = 12u;
    static constexpr uint32_t kBookmarkCount = 5u;

    struct Bookmark {
        const char* label;
        const char* path;
    };

    char current_path_[128] = {'/', '\0'};
    kos::fs::DirEntry entries_[64]{};
    uint32_t entry_count_ = 0;
    uint32_t page_ = 0;
    int32_t selected_ = -1;
    bool needs_refresh_ = true;

    kos::gfx::Rect up_btn_ = {0, 0, 0, 0};
    kos::gfx::Rect sidebar_btns_[kBookmarkCount]{};
    kos::gfx::Rect item_rects_[kPageSize]{};
    kos::gfx::Rect prev_btn_ = {0, 0, 0, 0};
    kos::gfx::Rect next_btn_ = {0, 0, 0, 0};
    uint32_t visible_count_ = 0;
    bool pager_visible_ = false;

    static const Bookmark kBookmarks[kBookmarkCount];
    static bool EnumDirCallback(const kos::fs::DirEntry* entry, void* userdata);
    uint32_t StrLen(const char* s) const;
    void StrCopy(char* dst, uint32_t dstSize, const char* src) const;
    void GoUp();
    void NavigateTo(const char* path);
    void NavigateInto(const int8_t* entryName);
    void RefreshEntries();
    bool HandleMouseClick(int mx, int my);
};

/**
 * Wrapper component for Process Monitor/Viewer.
 */
class ProcessMonitorComponent : public BaseSystemComponent {
public:
    ProcessMonitorComponent() : BaseSystemComponent("ProcessMonitor") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    void OnWindowResized(uint32_t width, uint32_t height) override;
    
private:
    bool initialized_ = false;
    uint32_t refresh_counter_ = 0;
};

/**
 * Wrapper component for Mouse diagnostic window.
 */
class MouseDiagnosticComponent : public BaseSystemComponent {
public:
    MouseDiagnosticComponent() : BaseSystemComponent("MouseDiagnostic") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    
private:
    uint32_t last_seen_packets_ = 0;
    uint8_t event_flash_frames_ = 0;
};

/**
 * Wrapper component for Application Menu.
 * Displays discovered desktop applications in a graphical menu.
 */
class AppMenuComponent : public BaseSystemComponent {
public:
    struct DesktopAppEntry {
        char name[48];
        char exec[96];
        bool autostart;
        uint32_t priority;
    };
    
    AppMenuComponent() : BaseSystemComponent("AppMenu") {}
    
    void Render() override;
    bool OnInputEvent(const input::InputEvent& event) override;
    
    // Set the app list from window manager
    void SetAppList(const DesktopAppEntry* apps, uint32_t count) {
        if (!apps || count > kMaxApps) return;
        for (uint32_t i = 0; i < count; ++i) {
            apps_[i] = apps[i];
        }
        app_count_ = count;
        highlighted_index_ = 0;
        selection_pending_ = false;
    }
    
    // Get selected app for launching
    const DesktopAppEntry* GetSelectedApp() const {
        if (highlighted_index_ < app_count_) {
            return &apps_[highlighted_index_];
        }
        return nullptr;
    }
    
    // Check if app was selected for launching (double-click or Enter)
    bool IsSelectionPending() const {
        return selection_pending_;
    }
    
    // Clear the selection pending flag
    void ClearSelectionPending() {
        selection_pending_ = false;
    }
    
private:
    static constexpr uint32_t kMaxApps = 16u;
    static constexpr uint32_t kItemHeight = 24u;
    static constexpr uint32_t kPadding = 8u;
    static constexpr uint32_t kItemNameMaxLen = 32u;
    
    DesktopAppEntry apps_[kMaxApps]{};
    uint32_t app_count_ = 0;
    uint32_t highlighted_index_ = 0;
    bool selection_pending_ = false;
    uint32_t last_click_time_ = 0;
    
    uint32_t GetItemY(uint32_t index) const {
        return kPadding + index * kItemHeight;
    }
};

}}  // namespace kos::ui

#endif
