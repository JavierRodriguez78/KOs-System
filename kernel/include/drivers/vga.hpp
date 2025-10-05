#ifndef __KOS__DRIVERS__VGA_H
#define __KOS__DRIVERS__VGA_H

#include <common/types.hpp>

namespace kos{
    namespace drivers{
        class VGA{

            private: 
                static kos::common::uint16_t* VideoMemory;
                static kos::common::uint8_t x,y;
            public:
                VGA();
                ~VGA();
                void Init();
                void Clear();
                void PutChar(kos::common::int8_t c);
                void Write(const kos::common::int8_t* str);
                void WriteHex(kos::common::uint8_t key);
        };

    }
}

#endif