#include <input/event_queue.hpp>

namespace kos { namespace input {

InputEventQueue& InputEventQueue::Instance() {
    static InputEventQueue s_instance;
    return s_instance;
}

bool InputEventQueue::Enqueue(const InputEvent& event) {
    // Simple ring buffer: only enqueue if we have space
    if (count_ >= MAX_EVENTS) {
        return false;  // Queue full, event dropped
    }
    
    events_[write_idx_] = event;
    write_idx_ = (write_idx_ + 1) % MAX_EVENTS;
    ++count_;
    
    return true;
}

bool InputEventQueue::Dequeue(InputEvent& out_event) {
    if (count_ == 0) {
        return false;  // Queue empty
    }
    
    out_event = events_[read_idx_];
    read_idx_ = (read_idx_ + 1) % MAX_EVENTS;
    --count_;
    
    return true;
}

uint32_t InputEventQueue::Count() const {
    return count_;
}

void InputEventQueue::Clear() {
    read_idx_ = 0;
    write_idx_ = 0;
    count_ = 0;
}

}}  // namespace kos::input
