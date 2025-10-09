#ifndef __KOS__FS__FILESYSTEM_H
#define __KOS__FS__FILESYSTEM_H

#include <common/types.hpp>

namespace kos { 
    namespace fs {
        class Filesystem {
            public:
                virtual ~Filesystem() {}
                virtual bool Mount() = 0;
                virtual void ListRoot() = 0;
                virtual void DebugInfo() = 0;
        };      
    }
}

#endif
