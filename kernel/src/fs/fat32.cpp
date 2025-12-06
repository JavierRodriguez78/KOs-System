#include <fs/fat32.hpp>
#include <console/tty.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <lib/stdio.hpp>

using namespace kos::fs;
using namespace kos::drivers;
using namespace kos::common;
using namespace kos::console;
using namespace kos::lib;

static TTY tty;
namespace kos { namespace sys { uint32_t CurrentListFlags(); } }

// Debug helpers to print hex values >8 bits using TTY's 8-bit WriteHex
static void printHex16(uint16_t v) {
    tty.Write("0x");
    tty.WriteHex((uint8_t)((v >> 8) & 0xFF));
    tty.WriteHex((uint8_t)(v & 0xFF));
}

static void printHex32(uint32_t v) {
    tty.Write("0x");
    tty.WriteHex((uint8_t)((v >> 24) & 0xFF));
    tty.WriteHex((uint8_t)((v >> 16) & 0xFF));
    tty.WriteHex((uint8_t)((v >> 8) & 0xFF));
    tty.WriteHex((uint8_t)(v & 0xFF));
}

kos::fs::FAT32::FAT32(BlockDevice* dev)
    : dev(dev), volumeStartLBA(0), fatStartLBA(0), dataStartLBA(0), mountedFlag(false) {
    bpb = {};
}

kos::fs::FAT32::~FAT32() {}

bool FAT32::ReadSector(uint32_t lba, uint8_t* buf) {
    return dev->ReadSectors(lba, 1, buf);
}

bool FAT32::WriteSector(uint32_t lba, const uint8_t* buf) {
    return dev->WriteSectors(lba, 1, const_cast<uint8_t*>(buf));
}

bool FAT32::ReadSectors(uint32_t lba, uint32_t count, uint8_t* buf) {
    return dev->ReadSectors(lba, count, buf);
}

bool FAT32::WriteSectors(uint32_t lba, uint32_t count, const uint8_t* buf) {
    return dev->WriteSectors(lba, (uint8_t)count, const_cast<uint8_t*>(buf));
}

uint32_t FAT32::DetectFAT32PartitionStart() {
    uint8_t mbr[512];
    if (!ReadSector(0, mbr)) {
        tty.Write("FAT32: Error leyendo MBR (LBA 0)\n");
        return 0;
    }
    // Dump primeros bytes del MBR para depurar
    if (Logger::IsDebugEnabled()) {
        tty.Write("FAT32: Dump MBR[0..32) en LBA 0\n");
        for (int32_t i = 0; i < 32; ++i) { 
            tty.WriteHex(mbr[i]); 
            tty.PutChar(' ');
        }
        tty.PutChar('\n');
        tty.Write("FAT32: MBR firma (510,511): "); 
        tty.WriteHex(mbr[510]); 
        tty.PutChar(' '); 
        tty.WriteHex(mbr[511]); 
        tty.PutChar('\n');
    }
    bool bootSig = (mbr[510] == 0x55 && mbr[511] == 0xAA);
    bool hasPartitions = false;
    // MBR partition table at 446, four entries of 16 bytes each
    uint32_t fat32LBA = 0xFFFFFFFF; // marcador: MBR presente pero sin FAT32
    for (int32_t i = 0; i < 4; ++i) {
        int off = 446 + i * 16;
        uint8_t type = mbr[off + 4];
        if (type != 0x00) hasPartitions = true;
        uint32_t lbaStartDbg = (uint32_t)mbr[off + 8] |
                                    ((uint32_t)mbr[off + 9] << 8) |
                                    ((uint32_t)mbr[off + 10] << 16) |
                                    ((uint32_t)mbr[off + 11] << 24);
        if(Logger::IsDebugEnabled()){
            tty.Write("FAT32: Particion "); 
            tty.WriteHex((uint8_t)i); 
            tty.Write(": tipo="); 
            tty.WriteHex(type); 
            tty.Write(" startLBA=");
            printHex32(lbaStartDbg); 
            tty.PutChar('\n');
        }
        // Considerar particiones FAT clásicas: 0x01,0x04,0x06,0x0E, y FAT32 0x0B,0x0C
        if (type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0E || type == 0x0B || type == 0x0C) {
            uint32_t lbaStart = lbaStartDbg;
            // Leer el boot sector de la partición y comprobar si es FAT32
            uint8_t bs[512];
            if (!ReadSector(lbaStart, bs)) {
                tty.Write("FAT32: No se pudo leer boot sector de particion en LBA "); printHex32(lbaStart); tty.PutChar('\n');
                continue;
            }
            if (!(bs[510] == 0x55 && bs[511] == 0xAA)) {
                tty.Write("FAT32: Boot sector sin firma 0x55AA en LBA "); printHex32(lbaStart); tty.PutChar('\n');
                continue;
            }
            // FS type string para FAT32 en offset 0x52 (82)
            bool isFAT32 = (bs[82] == 'F' && bs[83] == 'A' && bs[84] == 'T' && bs[85] == '3' && bs[86] == '2');
            if (isFAT32) {
                tty.Write("FAT32: Partición FAT32 encontrada en LBA "); printHex32(lbaStart); tty.PutChar('\n');
                return lbaStart;
            } else {
                // No es FAT32 (posible FAT12/16); continuar buscando
                fat32LBA = 0xFFFFFFFF;
            }
        }
    }
    if (bootSig && hasPartitions) {
        if (Logger::IsDebugEnabled()) {
            tty.Write("FAT32: MBR presente pero sin particiones FAT32. (Se detectó p.ej. FAT16 tipo 0x06)\n");
        }
        return 0xFFFFFFFF; // no intentar superfloppy si hay MBR válido
    }
    tty.Write("FAT32: No hay MBR (o sin entradas), intentando superfloppy en LBA 0\n");
    return 0; // superfloppy
}

