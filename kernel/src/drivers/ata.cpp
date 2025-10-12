#include <drivers/ata.hpp>
#include <drivers/ata_constants.hpp>

using namespace kos::drivers;
using namespace kos::hardware;
using namespace kos::common;

ATADriver::ATADriver(ATADriver::Bus bus, ATADriver::Drive drive)
    : dataPort((uint16_t)bus),
      errorPort((uint16_t)bus + 1),
      sectorCountPort((uint16_t)bus + 2),
      lbaLowPort((uint16_t)bus + 3),
      lbaMidPort((uint16_t)bus + 4),
      lbaHighPort((uint16_t)bus + 5),
      driveHeadPort((uint16_t)bus + 6),
      commandPort((uint16_t)bus + 7),
      controlPort((uint16_t)((bus == Primary) ? ATA_PRIMARY_CONTROL_PORT : ATA_SECONDARY_CONTROL_PORT))
{
    ioBase = (uint16_t)bus;
    driveSel = drive;
}

ATADriver::~ATADriver() {}

void ATADriver::Activate() {
    // Disable IDE IRQs (nIEN=1) since we use polling to avoid unhandled IRQ14 (0x2E)
    controlPort.Write(ATA_CTRL_nIEN);
}

void ATADriver::SelectDrive() {
    uint8_t driveSelValue = ATA_DRIVE_LBA; // LBA bit set, master
    if (driveSel == Slave){
        driveSelValue |= ATA_DRIVE_SLAVE ;
    } 
    driveHeadPort.Write(driveSelValue);
    // 400ns delay by reading alternate status
    for (int32_t i = 0; i < ATA_DELAY_400NS_READS; ++i){
        (void)controlPort.Read();
    } 
}
bool ATADriver::Identify() {
    // Select drive
    SelectDrive();
    // Clear LBA regs and sector count
    sectorCountPort.Write(0);
    lbaLowPort.Write(0);
    lbaMidPort.Write(0);
    lbaHighPort.Write(0);
    // Send IDENTIFY (0xEC)
    commandPort.Write(0xEC);

    // Poll for status. If status is 0, no device. If ERR set, not ATA (maybe ATAPI).
    for (int32_t i = 0; i < ATA_IDENTIFY_TIMEOUT; ++i) {
        uint8_t s = commandPort.Read();
        if (s == 0x00) return false; // no device
        if (s & ATA_STATUS_ERR) return false;  // ERR => not ATA
        if (!(s & ATA_STATUS_BSY) && (s & ATA_STATUS_DRQ)) {
            // DRQ ready: read 256 words to drain IDENTIFY data
            for (int w = 0; w < ATA_WORDS_PER_SECTOR; ++w) (void)dataPort.Read();
            return true;
        }
    }
    return false; // timeout
}

bool ATADriver::WaitForReady() {
    // Poll status with timeout: Bit7=BSY, Bit3=DRQ, Bit0=ERR, Bit5=DF
    uint8_t status;
    int32_t timeout = ATA_READY_TIMEOUT; // simple spin timeout
    int32_t ffStreak = 0;
    while (timeout-- > 0) {
        status = commandPort.Read();
        if (status == 0xFF) { // floating bus => no device
            if (++ffStreak > ATA_NO_DEVICE_FF_STREAK_LIMIT) return false;
        } else {
            ffStreak = 0;
        }
        if (status & ATA_STATUS_ERR) return false; // ERR
        if (status & ATA_STATUS_DF) return false; // DF
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) return true; // !BSY and DRQ
    }
    return false; // timed out
}

