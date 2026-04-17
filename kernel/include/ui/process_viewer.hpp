#ifndef KOS_UI_PROCESS_VIEWER_HPP
#define KOS_UI_PROCESS_VIEWER_HPP

#include <common/types.hpp>
#include <graphics/compositor.hpp>
#include <graphics/font8x8_basic.hpp>

namespace kos { namespace ui {

class ProcessViewer {
public:
    // Initialize with a window id to render into
    static void Initialize(uint32_t windowId);
    
    // Render the process list UI into the window
    static void Render();
    
    // Process keyboard input (ASCII, special: '\n' enter, '\b' backspace, arrow keys: 'U' up, 'D' down)
    static void OnKeyDown(int8_t c);
    
    // Refresh process list from system
    static void RefreshProcessList();
    
private:
    // Window and display state
    static uint32_t s_win_id;
    static bool s_ready;
    
    // Process list management
    static const uint32_t MAX_PROCESSES = 16;
    
    struct ProcessInfo {
        uint32_t thread_id;
        uint32_t pid;
        char name[32];
        uint8_t state;              // TaskState
        uint8_t priority;           // ThreadPriority
        uint32_t runtime_ticks;
    };
    
    static ProcessInfo s_processes[MAX_PROCESSES];
    static uint32_t s_process_count;
    static uint32_t s_selected_index;
    static uint32_t s_scroll_offset;
    
    // UI Constants
    static const uint32_t COLUMN_WIDTH = 15;
    static const uint32_t ROW_HEIGHT = 10;
    static const uint32_t TEXT_COLOR = 0xFFFFFFFFu;      // White
    static const uint32_t BG_COLOR = 0xFF1F1F2Eu;        // Dark background
    static const uint32_t HEADER_COLOR = 0xFF4B5563u;    // Gray header
    static const uint32_t SELECT_COLOR = 0xFF3B82F6u;    // Blue selection
    static const uint32_t PADDING = 8;
    
    // Helper functions
    static void drawText(uint32_t x, uint32_t y, const char* text, uint32_t fg, uint32_t bg);
    static void drawTextN(uint32_t x, uint32_t y, const char* text, uint32_t len, uint32_t fg, uint32_t bg);
    static void drawProcessRow(uint32_t row, uint32_t y, const ProcessInfo& proc, bool selected);
    static void drawHeader();
    static void drawProcessList();
    static const char* getStateString(uint8_t state);
    static const char* getPriorityString(uint8_t priority);
    static uint32_t getStateColor(uint8_t state);
    static void handleSelection();
};

}}

#endif // KOS_UI_PROCESS_VIEWER_HPP
