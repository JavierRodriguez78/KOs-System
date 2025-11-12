#ifndef __KOS__CONSOLE__LOGGER_H
#define __KOS__CONSOLE__LOGGER_H

#include <common/types.hpp>
#include <console/tty.hpp>
#include <arch/x86/hardware/rtc/rtc.hpp>
#include <lib/libc/string.h>

using namespace kos::common;
using namespace kos::console;
using namespace kos::arch::x86::hardware::rtc;


namespace kos {
    
    namespace console {

        class Logger {
    
            public:
                // Column where the '[' of the status should appear (0-based columns)
                // On 80x25 VGA, 70 aligns near the right side while leaving room for " [ OK ]"
                static const int STATUS_COL = 70;
                // Global debug flag
                static bool s_debugEnabled;
                // When true, suppress output to TTY (used to hide boot logs from GUI terminal)
                static bool s_mutedTTY;

                // Enable/disable debug logging
                static inline void SetDebugEnabled(bool en) { s_debugEnabled = en; }
                static inline bool IsDebugEnabled() { return s_debugEnabled; }
                // Prints: [YYYY-MM-DD HH:MM:SS] message\n
                static void Log(const char* msg) {
                    if (s_mutedTTY) { // Still forward to journal, but skip TTY
                        extern void LogToJournal(const char* message);
                        LogToJournal(msg);
                        return;
                    }
                    kos::arch::x86::hardware::rtc::DateTime dt; kos::arch::x86::hardware::rtc::RTC::Read(dt);
                    printTs(dt);
                    TTY::Write((const int8_t*)" ");
                    TTY::Write((const int8_t*)msg);
                    TTY::PutChar('\n');
                    // Forward log to JournalService for journald-like persistence
                    extern void LogToJournal(const char* message);
                    LogToJournal(msg);
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
                    if (s_mutedTTY) {
                        extern void LogToJournalKV(const char* key, const char* value);
                        LogToJournalKV(key, value);
                        return;
                    }
                    DateTime dt; 
                    RTC::Read(dt);
                    printTs(dt);
                    TTY::Write((const int8_t*)" ");
                    TTY::Write((const int8_t*)key);
                    TTY::Write((const int8_t*)": ");
                    TTY::Write((const int8_t*)value);
                    TTY::PutChar('\n');
                    // Forward log to JournalService
                    extern void LogToJournalKV(const char* key, const char* value);
                    LogToJournalKV(key, value);
                }

                static void LogRaw(const char* msg) {
                    if (s_mutedTTY) return;
                    TTY::Write((const int8_t*)msg);
                }

                // Linux-like status at end of the line: [ OK ] or [FAIL]
                // Usage: LogStatus("Initializing ...", true/false)
                static void LogStatus(const char* msg, bool ok) {
                        if (s_mutedTTY) {
                            extern void LogToJournalStatus(const char* msg, bool ok);
                            LogToJournalStatus(msg, ok);
                            return;
                        }
                        DateTime dt;
                        RTC::Read(dt);
                        printTs(dt);
                        TTY::Write((const int8_t*)" ");
                        TTY::Write((const int8_t*)msg);
                        int col = 21 + 1 + (int)strlen(msg);
                        int pad = STATUS_COL - col;
                        if (pad < 1) pad = 1;
                        for (int i = 0; i < pad; ++i) TTY::PutChar(' ');
                        if (ok) {
                            TTY::Write((const int8_t*)" [");
                            TTY::SetColor(10, 0);
                            TTY::Write((const int8_t*)" OK ");
                            TTY::SetAttr(0x07);
                            TTY::Write((const int8_t*)"]\n");
                        } else {
                            TTY::Write((const int8_t*)" [");
                            TTY::SetColor(12, 0);
                            TTY::Write((const int8_t*)"FAIL");
                            TTY::SetAttr(0x07);
                            TTY::Write((const int8_t*)"]\n");
                        }
                        // Forward status log to JournalService
                        extern void LogToJournalStatus(const char* msg, bool ok);
                        LogToJournalStatus(msg, ok);
                }

            private:
            public: // control API (kept simple)
            static inline void MuteTTY(bool m) { s_mutedTTY = m; }
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
            static void printTs(const DateTime& dt) {
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
