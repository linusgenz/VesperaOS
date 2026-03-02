//
// Created by linus on 03.07.25.
//

#include "fat32.h"

#include <sort.h>

#include "../../include/log.h"
#include "../../include/path.h"
#include "../../include/string.h"
#include "fat32_lfn.h"
#include "fat32_time.h"
#include "fat32_vfs_adapter.h"

namespace FAT32 {
    // ============================================================================
    // Helper Functions - Memory Management
    // ============================================================================

    static uint8_t* AllocClusterBuffer(uint32_t clusterBytes) {
        const size_t pages = (clusterBytes + 0xFFF) / 0x1000;
        auto* page = static_cast<uint8_t*>(kernel::memory::request_pages(pages));
        if (page) memset(page, 0, pages * 0x1000);
        return page;
    }

    static void FreeClusterBuffer(uint8_t* ptr, uint32_t clusterBytes) {
        kernel::memory::free_pages(ptr, (clusterBytes + 0xFFF) / 0x1000);
    }

    // ============================================================================
    // FileSystem Implementation
    // ============================================================================

    FileSystem::FileSystem(BlockDevice* device)
        : device(device)
        , fs_valid(false)
        , sectorSize(device->get_sector_size())
        , nextFreeCluster(2)
        , cacheAccessCounter(0) {
        uint8_t sector[512];
        if (!device->read(0, 1, sector, 512)) {
            Log::Error("[FAT32] Failed to read first sector");
            return;
        }

        memcpy(&bpb, sector, sizeof(BPB_FAT32));

        if (bpb.tableCount < 1 || bpb.sectorsPerCluster == 0) return;
        if (!probe_fs()) return;

        for (auto& i : fatCache) {
            i.sector = 0;
            i.lastUsed = 0;
            i.valid = false;
        }

        LoadFSInfo();

        const uint32_t totalSectors = (bpb.totalSectors16 != 0) ? bpb.totalSectors16 : bpb.totalSectors32;

        const uint32_t dataSectors = totalSectors - (bpb.reservedSectorCount + (bpb.tableCount * bpb.FATSize32));

        clusterCount = dataSectors / bpb.sectorsPerCluster;

        dataStart = bpb.reservedSectorCount + (bpb.tableCount * bpb.FATSize32);

        fs_valid = true;
    }

    FileSystem::~FileSystem() {
        WriteFSInfo();
    };

    bool FileSystem::probe_fs() const {
        const uint32_t totalSectors = (bpb.totalSectors16 != 0) ? bpb.totalSectors16 : bpb.totalSectors32;

        const uint32_t rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;

        const uint32_t dataSectors =
            totalSectors - (bpb.reservedSectorCount + (bpb.tableCount * bpb.FATSize32) + rootDirSectors);

        const uint32_t clusterCount = dataSectors / bpb.sectorsPerCluster;

        const bool isFat32 = bpb.rootEntryCount == 0 && bpb.FATSize16 == 0 && clusterCount >= 65525;

        return isFat32;
    }

    bool FileSystem::is_valid() const {
        return fs_valid;
    }
    uint32_t FileSystem::GetRootCluster() const {
        return bpb.rootCluster;
    }
    uint32_t FileSystem::bytesPerCluster() const {
        return bpb.bytesPerSector * bpb.sectorsPerCluster;
    }

    uint32_t FileSystem::ClusterToSector(const uint32_t cluster) const {
        return dataStart + (cluster - 2) * bpb.sectorsPerCluster;
    }

    // ============================================================================
    // Cache
    // ============================================================================

    bool FileSystem::ReadFATSector(uint32_t fat_sector, uint8_t* buffer) const {
        cacheAccessCounter++;

        // Search for cached entry
        for (size_t i = 0; i < FAT_CACHE_SIZE; ++i) {
            if (fatCache[i].valid && fatCache[i].sector == fat_sector) {
                cacheStats.hits++;
                fatCache[i].lastUsed = cacheAccessCounter;
                memcpy(buffer, fatCache[i].data, bpb.bytesPerSector);
                return true;
            }
        }

        // Cache miss - read from disk
        cacheStats.misses++;
        if (!device->read(fat_sector, 1, buffer, bpb.bytesPerSector)) return false;

        // Find slot for new entry (LRU replacement)
        size_t replaceIdx = 0;
        uint32_t oldestAccess = fatCache[0].valid ? fatCache[0].lastUsed : 0;

        for (size_t i = 0; i < FAT_CACHE_SIZE; ++i) {
            if (!fatCache[i].valid) {
                // Found empty slot
                replaceIdx = i;
                break;
            }

            if (fatCache[i].lastUsed < oldestAccess) {
                oldestAccess = fatCache[i].lastUsed;
                replaceIdx = i;
            }
        }

        // Store in cache
        fatCache[replaceIdx].sector = fat_sector;
        memcpy(fatCache[replaceIdx].data, buffer, bpb.bytesPerSector);
        fatCache[replaceIdx].lastUsed = cacheAccessCounter;
        fatCache[replaceIdx].valid = true;

        return true;
    }

    void FileSystem::InvalidateFATCache() const {
        for (auto& i : fatCache) {
            i.valid = false;
        }
    }

    void FileSystem::InvalidateFATCacheSector(uint32_t sector) const {
        for (auto& i : fatCache) {
            if (i.valid && i.sector == sector) {
                i.valid = false;
            }
        }
    }

    // ============================================================================
    // FSInfo
    // ============================================================================

    bool FileSystem::LoadFSInfo() {
        freeClusterCount = 0xFFFFFFFF;
        nextFreeCluster = 2;

        if (bpb.FSInfo == 0) return false;

        FSINFO fsinfo{};
        if (!device->read(bpb.FSInfo, 1, &fsinfo, sizeof(FSINFO))) return false;

        if (fsinfo.LeadSig != 0x41615252) return false;
        if (fsinfo.StrucSig != 0x61417272) return false;
        if (fsinfo.TrailSig != 0xAA550000) return false;

        freeClusterCount = fsinfo.Free_Count;
        nextFreeCluster = fsinfo.Nxt_Free;

        if (nextFreeCluster < 2) nextFreeCluster = 2;

        return true;
    }

