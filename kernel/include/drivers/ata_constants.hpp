#pragma once
#include <common/types.hpp>
using namespace kos::common;

// ===============================================================
// ATA - CONTROL, STATUS AND COMMAND CONSTANTS
// ===============================================================
//
// This file groups together all constants related to the
// ATA/IDE protocol. It is designed to be used by the
// ATA driver (e.g., ATADriver) and other types of hardware.
//
// References:
//   - ATA/ATAPI-6 Specification
//   - OSDev Wiki: https://wiki.osdev.org/ATA_PIO_Mode
// ===============================================================


/**
 * @file ata_constants.hpp
 * @brief Constants and definitions for the ATA/IDE controller.
 *
 * Contains:
 *  - ATA ports
 *  - Control and status bits
 *  - ATA commands
 *  - Drive configuration
 *  - Timing parameters and sector sizes
 *  - Timeouts and safety limits
 *
 * @note These constants are used in ATADriver.cpp.
 */
namespace kos{
    namespace drivers{
        // -----------------------------
        // Alternative control ports
        // -----------------------------
        constexpr uint16_t ATA_PRIMARY_CONTROL_PORT = 0x3F6;
        constexpr uint16_t ATA_SECONDARY_CONTROL_PORT = 0x376;

        // -----------------------------
        // Bits control (Alternate Status / Device Control Register)
        // -----------------------------
        constexpr uint8_t ATA_CTRL_nIEN = 0x02; // Interrupt Disabled (nIEN=1)
        
        // -----------------------------
        // Status register bits (read-only)
        // -----------------------------
        constexpr uint8_t ATA_STATUS_BSY = 0x80; // Busy
        constexpr uint8_t ATA_STATUS_DRDY = 0x40; // Drive Ready
        constexpr uint8_t ATA_STATUS_DRQ = 0x08; // Data Request
        constexpr uint8_t ATA_STATUS_ERR = 0x01; // Error
        constexpr uint8_t ATA_STATUS_DF = 0x20; // Device Fault

        // -----------------------------
        // Commands ATA Register (write-only)
        // -----------------------------
        constexpr uint8_t ATA_CMD_IDENTIFY = 0xEC; // Identify Device
        constexpr uint8_t ATA_CMD_READ_SECTORS = 0x20; // Read Sectors (PIO)
        constexpr uint8_t ATA_CMD_WRITE_SECTORS = 0x30; // Write Sectors (PIO)
        constexpr uint8_t ATA_CMD_CACHE_FLUSH = 0xE7; // Flush Cache

        // -----------------------------
        // Configuration Drive/Head register bits
        // -----------------------------
        constexpr uint8_t ATA_DRIVE_LBA = 0xE0; // Bits 7:5 = 101, modo LBA enabled
        constexpr uint8_t ATA_DRIVE_SLAVE = 0x10;  // Bits 4 = 1 for Slave, 0 for Master
    
        // -----------------------------
        // Timing and transfer parameters
        // -----------------------------
        constexpr int32_t ATA_SECTOR_SIZE = 512; // Sector size in bytes
        constexpr int32_t ATA_WORDS_PER_SECTOR = 256; // Max words per sector
        constexpr int32_t ATA_DELAY_400NS_READS = 4; // 400ns delay via 4 reads

        // -----------------------------
        // Timeouts and safety limits
        // -----------------------------
        constexpr int32_t ATA_IDENTIFY_TIMEOUT = 10000; // Iterations for IDENTIFY timeout
        constexpr int32_t ATA_READY_TIMEOUT = 100000; // Limit for waiting device ready
        constexpr int32_t ATA_NO_DEVICE_FF_STREAK_LIMIT = 1000; // Number Read FF Streak Limit
        constexpr int32_t ATA_NO_DEVICE_CHECK_LIMIT = 1000; // Iterations for presence check
        constexpr int32_t ATA_NO_DEVICE_THRESHOLD = 900; // Dispositive Device Threshold
    }
}