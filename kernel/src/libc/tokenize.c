#include <lib/libc/tokenize.h>

int kos_split_args(int8_t* buf, const int8_t** argv, int maxArgs) {
    if (!buf || !argv || maxArgs <= 0) return 0;
    int argc = 0; int8_t* p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    while (*p && argc < maxArgs) {
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r') ++p;
        if (!*p) break;
        *p++ = 0;
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    }
    return argc;
}

int kos_build_prog_path(const int8_t* prog, int8_t* out, int outSize) {
    if (!prog || !out || outSize <= 0) return -1;
    int pi = 0;
    if (prog[0] == '/') {
        // copy absolute path
        while (prog[pi] && pi < outSize - 1) { out[pi] = prog[pi]; ++pi; }
        out[pi] = 0;
        return pi;
    }
    // prefix "/bin/"
    if (outSize < 6) return -1; // need at least "/bin/\0"
    out[pi++] = '/'; out[pi++] = 'b'; out[pi++] = 'i'; out[pi++] = 'n'; out[pi++] = '/';
    int i = 0;
    while (prog[i] && pi < outSize - 1) { out[pi++] = prog[i++]; }
    // append .elf if not present
    int needExt = 1;
    if (pi >= 4) {
        if (out[pi-4]=='.' && out[pi-3]=='e' && out[pi-2]=='l' && out[pi-1]=='f') needExt = 0;
    }
    if (needExt) {
        if (pi + 4 >= outSize) { out[(pi<outSize)?pi:outSize-1] = 0; return -1; }
        out[pi++] = '.'; out[pi++] = 'e'; out[pi++] = 'l'; out[pi++] = 'f';
    }
    out[pi] = 0;
    return pi;
}
