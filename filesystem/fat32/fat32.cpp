//
// Created by linus on 03.07.25.
//

#include "fat32.h"

#include "../../include/log.h"
#include "../../kernel/include/memory.h"
#include "../../include/string.h"
#include "../../kernel/include/basic_renderer.h"
#include "../../include/path.h"

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

    uint8_t *AllocClusterBuffer(uint32_t clusterBytes) {
        return (uint8_t *) kernel::memory::request_pages((clusterBytes + 0xFFF) / 0x1000);
    }

    void FreeClusterBuffer(uint8_t *ptr, uint32_t clusterBytes) {
        kernel::memory::free_pages(ptr, (clusterBytes + 0xFFF) / 0x1000);
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
                    kernel::memory::free(chain);
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
        return chain; // Caller muss kernel::memory::free() machen
    }

    FileEntry *FileSystem::ReadDirectory(const char *path, size_t &outCount) const {
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
        size_t chainCount = 0;
        uint32_t *chain = GetClusterChain(cluster, chainCount);
        if (!chain) {
            kernel::memory::free(entries);
            return nullptr;
        }

        LFNBufferEntry lfnBuffer[20];
        size_t lfnCount = 0;

        for (size_t ci = 0; ci < chainCount; ++ci) {
            uint8_t clusterBuffer[bytesPerCluster()];
            if (!ReadCluster(chain[ci], clusterBuffer)) continue;

            size_t entryCountInCluster = bytesPerCluster() / sizeof(DirectoryEntry);
            for (size_t i = 0; i < entryCountInCluster; i++) {
                const auto entry = reinterpret_cast<DirectoryEntry *>(clusterBuffer + i * sizeof(DirectoryEntry));

                if (entry->name[0] == 0x00) break;
                if (entry->name[0] == 0xE5) continue;
                if (entry->attr == ATTR_VOLUME_ID) continue;


                if (entry->attr == ATTR_LONG_NAME) {
                    if (lfnCount < 20) {
                        lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName *>(entry);
                    }
                    continue;
                }

                entries[outCount].SetIsDir((entry->attr & ATTR_DIRECTORY) != 0);

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
                    entries[outCount].SetLongName(nameBuffer);
                    lfnCount = 0;
                } else {
                    entries[outCount].SetLongName(nullptr);
                    lfnCount = 0;
                }

                char shortName[13];
                ExtractShortName(entry->name, shortName, sizeof(shortName));

      //          Log::debug("readdir: %s : %s", shortName, entry->name);

                entries[outCount].SetShortName(shortName);
                entries[outCount].SetDirectoryEntry(*entry);

                outCount++;
                if (outCount >= READ_DIR_MAX_ENTRIES) {
                    // TODO when more entries then MAX_ENTRIES go out. have to look into this later
                    break;
                };
            }
            if (outCount >= READ_DIR_MAX_ENTRIES) {
                break;
            };
        }

        kernel::memory::free(chain);
        return entries;
    }


    bool FileSystem::ReadFile(Fat32Node* node, char* buffer, const size_t bufferSize, size_t &outFileSize) const {
        if (!node || !buffer) return false;

        const size_t fileSize = node->fileSize;
        if (fileSize > bufferSize) return false;

        size_t clusterCount = 0;
        uint32_t* clusters = GetClusterChain(node->cluster, clusterCount);
        if (!clusters) return false;

        size_t clusterSize = bytesPerCluster();
        uint32_t page_count = (clusterSize + 4095) / 4096;
        uint8_t* clusterBuffer = static_cast<uint8_t*>(kernel::memory::request_pages(page_count));
        if (!clusterBuffer) {
            kernel::memory::free(clusters);
            return false;
        }

        size_t bytesRead = 0;
        for (size_t i = 0; i < clusterCount && bytesRead < fileSize; ++i) {
            if (!ReadCluster(clusters[i], clusterBuffer)) {
                kernel::memory::free_pages(clusterBuffer, page_count);
                kernel::memory::free(clusters);
                return false;
            }

            size_t toCopy = clusterSize;
            if (bytesRead + toCopy > fileSize)
                toCopy = fileSize - bytesRead;

            memcpy(buffer + bytesRead, clusterBuffer, toCopy);
            bytesRead += toCopy;
        }

        buffer[min(bytesRead, bufferSize - 1)] = '\0';
        outFileSize = bytesRead;

        kernel::memory::free_pages(clusterBuffer, page_count);
        kernel::memory::free(clusters);

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

    void ExtractShortName(const unsigned char *rawName, char *shortNameBuffer, const size_t bufferSize) {
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

        uint32_t totalClusters = bpb.FATSize32 * bpb.bytesPerSector / 4;
        uint32_t sectorsInFAT = bpb.FATSize32;

        for (uint32_t sector = fatStart; sector < fatStart + sectorsInFAT; sector++) {
            if (!device->read(sector, 1, sectorData)) return 0;

            // per Sector 128 entries (512 Bytes / 4 Bytes per entry)
            for (uint32_t i = 0; i < 128; i++) {
                uint32_t cluster = (sector - fatStart) * 128 + i;
                if (cluster < 2) continue; // Cluster 0 and 1 reserved
                if (cluster >= totalClusters) break;

                uint32_t entry = *reinterpret_cast<uint32_t *>(sectorData + i * 4);
                if ((entry & 0x0FFFFFFF) == 0) {
                    return cluster;
                }
            }
        }
        return 0; // no free cluster :/
    }


    bool FileSystem::WriteDirectoryEntry(uint32_t dirCluster, const void *entryRaw) {
        size_t chainCount = 0;
        uint32_t *chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) {
            Log::Error("GetClusterChain failed");
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
                    kernel::memory::free(chain);
                    return true;
                }
            }
        }

        // kein Platz gefunden → neuen Cluster anfügen
        uint32_t lastCluster = chain[chainCount - 1];
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) {
            Log::Error("No free cluster to expand directory");
            kernel::memory::free(chain);
            return false;
        }

        if (!WriteFATEntry(lastCluster, newCluster)) {
            Log::Error("Failed to link new cluster in FAT");
            kernel::memory::free(chain);
            return false;
        }

        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) {
            Log::Error("Failed to terminate FAT chain");
            kernel::memory::free(chain);
            return false;
        }

        uint8_t zeroBuffer[bytesPerCluster()];
        memset(zeroBuffer, 0, sizeof(zeroBuffer));
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zeroBuffer);

        DirectoryEntry *first = reinterpret_cast<DirectoryEntry *>(zeroBuffer);
        memcpy(first, entryRaw, sizeof(DirectoryEntry));
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zeroBuffer);

        Log::Error("Expanded directory with new cluster");

        kernel::memory::free(chain);
        return true;
    }

    bool FileSystem::OverwriteDirectoryEntry(uint32_t cluster, size_t entryIndex, const DirectoryEntry *newEntry) {
        uint8_t buffer[bytesPerCluster()];
        if (!ReadCluster(cluster, buffer)) return false;

        DirectoryEntry *entries = reinterpret_cast<DirectoryEntry *>(buffer);
        entries[entryIndex] = *newEntry;

        return device->write(ClusterToSector(cluster), bpb.sectorsPerCluster, buffer);
    }

    bool FileSystem::CreateDirectory(Fat32Node *parentDir, const char *name) {
        if (!parentDir || !name || name[0] == '\0') return false;

        const uint32_t parentCluster = parentDir->cluster;

        // 1. Neuen Cluster reservieren
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        const uint32_t clusterSize = bytesPerCluster();
        uint8_t *zero = AllocClusterBuffer(clusterSize);
        if (!zero) return false;
        memset(zero, 0, clusterSize);

        // 2. "."- und ".."-Einträge setzen
        DirectoryEntry *dir = reinterpret_cast<DirectoryEntry *>(zero);

        // "." Eintrag
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, ".          ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = newCluster & 0xFFFF;
        dir->firstClusterHigh = (newCluster >> 16) & 0xFFFF;

        // ".." Eintrag
        dir++;
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, "..         ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = parentCluster & 0xFFFF;
        dir->firstClusterHigh = (parentCluster >> 16) & 0xFFFF;

        // 3. Leeren Cluster auf Platte schreiben
        bool writeOk = device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);
        FreeClusterBuffer(zero, clusterSize);
        if (!writeOk) return false;

        // 4. ShortName erzeugen
        char shortName[12] = {};
        if (!MakeShortName(name, shortName)) return false;

        // 5. Verzeichniseintrag mit LFN schreiben
        DirectoryEntry newEntry = {};
        memcpy(newEntry.name, shortName, 11);
        newEntry.attr = ATTR_DIRECTORY;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;

        return WriteDirectoryEntryWithLFN(parentCluster, name, shortName, &newEntry);
    }

    bool FileSystem::CreateFile(Fat32Node *parentDir, const char *name) {
        if (!parentDir || !name || name[0] == '\0') return false;

        const uint32_t parentCluster = parentDir->cluster;

        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false; // EOF

        const uint32_t clusterSize = bytesPerCluster();
        uint8_t *zero = AllocClusterBuffer(clusterSize);
        if (!zero) return false;
        memset(zero, 0, clusterSize);
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);
        FreeClusterBuffer(zero, clusterSize);

        char shortName[11];
        if (!MakeShortName(name, shortName)) return false;

        DirectoryEntry newEntry = {};
        memcpy(newEntry.name, shortName, 11);
        newEntry.attr = ATTR_ARCHIVE;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;

        return WriteDirectoryEntryWithLFN(parentDir->cluster, name, shortName, &newEntry);
    }

    bool FileSystem::WriteDirectoryEntryWithLFN(uint32_t dirCluster, const char *longName, const char *shortName,
                                                const DirectoryEntry *shortEntry) {
        const size_t nameLen = strlen(longName);
        const size_t entriesNeeded = (nameLen + 12) / 13;
        const size_t totalNeeded = entriesNeeded + 1;

        const size_t entrySize = sizeof(DirectoryEntry);
        const size_t clusterSize = bytesPerCluster();
        const size_t entriesPerCluster = clusterSize / entrySize;

        size_t clusterCount = 0;
        uint32_t *chain = GetClusterChain(dirCluster, clusterCount);
        if (!chain) return false;

        for (size_t ci = 0; ci < clusterCount; ++ci) {
            uint32_t cluster = chain[ci];
            uint8_t buffer[clusterSize];
            if (!ReadCluster(cluster, buffer)) continue;

            DirectoryEntry *entries = reinterpret_cast<DirectoryEntry *>(buffer);
            size_t freeCount = 0;
            size_t startIndex = 0;

            for (size_t i = 0; i < entriesPerCluster; ++i) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    if (freeCount == 0) startIndex = i;
                    freeCount++;
                    if (freeCount >= totalNeeded) {
                        // genug Platz

                        // UTF-16 Name vorbereiten
                        uint16_t nameBuffer[256] = {};
                        for (size_t j = 0; j < nameLen; ++j)
                            nameBuffer[j] = (uint8_t) longName[j];

                        uint8_t checksum = LFNChecksum(shortName);

                        for (int lfnIndex = (int) entriesNeeded - 1; lfnIndex >= 0; --lfnIndex) {
                            LongFileName lfn = {};
                            lfn.order = lfnIndex + 1;
                            if (lfnIndex == (int) entriesNeeded - 1)
                                lfn.order |= 0x40;
                            lfn.attr = ATTR_LONG_NAME;
                            lfn.type = 0;
                            lfn.checksum = checksum;
                            lfn.firstClusterLow = 0;

                            size_t offset = lfnIndex * 13;
                            auto copy = [&](uint16_t *dest, int count, size_t &off) {
                                for (int c = 0; c < count; ++c) {
                                    if (off < nameLen) {
                                        dest[c] = nameBuffer[off++];
                                    } else if (off == nameLen) {
                                        dest[c] = 0x0000;  // null-terminator
                                        off++;
                                    } else {
                                        dest[c] = 0xFFFF;  // padding
                                    }
                                }
                            };


                            copy(lfn.name1, 5, offset);
                            copy(lfn.name2, 6, offset);
                            copy(lfn.name3, 2, offset);

                            memcpy(&entries[startIndex + lfnIndex], &lfn, sizeof(LongFileName));
                        }

                        memcpy(&entries[startIndex + entriesNeeded], shortEntry, sizeof(DirectoryEntry));

                        if (!device->write(ClusterToSector(cluster), bpb.sectorsPerCluster, buffer)) {
                            kernel::memory::free(chain);
                            return false;
                        }

                        kernel::memory::free(chain);
                        return true;
                    }
                } else {
                    freeCount = 0;
                }
            }
        }

        // Kein Platz gefunden → Cluster erweitern und rekursiv nochmal versuchen
        uint32_t lastCluster = chain[clusterCount - 1];
        kernel::memory::free(chain);

        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;

        if (!WriteFATEntry(lastCluster, newCluster)) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        uint8_t zero[clusterSize];
        memset(zero, 0, sizeof(zero));
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero);

        return WriteDirectoryEntryWithLFN(dirCluster, longName, shortName, shortEntry);
    }

    bool FileSystem::DeleteDirectoryEntryInDirectory(uint32_t dirCluster, const char *name) {
        size_t chainCount = 0;
        uint32_t *chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) {
            return false;
        }

        bool deleted = false;

        for (size_t i = 0; i < chainCount; ++i) {
            uint8_t buffer[bytesPerCluster()];
            if (!ReadCluster(chain[i], buffer)) {
                continue;
            }

            DirectoryEntry *entries = reinterpret_cast<DirectoryEntry *>(buffer);
            size_t entryCount = bytesPerCluster() / sizeof(DirectoryEntry);

            LFNBufferEntry lfnBuffer[20];
            size_t lfnCount = 0;

            for (size_t j = 0; j < entryCount; ++j) {
                if (entries[j].name[0] == 0x00) break; // Ende des Verzeichnisses
                if (entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == ATTR_VOLUME_ID) continue;

                if (entries[j].attr == ATTR_LONG_NAME) {
                    if (lfnCount < 20) {
                        lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName *>(&entries[j]);
                    }
                    continue;
                }

                // LFN zusammensetzen (falls vorhanden)
                char fullName[256];
                fullName[0] = '\0';

                if (lfnCount > 0) {
                    // Sortieren
                    for (size_t a = 0; a < lfnCount - 1; a++) {
                        for (size_t b = 0; b < lfnCount - 1 - a; b++) {
                            if ((lfnBuffer[b].lfnEntry.order & 0x3F) > (lfnBuffer[b + 1].lfnEntry.order & 0x3F)) {
                                auto tmp = lfnBuffer[b];
                                lfnBuffer[b] = lfnBuffer[b + 1];
                                lfnBuffer[b + 1] = tmp;
                            }
                        }
                    }

                    size_t pos = 0;
                    for (size_t l = 0; l < lfnCount; ++l)
                        CopyLFNPart(&lfnBuffer[l].lfnEntry, fullName, pos, sizeof(fullName));

                    fullName[pos] = '\0';
                    lfnCount = 0;
                } else {
                    // Falls keine LFN, verwende Kurzname
                    ExtractShortName(entries[j].name, fullName, sizeof(fullName));
                }

                if (strcmp(fullName, name) == 0) {
                    entries[j].name[0] = 0xE5; // markiere als gelöscht

                    // LFN davor löschen
                    for (int k = (int) j - 1; k >= 0; --k) {
                        if ((entries[k].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
                        entries[k].name[0] = 0xE5;
                    }

                    device->write(ClusterToSector(chain[i]), bpb.sectorsPerCluster, buffer);

                    deleted = true;
                    break;
                }
            }

            if (deleted) break;
        }

        kernel::memory::free(chain);
        return deleted;
    }


    bool FileSystem::DeleteFile(Fat32Node *parentDir, const char *name) {
        uint32_t parentCluster = parentDir->cluster; // Cluster des Verzeichnisses

        size_t entryCount = 0;
        FileEntry *entries = ReadDirectory(parentCluster, entryCount);


        if (!entries) return false;

        for (size_t i = 0; i < entryCount; ++i) {
            if (strcmp(entries[i].GetName(), name) == 0 && !entries[i].isDir()) {
                uint32_t start = entries[i].GetFirstCluster();

                size_t count = 0;
                uint32_t *chain = GetClusterChain(start, count);
                if (chain) {
                    for (size_t j = 0; j < count; ++j)
                        WriteFATEntry(chain[j], 0);
                    kernel::memory::free(chain);
                }

                kernel::memory::free(entries);

                return DeleteDirectoryEntryInDirectory(parentCluster, name);
            }
        }

        kernel::memory::free(entries);
        return false;
    }

    bool FileSystem::RemoveDirectory(Fat32Node *parentDir, const char *name) {
        uint32_t parentCluster = parentDir->cluster; // korrekt initialisiert

        size_t entryCount = 0;
        FileEntry *entries = ReadDirectory(parentCluster, entryCount);
        if (!entries) return false;

        for (size_t i = 0; i < entryCount; ++i) {
            if (strcmp(entries[i].GetName(), name) == 0 && entries[i].isDir()) {
                uint32_t target = entries[i].GetFirstCluster();

                // Check if directory is empty (only "." and ".." allowed)
                size_t dirEntryCount = 0;
                FileEntry *dirEntries = ReadDirectory(target, dirEntryCount);
                if (!dirEntries || dirEntryCount > 2) {
                    kernel::memory::free(dirEntries);
                    kernel::memory::free(entries);
                    return false; // Not empty
                }

                // Free cluster chain
                size_t count = 0;
                uint32_t *chain = GetClusterChain(target, count);
                if (chain) {
                    for (size_t j = 0; j < count; ++j)
                        WriteFATEntry(chain[j], 0);
                    kernel::memory::free(chain);
                }

                kernel::memory::free(dirEntries);
                kernel::memory::free(entries);
                return DeleteDirectoryEntryInDirectory(parentCluster, name);
            }
        }

        kernel::memory::free(entries);
        return false;
    }

    // TODO funktioniert nicht für files
    bool FileSystem::Rename(Fat32Node *parentDir, const char *oldName, const char *newName) {
        if (!parentDir || !oldName || !newName || oldName[0] == '\0' || newName[0] == '\0') {
            return false;
        }

        uint32_t dirCluster = parentDir->cluster;

        size_t entryCount = 0;
        FileEntry *entries = ReadDirectory(dirCluster, entryCount);
        if (!entries || entryCount == 0) return false;

        size_t chainCount = 0;
        uint32_t *chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) {
            kernel::memory::free(entries);
            return false;
        }

        size_t entriesPerCluster = bytesPerCluster() / sizeof(DirectoryEntry);

        int foundIndex = -1;
        for (size_t i = 0; i < entryCount; ++i) {
            if (entries[i].GetName() && strcmp(entries[i].GetName(), oldName) == 0) {
                foundIndex = (int) i;
                break;
            }
        }

        if (foundIndex < 0) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        // Suche Shortentry auf Platte (Cluster + Index)
        uint32_t targetCluster = 0;
        int targetEntryIndex = -1;
        DirectoryEntry oldEntry = entries[foundIndex].GetDirectoryEntry();

        for (size_t ci = 0; ci < chainCount && targetEntryIndex < 0; ++ci) {
            uint8_t buffer[bytesPerCluster()];
            if (!ReadCluster(chain[ci], buffer)) continue;

            DirectoryEntry *dirEntries = reinterpret_cast<DirectoryEntry *>(buffer);
            for (size_t i = 0; i < entriesPerCluster; ++i) {
                if (dirEntries[i].name[0] == 0x00) break;
                if (dirEntries[i].name[0] == 0xE5) continue;
                if (dirEntries[i].attr == ATTR_VOLUME_ID) continue;

                if (dirEntries[i].fileSize == oldEntry.fileSize &&
                    dirEntries[i].firstClusterLow == oldEntry.firstClusterLow &&
                    dirEntries[i].firstClusterHigh == oldEntry.firstClusterHigh) {
                    targetCluster = chain[ci];
                    targetEntryIndex = (int) i;
                    break;
                }
            }
        }

        if (targetEntryIndex < 0) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        uint8_t buffer[bytesPerCluster()];
        if (!ReadCluster(targetCluster, buffer)) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        DirectoryEntry *dirEntries = reinterpret_cast<DirectoryEntry *>(buffer);

        // Alte LFN-Anzahl bestimmen
        int oldLFNCount = 0;
        for (int i = targetEntryIndex - 1; i >= 0; --i) {
            if ((dirEntries[i].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
            oldLFNCount++;
        }
        int oldTotalCount = oldLFNCount + 1;

        // Neue LFN-Anzahl berechnen
        int newLFNCount = (int) ((strlen(newName) + 12) / 13);
        int newTotalCount = newLFNCount + 1;

        int oldStartIndex = targetEntryIndex - oldLFNCount;

        // Platz prüfen: passen neue Einträge an dieselbe Stelle?
        bool canOverwrite = true;
        if (newTotalCount > oldTotalCount) {
            int extra = newTotalCount - oldTotalCount;
            for (int i = oldStartIndex - 1; i >= oldStartIndex - extra; --i) {
                if (i < 0 || (dirEntries[i].name[0] != 0x00 && dirEntries[i].name[0] != 0xE5)) {
                    canOverwrite = false;
                    break;
                }
            }
        }

        // Shortname erzeugen
        char newShortName[12] = {};
        if (!MakeShortName(newName, newShortName)) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        if (canOverwrite) {
            const size_t nameLen = strlen(newName);

            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i)
                dirEntries[i].name[0] = 0xE5;

            uint8_t checksum = LFNChecksum(newShortName);
            uint16_t nameBuffer[260] = {};
            for (size_t i = 0; i < nameLen; ++i) {
                nameBuffer[i] = (uint8_t) newName[i];
            }

            // LFN schreiben
            for (int i = 0; i < newLFNCount; ++i) {
                LongFileName lfn = {};
                lfn.order = i + 1;
                if (i == newLFNCount - 1) lfn.order |= 0x40;
                lfn.attr = ATTR_LONG_NAME;
                lfn.type = 0;
                lfn.checksum = checksum;
                lfn.firstClusterLow = 0;

                size_t offset = i * 13;
                auto copy = [&](uint16_t *dest, int count, size_t &off) {
                    for (int c = 0; c < count; ++c) {
                        if (off < nameLen) {
                            dest[c] = nameBuffer[off++];
                        } else if (off == nameLen) {
                            dest[c] = 0x0000;  // null-terminator
                            off++;
                        } else {
                            dest[c] = 0xFFFF;  // padding
                        }
                    }
                };

                copy(lfn.name1, 5, offset);
                copy(lfn.name2, 6, offset);
                copy(lfn.name3, 2, offset);

                memcpy(&dirEntries[oldStartIndex + i], &lfn, sizeof(LongFileName));
            }

            // Shortentry schreiben
            DirectoryEntry updated = oldEntry;
            memcpy(updated.name, newShortName, 11);
            dirEntries[oldStartIndex + newLFNCount] = updated;

            if (!device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster, buffer)) {
                kernel::memory::free(entries);
                kernel::memory::free(chain);
                return false;
            }
        } else {
            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i)
                dirEntries[i].name[0] = 0xE5;
            if (!device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster, buffer)) {
                kernel::memory::free(entries);
                kernel::memory::free(chain);
                return false;
            }

            // neuen Eintrag komplett schreiben
            DirectoryEntry newEntry = oldEntry;
            memcpy(newEntry.name, newShortName, 11);
            if (!WriteDirectoryEntryWithLFN(dirCluster, newName, newShortName, &newEntry)) {
                kernel::memory::free(entries);
                kernel::memory::free(chain);
                return false;
            }
        }

        kernel::memory::free(entries);
        kernel::memory::free(chain);
        return true;
    }


    bool MakeShortName(const char *input, char *output11) {
        memset(output11, ' ', 11);

        const char *dot = strrchr(input, '.');
        size_t nameLen = dot ? (size_t) (dot - input) : strlen(input);
        size_t extLen = dot ? strlen(dot + 1) : 0;

        if (nameLen == 0) return false;

        size_t outPos = 0;
        for (size_t i = 0; i < nameLen && outPos < 8; i++) {
            char c = input[i];
            if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
            output11[outPos++] = to_upper(c);
        }

        // Nur wenn der Name zu lang ist oder zu einer Kollision führt, ~1 anhängen
        // Hier: wenn >8 Zeichen → abkürzen und ~1 verwenden
        if (outPos > 8) {
            outPos = 6;
            output11[outPos++] = '~';
            output11[outPos++] = '1';
        }

        if (dot && extLen > 0) {
            size_t extPos = 0;
            for (size_t i = 0; i < 3 && dot[1 + i]; i++) {
                char c = dot[1 + i];
                if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
                output11[8 + extPos++] = to_upper(c);
            }
        }

        return true;
    }


    uint8_t LFNChecksum(const char *shortName) {
        uint8_t sum = 0;
        for (int i = 0; i < 11; i++) {
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortName[i];
        }
        return sum;
    }

    bool FileSystem::IsDir(uint32_t cluster) {
        size_t entryCount = 0;
        FAT32::FileEntry *entries = ReadDirectory(cluster, entryCount);
        if (!entries) return false;
        kernel::memory::free(entries);
        return true;
    }


    uint32_t FileSystem::ResolvePathToCluster(const char *path) const {
        if (path[0] != '/') return 0; // Nur absolute Pfade
        uint32_t currentCluster = GetRootCluster();

        char components[16][32]; // max 16 items, max 31 char + '\0' per item
        size_t compCount = split_path(path, components, 16);

        for (size_t i = 0; i < compCount; i++) {
            uint32_t nextCluster = FindEntryCluster(currentCluster, components[i]);
            if (nextCluster == 0) return 0;
            currentCluster = nextCluster;
        }

        return currentCluster;
    }

    uint32_t FileSystem::FindEntryCluster(uint32_t dirCluster, const char *givenName) const {
        size_t entryCount = 0;
        FileEntry *entries = ReadDirectory(dirCluster, entryCount);
        if (!entries) return 0;

        for (size_t i = 0; i < entryCount; i++) {
            const char *entryName = entries[i].GetName();

            if (strcmp(entryName, givenName) == 0) {
                uint32_t cluster = entries[i].GetFirstCluster();
                kernel::memory::free(entries);
                return cluster;
            }
        }

        kernel::memory::free(entries);
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
