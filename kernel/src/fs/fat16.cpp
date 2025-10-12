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

bool FAT16::WriteSector(uint32_t lba, const uint8_t* buf) {
    return dev->WriteSectors(lba, 1, const_cast<uint8_t*>(buf));
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
    int col = 0; const int colWidth = 16; const int colsPerRow = 4;
    for (int32_t s = 0; s < 4; ++s) {
        if (!ReadSector(rootDirLBA + s, sector)){
            tty.Write("FAT16: Error reading root dir sector\n");
            break;
        }
        for (int32_t i = 0; i < 512; i += 32) {
            uint8_t first = sector[i];
            if (first == 0x00) {
                if (col>0) tty.PutChar('\n');
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
            // Filter . and ..
            if (e == 0 && ((n == 1 && name[0] == '.') || (n == 2 && name[0] == '.' && name[1] == '.'))) continue;
            // Build display string
            int8_t display[20]; int di = 0;
            for (int k=0; k<n && di < (int)sizeof(display)-1; ++k) display[di++] = name[k];
            if (!(attr & 0x10) && e>0) { if (di < (int)sizeof(display)-1) display[di++]='.'; for (int k=0; k<e && di < (int)sizeof(display)-1; ++k) display[di++] = ext[k]; }
            if (attr & 0x10) { if (di < (int)sizeof(display)-1) display[di++] = '/'; }
            display[di] = 0; int len = di;
            if (attr & 0x10) TTY::SetColor(10,0);
            tty.Write(display);
            if (attr & 0x10) TTY::SetAttr(0x07);
            for (int sps=len; sps<colWidth; ++sps) tty.PutChar(' ');
            if (++col >= colsPerRow) { tty.PutChar('\n'); col = 0; }
        }
    }
    if (col>0) tty.PutChar('\n');
}
void FAT16::ListDir(const int8_t* path) {
    if (!mounted) { tty.Write((const int8_t*)"FAT16: not mounted\n"); return; }
    if (!path || (path[0]=='/' && path[1]==0)) { ListRoot(); return; }
    // Traverse path components from root and ensure each is a directory
    const int8_t* p = (path[0] == '/') ? (path + 1) : path;
    uint32_t dirCl = 0; bool atRoot = true;
    int8_t comp[13];
    while (*p) {
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0; if (*p == '/') ++p; if (ci == 0) continue;
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a' - 'A');
        uint32_t childCl = 0, childSz = 0; bool isDir = false; bool ok = false;
        if (atRoot) ok = FindShortNameInRoot(comp, childCl, childSz, isDir);
        else ok = FindShortNameInDirCluster(dirCl, comp, childCl, childSz, isDir);
        if (!ok || !isDir) { tty.Write((const int8_t*)"ls: path not found\n"); return; }
        dirCl = childCl; atRoot = false;
    }
    uint8_t* buf = (uint8_t*)0x24000;
    if (!ReadCluster(dirCl, buf)) { tty.Write((const int8_t*)"ls: read dir failed\n"); return; }
    int col = 0; const int colWidth = 16; const int colsPerRow = 4;
    for (uint32_t i=0; i < bpb.sectorsPerCluster*512; i += 32) {
        uint8_t first = buf[i]; if (first==0x00) break; if (first==0xE5) continue; uint8_t attr = buf[i+11]; if (attr==0x0F) continue;
        int8_t name[13]; int ni=0; for (int j=0;j<8;++j){ int8_t c=(int8_t)buf[i+j]; if(c==' ') break; name[ni++]=c; } name[ni]=0;
        int8_t ext[4]; int ei=0; for (int j=0;j<3;++j){ int8_t c=(int8_t)buf[i+8+j]; if(c==' ') break; ext[ei++]=c; } ext[ei]=0;
        // Filter . and ..
        if (ei==0 && ((ni==1 && name[0]=='.') || (ni==2 && name[0]=='.' && name[1]=='.'))) continue;
        int8_t display[20]; int di=0; for (int k=0;k<ni && di < (int)sizeof(display)-1; ++k) display[di++]=name[k];
        if (!(attr & 0x10) && ei>0) { if (di < (int)sizeof(display)-1) display[di++]='.'; for (int k=0;k<ei && di < (int)sizeof(display)-1; ++k) display[di++]=ext[k]; }
        if (attr & 0x10) { if (di < (int)sizeof(display)-1) display[di++]='/'; }
        display[di]=0; int len = di;
        if (attr & 0x10) TTY::SetColor(10,0);
        tty.Write(display);
        if (attr & 0x10) TTY::SetAttr(0x07);
        for (int sps=len; sps<colWidth; ++sps) tty.PutChar(' ');
        if (++col >= colsPerRow) { tty.PutChar('\n'); col=0; }
    }
    if (col>0) tty.PutChar('\n');
}

bool FAT16::DirExists(const int8_t* path) {
    if (!mounted) return false;
    if (!path) return false;
    if (path[0]=='/' && path[1]==0) return true;
    const int8_t* p = (path[0]=='/') ? (path+1) : path;
    uint32_t dirCl = 0; bool atRoot = true; int8_t comp[13];
    while (*p) {
        int ci=0; while (*p && *p!='/') { if (ci<12) comp[ci++]=*p; ++p; }
        comp[ci]=0; if (*p=='/') ++p; if (ci==0) continue;
        for (int i=0; comp[i]; ++i) if (comp[i]>='a'&&comp[i]<='z') comp[i]-=('a'-'A');
        uint32_t childCl=0, childSz=0; bool isDir=false; bool ok=false;
        if (atRoot) ok = FindShortNameInRoot(comp, childCl, childSz, isDir);
        else ok = FindShortNameInDirCluster(dirCl, comp, childCl, childSz, isDir);
        if (!ok || !isDir) return false;
        dirCl = childCl; atRoot = false;
    }
    return true;
}

bool FAT16::ReadSectors(uint32_t lba, uint32_t count, uint8_t* buf) {
    return dev->ReadSectors(lba, (uint8_t)count, buf);
}

bool FAT16::WriteSectors(uint32_t lba, uint32_t count, const uint8_t* buf) {
    return dev->WriteSectors(lba, (uint8_t)count, const_cast<uint8_t*>(buf));
}

uint32_t FAT16::ClusterToLBA(uint32_t cluster) {
    return dataStartLBA + (cluster - 2) * bpb.sectorsPerCluster;
}

bool FAT16::ReadCluster(uint32_t cluster, uint8_t* buf) {
    return ReadSectors(ClusterToLBA(cluster), bpb.sectorsPerCluster, buf);
}

bool FAT16::WriteCluster(uint32_t cluster, const uint8_t* buf) {
    return WriteSectors(ClusterToLBA(cluster), bpb.sectorsPerCluster, buf);
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

bool FAT16::UpdateFAT(uint32_t cluster, uint16_t value) {
    uint32_t fatOffset = cluster * 2;
    uint32_t fatSector = fatStartLBA + (fatOffset / 512);
    uint32_t entOff = fatOffset % 512;
    uint8_t sec[512];
    if (!ReadSector(fatSector, sec)) return false;
    sec[entOff] = (uint8_t)(value & 0xFF);
    sec[entOff+1] = (uint8_t)((value >> 8) & 0xFF);
    if (!WriteSector(fatSector, sec)) return false;
    // Mirror to other FATs if any
    for (uint8_t f = 1; f < bpb.numFATs; ++f) {
        uint32_t mirror = fatSector + f * bpb.sectorsPerFAT;
        if (!WriteSector(mirror, sec)) return false;
    }
    return true;
}

uint32_t FAT16::AllocateCluster() {
    // naive scan for free FAT16 cluster (0x0000 free). Start from 2.
    // Calculate approximate cluster count from data region size
    // For FAT16, total sectors not cached; we scan FAT sectors size*512/2 entries
    uint32_t entries = (uint32_t)bpb.sectorsPerFAT * 512 / 2;
    uint8_t sec[512];
    for (uint32_t cl = 2; cl < entries; ++cl) {
        uint32_t fatOffset = cl * 2;
        uint32_t fatSector = fatStartLBA + (fatOffset / 512);
        uint32_t entOff = fatOffset % 512;
        if (!ReadSector(fatSector, sec)) return 0;
        uint16_t val = (uint16_t)sec[entOff] | ((uint16_t)sec[entOff+1] << 8);
        if (val == 0x0000) {
            if (UpdateFAT(cl, 0xFFFF)) return cl; // mark EOC
            return 0;
        }
    }
    return 0;
}

bool FAT16::InitDirCluster(uint32_t newCluster, uint32_t parentCluster) {
    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    uint8_t tmp[4096];
    for (uint32_t i = 0; i < bytesPerCluster && i < sizeof(tmp); ++i) tmp[i] = 0;
    auto writeEntry = [&](uint32_t off, const char* name, uint32_t startCl, bool isDir) {
        for (int i = 0; i < 11; ++i) tmp[off+i] = ' ';
        tmp[off+0] = name[0]; if (name[1]) tmp[off+1] = name[1];
        tmp[off+11] = isDir ? 0x10 : 0x20;
        tmp[off+26] = (uint8_t)(startCl & 0xFF);
        tmp[off+27] = (uint8_t)((startCl >> 8) & 0xFF);
        tmp[off+28] = tmp[off+29] = tmp[off+30] = tmp[off+31] = 0;
    };
    writeEntry(0, ".", newCluster, true);
    // For root parent in FAT16, parentCluster is 0 in ..
    writeEntry(32, "..", parentCluster, true);
    return WriteCluster(newCluster, tmp);
}

bool FAT16::AddEntryToRoot(const uint8_t shortName11[11], uint32_t startCluster, bool isDir) {
    uint8_t sec[512];
    for (uint32_t s = 0; s < rootDirSectors; ++s) {
        if (!ReadSector(rootDirLBA + s, sec)) return false;
        for (int i = 0; i < 512; i += 32) {
            uint8_t fb = sec[i];
            if (fb == 0x00 || fb == 0xE5) {
                // write entry here
                for (int j = 0; j < 11; ++j) sec[i + j] = shortName11[j];
                sec[i + 11] = isDir ? 0x10 : 0x20;
                sec[i + 26] = (uint8_t)(startCluster & 0xFF);
                sec[i + 27] = (uint8_t)((startCluster >> 8) & 0xFF);
                sec[i + 28] = sec[i + 29] = sec[i + 30] = sec[i + 31] = 0;
                return WriteSector(rootDirLBA + s, sec);
            }
        }
    }
    tty.Write((const int8_t*)"FAT16: Root directory full\n");
    return false;
}

void FAT16::PackShortName11(const int8_t* name83, uint8_t out11[11], bool& okIs83, bool upperOnly) {
    for (int i = 0; i < 11; ++i) out11[i] = ' ';
    okIs83 = true; int n = 0; int e = -1;
    for (int i = 0; name83 && name83[i]; ++i) {
        if (name83[i] == '.') { if (e >= 0) { okIs83 = false; break; } e = 0; continue; }
        int8_t c = name83[i]; if (c >= 'a' && c <= 'z' && upperOnly) c -= ('a' - 'A');
        bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c=='_' || c=='$' || c=='~' || c=='!';
        if (!ok) { okIs83 = false; break; }
        if (e < 0) { if (n < 8) out11[n++] = (uint8_t)c; }
        else { if (e < 3) out11[8 + (e++)] = (uint8_t)c; }
    }
}

int32_t FAT16::Mkdir(const int8_t* path, int32_t parents) {
    if (!mounted || !path) { tty.Write((const int8_t*)"FAT16: Mkdir invalid\n"); return -1; }
    const int8_t* p = (path[0] == '/') ? (path + 1) : path;
    uint32_t curCl = 0; bool atRoot = true; // Root directory is special (no cluster)
    int8_t comp[13];
    while (*p) {
        int ci = 0; while (*p && *p != '/' && ci < 12) comp[ci++] = *p++;
        comp[ci] = 0; if (*p == '/') ++p; if (ci == 0) continue;
        // Uppercase comp
        for (int i = 0; comp[i]; ++i) if (comp[i] >= 'a' && comp[i] <= 'z') comp[i] -= ('a' - 'A');
        // Check if exists
        uint32_t childCl = 0, childSz = 0; bool isDir = false; bool found = false;
        if (atRoot) {
            found = FindShortNameInRoot(comp, childCl, childSz, isDir);
        } else {
            found = FindShortNameInDirCluster(curCl, comp, childCl, childSz, isDir);
        }
        if (found) {
            if (!isDir) { tty.Write((const int8_t*)"FAT16: name exists as file\n"); return -1; }
            curCl = childCl; atRoot = false; continue;
        }
        // not found; must create here
        const int8_t* q = p; while (*q == '/') ++q; bool final = (*q == 0);
        if (!parents && !final) { tty.Write((const int8_t*)"FAT16: parent missing and -p not set\n"); return -1; }
        uint8_t short11[11]; bool ok83=false; PackShortName11(comp, short11, ok83, true);
        if (!ok83) { tty.Write((const int8_t*)"FAT16: invalid 8.3 name\n"); return -1; }
        uint32_t newCl = AllocateCluster(); if (newCl < 2) { tty.Write((const int8_t*)"FAT16: no free clusters\n"); return -1; }
        if (!InitDirCluster(newCl, atRoot ? 0 : curCl)) { tty.Write((const int8_t*)"FAT16: init dir failed\n"); return -1; }
        bool ok = false;
        if (atRoot) {
            ok = AddEntryToRoot(short11, newCl, true);
            if (!ok) return -1;
        } else {
            // write short entry into dir cluster directly
            uint8_t* clbuf = (uint8_t*)0x28000; if (!ReadCluster(curCl, clbuf)) return -1;
            uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster; uint32_t off=0;
            for (; off < bytesPerCluster; off += 32) { uint8_t fb = clbuf[off]; if (fb==0x00 || fb==0xE5) break; }
            if (off >= bytesPerCluster) { tty.Write((const int8_t*)"FAT16: dir full\n"); return -1; }
            for (int j=0;j<11;++j) clbuf[off+j]=short11[j]; clbuf[off+11]=0x10; clbuf[off+26]=(uint8_t)(newCl&0xFF); clbuf[off+27]=(uint8_t)((newCl>>8)&0xFF);
            clbuf[off+28]=clbuf[off+29]=clbuf[off+30]=clbuf[off+31]=0;
            if (!WriteCluster(curCl, clbuf)) return -1; ok = true;
        }
        curCl = newCl; atRoot = false;
    }
    return 0;
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
            int8_t merged[13]; int m=0;
            if (ni > 0) { String::memmove(merged + m, name, (uint32_t)ni); m += ni; }
            if (ei > 0) {
                merged[m++]='.';
                String::memmove(merged + m, ext, (uint32_t)ei);
                m += ei;
            }
            merged[m]=0;
            if (String::strcmp((const uint8_t*)merged, (const uint8_t*)shortName83) == 0) {
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
    int8_t merged[13]; int m=0;
    if (ni > 0) { String::memmove(merged + m, name, (uint32_t)ni); m += ni; }
    if (ei > 0) { merged[m++]='.'; String::memmove(merged + m, ext, (uint32_t)ei); m += ei; }
    merged[m]=0;
        if (String::strcmp((const uint8_t*)merged, (const uint8_t*)shortName83) == 0) {
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
