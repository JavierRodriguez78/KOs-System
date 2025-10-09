#ifndef __KOS__FS__FAT32_H
#define __KOS__FS__FAT32_H

#include <common/types.hpp>
#include <drivers/blockdevice.hpp>
#include <fs/filesystem.hpp>

using namespace kos::common;
using namespace kos::drivers;

namespace kos { 
    namespace fs {

        struct FAT32_BPB {
            uint16_t bytesPerSector;
            uint8_t sectorsPerCluster;
            uint16_t reservedSectors;
            uint8_t numFATs;
            uint32_t sectorsPerFAT;
            uint32_t rootCluster;
        };

    class FAT32 : public Filesystem {
        public:
            FAT32(BlockDevice* dev);
            virtual ~FAT32();
            virtual bool Mount();
            virtual void ListRoot(); // print short names to TTY
            virtual void DebugInfo(); // print cached BPB/layout values

        private:
            BlockDevice* dev;
            FAT32_BPB bpb;
            uint32_t volumeStartLBA; // partition start (0 for superfloppy)
            uint32_t fatStartLBA;
            uint32_t dataStartLBA;
            bool mountedFlag;

            bool ReadSector(uint32_t lba, uint8_t* buf);
            uint32_t ClusterToLBA(uint32_t cluster);
            uint32_t DetectFAT32PartitionStart();
            // Helpers for simple file read
            bool ReadSectors(uint32_t lba, uint32_t count, uint8_t* buf);
            bool ReadCluster(uint32_t cluster, uint8_t* buf);
            uint32_t NextCluster(uint32_t cluster);
            bool FindShortNameInDirCluster(uint32_t dirCluster, const int8_t* shortName83, uint32_t& outStartCluster, uint32_t& outFileSize);
        public:
            virtual int32_t ReadFile(const int8_t* path, uint8_t* outBuf, uint32_t maxLen) override;
    };

}}

#endif
