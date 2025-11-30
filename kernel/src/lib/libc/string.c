// C-only implementations of minimal string/memory functions for applications.
#include <lib/libc/string.h>

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char*       d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char*       d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d == s || n == 0) {
        return dest;
    }
    if (d < s) {
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i != 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char target = (unsigned char)(c & 0xFF);
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == target) {
            return (void*)(p + i);
        }
    }
    return 0; // NULL
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char v = (unsigned char)(c & 0xFF);
    for (size_t i = 0; i < n; ++i) {
        p[i] = v;
    }
    return s;
}

int strcmp(const char* a, const char* b) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (*pa && (*pa == *pb)) {
        ++pa; ++pb;
    }
    return (int)(*pa) - (int)(*pb);
}

int strncmp(const char* a, const char* b, size_t len) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    for (size_t i = 0; i < len; ++i) {
        unsigned char ca = pa[i];
        unsigned char cb = pb[i];
        if (ca != cb || ca == 0 || cb == 0) {
            return (int)ca - (int)cb;
        }
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) { ++len; }
    return len;
}

char* strstr(const char* haystack, const char* needle) {
    // Per C standard: if needle is empty, return haystack
    if (!needle || *needle == '\0') {
        return (char*)haystack;
    }
    const char* h = haystack;
    const char* n = needle;
    for (; *h; ++h) {
        if (*h == *n) {
            const char* h2 = h;
            const char* n2 = n;
            while (*n2 && (*h2 == *n2)) { ++h2; ++n2; }
            if (*n2 == '\0') {
                return (char*)h;
            }
        }
    }
    return 0; // NULL when no match
}
