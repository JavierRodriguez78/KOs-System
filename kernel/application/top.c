#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>
#include <stddef.h>
#include <console/kcursers_c.h>
#include <lib/libc/stddef.h>
// C ABI for logging to journal
extern void LogToJournal(const char* message);

#define ATTR_DEFAULT 0x07  /* light gray on black */
#define ATTR_HDR     0x0F  /* bright white on black */
#define ATTR_RUNNING 0x0A  /* bright green */
#define ATTR_READY   0x0B  /* bright cyan */
#define ATTR_SLEEP   0x0E  /* bright yellow */
#define ATTR_BLOCK   0x0C  /* bright red */
#define ATTR_IDLE    0x08  /* dark gray */

struct ProcInfo {
    uint32_t pid;
    char state[10];
    uint32_t prio;
    uint32_t time;
    char name[32];
};

static void format_size_kb(uint32_t kb, char* out, int outsz) {
    if (outsz <= 0) return;
    if (kb < 1024u) {
        // e.g., 512KB
        char num[16];
        snprintf(num, sizeof(num), "%u", (unsigned)kb);
        int nlen = (int)strlen(num);
        int idx = 0;
        for (; idx < nlen && idx < outsz-1; ++idx) out[idx] = num[idx];
        if (idx < outsz-1) out[idx++] = 'K';
        if (idx < outsz-1) out[idx++] = 'B';
        out[(idx < outsz) ? idx : outsz-1] = 0;
        return;
    }
    uint32_t mb = kb / 1024u;
    if (mb < 1024u) {
        // e.g., 523MB
        char num[16];
        snprintf(num, sizeof(num), "%u", (unsigned)mb);
        int nlen = (int)strlen(num);
        int idx = 0;
        for (; idx < nlen && idx < outsz-1; ++idx) out[idx] = num[idx];
        if (idx < outsz-1) out[idx++] = 'M';
        if (idx < outsz-1) out[idx++] = 'B';
        out[(idx < outsz) ? idx : outsz-1] = 0;
        return;
    }
    // GB with one decimal: X.YGB (integer math)
    uint32_t gb = mb / 1024u;
    uint32_t rem_mb = mb % 1024u;
    uint32_t frac = (rem_mb * 10u) / 1024u; // one decimal
    char num[32];
    // Build "X.YGB" manually to avoid width specifiers
    char gb_s[16]; snprintf(gb_s, sizeof(gb_s), "%u", (unsigned)gb);
    int idx = 0; int i = 0;
    while (gb_s[i] && idx < outsz-1) { out[idx++] = gb_s[i++]; }
    if (idx < outsz-1) out[idx++] = '.';
    char frac_s = (char)('0' + (frac % 10u));
    if (idx < outsz-1) out[idx++] = frac_s;
    if (idx < outsz-1) out[idx++] = 'G';
    if (idx < outsz-1) out[idx++] = 'B';
    out[(idx < outsz) ? idx : outsz-1] = 0;
}

static void draw_bar_line(uint32_t y, const char* label, int percent) {
    uint32_t rows, cols; kc_getmaxyx(&rows, &cols);
    if (y >= rows) return;
    if (percent < 0) percent = 0; if (percent > 100) percent = 100;
    kc_set_color(7,0);
    kc_move(0, y);
    // Build label manually: "CPU:  42% " without relying on %d/width
    char lbuf[64]; int idx = 0;
    // copy label
    const char* pl = label; while (*pl && idx < (int)sizeof(lbuf)-1) { lbuf[idx++] = *pl++; }
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = ':';
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = ' ';
    // percent as 3-digit left-padded
    int lp = percent; if (lp < 0) lp = 0; if (lp > 100) lp = 100;
    int hundreds = lp / 100; int tens = (lp / 10) % 10; int ones = lp % 10;
    // space or digit for hundreds
    char hch = (hundreds ? ('0' + hundreds) : ' ');
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = hch;
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = (char)('0' + tens);
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = (char)('0' + ones);
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = '%';
    if (idx < (int)sizeof(lbuf)-1) lbuf[idx++] = ' ';
    lbuf[(idx < (int)sizeof(lbuf)) ? idx : (int)sizeof(lbuf)-1] = 0;
    int lab_len = (int)strlen(lbuf);
    if (lab_len > (int)cols) {
        // Truncate label to fit line
        char t[128]; int c = (int)cols; if (c >= (int)sizeof(t)) c = (int)sizeof(t)-1; memcpy(t, lbuf, (size_t)c); t[c]=0; kc_addstr(t);
        return;
    }
    kc_addstr(lbuf);
    int bar_w = (int)cols - lab_len;
    if (bar_w < 10) bar_w = 10;
    kc_addch('[');
    int inner = bar_w - 2; if (inner < 1) inner = 1;
    int fill = (inner * lp) / 100;
    uint8_t fg = (lp < 60) ? 10 : (lp < 85 ? 14 : 12);
    kc_set_color(fg, 0);
    for (int i = 0; i < fill; ++i) kc_addch('#');
    kc_set_color(7, 0);
    for (int i = fill; i < inner; ++i) kc_addch('-');
    kc_addch(']');
}

