#ifndef KOS_UI_COMPONENT_HPP
#define KOS_UI_COMPONENT_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { namespace input {
struct InputEvent;
}}

namespace kos { namespace ui {

/**
 * Abstract interface for UI components that can be rendered and interact with events.
 * Allows modular, pluggable UI elements without hardcoding in window manager.
 */
class IUIComponent {
public:
    virtual ~IUIComponent() = default;
    
    /**
     * Get the window ID associated with this component.
     * Returns 0 if no window, non-zero if window ID.
     */
    virtual uint32_t GetWindowId() const = 0;
    
    /**
     * Called when component should render its content to the screen.
     * Assumes clip rect is already set by window manager.
     * Called every frame if needs_redraw is true.
     */
    virtual void Render() = 0;
    
    /**
     * Called when component receives an input event.
     * Component should handle or ignore as appropriate.
     * Return true if event was handled, false to propagate.
     */
    virtual bool OnInputEvent(const kos::input::InputEvent& event) = 0;
    
    /**
     * Called when window is focused/unfocused.
     */
    virtual void OnFocusChanged(bool focused) { }

    /**
     * Mark component as needing redraw. Called automatically on state changes.
     */
    virtual void InvalidateContent() { }

    /**
     * Called when window is shown/hidden (minimized).
     */
    virtual void OnVisibilityChanged(bool visible) { }
    
    /**
     * Called when window is resized.
     */
    virtual void OnWindowResized(uint32_t width, uint32_t height) { }

    /**
     * Get component name for debugging.
     */
    virtual const char* GetName() const = 0;
};

}}  // namespace kos::ui

#endif
