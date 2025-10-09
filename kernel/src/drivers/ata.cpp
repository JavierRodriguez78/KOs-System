#include <drivers/ata.hpp>

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
      controlPort((uint16_t)((bus == Primary) ? 0x3F6 : 0x376))
{
    ioBase = (uint16_t)bus;
    driveSel = drive;
}

ATADriver::~ATADriver() {}

void ATADriver::Activate() {
    // Disable IDE IRQs (nIEN=1) since we use polling to avoid unhandled IRQ14 (0x2E)
    controlPort.Write(0x02);
}

void ATADriver::SelectDrive() {
    uint8_t driveSelValue = 0xE0; // LBA bit set, master
    if (driveSel == Slave){
        driveSelValue |= 0x10;
    } 
    driveHeadPort.Write(driveSelValue);
    // 400ns delay by reading alternate status
    for (int32_t i = 0; i < 4; ++i){
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
    for (int32_t i = 0; i < 100000; ++i) {
        uint8_t s = commandPort.Read();
        if (s == 0x00) return false; // no device
        if (s & 0x01) return false;  // ERR => not ATA
        if (!(s & 0x80) && (s & 0x08)) {
            // DRQ ready: read 256 words to drain IDENTIFY data
            for (int w = 0; w < 256; ++w) (void)dataPort.Read();
            return true;
        }
    }
    return false; // timeout
}

bool ATADriver::WaitForReady() {
    // Poll status with timeout: Bit7=BSY, Bit3=DRQ, Bit0=ERR, Bit5=DF
    uint8_t status;
    int32_t timeout = 1000000; // simple spin timeout
    int32_t ffStreak = 0;
    while (timeout-- > 0) {
        status = commandPort.Read();
        if (status == 0xFF) { // floating bus => no device
            if (++ffStreak > 1000) return false;
        } else {
            ffStreak = 0;
        }
        if (status & 0x01) return false; // ERR
        if (status & 0x20) return false; // DF
        if (!(status & 0x80) && (status & 0x08)) return true; // !BSY and DRQ
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
    for (int32_t i = 0; i < 4; ++i){
         (void)controlPort.Read(); // 400ns
    }

    // Quick presence check: if status reads 0xFF or 0x00 consistently, assume no device
    {
        int32_t tries = 1000; int32_t ffCount = 0; int32_t zCount = 0; bool anyOther = false;
        while (tries--) {
            uint8_t s = commandPort.Read();
            if (s == 0xFF) ffCount++; else if (s == 0x00) zCount++; else { anyOther = true; break; }
        }
        if (!anyOther && (ffCount > 900 || zCount > 900)) {
            return false;
        }
    }

    // Then set sector count and LBA low/mid/high
    sectorCountPort.Write(sectorCount);
    lbaLowPort.Write((uint8_t)(lba & 0xFF));
    lbaMidPort.Write((uint8_t)((lba >> 8) & 0xFF));
    lbaHighPort.Write((uint8_t)((lba >> 16) & 0xFF));
    commandPort.Write(0x20); // READ SECTORS (PIO) LBA28

    for (uint8_t s = 0; s < sectorCount; ++s) {
        if (!WaitForReady()){
            return false;
        } 
        // Each sector is 512 bytes = 256 words
        for (int32_t i = 0; i < 256; ++i) {
            uint16_t word = dataPort.Read();
            buffer[s*512 + i*2 + 0] = (uint8_t)(word & 0xFF);
            buffer[s*512 + i*2 + 1] = (uint8_t)((word >> 8) & 0xFF);
        }
    }
    return true;
}
