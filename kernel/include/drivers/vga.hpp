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
                static uint8_t attr; // text attribute (fg|bg)
            public:
                VGA();
                ~VGA();
                void Init();
                void Clear();
                void PutChar(int8_t c);
                void Write(const int8_t* str);
                void WriteHex(uint8_t key);
                // Color helpers
                void SetColor(uint8_t fg, uint8_t bg);
                void SetAttr(uint8_t a);
                static inline uint8_t MakeAttr(uint8_t fg, uint8_t bg) {
                    return ((bg & 0x0F) << 4) | (fg & 0x0F);
                }
        };

    }
}

#endif