    void FileSystem::WriteFSInfo() const {
        if (bpb.FSInfo == 0) return;

        FSINFO fsinfo{};
        fsinfo.LeadSig = 0x41615252;
        fsinfo.StrucSig = 0x61417272;
        fsinfo.Free_Count = freeClusterCount;
        fsinfo.Nxt_Free = nextFreeCluster;
        fsinfo.TrailSig = 0xAA550000;

        device->write(bpb.FSInfo, 1, &fsinfo, sizeof(FSINFO));
    }

    // ============================================================================
    // Cluster I/O Operations
    // ============================================================================

    ssize_t FileSystem::ReadCluster(const uint32_t cluster, void* buffer, size_t buffer_size) const {
        const uint32_t sector = ClusterToSector(cluster);
        return device->read(sector, bpb.sectorsPerCluster, buffer, buffer_size);
    }

    bool FileSystem::WriteCluster(uint32_t cluster, const void* data, size_t len, size_t offset) const {
        if (!data || len == 0) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        if (len + offset > clusterBytes) return false;

        uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
        if (!clusterBuffer) return false;

        // Read existing data if partial write
        if (len < clusterBytes || offset > 0) {
            if (!ReadCluster(cluster, clusterBuffer, clusterBytes)) {
                FreeClusterBuffer(clusterBuffer, clusterBytes);
                return false;
            }
        }

        memcpy(clusterBuffer + offset, data, len);

        const uint32_t sector = ClusterToSector(cluster);
        bool ok = device->write(sector, bpb.sectorsPerCluster, clusterBuffer, clusterBytes);

        FreeClusterBuffer(clusterBuffer, clusterBytes);
        return ok;
    }

    // ============================================================================
    // FAT Table Operations
    // ============================================================================

    bool FileSystem::IsValidFATEntry(uint32_t value) const {
        value &= 0x0FFFFFFF;

        if (value == 0) return true;            // free
        if (value >= 0x0FFFFFF8) return true;   // EOF
        if (value == 0x0FFFFFF7) return false;  // bad
        if (value < 2) return false;
        if (value >= clusterCount + 2) return false;

        return true;
    }

    uint32_t FileSystem::ReadFATEntryRaw(uint32_t fatSector, uint32_t offset) const {
        uint8_t buf[1024];  // max 2 sectors
        const uint32_t sector_size = bpb.bytesPerSector;

        // one sector
        if (offset <= sector_size - 4) {
            if (!ReadFATSector(fatSector, buf)) return 0x0FFFFFFF;

            return *reinterpret_cast<uint32_t*>(buf + offset);
        }

        // two sectors
        if (!ReadFATSector(fatSector, buf)) return 0x0FFFFFFF;

        if (!ReadFATSector(fatSector + 1, buf + sector_size)) return 0x0FFFFFFF;

        return *reinterpret_cast<uint32_t*>(buf + offset);
    }

    uint32_t FileSystem::GetFATEntry(uint32_t cluster) const {
        if (cluster < 2 || cluster >= clusterCount + 2) return 0x0FFFFFFF;

        const uint32_t fatOffset = cluster * 4;
        const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
        const uint32_t offset = fatOffset % bpb.bytesPerSector;

        // === FAT0 ===
        const uint32_t fat0Sector = bpb.reservedSectorCount + sectorOffset;

        uint32_t entry0 = ReadFATEntryRaw(fat0Sector, offset) & 0x0FFFFFFF;

        if (IsValidFATEntry(entry0)) return entry0;

        // === FAT1 fallback ===
        if (bpb.tableCount < 2) return entry0;

        const uint32_t fat1Sector = bpb.reservedSectorCount + bpb.FATSize32 + sectorOffset;

        if (uint32_t entry1 = ReadFATEntryRaw(fat1Sector, offset) & 0x0FFFFFFF; IsValidFATEntry(entry1)) {
            // repair FAT0
            const_cast<FileSystem*>(this)->WriteFATEntry(cluster, entry1);
            return entry1;
        }

        // Both broken → Force EOF
        return 0x0FFFFFFF;
    }

    uint32_t FileSystem::ReadFATEntry(uint32_t cluster, Sector& sec) const {
        const uint32_t fatOffset = cluster * 4;
        const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
        const uint32_t offset = fatOffset % bpb.bytesPerSector;

        if (const uint32_t fatSector = bpb.reservedSectorCount + sectorOffset; sec.sector != fatSector) {
            if (!ReadFATSector(fatSector, sec.buf)) return 0x0FFFFFFF;
            sec.sector = fatSector;
        }

        uint32_t entry = *reinterpret_cast<uint32_t*>(sec.buf + offset) & 0x0FFFFFFF;

        if (entry >= 0x0FFFFFF8 || entry == 0 || entry == 1 || entry == 0x0FFFFFF7) return 0;

        return entry;
    }

    bool FileSystem::WriteFATEntryRaw(uint32_t fatSector, uint32_t offset, uint32_t value) const {
        uint8_t buf[1024];
        const uint32_t sector_size = bpb.bytesPerSector;

        // one sector
        if (offset <= sector_size - 4) {
            if (!device->read(fatSector, 1, buf, sector_size)) return false;

            *reinterpret_cast<uint32_t*>(buf + offset) = value;

            if (!device->write(fatSector, 1, buf, sector_size)) return false;

            InvalidateFATCacheSector(fatSector);
            return true;
        }

        // two sectors
        if (!device->read(fatSector, 2, buf, sector_size * 2)) return false;

        *reinterpret_cast<uint32_t*>(buf + offset) = value;

        if (!device->write(fatSector, 2, buf, sector_size * 2)) return false;

        InvalidateFATCacheSector(fatSector);
        InvalidateFATCacheSector(fatSector + 1);
        return true;
    }

    bool FileSystem::WriteFATEntry(uint32_t cluster, uint32_t value) {
        value &= 0x0FFFFFFF;

        uint32_t old = GetFATEntry(cluster) & 0x0FFFFFFF;

        const uint32_t fatOffset = cluster * 4;
        const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
        const uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

        for (uint32_t fat = 0; fat < bpb.tableCount; ++fat) {
            const uint32_t fatBase = bpb.reservedSectorCount + fat * bpb.FATSize32;

            if (const uint32_t sector = fatBase + sectorOffset; !WriteFATEntryRaw(sector, offsetInSector, value)) return false;
        }

        if (freeClusterCount != 0xFFFFFFFF) {
            const bool wasFree = (old == 0);

            if (const bool isFree = (value == 0); wasFree && !isFree)
                freeClusterCount--;
            else if (!wasFree && isFree)
                freeClusterCount++;
        }

        return true;
    }

