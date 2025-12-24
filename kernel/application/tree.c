#include <lib/libc/stdio.h>
#include <lib/libc/stdint.h>
#include <lib/libc/string.h>
#include "app.h"

// Maximum recursion depth
#define MAX_DEPTH 16
// Maximum path length
#define MAX_PATH 256

// Counters for summary
static int32_t g_dir_count = 0;
static int32_t g_file_count = 0;

// Tree drawing characters
static const char* BRANCH_MIDDLE = "|-- ";
static const char* BRANCH_LAST   = "`-- ";
static const char* INDENT_PIPE   = "|   ";
static const char* INDENT_SPACE  = "    ";

// Build the prefix string based on the "last entry" flags at each level
static void build_prefix(char* prefix, const uint8_t* is_last, int depth) {
    int pi = 0;
    for (int i = 0; i < depth - 1 && pi < MAX_PATH - 5; ++i) {
        const char* seg = is_last[i] ? INDENT_SPACE : INDENT_PIPE;
        for (int j = 0; seg[j]; ++j) prefix[pi++] = seg[j];
    }
    prefix[pi] = 0;
}

// Forward declaration of the recursive tree function
static void tree_dir(const int8_t* path, int depth, uint8_t* is_last);

// Callback data structure for collecting entries
typedef struct {
    int8_t names[64][13];  // Max 64 entries per directory
    uint8_t is_dir[64];
    int count;
} DirCollector;

// Callback to collect directory entries
static int32_t collect_entry(const void* entry_ptr, void* userdata) {
    const kos_direntry_t* entry = (const kos_direntry_t*)entry_ptr;
    DirCollector* collector = (DirCollector*)userdata;
    
    if (collector->count >= 64) return 0; // Stop if full
    
    // Copy name
    int i = 0;
    for (; entry->name[i] && i < 12; ++i) {
        collector->names[collector->count][i] = entry->name[i];
    }
    collector->names[collector->count][i] = 0;
    collector->is_dir[collector->count] = entry->isDir ? 1 : 0;
    collector->count++;
    
    return 1; // Continue
}

// Recursive tree printing function
static void tree_dir(const int8_t* path, int depth, uint8_t* is_last) {
    if (depth >= MAX_DEPTH) return;
    
    DirCollector collector;
    collector.count = 0;
    
    // Use kernel's EnumDir via syscall
    int32_t count = kos_enumdir(path, collect_entry, &collector);
    if (count < 0) return;
    
    // Print each entry
    for (int i = 0; i < collector.count; ++i) {
        char prefix[MAX_PATH];
        build_prefix(prefix, is_last, depth);
        
        int is_last_entry = (i == collector.count - 1);
        const char* branch = is_last_entry ? BRANCH_LAST : BRANCH_MIDDLE;
        
        // Print the line
        kos_puts((const int8_t*)prefix);
        kos_puts((const int8_t*)branch);
        
        if (collector.is_dir[i]) {
            // Directory - print in color if possible
            kos_set_color(10, 0); // Green for directories
            kos_puts(collector.names[i]);
            kos_set_attr(0x07);
            kos_puts((const int8_t*)"/\n");
            g_dir_count++;
            
            // Build child path
            int8_t child_path[MAX_PATH];
            int pi = 0;
            for (int j = 0; path[j] && pi < MAX_PATH - 2; ++j) child_path[pi++] = path[j];
            if (pi > 0 && child_path[pi - 1] != '/') child_path[pi++] = '/';
            for (int j = 0; collector.names[i][j] && pi < MAX_PATH - 1; ++j) child_path[pi++] = collector.names[i][j];
            child_path[pi] = 0;
            
            // Recurse
            is_last[depth] = is_last_entry;
            tree_dir(child_path, depth + 1, is_last);
        } else {
            // Regular file
            kos_puts(collector.names[i]);
            kos_puts((const int8_t*)"\n");
            g_file_count++;
        }
    }
}

void app_tree(void) {
    // Get optional path argument
    const int8_t* path = 0;
    int32_t argc = kos_argc();
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        const int8_t* a = kos_argv(i);
        if (!a) continue;
        if (strcmp(a, (const int8_t*)"-h") == 0 || strcmp(a, (const int8_t*)"--help") == 0) {
            kos_puts((const int8_t*)"Usage: tree [path]\n");
            kos_puts((const int8_t*)"Display directory tree structure.\n");
            return;
        }
        path = a;
        break;
    }
    
    // Default to current directory or root
    const int8_t* display = path;
    if (!display || !display[0]) {
        display = kos_cwd();
        if (!display || !display[0]) display = (const int8_t*)"/";
    }
    
    // Reset counters
    g_dir_count = 0;
    g_file_count = 0;
    
    // Print the root of the tree
    kos_puts(display);
    kos_puts((const int8_t*)"\n");
    
    // Track "is last entry" for each depth level
    uint8_t is_last[MAX_DEPTH];
    for (int i = 0; i < MAX_DEPTH; ++i) is_last[i] = 0;
    
    // Start recursion
    tree_dir(display, 1, is_last);
    
    // Print summary
    kos_puts((const int8_t*)"\n");
    kos_printf((const int8_t*)"%d directories, %d files\n", g_dir_count, g_file_count);
}

#ifndef APP_EMBED
int main(void) {
    app_tree();
    return 0;
}
#endif
