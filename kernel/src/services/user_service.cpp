#include <services/user_service.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <memory/heap.hpp>
#include <fs/filesystem.hpp>

using namespace kos::services;
using namespace kos::lib;
using namespace kos::console;

// Singleton instance pointer
static UserService* g_user_service = nullptr;

namespace kos { namespace services {
    UserService* GetUserService() { return g_user_service; }
} }

bool UserService::Start() {
    // Initialize empty user list
    m_user_count = 0;
    m_current_index = -1;
    // Try to load persisted users
    bool loaded = false;
    if (kos::fs::g_fs_ptr) {
        static uint8_t buf[4096];
        int32_t n = kos::fs::g_fs_ptr->ReadFile((const int8_t*)"/ETC/USERS.CFG", buf, sizeof(buf)-1);
        if (n <= 0) {
            n = kos::fs::g_fs_ptr->ReadFile((const int8_t*)"/etc/users.cfg", buf, sizeof(buf)-1);
        }
        if (n > 0) {
            buf[n] = 0;
            char* cur = (char*)buf;
            while (*cur) {
                char* line = cur;
                char* nl = (char*)kos::lib::String::memchr(cur, '\n', (uint32_t)((buf + n) - (uint8_t*)cur));
                if (nl) { *nl = 0; cur = nl + 1; } else { cur += kos::lib::String::strlen((const int8_t*)cur); }
                if (*line == 0 || *line == '#') continue;
                // Format: name:uid:gid:passhash:salt:locked
                char* p = line;
                char* name = p; while (*p && *p != ':') ++p; if (*p != ':') continue; *p++ = 0;
                char* suid = p; while (*p && *p != ':') ++p; if (*p != ':') continue; *p++ = 0;
                char* sgid = p; while (*p && *p != ':') ++p; if (*p != ':') continue; *p++ = 0;
                char* shash = p; while (*p && *p != ':') ++p; if (*p != ':') continue; *p++ = 0;
                char* ssalt = p; while (*p && *p != ':') ++p; if (*p != ':') continue; *p++ = 0;
                char* slock = p; // last field to end
                // parse
                uint32_t uid = 0, gid = 0; uint64_t passh = 0; uint64_t salt = 0; bool locked = false;
                // simple decimal for uid/gid
                for (char* q = suid; *q; ++q) { if (*q >= '0' && *q <= '9') uid = uid*10 + (*q - '0'); }
                for (char* q = sgid; *q; ++q) { if (*q >= '0' && *q <= '9') gid = gid*10 + (*q - '0'); }
                // hex for hash (optional 0x prefix)
                char* h = shash; if (h[0]=='0' && (h[1]=='x'||h[1]=='X')) h+=2; 
                while (*h) {
                    uint8_t v;
                    if (*h>='0' && *h<='9') v = *h - '0';
                    else if (*h>='a' && *h<='f') v = 10 + (*h - 'a');
                    else if (*h>='A' && *h<='F') v = 10 + (*h - 'A');
                    else break;
                    passh = (passh<<4) | v; ++h;
                }
                // parse salt hex
                char* ssp = ssalt; if (ssp[0]=='0' && (ssp[1]=='x'||ssp[1]=='X')) ssp+=2; 
                while (*ssp) {
                    uint8_t v;
                    if (*ssp>='0' && *ssp<='9') v = *ssp - '0';
                    else if (*ssp>='a' && *ssp<='f') v = 10 + (*ssp - 'a');
                    else if (*ssp>='A' && *ssp<='F') v = 10 + (*ssp - 'A');
                    else break;
                    salt = (salt<<4) | v; ++ssp;
                }
                locked = (slock[0]=='1' || slock[0]=='t' || slock[0]=='T' || slock[0]=='y' || slock[0]=='Y');
                // add
                if (m_user_count < kMaxUsers) {
                    User u{}; u.uid = uid; u.gid = gid; u.pass_hash = passh; u.salt = salt; u.locked = locked;
                    int i = 0; while (name[i] && i < (int)sizeof(u.name)-1) { u.name[i] = name[i]; ++i; } u.name[i] = 0;
                    m_users[m_user_count++] = u;
                }
            }
            loaded = (m_user_count > 0);
        }
    }
    if (!loaded) {
        // Bootstrap defaults
        AddUser((const int8_t*)"root", (const int8_t*)"root", true);
        AddUser((const int8_t*)"user", (const int8_t*)"user", false);
        // Persist defaults if possible
        if (kos::fs::g_fs_ptr) {
            // Save current users
            // Build text content
            char out[1024]; int pos = 0;
            for (int i=0;i<m_user_count;++i) {
                const User& u = m_users[i];
                // name:uid:gid:passhash:salt:locked\n
                // name
                for (int j=0; u.name[j] && pos < (int)sizeof(out)-1; ++j) out[pos++] = u.name[j];
                out[pos++]=':';
                // uid
                char num[16]; int ni=0; uint32_t v=u.uid; char tmp[16]; int t=0; if (v==0){tmp[t++]='0';}
                while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
                while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
                out[pos++]=':';
                // gid
                ni=0; v=u.gid; t=0; if (v==0){tmp[t++]='0';}
                while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
                while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
                out[pos++]=':';
                // hash hex
                out[pos++]='0'; out[pos++]='x';
                uint64_t hv=u.pass_hash; int sh=60; for(; sh>=0 && pos<(int)sizeof(out)-1; sh-=4){
                    uint8_t nib=(hv>>sh)&0xF; out[pos++] = (nib<10)?char('0'+nib):char('a'+(nib-10));
                }
                out[pos++]=':';
                // salt hex
                out[pos++]='0'; out[pos++]='x';
                uint64_t sv=u.salt; int shs=60; for(; shs>=0 && pos<(int)sizeof(out)-1; shs-=4){
                    uint8_t nib=(sv>>shs)&0xF; out[pos++] = (nib<10)?char('0'+nib):char('a'+(nib-10));
                }
                out[pos++]=':';
                out[pos++] = u.locked ? '1' : '0';
                out[pos++]='\n';
            }
            kos::fs::g_fs_ptr->WriteFile((const int8_t*)"/ETC/USERS.CFG", (const uint8_t*)out, (uint32_t)pos);
        }
    }

    Logger::Log("UserService started: root + user created");
    // Register singleton accessor
    g_user_service = this;
    return true;
}