bool ATADriver::ReadSectors(uint32_t lba, uint8_t sectorCount, uint8_t* buffer) {
    if (sectorCount == 0){
        return true;
    } 

    SelectDrive();
    // Program drive/head first with high LBA bits and LBA mode
    uint8_t driveHead = 0xE0 | (driveSel == Slave ? 0x10 : 0) | ((lba >> 24) & 0x0F);
    driveHeadPort.Write(driveHead);

    for (int32_t i = 0; i < ATA_DELAY_400NS_READS; ++i){
         (void)controlPort.Read(); // 400ns
    }

    // Quick presence check: if status reads 0xFF or 0x00 consistently, assume no device
    {
        int32_t tries = ATA_NO_DEVICE_CHECK_LIMIT; int32_t ffCount = 0; int32_t zCount = 0; bool anyOther = false;
        while (tries--) {
            uint8_t s = commandPort.Read();
            if (s == 0xFF) ffCount++; else if (s == 0x00) zCount++; else { anyOther = true; break; }
        }
        if (!anyOther && (ffCount > ATA_NO_DEVICE_THRESHOLD || zCount > ATA_NO_DEVICE_THRESHOLD)) {
            return false;
        }
    }

    // Then set sector count and LBA low/mid/high
    sectorCountPort.Write(sectorCount);
    lbaLowPort.Write((uint8_t)(lba & 0xFF));
    lbaMidPort.Write((uint8_t)((lba >> 8) & 0xFF));
    lbaHighPort.Write((uint8_t)((lba >> 16) & 0xFF));
    
    // Send READ SECTORS (0x20)(LBA28)
    commandPort.Write(ATA_CMD_READ_SECTORS);
    
    // Read each sector
    for (uint8_t s = 0; s < sectorCount; ++s) {
        if (!WaitForReady()){
            return false;
        } 
        // Each sector is 512 bytes = 256 words
        for (int32_t i = 0; i < ATA_WORDS_PER_SECTOR; ++i) {
            uint16_t word = dataPort.Read();
            buffer[s*ATA_SECTOR_SIZE + i*2 + 0] = (uint8_t)(word & 0xFF);
            buffer[s*ATA_SECTOR_SIZE + i*2 + 1] = (uint8_t)((word >> 8) & 0xFF);
        }
    }
    return true;
}

bool ATADriver::WriteSectors(uint32_t lba, uint8_t sectorCount, const uint8_t* buffer) {
    if (sectorCount == 0) return true;

    SelectDrive();
    uint8_t driveHead = 0xE0 | (driveSel == Slave ? 0x10 : 0) | ((lba >> 24) & 0x0F);
    driveHeadPort.Write(driveHead);
    for (int32_t i = 0; i < ATA_DELAY_400NS_READS; ++i) { (void)controlPort.Read(); }

    // Presence check similar to read
    {
        int32_t tries = ATA_NO_DEVICE_CHECK_LIMIT; int32_t ffCount = 0; int32_t zCount = 0; bool anyOther = false;
        while (tries--) {
            uint8_t s = commandPort.Read();
            if (s == 0xFF) ffCount++; else if (s == 0x00) zCount++; else { anyOther = true; break; }
        }
        if (!anyOther && (ffCount > ATA_NO_DEVICE_THRESHOLD || zCount > ATA_NO_DEVICE_THRESHOLD)) {
            return false;
        }
    }

    sectorCountPort.Write(sectorCount);
    lbaLowPort.Write((uint8_t)(lba & 0xFF));
    lbaMidPort.Write((uint8_t)((lba >> 8) & 0xFF));
    lbaHighPort.Write((uint8_t)((lba >> 16) & 0xFF));

    // WRITE SECTORS (0x30)
    commandPort.Write(ATA_CMD_WRITE_SECTORS);

    for (uint8_t s = 0; s < sectorCount; ++s) {
        if (!WaitForReady()) return false;
        // Write 256 words per sector
        for (int32_t i = 0; i < ATA_WORDS_PER_SECTOR; ++i) {
            uint16_t word = (uint16_t)buffer[s*ATA_SECTOR_SIZE + i*2 + 0] |
                            (uint16_t)((uint16_t)buffer[s*ATA_SECTOR_SIZE + i*2 + 1] << 8);
            dataPort.Write(word);
        }
    }
    // After writing all sectors, wait for the device to complete the operation
    // Some devices require a flush for data to be committed reliably
    // Issue FLUSH CACHE (0xE7) and wait for !BSY (ignore if unsupported)
    commandPort.Write(0xE7);
    // Poll status: wait until BSY clears; error bits will cause failure
    {
        int32_t timeout = ATA_READY_TIMEOUT;
        while (timeout-- > 0) {
            uint8_t s = commandPort.Read();
            if (!(s & ATA_STATUS_BSY)) {
                if (s & (ATA_STATUS_ERR | ATA_STATUS_DF)) return false;
                break;
            }
        }
        if (timeout <= 0) return false;
    }
    return true;
}
