#include <application/init/service.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>
#include <fs/filesystem.hpp>
#include <lib/string.hpp>
#include <lib/stdio.hpp>
#include <lib/elfloader.hpp>
#include <lib/sysapi.hpp>
#include <memory/heap.hpp>
#include <process/thread_manager.hpp>

using namespace kos::console;
using namespace kos::lib;
using namespace kos::process;

// Access global filesystem selected during hardware init (defined in kernel.cpp)
extern kos::fs::Filesystem* g_fs_ptr;

namespace kos { namespace services {

static const int8_t* rc_paths[] = {
    (const int8_t*)"/etc/init.d/rc.local",
    (const int8_t*)"/ETC/INIT.D/RC.LOCAL",
};

// New: services definition config for dependency-based init
static const int8_t* svc_cfg_paths[] = {
    // Preferred new location
    (const int8_t*)"/etc/system.d/services.cfg",
    (const int8_t*)"/ETC/SYSTEM.D/SERVICES.CFG",
    // Backward-compatible legacy path
    (const int8_t*)"/etc/init.d/services.cfg",
    (const int8_t*)"/ETC/INIT.D/SERVICES.CFG",
};

// Simple restart policy
enum RestartPolicy : uint8_t { RESTART_NO = 0, RESTART_ON_FAILURE = 1, RESTART_ALWAYS = 2 };

// Service unit description (kept minimal and static-friendly)
struct InitUnit {
    // identity
    char name[32];
    // exec command and args (tokenized)
    const int8_t* argv[16];
    int32_t argc;
    // dependencies by name
    const char* after[8]; int afterCount;
    const char* before[8]; int beforeCount;
    const char* requires[8]; int requiresCount;
    // control
    RestartPolicy restart;
    uint32_t restartMs;
    bool enabled;
    // runtime
    volatile bool startedOnce;
    volatile bool running;
    uint32_t threadId;
};

// Small static registry for units read from config
static InitUnit* g_units[32];
static int g_unitCount = 0;

// Find unit by name (case-insensitive)
static InitUnit* find_unit(const char* name) {
    if (!name) return nullptr;
    for (int i = 0; i < g_unitCount; ++i) {
        // compare lowercase
        const char* a = g_units[i]->name;
        int ia = 0; int ib = 0; char ca, cb;
        while (a[ia] || name[ib]) {
            ca = a[ia]; cb = name[ib];
            if (ca >= 'A' && ca <= 'Z') ca = char(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = char(cb - 'A' + 'a');
            if (ca != cb) break; ++ia; ++ib;
        }
        if (a[ia] == 0 && name[ib] == 0) return g_units[i];
    }
    return nullptr;
}

// Split a comma-separated list in-place; returns number of items and fills arr with pointers into the buffer.
static int split_csv(int8_t* buf, const char** arr, int maxItems) {
    if (!buf) return 0;
    int n = 0; int8_t* p = buf;
    // trim leading spaces
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    while (*p && n < maxItems) {
        // mark start
        arr[n++] = (const char*)p;
        // scan to comma or end
        while (*p && *p != ',' && *p != '\n' && *p != '\r') ++p;
        if (!*p) break;
        *p++ = 0;
        // skip spaces
        while (*p == ' ' || *p == '\t') ++p;
    }
    return n;
}

// Split a line into argv tokens separated by spaces (no quotes/escapes). Returns argc.
static int split_args(int8_t* line, const int8_t** argv, int maxArgs) {
    int argc = 0;
    int8_t* p = line;
    // Trim leading spaces
    while (*p == ' ' || *p == '\t') ++p;
    while (*p && argc < maxArgs) {
        argv[argc++] = p;
        // advance to next space or end
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (!*p) break;
        *p++ = 0;
        while (*p == ' ' || *p == '\t') ++p; // skip spaces
    }
    return argc;
}

// Execute one service command (argv[0..argc-1]); builds path if needed and calls exec
static int32_t exec_argv(const int8_t** argv, int32_t argc) {
    if (!argv || argc <= 0) return -1;
    const int8_t* prog = argv[0];
    // Build absolute path for /bin/<prog>.elf if argv[0] has no '/' and no extension
    int8_t path[96];
    if (prog[0] != '/') {
        int pi = 0;
        path[pi++] = '/'; path[pi++] = 'b'; path[pi++] = 'i'; path[pi++] = 'n'; path[pi++] = '/';
        for (int i = 0; prog[i] && pi < (int)sizeof(path)-1; ++i) path[pi++] = prog[i];
        // append .elf if not present
        int needExt = 1;
        if (pi >= 4) {
            if (path[pi-4]=='.' && path[pi-3]=='e' && path[pi-2]=='l' && path[pi-1]=='f') needExt = 0;
        }
        if (needExt && pi + 4 < (int)sizeof(path)) { path[pi++]='.'; path[pi++]='e'; path[pi++]='l'; path[pi++]='f'; }
        path[pi] = 0;
    } else {
        // copy absolute path as-is
        int i=0; while (prog[i] && i < (int)sizeof(path)-1) { path[i] = prog[i]; ++i; } path[i]=0;
    }
    // Load file and execute
    static uint8_t elfBuf[256*1024];
    int32_t rn = g_fs_ptr ? g_fs_ptr->ReadFile(path, elfBuf, sizeof(elfBuf)) : -1;
    if (rn <= 0) {
        TTY::Write((const int8_t*)"initd: not found: "); TTY::Write(path); TTY::PutChar('\n');
        return -1;
  }
    // Pass args and command line (use joined argv[0] as cmdline for now)
    kos::sys::SetArgs(argc, argv, (const int8_t*)path);
    if (!ELFLoader::LoadAndExecute(elfBuf, (uint32_t)rn)) {
        TTY::Write((const int8_t*)"initd: ELF load failed\n");
        return -1;
    }
    return 0;
}

// Per-service worker: runs ExecStart, monitors return, restarts if policy says so
static void unit_worker_thread() {
    // On creation, ThreadManager gives us a description but we need to locate which unit we are for.
    // Simplify: store the InitUnit* in TLS via a simple static slot set just before thread create.
    extern InitUnit* g_thread_unit_slot; // forward static declared below
    InitUnit* u = g_thread_unit_slot; // captured for this thread
    g_thread_unit_slot = nullptr;
    if (!u) { SchedulerAPI::ExitThread(); return; }

    Logger::LogKV("initd: starting", u->name);
    u->running = true; u->startedOnce = true;

    while (u->enabled) {
        int32_t rc = exec_argv(u->argv, u->argc);
        // rc==0 means executed and returned; rc<0 means failed to start
        if (!u->enabled) break;
        if (u->restart == RESTART_ALWAYS || (u->restart == RESTART_ON_FAILURE && rc != 0)) {
            // wait restart delay
            SchedulerAPI::SleepThread((u->restartMs > 0) ? (int32_t)u->restartMs : 1000);
            continue;
        }
        break;
    }
    u->running = false;
    Logger::LogKV("initd: exited", u->name);
    SchedulerAPI::ExitThread();
}

// Thread-local-ish slot for passing InitUnit* into new system thread at creation time (simplified)
InitUnit* g_thread_unit_slot = nullptr;

// Create system thread to run the unit when its dependencies are satisfied
static void start_unit_thread(InitUnit* u) {
    if (!u || u->threadId) return;
    g_thread_unit_slot = u; // set slot for the thread entry to read
    uint32_t tid = ThreadManagerAPI::CreateSystemThread((void*)unit_worker_thread,
                        THREAD_SYSTEM_SERVICE, 4096, PRIORITY_NORMAL, u->name);
    u->threadId = tid;
}

// Supervisor: resolves ordering and launches services when requirements are ready
static void supervisor_thread() {
    Logger::Log("InitD supervisor thread running");
    // Simple loop that checks requirements and launches units when possible
    bool progress = true;
    while (true) {
        progress = false;
        for (int i = 0; i < g_unitCount; ++i) {
            InitUnit* u = g_units[i];
            if (!u->enabled || u->threadId) continue; // skip disabled or already started
            // Check Requires: all required units must exist and have started at least once and be running (best-effort)
            bool ok = true;
            for (int r = 0; r < u->requiresCount; ++r) {
                InitUnit* req = find_unit(u->requires[r]);
                if (!req || !req->startedOnce) { ok = false; break; }
            }
            // Also honor After= by waiting until those have started at least once
            if (ok) {
                for (int a = 0; a < u->afterCount; ++a) {
                    InitUnit* dep = find_unit(u->after[a]);
                    if (dep && !dep->startedOnce) { ok = false; break; }
                }
            }
            // Honor Before=: if any other unit declares before=our_name and hasn't started yet, wait
            if (ok) {
                for (int j = 0; j < g_unitCount; ++j) {
                    InitUnit* other = g_units[j];
                    if (other == u) continue;
                    for (int b = 0; b < other->beforeCount; ++b) {
                        if (String::strcmp((const uint8_t*)other->before[b], (const uint8_t*)u->name) == 0) {
                            if (!other->startedOnce) { ok = false; }
                            break;
                        }
                    }
                    if (!ok) break;
                }
            }
            if (ok) {
                start_unit_thread(u);
                progress = true;
            }
        }
        // Avoid tight loop; sleep and continue; if no progress and all units either started or disabled, we still keep monitoring restarts within worker threads
        SchedulerAPI::SleepThread(200);
    }
}

// Parse minimal services config format: lines like
// svc.NAME.execstart=/bin/foo.elf -p 8080
// svc.NAME.after=A,B  svc.NAME.before=C  svc.NAME.requires=X,Y
// svc.NAME.restart=always|on-failure|no
// svc.NAME.restartsec=2000
// svc.NAME.enabled=on|off
static bool parse_services_cfg() {
    if (!g_fs_ptr) return false;
    const uint32_t MAX_CFG = 8192;
    uint8_t* buf = (uint8_t*)kos::memory::Heap::Alloc(MAX_CFG);
    if (!buf) return false;
    int32_t n = -1; const int8_t* usedPath = nullptr;
    for (unsigned i = 0; i < sizeof(svc_cfg_paths)/sizeof(svc_cfg_paths[0]); ++i) {
        n = g_fs_ptr->ReadFile(svc_cfg_paths[i], buf, MAX_CFG - 1);
        if (n > 0) { usedPath = svc_cfg_paths[i]; break; }
    }
    if (n <= 0) { kos::memory::Heap::Free(buf); return false; }
    buf[n] = 0;
    Logger::LogKV("InitD: using services config", (const char*)usedPath);

    auto tolower_inline = [](char* s){ while (*s){ if(*s>='A'&&*s<='Z') *s=char(*s-'A'+'a'); ++s;} };
    auto trim_right = [](char* s){ int l=(int)String::strlen((const int8_t*)s); while(l>0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r')) s[--l]=0; };

    // reset registry
    g_unitCount = 0;

    char* cur = (char*)buf;
    while (*cur) {
        char* line = cur;
        char* nl = (char*)String::memchr((uint8_t*)cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += String::strlen((const int8_t*)cur); }
        // skip leading spaces and comments
        char* p = line; while (*p==' '||*p=='\t'||*p=='\r') ++p; if (*p==0 || *p=='#') continue;
        // key=value
        char* eq = (char*)String::memchr((uint8_t*)p, '=', (uint32_t)String::strlen((const int8_t*)p));
        if (!eq) continue; *eq = 0;
        char* key = p; char* val = eq + 1; trim_right(key);
        while (*val==' '||*val=='\t') ++val; trim_right(val);
        // We expect keys starting with "svc."
        if (!(key[0]=='s' && key[1]=='v' && key[2]=='c' && key[3]=='.')) continue;
        // find next '.' after name
        char* nameBeg = key + 4;
        char* dot = nameBeg; while (*dot && *dot != '.') ++dot;
        if (*dot != '.') continue; *dot = 0; char* prop = dot + 1;
        // locate or create unit
        InitUnit* u = find_unit(nameBeg);
        if (!u) {
            if (g_unitCount >= (int)(sizeof(g_units)/sizeof(g_units[0]))) continue;
            u = (InitUnit*)kos::memory::Heap::Alloc(sizeof(InitUnit));
            if (!u) continue;
            for (int i=0;i<(int)sizeof(*u);++i) ((uint8_t*)u)[i]=0;
            // store lowercased name
            int ni = 0; while (nameBeg[ni] && ni < (int)sizeof(u->name)-1) { char c = nameBeg[ni]; if(c>='A'&&c<='Z') c=char(c-'A'+'a'); u->name[ni++] = c; }
            u->name[ni] = 0; u->enabled = true; u->restart = RESTART_ON_FAILURE; u->restartMs = 2000;
            g_units[g_unitCount++] = u;
        }
        // dispatch prop
        // normalize prop to lowercase
        tolower_inline(prop);
        if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"execstart") == 0) {
            // tokenize into argv
            int8_t tmp[192]; int ti=0; while (val[ti] && ti < (int)sizeof(tmp)-1) { tmp[ti] = val[ti]; ++ti; } tmp[ti]=0;
            u->argc = split_args(tmp, u->argv, (int)(sizeof(u->argv)/sizeof(u->argv[0])));
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"after") == 0) {
            int8_t tmp[160]; int i=0; while (val[i] && i < (int)sizeof(tmp)-1) { tmp[i]=val[i]; ++i; } tmp[i]=0;
            u->afterCount = split_csv(tmp, u->after, (int)(sizeof(u->after)/sizeof(u->after[0])));
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"before") == 0) {
            int8_t tmp[160]; int i=0; while (val[i] && i < (int)sizeof(tmp)-1) { tmp[i]=val[i]; ++i; } tmp[i]=0;
            u->beforeCount = split_csv(tmp, u->before, (int)(sizeof(u->before)/sizeof(u->before[0])));
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"requires") == 0) {
            int8_t tmp[160]; int i=0; while (val[i] && i < (int)sizeof(tmp)-1) { tmp[i]=val[i]; ++i; } tmp[i]=0;
            u->requiresCount = split_csv(tmp, u->requires, (int)(sizeof(u->requires)/sizeof(u->requires[0])));
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"restart") == 0) {
            // lower-case val inline copy
            char v[16]; int vi=0; while (val[vi] && vi < (int)sizeof(v)-1) { char c = val[vi]; if(c>='A'&&c<='Z') c=char(c-'A'+'a'); v[vi++]=c; } v[vi]=0;
            if (String::strcmp((const uint8_t*)v, (const uint8_t*)"always") == 0) u->restart = RESTART_ALWAYS;
            else if (String::strcmp((const uint8_t*)v, (const uint8_t*)"no") == 0) u->restart = RESTART_NO;
            else u->restart = RESTART_ON_FAILURE;
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"restartsec") == 0) {
            // parse integer ms
            uint32_t ms = 0; for (int i=0; val[i]; ++i) { if (val[i] >= '0' && val[i] <= '9') { ms = ms*10 + (uint32_t)(val[i]-'0'); } else break; }
            u->restartMs = ms;
        } else if (String::strcmp((const uint8_t*)prop, (const uint8_t*)"enabled") == 0) {
            char v[8]; int vi=0; while (val[vi] && vi < (int)sizeof(v)-1) { char c = val[vi]; if(c>='A'&&c<='Z') c=char(c-'A'+'a'); v[vi++]=c; } v[vi]=0;
            u->enabled = (String::strcmp((const uint8_t*)v, (const uint8_t*)"on") == 0 || String::strcmp((const uint8_t*)v, (const uint8_t*)"true") == 0 || String::strcmp((const uint8_t*)v, (const uint8_t*)"1") == 0);
        }
    }

    kos::memory::Heap::Free(buf);

    // Note: Before= is honored at launch time by holding back units until all units that must come before them have started.
    return g_unitCount > 0;
}