    uint32_t FileSystem::NextCluster(uint32_t c) const {
        if (c < 2 || c >= clusterCount + 2) return 0;

        uint32_t next = GetFATEntry(c);

        if (next >= 0x0FFFFFF8)  // EOF
            return 0;

        if (next == 0 || next == 1)  // free / invalid
            return 0;

        if (next == 0x0FFFFFF7)  // bad
            return 0;

        return next;
    }

    // Floyd Cycle Detection
    bool FileSystem::HasFATLoop(uint32_t start) const {
        uint32_t tortoise = start;
        uint32_t hare = start;

        while (true) {
            tortoise = NextCluster(tortoise);
            if (tortoise == 0) return false;

            hare = NextCluster(hare);
            if (hare == 0) return false;

            hare = NextCluster(hare);
            if (hare == 0) return false;

            if (tortoise == hare) return true;  // Loop detected
        }
    }

    uint32_t FileSystem::FindFreeCluster() {
        if (clusterCount < 2) return 0;

        const uint32_t start = (nextFreeCluster >= 2 && nextFreeCluster < clusterCount + 2) ? nextFreeCluster : 2;

        for (uint32_t c = start; c < clusterCount + 2; ++c) {
            if (GetFATEntry(c) == 0) {
                nextFreeCluster = c + 1;
                return c;
            }
        }

        // Fallback: complete scan
        for (uint32_t c = 2; c < start; ++c) {
            if (GetFATEntry(c) == 0) {
                nextFreeCluster = c + 1;
                return c;
            }
        }

        return 0;
    }

    uint32_t* FileSystem::GetClusterChain(uint32_t startCluster, size_t& outCount) const {
        outCount = 0;

        if (startCluster < 2 || startCluster >= clusterCount + 2) return nullptr;

        if (HasFATLoop(startCluster)) return nullptr;

        // Phase 1: Cluster zählen mit Batch-Reads
        const uint32_t entriesPerSector = bpb.bytesPerSector / 4;
        constexpr uint32_t sectorsPerRead = 128;  // 64 KiB @ 512 B sectors
        const uint32_t bytesNeeded = sectorsPerRead * bpb.bytesPerSector;
        const uint32_t pages = (bytesNeeded + 0xFFF) / 0x1000;

        auto* batchBuffer = static_cast<uint32_t*>(kernel::memory::request_pages(pages));

        uint32_t cluster = startCluster;
        size_t count = 0;
        uint32_t currentBatchSector = UINT32_MAX;

        while (cluster >= 2 && cluster < clusterCount + 2) {
            ++count;

            const uint32_t fatOffset = cluster * 4;
            const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
            const uint32_t fatSector = bpb.reservedSectorCount + sectorOffset;
            const uint32_t batchStart = (fatSector / sectorsPerRead) * sectorsPerRead;

            // Load new batch if necessary
            if (batchStart != currentBatchSector) {
                const uint32_t sectorsToRead = ((bpb.reservedSectorCount + bpb.FATSize32) - batchStart < sectorsPerRead)
                                                   ? (bpb.reservedSectorCount + bpb.FATSize32) - batchStart
                                                   : sectorsPerRead;

                if (!device->read(batchStart, sectorsToRead, batchBuffer, sectorsToRead * bpb.bytesPerSector)) {
                    kernel::memory::free_pages(batchBuffer, pages);
                    return nullptr;
                }
                currentBatchSector = batchStart;
            }

            const uint32_t indexInBatch =
                (fatSector - batchStart) * entriesPerSector + (fatOffset % bpb.bytesPerSector) / 4;
            const uint32_t entry = batchBuffer[indexInBatch] & 0x0FFFFFFF;

            if (entry >= 0x0FFFFFF8 || entry == 0 || entry == 1 || entry == 0x0FFFFFF7) break;

            cluster = entry;
        }

        if (count == 0) {
            kernel::memory::free_pages(batchBuffer, pages);
            return nullptr;
        }

        auto* chain = static_cast<uint32_t*>(kernel::memory::malloc(count * sizeof(uint32_t)));
        if (!chain) {
            kernel::memory::free_pages(batchBuffer, pages);
            return nullptr;
        }

        cluster = startCluster;
        currentBatchSector = UINT32_MAX;

        for (size_t i = 0; i < count; ++i) {
            chain[i] = cluster;

            const uint32_t fatOffset = cluster * 4;
            const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
            const uint32_t fatSector = bpb.reservedSectorCount + sectorOffset;
            const uint32_t batchStart = (fatSector / sectorsPerRead) * sectorsPerRead;

            if (batchStart != currentBatchSector) {
                const uint32_t sectorsToRead = ((bpb.reservedSectorCount + bpb.FATSize32) - batchStart < sectorsPerRead)
                                                   ? (bpb.reservedSectorCount + bpb.FATSize32) - batchStart
                                                   : sectorsPerRead;

                device->read(batchStart, sectorsToRead, batchBuffer, sectorsToRead * bpb.bytesPerSector);
                currentBatchSector = batchStart;
            }

            const uint32_t indexInBatch =
                (fatSector - batchStart) * entriesPerSector + (fatOffset % bpb.bytesPerSector) / 4;
            cluster = batchBuffer[indexInBatch] & 0x0FFFFFFF;
        }

        kernel::memory::free_pages(batchBuffer, pages);
        outCount = count;
        return chain;
    }