bool FAT32::Mount() {
    uint8_t sector[512];
    // Detect partition; if none, assume superfloppy (volume at LBA 0)
    volumeStartLBA = DetectFAT32PartitionStart();
    if (volumeStartLBA == 0xFFFFFFFF) {
        if (Logger::IsDebugEnabled()) {
            tty.Write("FAT32: Abortando montaje: no hay particion FAT32 y MBR existe\n");
        }
        return false;
    }
    if (!ReadSector(volumeStartLBA, sector)) {
        tty.Write("FAT32: Error leyendo BPB en volumen LBA ");
        printHex32(volumeStartLBA);
        tty.PutChar('\n');
        return false;
    }
    // Hexdump primeros 64 bytes del sector leído
    tty.Write("FAT32: Dump BPB[0..64) en LBA "); printHex32(volumeStartLBA); tty.PutChar('\n');
    for (int i = 0; i < 64; ++i) {
        tty.WriteHex(sector[i]);
        tty.PutChar(' ');
    }
    tty.PutChar('\n');

    // Comprobaciones básicas de Boot Sector
    uint8_t sigLo = sector[510];
    uint8_t sigHi = sector[511];
    tty.Write("FAT32: Firma boot sector (esperado 0x55 0xAA): ");
    tty.WriteHex(sigLo); tty.PutChar(' '); tty.WriteHex(sigHi); tty.PutChar('\n');
    if (!(sigLo == 0x55 && sigHi == 0xAA)) {
        tty.Write("FAT32: Firma del boot sector inválida. No es un volumen FAT válido en LBA ");
        printHex32(volumeStartLBA); tty.PutChar('\n');
        return false;
    }
    uint8_t j0 = sector[0];
    uint8_t j2 = sector[2];
    tty.Write("FAT32: Salto inicial: "); 
    tty.WriteHex(j0); 
    tty.PutChar(' '); 
    tty.WriteHex(j2); 
    tty.PutChar('\n');
    // Tipo FS string para FAT32 en offset 82 (0x52), 8 bytes
    tty.Write("FAT32: FS type string en 0x52: ");
    for (int32_t i = 0; i < 8; ++i) {
        int8_t c = (int8_t)sector[82 + i];
        if (c >= 32 && c <= 126) tty.PutChar(c); else tty.PutChar('.');
    }
    tty.PutChar('\n');
    // Si estamos en modo superfloppy y no es FAT32, abortar
    if (volumeStartLBA == 0) {
        bool isFAT32 = (sector[82] == 'F' && sector[83] == 'A' && sector[84] == 'T' && sector[85] == '3' && sector[86] == '2');
        if (!isFAT32) {
            tty.Write("FAT32: Superfloppy en LBA 0 no es FAT32. Abortando.\n");
            return false;
        }
    }

    // Parse BPB (offsets per FAT32 spec)
    bpb.bytesPerSector   = sector[11] | (sector[12] << 8);
    bpb.sectorsPerCluster= sector[13];
    bpb.reservedSectors  = sector[14] | (sector[15] << 8);
    bpb.numFATs          = sector[16];
    bpb.sectorsPerFAT    = (uint32_t)sector[36] | ((uint32_t)sector[37] << 8) | ((uint32_t)sector[38] << 16) | ((uint32_t)sector[39] << 24);
    bpb.rootCluster      = (uint32_t)sector[44] | ((uint32_t)sector[45] << 8) | ((uint32_t)sector[46] << 16) | ((uint32_t)sector[47] << 24);
    // Total sectors (FAT32: 32-bit at offset 32)
    bpb.totalSectors     = (uint32_t)sector[32] | ((uint32_t)sector[33] << 8) | ((uint32_t)sector[34] << 16) | ((uint32_t)sector[35] << 24);

    fatStartLBA = volumeStartLBA + bpb.reservedSectors;
    dataStartLBA = fatStartLBA + bpb.numFATs * bpb.sectorsPerFAT;

    // Debug BPB and layout
    tty.Write("FAT32: BPB bytes/sector="); 
    printHex16(bpb.bytesPerSector); 
    tty.PutChar('\n');
    tty.Write("FAT32: BPB sectores/cluster="); 
    tty.WriteHex(bpb.sectorsPerCluster); 
    tty.PutChar('\n');
    tty.Write("FAT32: BPB reservados="); 
    printHex16(bpb.reservedSectors); 
    tty.PutChar('\n');
    tty.Write("FAT32: BPB numFATs="); 
    tty.WriteHex(bpb.numFATs); 
    tty.PutChar('\n');
    tty.Write("FAT32: BPB sectoresPorFAT="); 
    printHex32(bpb.sectorsPerFAT); 
    tty.PutChar('\n');
    tty.Write("FAT32: BPB rootCluster="); 
    printHex32(bpb.rootCluster); 
    tty.PutChar('\n');
    tty.Write("FAT32: volumenLBA="); 
    printHex32(volumeStartLBA); 
    tty.PutChar('\n');
    tty.Write("FAT32: fatStartLBA="); 
    printHex32(fatStartLBA); 
    tty.PutChar('\n');
    tty.Write("FAT32: dataStartLBA="); 
    printHex32(dataStartLBA); 
    tty.PutChar('\n');

    if (bpb.bytesPerSector != 512 || bpb.sectorsPerCluster == 0) {
        tty.Write("FAT32: BPB inválido (bytes/sector != 512 o sectores/cluster == 0)\n");
        tty.Write("FAT32: bytes/sector="); 
        printHex16(bpb.bytesPerSector); 
        tty.Write(" spc="); 
        tty.WriteHex(bpb.sectorsPerCluster); 
        tty.PutChar('\n');
        tty.Write("FAT32: numFATs="); 
        tty.WriteHex(bpb.numFATs); 
        tty.Write(" reservados="); 
        printHex16(bpb.reservedSectors); 
        tty.PutChar('\n');
        return false;
    }
    mountedFlag = true;
    return true;
}

uint32_t FAT32::ClusterToLBA(uint32_t cluster) {
    return dataStartLBA + (cluster - 2) * bpb.sectorsPerCluster;
}

bool FAT32::ReadCluster(uint32_t cluster, uint8_t* buf) {
    return ReadSectors(ClusterToLBA(cluster), bpb.sectorsPerCluster, buf);
}

// Read next cluster from FAT (FAT32). Very minimal and unoptimized.
uint32_t FAT32::NextCluster(uint32_t cluster) {
    // FAT entries are 32-bit, but upper 4 bits reserved. Need to read from FAT region.
    // Compute FAT sector and offset
    uint32_t fatOffset = cluster * 4;
    uint32_t fatSector = fatStartLBA + (fatOffset / 512);
    uint32_t entOffset = fatOffset % 512;
    uint8_t sec[512];
    if (!ReadSector(fatSector, sec)) return 0x0FFFFFFF; // EOC on error
    uint32_t val = (uint32_t)sec[entOffset] |
                   ((uint32_t)sec[entOffset + 1] << 8) |
                   ((uint32_t)sec[entOffset + 2] << 16) |
                   ((uint32_t)sec[entOffset + 3] << 24);
    return val & 0x0FFFFFFF;
}