uint64_t UserService::HashPassword(const int8_t* s, uint64_t salt) const {
    // Simple salted mix: two rounds of FNV-1a with salt folded
    const uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL ^ (salt*0x9e3779b97f4a7c15ULL);
    const uint64_t FNV_PRIME  = 0x100000001b3ULL;
    uint64_t h = FNV_OFFSET;
    if (s) {
        while (*s) {
            h ^= (uint8_t)(*s++);
            h *= FNV_PRIME;
        }
    }
    // second round with salt bytes
    uint64_t t = salt;
    for (int i=0;i<8;i++){ h ^= (uint8_t)(t & 0xFF); h *= FNV_PRIME; t >>= 8; }
    return h ^ (salt<<1);
}

int32_t UserService::IndexOf(const int8_t* name) const {
    if (!name) return -1;
    for (int32_t i = 0; i < m_user_count; ++i) {
        if (String::strcmp(m_users[i].name, name) == 0) return i;
    }
    return -1;
}

bool UserService::AddUser(const int8_t* name, const int8_t* password, bool superuser) {
    if (!name || !*name) return false;
    if (m_user_count >= kMaxUsers) return false;
    if (IndexOf(name) >= 0) return false;
    User u{};
    // uid: 0 reserved for root, others start at 1000
    u.uid = superuser ? 0u : (uint32_t)(1000u + (uint32_t)m_user_count);
    u.gid = superuser ? 0u : u.uid;
    u.locked = false;
    // copy name
    int i = 0; while (name[i] && i < (int)sizeof(u.name)-1) { u.name[i] = name[i]; ++i; } u.name[i] = 0;
    // derive per-user salt deterministically (no RNG available)
    uint64_t s = 0x9e3779b97f4a7c15ULL;
    for (int j=0; name[j]; ++j) { s ^= (uint8_t)name[j]; s = (s<<13) | (s>>51); s *= 0x100000001b3ULL; }
    s ^= (uint64_t)m_user_count * 0x5851f42d4c957f2dULL;
    u.salt = s;
    u.pass_hash = HashPassword(password, u.salt);
    m_users[m_user_count++] = u;
    // Persist change
    if (kos::fs::g_fs_ptr) {
        // simple resave all
        char out[1024]; int pos = 0;
        for (int i=0;i<m_user_count;++i) {
            const User& uu = m_users[i];
            for (int j=0; uu.name[j] && pos < (int)sizeof(out)-1; ++j) out[pos++] = uu.name[j];
            out[pos++]=':';
            // uid
            char num[16]; int ni=0; uint32_t v=uu.uid; char tmp[16]; int t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            // gid
            ni=0; v=uu.gid; t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            // hash hex
            out[pos++]='0'; out[pos++]='x';
            uint64_t hv=uu.pass_hash; int sh=60; for(; sh>=0 && pos<(int)sizeof(out)-1; sh-=4){
                uint8_t nib=(hv>>sh)&0xF; out[pos++] = (nib<10)?char('0'+nib):char('a'+(nib-10));
            }
            out[pos++]=':';
            out[pos++] = uu.locked ? '1' : '0';
            out[pos++]='\n';
        }
        kos::fs::g_fs_ptr->WriteFile((const int8_t*)"/ETC/USERS.CFG", (const uint8_t*)out, (uint32_t)pos);
    }
    return true;
}