// Helper: run legacy rc.local synchronously (existing behavior)
static bool run_rc_local() {
    Logger::Log("InitD: checking /etc/init.d/rc.local");

    const uint32_t MAX_RC = 8192;
    uint8_t* buf = (uint8_t*)kos::memory::Heap::Alloc(MAX_RC);
    if (!buf) { Logger::Log("InitD: OOM"); return false; }
    int32_t n = -1;
    const int8_t* usedPath = nullptr;
    for (unsigned i = 0; i < sizeof(rc_paths)/sizeof(rc_paths[0]); ++i) {
        n = g_fs_ptr->ReadFile(rc_paths[i], buf, MAX_RC - 1);
        if (n > 0) { usedPath = rc_paths[i]; break; }
    }
    if (n <= 0) {
        Logger::Log("InitD: no rc.local found; continuing");
        kos::memory::Heap::Free(buf);
        return true;
    }
    buf[n] = 0;

    Logger::LogKV("InitD: executing rc.local", (const char*)usedPath);

    // Parse and execute each non-empty non-comment line synchronously
    int8_t* cur = (int8_t*)buf;
    while (*cur) {
        int8_t* line = cur;
        // find end of line
        int8_t* nl = (int8_t*)String::memchr((uint8_t*)cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
        if (nl) { *nl = 0; cur = nl + 1; } else { cur += String::strlen((const int8_t*)cur); }

        // trim leading spaces
        int8_t* p = line; while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == 0) continue; // empty
        if (*p == '#') continue; // comment

        int8_t tmp[160];
        int li = 0; while (p[li] && li < (int)sizeof(tmp)-1) { tmp[li] = p[li]; ++li; }
        tmp[li] = 0;

        // Split into argv (simple space tokenizer)
        const int32_t MAX_ARGS = 16;
        const int8_t* argv[MAX_ARGS];
        int argc = split_args(tmp, argv, MAX_ARGS);
        if (argc <= 0) continue;

        // Execute
        TTY::Write((const int8_t*)"initd: exec "); TTY::Write(argv[0]); TTY::PutChar('\n');
        (void)exec_argv(argv, argc);
    }

    kos::memory::Heap::Free(buf);
    return true;
}

