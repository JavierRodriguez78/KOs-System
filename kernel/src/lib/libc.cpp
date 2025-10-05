#include <lib/libc.hpp>

using namespace kos::lib;

LibC::LibC(){

};

LibC::~LibC(){

};

kos::common::int32_t LibC::strcmp(const kos::common::uint8_t* a, const kos::common::uint8_t* b){
    while(*a && (*a == *b)){
        a++;
        b++;
    }
    return (int)(const kos::common::uint8_t)*a - (int)(const kos::common::uint8_t)*b;
}

kos::common::int32_t LibC::strcmp(kos::common::int8_t* a, const kos::common::int8_t* b, kos::common::uint32_t len){
    for (kos::common::uint32_t i= 0; i< len; i++){
        kos::common::uint8_t ca = (kos::common::uint8_t)a[i];
        kos::common::uint8_t cb = (kos::common::uint8_t)b[i];
        if(ca != cb || ca== 0 || cb == 0){
            return ca-cb;
        }
    }
    return 0;
}

kos::common::uint32_t strlen(const kos::common::uint8_t* s){
    kos::common::uint32_t len = 0;
    while(*s++){
        len++;
    }
    return len;
}