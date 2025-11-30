// C++ LibC class implementation and C ABI wrappers
#include <lib/string.hpp>
#include <lib/libc/string.h>


using namespace kos::lib;

int32_t String::strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (int32_t)((unsigned char)*a) - (int32_t)((unsigned char)*b);
}



char* String::strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) ++d; // move to end of dest
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char* String::strstr(const char* haystack, const char* needle) {
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
    return 0;
}
int32_t String::strcmp(const uint8_t* a, const uint8_t* b){
    while(*a && (*a == *b)){
        a++;
        b++;
    }
    return (int32_t)(uint8_t)(*a) - (int32_t)(uint8_t)(*b);
}

int32_t String::strcmp(int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

int32_t String::strcmp(const int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

uint32_t String::strlen(const int8_t* s){
    uint32_t len = 0;
    while(*s++){
        len++;
    }
    return len;
}

void* String::memmove(void* dest, const void* src, uint32_t n) {
    // Implement overlap-safe move similar to C's memmove, using uint32_t length
    uint8_t*       d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d == s || n == 0) {
        return dest;
    }
    if (d < s) {
        for (uint32_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (uint32_t i = n; i != 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

void* String::memchr(const void* s, int c, uint32_t n) {
    const uint8_t* p = (const uint8_t*)s;
    uint8_t target = (uint8_t)(c & 0xFF);
    for (uint32_t i = 0; i < n; ++i) {
        if (p[i] == target) {
            return (void*)(p + i);
        }
    }
    return 0;
}

int32_t String::memcmp(const void* a, const void* b, uint32_t n) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    for (uint32_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) {
            return (int32_t)pa[i] - (int32_t)pb[i];
        }
    }
    return 0;
}

void* String::memset(void* s, int c, uint32_t n) {
    uint8_t* p = (uint8_t*)s;
    uint8_t v = (uint8_t)(c & 0xFF);
    for (uint32_t i = 0; i < n; ++i) {
        p[i] = v;
    }
    return s;
}



int8_t* String::strncpy(int8_t* dest, const int8_t* src, uint32_t n) {
    uint32_t i = 0;
    for (; i < n - 1 && src[i]; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return dest;
}