#ifndef KOS_UI_WINDOW_REGISTRY_HPP
#define KOS_UI_WINDOW_REGISTRY_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { namespace ui {

class IUIComponent;

/**
 * Dynamic window registry - replaces static window IDs.
 * Manages window lifecycle and tracks UI components.
 * Single-threaded, no locks required (runs in window manager context only).
 */
class WindowRegistry {
public:
    static constexpr uint32_t MAX_WINDOWS = 64;
    
    /**
     * Register a UI component with the registry.
     * Returns the window ID (always non-zero on success).
     */
    uint32_t RegisterComponent(IUIComponent* component, uint32_t window_id);
    
    /**
     * Unregister a component and remove from registry.
     */
    bool UnregisterComponent(uint32_t window_id);
    
    /**
     * Get component by window ID.
     * Returns nullptr if not found.
     */
    IUIComponent* GetComponent(uint32_t window_id) const;
    
    /**
     * Enumerate all registered components in z-order.
     * Callback should return true to continue, false to stop.
     * Callback signature: bool(uint32_t window_id, IUIComponent* component)
     */
    template<typename Callback>
    void ForEachWindowInZOrder(Callback cb) const {
        // Simple linear order for now (components registered in order)
        // In future: track proper z-order
        for (uint32_t i = 0; i < num_components_; ++i) {
            if (!cb(component_ids_[i], components_[i])) {
                break;
            }
        }
    }
    
    /**
     * Get number of registered components.
     */
    uint32_t ComponentCount() const { return num_components_; }
    
    /**
     * Clear all registered components (usually on shutdown).
     */
    void Clear();
    
    /**
     * Get singleton instance.
     */
    static WindowRegistry& Instance();
    
protected:
    IUIComponent* components_[MAX_WINDOWS] = {};
    uint32_t component_ids_[MAX_WINDOWS] = {};
    uint32_t num_components_ = 0;
    
    WindowRegistry() = default;
    WindowRegistry(const WindowRegistry&) = delete;
    WindowRegistry& operator=(const WindowRegistry&) = delete;
};

}}  // namespace kos::ui

#endif
