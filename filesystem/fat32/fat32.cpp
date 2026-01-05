//
// Created by linus on 03.07.25.
//

#include "fat32.h"
#include "fat32_vfs_adapter.h"
#include "../../include/log.h"
#include "../../include/string.h"
#include "../../include/path.h"

namespace FAT32
{
    // ============================================================================
    // Helper Functions - Memory Management
    // ============================================================================

    static uint8_t* AllocClusterBuffer(uint32_t clusterBytes)
    {
        const size_t pages = (clusterBytes + 0xFFF) / 0x1000;
        auto* page = static_cast<uint8_t*>(kernel::memory::request_pages(pages));
        if (page) memset(page, 0, pages * 0x1000);
        return page;
    }

    static void FreeClusterBuffer(uint8_t* ptr, uint32_t clusterBytes)
    {
        kernel::memory::free_pages(ptr, (clusterBytes + 0xFFF) / 0x1000);
    }

    // ============================================================================
    // Helper Functions - Name Processing
    // ============================================================================

    static void ExtractShortName(const unsigned char* rawName, char* shortNameBuffer, const size_t bufferSize)
    {
        if (bufferSize < 13) return;
        memcpy(shortNameBuffer, rawName, 11);

        for (int i = 10; i >= 0; i--)
        {
            if (shortNameBuffer[i] == ' ')
                shortNameBuffer[i] = '\0';
            else
                break;
        }
        shortNameBuffer[12] = '\0';
    }

    static uint8_t LFNChecksum(const char* shortName)
    {
        uint8_t sum = 0;
        for (int i = 0; i < 11; i++)
        {
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortName[i];
        }
        return sum;
    }

    static bool CopyLFNPart(const LongFileName* lfn, char* buffer, size_t& pos, const size_t maxLen)
    {
        auto copyChars = [&](const uint16_t* src, const size_t count)
        {
            for (size_t i = 0; i < count; i++)
            {
                if (src[i] == 0x0000 || src[i] == 0xFFFF) return false;
                if (pos >= maxLen - 1) return false;
                buffer[pos++] = static_cast<char>(src[i] & 0xFF);
            }
            return true;
        };

        return copyChars(lfn->name1, 5) &&
            copyChars(lfn->name2, 6) &&
            copyChars(lfn->name3, 2);
    }

    bool MakeShortName(const char* input, char* output11)
    {
        memset(output11, ' ', 11);

        const char* dot = strrchr(input, '.');
        size_t nameLen = dot ? static_cast<size_t>(dot - input) : strlen(input);
        size_t extLen = dot ? strlen(dot + 1) : 0;

        if (nameLen == 0) return false;

        // Process name part (max 8 chars)
        size_t outPos = 0;
        for (size_t i = 0; i < nameLen && outPos < 8; i++)
        {
            char c = input[i];
            if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
            output11[outPos++] = to_upper(c);
        }

        // Add ~1 if name was truncated
        if (nameLen > 8)
        {
            output11[6] = '~';
            output11[7] = '1';
        }

        // Process extension (max 3 chars)
        if (dot && extLen > 0)
        {
            size_t extPos = 0;
            for (size_t i = 0; i < 3 && dot[1 + i]; i++)
            {
                char c = dot[1 + i];
                if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
                output11[8 + extPos++] = to_upper(c);
            }
        }

        return true;
    }

    // ============================================================================
    // FileSystem Implementation
    // ============================================================================

    FileSystem::FileSystem(BlockDevice* device)
        : device(device), valid(false), sectorSize(device->get_sector_size())
    {
        uint8_t sector[512];
        if (!device->read(0, 1, sector, 512))
        {
            Log::Error("[FAT32] Failed to read first sector");
            return;
        }

        memcpy(&bpb, sector, sizeof(BPB_FAT32));

        if (bpb.tableCount < 1 || bpb.sectorsPerCluster == 0) return;
        if (memcmp(bpb.fsType, "FAT32   ", 8) != 0) return;

        fatStart = bpb.reservedSectorCount;
        fatSize = bpb.FATSize32;
        dataStart = fatStart + (bpb.tableCount * fatSize);

        valid = true;
    }

