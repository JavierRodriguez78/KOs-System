#include <fs/fat16.hpp>
#include <console/tty.hpp>

using namespace kos::fs;
using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers;

static TTY tty;

FAT16::FAT16(BlockDevice* dev, uint32_t startLBA)
    : dev(dev), volumeStartLBA(startLBA), mounted(false) {
    bpb = {};
}

bool FAT16::ReadSector(uint32_t lba, uint8_t* buf) {
    return dev->ReadSectors(lba, 1, buf);
}

bool FAT16::Mount() {
    uint8_t sector[512];
    // If startLBA not provided, attempt to detect a FAT16 partition in MBR
    if (volumeStartLBA == 0) {
        uint8_t mbr[512];
        if (ReadSector(0, mbr) && mbr[510] == 0x55 && mbr[511] == 0xAA) {
            for (int32_t i = 0; i < 4; ++i) {
                int32_t off = 446 + i * 16;
                uint8_t type = mbr[off + 4];
                if (type == 0x04 || type == 0x06 || type == 0x0E || type == 0x01) {
                    uint32_t lba = (uint32_t)mbr[off + 8] | ((uint32_t)mbr[off + 9] << 8) | ((uint32_t)mbr[off + 10] << 16) | ((uint32_t)mbr[off + 11] << 24);
                    volumeStartLBA = lba;
                    break;
                }
            }
        }
    }
    if (!ReadSector(volumeStartLBA, sector)) {
        tty.Write("FAT16: Error read sector boot\n");
        return false;
    }
    if (!(sector[510] == 0x55 && sector[511] == 0xAA)) {
        tty.Write("FAT16: Invalid boot signature\n");
        return false;
    }
    // Parse BPB
    bpb.bytesPerSector    = sector[11] | (sector[12] << 8);
    bpb.sectorsPerCluster = sector[13];
    bpb.reservedSectors   = sector[14] | (sector[15] << 8);
    bpb.numFATs           = sector[16];
    bpb.rootEntryCount    = sector[17] | (sector[18] << 8);
    bpb.sectorsPerFAT     = sector[22] | (sector[23] << 8);

    if (bpb.bytesPerSector != 512 || bpb.sectorsPerCluster == 0 || bpb.numFATs == 0) {
        tty.Write("FAT16: Invalid BPB\n");
        return false;
    }

    fatStartLBA = volumeStartLBA + bpb.reservedSectors;
    uint32_t rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
    rootDirLBA = fatStartLBA + (bpb.numFATs * bpb.sectorsPerFAT);
    dataStartLBA = rootDirLBA + rootDirSectors;

    mounted = true;
    return true;
}

void FAT16::ListRoot() {
    
    if (!mounted) { 
        tty.Write("FAT16: not mounted\n"); 
        return; 
    }

    uint8_t sector[512];
    // Read some sectors from the root dir (up to 4 sectors for demo)
    for (int32_t s = 0; s < 4; ++s) {
        if (!ReadSector(rootDirLBA + s, sector)){
            tty.Write("FAT16: Error reading root dir sector\n");
            break;
        }
        for (int32_t i = 0; i < 512; i += 32) {
            uint8_t first = sector[i];
            if (first == 0x00) {
                return; // End
            }
            if (first == 0xE5) {
                 continue; // deleted
            }
            uint8_t attr = sector[i + 11];
            if (attr == 0x0F) {
                continue; // LFN
            }
            int8_t name[9]; 
            int8_t ext[4];
            int32_t n = 0; 
            for (int32_t j = 0; j < 8; ++j) { 
                char c = (char)sector[i + j]; if (c == ' ') break; 
                name[n++] = c; 
            }
            name[n] = 0;
            int32_t e = 0; 
            for (int32_t  j = 0; j < 3; ++j) { 
                int8_t c = (int8_t)sector[i + 8 + j]; 
                if (c == ' '){
                    break;
                }
                ext[e++] = c; 
            }
            ext[e] = 0;
            if (e > 0) { 
                tty.Write(name); 
                tty.Write("."); 
                tty.Write(ext); 
                tty.PutChar('\n'); 
            }else { 
                tty.Write(name); tty.PutChar('\n'); 
            }
        }
    }
}

void FAT16::DebugInfo() {
    if (!mounted) { 
        tty.Write("FAT16: not mounted\n"); 
        return; 
    }
    tty.Write("FAT16 Debug Info\n");
    tty.Write(" bytes/sector="); 
    tty.WriteHex((uint8_t)(bpb.bytesPerSector >> 8)); 
    tty.WriteHex((uint8_t)(bpb.bytesPerSector & 0xFF)); 
    tty.PutChar('\n');
    tty.Write(" sectors/cluster="); 
    tty.WriteHex(bpb.sectorsPerCluster); 
    tty.PutChar('\n');
    tty.Write(" reserved="); 
    tty.WriteHex((uint8_t)(bpb.reservedSectors >> 8)); 
    tty.WriteHex((uint8_t)(bpb.reservedSectors & 0xFF)); 
    tty.PutChar('\n');
    tty.Write(" numFATs="); 
    tty.WriteHex(bpb.numFATs); 
    tty.PutChar('\n');
    tty.Write(" root entries="); 
    tty.WriteHex((uint8_t)(bpb.rootEntryCount >> 8)); 
    tty.WriteHex((uint8_t)(bpb.rootEntryCount & 0xFF)); 
    tty.PutChar('\n');
}
