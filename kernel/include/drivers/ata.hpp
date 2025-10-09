#ifndef __KOS__DRIVERS__ATA_H
#define __KOS__DRIVERS__ATA_H

#include <common/types.hpp>
#include <drivers/blockdevice.hpp>
#include <hardware/port.hpp>

using namespace kos::common;
using namespace kos::drivers;
using namespace kos::hardware;

namespace kos { 
    namespace drivers {
        class ATADriver : public BlockDevice {
            public:
                enum Bus { Primary = 0x1F0, Secondary = 0x170 };
                enum Drive { Master = 0, Slave = 1 };

            ATADriver(Bus bus, Drive drive);
            ~ATADriver();

            virtual void Activate();
            // Issue IDENTIFY DEVICE (0xEC). Returns true if an ATA device responds.
            bool Identify();
            virtual bool ReadSectors(uint32_t lba,
                                    uint8_t sectorCount,
                                    uint8_t* buffer);

            private:
                Port16Bit dataPort;     // 0x1F0 data port (16-bit)
                Port8Bit errorPort;     // 0x1F1
                Port8Bit sectorCountPort; // 0x1F2
                Port8Bit lbaLowPort;      // 0x1F3
                Port8Bit lbaMidPort;      // 0x1F4
                Port8Bit lbaHighPort;     // 0x1F5
                Port8Bit driveHeadPort;   // 0x1F6
                Port8Bit commandPort;     // 0x1F7
                Port8Bit controlPort;     // 0x3F6

                uint16_t ioBase;
                Drive driveSel;

                void SelectDrive();
                bool WaitForReady();
        };
    }
}

#endif
