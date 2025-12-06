#include <console/kcursers.hpp>
#include <lib/libc/stdio.h>
#include <lib/libc/string.h>
#include <lib/libc/stdint.h>

using namespace kos::console::kcursers;

struct ProcInfo {
    uint32_t pid; char state[10]; uint32_t prio; uint32_t time; char name[32];
};

static int parse_line(const char* line, ProcInfo* info) {
    if (!line || !info) return 0; const char* p = line;
    while (*p==' '||*p=='\t'||*p=='\r') ++p; if (*p<'0'||*p>'9') return 0;
    info->pid=0; int got=0; while (*p>='0'&&*p<='9'){info->pid=info->pid*10+(*p-'0');++p;got=1;} if(!got)return 0; while(*p==' '||*p=='\t')++p;
    int i=0; got=0; while(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n'&&i<9){info->state[i++]=*p++;got=1;} info->state[i]=0; if(!got)return 0; while(*p==' '||*p=='\t')++p;
    info->prio=0; got=0; while(*p>='0'&&*p<='9'){info->prio=info->prio*10+(*p-'0');++p;got=1;} if(!got)return 0; while(*p==' '||*p=='\t')++p;
    info->time=0; got=0; while(*p>='0'&&*p<='9'){info->time=info->time*10+(*p-'0');++p;got=1;} if(!got)return 0; while(*p==' '||*p=='\t')++p;
    i=0; got=0; while(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n'&&i<31){info->name[i++]=*p++;got=1;} info->name[i]=0; if(!got)return 0;
    return 5;
}

static void print_padded(const char* s, int width) {
    if (!s) s = ""; addstr(s); int len=0; while (s[len] && len<width) ++len; for (; len<width; ++len) addch(' ');
}

extern "C" int main(void) {
    char buffer[4096];
    while (1) {
        clear(); set_color(15,0);
        addstr("KOS top (kcursers) - Process List\n");
        addstr("PID   STATE     PRIO  TIME    NAME\n");
        set_color(7,0);

        int32_t n = 0;
        if (kos_sys_table()->get_process_info) {
            n = kos_sys_table()->get_process_info(buffer, (int32_t)sizeof(buffer)-1);
            if (n < 0) n = 0; if (n >= (int32_t)sizeof(buffer)) n = (int32_t)sizeof(buffer)-1; buffer[n]=0;
        } else {
            addstr("(process info service unavailable)\n"); n=0; buffer[0]=0;
        }

        char* p = buffer;
        while (*p) {
            char* line = p; while (*p && *p!='\n' && *p!='\r') ++p; char saved=*p; *p=0;
            ProcInfo info; if (parse_line(line, &info)==5) {
                uint8_t fg = 7;
                if (strcmp(info.state, "RUNNING")==0) fg=10; else if (strcmp(info.state,"READY")==0) fg=11; else if (strcmp(info.state,"SLEEPING")==0) fg=14; else if (strcmp(info.state,"BLOCKED")==0) fg=12; else if (strcmp(info.state,"IDLE")==0) fg=8;
                set_color(fg,0);
                char pidbuf[16]; snprintf(pidbuf, sizeof(pidbuf), "%5u  ", (unsigned)info.pid); addstr(pidbuf);
                print_padded(info.state, 8);
                uint8_t prfg = (info.prio<=1?10:(info.prio<=3?14:12)); set_color(prfg,0);
                char priobuf[8]; snprintf(priobuf, sizeof(priobuf), "  %2u", (unsigned)info.prio); addstr(priobuf);
                set_color(fg,0);
                char timebuf[16]; snprintf(timebuf, sizeof(timebuf), "   %6u  ", (unsigned)info.time); addstr(timebuf);
                addstr(info.name); addch('\n'); set_color(7,0);
            }
            *p=saved; if (*p=='\n'||*p=='\r') ++p; if (*p=='\n'&&saved=='\r') ++p;
        }
        addstr("\nPress Ctrl+C to exit\n");
        // brief delay and Ctrl+C exit
        for (int slice=0;slice<50;++slice){ for(volatile int spin=0;spin<80000;++spin){} int8_t ch; if (kos_key_poll(&ch)&&ch==3){ addstr("^C\n"); return 0; } }
    }
    return 0;
}