    bool FileSystem::FreeClusterChain(uint32_t startCluster) {
        if (startCluster < 2 || startCluster >= clusterCount + 2) return false;

        size_t count = 0;
        uint32_t* chain = GetClusterChain(startCluster, count);
        if (!chain || count == 0) return false;

        klib::sort(chain, chain + count);

        const uint32_t entriesPerSector = bpb.bytesPerSector / 4;
        const uint32_t sectorsPerBatch = 128;  // 64KB Batches
        const uint32_t bytesNeeded = sectorsPerBatch * bpb.bytesPerSector;
        const uint32_t pages = (bytesNeeded + 0xFFF) / 0x1000;

        auto* batchBuffer = static_cast<uint32_t*>(kernel::memory::request_pages(pages));
        if (!batchBuffer) {
            kernel::memory::free(chain);
            return false;
        }

        // Für jede FAT-Kopie
        for (uint32_t fat = 0; fat < bpb.tableCount; ++fat) {
            const uint32_t fatBase = bpb.reservedSectorCount + fat * bpb.FATSize32;

            uint32_t currentBatchStart = UINT32_MAX;
            uint32_t currentBatchEnd = 0;
            bool batchDirty = false;

            for (size_t i = 0; i < count; ++i) {
                const uint32_t cluster = chain[i];
                const uint32_t fatOffset = cluster * 4;
                const uint32_t sectorOffset = fatOffset / bpb.bytesPerSector;
                const uint32_t fatSector = fatBase + sectorOffset;
                const uint32_t batchStart = (fatSector / sectorsPerBatch) * sectorsPerBatch;
                const uint32_t batchEnd = batchStart + sectorsPerBatch;

                // Neue Batch?
                if (batchStart != currentBatchStart) {
                    // Alte Batch schreiben
                    if (batchDirty) {
                        const uint32_t sectorsToWrite = currentBatchEnd - currentBatchStart;
                        device->write(
                            currentBatchStart, sectorsToWrite, batchBuffer, sectorsToWrite * bpb.bytesPerSector
                        );

                        // Cache invalidieren
                        for (uint32_t s = currentBatchStart; s < currentBatchEnd; ++s) InvalidateFATCacheSector(s);
                    }

                    // Neue Batch laden
                    currentBatchStart = batchStart;
                    currentBatchEnd = (batchEnd > fatBase + bpb.FATSize32) ? fatBase + bpb.FATSize32 : batchEnd;

                    if (const uint32_t sectorsToRead = currentBatchEnd - currentBatchStart; !device->read(
                            currentBatchStart, sectorsToRead, batchBuffer, sectorsToRead * bpb.bytesPerSector
                        )) {
                        kernel::memory::free_pages(batchBuffer, pages);
                        kernel::memory::free(chain);
                        return false;
                    }

                    batchDirty = false;
                }

                // Entry in Batch auf 0 setzen
                const uint32_t indexInBatch =
                    (fatSector - currentBatchStart) * entriesPerSector + (fatOffset % bpb.bytesPerSector) / 4;
                batchBuffer[indexInBatch] = 0;
                batchDirty = true;
            }

            // Letzte Batch schreiben
            if (batchDirty) {
                const uint32_t sectorsToWrite = currentBatchEnd - currentBatchStart;
                device->write(currentBatchStart, sectorsToWrite, batchBuffer, sectorsToWrite * bpb.bytesPerSector);

                for (uint32_t s = currentBatchStart; s < currentBatchEnd; ++s) InvalidateFATCacheSector(s);
            }
        }

        kernel::memory::free_pages(batchBuffer, pages);

        // Free cluster count aktualisieren
        if (freeClusterCount != 0xFFFFFFFF) freeClusterCount += count;

        kernel::memory::free(chain);
        return true;
    }

    // ============================================================================
    // Directory Operations
    // ============================================================================

    FileEntry* FileSystem::ReadDirectory(const char* path, size_t& outCount) const {
        outCount = 0;
        uint32_t cluster = ResolvePathToCluster(path);
        if (cluster == 0) return nullptr;

        return ReadDirectory(cluster, outCount);
    }

