//
// Created by linus on 03.07.25.
//

#ifndef FAT32_CPP_H
#define FAT32_CPP_H
#include "../../include/string.h"
#include "../../kernel/devices/blockdevice.h"
#include <cstdint>
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
        uint8_t jmpBoot[3]; // 0x00
        uint8_t OEMName[8]; // 0x03
        uint16_t bytesPerSector; // 0x0B
        uint8_t sectorsPerCluster; // 0x0D
        uint16_t reservedSectorCount; // 0x0E
        uint8_t tableCount; // 0x10
        uint16_t rootEntryCount; // 0x11
        uint16_t totalSectors16; // 0x13
        uint8_t mediaType; // 0x15
        uint16_t FATSize16; // 0x16
        uint16_t sectorsPerTrack; // 0x18
        uint16_t headSideCount; // 0x1A
        uint32_t hiddenSectors; // 0x1C
        uint32_t totalSectors32; // 0x20

        // FAT32 Extended BIOS Parameter Block
        uint32_t FATSize32; // 0x24
        uint16_t extFlags; // 0x28
        uint16_t FSVersion; // 0x2A
        uint32_t rootCluster; // 0x2C
        uint16_t FSInfo; // 0x30
        uint16_t backupBootSector; // 0x32
        uint8_t reserved[12]; // 0x34
        uint8_t driveNumber; // 0x40
        uint8_t reserved1; // 0x41
        uint8_t bootSignature; // 0x42
        uint32_t volumeID; // 0x43
        uint8_t volumeLabel[11]; // 0x47
        uint8_t fsType[8]; // 0x52
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
        size_t indexInCluster = 0;

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
            FormatShortName();
        }

        void SetDirectoryEntry(const DirectoryEntry &entry) {
            dirEntry = entry;
            FormatShortName();
        }

        void SetIsDir(const bool value) {
            _isDir = value;
        }

        [[nodiscard]] bool isDir() const {
            return _isDir;
        }

        [[nodiscard]] const char *GetLongName() const {
            return longName;
        }

        [[nodiscard]] const char *GetName() const {
            if (longName[0] != '\0') return longName;
            return formattedShortName;
        }

        [[nodiscard]] const char *GetFormattedShortName() const {
            return formattedShortName;
        }

        [[nodiscard]] const DirectoryEntry GetDirectoryEntry() const {
            return dirEntry;
        }

        [[nodiscard]] uint32_t GetFirstCluster() const {
            return (static_cast<uint32_t>(dirEntry.firstClusterHigh) << 16) | dirEntry.firstClusterLow;
        }

        [[nodiscard]] uint32_t GetFileSize() const {
            return dirEntry.fileSize;
        }

        void SetFileSize(uint32_t size) {
            dirEntry.fileSize = size;
        }

        void SetIndexInCluster(size_t index) {
            indexInCluster = index;
        }

        [[nodiscard]] size_t GetIndexInCluster() const {
            return indexInCluster;
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

        ~FileSystem();

        [[nodiscard]] bool is_valid() const;

        [[nodiscard]] uint32_t GetRootCluster() const;

        bool IsDir(uint32_t cluster);

        uint32_t ResolvePathToCluster(const char *path) const;

        bool ReadFile(Fat32Node *node, void *buffer, size_t len, size_t &outActual, size_t offset = 0) const;

        bool WriteFile(Fat32Node *node, const void *buffer, size_t len);

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

        static size_t FindFirstLFNIndex(const FAT32::FileEntry *entries, size_t shortNameIndex);

    private:
        BlockDevice *device;
        BPB_FAT32 bpb;
        bool valid;

        uint32_t sectorSize;
        uint32_t fatStart;
        uint32_t fatSize;
        uint32_t dataStart;

        [[nodiscard]] uint32_t ClusterToSector(uint32_t cluster) const;

        bool ReadCluster(uint32_t cluster, void *buffer) const;

        bool WriteCluster(uint32_t cluster, const void *data, size_t len, size_t offset) const;

        [[nodiscard]] uint32_t bytesPerCluster() const;

        [[nodiscard]] uint32_t GetFATEntry(uint32_t cluster) const;

        bool WriteFATEntry(uint32_t cluster, uint32_t value);

        uint32_t FindFreeCluster();

        uint32_t *GetClusterChain(uint32_t startCluster, size_t &outCount) const;

        bool WriteDirectoryEntry(uint32_t dirCluster, const void *entry);

        bool UpdateDirectoryEntryWithLFN(uint32_t parentCluster, size_t firstLFNIndex,
                                         const char *longName, const char *shortName,
                                         const DirectoryEntry *shortEntry) const;

        bool OverwriteDirectoryEntry(uint32_t cluster, size_t entryIndex, const DirectoryEntry *newEntry) const;

        uint32_t FindEntryCluster(uint32_t dirCluster, const char *givenName) const;
    };


    bool CopyLFNPart(const LongFileName *lfn, char *buffer, size_t &pos, size_t maxLen);

    void ExtractShortName(const unsigned char *rawName, char *shortNameBuffer, size_t bufferSize);

    bool MakeShortName(const char *input, char *output11);

    uint8_t LFNChecksum(const char *shortName);
}

#endif //FAT32_CPP_H
