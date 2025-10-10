#include <fs/fat16.hpp>
#include <console/tty.hpp>
#include <lib/string.hpp>

using namespace kos::fs;
using namespace kos::common;
using namespace kos::console;
using namespace kos::drivers;
using namespace kos::lib;

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
    rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
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

bool FAT16::ReadSectors(uint32_t lba, uint32_t count, uint8_t* buf) {
    return dev->ReadSectors(lba, (uint8_t)count, buf);
}

uint32_t FAT16::ClusterToLBA(uint32_t cluster) {
    return dataStartLBA + (cluster - 2) * bpb.sectorsPerCluster;
}

bool FAT16::ReadCluster(uint32_t cluster, uint8_t* buf) {
    return ReadSectors(ClusterToLBA(cluster), bpb.sectorsPerCluster, buf);
}

uint32_t FAT16::NextCluster(uint32_t cluster) {
    // FAT16 entries are 16-bit
    uint32_t fatOffset = cluster * 2;
    uint32_t fatSector = fatStartLBA + (fatOffset / 512);
    uint32_t entOff = fatOffset % 512;
    uint8_t sec[512];
    if (!ReadSector(fatSector, sec)) return 0xFFFF;
    uint16_t val = (uint16_t)sec[entOff] | ((uint16_t)sec[entOff+1] << 8);
    return val;
}

bool FAT16::FindShortNameInRoot(const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize, bool& isDir) {
    uint8_t sec[512];
    for (uint32_t s = 0; s < rootDirSectors; ++s) {
        if (!ReadSector(rootDirLBA + s, sec)) return false;
        for (int i = 0; i < 512; i += 32) {
            uint8_t first = sec[i];
            if (first == 0x00) return false;
            if (first == 0xE5) continue;
            uint8_t attr = sec[i + 11];
            if (attr == 0x0F) continue;
            // Build short name
            int8_t name[13]; int ni = 0;
            for (int j = 0; j < 8; ++j) { int8_t c = (int8_t)sec[i+j]; if (c==' ') break; name[ni++] = c; }
            int8_t ext[4]; int ei = 0; for (int j = 0; j < 3; ++j) { int8_t c = (int8_t)sec[i+8+j]; if (c==' ') break; ext[ei++] = c; }
            name[ni] = 0;
            int8_t merged[13]; int m=0; for (int k=0;k<ni;++k) merged[m++]=name[k]; if(ei>0){ merged[m++]='.'; for(int k=0;k<ei;++k) merged[m++]=ext[k]; } merged[m]=0;
            if (kos::lib::String::strcmp((const uint8_t*)merged, (const uint8_t*)shortName83) == 0) {
                uint16_t cl = (uint16_t)sec[i+26] | ((uint16_t)sec[i+27] << 8);
                outStartCluster = cl;
                outFileSize = (uint32_t)sec[i+28] | ((uint32_t)sec[i+29]<<8) | ((uint32_t)sec[i+30]<<16) | ((uint32_t)sec[i+31]<<24);
                isDir = (attr & 0x10) != 0;
                return true;
            }
        }
    }
    return false;
}

bool FAT16::FindShortNameInDirCluster(uint32_t dirCluster, const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize, bool& isDir) {
    uint8_t* buf = (uint8_t*)0x24000;
    if (!ReadCluster(dirCluster, buf)) return false;
    for (uint32_t i = 0; i < bpb.sectorsPerCluster*512; i += 32) {
        uint8_t first = buf[i];
        if (first == 0x00) return false;
        if (first == 0xE5) continue;
        uint8_t attr = buf[i + 11];
        if (attr == 0x0F) continue;
        int8_t name[13]; int ni=0; for (int j=0;j<8;++j){ int8_t c=(int8_t)buf[i+j]; if(c==' ') break; name[ni++]=c; } name[ni]=0;
        int8_t ext[4]; int ei=0; for (int j=0;j<3;++j){ int8_t c=(int8_t)buf[i+8+j]; if(c==' ') break; ext[ei++]=c; } ext[ei]=0;
        int8_t merged[13]; int m=0; for(int k=0;k<ni;++k) merged[m++]=name[k]; if(ei>0){ merged[m++]='.'; for(int k=0;k<ei;++k) merged[m++]=ext[k]; } merged[m]=0;
        if (kos::lib::String::strcmp((const uint8_t*)merged, (const uint8_t*)shortName83) == 0) {
            uint16_t cl = (uint16_t)buf[i+26] | ((uint16_t)buf[i+27] << 8);
            outStartCluster = cl;
            outFileSize = (uint32_t)buf[i+28] | ((uint32_t)buf[i+29]<<8) | ((uint32_t)buf[i+30]<<16) | ((uint32_t)buf[i+31]<<24);
            isDir = (attr & 0x10) != 0;
            return true;
        }
    }
    return false;
}

int32_t FAT16::ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) {
    if (!mounted) return -1; if (!path || path[0] != '/') return -1;
    const int8_t* p = path + 1;
    // optionally enter /bin
    bool isDir=false; uint32_t binCl=0, binSz=0;
    if (p[0]=='b' && p[1]=='i' && p[2]=='n' && p[3]=='/') {
        const int8_t BIN[4] = {'B','I','N',0};
        if (FindShortNameInRoot(BIN, binCl, binSz, isDir) && isDir) {
            p += 4;
        } else {
            return -1;
        }
    }
    // uppercase
    int8_t name83[13]; uint32_t ni=0; while(p[ni] && ni<sizeof(name83)-1){ int8_t c=p[ni]; if(c>='a'&&c<='z') c -= ('a'-'A'); name83[ni++]=c; } name83[ni]=0;
    uint32_t startCl=0, fileSz=0; bool dir=false;
    bool ok=false;
    if (binCl >= 2) ok = FindShortNameInDirCluster(binCl, name83, startCl, fileSz, dir);
    else ok = FindShortNameInRoot(name83, startCl, fileSz, dir);
    if (!ok || dir) return -1;
    // Read chain
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    uint32_t toRead = (fileSz < maxLen) ? fileSz : maxLen;
    uint8_t* dst = outBuf; uint32_t read = 0; uint8_t* clBuf = (uint8_t*)0x26000; uint32_t cl=startCl;
    while (cl >= 2 && cl < 0xFFF8 && read < toRead) {
        if (!ReadCluster(cl, clBuf)) break;
        uint32_t chunk = bytesPerCluster; if (read + chunk > toRead) chunk = toRead - read;
        for (uint32_t i=0;i<chunk;++i) dst[read+i] = clBuf[i];
        read += chunk;
        uint32_t nxt = NextCluster(cl);
        if (nxt >= 0xFFF8 || nxt==0) break; cl = nxt;
    }
    return (int32_t)read;
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
