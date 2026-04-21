#ifndef KOS_UI_SYSTEM_COMPONENTS_HPP
#define KOS_UI_SYSTEM_COMPONENTS_HPP

#include <ui/component.hpp>

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
    // Component state
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
    // Component state
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
    // Component state
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

}}  // namespace kos::ui

#endif