int parse_line(const char* line, struct ProcInfo* info) {
    // Expected: "<pid> <state> <prio> <time> <name>"; return 5 only when all parsed
    if (!line || !info) return 0;
    const char* p = line;
    // Skip leading spaces and reject if first non-space isn't a digit
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    if (*p < '0' || *p > '9') return 0;

    // Parse pid
    info->pid = 0;
    int got_pid = 0;
    while (*p >= '0' && *p <= '9') { info->pid = info->pid * 10 + (*p - '0'); ++p; got_pid = 1; }
    if (!got_pid) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse state (non-space token)
    int i = 0; int got_state = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 9) { info->state[i++] = *p++; got_state = 1; }
    info->state[i] = 0;
    if (!got_state) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse prio
    info->prio = 0; int got_prio = 0;
    while (*p >= '0' && *p <= '9') { info->prio = info->prio * 10 + (*p - '0'); ++p; got_prio = 1; }
    if (!got_prio) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse time
    info->time = 0; int got_time = 0;
    while (*p >= '0' && *p <= '9') { info->time = info->time * 10 + (*p - '0'); ++p; got_time = 1; }
    if (!got_time) return 0;
    while (*p == ' ' || *p == '\t') ++p;

    // Parse name (token until whitespace)
    i = 0; int got_name = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 31) { info->name[i++] = *p++; got_name = 1; }
    info->name[i] = 0;
    if (!got_name) return 0;

    return 5;
}

static void print_padded_at(uint32_t x, uint32_t y, const char* s, int width) {
    uint32_t rows, cols; kc_getmaxyx(&rows, &cols);
    if (y >= rows) y = (rows ? rows - 1 : 0);
    if ((int)width > (int)cols - (int)x) width = (int)cols - (int)x;
    if (width < 0) width = 0;
    kc_move(x, y);
    if (!s) s = "";
    int len = 0; while (s[len] && len < width) len++;
    kc_addstr(s);
    for (; len < width; ++len) kc_addch(' ');
}

// Minimal libc may not support width in "%u"; pad manually.
static void print_uint_padded_at(uint32_t x, uint32_t y, unsigned v, int width) {
    char num[32];
    snprintf(num, sizeof(num), "%u", v);
    int nlen = (int)strlen(num);
    if (nlen > width) nlen = width; // clamp
    uint32_t rows, cols; kc_getmaxyx(&rows, &cols);
    if (y >= rows) y = (rows ? rows - 1 : 0);
    kc_move(x, y);
    int pad = width - nlen;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; ++i) kc_addch(' ');
    if (nlen > 0) {
        char tmp[32]; int copy = nlen; if (copy >= (int)sizeof(tmp)) copy = (int)sizeof(tmp) - 1;
        memcpy(tmp, num, (size_t)copy); tmp[copy] = 0; kc_addstr(tmp);
    }
}