bool FAT32::UpdateFAT(uint32_t cluster, uint32_t value) {
    // Update a single FAT entry for FAT32 (lower 28 bits used)
    uint32_t fatOffset = cluster * 4;
    uint32_t fatSector = fatStartLBA + (fatOffset / 512);
    uint32_t entOffset = fatOffset % 512;
    uint8_t sec[512];
    if (!ReadSector(fatSector, sec)) return false;
    uint32_t cur = (uint32_t)sec[entOffset] |
                   ((uint32_t)sec[entOffset + 1] << 8) |
                   ((uint32_t)sec[entOffset + 2] << 16) |
                   ((uint32_t)sec[entOffset + 3] << 24);
    cur &= 0xF0000000; // keep upper 4 bits
    cur |= (value & 0x0FFFFFFF);
    sec[entOffset]     = (uint8_t)(cur & 0xFF);
    sec[entOffset + 1] = (uint8_t)((cur >> 8) & 0xFF);
    sec[entOffset + 2] = (uint8_t)((cur >> 16) & 0xFF);
    sec[entOffset + 3] = (uint8_t)((cur >> 24) & 0xFF);
    if (!WriteSector(fatSector, sec)) return false;
    // Mirror to additional FATs if present
    for (uint8_t f = 1; f < bpb.numFATs; ++f) {
        uint32_t mirrorLBA = fatSector + f * bpb.sectorsPerFAT;
        if (!WriteSector(mirrorLBA, sec)) return false;
    }
    return true;
}

bool FAT32::WriteCluster(uint32_t cluster, const uint8_t* buf) {
    return WriteSectors(ClusterToLBA(cluster), bpb.sectorsPerCluster, buf);
}

uint32_t FAT32::AllocateCluster() {
    // Scan FAT entries directly based on FAT size (more robust than deriving totalClusters)
    uint32_t entries = bpb.sectorsPerFAT * 512 / 4; // 4 bytes per FAT32 entry
    if (entries < 3) return 0; // not valid
    uint8_t sec[512];
    for (uint32_t cl = 2; cl < entries; ++cl) {
        uint32_t fatOffset = cl * 4;
        uint32_t fatSector = fatStartLBA + (fatOffset / 512);
        uint32_t entOffset = fatOffset % 512;
        if (!ReadSector(fatSector, sec)) return 0;
        uint32_t val = (uint32_t)sec[entOffset] |
                       ((uint32_t)sec[entOffset + 1] << 8) |
                       ((uint32_t)sec[entOffset + 2] << 16) |
                       ((uint32_t)sec[entOffset + 3] << 24);
        val &= 0x0FFFFFFF;
        if (val == 0x00000000) {
            // Mark allocated with EOC
            if (UpdateFAT(cl, 0x0FFFFFF8)) return cl;
            return 0;
        }
    }
    return 0;
}

void FAT32::PackShortName11(const int8_t* name83, uint8_t out11[11], bool& okIs83, bool upperOnly) {
    // Convert NAME[.EXT] into padded 8 + 3 upper-case, truncating when needed.
    for (int i = 0; i < 11; ++i) out11[i] = ' ';
    okIs83 = true;
    int n = 0; int e = -1;
    for (int i = 0; name83 && name83[i]; ++i) {
        if (name83[i] == '.') { if (e >= 0) { okIs83 = false; break; } e = 0; continue; }
        int8_t c = name83[i];
        if (c >= 'a' && c <= 'z' && upperOnly) c = c - ('a' - 'A');
        // Basic allowed set; if not allowed, fail
        bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c=='_' || c=='$' || c=='~' || c=='!';
        if (!ok) { okIs83 = false; break; }
        if (e < 0) {
            if (n < 8) out11[n++] = (uint8_t)c; // truncate beyond 8
        } else {
            if (e < 3) out11[8 + (e++)] = (uint8_t)c; // truncate beyond 3
        }
    }
}

bool FAT32::InitDirCluster(uint32_t newDirCluster, uint32_t parentCluster) {
    uint8_t cl[4096]; // assume max cluster size of 8*512 for simplicity; will only write first sectors actually used by bpb.sectorsPerCluster
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    for (uint32_t i = 0; i < bytesPerCluster; ++i) cl[i] = 0;
    // Create "." and ".." entries (short entries)
    auto writeEntry = [&](uint32_t off, const char* name, uint32_t startCl, bool isDir) {
        // Fill short name: 11 bytes (space-padded). For '.' and '..', just the dots at beginning.
        for (int i = 0; i < 11; ++i) cl[off + i] = ' ';
        // name expected to be "." or ".."
        cl[off + 0] = name[0];
        if (name[1]) cl[off + 1] = name[1];
        cl[off + 11] = isDir ? 0x10 : 0x20; // ATTR; 0x10 = directory
        cl[off + 26] = (uint8_t)(startCl & 0xFF);
        cl[off + 27] = (uint8_t)((startCl >> 8) & 0xFF);
        cl[off + 20] = (uint8_t)((startCl >> 16) & 0xFF);
        cl[off + 21] = (uint8_t)((startCl >> 24) & 0xFF);
        // size=0 for dirs
        cl[off + 28] = cl[off + 29] = cl[off + 30] = cl[off + 31] = 0;
    };
    writeEntry(0, ".", newDirCluster, true);
    writeEntry(32, "..", parentCluster, true);
    return WriteCluster(newDirCluster, cl);
}

bool FAT32::FindShortEntryWithOffset(uint32_t dirCluster, const uint8_t shortName11[11], uint32_t& outClusterContaining, uint32_t& outEntryOffset, uint32_t& outStartCluster, uint32_t& outFileSize, uint8_t& outAttr) {
    outClusterContaining = 0; outEntryOffset = 0; outStartCluster = 0; outFileSize = 0; outAttr = 0;
    uint8_t* clBuf = (uint8_t*)0x36000; // scratch
    uint32_t cluster = dirCluster;
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        if (!ReadCluster(cluster, clBuf)) return false;
        for (uint32_t off = 0; off < bytesPerCluster; off += 32) {
            uint8_t fb = clBuf[off]; if (fb == 0x00) break; if (fb == 0xE5) continue; uint8_t attr = clBuf[off + 11]; if (attr == 0x0F) continue;
            bool match = true;
            for (int i = 0; i < 11; ++i) { if (clBuf[off + i] != shortName11[i]) { match = false; break; } }
            if (match) {
                outClusterContaining = cluster;
                outEntryOffset = off;
                outAttr = attr;
                uint16_t lo = (uint16_t)clBuf[off + 26] | ((uint16_t)clBuf[off + 27] << 8);
                uint16_t hi = (uint16_t)clBuf[off + 20] | ((uint16_t)clBuf[off + 21] << 8);
                outStartCluster = ((uint32_t)hi << 16) | lo;
                outFileSize = (uint32_t)clBuf[off + 28] | ((uint32_t)clBuf[off + 29] << 8) | ((uint32_t)clBuf[off + 30] << 16) | ((uint32_t)clBuf[off + 31] << 24);
                return true;
            }
        }
        uint32_t next = NextCluster(cluster);
        if (next >= 0x0FFFFFF8 || next == 0) break;
        cluster = next;
    }
    return false;
}

