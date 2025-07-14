//
// Created by linus on 03.07.25.
//

#include "fat32.h"

#include "../include/log.h"
#include "../kernel/include/memory.h"
#include "../kernel/memory/heap.h"
#include "../include/string.h"
#include "../kernel/include/basic_renderer.h"
#include "../kernel/include/page_frame_allocator.h"
#include "../include/path.h"

namespace FAT32 {
    FileSystem::FileSystem(BlockDevice *device) {
        this->device = device;
        this->valid = false;

        uint8_t sector[512];
        if (!device->read(0, 1, sector)) {
            return;
        }

        memcpy(&bpb, sector, sizeof(BPB_FAT32));

        if (bpb.bytesPerSector != 512) return;
        if (bpb.tableCount < 1 || bpb.sectorsPerCluster == 0) return;
        if (memcmp(bpb.fsType, "FAT32   ", 8) != 0) return;

        fatStart = bpb.reservedSectorCount;
        fatSize = bpb.FATSize32;
        dataStart = fatStart + (bpb.tableCount * fatSize);

        this->valid = true;
    }

    uint8_t* AllocClusterBuffer(uint32_t clusterBytes) {
        return (uint8_t*)global_allocator.request_pages((clusterBytes + 0xFFF) / 0x1000);
    }

    void FreeClusterBuffer(uint8_t* ptr, uint32_t clusterBytes) {
        global_allocator.free_pages(ptr, (clusterBytes + 0xFFF) / 0x1000);
    }


    bool FileSystem::is_valid() const {
        return valid;
    }

    uint32_t FileSystem::GetRootCluster() const {
        return bpb.rootCluster;
    }

    uint32_t FileSystem::ClusterToSector(const uint32_t cluster) const {
        return dataStart + (cluster - 2) * bpb.sectorsPerCluster;
    }

    bool FileSystem::ReadCluster(const uint32_t cluster, void *buffer) const {
        const uint32_t sector = ClusterToSector(cluster);
        return device->read(sector, bpb.sectorsPerCluster, buffer);
    }

    uint32_t FileSystem::bytesPerCluster() const {
        return bpb.bytesPerSector * bpb.sectorsPerCluster;
    }

    uint32_t FileSystem::GetFATEntry(const uint32_t cluster) const {
        // FAT-Entry: 4 byte, fatStart in sector, 512 byte/sector
        const uint32_t fatOffset = cluster * 4;
        const uint32_t sector = fatStart + (fatOffset / bpb.bytesPerSector);
        const uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

        uint8_t sectorData[512];
        if (!device->read(sector, 1, sectorData)) return 0x0FFFFFFF; // Fehler = EOF

        const uint32_t entry = *reinterpret_cast<uint32_t *>(sectorData + offsetInSector);
        return entry & 0x0FFFFFFF;
    }

    uint32_t *FileSystem::GetClusterChain(const uint32_t startCluster, size_t &outCount) const {
        size_t capacity = 16;
        size_t count = 0;
        size_t size = sizeof(uint32_t) * capacity;
        uint32_t *chain = (uint32_t *) malloc(size);

        uint32_t cluster = startCluster;

        while (cluster < 0x0FFFFFF8) {
            if (count == capacity) {
                // double size
                capacity *= 2;
                uint32_t *newChain = static_cast<uint32_t *>(realloc(chain, size, sizeof(uint32_t) * capacity));
                size = sizeof(uint32_t) * capacity;
                if (!newChain) {
                    free(chain);
                    outCount = 0;
                    return nullptr;
                }
                chain = newChain;
            }

            chain[count++] = cluster;
            cluster = GetFATEntry(cluster);
            if (cluster == 0x0FFFFFFF || cluster == 0) break;
        }

        outCount = count;
        return chain; // Caller muss free() machen
    }

    FileEntry* FileSystem::ReadDirectory(const char* path, size_t& outCount) const {
        outCount = 0;

        uint32_t cluster = ResolvePathToCluster(path);
        if (cluster == 0) return nullptr;

        return ReadDirectory(cluster, outCount);
    }


