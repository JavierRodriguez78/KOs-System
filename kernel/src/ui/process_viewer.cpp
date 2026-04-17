#include <ui/process_viewer.hpp>
#include <ui/framework.hpp>
#include <process/thread_manager.hpp>
#include <graphics/framebuffer.hpp>
#include <lib/string.hpp>
#include <console/tty.hpp>

using namespace kos::ui;
using namespace kos::gfx;
using namespace kos::process;

// Static member definitions
uint32_t ProcessViewer::s_win_id = 0;
bool     ProcessViewer::s_ready = false;
ProcessViewer::ProcessInfo ProcessViewer::s_processes[ProcessViewer::MAX_PROCESSES];
uint32_t ProcessViewer::s_process_count = 0;
uint32_t ProcessViewer::s_selected_index = 0;
uint32_t ProcessViewer::s_scroll_offset = 0;

void ProcessViewer::Initialize(uint32_t windowId) {
    s_win_id = windowId;
    s_ready = (s_win_id != 0);
    s_selected_index = 0;
    s_scroll_offset = 0;
    s_process_count = 0;
    RefreshProcessList();
}

void ProcessViewer::RefreshProcessList() {
    if (!g_thread_manager) return;
    
    s_process_count = 0;
    
    // Get thread entries from thread manager
    // We need to iterate through all registered threads
    ThreadEntry* current = nullptr;
    
    // Access the thread registry (we'll need to add a public accessor method)
    // For now, we can use the global scheduler to get basic thread info
    if (g_scheduler) {
        // Index through known task slots
        for (uint32_t i = 0; i < MAX_PROCESSES && s_process_count < MAX_PROCESSES; i++) {
            Thread* task = g_scheduler->FindTask(i + 1); // task_id starts from 1
            if (task && task->state != TASK_TERMINATED) {
                ProcessInfo& proc = s_processes[s_process_count];
                proc.thread_id = task->task_id;
                proc.pid = g_thread_manager->GetPid(task->task_id);
                
                // Copy thread name
                if (task->name) {
                    int j = 0;
                    while (task->name[j] && j < 31) {
                        proc.name[j] = task->name[j];
                        j++;
                    }
                    proc.name[j] = 0;
                } else {
                    proc.name[0] = 0;
                }
                
                proc.state = (uint8_t)task->state;
                proc.priority = (uint8_t)task->priority;
                proc.runtime_ticks = task->total_runtime;
                s_process_count++;
            }
        }
    }
    
    // Reset selection if needed
    if (s_selected_index >= s_process_count && s_process_count > 0) {
        s_selected_index = s_process_count - 1;
    }
}

void ProcessViewer::drawText(uint32_t x, uint32_t y, const char* text, uint32_t fg, uint32_t bg) {
    if (!text) return;
    for (uint32_t i = 0; text[i]; ++i) {
        char ch = text[i];
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kFont8x8Basic[ch - 32];
        Compositor::DrawGlyph8x8(x + i * 8, y, glyph, fg, bg);
    }
}

void ProcessViewer::drawTextN(uint32_t x, uint32_t y, const char* text, uint32_t len, uint32_t fg, uint32_t bg) {
    if (!text) return;
    for (uint32_t i = 0; i < len && text[i]; ++i) {
        char ch = text[i];
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t* glyph = kFont8x8Basic[ch - 32];
        Compositor::DrawGlyph8x8(x + i * 8, y, glyph, fg, bg);
    }
}

const char* ProcessViewer::getStateString(uint8_t state) {
    switch (state) {
        case 0: return "Ready";
        case 1: return "Running";
        case 2: return "Blocked";
        case 3: return "Sleeping";
        case 4: return "Suspended";
        case 5: return "Terminated";
        default: return "Unknown";
    }
}

const char* ProcessViewer::getPriorityString(uint8_t priority) {
    switch (priority) {
        case 0: return "Critical";
        case 1: return "High";
        case 2: return "Normal";
        case 3: return "Low";
        case 4: return "Idle";
        default: return "Unknown";
    }
}