    FileEntry* FileSystem::ReadDirectory(uint32_t cluster, size_t& outCount) const {
        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));
        if (!entries) {
            outCount = 0;
            return nullptr;
        }

        outCount = 0;
        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(cluster, chainCount);
        if (!chain) {
            kernel::memory::free(entries);
            return nullptr;
        }

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        LFNBufferEntry lfnBuffer[20];
        size_t lfnCount = 0;

        for (size_t ci = 0; ci < chainCount; ++ci) {
            uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
            if (!clusterBuffer) continue;

            if (!ReadCluster(chain[ci], clusterBuffer, clusterBytes)) {
                FreeClusterBuffer(clusterBuffer, clusterBytes);
                continue;
            }

            for (size_t i = 0; i < entriesPerCluster; i++) {
                const auto entry = reinterpret_cast<DirectoryEntry*>(clusterBuffer + i * sizeof(DirectoryEntry));

                // end
                if (entry->name[0] == 0x00) {
                    break;
                }  // deleted or volume label
                if (entry->name[0] == 0xE5 || entry->attr == ATTR_VOLUME_ID) {
                    continue;
                }

                // Handle LFN entries
                if (entry->attr == ATTR_LONG_NAME) {
                    if (lfnCount < 20) {
                        lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName*>(entry);
                    }
                    continue;
                }

                // Regular entry
                entries[outCount].SetIsDir((entry->attr & ATTR_DIRECTORY) != 0);

                // Process collected LFN entries
                if (lfnCount > 0) {
                    char nameBuffer[256];
                    size_t pos = 0;

                    // LFN entries are stored in descending order in the buffer,
                    // so they are processed from back to front.
                    for (int j = static_cast<int>(lfnCount) - 1; j >= 0; --j) {
                        CopyLFNPart(&lfnBuffer[j].lfnEntry, nameBuffer, pos, sizeof(nameBuffer));
                    }

                    nameBuffer[pos] = '\0';
                    entries[outCount].SetLongName(nameBuffer);
                    lfnCount = 0;
                }

                else {
                    entries[outCount].SetLongName(nullptr);
                }

                // Set short name
                char shortName[13];
                ExtractShortName(entry->name, shortName, sizeof(shortName));
                entries[outCount].SetDirectoryEntry(*entry);
                entries[outCount].SetShortName(shortName);
                entries[outCount].SetIndexInCluster(ci * entriesPerCluster + i);

                outCount++;
                if (outCount >= READ_DIR_MAX_ENTRIES) break;
            }

            FreeClusterBuffer(clusterBuffer, clusterBytes);
            if (outCount >= READ_DIR_MAX_ENTRIES) break;
        }

        kernel::memory::free(chain);
        return entries;
    }

    bool FileSystem::OverwriteDirectoryEntry(
        uint32_t parentCluster, size_t entryIndex, const DirectoryEntry* newEntry
    ) const {
        size_t clusterCount = 0;
        uint32_t* clusters = GetClusterChain(parentCluster, clusterCount);
        if (!clusters) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);
        const size_t clusterIdx = entryIndex / entriesPerCluster;
        const size_t offsetInCluster = entryIndex % entriesPerCluster;

        if (clusterIdx >= clusterCount) {
            kernel::memory::free(clusters);
            return false;
        }

        const uint32_t targetCluster = clusters[clusterIdx];
        uint8_t* buffer = AllocClusterBuffer(clusterBytes);
        if (!buffer) {
            kernel::memory::free(clusters);
            return false;
        }

        if (!ReadCluster(targetCluster, buffer, clusterBytes)) {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(clusters);
            return false;
        }

        auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        entries[offsetInCluster] = *newEntry;

        bool ok = device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster, buffer, clusterBytes);

        FreeClusterBuffer(buffer, clusterBytes);
        kernel::memory::free(clusters);
        return ok;
    }

    // ============================================================================
    // File Operations
    // ============================================================================

    bool FileSystem::ReadFile(Fat32Node* node, void* buffer, size_t len, size_t& outActual, size_t offset) const {
        if (!node || !buffer || len == 0) return false;

        if (offset >= node->fileSize) {
            outActual = 0;
            return true;
        }

        uint32_t startCluster = node->cluster;
        if (startCluster == 0) {
            outActual = 0;
            return true;
        }

        const size_t clusterBytes = bytesPerCluster();
        const size_t remaining = node->fileSize - offset;
        const size_t toRead = (len < remaining) ? len : remaining;
        const size_t clusterIndex = offset / clusterBytes;
        const size_t offsetInCluster = offset % clusterBytes;

        size_t clusterCount = 0;
        uint32_t* clusterChain = GetClusterChain(startCluster, clusterCount);
        if (!clusterChain) return false;

        if (clusterIndex >= clusterCount) {
            kernel::memory::free(clusterChain);
            outActual = 0;
            return true;
        }

        auto* dest = static_cast<uint8_t*>(buffer);
        size_t bytesRead = 0;

        for (size_t i = clusterIndex; i < clusterCount && bytesRead < toRead; i++) {
            uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
            if (!clusterBuffer) {
                kernel::memory::free(clusterChain);
                return false;
            }

            if (!ReadCluster(clusterChain[i], clusterBuffer, clusterBytes)) {
                FreeClusterBuffer(clusterBuffer, clusterBytes);
                kernel::memory::free(clusterChain);
                return false;
            }

            const size_t startPos = (i == clusterIndex) ? offsetInCluster : 0;
            const size_t availableInCluster = clusterBytes - startPos;
            const size_t toCopy = (toRead - bytesRead < availableInCluster) ? (toRead - bytesRead) : availableInCluster;

            memcpy(dest + bytesRead, clusterBuffer + startPos, toCopy);
            bytesRead += toCopy;

            FreeClusterBuffer(clusterBuffer, clusterBytes);
        }

        UpdateAccessTime(node->dirEntry);

        kernel::memory::free(clusterChain);
        outActual = bytesRead;
        return true;
    }

    bool FileSystem::WriteFile(Fat32Node* node, const void* buffer, size_t len) {
        if (!node || !buffer || len == 0) return false;

        if (IsProtected(node->dirEntry)) return false;

        const size_t clusterBytes = bytesPerCluster();
        const size_t neededClusters = (len + clusterBytes - 1) / clusterBytes;

        uint32_t startCluster = node->cluster;

        size_t existingClustersCount = 0;
        uint32_t* clusterChain = nullptr;

        // Get existing cluster chain if file already has data
        if (startCluster != 0) {
            clusterChain = GetClusterChain(startCluster, existingClustersCount);
            if (!clusterChain || existingClustersCount == 0) return false;
        }

        // Allocate additional clusters if needed
        if (existingClustersCount < neededClusters) {
            const size_t additional = neededClusters - existingClustersCount;

            // File has no cluster yet → allocate first one
            if (startCluster == 0) {
                startCluster = FindFreeCluster();
                if (startCluster == 0) return false;

                WriteFATEntry(startCluster, 0x0FFFFFFF);
                node->cluster = startCluster;

                clusterChain = GetClusterChain(startCluster, existingClustersCount);
                if (!clusterChain || existingClustersCount == 0) return false;
            }

            if (!clusterChain) {
                return false;
            }

            uint32_t lastCluster = clusterChain[existingClustersCount - 1];

            for (size_t i = 0; i < additional; ++i) {
                uint32_t freeCluster = FindFreeCluster();
                if (freeCluster == 0) {
                    kernel::memory::free(clusterChain);
                    return false;
                }

                WriteFATEntry(lastCluster, freeCluster);
                lastCluster = freeCluster;
                WriteFATEntry(lastCluster, 0x0FFFFFFF);
            }

            kernel::memory::free(clusterChain);
            clusterChain = nullptr;

            clusterChain = GetClusterChain(startCluster, existingClustersCount);
            if (!clusterChain || existingClustersCount < neededClusters) return false;
        }

        if (!clusterChain) {
            return false;
        }

        // Write data
        const auto* src = static_cast<const uint8_t*>(buffer);
        size_t remaining = len;

        for (size_t i = 0; i < existingClustersCount && remaining > 0; ++i) {
            const size_t toWrite = (remaining > clusterBytes) ? clusterBytes : remaining;

            if (!WriteCluster(clusterChain[i], src, toWrite)) {
                kernel::memory::free(clusterChain);
                return false;
            }

            src += toWrite;
            remaining -= toWrite;
        }

        kernel::memory::free(clusterChain);

        // Update directory entry
        node->fileSize = len;
        node->dirEntry.fileSize = static_cast<uint32_t>(len);
        node->dirEntry.firstClusterLow = static_cast<uint16_t>(startCluster & 0xFFFF);
        node->dirEntry.firstClusterHigh = static_cast<uint16_t>((startCluster >> 16) & 0xFFFF);
        UpdateWriteTime(node->dirEntry);

        return OverwriteDirectoryEntry(node->parentCluster, node->currentIndex, &node->dirEntry);
    }

    // ============================================================================
    // LFN Support Functions
    // ============================================================================

    bool FileSystem::WriteLFNEntries(
        DirectoryEntry* entries, size_t startIndex, const char* longName, const char* shortName, size_t nameLen
    ) const {
        const size_t entriesNeeded = (nameLen + 12) / 13;
        uint16_t nameBuffer[256] = {};

        for (size_t j = 0; j < nameLen; ++j) nameBuffer[j] = static_cast<uint8_t>(longName[j]);

        uint8_t checksum = ChkSum(shortName);

        for (int lfnIndex = static_cast<int>(entriesNeeded) - 1; lfnIndex >= 0; --lfnIndex) {
            LongFileName lfn = {};
            lfn.order = static_cast<uint8_t>(lfnIndex + 1);
            if (lfnIndex == static_cast<int>(entriesNeeded) - 1) lfn.order |= 0x40;

            lfn.attr = ATTR_LONG_NAME;
            lfn.type = 0;
            lfn.checksum = checksum;
            lfn.firstClusterLow = 0;

            size_t namePos = static_cast<size_t>(lfnIndex) * 13;

            auto copy_from_name = [&](uint16_t* dest, int count) {
                for (int c = 0; c < count; ++c) {
                    if (namePos < nameLen)
                        dest[c] = nameBuffer[namePos++];
                    else if (namePos == nameLen) {
                        dest[c] = 0x0000;
                        namePos++;
                    } else
                        dest[c] = 0xFFFF;
                }
            };

            copy_from_name(lfn.name1, 5);
            copy_from_name(lfn.name2, 6);
            copy_from_name(lfn.name3, 2);

            memcpy(&entries[startIndex + lfnIndex], &lfn, sizeof(LongFileName));
        }

        return true;
    }

    bool FileSystem::WriteDirectoryEntryWithLFN(
        uint32_t dirCluster, const char* longName, const char* shortName, const DirectoryEntry* shortEntry
    ) {
        const size_t nameLen = strlen(longName);
        const size_t entriesNeeded = (nameLen + 12) / 13;
        const size_t totalNeeded = entriesNeeded + 1;
        const size_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        size_t clusterCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, clusterCount);
        if (!chain) return false;

        // Try to find space in existing clusters
        for (size_t ci = 0; ci < clusterCount; ++ci) {
            uint32_t cluster = chain[ci];
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(cluster, buffer, clusterBytes)) {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            size_t freeCount = 0;
            size_t startIndex = 0;

            for (size_t i = 0; i < entriesPerCluster; ++i) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    if (entries[i].attr == ATTR_VOLUME_ID) {
                        freeCount = 0;
                        continue;
                    }

                    if (freeCount == 0) startIndex = i;
                    freeCount++;

                    if (freeCount >= totalNeeded) {
                        WriteLFNEntries(entries, startIndex, longName, shortName, nameLen);
                        memcpy(&entries[startIndex + entriesNeeded], shortEntry, sizeof(DirectoryEntry));

                        bool ok = device->write(ClusterToSector(cluster), bpb.sectorsPerCluster, buffer, clusterBytes);
                        FreeClusterBuffer(buffer, clusterBytes);
                        kernel::memory::free(chain);
                        return ok;
                    }
                } else {
                    freeCount = 0;
                }
            }

            FreeClusterBuffer(buffer, clusterBytes);
        }

        // Need to allocate new cluster
        uint32_t lastCluster = chain[clusterCount - 1];
        kernel::memory::free(chain);

        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;

        if (!WriteFATEntry(lastCluster, newCluster)) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        uint8_t* zero = AllocClusterBuffer(clusterBytes);
        memset(zero, 0, clusterBytes);
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero, clusterBytes);
        FreeClusterBuffer(zero, clusterBytes);

        return WriteDirectoryEntryWithLFN(dirCluster, longName, shortName, shortEntry);
    }

    // ============================================================================
    // Create/Delete Operations
    // ============================================================================

    bool FileSystem::CreateDirectory(const Fat32Node* parentDir, const char* name) {
        if (!parentDir || !name || name[0] == '\0') return false;

        const uint32_t parentCluster = parentDir->cluster;
        const uint32_t clusterBytes = bytesPerCluster();

        // Allocate new cluster
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        uint8_t* zero = AllocClusterBuffer(clusterBytes);
        if (!zero) return false;
        memset(zero, 0, clusterBytes);

        // Create "." and ".." entries
        auto* dir = reinterpret_cast<DirectoryEntry*>(zero);

        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, ".          ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = newCluster & 0xFFFF;
        dir->firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        UpdateCreateTime(*dir);

        dir++;
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, "..         ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = parentCluster & 0xFFFF;
        dir->firstClusterHigh = (parentCluster >> 16) & 0xFFFF;
        UpdateCreateTime(*dir);

        bool writeOk = device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero, clusterBytes);
        FreeClusterBuffer(zero, clusterBytes);
        if (!writeOk) return false;

        // Create directory entry
        char shortName[12] = {};
        if (!MakeShortName(name, shortName)) return false;

        DirectoryEntry newEntry = {};
        memcpy(newEntry.name, shortName, 11);
        newEntry.attr = ATTR_DIRECTORY;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;
        UpdateCreateTime(newEntry);

        WriteFSInfo();

        return WriteDirectoryEntryWithLFN(parentCluster, name, shortName, &newEntry);
    }

    bool FileSystem::CreateFile(const Fat32Node* parentDir, const char* name) {
        if (!parentDir || !name || name[0] == '\0') return false;

        const uint32_t parentCluster = parentDir->cluster;
        const uint32_t clusterBytes = bytesPerCluster();

        // Allocate cluster for file
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;

        // Initialize cluster
        uint8_t* zero = AllocClusterBuffer(clusterBytes);
        memset(zero, 0, clusterBytes);
        if (!device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero, clusterBytes)) {
            FreeClusterBuffer(zero, clusterBytes);
            return false;
        }
        FreeClusterBuffer(zero, clusterBytes);

        // Create directory entry
        char shortName[11];
        if (!MakeShortName(name, shortName)) return false;

        DirectoryEntry newEntry = {};
        memcpy(newEntry.name, shortName, 11);
        newEntry.attr = ATTR_ARCHIVE;
        newEntry.firstClusterLow = newCluster & 0xFFFF;
        newEntry.firstClusterHigh = (newCluster >> 16) & 0xFFFF;
        newEntry.fileSize = 0;
        UpdateCreateTime(newEntry);

        if (!WriteDirectoryEntryWithLFN(parentCluster, name, shortName, &newEntry)) return false;

        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        WriteFSInfo();
        return true;
    }

    bool FileSystem::DeleteDirectoryEntryInDirectory(uint32_t dirCluster, const char* name) {
        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        // Struktur für gefundene Einträge
        struct FoundEntry {
            uint32_t clusterIndex;
            size_t entryIndex;
            size_t lfnStartIndex;
            size_t lfnCount;
        };

        FoundEntry found = {0, 0, 0, 0};
        bool entryFound = false;

        // Phase 1: Finde den Eintrag
        for (size_t ci = 0; ci < chainCount && !entryFound; ++ci) {
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(chain[ci], buffer, clusterBytes)) {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);

            size_t lfnStartIdx = 0;
            size_t lfnCount = 0;

            for (size_t j = 0; j < entriesPerCluster; ++j) {
                if (entries[j].name[0] == 0x00) break;
                if (entries[j].name[0] == 0xE5) {
                    lfnCount = 0;
                    continue;
                }
                if (entries[j].attr == ATTR_VOLUME_ID) {
                    lfnCount = 0;
                    continue;
                }

                // LFN Entry
                if (entries[j].attr == ATTR_LONG_NAME) {
                    if (lfnCount == 0) lfnStartIdx = j;
                    lfnCount++;
                    continue;
                }

                // Regular Entry - Name rekonstruieren
                char fullName[256] = {};

                if (lfnCount > 0) {
                    // LFN Name aus gesammelten Entries
                    LFNBufferEntry lfnBuffer[20];
                    size_t actualLfnCount = (lfnCount < 20) ? lfnCount : 20;

                    for (size_t l = 0; l < actualLfnCount; ++l) {
                        lfnBuffer[l].lfnEntry = *reinterpret_cast<LongFileName*>(&entries[lfnStartIdx + l]);
                    }

                    // Sort by order
                    for (size_t a = 0; a < actualLfnCount - 1; a++) {
                        for (size_t b = 0; b < actualLfnCount - 1 - a; b++) {
                            if ((lfnBuffer[b].lfnEntry.order & 0x3F) > (lfnBuffer[b + 1].lfnEntry.order & 0x3F)) {
                                auto tmp = lfnBuffer[b];
                                lfnBuffer[b] = lfnBuffer[b + 1];
                                lfnBuffer[b + 1] = tmp;
                            }
                        }
                    }

                    size_t pos = 0;
                    for (size_t l = 0; l < actualLfnCount; ++l)
                        CopyLFNPart(&lfnBuffer[l].lfnEntry, fullName, pos, sizeof(fullName));
                    fullName[pos] = '\0';
                } else {
                    ExtractShortName(entries[j].name, fullName, sizeof(fullName));
                }

                // Match gefunden?
                if (strcasecmp(fullName, name) == 0) {
                    found.clusterIndex = ci;
                    found.entryIndex = j;
                    found.lfnStartIndex = lfnStartIdx;
                    found.lfnCount = lfnCount;
                    entryFound = true;
                    break;
                }

                lfnCount = 0;
            }

            FreeClusterBuffer(buffer, clusterBytes);
        }

        if (!entryFound) {
            kernel::memory::free(chain);
            return false;
        }

        // Phase 2: Lösche den Eintrag
        uint8_t* buffer = AllocClusterBuffer(clusterBytes);
        if (!buffer) {
            kernel::memory::free(chain);
            return false;
        }

        if (!ReadCluster(chain[found.clusterIndex], buffer, clusterBytes)) {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(chain);
            return false;
        }

        auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);

        // Markiere Short Entry als gelöscht
        entries[found.entryIndex].name[0] = 0xE5;

        // Markiere alle LFN Entries als gelöscht
        for (size_t i = 0; i < found.lfnCount; ++i) {
            entries[found.lfnStartIndex + i].name[0] = 0xE5;
        }

        bool success =
            device->write(ClusterToSector(chain[found.clusterIndex]), bpb.sectorsPerCluster, buffer, clusterBytes);

        FreeClusterBuffer(buffer, clusterBytes);
        kernel::memory::free(chain);

        return success;
    }

    bool FileSystem::DeleteFile(const Fat32Node* parentDir, const char* name) {
        if (!parentDir || !name) return false;

        const uint32_t parentCluster = parentDir->cluster;

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(parentCluster, entryCount);
        if (!entries) return false;

        uint32_t startCluster = 0;
        DirectoryEntry victim = {};
        bool found = false;

        for (size_t i = 0; i < entryCount; ++i) {
            if (!entries[i].isDir() && strcmp(entries[i].GetName(), name) == 0) {
                startCluster = entries[i].GetFirstCluster();
                victim = entries[i].GetDirectoryEntry();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);
        if (!found) return false;

        if (IsProtected(victim)) return false;

        if (startCluster != 0) FreeClusterChain(startCluster);

        WriteFSInfo();
        return DeleteDirectoryEntryInDirectory(parentCluster, name);
    }

    bool FileSystem::RemoveDirectory(const Fat32Node* parentDir, const char* name) {
        if (!parentDir || !name) return false;

        const uint32_t parentCluster = parentDir->cluster;

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(parentCluster, entryCount);
        if (!entries) return false;

        uint32_t targetCluster = 0;
        bool found = false;

        for (size_t i = 0; i < entryCount; ++i) {
            if (entries[i].isDir() && strcmp(entries[i].GetName(), name) == 0) {
                targetCluster = entries[i].GetFirstCluster();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);
        if (!found) return false;

        // Check empty (only "." and "..")
        size_t dirEntryCount = 0;
        FileEntry* dirEntries = ReadDirectory(targetCluster, dirEntryCount);
        if (!dirEntries || dirEntryCount > 2) {
            kernel::memory::free(dirEntries);
            return false;
        }
        kernel::memory::free(dirEntries);

        FreeClusterChain(targetCluster);

        WriteFSInfo();
        return DeleteDirectoryEntryInDirectory(parentCluster, name);
    }

    // ============================================================================
    // Rename Operation
    // ============================================================================

    bool FileSystem::Rename(const Fat32Node* parentDir, const char* oldName, const char* newName) {
        if (!parentDir || !oldName || !newName || oldName[0] == '\0' || newName[0] == '\0') return false;

        const uint32_t dirCluster = parentDir->cluster;
        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(dirCluster, entryCount);
        if (!entries || entryCount == 0) return false;

        // Find the entry to rename
        int foundIndex = -1;
        for (size_t i = 0; i < entryCount; ++i) {
            if (entries[i].GetName() && strcmp(entries[i].GetName(), oldName) == 0) {
                foundIndex = static_cast<int>(i);
                break;
            }
        }

        if (foundIndex < 0) {
            kernel::memory::free(entries);
            return false;
        }

        DirectoryEntry oldEntry = entries[foundIndex].GetDirectoryEntry();
        if (IsProtected(oldEntry)) {
            kernel::memory::free(entries);
            return false;
        }

        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) {
            kernel::memory::free(entries);
            return false;
        }

        // Find the actual entry on disk
        uint32_t targetCluster = 0;
        int targetEntryIndex = -1;

        for (size_t ci = 0; ci < chainCount && targetEntryIndex < 0; ++ci) {
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(chain[ci], buffer, clusterBytes)) {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            const auto* dirEntries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (size_t i = 0; i < entriesPerCluster; ++i) {
                if (dirEntries[i].name[0] == 0x00) break;
                if (dirEntries[i].name[0] == 0xE5) continue;
                if (dirEntries[i].attr == ATTR_VOLUME_ID) continue;

                if (dirEntries[i].fileSize == oldEntry.fileSize &&
                    dirEntries[i].firstClusterLow == oldEntry.firstClusterLow &&
                    dirEntries[i].firstClusterHigh == oldEntry.firstClusterHigh) {
                    targetCluster = chain[ci];
                    targetEntryIndex = static_cast<int>(i);
                    break;
                }
            }

            FreeClusterBuffer(buffer, clusterBytes);
        }

        if (targetEntryIndex < 0) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        uint8_t* buffer = AllocClusterBuffer(clusterBytes);
        if (!buffer) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        if (!ReadCluster(targetCluster, buffer, clusterBytes)) {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        auto* dirEntries = reinterpret_cast<DirectoryEntry*>(buffer);

        // Count old LFN entries
        int oldLFNCount = 0;
        for (int i = targetEntryIndex - 1; i >= 0; --i) {
            if ((dirEntries[i].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
            oldLFNCount++;
        }
        const int oldTotalCount = oldLFNCount + 1;
        const int oldStartIndex = targetEntryIndex - oldLFNCount;

        // Calculate new LFN entries needed
        const size_t newNameLen = strlen(newName);
        const int newLFNCount = static_cast<int>((newNameLen + 12) / 13);
        const int newTotalCount = newLFNCount + 1;

        // Generate new short name
        char newShortName[12] = {};
        if (!MakeShortName(newName, newShortName)) {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        // Check if we can overwrite in place
        bool canOverwrite = true;
        if (newTotalCount > oldTotalCount) {
            const int extra = newTotalCount - oldTotalCount;
            for (int i = oldStartIndex - 1; i >= oldStartIndex - extra; --i) {
                if (i < 0 || (dirEntries[i].name[0] != 0x00 && dirEntries[i].name[0] != 0xE5)) {
                    canOverwrite = false;
                    break;
                }
            }
        }

        bool success = false;

        if (canOverwrite) {
            // Mark old entries as deleted
            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i) dirEntries[i].name[0] = 0xE5;

            // Write new LFN entries
            WriteLFNEntries(dirEntries, oldStartIndex, newName, newShortName, newNameLen);

            // Write new short entry
            DirectoryEntry updated = oldEntry;
            memcpy(updated.name, newShortName, 11);
            dirEntries[oldStartIndex + newLFNCount] = updated;

            success = device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster, buffer, clusterBytes);
        } else {
            // Delete old entry and create new one
            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i) dirEntries[i].name[0] = 0xE5;

            device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster, buffer, clusterBytes);

            DirectoryEntry newEntry = oldEntry;
            memcpy(newEntry.name, newShortName, 11);
            success = WriteDirectoryEntryWithLFN(dirCluster, newName, newShortName, &newEntry);
        }

        FreeClusterBuffer(buffer, clusterBytes);
        kernel::memory::free(entries);
        kernel::memory::free(chain);
        return success;
    }

    // ============================================================================
    // Path Resolution
    // ============================================================================

    uint32_t FileSystem::ResolvePathToCluster(const char* path) const {
        if (path[0] != '/') return 0;

        uint32_t currentCluster = GetRootCluster();

        char components[16][32];
        size_t compCount = split_path(path, components, 16);

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

        uint32_t result = 0;
        for (size_t i = 0; i < entryCount; i++) {
            if (const char* entryName = entries[i].GetName(); strcmp(entryName, givenName) == 0) {
                result = entries[i].GetFirstCluster();
                break;
            }
        }

        kernel::memory::free(entries);
        return result;
    }

    // ============================================================================
    // FileEntry Helper
    // ============================================================================

    void FileEntry::FormatShortName() {
        char name[9] = {};
        char ext[4] = {};

        memcpy(name, shortName, 8);
        for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

        memcpy(ext, shortName + 8, 3);
        for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';

        const size_t nameLen = strlen(name);
        memcpy(formattedShortName, name, nameLen);

        if (ext[0] != '\0') {
            formattedShortName[nameLen] = '.';
            memcpy(formattedShortName + nameLen + 1, ext, strlen(ext));
            formattedShortName[nameLen + 1 + strlen(ext)] = '\0';
        } else {
            formattedShortName[nameLen] = '\0';
        }
    }

    bool FileSystem::IsProtected(const DirectoryEntry& e) {
        return e.attr & (ATTR_READ_ONLY | ATTR_SYSTEM);
    }
}  // namespace FAT32
