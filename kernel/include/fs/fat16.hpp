#ifndef __KOS__FS__FAT16_H
#define __KOS__FS__FAT16_H

#include <common/types.hpp>
#include <drivers/blockdevice.hpp>
#include <fs/filesystem.hpp>

using namespace kos::common;
using namespace kos::drivers;

namespace kos { 
    
    namespace fs {

        struct FAT16_BPB {
            uint16_t bytesPerSector;
            uint8_t sectorsPerCluster;
            uint16_t reservedSectors;
            uint8_t numFATs;
            uint16_t rootEntryCount;
            uint16_t sectorsPerFAT;
        };

        class FAT16 : public Filesystem {
            public:
                FAT16(BlockDevice* dev, uint32_t startLBA = 0);
                virtual ~FAT16() {}
                virtual bool Mount();
                virtual void ListRoot();
                virtual void DebugInfo();
                virtual int32_t ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) override;

            private:
                BlockDevice* dev;
                FAT16_BPB bpb;
                uint32_t volumeStartLBA;
                uint32_t fatStartLBA;
                uint32_t rootDirLBA;
                uint32_t rootDirSectors;
                uint32_t dataStartLBA;
                bool mounted;

                bool ReadSector(uint32_t lba, uint8_t* buf);
                bool ReadSectors(uint32_t lba, uint32_t count, uint8_t* buf);
                uint32_t ClusterToLBA(uint32_t cluster);
                bool ReadCluster(uint32_t cluster, uint8_t* buf);
                uint32_t NextCluster(uint32_t cluster);
                bool FindShortNameInRoot(const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize, bool& isDir);
                bool FindShortNameInDirCluster(uint32_t dirCluster, const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize, bool& isDir);
        };
    }
}

#endif