bool FAT32::AddFileEntryToDir(uint32_t dirCluster, const uint8_t shortName11[11], uint32_t startCluster, uint32_t initialSize) {
    // Reuse directory writing logic similar to AddEntryToDirCluster (simplified for files)
    uint8_t* cl = (uint8_t*)0x37000; // scratch
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    uint32_t cur = dirCluster;
    while (true) {
        if (!ReadCluster(cur, cl)) return false;
        uint32_t off = 0;
        for (; off < bytesPerCluster; off += 32) {
            uint8_t fb = cl[off]; if (fb == 0x00 || fb == 0xE5) break; }
        if (off < bytesPerCluster) {
            for (int i = 0; i < 11; ++i) cl[off + i] = shortName11[i];
            cl[off + 11] = 0x20; // ATTR: regular file
            cl[off + 26] = (uint8_t)(startCluster & 0xFF);
            cl[off + 27] = (uint8_t)((startCluster >> 8) & 0xFF);
            cl[off + 20] = (uint8_t)((startCluster >> 16) & 0xFF);
            cl[off + 21] = (uint8_t)((startCluster >> 24) & 0xFF);
            uint32_t sz = initialSize;
            cl[off + 28] = (uint8_t)(sz & 0xFF);
            cl[off + 29] = (uint8_t)((sz >> 8) & 0xFF);
            cl[off + 30] = (uint8_t)((sz >> 16) & 0xFF);
            cl[off + 31] = (uint8_t)((sz >> 24) & 0xFF);
            return WriteCluster(cur, cl);
        }
        // Extend directory if needed
        uint32_t nxt = NextCluster(cur);
        if (nxt >= 0x0FFFFFF8 || nxt == 0) {
            uint32_t newCl = AllocateCluster(); if (newCl < 2) return false;
            if (!UpdateFAT(cur, newCl)) return false;
            if (!UpdateFAT(newCl, 0x0FFFFFF8)) return false;
            uint8_t zero[512]; for (int i=0;i<512;++i) zero[i]=0;
            for (uint8_t s=0; s < bpb.sectorsPerCluster; ++s) { if (!WriteSector(ClusterToLBA(newCl) + s, zero)) return false; }
            cur = newCl;
        } else {
            cur = nxt;
        }
    }
}

int32_t FAT32::WriteFile(const int8_t* path, const uint8_t* data, uint32_t len) {
    if (!mountedFlag || !path || !data || len == 0) return -1;
    if (path[0] != '/') return -1;
    // Traverse directories to the parent directory of target file
    uint32_t dirCl = bpb.rootCluster;
    const int8_t* p = path + 1; // skip leading '/'
    int8_t comp[13];
    int8_t finalName[13]; finalName[0]=0;
    while (*p) {
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++; comp[ci]=0;
        if (*p == '/') ++p; if (ci == 0) continue;
        // Peek ahead to see if this is last component
        const int8_t* q = p; while (*q == '/') ++q; bool isLast = (*q == 0);
        // Uppercase for 8.3
        for (int i=0; comp[i]; ++i) if (comp[i]>='a'&&comp[i]<='z') comp[i] -= ('a'-'A');
        if (isLast) {
            // This is file name
            int fi=0; while (comp[fi] && fi < 12) { finalName[fi]=comp[fi]; ++fi; } finalName[fi]=0; break;
        }
        uint32_t childCl=0, childSz=0; if (!FindShortNameInDirCluster(dirCl, comp, childCl, childSz)) return -1; // parent dir missing
        dirCl = childCl;
    }
    if (finalName[0]==0) return -1; // no file name
    // Build short 11-byte name (upper-case); allow truncation.
    uint8_t short11[11]; bool ok83=false; PackShortName11(finalName, short11, ok83, true); if(!ok83) return -1;
    uint32_t entryCl=0, entryOff=0, startCl=0, fileSize=0; uint8_t attr=0;
    bool exists = FindShortEntryWithOffset(dirCl, short11, entryCl, entryOff, startCl, fileSize, attr);
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    if (!exists) {
        // Create new file: allocate first cluster
        uint32_t newCl = AllocateCluster(); if (newCl < 2) return -1;
        // Zero-init clusters needed
        uint8_t zero[512]; for (int i=0;i<512;++i) zero[i]=0;
        for (uint8_t s=0; s < bpb.sectorsPerCluster; ++s) { if (!WriteSector(ClusterToLBA(newCl) + s, zero)) return -1; }
        if (!UpdateFAT(newCl, 0x0FFFFFF8)) return -1; // mark EOC
        // Write initial data across cluster chain (allocate more clusters if needed)
        uint32_t remaining = len; const uint8_t* src = data; uint32_t current = newCl;
        while (remaining) {
            uint8_t* buf = (uint8_t*)0x38000; // scratch
            // Zero fresh buffer
            for (uint32_t i=0;i<bytesPerCluster;++i) buf[i]=0;
            uint32_t chunk = (remaining > bytesPerCluster) ? bytesPerCluster : remaining;
            for (uint32_t i=0;i<chunk;++i) buf[i] = src[i];
            if (!WriteCluster(current, buf)) return -1;
            src += chunk; remaining -= chunk;
            if (remaining) {
                uint32_t nextCl = AllocateCluster(); if (nextCl < 2) return -1;
                if (!UpdateFAT(current, nextCl)) return -1;
                if (!UpdateFAT(nextCl, 0x0FFFFFF8)) return -1;
                // Zero new cluster sectors
                for (uint8_t s=0; s < bpb.sectorsPerCluster; ++s) { if (!WriteSector(ClusterToLBA(nextCl) + s, zero)) return -1; }
                current = nextCl;
            }
        }
        if (!AddFileEntryToDir(dirCl, short11, newCl, len)) return -1;
        return (int32_t)len;
    }
    // Append to existing file
    if (!(attr & 0x20)) return -1; // not a regular file
    uint32_t lastCluster = startCl;
    // Walk to end of cluster chain
    while (true) {
        uint32_t nxt = NextCluster(lastCluster);
        if (nxt >= 0x0FFFFFF8 || nxt == 0) break;
        lastCluster = nxt;
    }
    uint32_t offsetInLast = fileSize % bytesPerCluster;
    uint32_t remaining = len; const uint8_t* src = data;
    // If last cluster has space, read-modify-write
    if (offsetInLast < bytesPerCluster && (fileSize > 0 || offsetInLast != 0)) {
        uint8_t* buf = (uint8_t*)0x39000;
        if (!ReadCluster(lastCluster, buf)) return -1;
        uint32_t space = bytesPerCluster - offsetInLast;
        uint32_t chunk = (remaining > space) ? space : remaining;
        for (uint32_t i=0;i<chunk;++i) buf[offsetInLast + i] = src[i];
        if (!WriteCluster(lastCluster, buf)) return -1;
        src += chunk; remaining -= chunk; fileSize += chunk;
        offsetInLast += chunk;
    }
    // Allocate new clusters if more data remains
    while (remaining) {
        uint32_t newCl = AllocateCluster(); if (newCl < 2) return -1;
        if (!UpdateFAT(lastCluster, newCl)) return -1;
        if (!UpdateFAT(newCl, 0x0FFFFFF8)) return -1;
        uint8_t* buf = (uint8_t*)0x3A000;
        // Zero-init
        for (uint32_t i=0;i<bytesPerCluster;++i) buf[i]=0;
        uint32_t chunk = (remaining > bytesPerCluster) ? bytesPerCluster : remaining;
        for (uint32_t i=0;i<chunk;++i) buf[i] = src[i];
        if (!WriteCluster(newCl, buf)) return -1;
        src += chunk; remaining -= chunk; fileSize += chunk;
        lastCluster = newCl;
    }
    // Update directory entry size and (low/high) cluster fields if they were zero (should not change start cluster for append)
    uint8_t* dirBuf = (uint8_t*)0x3B000;
    if (!ReadCluster(entryCl, dirBuf)) return -1;
    dirBuf[entryOff + 28] = (uint8_t)(fileSize & 0xFF);
    dirBuf[entryOff + 29] = (uint8_t)((fileSize >> 8) & 0xFF);
    dirBuf[entryOff + 30] = (uint8_t)((fileSize >> 16) & 0xFF);
    dirBuf[entryOff + 31] = (uint8_t)((fileSize >> 24) & 0xFF);
    if (!WriteCluster(entryCl, dirBuf)) return -1;
    return (int32_t)len;
}

