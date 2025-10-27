#ifndef KOS_APPLICATION_APP_LOG_H
#define KOS_APPLICATION_APP_LOG_H

#include <lib/libc/stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
// Exported by kernel for journald-like logging
extern void LogToJournal(const char* message);
static inline void app_log(const int8_t* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    kos_vprintf(fmt, ap);
    va_end(ap);
    // Forward to journald-like service
    char buf[256];
    va_start(ap, fmt);
    vsnprintf((char*)buf, sizeof(buf), (const char*)fmt, ap);
    va_end(ap);
    LogToJournal(buf);
}
#ifdef __cplusplus
}
#endif
#endif // KOS_APPLICATION_APP_LOG_H
