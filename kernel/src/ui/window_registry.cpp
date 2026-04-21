#include <ui/window_registry.hpp>
#include <ui/component.hpp>

namespace kos { namespace ui {

WindowRegistry& WindowRegistry::Instance() {
    static WindowRegistry s_instance;
    return s_instance;
}

uint32_t WindowRegistry::RegisterComponent(IUIComponent* component, uint32_t window_id) {
    if (!component || window_id == 0) {
        return 0;  // Invalid
    }
    
    if (num_components_ >= MAX_WINDOWS) {
        return 0;  // Registry full
    }
    
    // Check for duplicate window ID
    for (uint32_t i = 0; i < num_components_; ++i) {
        if (component_ids_[i] == window_id) {
            return 0;  // Already registered
        }
    }
    
    components_[num_components_] = component;
    component_ids_[num_components_] = window_id;
    ++num_components_;
    
    return window_id;
}

bool WindowRegistry::UnregisterComponent(uint32_t window_id) {
    for (uint32_t i = 0; i < num_components_; ++i) {
        if (component_ids_[i] == window_id) {
            // Shift remaining components
            for (uint32_t j = i; j < num_components_ - 1; ++j) {
                components_[j] = components_[j + 1];
                component_ids_[j] = component_ids_[j + 1];
            }
            --num_components_;
            return true;
        }
    }
    return false;
}

IUIComponent* WindowRegistry::GetComponent(uint32_t window_id) const {
    for (uint32_t i = 0; i < num_components_; ++i) {
        if (component_ids_[i] == window_id) {
            return components_[i];
        }
    }
    return nullptr;
}

void WindowRegistry::Clear() {
    for (uint32_t i = 0; i < num_components_; ++i) {
        components_[i] = nullptr;
        component_ids_[i] = 0;
    }
    num_components_ = 0;
}

}}  // namespace kos::ui