    FileSystem::~FileSystem() = default;

    bool FileSystem::is_valid() const { return valid; }
    uint32_t FileSystem::GetRootCluster() const { return bpb.rootCluster; }
    uint32_t FileSystem::bytesPerCluster() const { return bpb.bytesPerSector * bpb.sectorsPerCluster; }

    uint32_t FileSystem::ClusterToSector(const uint32_t cluster) const
    {
        return dataStart + (cluster - 2) * bpb.sectorsPerCluster;
    }

    // ============================================================================
    // Cluster I/O Operations
    // ============================================================================

    ssize_t FileSystem::ReadCluster(const uint32_t cluster, void* buffer, size_t buffer_size) const
    {
        const uint32_t sector = ClusterToSector(cluster);
        return device->read(sector, bpb.sectorsPerCluster, buffer, buffer_size);
    }

    bool FileSystem::WriteCluster(uint32_t cluster, const void* data, size_t len, size_t offset) const
    {
        if (!data || len == 0) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        if (len + offset > clusterBytes) return false;

        uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
        if (!clusterBuffer) return false;

        // Read existing data if partial write
        if (len < clusterBytes || offset > 0)
        {
            if (!ReadCluster(cluster, clusterBuffer, clusterBytes))
            {
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

    uint32_t FileSystem::GetFATEntry(const uint32_t cluster) const
    {
        const uint32_t fatOffset = cluster * 4;
        const uint32_t sector = fatStart + (fatOffset / bpb.bytesPerSector);
        const uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

        uint8_t sectorData[512];
        if (!device->read(sector, 1, sectorData, 512))
            return 0x0FFFFFFF; // Error = EOF

        const uint32_t entry = *reinterpret_cast<uint32_t*>(sectorData + offsetInSector);
        return entry & 0x0FFFFFFF;
    }

    bool FileSystem::WriteFATEntry(uint32_t cluster, uint32_t value)
    {
        const uint32_t fatOffset = cluster * 4;
        const uint32_t sector = fatStart + (fatOffset / bpb.bytesPerSector);
        const uint32_t offsetInSector = fatOffset % bpb.bytesPerSector;

        uint8_t sectorData[512];
        if (!device->read(sector, 1, sectorData, 512)) return false;

        *reinterpret_cast<uint32_t*>(sectorData + offsetInSector) = value;

        return device->write(sector, 1, sectorData, 512);
    }

    uint32_t FileSystem::FindFreeCluster()
    {
        uint8_t sectorData[512];
        const uint32_t totalClusters = bpb.FATSize32 * bpb.bytesPerSector / 4;
        const uint32_t sectorsInFAT = bpb.FATSize32;

        for (uint32_t sector = fatStart; sector < fatStart + sectorsInFAT; sector++)
        {
            if (!device->read(sector, 1, sectorData, 512)) return 0;

            // 128 entries per sector (512 bytes / 4 bytes per entry)
            for (uint32_t i = 0; i < 128; i++)
            {
                uint32_t cluster = (sector - fatStart) * 128 + i;
                if (cluster < 2) continue; // Clusters 0 and 1 are reserved
                if (cluster >= totalClusters) break;

                uint32_t entry = *reinterpret_cast<uint32_t*>(sectorData + i * 4);
                if ((entry & 0x0FFFFFFF) == 0)
                {
                    return cluster;
                }
            }
        }
        return 0;
    }

    uint32_t* FileSystem::GetClusterChain(const uint32_t startCluster, size_t& outCount) const
    {
        size_t capacity = 16;
        size_t count = 0;
        auto* chain = static_cast<uint32_t*>(kernel::memory::malloc(capacity * sizeof(uint32_t)));

        uint32_t cluster = startCluster;

        while (cluster < 0x0FFFFFF8)
        {
            if (count == capacity)
            {
                capacity *= 2;
                auto* newChain = static_cast<uint32_t*>(
                    kernel::memory::realloc(chain, count * sizeof(uint32_t), capacity * sizeof(uint32_t)));
                if (!newChain)
                {
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
        return chain;
    }

    // ============================================================================
    // Directory Operations
    // ============================================================================

    FileEntry* FileSystem::ReadDirectory(const char* path, size_t& outCount) const
    {
        outCount = 0;
        uint32_t cluster = ResolvePathToCluster(path);
        if (cluster == 0) return nullptr;

        return ReadDirectory(cluster, outCount);
    }

    FileEntry* FileSystem::ReadDirectory(uint32_t cluster, size_t& outCount) const
    {
        auto* entries = static_cast<FileEntry*>(
            kernel::memory::malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));
        if (!entries)
        {
            outCount = 0;
            return nullptr;
        }

        outCount = 0;
        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(cluster, chainCount);
        if (!chain)
        {
            kernel::memory::free(entries);
            return nullptr;
        }

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        LFNBufferEntry lfnBuffer[20];
        size_t lfnCount = 0;

        for (size_t ci = 0; ci < chainCount; ++ci)
        {
            uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
            if (!clusterBuffer) continue;

            if (!ReadCluster(chain[ci], clusterBuffer, clusterBytes))
            {
                FreeClusterBuffer(clusterBuffer, clusterBytes);
                continue;
            }

            for (size_t i = 0; i < entriesPerCluster; i++)
            {
                const auto entry = reinterpret_cast<DirectoryEntry*>(
                    clusterBuffer + i * sizeof(DirectoryEntry));

                if (entry->name[0] == 0x00) break;
                if (entry->name[0] == 0xE5) continue;
                if (entry->attr == ATTR_VOLUME_ID) continue;

                // Handle LFN entries
                if (entry->attr == ATTR_LONG_NAME)
                {
                    if (lfnCount < 20)
                    {
                        lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName*>(entry);
                    }
                    continue;
                }

                // Regular entry
                entries[outCount].SetIsDir((entry->attr & ATTR_DIRECTORY) != 0);

                // Process collected LFN entries
                if (lfnCount > 0)
                {
                    // Sort LFN entries
                    for (size_t j = 0; j < lfnCount - 1; j++)
                    {
                        for (size_t k = 0; k < lfnCount - 1 - j; k++)
                        {
                            if ((lfnBuffer[k].lfnEntry.order & 0x3F) >
                                (lfnBuffer[k + 1].lfnEntry.order & 0x3F))
                            {
                                const auto tmp = lfnBuffer[k];
                                lfnBuffer[k] = lfnBuffer[k + 1];
                                lfnBuffer[k + 1] = tmp;
                            }
                        }
                    }

                    // Construct long name
                    char nameBuffer[256];
                    size_t pos = 0;
                    for (size_t j = 0; j < lfnCount; j++)
                    {
                        CopyLFNPart(&lfnBuffer[j].lfnEntry, nameBuffer, pos, sizeof(nameBuffer));
                    }
                    nameBuffer[pos] = '\0';
                    entries[outCount].SetLongName(nameBuffer);
                    lfnCount = 0;
                }
                else
                {
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

    bool FileSystem::OverwriteDirectoryEntry(uint32_t parentCluster, size_t entryIndex,
                                             const DirectoryEntry* newEntry) const
    {
        size_t clusterCount = 0;
        uint32_t* clusters = GetClusterChain(parentCluster, clusterCount);
        if (!clusters) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);
        const size_t clusterIdx = entryIndex / entriesPerCluster;
        const size_t offsetInCluster = entryIndex % entriesPerCluster;

        if (clusterIdx >= clusterCount)
        {
            kernel::memory::free(clusters);
            return false;
        }

        const uint32_t targetCluster = clusters[clusterIdx];
        uint8_t* buffer = AllocClusterBuffer(clusterBytes);
        if (!buffer)
        {
            kernel::memory::free(clusters);
            return false;
        }

        if (!ReadCluster(targetCluster, buffer, clusterBytes))
        {
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

    bool FileSystem::ReadFile(const Fat32Node* node, void* buffer, size_t len,
                              size_t& outActual, size_t offset) const
    {
        if (!node || !buffer || len == 0) return false;

        if (offset >= node->fileSize)
        {
            outActual = 0;
            return true;
        }

        uint32_t startCluster = node->cluster;
        if (startCluster == 0)
        {
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

        if (clusterIndex >= clusterCount)
        {
            kernel::memory::free(clusterChain);
            outActual = 0;
            return true;
        }

        auto* dest = static_cast<uint8_t*>(buffer);
        size_t bytesRead = 0;

        for (size_t i = clusterIndex; i < clusterCount && bytesRead < toRead; i++)
        {
            uint8_t* clusterBuffer = AllocClusterBuffer(clusterBytes);
            if (!clusterBuffer)
            {
                kernel::memory::free(clusterChain);
                return false;
            }

            if (!ReadCluster(clusterChain[i], clusterBuffer, clusterBytes))
            {
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

        kernel::memory::free(clusterChain);
        outActual = bytesRead;
        return true;
    }

    bool FileSystem::WriteFile(Fat32Node* node, const void* buffer, size_t len)
    {
        if (!node || !buffer || len == 0)
            return false;

        const size_t clusterBytes = bytesPerCluster();
        const size_t neededClusters =
            (len + clusterBytes - 1) / clusterBytes;

        uint32_t startCluster = node->cluster;

        size_t existingClustersCount = 0;
        uint32_t* clusterChain = nullptr;

        // Get existing cluster chain if file already has data
        if (startCluster != 0)
        {
            clusterChain = GetClusterChain(startCluster, existingClustersCount);
            if (!clusterChain || existingClustersCount == 0)
                return false;
        }

        // Allocate additional clusters if needed
        if (existingClustersCount < neededClusters)
        {
            const size_t additional = neededClusters - existingClustersCount;

            // File has no cluster yet → allocate first one
            if (startCluster == 0)
            {
                startCluster = FindFreeCluster();
                if (startCluster == 0)
                    return false;

                WriteFATEntry(startCluster, 0x0FFFFFFF);
                node->cluster = startCluster;

                clusterChain = GetClusterChain(startCluster, existingClustersCount);
                if (!clusterChain || existingClustersCount == 0)
                    return false;
            }

            uint32_t lastCluster = clusterChain[existingClustersCount - 1];

            for (size_t i = 0; i < additional; ++i)
            {
                uint32_t freeCluster = FindFreeCluster();
                if (freeCluster == 0)
                {
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
            if (!clusterChain || existingClustersCount < neededClusters)
                return false;
        }

        // Write data
        const auto* src = static_cast<const uint8_t*>(buffer);
        size_t remaining = len;

        for (size_t i = 0; i < existingClustersCount && remaining > 0; ++i)
        {
            const size_t toWrite =
                (remaining > clusterBytes) ? clusterBytes : remaining;

            if (!WriteCluster(clusterChain[i], src, toWrite))
            {
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
        node->dirEntry.firstClusterLow =
            static_cast<uint16_t>(startCluster & 0xFFFF);
        node->dirEntry.firstClusterHigh =
            static_cast<uint16_t>((startCluster >> 16) & 0xFFFF);

        return OverwriteDirectoryEntry(
            node->parentCluster,
            node->currentIndex,
            &node->dirEntry
        );
    }


    // ============================================================================
    // LFN Support Functions
    // ============================================================================

    bool FileSystem::WriteLFNEntries(DirectoryEntry* entries, size_t startIndex,
                                     const char* longName, const char* shortName,
                                     size_t nameLen) const
    {
        const size_t entriesNeeded = (nameLen + 12) / 13;
        uint16_t nameBuffer[256] = {};

        for (size_t j = 0; j < nameLen; ++j)
            nameBuffer[j] = static_cast<uint8_t>(longName[j]);

        uint8_t checksum = LFNChecksum(shortName);

        for (int lfnIndex = static_cast<int>(entriesNeeded) - 1; lfnIndex >= 0; --lfnIndex)
        {
            LongFileName lfn = {};
            lfn.order = static_cast<uint8_t>(lfnIndex + 1);
            if (lfnIndex == static_cast<int>(entriesNeeded) - 1)
                lfn.order |= 0x40;

            lfn.attr = ATTR_LONG_NAME;
            lfn.type = 0;
            lfn.checksum = checksum;
            lfn.firstClusterLow = 0;

            size_t namePos = static_cast<size_t>(lfnIndex) * 13;

            auto copy_from_name = [&](uint16_t* dest, int count)
            {
                for (int c = 0; c < count; ++c)
                {
                    if (namePos < nameLen)
                        dest[c] = nameBuffer[namePos++];
                    else if (namePos == nameLen)
                    {
                        dest[c] = 0x0000;
                        namePos++;
                    }
                    else
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

    bool FileSystem::WriteDirectoryEntryWithLFN(uint32_t dirCluster, const char* longName,
                                                const char* shortName, const DirectoryEntry* shortEntry)
    {
        const size_t nameLen = strlen(longName);
        const size_t entriesNeeded = (nameLen + 12) / 13;
        const size_t totalNeeded = entriesNeeded + 1;
        const size_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        size_t clusterCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, clusterCount);
        if (!chain) return false;

        // Try to find space in existing clusters
        for (size_t ci = 0; ci < clusterCount; ++ci)
        {
            uint32_t cluster = chain[ci];
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(cluster, buffer, clusterBytes))
            {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            size_t freeCount = 0;
            size_t startIndex = 0;

            for (size_t i = 0; i < entriesPerCluster; ++i)
            {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5)
                {
                    if (freeCount == 0) startIndex = i;
                    freeCount++;

                    if (freeCount >= totalNeeded)
                    {
                        WriteLFNEntries(entries, startIndex, longName, shortName, nameLen);
                        memcpy(&entries[startIndex + entriesNeeded], shortEntry, sizeof(DirectoryEntry));

                        bool ok = device->write(ClusterToSector(cluster), bpb.sectorsPerCluster,
                                                buffer, clusterBytes);
                        FreeClusterBuffer(buffer, clusterBytes);
                        kernel::memory::free(chain);
                        return ok;
                    }
                }
                else
                {
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

    bool FileSystem::CreateDirectory(const Fat32Node* parentDir, const char* name)
    {
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

        dir++;
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, "..         ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->firstClusterLow = parentCluster & 0xFFFF;
        dir->firstClusterHigh = (parentCluster >> 16) & 0xFFFF;

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

        return WriteDirectoryEntryWithLFN(parentCluster, name, shortName, &newEntry);
    }

    bool FileSystem::CreateFile(const Fat32Node* parentDir, const char* name)
    {
        if (!parentDir || !name || name[0] == '\0') return false;

        const uint32_t parentCluster = parentDir->cluster;
        const uint32_t clusterBytes = bytesPerCluster();

        // Allocate cluster for file
        uint32_t newCluster = FindFreeCluster();
        if (newCluster == 0) return false;
        if (!WriteFATEntry(newCluster, 0x0FFFFFFF)) return false;

        // Initialize cluster
        uint8_t* zero = AllocClusterBuffer(clusterBytes);
        if (!zero) return false;
        memset(zero, 0, clusterBytes);
        device->write(ClusterToSector(newCluster), bpb.sectorsPerCluster, zero, clusterBytes);
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

        return WriteDirectoryEntryWithLFN(parentCluster, name, shortName, &newEntry);
    }

    bool FileSystem::DeleteDirectoryEntryInDirectory(uint32_t dirCluster, const char* name)
    {
        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, chainCount);
        if (!chain) return false;

        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entryCount = clusterBytes / sizeof(DirectoryEntry);
        bool deleted = false;

        for (size_t i = 0; i < chainCount && !deleted; ++i)
        {
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(chain[i], buffer, clusterBytes))
            {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            LFNBufferEntry lfnBuffer[20];
            size_t lfnCount = 0;

            for (size_t j = 0; j < entryCount; ++j)
            {
                if (entries[j].name[0] == 0x00) break;
                if (entries[j].name[0] == 0xE5) continue;
                if (entries[j].attr == ATTR_VOLUME_ID) continue;

                // Collect LFN entries
                if (entries[j].attr == ATTR_LONG_NAME)
                {
                    if (lfnCount < 20)
                    {
                        lfnBuffer[lfnCount++].lfnEntry = *reinterpret_cast<LongFileName*>(&entries[j]);
                    }
                    continue;
                }

                // Construct name
                char fullName[256] = {};
                if (lfnCount > 0)
                {
                    // Sort LFN entries
                    for (size_t a = 0; a < lfnCount - 1; a++)
                    {
                        for (size_t b = 0; b < lfnCount - 1 - a; b++)
                        {
                            if ((lfnBuffer[b].lfnEntry.order & 0x3F) >
                                (lfnBuffer[b + 1].lfnEntry.order & 0x3F))
                            {
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
                }
                else
                {
                    ExtractShortName(entries[j].name, fullName, sizeof(fullName));
                }

                // Check if this is the entry to delete
                if (strcmp(fullName, name) == 0)
                {
                    entries[j].name[0] = 0xE5;

                    // Mark LFN entries as deleted
                    for (int k = static_cast<int>(j) - 1; k >= 0; --k)
                    {
                        if ((entries[k].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
                        entries[k].name[0] = 0xE5;
                    }

                    device->write(ClusterToSector(chain[i]), bpb.sectorsPerCluster, buffer, clusterBytes);
                    deleted = true;
                }

                lfnCount = 0;
                if (deleted) break;
            }

            FreeClusterBuffer(buffer, clusterBytes);
        }

        kernel::memory::free(chain);
        return deleted;
    }

    bool FileSystem::DeleteFile(const Fat32Node* parentDir, const char* name)
    {
        if (!parentDir || !name) return false;

        const uint32_t parentCluster = parentDir->cluster;

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(parentCluster, entryCount);
        if (!entries) return false;

        bool found = false;
        uint32_t startCluster = 0;

        for (size_t i = 0; i < entryCount; ++i)
        {
            if (strcmp(entries[i].GetName(), name) == 0 && !entries[i].isDir())
            {
                startCluster = entries[i].GetFirstCluster();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);

        if (!found) return false;

        // Free cluster chain
        size_t count = 0;
        if (uint32_t* chain = GetClusterChain(startCluster, count))
        {
            for (size_t j = 0; j < count; ++j)
                WriteFATEntry(chain[j], 0);
            kernel::memory::free(chain);
        }

        return DeleteDirectoryEntryInDirectory(parentCluster, name);
    }

    bool FileSystem::RemoveDirectory(const Fat32Node* parentDir, const char* name)
    {
        if (!parentDir || !name) return false;

        const uint32_t parentCluster = parentDir->cluster;

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(parentCluster, entryCount);
        if (!entries) return false;

        bool found = false;
        uint32_t targetCluster = 0;

        for (size_t i = 0; i < entryCount; ++i)
        {
            if (strcmp(entries[i].GetName(), name) == 0 && entries[i].isDir())
            {
                targetCluster = entries[i].GetFirstCluster();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);

        if (!found) return false;

        // Check if directory is empty (only "." and ".." allowed)
        size_t dirEntryCount = 0;
        FileEntry* dirEntries = ReadDirectory(targetCluster, dirEntryCount);
        if (!dirEntries || dirEntryCount > 2)
        {
            kernel::memory::free(dirEntries);
            return false;
        }
        kernel::memory::free(dirEntries);

        // Free cluster chain
        size_t count = 0;
        if (uint32_t* chain = GetClusterChain(targetCluster, count))
        {
            for (size_t j = 0; j < count; ++j)
                WriteFATEntry(chain[j], 0);
            kernel::memory::free(chain);
        }

        return DeleteDirectoryEntryInDirectory(parentCluster, name);
    }

    // ============================================================================
    // Rename Operation
    // ============================================================================

    bool FileSystem::Rename(const Fat32Node* parentDir, const char* oldName, const char* newName)
    {
        if (!parentDir || !oldName || !newName || oldName[0] == '\0' || newName[0] == '\0')
            return false;

        const uint32_t dirCluster = parentDir->cluster;
        const uint32_t clusterBytes = bytesPerCluster();
        const size_t entriesPerCluster = clusterBytes / sizeof(DirectoryEntry);

        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(dirCluster, entryCount);
        if (!entries || entryCount == 0) return false;

        // Find the entry to rename
        int foundIndex = -1;
        for (size_t i = 0; i < entryCount; ++i)
        {
            if (entries[i].GetName() && strcmp(entries[i].GetName(), oldName) == 0)
            {
                foundIndex = static_cast<int>(i);
                break;
            }
        }

        if (foundIndex < 0)
        {
            kernel::memory::free(entries);
            return false;
        }

        DirectoryEntry oldEntry = entries[foundIndex].GetDirectoryEntry();

        size_t chainCount = 0;
        uint32_t* chain = GetClusterChain(dirCluster, chainCount);
        if (!chain)
        {
            kernel::memory::free(entries);
            return false;
        }

        // Find the actual entry on disk
        uint32_t targetCluster = 0;
        int targetEntryIndex = -1;

        for (size_t ci = 0; ci < chainCount && targetEntryIndex < 0; ++ci)
        {
            uint8_t* buffer = AllocClusterBuffer(clusterBytes);
            if (!buffer) continue;

            if (!ReadCluster(chain[ci], buffer, clusterBytes))
            {
                FreeClusterBuffer(buffer, clusterBytes);
                continue;
            }

            const auto* dirEntries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (size_t i = 0; i < entriesPerCluster; ++i)
            {
                if (dirEntries[i].name[0] == 0x00) break;
                if (dirEntries[i].name[0] == 0xE5) continue;
                if (dirEntries[i].attr == ATTR_VOLUME_ID) continue;

                if (dirEntries[i].fileSize == oldEntry.fileSize &&
                    dirEntries[i].firstClusterLow == oldEntry.firstClusterLow &&
                    dirEntries[i].firstClusterHigh == oldEntry.firstClusterHigh)
                {
                    targetCluster = chain[ci];
                    targetEntryIndex = static_cast<int>(i);
                    break;
                }
            }

            FreeClusterBuffer(buffer, clusterBytes);
        }

        if (targetEntryIndex < 0)
        {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        uint8_t* buffer = AllocClusterBuffer(clusterBytes);
        if (!buffer)
        {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        if (!ReadCluster(targetCluster, buffer, clusterBytes))
        {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        auto* dirEntries = reinterpret_cast<DirectoryEntry*>(buffer);

        // Count old LFN entries
        int oldLFNCount = 0;
        for (int i = targetEntryIndex - 1; i >= 0; --i)
        {
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
        if (!MakeShortName(newName, newShortName))
        {
            FreeClusterBuffer(buffer, clusterBytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        // Check if we can overwrite in place
        bool canOverwrite = true;
        if (newTotalCount > oldTotalCount)
        {
            const int extra = newTotalCount - oldTotalCount;
            for (int i = oldStartIndex - 1; i >= oldStartIndex - extra; --i)
            {
                if (i < 0 || (dirEntries[i].name[0] != 0x00 && dirEntries[i].name[0] != 0xE5))
                {
                    canOverwrite = false;
                    break;
                }
            }
        }

        bool success = false;

        if (canOverwrite)
        {
            // Mark old entries as deleted
            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i)
                dirEntries[i].name[0] = 0xE5;

            // Write new LFN entries
            WriteLFNEntries(dirEntries, oldStartIndex, newName, newShortName, newNameLen);

            // Write new short entry
            DirectoryEntry updated = oldEntry;
            memcpy(updated.name, newShortName, 11);
            dirEntries[oldStartIndex + newLFNCount] = updated;

            success = device->write(ClusterToSector(targetCluster), bpb.sectorsPerCluster,
                                    buffer, clusterBytes);
        }
        else
        {
            // Delete old entry and create new one
            for (int i = oldStartIndex; i < oldStartIndex + oldTotalCount; ++i)
                dirEntries[i].name[0] = 0xE5;

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

    uint32_t FileSystem::ResolvePathToCluster(const char* path) const
    {
        if (path[0] != '/') return 0;

        uint32_t currentCluster = GetRootCluster();

        char components[16][32];
        size_t compCount = split_path(path, components, 16);

        for (size_t i = 0; i < compCount; i++)
        {
            uint32_t nextCluster = FindEntryCluster(currentCluster, components[i]);
            if (nextCluster == 0) return 0;
            currentCluster = nextCluster;
        }

        return currentCluster;
    }

    uint32_t FileSystem::FindEntryCluster(uint32_t dirCluster, const char* givenName) const
    {
        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(dirCluster, entryCount);
        if (!entries) return 0;

        uint32_t result = 0;
        for (size_t i = 0; i < entryCount; i++)
        {
            const char* entryName = entries[i].GetName();
            if (strcmp(entryName, givenName) == 0)
            {
                result = entries[i].GetFirstCluster();
                break;
            }
        }

        kernel::memory::free(entries);
        return result;
    }

    bool FileSystem::IsDir(uint32_t cluster) const
    {
        size_t entryCount = 0;
        FileEntry* entries = ReadDirectory(cluster, entryCount);
        if (!entries) return false;
        kernel::memory::free(entries);
        return true;
    }

    size_t FileSystem::FindFirstLFNIndex(const FileEntry* entries, size_t shortNameIndex)
    {
        if (shortNameIndex == 0) return shortNameIndex;

        size_t firstLFN = shortNameIndex;
        for (int i = static_cast<int>(shortNameIndex) - 1; i >= 0; i--)
        {
            DirectoryEntry entry = entries[i].GetDirectoryEntry();
            if (entry.attr == ATTR_LONG_NAME)
            {
                firstLFN = i;
            }
            else
            {
                break;
            }
        }

        return firstLFN;
    }

    // ============================================================================
    // FileEntry Helper
    // ============================================================================

    void FileEntry::FormatShortName()
    {
        char name[9] = {};
        char ext[4] = {};

        memcpy(name, shortName, 8);
        for (int i = 7; i >= 0 && name[i] == ' '; i--)
            name[i] = '\0';

        memcpy(ext, shortName + 8, 3);
        for (int i = 2; i >= 0 && ext[i] == ' '; i--)
            ext[i] = '\0';

        const size_t nameLen = strlen(name);
        memcpy(formattedShortName, name, nameLen);

        if (ext[0] != '\0')
        {
            formattedShortName[nameLen] = '.';
            memcpy(formattedShortName + nameLen + 1, ext, strlen(ext));
            formattedShortName[nameLen + 1 + strlen(ext)] = '\0';
        }
        else
        {
            formattedShortName[nameLen] = '\0';
        }
    }
}
