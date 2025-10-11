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

        /**
         *  @class ATADriver
         *  @brief Driver for ATA (IDE) disks in PIO mode (no DMA support yet)  
         *  Provides initialisation, disk identification, and sector reading function 
         *  using LBA28 mode.
         */
        class ATADriver : public BlockDevice {
            public:
                
                /**
                 * @enum Bus
                 * @brief Primary or Secondary ATA bus
                */
                enum Bus { Primary = 0x1F0, Secondary = 0x170 };
                
                /**
                 * @enum Drive
                 * @brief Master or Slave drive on the selected bus
                */
                enum Drive { Master = 0, Slave = 1 };

                /**
                 * @brief Contructor: initialises ports according to bus and drive
                 * @param bus ATA bus(Primary or Secondary)
                 * @param drive Drive to select (Master or Slave)
                */
                ATADriver(Bus bus, Drive drive);

                /**
                 * @brief Destructor: cleans up resources
                */
                ~ATADriver();

                /**  
                 * @brief Disables IDE interrupts (nIEN=1) and enables polling
                */
                virtual void Activate();
            
                
                /**
                 * @brief Sends the IDENTIFY SERVICE command to the disk.
                 * @return true if the device responds correctly, false otherwise.
                 * Issue IDENTIFY DEVICE (0xEC). Returns true if an ATA device responds.
                 */
                bool Identify();

                /**
                 * @brief Reads sectors from the disk in PIO mode (READ SECTORS 0x20).
                 * @param lba Initial LBA address.
                 * @param sectorCount Number of sectors to read (1-255).
                 * @param buffer Pointer to the buffer where data will be stored. Must be large enough to hold sectorCount * 512 bytes.
                 * @return true on success, false on failure.
                 * 
                 * This function reads up to 255 sectors (512 bytes each) starting at the specified LBA into the provided buffer.
                 * It returns true if the read operation is successful, and false otherwise.
                */
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