bool FAT32::AddEntryToDirCluster(uint32_t dirCluster, const uint8_t shortName11[11], uint32_t startCluster, bool isDir) {
    uint8_t* cl = (uint8_t*)0x40000; // scratch
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    uint32_t cur = dirCluster;
    while (true) {
        if (!ReadCluster(cur, cl)) return false;
        // Find free entry (0x00 or 0xE5)
        uint32_t off = 0;
        for (; off < bytesPerCluster; off += 32) {
            uint8_t fb = cl[off];
            if (fb == 0x00 || fb == 0xE5) break;
        }
        if (off < bytesPerCluster) {
            // Write short 8.3 entry here
            for (int i = 0; i < 11; ++i) cl[off + i] = shortName11[i];
            cl[off + 11] = isDir ? 0x10 : 0x20; // ATTR
            cl[off + 26] = (uint8_t)(startCluster & 0xFF);
            cl[off + 27] = (uint8_t)((startCluster >> 8) & 0xFF);
            cl[off + 20] = (uint8_t)((startCluster >> 16) & 0xFF);
            cl[off + 21] = (uint8_t)((startCluster >> 24) & 0xFF);
            cl[off + 28] = cl[off + 29] = cl[off + 30] = cl[off + 31] = 0; // size
            return WriteCluster(cur, cl);
        }
        // No free slot in this cluster; move to next or extend chain
        uint32_t nxt = NextCluster(cur);
        if (nxt >= 0x0FFFFFF8 || nxt == 0) {
            // Allocate a new cluster for the directory and link it
            uint32_t newCl = AllocateCluster();
            if (newCl < 2) return false;
            if (!UpdateFAT(cur, newCl)) return false;
            if (!UpdateFAT(newCl, 0x0FFFFFF8)) return false; // EOC
            // Zero-initialize the new cluster
            uint8_t zero[512]; for (int i=0;i<512;++i) zero[i]=0;
            for (uint8_t s = 0; s < bpb.sectorsPerCluster; ++s) {
                if (!WriteSector(ClusterToLBA(newCl) + s, zero)) return false;
            }
            cur = newCl; // and loop to write entry there
        } else {
            cur = nxt;
        }
    }
}

void FAT32::ListRoot() {
    if (bpb.bytesPerSector == 0) {
        tty.Write("FAT32 not mounted\n");
        return;
    }
    uint32_t flags = kos::sys::CurrentListFlags();

    uint8_t sector[512];
    uint32_t lba = ClusterToLBA(bpb.rootCluster);
    if (!ReadSector(lba, sector)) {
        tty.Write("FAT32: Error leyendo clúster raíz en LBA ");
        printHex32(lba);
        tty.PutChar('\n');
        return;
    }

    // Iterate directory entries (32 bytes each)
    int col = 0; const int colWidth = 16; const int colsPerRow = 4;
    for (int32_t i = 0; i < 512; i += 32) {
        uint8_t firstByte = sector[i];
        if (firstByte == 0x00) break; // no more entries
        if (firstByte == 0xE5) continue; // deleted
        uint8_t attr = sector[i + 11];
        if (attr == 0x0F) continue; // long name entry

        // Short name 8.3
        int8_t name[13];
        int32_t idx = 0;
        for (int32_t j = 0; j < 8; ++j) {
            int8_t c = (int8_t)sector[i + j];
            if (c == ' ') break;
            name[idx++] = c;
        }
        // Extension
        int8_t ext[4];
        int32_t e = 0;
        for (int32_t j = 0; j < 3; ++j) {
            int8_t c = (int8_t)sector[i + 8 + j];
            if (c == ' ') break;
            ext[e++] = c;
        }
        name[idx] = 0;
    // Filter . and .. unless -a
    if (!(flags & (1u<<1))) { if (e == 0 && ((idx == 1 && name[0] == '.') || (idx == 2 && name[0] == '.' && name[1] == '.'))) continue; }
        // Build display string
        int8_t display[20]; int di = 0;
        for (int k = 0; k < idx && di < (int)sizeof(display) - 1; ++k) display[di++] = name[k];
        if (!(attr & 0x10) && e > 0) {
            if (di < (int)sizeof(display) - 1) display[di++] = '.';
            for (int k = 0; k < e && di < (int)sizeof(display) - 1; ++k) display[di++] = ext[k];
        }
        if (attr & 0x10) { if (di < (int)sizeof(display) - 1) display[di++] = '/'; }
        display[di] = 0;
        if (flags & 1u) {
            // long listing: show type, name, size, timestamp (YYYY-MM-DD HH:MM) in decimal
            uint32_t size = (uint32_t)sector[i + 28] | ((uint32_t)sector[i + 29] << 8) | ((uint32_t)sector[i + 30] << 16) | ((uint32_t)sector[i + 31] << 24);
            uint16_t time = (uint16_t)sector[i + 22] | ((uint16_t)sector[i + 23] << 8);
            uint16_t date = (uint16_t)sector[i + 24] | ((uint16_t)sector[i + 25] << 8);
            uint32_t year = 1980 + ((date >> 9) & 0x7F);
            uint32_t month = (date >> 5) & 0x0F;
            uint32_t day = date & 0x1F;
            uint32_t hour = (time >> 11) & 0x1F;
            uint32_t min = (time >> 5) & 0x3F;
            tty.Write((const int8_t*)((attr & 0x10) ? "d " : "- "));
            tty.Write((const int8_t*)" "); tty.Write(display);
            tty.Write((const int8_t*)"  ");
            kos::sys::printf((const int8_t*)"%10u  %04u-%02u-%02u %02u:%02u\n", size, year, month, day, hour, min);
        } else {
            int len = di;
            if (attr & 0x10) TTY::SetColor(10, 0);
            tty.Write(display);
            if (attr & 0x10) TTY::SetAttr(0x07);
            for (int s = len; s < colWidth; ++s) tty.PutChar(' ');
            if (++col >= colsPerRow) { tty.PutChar('\n'); col = 0; }
        }
    }
    if (col > 0 && !(flags & 1u)) tty.PutChar('\n');
}

