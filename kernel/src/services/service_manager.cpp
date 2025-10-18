#include <services/service_manager.hpp>
#include <fs/filesystem.hpp>
#include <lib/string.hpp>
#include <memory/heap.hpp>
#include <process/timer.hpp>

using namespace kos::console;
using namespace kos::fs;
using namespace kos::lib;
using namespace kos::memory;

// Helper at global scope to access the filesystem pointer defined in kernel.cpp
static kos::fs::Filesystem* get_fs() {
    extern kos::fs::Filesystem* g_fs_ptr; // defined in kernel.cpp
    return g_fs_ptr;
}

namespace kos { 
    namespace services {

ServiceNode* ServiceManager::s_head = nullptr;
bool ServiceManager::s_debugCfg = false;
uint32_t ServiceManager::s_boot_ms = 0;


void ServiceManager::Register(IService* service) {
    if (!service) return;
    ServiceNode* node = (ServiceNode*)Heap::Alloc(sizeof(ServiceNode));
    if (!node) return;
    node->svc = service;
    node->enabled = service->DefaultEnabled();
    node->last_tick_ms = 0;
    node->next = s_head;
    s_head = node;
}

static bool starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    uint32_t n = String::strlen((const int8_t*)prefix);
    return String::strcmp((const int8_t*)s, (const int8_t*)prefix, n) == 0;
}

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    return p;
}

static void strtolower(char* s) {
    for (; *s; ++s) {
        if (*s >= 'A' && *s <= 'Z') *s = char(*s - 'A' + 'a');
    }
}

bool ServiceManager::DebugFromConfig() { return s_debugCfg; }

void ServiceManager::ApplyConfig() {
    Filesystem* fs = get_fs();
    if (!fs) {
        Logger::Log("ServiceManager: No filesystem; using defaults");
        return;
    }

    // Try to read /etc/services.cfg (FAT is typically uppercase, but we normalize path handling in FS)
    const int8_t* path = (const int8_t*)"/etc/services.cfg";
    const uint32_t MAX_CFG = 4096;
    uint8_t* buf = (uint8_t*)Heap::Alloc(MAX_CFG);
    if (!buf) return;
    int32_t n = fs->ReadFile(path, buf, MAX_CFG - 1);
    if (n <= 0) {
        Heap::Free(buf);
        Logger::Log("ServiceManager: No config found; using defaults");
        return;
    }
    buf[n] = 0;

    // Parse line by line: key=value
    char* cur = (char*)buf;
    while (*cur) {
        // Find end of line
        char* line = cur;
        char* nl = (char*)String::memchr(cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += String::strlen((const int8_t*)cur); }

        // Trim leading spaces
        const char* p = skip_ws(line);
        if (*p == 0 || *p == '#') continue;

        // Find '='
        char* eq = (char*)String::memchr(p, '=', String::strlen((const int8_t*)p));
        if (!eq) continue;
        *eq = 0;
        char* key = (char*)p;
        char* val = skip_ws(eq + 1) == eq + 1 ? (eq + 1) : (char*)skip_ws(eq + 1);

        // Normalize: lowercase key and value without trailing spaces
        // Trim right spaces for key
        int keylen = (int)String::strlen((const int8_t*)key);
        while (keylen > 0 && (key[keylen - 1] == ' ' || key[keylen - 1] == '\t')) { key[--keylen] = 0; }
        // Trim val trailing spaces
        int vallen = (int)String::strlen((const int8_t*)val);
        while (vallen > 0 && (val[vallen - 1] == ' ' || val[vallen - 1] == '\r' || val[vallen - 1] == '\t')) { val[--vallen] = 0; }
        strtolower(key);
        strtolower(val);

        if (String::strcmp((const uint8_t*)key, (const uint8_t*)"debug") == 0) {
            s_debugCfg = (String::strcmp((const uint8_t*)val, (const uint8_t*)"on") == 0 ||
                          String::strcmp((const uint8_t*)val, (const uint8_t*)"true") == 0 ||
                          String::strcmp((const uint8_t*)val, (const uint8_t*)"1") == 0);
            continue;
        }

        if (starts_with(key, "service.")) {
            const char* svcname = key + 8; // after 'service.'
            ServiceNode* node = s_head;
            while (node) {
                // Compare case-insensitive: we lowercased key; ensure service names are comparable
                // Copy service name to tmp lower buffer
                const char* nm = node->svc->Name();
                char tmp[64];
                int i = 0; for (; nm[i] && i < 63; ++i) { tmp[i] = nm[i]; }
                tmp[i] = 0; strtolower(tmp);
                if (String::strcmp((const uint8_t*)tmp, (const uint8_t*)svcname) == 0) {
                    node->enabled = (String::strcmp((const uint8_t*)val, (const uint8_t*)"on") == 0 ||
                                     String::strcmp((const uint8_t*)val, (const uint8_t*)"true") == 0 ||
                                     String::strcmp((const uint8_t*)val, (const uint8_t*)"1") == 0);
                    break;
                }
                node = node->next;
            }
        }
    }

    Heap::Free(buf);
}

uint32_t ServiceManager::UptimeMs() {
    // Approximate uptime using PIT frequency and ticks.
    // Here we don't track tick count globally, so we approximate with a monotonically increasing ms counter
    // using the scheduler's tick rate if available.
    // For simplicity in this environment, we reuse RTC seconds for coarse time or keep an internal counter.
    // We'll use a simple static counter incremented by TickAll call intervals; s_boot_ms is starting point.
    extern SchedulerTimerHandler* g_timer_handler_for_services; // optional
    (void)g_timer_handler_for_services;
    // Not strictly accurate; we update per TickAll cadence.
    // Return current ms via RTC second granularity isn't ideal; leave as placeholder.
    // For scheduling of ticks we rely on SleepThread durations rather than exact uptime.
    return 0;
}

void ServiceManager::InitAndStart() {
    Logger::Log("ServiceManager: applying configuration");
    ApplyConfig();
    if (s_debugCfg) Logger::SetDebugEnabled(true);

    // Start enabled services
    ServiceNode* node = s_head;
    while (node) {
        if (node->enabled) {
            Logger::LogKV("Starting service", node->svc->Name());
            bool ok = node->svc->Start();
            Logger::LogStatus("Service start", ok);
        } else {
            Logger::LogKV("Service disabled", node->svc->Name());
        }
        node = node->next;
    }
}

void ServiceManager::TickAll() {
    // Iterate and call Tick() if interval elapsed. We'll just call unconditionally; intervals are advisory.
    ServiceNode* node = s_head;
    while (node) {
        if (node->enabled) {
            node->svc->Tick();
        }
        node = node->next;
    }
}

bool ServiceManager::IsEnabled(const char* name) {
    ServiceNode* node = s_head;
    while (node) {
        if (String::strcmp((const uint8_t*)node->svc->Name(), (const uint8_t*)name) == 0) return node->enabled;
        node = node->next;
    }
    return false;
}

// Background thread to drive TickAll periodically
static void service_manager_thread() {
    Logger::Log("ServiceManager thread running");
    while (true) {
        ServiceManager::TickAll();
        SchedulerAPI::SleepThread(500); // 500ms cadence
    }
}

namespace ServiceAPI {
    bool StartManagerThread() {
        if (!g_thread_manager) return false;
        uint32_t tid = ThreadManagerAPI::CreateSystemThread((void*)service_manager_thread,
            THREAD_SYSTEM_SERVICE, 4096, PRIORITY_LOW, "service-manager");
        return tid != 0;
    }
}

}} // namespace
