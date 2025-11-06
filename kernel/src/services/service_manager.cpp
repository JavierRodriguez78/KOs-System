#include <services/service_manager.hpp>
#include <services/journal_service.hpp>
#include <services/banner_service.hpp>
#include <services/time_service.hpp>
#include <services/filesystem_service.hpp>
#include <services/network_manager.hpp>
#include <application/init/service.hpp>
#include <fs/filesystem.hpp>
#include <lib/string.hpp>
#include <lib/libc/string.h>
#include <memory/heap.hpp>
#include <process/timer.hpp>
#include <common/types.hpp>

using namespace kos::console;
using namespace kos::fs;
using namespace kos::lib;
using namespace kos::memory;
using namespace kos::services;

// Global pointer to the registered JournalService instance
static JournalService* g_journal_service = nullptr;

// Accessor for JournalService
JournalService* GetJournalService() { return g_journal_service; }

// Logger forwarding functions for journald-like logging
extern "C" void LogToJournal(const char* message) {
    if (g_journal_service && ServiceManager::IsEnabled("JOURNAL")) {
        g_journal_service->log(message);
    }
}
extern "C" void LogToJournalKV(const char* key, const char* value) {
    if (g_journal_service && ServiceManager::IsEnabled("JOURNAL")) {
        char buf[256];
    String::strncpy((int8_t*)buf, (const int8_t*)key, 128);
    String::strcat((char*)buf, ": ");
    String::strcat((char*)buf, (const char*)value);
        g_journal_service->log(buf);
    }
}
extern "C" void LogToJournalStatus(const char* msg, bool ok) {
    if (g_journal_service && ServiceManager::IsEnabled("JOURNAL")) {
        char buf[256];
    String::strncpy((int8_t*)buf, (const int8_t*)msg, 220);
    String::strcat((char*)buf, ok ? " [OK]" : " [FAIL]");
        g_journal_service->log(buf);
    }
}

// Helper at global scope to access the filesystem pointer defined in kernel.cpp
Filesystem* get_fs() {
    return kos::fs::g_fs_ptr;
}

// Helper functions for config parsing
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

// Static member definitions for ServiceManager
ServiceNode* ServiceManager::s_head = nullptr;
bool ServiceManager::s_debugCfg = false;
uint32_t ServiceManager::s_boot_ms = 0;

// Register built-in services here
static void RegisterBuiltinServices() {
    // To avoid duplicate registrations with kernel.cpp, we only register
    // services that are NOT already registered by the kernel main:
    // - Kernel registers: FS, BANNER, TIME, INITD (and WindowManager)
    // - ServiceManager registers: JOURNAL and NETWORK only

    static JournalService journalService;
    g_journal_service = &journalService;
    ServiceManager::Register(&journalService);

    static NetworkManagerService networkService;
    ServiceManager::Register(&networkService);
}


void ServiceManager::ApplyConfig() {
    Filesystem* fs = get_fs();
    if (!fs) {
        Logger::Log("ServiceManager: No filesystem; using defaults");
        return;
    }
    const int8_t* path = (const int8_t*)"/etc/services.cfg";
    const uint32_t MAX_CFG = 4096;
    uint8_t* buf = (uint8_t*)Heap::Alloc(MAX_CFG);
    if (!buf) return;
    int32_t n = fs->ReadFile(path, buf, MAX_CFG - 1);
    if (n <= 0) {
        path = (const int8_t*)"/ETC/SERVICES.CFG";
        n = fs->ReadFile(path, buf, MAX_CFG - 1);
    }
    if (n <= 0) {
        Heap::Free(buf);
        Logger::Log("ServiceManager: No config found; using defaults");
        return;
    }
    buf[n] = 0;

    char* cur = (char*)buf;
    while (*cur) {
        char* line = cur;
        char* nl = (char*)String::memchr(cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += String::strlen((const int8_t*)cur); }

        const char* p = skip_ws(line);
        if (*p == 0 || *p == '#') continue;

        char* eq = (char*)String::memchr(p, '=', String::strlen((const int8_t*)p));
        if (!eq) continue;
        *eq = 0;
        char* key = (char*)p;
        char* val = skip_ws(eq + 1) == eq + 1 ? (eq + 1) : (char*)skip_ws(eq + 1);

        int keylen = (int)String::strlen((const int8_t*)key);
        while (keylen > 0 && (key[keylen - 1] == ' ' || key[keylen - 1] == '\t')) { key[--keylen] = 0; }
        int vallen = (int)String::strlen((const int8_t*)val);
        while (vallen > 0 && (val[vallen - 1] == ' ' || val[vallen - 1] == '\r' || val[vallen - 1] == '\t')) { val[--vallen] = 0; }
        strtolower(key);
        strtolower(val);

        if (String::strcmp((const uint8_t*)key, (const uint8_t*)"debug") == 0) {
            s_debugCfg = (String::strcmp((const char*)val, "on") == 0 ||
                          String::strcmp((const char*)val, "true") == 0 ||
                          String::strcmp((const char*)val, "1") == 0);
            continue;
        }

        if (starts_with(key, "service.")) {
            const char* svcname = key + 8;
            ServiceNode* node = s_head;
            while (node) {
                const char* nm = node->svc->Name();
                char tmp[64];
                int i = 0; for (; nm[i] && i < 63; ++i) { tmp[i] = nm[i]; }
                tmp[i] = 0; strtolower(tmp);
                if (String::strcmp((const uint8_t*)tmp, (const uint8_t*)svcname) == 0) {
                    node->enabled = (String::strcmp((const char*)val, "on") == 0 ||
                                     String::strcmp((const char*)val, "true") == 0 ||
                                     String::strcmp((const char*)val, "1") == 0);
                    break;
                }
                node = node->next;
            }
        }
    }

    Heap::Free(buf);
}

uint32_t ServiceManager::UptimeMs() {
    extern SchedulerTimerHandler* g_timer_handler_for_services;
    (void)g_timer_handler_for_services;
    return 0;
}

void ServiceManager::InitAndStart() {
    Logger::Log("ServiceManager: applying configuration");
    RegisterBuiltinServices();
    ApplyConfig();
    if (s_debugCfg) Logger::SetDebugEnabled(true);

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
        SchedulerAPI::SleepThread(500);
    }
}

// Implement the public API in the proper namespace as declared in the header
namespace kos { namespace services { namespace ServiceAPI {
    bool StartManagerThread() {
        if (!g_thread_manager) return false;
        uint32_t tid = ThreadManagerAPI::CreateSystemThread((void*)service_manager_thread,
            THREAD_SYSTEM_SERVICE, 4096, PRIORITY_LOW, "service-manager");
        return tid != 0;
    }
} } }

// ...existing code...

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


bool ServiceManager::DebugFromConfig() { return s_debugCfg; }