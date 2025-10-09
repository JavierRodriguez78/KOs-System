#ifndef __KOS__DRIVERS__BLOCKDEVICE_H
#define __KOS__DRIVERS__BLOCKDEVICE_H

#include <common/types.hpp>
#include <drivers/driver.hpp>

using namespace kos::common;

namespace kos {
    namespace drivers {

        // Abstract interface for block devices (disks)
        class BlockDevice : public Driver {
        public:
            virtual ~BlockDevice() {}

            // Read up to 255 sectors (512 bytes each) starting at LBA into buffer
            // Returns true on success
            virtual bool ReadSectors(uint32_t lba,
                             uint8_t sectorCount,
                             uint8_t* buffer) = 0;
            };

    } // namespace drivers
}

#endif
