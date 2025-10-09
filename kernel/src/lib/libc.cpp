#include <lib/libc.hpp>

using namespace kos::lib;
using namespace kos::common;


int32_t LibC::strcmp(const uint8_t* a, const uint8_t* b){
    while(*a && (*a == *b)){
        a++;
        b++;
    }
    return (int32_t)(uint8_t)(*a) - (int32_t)(uint8_t)(*b);
}

int32_t LibC::strcmp(int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

int32_t LibC::strcmp(const int8_t* a, const int8_t* b, uint32_t len){
    for (uint32_t i= 0; i< len; i++){
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

uint32_t LibC::strlen(const int8_t* s){
    uint32_t len = 0;
    while(*s++){
        len++;
    }
    return len;
}