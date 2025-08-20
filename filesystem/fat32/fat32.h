//
// Created by linus on 03.07.25.
//

#ifndef FAT32_CPP_H
#define FAT32_CPP_H
#include <cstdint>
#include <cstddef>
#include "../../include/log.h"
#include "../../include/string.h"
#include "../../kernel/include/basic_renderer.h"
#include "../../kernel/devices/blockdevice.h"
// https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf

struct Fat32Node;

namespace FAT32 {
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define READ_DIR_MAX_ENTRIES 256

    struct BPB_FAT32 {
        uint8_t jumpBoot[3];
        uint8_t oemName[8];
        uint16_t bytesPerSector;
        uint8_t sectorsPerCluster;
        uint16_t reservedSectorCount;
        uint8_t tableCount;
        uint16_t rootEntryCount;
        uint16_t totalSectors16;
        uint8_t mediaType;
        uint16_t tableSize16;
        uint16_t sectorsPerTrack;
        uint16_t headSideCount;
        uint32_t hiddenSectors;
        uint32_t totalSectors32;

        // FAT32 Extended BIOS Parameter Block (EBPB) Felder
        uint32_t FATSize32;
        uint16_t extFlags;
        uint16_t fatVersion;
        uint32_t rootCluster;
        uint16_t fsInfo;
        uint16_t backupBootSector;
        uint8_t reserved[12];
        uint8_t driveNumber;
        uint8_t reserved1;
        uint8_t bootSignature;
        uint32_t volumeID;
        uint8_t volumeLabel[11];
        uint8_t fsType[8];
    }__attribute__((packed));

    //  Section 6: Directory Structure FAT spec
    struct DirectoryEntry {
        unsigned char name[11]; // 8 + 3 Bytes
        uint8_t attr;
        uint8_t reserved;
        uint8_t creationTimeTenths;
        uint16_t creationTime;
        uint16_t creationDate;
        uint16_t lastAccessDate;
        uint16_t firstClusterHigh;
        uint16_t writeTime;
        uint16_t writeDate;
        uint16_t firstClusterLow;
        uint32_t fileSize;
    } __attribute__((packed));

    class FileEntry {
        char shortName[13] = {};
        char longName[256] = {};
        char formattedShortName[13] = {};
        DirectoryEntry dirEntry = {};
        bool _isDir = false;

    public:
        void SetLongName(const char *name) {
            if (!name || name[0] == '\0') {
                longName[0] = '\0';
            } else {
                strncpy(longName, name, sizeof(longName));
            }
        }

        void SetShortName(const char *name) {
            strncpy(shortName, name, sizeof(shortName));
        }

        void SetDirectoryEntry(const DirectoryEntry &entry) {
            dirEntry = entry;
            FormatShortName();
        }

        void SetIsDir(const bool value) {
            _isDir = value;
        }

        bool isDir() const {
            return _isDir;
        }

        const char *GetLongName() const {
            return longName;
        }

        const char *GetName() const {
            if (longName[0] != '\0') return longName;
            return formattedShortName;
        }

        const char *GetFormattedShortName() const {
            return formattedShortName;
        }

        DirectoryEntry GetDirectoryEntry() const {
            return dirEntry;
        }

        uint32_t GetFirstCluster() const {
            return (static_cast<uint32_t>(dirEntry.firstClusterHigh) << 16) | dirEntry.firstClusterLow;
        }

        uint32_t GetFileSize() const {
            return dirEntry.fileSize;
        }

        void FormatShortName();
    };

    struct LongFileName {
        uint8_t order;
        uint16_t name1[5];
        uint8_t attr;
        uint8_t type;
        uint8_t checksum;
        uint16_t name2[6];
        uint16_t firstClusterLow;
        uint16_t name3[2];
    } __attribute__((packed));

    struct LFNBufferEntry {
        LongFileName lfnEntry;
    };

    class FileSystem {
    public:
        explicit FileSystem(BlockDevice *device);

        bool is_valid() const;

        uint32_t GetRootCluster() const;

        bool IsDir(uint32_t cluster);

        uint32_t ResolvePathToCluster(const char *path) const;

        bool ReadFile(Fat32Node* node, char* buffer, const size_t bufferSize, size_t &outFileSize) const;

        FileEntry *ReadDirectory(const char *path, size_t &outCount) const;
        FileEntry *ReadDirectory(uint32_t cluster, size_t &outCount) const;

        bool CreateFile(Fat32Node *parentDir, const char *name);

        bool WriteDirectoryEntryWithLFN(uint32_t dirCluster, const char *longName, const char *shortName,
                                        const DirectoryEntry *shortEntry);

        bool DeleteDirectoryEntryInDirectory(uint32_t dirCluster, const char *name);

        bool CreateDirectory(Fat32Node *parentDir, const char *name);

        bool RemoveDirectory(Fat32Node *parentDir, const char *name);

        bool Rename(Fat32Node *parentDir, const char *oldName, const char *newName);

        bool DeleteFile(Fat32Node *parentDir, const char *name);

    private:
        BlockDevice *device;
        BPB_FAT32 bpb;
        bool valid;

        uint32_t fatStart;
        uint32_t fatSize;
        uint32_t dataStart;

        uint32_t ClusterToSector(uint32_t cluster) const;

        bool ReadCluster(uint32_t cluster, void *buffer) const;

        uint32_t bytesPerCluster() const;

        uint32_t GetFATEntry(uint32_t cluster) const;

        bool WriteFATEntry(uint32_t cluster, uint32_t value);

        uint32_t FindFreeCluster();

        uint32_t *GetClusterChain(uint32_t startCluster, size_t &outCount) const;

        bool WriteDirectoryEntry(uint32_t dirCluster, const void *entry);

        bool OverwriteDirectoryEntry(uint32_t cluster, size_t entryIndex, const DirectoryEntry *newEntry);

        uint32_t FindEntryCluster(uint32_t dirCluster, const char *givenName) const;
    };


    bool CopyLFNPart(const LongFileName *lfn, char *buffer, size_t &pos, size_t maxLen);

    void ExtractShortName(const unsigned char *rawName, char *shortNameBuffer, size_t bufferSize);

    bool MakeShortName(const char *input, char *output11);

    uint8_t LFNChecksum(const char *shortName);
}

#endif //FAT32_CPP_H