int top_main(int argc, char** argv) {
    (void)argc; (void)argv;
    kc_init();
    char buffer[4096];
    uint32_t rows = 25, cols = 80;
    // Keep previous-rendered row text to enable diff-based updates
    static char prev_rows_buf[64][128];
    static uint32_t prev_rows_count = 0;
    // Prefer RTC-based pacing to reduce visible flicker
    uint32_t prev_rows = 0, prev_cols = 0;
    static char diag_line[160]; diag_line[0] = 0;
    static char diag_lines[5][160];
    static uint32_t diag_count = 0;
    static int diag_enabled = 0; // toggled by 'D'
    // For CPU meter based on scheduler time deltas
    static uint32_t prev_sum_time = 0;
    static uint32_t prev_idle_time = 0;
    static int cpu_percent_cached = 0;
    while (1) {
        // Fast-path: if Ctrl+C was pressed between refreshes, exit immediately
        int8_t pending;
        while (kos_key_poll(&pending)) {
            if (pending == 3) { // ETX
                kos_puts("^C\n");
                return 0;
            }
        }
        // Pace using RTC seconds (~1 Hz refresh); avoid frequent clears
        if (kos_sys_table()->get_datetime) {
            uint16_t y; uint8_t mo, d, h, mi, s0, s1;
            kos_sys_table()->get_datetime(&y,&mo,&d,&h,&mi,&s0);
            do {
                int8_t eat; while (kos_key_poll(&eat)) { if (eat == 3) { kos_puts("^C\n"); return 0; } }
                kos_sys_table()->get_datetime(&y,&mo,&d,&h,&mi,&s1);
            } while (s1 == s0);
        }
        kc_getmaxyx(&rows, &cols);
        // Only clear/redraw static chrome when size changes
        if (rows != prev_rows || cols != prev_cols) {
            kc_clear();
        }

        // Header
        // Header: redraw when size changes, otherwise overwrite in place
        kc_set_color(15, 0);
        kc_move(0, 0);
        {
            const char* title = "KOS Top - htop style";
            // Truncate/pad to full width
            int tlen = (int)strlen(title);
            if (tlen > (int)cols) tlen = (int)cols;
            if (tlen > 0) {
                char tbuf[128];
                int copy = tlen;
                if (copy >= (int)sizeof(tbuf)) copy = (int)sizeof(tbuf) - 1;
                memcpy(tbuf, title, (size_t)copy);
                tbuf[copy] = 0;
                kc_addstr(tbuf);
            }
            for (uint32_t i = (uint32_t)tlen; i < cols; ++i) kc_addch(' ');
        }
        // Summary line with memory and tasks
        kc_set_color(7, 0);
        kc_move(0, 1);
        {
            uint32_t total_frames = 0, free_frames = 0;
            uint32_t heap_size = 0, heap_used = 0;
            if (kos_sys_table()->get_total_frames) total_frames = kos_sys_table()->get_total_frames();
            if (kos_sys_table()->get_free_frames) free_frames = kos_sys_table()->get_free_frames();
            if (kos_sys_table()->get_heap_size) heap_size = kos_sys_table()->get_heap_size();
            if (kos_sys_table()->get_heap_used) heap_used = kos_sys_table()->get_heap_used();

            uint32_t mem_total_kb = total_frames * 4; // 4KB per frame
            uint32_t mem_used_kb = (total_frames - free_frames) * 4;
            // Render summary with human-friendly units
            char mem_used_s[32]; char mem_total_s[32];
            char heap_used_s[32]; char heap_total_s[32];
            format_size_kb(mem_used_kb, mem_used_s, sizeof(mem_used_s));
            format_size_kb(mem_total_kb, mem_total_s, sizeof(mem_total_s));
            format_size_kb(heap_used/1024u, heap_used_s, sizeof(heap_used_s));
            format_size_kb(heap_size/1024u, heap_total_s, sizeof(heap_total_s));
            char sum[128];
            // Build: "MEM: used/total  HEAP: used/total"
            int si = 0; const char* pfx1 = "MEM: "; const char* pfx2 = "  HEAP: ";
            for (int i=0; pfx1[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = pfx1[i];
            for (int i=0; mem_used_s[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = mem_used_s[i];
            if (si < (int)sizeof(sum)-1) sum[si++] = '/';
            for (int i=0; mem_total_s[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = mem_total_s[i];
            for (int i=0; pfx2[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = pfx2[i];
            for (int i=0; heap_used_s[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = heap_used_s[i];
            if (si < (int)sizeof(sum)-1) sum[si++] = '/';
            for (int i=0; heap_total_s[i] && si < (int)sizeof(sum)-1; ++i) sum[si++] = heap_total_s[i];
            sum[(si < (int)sizeof(sum)) ? si : (int)sizeof(sum)-1] = 0;
            // Pad summary to screen width to erase artifacts
            // Truncate and pad summary to screen width to erase artifacts
            int slen = (int)strlen(sum);
            if (slen > (int)cols) slen = (int)cols;
            if (slen > 0) {
                char sbuf[128];
                int copy = slen;
                if (copy >= (int)sizeof(sbuf)) copy = (int)sizeof(sbuf) - 1;
                memcpy(sbuf, sum, (size_t)copy);
                sbuf[copy] = 0;
                kc_addstr(sbuf);
            }
            for (uint32_t i = slen; i < cols; ++i) kc_addch(' ');
        }

        // Leave two lines for meter bars under the summary
        // Process table box (leave one line at bottom for footer)
        uint32_t table_x = 0;
        uint32_t table_y = 4;
        uint32_t table_w = cols;
        uint32_t table_h = (rows > 10) ? (rows - 6) : (rows - 4);
        if (table_w < 20) table_w = 20;
        if (table_h < 6) table_h = 6;
        // Only draw box if it fits within screen (avoid bottom overflow)
        uint32_t bottom = table_y + table_h;
        if (bottom > rows) {
            // Shrink to fit, leaving last line for footer
            if (rows > table_y + 2) table_h = rows - table_y - 2;
            bottom = table_y + table_h;
        }
        if (rows != prev_rows || cols != prev_cols) {
            // Draw table box
            kc_set_color(7, 0);
            if (table_h >= 3) kc_draw_box(table_x, table_y, table_w, table_h);
            // htop-like colored column header bar
            uint32_t inner_w = (table_w > 4) ? (table_w - 4) : table_w;
            kc_set_color(0, 11); // cyan background
            kc_move(table_x + 1, table_y + 1);
            // Fill inner width with spaces to create a bar
            for (uint32_t i = 0; i < table_w - 2; ++i) kc_addch(' ');
            // Write labels centered-ish
            kc_set_color(15, 11); // white on cyan
            kc_move(table_x + 2, table_y + 1);
            const char* chead = "PID  STATE   PRIO  TIME   NAME";
            int chlen = (int)strlen(chead);
            if (chlen > (int)inner_w) chlen = (int)inner_w;
            char chbuf[64];
            int ccopy = chlen;
            if (ccopy >= (int)sizeof(chbuf)) ccopy = (int)sizeof(chbuf) - 1;
            memcpy(chbuf, chead, (size_t)ccopy);
            chbuf[ccopy] = 0;
            kc_addstr(chbuf);
            kc_set_color(7, 0);
        }

        // Fetch rows from kernel
        int32_t n = 0;
        if (kos_sys_table()->get_process_info) {
            n = kos_sys_table()->get_process_info(buffer, (int32_t)sizeof(buffer)-1);
            if (n < 0) n = 0;
            if (n >= (int32_t)sizeof(buffer)) n = (int32_t)sizeof(buffer) - 1;
            buffer[n] = 0;
        } else {
            kos_puts("(process info service unavailable)\n");
            n = 0; buffer[0] = 0;
        }

        // If diagnostics are enabled, pre-capture up to 5 raw lines from buffer
        if (diag_enabled) {
            diag_count = 0;
            char* pp = buffer;
            while (*pp && diag_count < 5) {
                char* ln = pp;
                while (*pp && *pp != '\n' && *pp != '\r') ++pp;
                char savedc = *pp; *pp = 0;
                int dcopy = (int)strlen(ln);
                if (dcopy >= (int)sizeof(diag_lines[diag_count])) dcopy = (int)sizeof(diag_lines[diag_count]) - 1;
                memcpy(diag_lines[diag_count], ln, (size_t)dcopy);
                diag_lines[diag_count][dcopy] = 0;
                diag_count++;
                *pp = savedc;
                if (*pp == '\n' || *pp == '\r') ++pp;
                if (*pp == '\n' && savedc == '\r') ++pp;
            }
        }
        // Iterate lines & accumulate CPU times (for meter)
        // Render rows inside box with fixed positions (diff-based updates)
        uint32_t max_rows = (table_h >= 3) ? (table_h - 3) : 0; // header + top/bottom borders
        uint32_t row_index = 0;
        uint32_t tasks = 0;
        uint32_t sum_time = 0;
        uint32_t idle_time = 0;
        char* p = buffer;
        diag_line[0] = 0; // reset diagnostics per refresh (single failing line)
        if (!diag_enabled) diag_count = 0; // only auto-collect when enabled
        while (*p && row_index < max_rows) {
            char* line = p;
            while (*p && *p != '\n' && *p != '\r') ++p;
            char saved = *p; *p = 0;

            struct ProcInfo info;
            if (parse_line(line, &info) == 5) {
                sum_time += (uint64_t)info.time;
                if (strcmp(info.name, "idle-thread") == 0) idle_time = (uint64_t)info.time;
                uint8_t fg = 7;
                if (strcmp(info.state, "RUNNING") == 0) fg = 10;
                else if (strcmp(info.state, "READY") == 0) fg = 11;
                else if (strcmp(info.state, "SLEEPING") == 0) fg = 14;
                else if (strcmp(info.state, "BLOCKED") == 0) fg = 12;
                else if (strcmp(info.state, "IDLE") == 0) fg = 8;

                uint32_t y = table_y + 2 + row_index;
                if (y >= rows) break; // safety: don't draw beyond bottom
                uint32_t x = table_x + 2;
                // Compose a single row string for diff comparison
                char rowtxt[128];
                // Build a comparable row text with manual padding to avoid unsupported printf widths
                {
                    char pid_s[16]; snprintf(pid_s, sizeof(pid_s), "%u", (unsigned)info.pid);
                    char prio_s[16]; snprintf(prio_s, sizeof(prio_s), "%u", (unsigned)info.prio);
                    char time_s[16]; snprintf(time_s, sizeof(time_s), "%u", (unsigned)info.time);
                    // left pad to widths 5,2,6
                    char pid_p[8]; int pid_pad = 5 - (int)strlen(pid_s); if (pid_pad < 0) pid_pad = 0; int pi = 0; for (; pi < pid_pad && pi < 7; ++pi) pid_p[pi] = ' '; pid_p[pi] = 0; strncat(pid_p, pid_s, sizeof(pid_p)-1 - (size_t)pi);
                    char prio_p[4]; int pr_pad = 2 - (int)strlen(prio_s); if (pr_pad < 0) pr_pad = 0; int ppi = 0; for (; ppi < pr_pad && ppi < 3; ++ppi) prio_p[ppi] = ' '; prio_p[ppi] = 0; strncat(prio_p, prio_s, sizeof(prio_p)-1 - (size_t)ppi);
                    char time_p[10]; int t_pad = 6 - (int)strlen(time_s); if (t_pad < 0) t_pad = 0; int ti = 0; for (; ti < t_pad && ti < 9; ++ti) time_p[ti] = ' '; time_p[ti] = 0; strncat(time_p, time_s, sizeof(time_p)-1 - (size_t)ti);
                    snprintf(rowtxt, sizeof(rowtxt), "%s  %-8s  %s   %s  %s", pid_p, info.state, prio_p, time_p, info.name);
                }
                // Only rewrite if content changed
                if (row_index >= prev_rows_count || strcmp(prev_rows_buf[row_index], rowtxt) != 0) {
                    // Update stored copy
                    size_t rl = strlen(rowtxt);
                    if (rl >= sizeof(prev_rows_buf[row_index])) rl = sizeof(prev_rows_buf[row_index]) - 1;
                    memcpy(prev_rows_buf[row_index], rowtxt, rl);
                    prev_rows_buf[row_index][rl] = 0;

                    // Draw with minimal color changes and prevent wrapping
                    kc_set_color(fg, 0);
                    kc_move(x, y);
                    // PID
                    print_uint_padded_at(x, y, (unsigned)info.pid, 5);
                    kc_move(x + 5, y); kc_addstr("  ");
                    // STATE
                    print_padded_at(x + 7, y, info.state, 8);
                    // PRIO (colorized)
                    uint8_t prfg = (info.prio <= 1 ? 10 : (info.prio <= 3 ? 14 : 12));
                    kc_set_color(prfg, 0);
                    print_uint_padded_at(x + 7 + 8 + 2, y, (unsigned)info.prio, 2);
                    // TIME and NAME
                    kc_set_color(fg, 0);
                    // TIME
                    char spaces3[] = "   "; kc_move(x + 7 + 8 + 2, y); kc_addstr(spaces3);
                    print_uint_padded_at(x + 7 + 8 + 2 + 3, y, (unsigned)info.time, 6);
                    kc_move(x + 7 + 8 + 2 + 3 + 6, y); kc_addstr("  ");
                    uint32_t inner_w = (table_w > 4) ? (table_w - 4) : table_w; // width inside box
                    uint32_t name_x = x + 7 + 8 + 2 + 3 + 8 + 2; // after time and two spaces
                    kc_move(name_x, y);
                    // Truncate and pad NAME to avoid wrapping beyond box
                    int remaining = (int)(table_x + 2 + inner_w - name_x);
                    if (remaining < 0) remaining = 0;
                    int namelen = (int)strlen(info.name);
                    if (namelen > remaining) namelen = remaining;
                    if (namelen > 0) {
                        // Write only the visible part of the name
                        char nbuf[128];
                        int copy = namelen;
                        if (copy >= (int)sizeof(nbuf)) copy = (int)sizeof(nbuf) - 1;
                        memcpy(nbuf, info.name, (size_t)copy);
                        nbuf[copy] = 0;
                        kc_addstr(nbuf);
                    }
                    // Pad the rest of the line inside the box
                    for (int i = namelen; i < remaining; ++i) kc_addch(' ');
                    kc_set_color(7, 0);
                }
                row_index++;
                tasks++;
            } else {
                // Fallback: render the raw line to help diagnose format issues
                uint32_t y = table_y + 2 + row_index;
                if (y >= rows) break;
                uint32_t x = table_x + 2;
                uint32_t inner_w = (table_w > 4) ? (table_w - 4) : table_w;
                // Truncate raw line to fit
                int rlen = 0; while (line[rlen] && rlen < (int)inner_w) rlen++;
                // Compare to previous; only redraw if changed
                char rowtxt[128];
                int copy = rlen; if (copy >= (int)sizeof(rowtxt)) copy = (int)sizeof(rowtxt) - 1;
                memcpy(rowtxt, line, (size_t)copy); rowtxt[copy] = 0;
                // Capture first failing raw line to display as on-screen diagnostic
                if (diag_line[0] == 0) {
                    int dcopy = (int)strlen(rowtxt);
                    if (dcopy >= (int)sizeof(diag_line)) dcopy = (int)sizeof(diag_line) - 1;
                    memcpy(diag_line, rowtxt, (size_t)dcopy);
                    diag_line[dcopy] = 0;
                }
                if (diag_count < 5) {
                    int dcopy = (int)strlen(rowtxt);
                    if (dcopy >= (int)sizeof(diag_lines[diag_count])) dcopy = (int)sizeof(diag_lines[diag_count]) - 1;
                    memcpy(diag_lines[diag_count], rowtxt, (size_t)dcopy);
                    diag_lines[diag_count][dcopy] = 0;
                    diag_count++;
                }
                if (row_index >= prev_rows_count || strcmp(prev_rows_buf[row_index], rowtxt) != 0) {
                    size_t rl = strlen(rowtxt);
                    if (rl >= sizeof(prev_rows_buf[row_index])) rl = sizeof(prev_rows_buf[row_index]) - 1;
                    memcpy(prev_rows_buf[row_index], rowtxt, rl);
                    prev_rows_buf[row_index][rl] = 0;

                    kc_set_color(7, 0);
                    kc_move(x, y);
                    if (rlen > 0) {
                        char tmp[128]; int tcopy = rlen; if (tcopy >= (int)sizeof(tmp)) tcopy = (int)sizeof(tmp) - 1;
                        memcpy(tmp, line, (size_t)tcopy); tmp[tcopy] = 0;
                        kc_addstr(tmp);
                    }
                    // Pad remainder
                    for (uint32_t i = (uint32_t)rlen; i < inner_w; ++i) kc_addch(' ');
                }
                row_index++;
                tasks++;
            }

            *p = saved;
            if (*p == '\n' || *p == '\r') ++p;
            if (*p == '\n' && saved == '\r') ++p;

            int8_t ch_line;
            if (kos_key_poll(&ch_line) && ch_line == 3) {
                kos_puts("^C\n");
                return 0;
            }
        }

        // Compute CPU usage based on time deltas (1 sec pacing above)
        if (sum_time != prev_sum_time || idle_time != prev_idle_time) {
            uint32_t d_total = sum_time - prev_sum_time; // wraps naturally
            uint32_t d_idle = idle_time - prev_idle_time; // wraps naturally
            if (d_total > 0) {
                // cpu% = 100 * (1 - idle/total)
                uint32_t used = (d_total > d_idle) ? (d_total - d_idle) : 0;
                cpu_percent_cached = (int)((used * 100u) / d_total);
                if (cpu_percent_cached < 0) cpu_percent_cached = 0;
                if (cpu_percent_cached > 100) cpu_percent_cached = 100;
            } else {
                // No progress -> idle
                cpu_percent_cached = 0;
            }
        }
        prev_sum_time = sum_time;
        prev_idle_time = idle_time;

        // Draw CPU and MEM meter bars under the summary line
        // Memory percent used
        uint32_t total_frames = 0, free_frames = 0;
        if (kos_sys_table()->get_total_frames) total_frames = kos_sys_table()->get_total_frames();
        if (kos_sys_table()->get_free_frames) free_frames = kos_sys_table()->get_free_frames();
        uint32_t mem_total_kb = total_frames * 4; // 4KB per frame
        uint32_t mem_used_kb = (total_frames - free_frames) * 4;
        int mem_percent = (mem_total_kb ? (int)((mem_used_kb * 100u) / mem_total_kb) : 0);

        // If time didn't advance at all, treat CPU as idle to avoid stale bars
        int cpu_to_show = cpu_percent_cached;
        if (sum_time == prev_sum_time && idle_time == prev_idle_time) cpu_to_show = 0;
        draw_bar_line(2, "CPU", cpu_to_show);
        draw_bar_line(3, "MEM", mem_percent);
        // Optional HEAP bar: shows heap usage percent
        uint32_t heap_size = 0, heap_used = 0;
        if (kos_sys_table()->get_heap_size) heap_size = kos_sys_table()->get_heap_size();
        if (kos_sys_table()->get_heap_used) heap_used = kos_sys_table()->get_heap_used();
        int heap_percent = 0;
        if (heap_size > 0) {
            uint32_t heap_total_kb = heap_size / 1024;
            uint32_t heap_used_kb = heap_used / 1024;
            if (heap_total_kb > 0) heap_percent = (int)((heap_used_kb * 100u) / heap_total_kb);
        }
        draw_bar_line(4, "HEAP", heap_percent);

        // Clear remaining rows inside the box to avoid leftover text
        while (row_index < max_rows) {
            uint32_t y = table_y + 2 + row_index;
            if (y >= rows) break;
            kc_set_color(7, 0);
            kc_move(table_x + 2, y);
            uint32_t inner_w = (table_w > 4) ? (table_w - 4) : table_w;
            for (uint32_t i = 0; i < inner_w; ++i) kc_addch(' ');
            // Clear previous buffer entry to reflect blank line
            if (row_index < (uint32_t)64) prev_rows_buf[row_index][0] = 0;
            row_index++;
        }
        // Track how many rows were actually rendered
        prev_rows_count = (tasks < max_rows) ? tasks : max_rows;

        // Footer/status bar at fixed bottom line with tasks count
        kc_set_color(7, 0);
        // Footer/status bar (clamp to bottom line)
        uint32_t fy = (rows ? rows - 1 : 0);
        // Render htop-like footer help keys bar
        kc_set_color(0, 9); // blue background
        kc_move(0, fy);
        // Avoid writing to last column on the last row to prevent scroll
        uint32_t safe_cols = (cols > 0) ? (cols - 1) : 0;
        for (uint32_t i = 0; i < safe_cols; ++i) kc_addch(' ');
        // Write key labels with contrasting colors
        kc_set_color(15, 9); // white on blue
        kc_move(1, fy);
        const char* keys = "F1 Help  F2 Setup  F3 Search  F4 Filter  F5 Tree  F6 SortBy  F7 Nice -  F8 Nice +  F9 Kill  F10 Quit   D Diagnostics";
        int klen = (int)strlen(keys);
        // keep at most cols-2 to avoid touching last column on bottom row
        if (klen > (int)cols - 2) klen = (int)cols - 2;
        if (klen > 0) {
            char kbuf[160];
            int copy = klen;
            if (copy >= (int)sizeof(kbuf)) copy = (int)sizeof(kbuf) - 1;
            memcpy(kbuf, keys, (size_t)copy);
            kbuf[copy] = 0;
            kc_addstr(kbuf);
        }
        kc_set_color(7, 0);
        // Diagnostics area: show up to 5 raw lines when enabled
        if (diag_enabled && rows >= 2) {
            if (diag_count == 0) {
                // Show placeholder to confirm diagnostics are enabled
                uint32_t dy = (fy > 0) ? (fy - 1) : 0;
                kc_set_color(15, 0);
                kc_move(0, dy);
                const char* msg = "RAW: (none)";
                int maxlen = (int)cols - 1;
                int mlen = (int)strlen(msg);
                if (mlen > maxlen) mlen = maxlen;
                if (mlen > 0) {
                    char mbuf[32]; int copy = mlen; if (copy >= (int)sizeof(mbuf)) copy = (int)sizeof(mbuf) - 1;
                    memcpy(mbuf, msg, (size_t)copy); mbuf[copy] = 0; kc_addstr(mbuf);
                }
                for (int j = mlen; j < maxlen; ++j) kc_addch(' ');
                kc_set_color(7, 0);
            }
            if (diag_count > 0) {
            int lines_to_show = (int)diag_count;
            if (lines_to_show > 5) lines_to_show = 5;
            for (int i = lines_to_show - 1; i >= 0; --i) {
                uint32_t dy = fy - (uint32_t)(lines_to_show - i);
                if (dy >= rows) break;
                kc_set_color(15, 0);
                kc_move(0, dy);
                const char* src = diag_lines[i];
                // Prefix and truncate to avoid last column
                const char* prefix = "RAW: ";
                int plen = (int)strlen(prefix);
                int maxlen = (int)cols - 1; // avoid last column
                int dlen = (int)strlen(src);
                int total = plen + dlen;
                if (total > maxlen) dlen = maxlen - plen;
                if (dlen < 0) dlen = 0;
                kc_addstr(prefix);
                if (dlen > 0) {
                    char dbuf[160];
                    int dcopy = dlen; if (dcopy >= (int)sizeof(dbuf)) dcopy = (int)sizeof(dbuf) - 1;
                    memcpy(dbuf, src, (size_t)dcopy); dbuf[dcopy] = 0;
                    kc_addstr(dbuf);
                }
                // Pad remainder safely
                int written = plen + dlen;
                for (int j = written; j < maxlen; ++j) kc_addch(' ');
                kc_set_color(7, 0);
            }
            }
        }
        kc_refresh();
        prev_rows = rows; prev_cols = cols;
        // Quit hotkey
        int8_t ch;
        if (kos_key_poll(&ch)) {
            if (ch == 'q' || ch == 'Q' || ch == 3) { kc_set_color(7,0); kc_move(0, rows-1); kc_addstr("Exiting top    "); kc_refresh(); return 0; }
            if (ch == 'd' || ch == 'D') { diag_enabled = !diag_enabled; }
        }
    }
    return 0;
}
#ifndef APP_EMBED
int main(void) {
    top_main(0, NULL);
    return 0;
}
#endif