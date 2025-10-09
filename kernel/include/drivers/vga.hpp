#ifndef __KOS__DRIVERS__VGA_H
#define __KOS__DRIVERS__VGA_H

#include <common/types.hpp>

using namespace kos::common;

namespace kos{
    namespace drivers{
        class VGA{

            private: 
                static uint16_t* VideoMemory;
                static uint8_t x,y;
            public:
                VGA();
                ~VGA();
                void Init();
                void Clear();
                void PutChar(int8_t c);
                void Write(const int8_t* str);
                void WriteHex(uint8_t key);
        };

    }
}

#endif