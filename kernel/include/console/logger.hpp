#ifndef __KOS__CONSOLE__LOGGER_H
#define __KOS__CONSOLE__LOGGER_H

#include <common/types.hpp>
#include <console/tty.hpp>
#include <hardware/rtc.hpp>
#include <lib/libc.hpp>

namespace kos {
namespace console {

    class Logger {
    public:
        // Column where the '[' of the status should appear (0-based columns)
        // On 80x25 VGA, 70 aligns near the right side while leaving room for " [ OK ]"
        static const int STATUS_COL = 70;
        // Global debug flag
        static bool s_debugEnabled;

        // Enable/disable debug logging
        static inline void SetDebugEnabled(bool en) { s_debugEnabled = en; }
        static inline bool IsDebugEnabled() { return s_debugEnabled; }
        // Prints: [YYYY-MM-DD HH:MM:SS] message\n
        static void Log(const char* msg) {
            kos::hardware::DateTime dt; kos::hardware::RTC::Read(dt);
            printTs(dt);
            TTY::Write((const int8_t*)" ");
            TTY::Write((const int8_t*)msg);
            TTY::PutChar('\n');
        }

        // Debug-level logs (printed only if s_debugEnabled)
        static inline void Debug(const char* msg) {
            if (!s_debugEnabled) return;
            Log(msg);
        }
        static inline void DebugKV(const char* key, const char* value) {
            if (!s_debugEnabled) return;
            LogKV(key, value);
        }
        static inline void DebugRaw(const char* msg) {
            if (!s_debugEnabled) return;
            LogRaw(msg);
        }

        static void LogKV(const char* key, const char* value) {
            kos::hardware::DateTime dt; kos::hardware::RTC::Read(dt);
            printTs(dt);
            TTY::Write((const int8_t*)" ");
            TTY::Write((const int8_t*)key);
            TTY::Write((const int8_t*)": ");
            TTY::Write((const int8_t*)value);
            TTY::PutChar('\n');
        }

        static void LogRaw(const char* msg) {
            TTY::Write((const int8_t*)msg);
        }

        // Linux-like status at end of the line: [ OK ] or [FAIL]
        // Usage: LogStatus("Initializing ...", true/false)
        static void LogStatus(const char* msg, bool ok) {
            kos::hardware::DateTime dt; kos::hardware::RTC::Read(dt);
            printTs(dt);
            TTY::Write((const int8_t*)" ");
            TTY::Write((const int8_t*)msg);
            // Compute current column: timestamp (21) + 1 space + msg length
            int col = 21 + 1 + kos::lib::LibC::strlen((const int8_t*)msg);
            int pad = STATUS_COL - col;
            if (pad < 1) pad = 1;
            for (int i = 0; i < pad; ++i) TTY::PutChar(' ');
            // Save default color (assume light grey on black is 0x07)
            if (ok) {
                // Bright Green (10)
                TTY::Write((const int8_t*)" [");
                TTY::SetColor(10, 0); // bright green on black
                TTY::Write((const int8_t*)" OK ");
                TTY::SetAttr(0x07);
                TTY::Write((const int8_t*)"]\n");
            } else {
                // Bright Red (12)
                TTY::Write((const int8_t*)" [");
                TTY::SetColor(12, 0); // bright red on black
                TTY::Write((const int8_t*)"FAIL");
                TTY::SetAttr(0x07);
                TTY::Write((const int8_t*)"]\n");
            }
        }

    private:
        static void write2(uint8_t v) {
            char b[3];
            b[0] = '0' + (v / 10);
            b[1] = '0' + (v % 10);
            b[2] = 0;
            TTY::Write((const int8_t*)b);
        }
        static void write4(uint16_t v) {
            char b[5];
            b[0] = '0' + ((v / 1000) % 10);
            b[1] = '0' + ((v / 100) % 10);
            b[2] = '0' + ((v / 10) % 10);
            b[3] = '0' + (v % 10);
            b[4] = 0;
            TTY::Write((const int8_t*)b);
        }
        static void printTs(const kos::hardware::DateTime& dt) {
            TTY::Write((const int8_t*)"[");
            write4(dt.year);
            TTY::Write((const int8_t*)"-");
            write2(dt.month);
            TTY::Write((const int8_t*)"-");
            write2(dt.day);
            TTY::Write((const int8_t*)" ");
            write2(dt.hour);
            TTY::Write((const int8_t*)":");
            write2(dt.minute);
            TTY::Write((const int8_t*)":");
            write2(dt.second);
            TTY::Write((const int8_t*)"]");
        }
    };

}
}

#endif