void FAT32::ListDir(const int8_t* path) {
    if (!mountedFlag) { tty.Write((const int8_t*)"FAT32 not mounted\n"); return; }
    if (!path || (path[0] == '/' && path[1] == 0)) { ListRoot(); return; }
    uint32_t flags = kos::sys::CurrentListFlags();
    // Traverse path components from root cluster
    uint32_t dirCl = bpb.rootCluster;
    const int8_t* p = (path[0] == '/') ? (path + 1) : path;
    int8_t comp[13];
    while (*p) {
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0; if (*p == '/') ++p; if (ci == 0) continue;
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a' - 'A');
        uint32_t childCl = 0, childSz = 0; if (!FindShortNameInDirCluster(dirCl, comp, childCl, childSz)) {
            // Print the full path provided to help debugging instead of only the failing component
            tty.Write((const int8_t*)"ls: path not found: ");
            tty.Write(path ? path : (const int8_t*)"(null)");
            tty.PutChar('\n');
            return;
        }
        dirCl = childCl;
    }
    uint8_t* clusterBuf = (uint8_t*)0x22000;
    if (!ReadCluster(dirCl, clusterBuf)) { tty.Write((const int8_t*)"ls: read dir failed\n"); return; }
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    int col = 0; const int colWidth = 16; const int colsPerRow = 4;
    for (uint32_t i = 0; i < bytesPerCluster; i += 32) {
        uint8_t fb = clusterBuf[i]; if (fb == 0x00) break; if (fb == 0xE5) continue; uint8_t attr = clusterBuf[i+11]; if (attr == 0x0F) continue;
        int8_t name[13]; int ni=0; for (int j=0;j<8;++j){ int8_t c=(int8_t)clusterBuf[i+j]; if(c==' ') break; name[ni++]=c; } name[ni]=0;
        int8_t ext[4]; int ei=0; for (int j=0;j<3;++j){ int8_t c=(int8_t)clusterBuf[i+8+j]; if(c==' ') break; ext[ei++]=c; } ext[ei]=0;
        if (!(flags & (1u<<1))) { if (ei==0 && ((ni==1 && name[0]=='.') || (ni==2 && name[0]=='.' && name[1]=='.'))) continue; }
        // Build display
        int8_t display[20]; int di=0;
        for (int k=0; k<ni && di < (int)sizeof(display)-1; ++k) display[di++]=name[k];
        if (!(attr & 0x10) && ei>0) { if (di < (int)sizeof(display)-1) display[di++]='.'; for (int k=0;k<ei && di < (int)sizeof(display)-1; ++k) display[di++]=ext[k]; }
        if (attr & 0x10) { if (di < (int)sizeof(display)-1) display[di++] = '/'; }
        display[di]=0;
        if (flags & 1u) {
            uint32_t size = (uint32_t)clusterBuf[i + 28] | ((uint32_t)clusterBuf[i + 29] << 8) | ((uint32_t)clusterBuf[i + 30] << 16) | ((uint32_t)clusterBuf[i + 31] << 24);
            uint16_t time = (uint16_t)clusterBuf[i + 22] | ((uint16_t)clusterBuf[i + 23] << 8);
            uint16_t date = (uint16_t)clusterBuf[i + 24] | ((uint16_t)clusterBuf[i + 25] << 8);
            uint32_t year = 1980 + ((date >> 9) & 0x7F);
            uint32_t month = (date >> 5) & 0x0F;
            uint32_t day = date & 0x1F;
            uint32_t hour = (time >> 11) & 0x1F;
            uint32_t min = (time >> 5) & 0x3F;
            tty.Write((const int8_t*)((attr & 0x10) ? "d " : "- "));
            tty.Write((const int8_t*)" "); tty.Write(display);
            tty.Write((const int8_t*)"  ");
            kos::sys::printf((const int8_t*)"%10u  %04u-%02u-%02u %02u:%02u\n", size, year, month, day, hour, min);
        } else {
            int len = di;
            if (attr & 0x10) TTY::SetColor(10,0);
            tty.Write(display);
            if (attr & 0x10) TTY::SetAttr(0x07);
            for (int s=len; s<colWidth; ++s) tty.PutChar(' ');
            if (++col >= colsPerRow) { tty.PutChar('\n'); col=0; }
        }
    }
    if (col > 0 && !(flags & 1u)) tty.PutChar('\n');
}

bool FAT32::DirExists(const int8_t* path) {
    if (!mountedFlag) return false;
    if (!path) return false;
    if (path[0] == '/' && path[1] == 0) return true; // root always exists
    // Traverse path components from root cluster
    uint32_t dirCl = bpb.rootCluster;
    const int8_t* p = (path[0] == '/') ? (path + 1) : path;
    int8_t comp[13];
    while (*p) {
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0; if (*p == '/') ++p; if (ci == 0) continue;
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a' - 'A');
        uint32_t childCl = 0, childSz = 0; 
        if (!FindShortNameInDirCluster(dirCl, comp, childCl, childSz)) return false; // component not found
        dirCl = childCl; // descend
    }
    return true;
}