uint32_t ProcessViewer::getStateColor(uint8_t state) {
    switch (state) {
        case 0: return 0xFF90EE90u; // Green - Ready
        case 1: return 0xFF00FF00u; // Bright green - Running
        case 2: return 0xFFFFFF00u; // Yellow - Blocked
        case 3: return 0xFFFFA500u; // Orange - Sleeping
        case 4: return 0xFFFF6347u; // Red - Suspended
        case 5: return 0xFF808080u; // Gray - Terminated
        default: return 0xFFFFFFFFu;
    }
}

void ProcessViewer::drawHeader() {
    WindowDesc d;
    if (!GetWindowDesc(s_win_id, d)) return;
    
    const uint32_t th = TitleBarHeight();
    const uint32_t padX = 4;
    const uint32_t padY = 4;
    uint32_t y = d.y + th + padY;
    
    // Clear header area
    Compositor::FillRect(d.x, y, d.w, ROW_HEIGHT + 2, HEADER_COLOR);
    
    // Column headers
    uint32_t x = d.x + padX;
    drawText(x, y + 2, "ID", TEXT_COLOR, HEADER_COLOR);
    x += COLUMN_WIDTH * 8;
    drawText(x, y + 2, "Name", TEXT_COLOR, HEADER_COLOR);
    x += COLUMN_WIDTH * 8 * 2;
    drawText(x, y + 2, "State", TEXT_COLOR, HEADER_COLOR);
    x += COLUMN_WIDTH * 8;
    drawText(x, y + 2, "Priority", TEXT_COLOR, HEADER_COLOR);
    x += COLUMN_WIDTH * 8;
    drawText(x, y + 2, "Runtime", TEXT_COLOR, HEADER_COLOR);
}

void ProcessViewer::drawProcessRow(uint32_t row, uint32_t y, const ProcessInfo& proc, bool selected) {
    WindowDesc d;
    if (!GetWindowDesc(s_win_id, d)) return;
    
    const uint32_t padX = 4;
    uint32_t bg = selected ? SELECT_COLOR : BG_COLOR;
    uint32_t fg = selected ? 0xFF000000u : TEXT_COLOR;
    
    // Draw background highlight if selected
    Compositor::FillRect(d.x, y, d.w, ROW_HEIGHT, bg);
    
    uint32_t x = d.x + padX;
    
    // Thread ID
    char id_str[8];
    int id_len = 0;
    {
        uint32_t v = proc.thread_id;
        char rev[8];
        int ri = 0;
        if (v == 0) rev[ri++] = '0';
        else {
            while (v && ri < 8) { rev[ri++] = char('0' + (v % 10)); v /= 10; }
        }
        while (ri > 0) id_str[id_len++] = rev[--ri];
        id_str[id_len] = 0;
    }
    drawText(x, y + 1, id_str, fg, bg);
    x += COLUMN_WIDTH * 8;
    
    // Process Name
    drawTextN(x, y + 1, proc.name, 15, fg, bg);
    x += COLUMN_WIDTH * 8 * 2;
    
    // State (with color)
    const char* state_str = getStateString(proc.state);
    uint32_t state_color = getStateColor(proc.state);
    drawText(x, y + 1, state_str, state_color, bg);
    x += COLUMN_WIDTH * 8;
    
    // Priority
    const char* prio_str = getPriorityString(proc.priority);
    drawText(x, y + 1, prio_str, fg, bg);
    x += COLUMN_WIDTH * 8;
    
    // Runtime (in ticks)
    char runtime_str[12];
    int rt_len = 0;
    {
        uint32_t v = proc.runtime_ticks;
        char rev[12];
        int ri = 0;
        if (v == 0) rev[ri++] = '0';
        else {
            while (v && ri < 12) { rev[ri++] = char('0' + (v % 10)); v /= 10; }
        }
        while (ri > 0) runtime_str[rt_len++] = rev[--ri];
        runtime_str[rt_len] = 0;
    }
    drawText(x, y + 1, runtime_str, fg, bg);
}