    FileEntry *FileSystem::ReadDirectory(uint32_t cluster, size_t &outCount) const {
        FileEntry *entries = static_cast<FileEntry *>(malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));
        if (!entries) {
            outCount = 0;
            return nullptr;
        }

        outCount = 0;

        uint8_t clusterBuffer[bpb.bytesPerSector * bpb.sectorsPerCluster];

        if (!ReadCluster(cluster, clusterBuffer)) {
            free(entries);
            outCount = 0;
            return nullptr;
        }

        size_t entryCountInCluster = (bpb.bytesPerSector * bpb.sectorsPerCluster) / sizeof(DirectoryEntry);

        LFNBufferEntry lfnBuffer[20];
        size_t lfnCount = 0;

        for (size_t i = 0; i < entryCountInCluster; i++) {
            const auto entry = reinterpret_cast<DirectoryEntry *>(clusterBuffer + i * sizeof(DirectoryEntry));

            if (entry->name[0] == 0x00) break; // Ende des Verzeichnisses
            if (entry->name[0] == 0xE5) continue; // deleted entry
            if (entry->attr == ATTR_VOLUME_ID) continue; // skip volume label


            if (entry->attr == ATTR_LONG_NAME) {
                if (lfnCount < 20) {
                    lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName *>(entry);
                }
                continue;
            }

            // set isDir flag
            entries[outCount].SetIsDir((entry->attr & ATTR_DIRECTORY) != 0);

            // Hier eventuell weitere Filter für versteckte/Systemdateien, falls gewünscht


            if (lfnCount > 0) {
                for (size_t j = 0; j < lfnCount - 1; j++) {
                    for (size_t k = 0; k < lfnCount - 1 - j; k++) {
                        if ((lfnBuffer[k].lfnEntry.order & 0x3F) > (lfnBuffer[k + 1].lfnEntry.order & 0x3F)) {
                            const auto tmp = lfnBuffer[k];
                            lfnBuffer[k] = lfnBuffer[k + 1];
                            lfnBuffer[k + 1] = tmp;
                        }
                    }
                }

                char nameBuffer[256];
                size_t pos = 0;
                for (size_t j = 0; j < lfnCount; j++) {
                    CopyLFNPart(&lfnBuffer[j].lfnEntry, nameBuffer, pos, sizeof(nameBuffer));
                }
                nameBuffer[pos] = '\0';

                //   strncpy(entries[outCount].longName, nameBuffer, sizeof(entries[outCount].longName));
                entries[outCount].SetLongName(nameBuffer);
                lfnCount = 0;
            } else {
                entries[outCount].SetLongName(nullptr);
                lfnCount = 0;
            }

            char shortName[13];
            ExtractShortName(entry->name, shortName, sizeof(shortName));
            entries[outCount].SetShortName(shortName);

            entries[outCount].SetDirectoryEntry(*entry);
            outCount++;
            if (outCount >= READ_DIR_MAX_ENTRIES) {
                // cap cuz we would need to realloc. TODO only shows READ_DIR_MAX_ENTRIES. if more we break out
                break;
            }
        }

