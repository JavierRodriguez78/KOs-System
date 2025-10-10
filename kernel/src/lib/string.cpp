// C++ LibC class implementation and C ABI wrappers
#include <lib/string.hpp>
#include <lib/libc/string.h>


using namespace kos::lib;



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