void ProcessViewer::drawProcessList() {
    WindowDesc d;
    if (!GetWindowDesc(s_win_id, d)) return;
    
    const uint32_t th = TitleBarHeight();
    const uint32_t padY = 4;
    
    // Calculate visible rows
    uint32_t header_y = d.y + th + padY;
    uint32_t content_start_y = header_y + ROW_HEIGHT + 2;
    uint32_t available_height = (d.y + d.h) - content_start_y;
    uint32_t visible_rows = available_height / ROW_HEIGHT;
    
    // Clear content area
    Compositor::FillRect(d.x, content_start_y, d.w, available_height, BG_COLOR);
    
    // Draw visible process rows
    for (uint32_t i = 0; i < visible_rows && (s_scroll_offset + i) < s_process_count; i++) {
        uint32_t proc_idx = s_scroll_offset + i;
        uint32_t row_y = content_start_y + (i * ROW_HEIGHT);
        bool is_selected = (proc_idx == s_selected_index);
        drawProcessRow(i, row_y, s_processes[proc_idx], is_selected);
    }
    
    // Draw footer with info
    uint32_t footer_y = d.y + d.h - 10;
    char footer[64];
    int footer_len = 0;
    const char* prefix = "Processes: ";
    for (int i = 0; prefix[i] && footer_len < 60; i++) {
        footer[footer_len++] = prefix[i];
    }
    // Add count
    {
        uint32_t v = s_process_count;
        char rev[8];
        int ri = 0;
        if (v == 0) rev[ri++] = '0';
        else {
            while (v && ri < 8) { rev[ri++] = char('0' + (v % 10)); v /= 10; }
        }
        while (ri > 0 && footer_len < 60) footer[footer_len++] = rev[--ri];
    }
    footer[footer_len] = 0;
    drawText(d.x + 4, footer_y, footer, TEXT_COLOR, BG_COLOR);
}

void ProcessViewer::handleSelection() {
    // Refresh the process list to get latest information
    RefreshProcessList();
}

void ProcessViewer::OnKeyDown(int8_t c) {
    if (!s_ready || s_process_count == 0) return;
    
    switch (c) {
        case 'U': // Up arrow
        case 'u':
            if (s_selected_index > 0) {
                s_selected_index--;
                if (s_selected_index < s_scroll_offset) {
                    s_scroll_offset = s_selected_index;
                }
            }
            break;
            
        case 'D': // Down arrow
        case 'd':
            if (s_selected_index < s_process_count - 1) {
                s_selected_index++;
                // Adjust scroll if needed
                WindowDesc d;
                if (GetWindowDesc(s_win_id, d)) {
                    uint32_t th = TitleBarHeight();
                    uint32_t available_height = (d.y + d.h) - (d.y + th + 30);
                    uint32_t visible_rows = available_height / ROW_HEIGHT;
                    if (s_selected_index >= s_scroll_offset + visible_rows) {
                        s_scroll_offset = s_selected_index - visible_rows + 1;
                    }
                }
            }
            break;
            
        case 'R': // Refresh
        case 'r':
            handleSelection();
            break;
            
        case '\n': // Enter - could perform action on selected process
            // TODO: Show action menu or process details
            break;
    }
}

void ProcessViewer::Render() {
    if (!s_ready) return;
    
    WindowDesc d;
    if (!GetWindowDesc(s_win_id, d)) return;
    
    const uint32_t th = TitleBarHeight();
    
    // Clear entire client area
    Compositor::FillRect(d.x, d.y + th, d.w, d.h > th ? d.h - th : 0, BG_COLOR);
    
    // Draw header
    drawHeader();
    
    // Draw process list
    drawProcessList();
}
