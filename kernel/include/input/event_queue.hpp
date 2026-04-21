#ifndef KOS_INPUT_EVENT_QUEUE_HPP
#define KOS_INPUT_EVENT_QUEUE_HPP

#include <common/types.hpp>

using namespace kos::common;

namespace kos { namespace input {

enum class EventType : uint8_t {
    KeyPress = 0,
    KeyRelease = 1,
    MouseMove = 2,
    MousePress = 3,
    MouseRelease = 4,
    MouseEnter = 5,
    MouseLeave = 6,
    MouseScroll = 7,
};

struct InputEvent {
    EventType type;
    uint32_t timestamp_ms;
    uint32_t target_window;  // 0 = global focus, non-zero = specific window
    
    // Event data (union for different event types)
    union {
        struct {
            uint32_t key_code;
            uint32_t modifiers;  // Shift, Ctrl, Alt flags
        } key_data;
        
        struct {
            int32_t x;
            int32_t y;
            uint8_t buttons;  // bit 0=left, bit 1=right, bit 2=middle
        } mouse_data;
        
        struct {
            int32_t dx;
            int32_t dy;
        } scroll_data;
    };
};

/**
 * Unified input event queue for keyboard and mouse events.
 * Replaces handler overrides and allows decoupled input handling.
 * Thread-safe for single producer (drivers) + single consumer (window manager).
 */
class InputEventQueue {
public:
    static constexpr uint32_t MAX_EVENTS = 256;
    
    /**
     * Enqueue an input event. Safe to call from interrupt handlers.
     * Returns true if event was queued, false if queue full.
     */
    bool Enqueue(const InputEvent& event);
    
    /**
     * Dequeue the next event. Returns true if event available, false if queue empty.
     */
    bool Dequeue(InputEvent& out_event);
    
    /**
     * Get number of pending events.
     */
    uint32_t Count() const;
    
    /**
     * Clear all queued events.
     */
    void Clear();
    
    /**
     * Get singleton instance.
     */
    static InputEventQueue& Instance();
    
protected:
    InputEvent events_[MAX_EVENTS];
    uint32_t write_idx_ = 0;
    uint32_t read_idx_ = 0;
    uint32_t count_ = 0;
    
    InputEventQueue() = default;
    InputEventQueue(const InputEventQueue&) = delete;
    InputEventQueue& operator=(const InputEventQueue&) = delete;
};

}}  // namespace kos::input

#endif
