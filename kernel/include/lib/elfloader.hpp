#ifndef KOS_LIB_ELFLOADER_HPP
#define KOS_LIB_ELFLOADER_HPP

#include <common/types.hpp>
using namespace kos::common;

namespace kos { 
    namespace lib {

        class ELFLoader {
        public:
            // Load an ELF32 image from memory and execute entry.
            // Returns true if executed (and returned), false on parse/load error.
            static bool LoadAndExecute(const uint8_t* image, uint32_t size);
        };

    }
}

#endif