// Find short 8.3 name in a single directory cluster (no subdir traversal beyond one hop)
bool FAT32::FindShortNameInDirCluster(uint32_t dirCluster, const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize) {
    uint8_t* clusterBuf = (uint8_t*)0x20000; // scratch area
    uint32_t cl = dirCluster;
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        if (!ReadCluster(cl, clusterBuf)) return false;
        for (uint32_t i = 0; i < bpb.sectorsPerCluster * 512; i += 32) {
            uint8_t firstByte = clusterBuf[i];
            if (firstByte == 0x00) break; // end of entries in this cluster chain
            if (firstByte == 0xE5) continue;
            uint8_t attr = clusterBuf[i + 11];
            if (attr == 0x0F) continue; // skip LFN
            // Build short name
            int8_t name[13]; int32_t idx = 0;
            for (int j = 0; j < 8; ++j) { int8_t c = (int8_t)clusterBuf[i + j]; if (c == ' ') break; name[idx++] = c; }
            int8_t ext[4]; int e = 0; for (int j = 0; j < 3; ++j) { int8_t c = (int8_t)clusterBuf[i + 8 + j]; if (c == ' ') break; ext[e++] = c; }
            name[idx] = 0;
            int8_t merged[13]; int m = 0;
            if (idx > 0) { String::memmove(merged + m, name, (uint32_t)idx); m += idx; }
            if (e > 0) { merged[m++] = '.'; String::memmove(merged + m, ext, (uint32_t)e); m += e; }
            merged[m] = 0;
            for (int t = 0; merged[t]; ++t) if (merged[t] >= 'a' && merged[t] <= 'z') merged[t] -= ('a' - 'A');
            if (String::strcmp((const uint8_t*)merged, (const uint8_t*)shortName83) == 0) {
                uint16_t lo = (uint16_t)clusterBuf[i + 26] | ((uint16_t)clusterBuf[i + 27] << 8);
                uint16_t hi = (uint16_t)clusterBuf[i + 20] | ((uint16_t)clusterBuf[i + 21] << 8);
                outStartCluster = ((uint32_t)hi << 16) | lo;
                outFileSize = (uint32_t)clusterBuf[i + 28] | ((uint32_t)clusterBuf[i + 29] << 8) | ((uint32_t)clusterBuf[i + 30] << 16) | ((uint32_t)clusterBuf[i + 31] << 24);
                return true;
            }
        }
        uint32_t next = NextCluster(cl);
        if (next >= 0x0FFFFFF8 || next == 0) break;
        cl = next;
    }
    return false;
}

int32_t FAT32::ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
    if (!mountedFlag) return -1;
    if (!path || path[0] != '/') return -1;

    // Traverse arbitrary path components from root cluster
    uint32_t dirCluster = bpb.rootCluster;
    const int8_t* p = path + 1; // skip leading '/'
    int8_t comp[13];
    while (*p) {
        // Extract next component
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0;
        if (*p == '/') ++p; // skip '/'
        if (ci == 0) continue; // skip redundant slashes

        // Uppercase for 8.3 short-name compare
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a' - 'A');

        // Determine if last component
        const int8_t* q = p; while (*q == '/') ++q; bool isLast = (*q == 0);

        uint32_t entryCl = 0, entrySize = 0;
        if (!FindShortNameInDirCluster(dirCluster, comp, entryCl, entrySize)) return -1; // component not found

        if (!isLast) {
            // Must be a directory; descend (we don't validate attr here due to short helper, but paths are created as dirs by Mkdir)
            dirCluster = entryCl;
            continue;
        }

        // Last component: treat as file and read its FAT chain
        uint32_t toRead = (entrySize < maxLen) ? entrySize : maxLen;
        uint32_t bytesRead = 0;
        uint8_t* dst = outBuf;
        uint32_t cluster = entryCl;
        uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
        uint8_t* clBuf = (uint8_t*)0x30000; // scratch
        while (cluster >= 2 && cluster < 0x0FFFFFF8 && bytesRead < toRead) {
            if (!ReadCluster(cluster, clBuf)) break;
            uint32_t chunk = bytesPerCluster;
            if (bytesRead + chunk > toRead) chunk = toRead - bytesRead;
            kos::lib::String::memmove(dst + bytesRead, clBuf, chunk);
            bytesRead += chunk;
            uint32_t next = NextCluster(cluster);
            if (next >= 0x0FFFFFF8 || next == 0) break;
            cluster = next;
        }
        return (int32_t)bytesRead;
    }
    return -1; // empty path
}

int32_t FAT32::Mkdir(const int8_t* path, int32_t parents) {
    if (!mountedFlag || !path) {
        tty.Write((const int8_t*)"FAT32: Mkdir invalid (not mounted or null path)\n");
        return -1;
    }
    // Accept both absolute and relative paths; treat relative as root-relative for now
    uint32_t curDir = bpb.rootCluster;
    const int8_t* p = (path[0] == '/') ? (path + 1) : path;
    int8_t comp[13];
    while (*p) {
        // Extract next component up to '/' or end
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0;
        if (*p == '/') ++p; // skip '/'
        if (ci == 0) continue; // skip redundant slashes
        // Uppercase for 8.3 compare
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a'-'A');
        // Check if this component exists under curDir
        uint32_t childCl = 0, childSize = 0;
        if (FindShortNameInDirCluster(curDir, comp, childCl, childSize)) {
            curDir = childCl; // descend
            continue;
        }
        // Not found: if not -p and not at final component, fail
        // Determine if this is the final component
        const int8_t* q = p; while (*q == '/') ++q; bool finalComp = (*q == 0);
        if (!parents && !finalComp) {
            tty.Write((const int8_t*)"FAT32: parent missing and -p not set\n");
            return -1;
        }
        // Create this component as a new directory under curDir
        // Pack 8.3 short name
        uint8_t short11[11]; bool ok83 = false; PackShortName11(comp, short11, ok83, true);
        if (!ok83) { tty.Write((const int8_t*)"FAT32: invalid 8.3 name\n"); return -1; }
        // Allocate a cluster for the new dir
        uint32_t newCl = AllocateCluster();
        if (newCl < 2) { tty.Write((const int8_t*)"FAT32: no free clusters\n"); return -1; }
        // Initialize directory cluster
        if (!InitDirCluster(newCl, curDir)) { tty.Write((const int8_t*)"FAT32: init dir failed\n"); return -1; }
        // Add entry to parent directory
        if (!AddEntryToDirCluster(curDir, short11, newCl, true)) { tty.Write((const int8_t*)"FAT32: add entry failed\n"); return -1; }
        // Descend into the newly created directory and continue
        curDir = newCl;
    }
    return 0;
}

