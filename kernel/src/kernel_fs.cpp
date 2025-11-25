#include <kernel/fs.hpp>
#include <drivers/ata/ata.hpp>
#include <fs/fat16.hpp>
#include <fs/fat32.hpp>
#include <console/logger.hpp>
#include <kernel/globals.hpp>

using namespace kos;
using namespace kos::drivers::ata;
using namespace kos::fs;
using namespace kos::drivers;

namespace kos { namespace kernel {

// Prepare ATA drivers for all 4 IDE positions and FAT instances for scanning
static ATADriver g_ata_p0m(ATADriver::Primary, ATADriver::Master);
static ATADriver g_ata_p0s(ATADriver::Primary, ATADriver::Slave);
static ATADriver g_ata_p1m(ATADriver::Secondary, ATADriver::Master);
static ATADriver g_ata_p1s(ATADriver::Secondary, ATADriver::Slave);

static FAT32 g_fs32_p0m(&g_ata_p0m);
static FAT32 g_fs32_p0s(&g_ata_p0s);
static FAT32 g_fs32_p1m(&g_ata_p1m);
static FAT32 g_fs32_p1s(&g_ata_p1s);

// Simple FAT16 instances with no pre-known startLBA
static FAT16 g_fs16_p0m(&g_ata_p0m);
static FAT16 g_fs16_p0s(&g_ata_p0s);
static FAT16 g_fs16_p1m(&g_ata_p1m);
static FAT16 g_fs16_p1s(&g_ata_p1s);

bool ScanAndMountFilesystems()
{
    struct Candidate { ATADriver* ata; FAT32* fs32; FAT16* fs16; const char* name; };
    Candidate cands[] = {
        { &g_ata_p0m, &g_fs32_p0m, &g_fs16_p0m, "Primary Master" },
        { &g_ata_p0s, &g_fs32_p0s, &g_fs16_p0s, "Primary Slave" },
        { &g_ata_p1m, &g_fs32_p1m, &g_fs16_p1m, "Secondary Master" },
        { &g_ata_p1s, &g_fs32_p1s, &g_fs16_p1s, "Secondary Slave" }
    };

    Logger::Log("Scanning ATA devices for filesystems");
    for (unsigned i = 0; i < sizeof(cands)/sizeof(cands[0]); ++i) {
        cands[i].ata->Activate();
        Logger::LogKV("Probe", cands[i].name);
        if (!cands[i].ata->Identify()) {
            Logger::LogKV("No ATA device detected at", cands[i].name);
            continue;
        }
        if (cands[i].fs32->Mount()) {
            Logger::LogKV("FAT32 mounted on", cands[i].name);
            Logger::LogStatus("Filesystem ready", true);
            g_fs_ptr = cands[i].fs32;
            return true;
        } else {
            Logger::LogKV("Not FAT32 at", cands[i].name);
            Logger::LogStatus("FAT32 mount failed", false);
            if (cands[i].fs16->Mount()) {
                Logger::LogKV("FAT16 mounted on", cands[i].name);
                Logger::LogStatus("Filesystem ready", true);
                g_fs_ptr = cands[i].fs16;
                return true;
            }
        }
    }
    Logger::Log("No filesystem mounted; continuing without disk");
    return false;
}

} }
