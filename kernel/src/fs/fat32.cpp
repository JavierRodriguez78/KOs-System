#include <fs/fat32.hpp>
#include <console/tty.hpp>

using namespace kos::fs;
using namespace kos::drivers;
using namespace kos::common;
using namespace kos::console;

static TTY tty;

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

uint32_t FAT32::DetectFAT32PartitionStart() {
    uint8_t mbr[512];
    if (!ReadSector(0, mbr)) {
        tty.Write("FAT32: Error leyendo MBR (LBA 0)\n");
        return 0;
    }
    // Dump primeros bytes del MBR para depurar
    tty.Write("FAT32: Dump MBR[0..32) en LBA 0\n");
    for (int32_t i = 0; i < 32; ++i) { 
        tty.WriteHex(mbr[i]); tty.PutChar(' ');
     }
    tty.PutChar('\n');
    tty.Write("FAT32: MBR firma (510,511): "); 
    tty.WriteHex(mbr[510]); 
    tty.PutChar(' '); 
    tty.WriteHex(mbr[511]); 
    tty.PutChar('\n');
    bool bootSig = (mbr[510] == 0x55 && mbr[511] == 0xAA);
    bool hasPartitions = false;
    // MBR partition table at 446, four entries of 16 bytes each
    uint32_t fat32LBA = 0xFFFFFFFF; // marcador: MBR presente pero sin FAT32
    for (int32_t i = 0; i < 4; ++i) {
        int off = 446 + i * 16;
        uint8_t type = mbr[off + 4];
        if (type != 0x00) hasPartitions = true;
        tty.Write("FAT32: Particion "); 
        tty.WriteHex((uint8_t)i); 
        tty.Write(": tipo="); 
        tty.WriteHex(type); 
        tty.Write(" startLBA=");
        uint32_t lbaStartDbg = (uint32_t)mbr[off + 8] |
                                ((uint32_t)mbr[off + 9] << 8) |
                                ((uint32_t)mbr[off + 10] << 16) |
                                ((uint32_t)mbr[off + 11] << 24);
        printHex32(lbaStartDbg); 
        tty.PutChar('\n');
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
        tty.Write("FAT32: MBR presente pero sin particiones FAT32. (Se detectó p.ej. FAT16 tipo 0x06)\n");
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
        tty.Write("FAT32: Abortando montaje: no hay particion FAT32 y MBR existe\n");
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

void FAT32::ListRoot() {
    if (bpb.bytesPerSector == 0) {
        tty.Write("FAT32 not mounted\n");
        return;
    }

    uint8_t sector[512];
    uint32_t lba = ClusterToLBA(bpb.rootCluster);
    if (!ReadSector(lba, sector)) {
        tty.Write("FAT32: Error leyendo clúster raíz en LBA ");
        printHex32(lba);
        tty.PutChar('\n');
        return;
    }

    // Iterate directory entries (32 bytes each)
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
        int32_t extStart = idx;
        int8_t ext[4];
        int32_t e = 0;
        for (int32_t j = 0; j < 3; ++j) {
            int8_t c = (int8_t)sector[i + 8 + j];
            if (c == ' ') break;
            ext[e++] = c;
        }
        name[idx] = 0;
        if (e > 0) {
            tty.Write(name);
            tty.Write(".");
            int8_t extStr[4];
            for (int32_t k = 0; k < e; ++k) extStr[k] = ext[k];
            extStr[e] = 0;
            tty.Write(extStr);
            tty.PutChar('\n');
        } else {
            tty.Write(name);
            tty.PutChar('\n');
        }
    }
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