void FAT32::DebugInfo() {
    if (!mountedFlag) {
        tty.Write("FAT32: no montado\n");
        return;
    }
    tty.Write("FAT32 Debug Info\n");
    tty.Write(" bytes/sector="); 
    printHex16(bpb.bytesPerSector); 
    tty.PutChar('\n');
    tty.Write(" sectores/cluster="); 
    tty.WriteHex(bpb.sectorsPerCluster); 
    tty.PutChar('\n');
    tty.Write(" reservados="); 
    printHex16(bpb.reservedSectors); 
    tty.PutChar('\n');
    tty.Write(" numFATs="); 
    tty.WriteHex(bpb.numFATs); 
    tty.PutChar('\n');
    tty.Write(" sectoresPorFAT="); 
    printHex32(bpb.sectorsPerFAT); 
    tty.PutChar('\n');
    tty.Write(" rootCluster="); 
    printHex32(bpb.rootCluster); 
    tty.PutChar('\n');
    tty.Write(" volumenLBA="); 
    printHex32(volumeStartLBA); 
    tty.PutChar('\n');
    tty.Write(" fatStartLBA="); 
    printHex32(fatStartLBA); 
    tty.PutChar('\n');
    tty.Write(" dataStartLBA="); 
    printHex32(dataStartLBA); 
    tty.PutChar('\n');
}

// (No DebugInfo method; detailed logs are printed during Mount())

int32_t FAT32::Rename(const int8_t* src, const int8_t* dst) {
    if (!mountedFlag || !src || !dst) return -1;
    if (src[0] != '/' || dst[0] != '/') return -1;

    auto split_parent_basename = [](const int8_t* path, int8_t* parentOut, int parentCap, int8_t* baseOut, int baseCap) {
        // Copy path, find last '/'
        int len = 0; while (path[len]) ++len;
        int last = -1; for (int i = 0; i < len; ++i) if (path[i] == '/') last = i;
        if (last <= 0) { // root or malformed, parent='/'
            if (parentCap > 1) { parentOut[0] = '/'; parentOut[1] = 0; } else if (parentCap > 0) parentOut[0] = 0;
            int bi = 0; for (int i = last+1; path[i] && bi < baseCap-1; ++i) baseOut[bi++] = path[i]; if (baseCap>0) baseOut[bi]=0;
            return;
        }
        int pi = 0; for (int i = 0; i < last && pi < parentCap-1; ++i) parentOut[pi++] = path[i];
        if (pi == 0 && parentCap > 0) { parentOut[pi++] = '/'; }
        if (parentCap > 0) parentOut[pi] = 0;
        int bi = 0; for (int i = last+1; path[i] && bi < baseCap-1; ++i) baseOut[bi++] = path[i]; if (baseCap>0) baseOut[bi]=0;
    };

    int8_t srcParent[160], srcBase[32]; srcParent[0]=0; srcBase[0]=0;
    int8_t dstParent[160], dstBase[32]; dstParent[0]=0; dstBase[0]=0;
    split_parent_basename(src, srcParent, sizeof(srcParent), srcBase, sizeof(srcBase));
    split_parent_basename(dst, dstParent, sizeof(dstParent), dstBase, sizeof(dstBase));
    if (srcBase[0] == 0 || dstBase[0] == 0) return -1;

    // Traverse from root to get parent clusters
    auto walk_to_dir = [&](const int8_t* path, uint32_t& outDirCl) -> bool {
        outDirCl = bpb.rootCluster;
        if (path[0] == '/' && path[1] == 0) return true;
        const int8_t* p = (path[0] == '/') ? (path + 1) : path;
        int8_t comp[13];
        while (*p) {
            int ci=0; while (*p && *p!='/' && ci<12) comp[ci++]=*p++; comp[ci]=0; if (*p=='/') ++p; if (ci==0) continue;
            for (int i=0; comp[i]; ++i) if (comp[i]>='a'&&comp[i]<='z') comp[i]-=('a'-'A');
            uint32_t childCl=0, childSz=0; if (!FindShortNameInDirCluster(outDirCl, comp, childCl, childSz)) return false; outDirCl = childCl;
        }
        return true;
    };

    uint32_t srcDirCl=0, dstDirCl=0; if (!walk_to_dir(srcParent, srcDirCl)) return -1; if (!walk_to_dir(dstParent, dstDirCl)) return -1;

    // Build 8.3 names
    for (int i=0; srcBase[i]; ++i) if (srcBase[i]>='a'&&srcBase[i]<='z') srcBase[i]-=('a'-'A');
    for (int i=0; dstBase[i]; ++i) if (dstBase[i]>='a'&&dstBase[i]<='z') dstBase[i]-=('a'-'A');
    uint8_t src11[11], dst11[11]; bool ok1=false, ok2=false; PackShortName11(srcBase, src11, ok1, true); PackShortName11(dstBase, dst11, ok2, true);
    if (!ok1 || !ok2) return -1; // invalid names

    // Locate src entry, fetch cluster/size/attr and entry position
    uint32_t entryCl=0, entryOff=0, startCl=0, fileSize=0; uint8_t attr=0;
    if (!FindShortEntryWithOffset(srcDirCl, src11, entryCl, entryOff, startCl, fileSize, attr)) return -1;

    // Fail if destination name exists in dest directory (no overwrite for now)
    uint32_t tmpCl=0, tmpSz=0; if (FindShortNameInDirCluster(dstDirCl, (const int8_t*)dstBase, tmpCl, tmpSz)) return -1;

    // Same directory rename: modify name in place
    if (srcDirCl == dstDirCl) {
        uint8_t* dirBuf = (uint8_t*)0x3C000;
        if (!ReadCluster(entryCl, dirBuf)) return -1;
        for (int i=0;i<11;++i) dirBuf[entryOff+i] = dst11[i];
        return WriteCluster(entryCl, dirBuf) ? 0 : -1;
    }

    // Cross-directory move: support files only (attr 0x20). Directories need '..' update.
    if (!(attr & 0x20)) {
        tty.Write((const int8_t*)"mv: moving directories across parents not supported\n");
        return -1;
    }
    // Add new entry to destination dir with same start cluster and size
    if (!AddFileEntryToDir(dstDirCl, dst11, startCl, fileSize)) return -1;
    // Mark old entry as deleted (0xE5)
    uint8_t* dirBuf = (uint8_t*)0x3D000; if (!ReadCluster(entryCl, dirBuf)) return -1; dirBuf[entryOff] = 0xE5; if (!WriteCluster(entryCl, dirBuf)) return -1;
    return 0;
}