bool UserService::DelUser(const int8_t* name) {
    int32_t idx = IndexOf(name);
    if (idx < 0) return false;
    // Prevent deleting root
    if (m_users[idx].uid == 0) return false;
    // If deleting current user, logout
    if (idx == m_current_index) m_current_index = -1;
    // Compact array
    for (int32_t i = idx + 1; i < m_user_count; ++i) m_users[i-1] = m_users[i];
    --m_user_count;
    if (kos::fs::g_fs_ptr) {
        char out[1024]; int pos = 0;
        for (int i=0;i<m_user_count;++i) {
            const User& uu = m_users[i];
            for (int j=0; uu.name[j] && pos < (int)sizeof(out)-1; ++j) out[pos++] = uu.name[j];
            out[pos++]=':';
            char num[16]; int ni=0; uint32_t v=uu.uid; char tmp[16]; int t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            ni=0; v=uu.gid; t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            out[pos++]='0'; out[pos++]='x';
            uint64_t hv=uu.pass_hash; int sh=60; for(; sh>=0 && pos<(int)sizeof(out)-1; sh-=4){
                uint8_t nib=(hv>>sh)&0xF; out[pos++] = (nib<10)?char('0'+nib):char('a'+(nib-10));
            }
            out[pos++]=':';
            out[pos++] = uu.locked ? '1' : '0';
            out[pos++]='\n';
        }
        kos::fs::g_fs_ptr->WriteFile((const int8_t*)"/ETC/USERS.CFG", (const uint8_t*)out, (uint32_t)pos);
    }
    return true;
}

bool UserService::SetPassword(const int8_t* name, const int8_t* newPassword) {
    int32_t idx = IndexOf(name);
    if (idx < 0) return false;
    if (m_users[idx].locked) return false;
    m_users[idx].pass_hash = HashPassword(newPassword, m_users[idx].salt);
    if (kos::fs::g_fs_ptr) {
        char out[1024]; int pos = 0;
        for (int i=0;i<m_user_count;++i) {
            const User& uu = m_users[i];
            for (int j=0; uu.name[j] && pos < (int)sizeof(out)-1; ++j) out[pos++] = uu.name[j];
            out[pos++]=':';
            char num[16]; int ni=0; uint32_t v=uu.uid; char tmp[16]; int t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            ni=0; v=uu.gid; t=0; if (v==0){tmp[t++]='0';}
            while (v>0) { tmp[t++] = char('0' + (v%10)); v/=10; }
            while (t>0) num[ni++]=tmp[--t]; num[ni]=0; for(int j=0;j<ni && pos<(int)sizeof(out)-1;++j) out[pos++]=num[j];
            out[pos++]=':';
            out[pos++]='0'; out[pos++]='x';
            uint64_t hv=uu.pass_hash; int sh=60; for(; sh>=0 && pos<(int)sizeof(out)-1; sh-=4){
                uint8_t nib=(hv>>sh)&0xF; out[pos++] = (nib<10)?char('0'+nib):char('a'+(nib-10));
            }
            out[pos++]=':';
            out[pos++] = uu.locked ? '1' : '0';
            out[pos++]='\n';
        }
        kos::fs::g_fs_ptr->WriteFile((const int8_t*)"/ETC/USERS.CFG", (const uint8_t*)out, (uint32_t)pos);
    }
    return true;
}

bool UserService::Authenticate(const int8_t* name, const int8_t* password) {
    int32_t idx = IndexOf(name);
    if (idx < 0) return false;
    if (m_users[idx].locked) return false;
    uint64_t h = HashPassword(password, m_users[idx].salt);
    return h == m_users[idx].pass_hash;
}

bool UserService::Login(const int8_t* name, const int8_t* password) {
    if (!Authenticate(name, password)) return false;
    m_current_index = IndexOf(name);
    return (m_current_index >= 0);
}

void UserService::Logout() {
    m_current_index = -1;
}

const User* UserService::CurrentUser() const {
    if (m_current_index < 0 || m_current_index >= m_user_count) return nullptr;
    return &m_users[m_current_index];
}

const int8_t* UserService::CurrentUserName() const {
    const User* u = CurrentUser();
    return u ? u->name : (const int8_t*)"(none)";
}

bool UserService::IsSuperuser() const {
    const User* u = CurrentUser();
    return u && (u->uid == 0);
}

int32_t UserService::UserCount() const { return m_user_count; }

const User* UserService::Find(const int8_t* name) const {
    int32_t idx = IndexOf(name);
    return (idx >= 0) ? &m_users[idx] : nullptr;
}