bool InitDService::Start() {
    if (!g_fs_ptr) {
        // No filesystem: nothing to run
        Logger::Log("InitD: no filesystem; skipping rc.local");
        return true; // don't block boot
    }

    // Detect userspace init presence; if present, we'll still run services.cfg supervisor
    // but will skip rc.local fallback to avoid duplication.
    bool user_init_present = false;
    {
        uint8_t test[1];
        int32_t t = g_fs_ptr->ReadFile((const int8_t*)"/bin/init.elf", test, 1);
        user_init_present = (t > 0);
        if (user_init_present) {
            Logger::Log("InitD: /bin/init.elf present; will run services manager if configured, skipping rc.local fallback");
        }
    }

    // Ensure init process starts with root as working directory
    kos::sys::SetCwd((const int8_t*)"/");
    // Preferred mode: dependency-aware services if config exists (runs regardless of userspace init)
    if (parse_services_cfg()) {
        // Start a supervisor thread to launch and monitor units
        uint32_t tid = ThreadManagerAPI::CreateSystemThread((void*)supervisor_thread,
                                    THREAD_SYSTEM_SERVICE, 4096, PRIORITY_LOW, "initd-supervisor");
        (void)tid;
        // If userspace init exists, let it continue; we don't need rc.local
        if (user_init_present) return true;
        // Otherwise, continue to rc.local fallback if desired (but we already have services configured)
        return true;
    }

    // Fallback: legacy rc.local sequential execution (only if no userspace init)
    if (!user_init_present) {
        return run_rc_local();
    }
    return true;
}

}} // namespace