        return entries;
    }

    bool FileSystem::ReadFile(const char *filename, char *buffer, const size_t bufferSize, size_t &outFileSize) const {
        size_t entryCount = 0;
        const auto entries = ReadDirectory(GetRootCluster(), entryCount);
        if (!entries) {
            return false;
        }

        const FileEntry *fileEntry = nullptr;
        for (size_t i = 0; i < entryCount; i++) {
            if (strcmp(entries[i].GetLongName(), filename) == 0 && !entries[i].isDir()) {
                fileEntry = &entries[i];
                break;
            }
        }

        if (!fileEntry) {
            free(entries);
            return false;
        }

        const uint32_t startCluster = fileEntry->GetFirstCluster();
        const size_t fileSize = fileEntry->GetFileSize();

        if (fileSize > bufferSize) {
            // Buffer zu klein
            free(entries);
            return false;
        }

        size_t clusterCount = 0;
        uint32_t *clusters = GetClusterChain(startCluster, clusterCount);
        if (!clusters) {
            free(entries);
            return false;
        }

        uint32_t clusterSize = bytesPerCluster();
        size_t pagesNeeded = (clusterSize + 4095) / 4096;
        uint8_t *clusterBuffer = static_cast<uint8_t *>(global_allocator.request_pages(pagesNeeded));
        if (!clusterBuffer) {
            free(clusters);
            free(entries);
            return false;
        }

        size_t bytesRead = 0;
        for (size_t i = 0; i < clusterCount && bytesRead < fileSize; i++) {
            if (!ReadCluster(clusters[i], clusterBuffer)) {
                free(clusterBuffer);
                free(clusters);
                free(entries);
                return false;
            }

            size_t toCopy = clusterSize;
            if (bytesRead + toCopy > fileSize) {
                toCopy = fileSize - bytesRead;
            }

            memcpy(buffer + bytesRead, clusterBuffer, toCopy);
            bytesRead += toCopy;
        }

        if (bytesRead < bufferSize) {
            buffer[bytesRead] = '\0';
        } else if (bufferSize > 0) {
            buffer[bufferSize - 1] = '\0';
        }

        outFileSize = bytesRead;
        free(clusterBuffer);
        free(clusters);
        free(entries);
        return true;
    }


    bool CopyLFNPart(const LongFileName *lfn, char *buffer, size_t &pos, const size_t maxLen) {
        auto copyChars = [&](const uint16_t *src, const size_t count) {
            for (size_t i = 0; i < count; i++) {
                if (src[i] == 0x0000 || src[i] == 0xFFFF) return false;
                if (pos >= maxLen - 1) return false;
                buffer[pos++] = static_cast<char>(src[i] & 0xFF);
            }
            return true;
        };

        if (!copyChars(lfn->name1, 5)) return false;
        if (!copyChars(lfn->name2, 6)) return false;
        if (!copyChars(lfn->name3, 2)) return false;
        return true;
    }

    void ExtractShortName(const char *rawName, char *shortNameBuffer, const size_t bufferSize) {
        if (bufferSize < 13) return; // 8+3 + null
        memcpy(shortNameBuffer, rawName, 11);
        for (int i = 10; i >= 0; i--) {
            if (shortNameBuffer[i] == ' ') shortNameBuffer[i] = '\0';
            else break;
        }

        shortNameBuffer[12] = '\0';
    }

    bool FileSystem::WriteFATEntry(uint32_t cluster, uint32_t value) {
        uint32_t fatOffset = cluster * 4;
        uint32_t sector = fatStart + (fatOffset / bpb.bytesPerSector);
        uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

        uint8_t sectorData[512];
        if (!device->read(sector, 1, sectorData)) return false;

        *reinterpret_cast<uint32_t *>(sectorData + offsetInSector) = value;

        return device->write(sector, 1, sectorData);
    }

    uint32_t FileSystem::FindFreeCluster() {
        uint8_t sectorData[512];
        for (uint32_t cluster = 2; cluster < bpb.FATSize32 * bpb.bytesPerSector / 4; cluster++) {
            uint32_t fatOffset = cluster * 4;
            uint32_t sector = fatStart + (fatOffset / bpb.bytesPerSector);
            uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

            if (!device->read(sector, 1, sectorData)) return 0;

            uint32_t entry = *reinterpret_cast<uint32_t *>(sectorData + offsetInSector);
            if ((entry & 0x0FFFFFFF) == 0) {
                return cluster;
            }
        }
        return 0;
    }

    bool FileSystem::WriteDirectoryEntry(uint32_t dirCluster, const void *entryRaw) {
        size_t chainCount = 0;
        uint32_t *chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) {
            Log::Error("GetClusterChain failed\n");
            return false;
        }

        for (size_t i = 0; i < chainCount; i++) {
            uint32_t cluster = chain[i];
            uint8_t buffer[bytesPerCluster()];
            if (!ReadCluster(cluster, buffer)) continue;

            size_t count = bytesPerCluster() / sizeof(DirectoryEntry);
            for (size_t j = 0; j < count; j++) {
                DirectoryEntry *ent = reinterpret_cast<DirectoryEntry *>(buffer + j * sizeof(DirectoryEntry));
                if (ent->name[0] == 0x00 || ent->name[0] == 0xE5) {
                    memcpy(ent, entryRaw, sizeof(DirectoryEntry));
                    device->write(ClusterToSector(cluster), bpb.sectorsPerCluster, buffer);
                    free(chain);
                    return true;
                }
            }
        }

        // kein Platz gefunden → neuen Cluster anfügen
        uint32_t lastCluster = chain[chainCount - 1];
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) {
            Log::Error("No free cluster to expand directory");
            free(chain);
            return false;
        }

        if (!WriteFATEntry(lastCluster, newCluster)) {
            Log::Error("Failed to link new cluster in FAT");
            free(chain);
            return false;
        }

        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) {
            Log::Error("Failed to terminate FAT chain");
            free(chain);
            return false;
        }

        uint8_t zeroBuffer[bytesPerCluster()];
        memset(zeroBuffer, 0, sizeof(zeroBuffer));
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zeroBuffer);

        DirectoryEntry *first = reinterpret_cast<DirectoryEntry *>(zeroBuffer);
        memcpy(first, entryRaw, sizeof(DirectoryEntry));
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zeroBuffer);

        Log::Error("Expanded directory with new cluster");

        free(chain);
        return true;
    }


    bool FileSystem::CreateDirectory(const char *name) {
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        uint32_t clusterSize = bytesPerCluster();
        uint8_t* zero = AllocClusterBuffer(clusterSize);
        if (!zero) return false;

        memset(zero, 0, clusterSize);

        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);

        DirectoryEntry *dir = reinterpret_cast<DirectoryEntry *>(zero);
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, ".          ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = newCluster & 0xFFFF;
        dir->firstClusterHigh = (newCluster >> 16) & 0xFFFF;

        dir++;
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, "..         ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = GetRootCluster() & 0xFFFF;
        dir->firstClusterHigh = (GetRootCluster() >> 16) & 0xFFFF;

        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);

        DirectoryEntry newEntry = {};
        char shortName[11];
        if (!MakeShortName(name, shortName)) return false;

        WriteLFNEntries(name, shortName, GetRootCluster());

        memcpy(newEntry.name, shortName, 11);
        newEntry.attr = ATTR_DIRECTORY;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;

        FreeClusterBuffer(zero, clusterSize);
        return WriteDirectoryEntry(GetRootCluster(), &newEntry);
    }

    bool FileSystem::CreateFile(const char *name) {
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        uint32_t clusterSize = bytesPerCluster();
        uint8_t* zero = AllocClusterBuffer(clusterSize);
        if (!zero) return false;

        memset(zero, 0, clusterSize);
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);

        DirectoryEntry newEntry = {};

        char shortName[11];
        if (!MakeShortName(name, shortName)) return false;
        memcpy(newEntry.name, shortName, 11);
        WriteLFNEntries(name, shortName, GetRootCluster());
        newEntry.attr = ATTR_ARCHIVE;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;

        FreeClusterBuffer(zero, clusterSize);
        return WriteDirectoryEntry(GetRootCluster(), &newEntry);
    }

    bool MakeShortName(const char *input, char *output11) {
        memset(output11, ' ', 11);

        const char *dot = strrchr(input, '.');
        size_t nameLen = dot ? (size_t) (dot - input) : strlen(input);
        size_t extLen = dot ? strlen(dot + 1) : 0;

        if (nameLen == 0) return false;

        size_t copyLen = nameLen > 6 ? 6 : nameLen;

        size_t outPos = 0;
        for (size_t i = 0; i < copyLen && outPos < 8; i++) {
            char c = input[i];
            if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
            output11[outPos++] = to_upper(c);
        }

        if (outPos <= 6) {
            output11[outPos++] = '~';
            output11[outPos++] = '1';
        }

        if (dot && extLen > 0) {
            for (size_t i = 0; i < 3 && dot[1 + i]; i++) {
                output11[8 + i] = to_upper(dot[1 + i]);
            }
        }

        return true;
    }

    uint8_t LFNChecksum(const char *shortName) {
        uint8_t sum = 0;
        for (int i = 0; i < 11; i++) {
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t) shortName[i];
        }
        return sum;
    }

    bool FileSystem::WriteLFNEntries(const char *longName, const char *shortName, uint32_t dirCluster) {
        const size_t nameLen = strlen(longName);
        const size_t entriesNeeded = (nameLen + 12) / 13;
        uint8_t checksum = LFNChecksum(shortName);

        uint16_t nameBuffer[256];
        memset(nameBuffer, 0xFFFF, sizeof(nameBuffer));
        for (size_t i = 0; i < nameLen; i++) {
            nameBuffer[i] = (uint8_t) longName[i];
        }
        nameBuffer[nameLen] = 0x0000;


        for (int i = entriesNeeded - 1; i >= 0; i--) {
            LongFileName lfn = {};
            lfn.order = i + 1;
            if (i == entriesNeeded - 1)
                lfn.order |= 0x40; // last entry
            lfn.attr = 0x0F;
            lfn.type = 0;
            lfn.checksum = checksum;
            lfn.firstClusterLow = 0;

            auto copy = [&](uint16_t *dest, int count, size_t &offset) {
                for (int j = 0; j < count; j++) {
                    if (offset < nameLen)
                        dest[j] = nameBuffer[offset++];
                    else
                        dest[j] = 0x0000;
                }
            };


            size_t offset = i * 13;
            copy(lfn.name1, 5, offset);
            copy(lfn.name2, 6, offset);
            copy(lfn.name3, 2, offset);


            if (!WriteDirectoryEntry(dirCluster, &lfn)) {
                return false;
            }
        }

        return true;
    }

    uint32_t FileSystem::ResolvePathToCluster(const char* path) const {
        if (path[0] != '/') return 0; // Nur absolute Pfade
        uint32_t currentCluster = GetRootCluster();

        char components[16][32]; // max 16 items, max 31 char + '\0' per item
        size_t compCount = SplitPath(path, components, 16);

        for (size_t i = 0; i < compCount; i++) {
            uint32_t nextCluster = FindEntryCluster(currentCluster, components[i]);
            if (nextCluster == 0) return 0;
            currentCluster = nextCluster;
        }

        return currentCluster;
    }

    uint32_t FileSystem::FindEntryCluster(uint32_t dirCluster, const char* givenName) const {
        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(dirCluster, entryCount);
        if (!entries) return 0;

        for (size_t i = 0; i < entryCount; i++) {
            const char* entryName = entries[i].GetName();

            if (strcmp(entryName, givenName) == 0) {
                uint32_t cluster = entries[i].GetFirstCluster();
                free(entries);
                return cluster;
            }
        }

        free(entries);
        return 0;
    }

    void FileEntry::FormatShortName() {
        char name[9] = {};
        char ext[4] = {};

        memcpy(name, shortName, 8);
        for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

        memcpy(ext, shortName + 8, 3);
        for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';

        size_t nameLen = strlen(name);
        memcpy(formattedShortName, name, nameLen);

        if (ext[0] != '\0') {
            formattedShortName[nameLen] = '.';
            memcpy(formattedShortName + nameLen + 1, ext, strlen(ext));
            formattedShortName[nameLen + 1 + strlen(ext)] = '\0';
        } else {
            formattedShortName[nameLen] = '\0';
        }
    }

}
