//
// Created by linus on 03.07.25.
//

#include "fat32.h"

#include <log.h>
#include <path.h>
#include <sort.h>
#include <string.h>

#include "fat32_lfn.h"
#include "fat32_time.h"
#include "fat32_vfs_adapter.h"

namespace fat32 {
    // ============================================================================
    // Helper Functions - Memory Management
    // ============================================================================

    static uint8_t* alloc_cluster_buffer(uint32_t cluster_bytes) {
        const size_t pages = (cluster_bytes + 0xFFF) / 0x1000;
        virt_addr_t page = kernel::memory::request_pages(pages);
        if (!virt_null(page)) memset(page, 0, pages * 0x1000);
        return virt_as<uint8_t>(page);
    }

    static void free_cluster_buffer(const uint8_t* ptr, const uint32_t cluster_bytes) {
        kernel::memory::free_pages(make_virt((void*)ptr), (cluster_bytes + 0xFFF) / 0x1000);
    }

    // ============================================================================
    // FileSystem Implementation
    // ============================================================================

    FileSystem::FileSystem(BlockDevice* device)
        : device(device)
        , fs_valid(false)
        , next_free_cluster(2)
        , cache_access_counter(0) {
        uint8_t sector[512];
        if (!device->read(0, 1, sector, 512)) {
            Log::error("[FAT32] Failed to read first sector");
            return;
        }

        memcpy(&bpb, sector, sizeof(BPB_FAT32));

        if (bpb.table_count < 1 || bpb.sectors_per_cluster == 0) return;

        const uint32_t total_sectors = (bpb.total_sectors16 != 0) ? bpb.total_sectors16 : bpb.total_sectors32;

        const uint32_t data_sectors = total_sectors - (bpb.reserved_sector_count + (bpb.table_count * bpb.fat_size32));

        cluster_count = data_sectors / bpb.sectors_per_cluster;

        data_start = bpb.reserved_sector_count + (bpb.table_count * bpb.fat_size32);

        if (!probe_fs()) return;

        for (auto& i : fat_cache) {
            i.sector = 0;
            i.last_used = 0;
            i.valid = false;
        }

        load_fs_info();

        sector_size = bpb.bytes_per_sector;

        fs_valid = true;
    }

    FileSystem::~FileSystem() {
        write_fs_info();
    }

    bool FileSystem::probe_fs() const {
        return bpb.root_entry_count == 0 && bpb.fat_size16 == 0 && cluster_count >= 65525;
    }

    bool FileSystem::is_valid() const {
        return fs_valid;
    }
    uint32_t FileSystem::get_root_cluster() const {
        return bpb.root_cluster;
    }
    uint32_t FileSystem::bytes_per_cluster() const {
        return bpb.bytes_per_sector * bpb.sectors_per_cluster;
    }

    uint32_t FileSystem::cluster_to_sector(const uint32_t cluster) const {
        return data_start + (cluster - 2) * bpb.sectors_per_cluster;
    }

    // ============================================================================
    // Cache
    // ============================================================================

    bool FileSystem::read_fat_sector(uint32_t fat_sector, uint8_t* buffer) const {
        cache_access_counter++;

        // Search for cached entry
        for (auto& [sector, data, lastUsed, valid] : fat_cache) {
            if (valid && sector == fat_sector) {
                cache_stats.hits++;
                lastUsed = cache_access_counter;
                memcpy(buffer, data, bpb.bytes_per_sector);
                return true;
            }
        }

        // Cache miss - read from disk
        cache_stats.misses++;
        if (!device->read(fat_sector, 1, buffer, bpb.bytes_per_sector)) return false;

        // Find slot for new entry (LRU replacement)
        size_t replace_idx = 0;
        uint32_t oldest_access = fat_cache[0].valid ? fat_cache[0].last_used : 0;

        for (size_t i = 0; i < FAT_CACHE_SIZE; ++i) {
            if (!fat_cache[i].valid) {
                // Found empty slot
                replace_idx = i;
                break;
            }

            if (fat_cache[i].last_used < oldest_access) {
                oldest_access = fat_cache[i].last_used;
                replace_idx = i;
            }
        }

        // Store in cache
        fat_cache[replace_idx].sector = fat_sector;
        memcpy(fat_cache[replace_idx].data, buffer, bpb.bytes_per_sector);
        fat_cache[replace_idx].last_used = cache_access_counter;
        fat_cache[replace_idx].valid = true;

        return true;
    }

    void FileSystem::invalidate_fat_cache() const {
        for (auto& i : fat_cache) {
            i.valid = false;
        }
    }

    void FileSystem::invalidate_fat_cache_sector(uint32_t sector) const {
        for (auto& i : fat_cache) {
            if (i.valid && i.sector == sector) {
                i.valid = false;
            }
        }
    }

    // ============================================================================
    // FSInfo
    // ============================================================================

    bool FileSystem::load_fs_info() {
        free_cluster_count = 0xFFFFFFFF;
        next_free_cluster = 2;

        if (bpb.fs_info == 0) return false;

        FSINFO fsinfo{};
        if (!device->read(bpb.fs_info, 1, &fsinfo, sizeof(FSINFO))) return false;

        if (fsinfo.lead_sig != 0x41615252) return false;
        if (fsinfo.struc_sig != 0x61417272) return false;
        if (fsinfo.trail_sig != 0xAA550000) return false;

        free_cluster_count = fsinfo.free_count;
        next_free_cluster = fsinfo.nxt_free;

        if (next_free_cluster < 2) next_free_cluster = 2;

        return true;
    }

    void FileSystem::write_fs_info() const {
        if (bpb.fs_info == 0) return;

        FSINFO fsinfo{};
        fsinfo.lead_sig = 0x41615252;
        fsinfo.struc_sig = 0x61417272;
        fsinfo.free_count = free_cluster_count;
        fsinfo.nxt_free = next_free_cluster;
        fsinfo.trail_sig = 0xAA550000;

        device->write(bpb.fs_info, 1, &fsinfo, sizeof(FSINFO));
    }

    uint32_t FileSystem::get_free_cluster_count() {
        if (free_cluster_count != 0xFFFFFFFF) return free_cluster_count;

        uint32_t count = 0;
        uint8_t buf[512];
        uint32_t last_sector = UINT32_MAX;

        for (uint32_t c = 2; c < cluster_count + 2; ++c) {
            const uint32_t fat_offset = c * 4;
            const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;

            if (const uint32_t fat_sector = bpb.reserved_sector_count + sector_offset; fat_sector != last_sector) {
                if (!read_fat_sector(fat_sector, buf)) continue;
                last_sector = fat_sector;
            }

            const uint32_t offset_in_sector = fat_offset % bpb.bytes_per_sector;

            if (const uint32_t entry = *reinterpret_cast<uint32_t*>(buf + offset_in_sector) & 0x0FFFFFFF; entry == 0) {
                ++count;
            }
        }

        free_cluster_count = count;
        write_fs_info();
        return free_cluster_count;
    }

    // ============================================================================
    // Cluster I/O Operations
    // ============================================================================

    ssize_t FileSystem::read_cluster(const uint32_t cluster, void* buffer, size_t buffer_size) const {
        const uint32_t sector = cluster_to_sector(cluster);
        return device->read(sector, bpb.sectors_per_cluster, buffer, buffer_size);
    }

    bool FileSystem::write_cluster(uint32_t cluster, const void* data, size_t len, size_t offset) const {
        if (!data || len == 0) return false;

        const uint32_t cluster_bytes = bytes_per_cluster();
        if (len + offset > cluster_bytes) return false;

        uint8_t* cluster_buffer = alloc_cluster_buffer(cluster_bytes);
        if (!cluster_buffer) return false;

        // Read existing data if partial write
        if (len < cluster_bytes || offset > 0) {
            if (!read_cluster(cluster, cluster_buffer, cluster_bytes)) {
                free_cluster_buffer(cluster_buffer, cluster_bytes);
                return false;
            }
        }

        memcpy(cluster_buffer + offset, data, len);

        const uint32_t sector = cluster_to_sector(cluster);
        bool ok = device->write(sector, bpb.sectors_per_cluster, cluster_buffer, cluster_bytes);

        free_cluster_buffer(cluster_buffer, cluster_bytes);
        return ok;
    }

    // ============================================================================
    // FAT Table Operations
    // ============================================================================

    bool FileSystem::is_valid_fat_entry(uint32_t value) const {
        value &= 0x0FFFFFFF;

        if (value == 0) return true;            // free
        if (value >= 0x0FFFFFF8) return true;   // EOF
        if (value == 0x0FFFFFF7) return false;  // bad
        if (value < 2) return false;
        if (value >= cluster_count + 2) return false;

        return true;
    }

    uint32_t FileSystem::read_fat_entry_raw(const uint32_t fat_sector, uint32_t offset) const {
        uint8_t buf[1024];  // max 2 sectors

        // one sector
        if (offset <= sector_size - 4) {
            if (!read_fat_sector(fat_sector, buf)) return 0x0FFFFFFF;

            return *reinterpret_cast<uint32_t*>(buf + offset);
        }

        // two sectors
        if (!read_fat_sector(fat_sector, buf)) return 0x0FFFFFFF;

        if (!read_fat_sector(fat_sector + 1, buf + sector_size)) return 0x0FFFFFFF;

        return *reinterpret_cast<uint32_t*>(buf + offset);
    }

    uint32_t FileSystem::get_fat_entry(uint32_t cluster) const {
        if (cluster < 2 || cluster >= cluster_count + 2) return 0x0FFFFFFF;

        const uint32_t fat_offset = cluster * 4;
        const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
        const uint32_t offset = fat_offset % bpb.bytes_per_sector;

        // === FAT0 ===
        const uint32_t fat0_sector = bpb.reserved_sector_count + sector_offset;

        uint32_t entry0 = read_fat_entry_raw(fat0_sector, offset) & 0x0FFFFFFF;

        if (is_valid_fat_entry(entry0)) return entry0;

        // === FAT1 fallback ===
        if (bpb.table_count < 2) return entry0;

        const uint32_t fat1_sector = bpb.reserved_sector_count + bpb.fat_size32 + sector_offset;

        if (uint32_t entry1 = read_fat_entry_raw(fat1_sector, offset) & 0x0FFFFFFF; is_valid_fat_entry(entry1)) {
            // repair FAT0
            const_cast<FileSystem*>(this)->write_fat_entry(cluster, entry1);
            return entry1;
        }

        // Both broken → Force EOF
        return 0x0FFFFFFF;
    }

    uint32_t FileSystem::read_fat_entry(uint32_t cluster, Sector& sec) const {
        const uint32_t fat_offset = cluster * 4;
        const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
        const uint32_t offset = fat_offset % bpb.bytes_per_sector;

        if (const uint32_t fat_sector = bpb.reserved_sector_count + sector_offset; sec.sector != fat_sector) {
            if (!read_fat_sector(fat_sector, sec.buf)) return 0x0FFFFFFF;
            sec.sector = fat_sector;
        }

        uint32_t entry = *reinterpret_cast<uint32_t*>(sec.buf + offset) & 0x0FFFFFFF;

        if (entry >= 0x0FFFFFF8 || entry == 0 || entry == 1 || entry == 0x0FFFFFF7) return 0;

        return entry;
    }

    bool FileSystem::write_fat_entry_raw(uint32_t fat_sector, uint32_t offset, uint32_t value) const {
        uint8_t buf[1024];

        // one sector
        if (offset <= sector_size - 4) {
            if (!device->read(fat_sector, 1, buf, sector_size)) return false;

            *reinterpret_cast<uint32_t*>(buf + offset) = value;

            if (!device->write(fat_sector, 1, buf, sector_size)) return false;

            invalidate_fat_cache_sector(fat_sector);
            return true;
        }

        // two sectors
        if (!device->read(fat_sector, 2, buf, sector_size * 2)) return false;

        *reinterpret_cast<uint32_t*>(buf + offset) = value;

        if (!device->write(fat_sector, 2, buf, sector_size * 2)) return false;

        invalidate_fat_cache_sector(fat_sector);
        invalidate_fat_cache_sector(fat_sector + 1);
        return true;
    }

    bool FileSystem::write_fat_entry(uint32_t cluster, uint32_t value) {
        value &= 0x0FFFFFFF;

        uint32_t old = get_fat_entry(cluster) & 0x0FFFFFFF;

        const uint32_t fat_offset = cluster * 4;
        const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
        const uint32_t offset_in_sector = fat_offset % bpb.bytes_per_sector;

        for (uint32_t fat = 0; fat < bpb.table_count; ++fat) {
            const uint32_t fat_base = bpb.reserved_sector_count + fat * bpb.fat_size32;

            if (const uint32_t sector = fat_base + sector_offset; !write_fat_entry_raw(sector, offset_in_sector, value))
                return false;
        }

        if (free_cluster_count != 0xFFFFFFFF) {
            const bool was_free = (old == 0);

            if (const bool is_free = (value == 0); was_free && !is_free)
                free_cluster_count--;
            else if (!was_free && is_free)
                free_cluster_count++;
        }

        return true;
    }

    uint32_t FileSystem::next_cluster(uint32_t c) const {
        if (c < 2 || c >= cluster_count + 2) return 0;

        uint32_t next = get_fat_entry(c);

        if (next >= 0x0FFFFFF8)  // EOF
            return 0;

        if (next == 0 || next == 1)  // free / invalid
            return 0;

        if (next == 0x0FFFFFF7)  // bad
            return 0;

        return next;
    }

    // Floyd Cycle Detection
    bool FileSystem::has_fat_loop(uint32_t start) const {
        uint32_t tortoise = start;
        uint32_t hare = start;

        while (true) {
            tortoise = next_cluster(tortoise);
            if (tortoise == 0) return false;

            hare = next_cluster(hare);
            if (hare == 0) return false;

            hare = next_cluster(hare);
            if (hare == 0) return false;

            if (tortoise == hare) return true;  // Loop detected
        }
    }

    uint32_t FileSystem::find_free_cluster() {
        if (cluster_count < 2) return 0;

        const uint32_t start =
            (next_free_cluster >= 2 && next_free_cluster < cluster_count + 2) ? next_free_cluster : 2;

        for (uint32_t c = start; c < cluster_count + 2; ++c) {
            if (get_fat_entry(c) == 0) {
                next_free_cluster = c + 1;
                return c;
            }
        }

        // Fallback: complete scan
        for (uint32_t c = 2; c < start; ++c) {
            if (get_fat_entry(c) == 0) {
                next_free_cluster = c + 1;
                return c;
            }
        }

        return 0;
    }

    uint32_t* FileSystem::get_cluster_chain(const uint32_t start_cluster, size_t& out_count) const {
        out_count = 0;

        if (start_cluster < 2 || start_cluster >= cluster_count + 2) return nullptr;

        if (has_fat_loop(start_cluster)) return nullptr;

        // Phase 1: Cluster zählen mit Batch-Reads
        const uint32_t entries_per_sector = bpb.bytes_per_sector / 4;
        constexpr uint32_t sectors_per_read = 128;  // 64 KiB @ 512 B sectors
        const uint32_t bytes_needed = sectors_per_read * bpb.bytes_per_sector;
        const uint32_t pages = (bytes_needed + 0xFFF) / 0x1000;

        virt_addr_t batch_virt = kernel::memory::request_pages(pages);
        if (virt_null(batch_virt)) return nullptr;
        auto* batch_buffer = virt_as<uint32_t>(batch_virt);

        uint32_t cluster = start_cluster;
        size_t count = 0;
        uint32_t current_batch_sector = UINT32_MAX;

        while (cluster >= 2 && cluster < cluster_count + 2) {
            ++count;

            const uint32_t fat_offset = cluster * 4;
            const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
            const uint32_t fat_sector = bpb.reserved_sector_count + sector_offset;
            const uint32_t batch_start = (fat_sector / sectors_per_read) * sectors_per_read;

            // Load new batch if necessary
            if (batch_start != current_batch_sector) {
                const size_t sectors_to_read =
                    ((bpb.reserved_sector_count + bpb.fat_size32) - batch_start < sectors_per_read)
                        ? (bpb.reserved_sector_count + bpb.fat_size32) - batch_start
                        : sectors_per_read;

                if (!device->read(batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector)) {
                    kernel::memory::free_pages(batch_virt, pages);
                    return nullptr;
                }
                current_batch_sector = batch_start;
            }

            const uint32_t index_in_batch =
                (fat_sector - batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
            const uint32_t entry = batch_buffer[index_in_batch] & 0x0FFFFFFF;

            if (entry >= 0x0FFFFFF8 || entry == 0 || entry == 1 || entry == 0x0FFFFFF7) break;

            cluster = entry;
        }

        if (count == 0) {
            kernel::memory::free_pages(batch_virt, pages);
            return nullptr;
        }

        auto* chain = static_cast<uint32_t*>(kernel::memory::malloc(count * sizeof(uint32_t)));
        if (!chain) {
            kernel::memory::free_pages(batch_virt, pages);
            return nullptr;
        }

        cluster = start_cluster;
        current_batch_sector = UINT32_MAX;

        for (size_t i = 0; i < count; ++i) {
            chain[i] = cluster;

            const uint32_t fat_offset = cluster * 4;
            const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
            const uint32_t fat_sector = bpb.reserved_sector_count + sector_offset;
            const uint32_t batch_start = (fat_sector / sectors_per_read) * sectors_per_read;

            if (batch_start != current_batch_sector) {
                const size_t sectors_to_read =
                    ((bpb.reserved_sector_count + bpb.fat_size32) - batch_start < sectors_per_read)
                        ? (bpb.reserved_sector_count + bpb.fat_size32) - batch_start
                        : sectors_per_read;

                device->read(batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector);
                current_batch_sector = batch_start;
            }

            const uint32_t index_in_batch =
                (fat_sector - batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
            cluster = batch_buffer[index_in_batch] & 0x0FFFFFFF;
        }

        kernel::memory::free_pages(batch_virt, pages);
        out_count = count;
        return chain;
    }

    bool FileSystem::free_cluster_chain(const uint32_t start_cluster) {
        if (start_cluster < 2 || start_cluster >= cluster_count + 2) return false;

        size_t count = 0;
        uint32_t* chain = get_cluster_chain(start_cluster, count);
        if (!chain || count == 0) return false;

        klib::sort(chain, chain + count);

        const uint32_t entries_per_sector = bpb.bytes_per_sector / 4;
        constexpr uint32_t sectors_per_batch = 128;  // 64KB Batches
        const uint32_t bytes_needed = sectors_per_batch * bpb.bytes_per_sector;
        const uint32_t pages = (bytes_needed + 0xFFF) / 0x1000;

        virt_addr_t batch_virt = kernel::memory::request_pages(pages);
        if (virt_null(batch_virt)) {
            kernel::memory::free(chain);
            return false;
        }
        auto* batch_buffer = virt_as<uint32_t>(batch_virt);

        // Für jede FAT-Kopie
        for (uint32_t fat = 0; fat < bpb.table_count; ++fat) {
            const uint32_t fat_base = bpb.reserved_sector_count + fat * bpb.fat_size32;

            uint32_t current_batch_start = UINT32_MAX;
            uint32_t current_batch_end = 0;
            bool batch_dirty = false;

            for (size_t i = 0; i < count; ++i) {
                const uint32_t cluster = chain[i];
                const uint32_t fat_offset = cluster * 4;
                const uint32_t sector_offset = fat_offset / bpb.bytes_per_sector;
                const uint32_t fat_sector = fat_base + sector_offset;
                const uint32_t batch_start = (fat_sector / sectors_per_batch) * sectors_per_batch;
                const uint32_t batch_end = batch_start + sectors_per_batch;

                // Neue Batch?
                if (batch_start != current_batch_start) {
                    // Alte Batch schreiben
                    if (batch_dirty) {
                        const size_t sectors_to_write = current_batch_end - current_batch_start;
                        device->write(
                            current_batch_start, sectors_to_write, batch_buffer, sectors_to_write * bpb.bytes_per_sector
                        );

                        // Cache invalidieren
                        for (uint32_t s = current_batch_start; s < current_batch_end; ++s)
                            invalidate_fat_cache_sector(s);
                    }

                    // Neue Batch laden
                    current_batch_start = batch_start;
                    current_batch_end = (batch_end > fat_base + bpb.fat_size32) ? fat_base + bpb.fat_size32 : batch_end;

                    if (const size_t sectors_to_read = current_batch_end - current_batch_start; !device->read(
                            current_batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector
                        )) {
                        kernel::memory::free_pages(batch_virt, pages);
                        kernel::memory::free(chain);
                        return false;
                    }

                    batch_dirty = false;
                }

                // Entry in Batch auf 0 setzen
                const uint32_t index_in_batch =
                    (fat_sector - current_batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
                batch_buffer[index_in_batch] = 0;
                batch_dirty = true;
            }

            // Letzte Batch schreiben
            if (batch_dirty) {
                const size_t sectors_to_write = current_batch_end - current_batch_start;
                device->write(
                    current_batch_start, sectors_to_write, batch_buffer, sectors_to_write * bpb.bytes_per_sector
                );

                for (uint32_t s = current_batch_start; s < current_batch_end; ++s) invalidate_fat_cache_sector(s);
            }
        }

        kernel::memory::free_pages(batch_virt, pages);

        // Free cluster count aktualisieren
        if (free_cluster_count != 0xFFFFFFFF) free_cluster_count += count;

        kernel::memory::free(chain);
        return true;
    }

    // ============================================================================
    // Directory Operations
    // ============================================================================

    FileEntry* FileSystem::read_directory(const char* path, size_t& out_count) const {
        out_count = 0;
        uint32_t cluster = resolve_path_to_cluster(path);
        if (cluster == 0) return nullptr;

        return read_directory(cluster, out_count);
    }

    FileEntry* FileSystem::read_directory(uint32_t cluster, size_t& out_count) const {
        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));
        if (!entries) {
            out_count = 0;
            return nullptr;
        }

        out_count = 0;
        size_t chain_count = 0;
        uint32_t* chain = get_cluster_chain(cluster, chain_count);
        if (!chain) {
            kernel::memory::free(entries);
            return nullptr;
        }

        const uint32_t cluster_bytes = bytes_per_cluster();
        const size_t entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        LfnBufferEntry lfn_buffer[20];
        size_t lfn_count = 0;

        for (size_t ci = 0; ci < chain_count; ++ci) {
            uint8_t* cluster_buffer = alloc_cluster_buffer(cluster_bytes);
            if (!cluster_buffer) continue;

            if (!read_cluster(chain[ci], cluster_buffer, cluster_bytes)) {
                free_cluster_buffer(cluster_buffer, cluster_bytes);
                continue;
            }

            for (size_t i = 0; i < entries_per_cluster; i++) {
                const auto entry = reinterpret_cast<DirectoryEntry*>(cluster_buffer + i * sizeof(DirectoryEntry));

                // end
                if (entry->name[0] == 0x00) {
                    break;
                }  // deleted or volume label
                if (entry->name[0] == 0xE5 || entry->attr == ATTR_VOLUME_ID) {
                    continue;
                }

                // Handle LFN entries
                if (entry->attr == ATTR_LONG_NAME) {
                    if (lfn_count < 20) {
                        lfn_buffer[lfn_count++].lfn_entry = *reinterpret_cast<LongFileName*>(entry);
                    }
                    continue;
                }

                // Regular entry
                entries[out_count].set_is_dir((entry->attr & ATTR_DIRECTORY) != 0);

                // Process collected LFN entries
                if (lfn_count > 0) {
                    char name_buffer[256];
                    size_t pos = 0;

                    // LFN entries are stored in descending order in the buffer,
                    // so they are processed from back to front.
                    for (int j = static_cast<int>(lfn_count) - 1; j >= 0; --j) {
                        copy_lfn_part(&lfn_buffer[j].lfn_entry, name_buffer, pos, sizeof(name_buffer));
                    }

                    name_buffer[pos] = '\0';
                    entries[out_count].set_long_name(name_buffer);
                    lfn_count = 0;
                }

                else {
                    entries[out_count].set_long_name(nullptr);
                }

                // Set short name
                char short_name[13];
                extract_short_name(entry->name, short_name, sizeof(short_name));
                entries[out_count].set_directory_entry(*entry);
                entries[out_count].set_short_name(short_name);
                entries[out_count].set_index_in_cluster(ci * entries_per_cluster + i);

                out_count++;
                if (out_count >= READ_DIR_MAX_ENTRIES) break;
            }

            free_cluster_buffer(cluster_buffer, cluster_bytes);
            if (out_count >= READ_DIR_MAX_ENTRIES) break;
        }

        kernel::memory::free(chain);
        return entries;
    }

    bool FileSystem::overwrite_directory_entry(
        const uint32_t parent_cluster, const size_t entry_index, const DirectoryEntry* new_entry
    ) const {
        size_t out_cluster_count = 0;
        uint32_t* clusters = get_cluster_chain(parent_cluster, out_cluster_count);
        if (!clusters) return false;

        const uint32_t cluster_bytes = bytes_per_cluster();
        const size_t entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);
        const size_t cluster_idx = entry_index / entries_per_cluster;
        const size_t offset_in_cluster = entry_index % entries_per_cluster;

        if (cluster_idx >= out_cluster_count) {
            kernel::memory::free(clusters);
            return false;
        }

        const uint32_t target_cluster = clusters[cluster_idx];
        uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
        if (!buffer) {
            kernel::memory::free(clusters);
            return false;
        }

        if (!read_cluster(target_cluster, buffer, cluster_bytes)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(clusters);
            return false;
        }

        auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        entries[offset_in_cluster] = *new_entry;

        bool ok = device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(clusters);
        return ok;
    }

    // ============================================================================
    // File Operations
    // ============================================================================

    bool FileSystem::read_file(Fat32Node* node, void* buffer, size_t len, size_t& out_actual, size_t offset) const {
        if (!node || !buffer || len == 0) return false;

        if (offset >= node->file_size) {
            out_actual = 0;
            return true;
        }

        const uint32_t start_cluster = node->cluster;
        if (start_cluster == 0) {
            out_actual = 0;
            return true;
        }

        const size_t cluster_bytes = bytes_per_cluster();
        const size_t remaining = node->file_size - offset;
        const size_t to_read = (len < remaining) ? len : remaining;
        const size_t cluster_index = offset / cluster_bytes;
        const size_t offset_in_cluster = offset % cluster_bytes;

        size_t out_cluster_count = 0;
        uint32_t* cluster_chain = get_cluster_chain(start_cluster, out_cluster_count);
        if (!cluster_chain) return false;

        if (cluster_index >= out_cluster_count) {
            kernel::memory::free(cluster_chain);
            out_actual = 0;
            return true;
        }

        auto* dest = static_cast<uint8_t*>(buffer);
        size_t bytes_read = 0;

        for (size_t i = cluster_index; i < out_cluster_count && bytes_read < to_read; i++) {
            uint8_t* cluster_buffer = alloc_cluster_buffer(cluster_bytes);
            if (!cluster_buffer) {
                kernel::memory::free(cluster_chain);
                return false;
            }

            if (!read_cluster(cluster_chain[i], cluster_buffer, cluster_bytes)) {
                free_cluster_buffer(cluster_buffer, cluster_bytes);
                kernel::memory::free(cluster_chain);
                return false;
            }

            const size_t start_pos = (i == cluster_index) ? offset_in_cluster : 0;
            const size_t available_in_cluster = cluster_bytes - start_pos;
            const size_t to_copy =
                (to_read - bytes_read < available_in_cluster) ? (to_read - bytes_read) : available_in_cluster;

            memcpy(dest + bytes_read, cluster_buffer + start_pos, to_copy);
            bytes_read += to_copy;

            free_cluster_buffer(cluster_buffer, cluster_bytes);
        }

        update_access_time(node->dir_entry);

        kernel::memory::free(cluster_chain);
        out_actual = bytes_read;
        return true;
    }

    bool FileSystem::write_file(Fat32Node* node, const void* buffer, const size_t len, const size_t offset) {
        if (!node || !buffer || len == 0) return false;
        if (is_protected(node->dir_entry)) return false;

        // Classic FAT32: offset > fileSize is prohibited
        if (offset > node->file_size) return false;

        const size_t cluster_bytes = bytes_per_cluster();
        const size_t new_size = (offset + len > node->file_size) ? (offset + len) : node->file_size;
        const size_t needed_clusters = (new_size + cluster_bytes - 1) / cluster_bytes;

        uint32_t start_cluster = node->cluster;
        size_t existing_clusters_count = 0;
        uint32_t* cluster_chain = nullptr;

        // Get existing cluster chain if file already has data
        if (start_cluster != 0) {
            cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
            if (!cluster_chain || existing_clusters_count == 0) return false;
        }

        // Allocate additional clusters if needed
        if (existing_clusters_count < needed_clusters) {
            size_t additional = needed_clusters - existing_clusters_count;

            // File has no cluster yet → allocate first one
            if (start_cluster == 0) {
                start_cluster = find_free_cluster();
                if (start_cluster == 0) return false;
                write_fat_entry(start_cluster, 0x0FFFFFFF);
                node->cluster = start_cluster;
                cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
                if (!cluster_chain) return false;
            }

            uint32_t last_cluster = cluster_chain[existing_clusters_count - 1];
            for (size_t i = 0; i < additional; i++) {
                const uint32_t free_cluster = find_free_cluster();
                if (free_cluster == 0) {
                    kernel::memory::free(cluster_chain);
                    return false;
                }
                write_fat_entry(last_cluster, free_cluster);
                write_fat_entry(free_cluster, 0x0FFFFFFF);
                last_cluster = free_cluster;
            }

            kernel::memory::free(cluster_chain);
            cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
            if (!cluster_chain || existing_clusters_count < needed_clusters) return false;
        }

        if (!cluster_chain) return false;

        auto src = static_cast<const uint8_t*>(buffer);
        size_t remaining = len;
        size_t current_offset = offset;
        size_t cluster_index = offset / cluster_bytes;

        uint8_t cluster_buf[cluster_bytes];

        while (remaining > 0 && cluster_index < existing_clusters_count) {
            const size_t offset_in_cluster = current_offset % cluster_bytes;
            const size_t space_in_cluster = cluster_bytes - offset_in_cluster;
            const size_t to_write = (remaining < space_in_cluster) ? remaining : space_in_cluster;

            if ((offset_in_cluster != 0) || (to_write < cluster_bytes)) {
                if (read_cluster(cluster_chain[cluster_index], cluster_buf, cluster_bytes) < 0) {
                    kernel::memory::free(cluster_chain);
                    return false;
                }
                memcpy(cluster_buf + offset_in_cluster, src, to_write);
                if (!write_cluster(cluster_chain[cluster_index], cluster_buf, cluster_bytes)) {
                    kernel::memory::free(cluster_chain);
                    return false;
                }
            } else {
                if (!write_cluster(cluster_chain[cluster_index], src, cluster_bytes)) {
                    kernel::memory::free(cluster_chain);
                    return false;
                }
            }

            src += to_write;
            remaining -= to_write;
            current_offset += to_write;
            cluster_index++;
        }

        kernel::memory::free(cluster_chain);

        node->file_size = new_size;
        node->dir_entry.file_size = static_cast<uint32_t>(new_size);
        node->dir_entry.first_cluster_low = static_cast<uint16_t>(start_cluster & 0xFFFF);
        node->dir_entry.first_cluster_high = static_cast<uint16_t>((start_cluster >> 16) & 0xFFFF);
        update_write_time(node->dir_entry);

        return overwrite_directory_entry(node->parent_cluster, node->current_index, &node->dir_entry);
    }

    // ============================================================================
    // LFN Support Functions
    // ============================================================================

    bool FileSystem::write_directory_entry_with_lfn(
        const uint32_t dir_cluster, const char* long_name, const char* short_name, const DirectoryEntry* short_entry
    ) {
        const size_t name_len = strlen(long_name);
        const size_t entries_needed = (12 + name_len) / 13;
        const size_t total_needed = entries_needed + 1;
        const size_t cluster_bytes = bytes_per_cluster();
        const size_t entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        size_t out_cluster_count = 0;
        uint32_t* chain = get_cluster_chain(dir_cluster, out_cluster_count);
        if (!chain) return false;

        // Try to find space in existing clusters
        for (size_t ci = 0; ci < out_cluster_count; ++ci) {
            uint32_t cluster = chain[ci];
            uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
            if (!buffer) continue;

            if (!read_cluster(cluster, buffer, cluster_bytes)) {
                free_cluster_buffer(buffer, cluster_bytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            size_t free_count = 0;
            size_t start_index = 0;

            for (size_t i = 0; i < entries_per_cluster; ++i) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    if (entries[i].attr == ATTR_VOLUME_ID) {
                        free_count = 0;
                        continue;
                    }

                    if (free_count == 0) start_index = i;
                    free_count++;

                    if (free_count >= total_needed) {
                        write_lfn_entries(entries, start_index, long_name, short_name, name_len);
                        memcpy(&entries[start_index + entries_needed], short_entry, sizeof(DirectoryEntry));

                        bool ok =
                            device->write(cluster_to_sector(cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);
                        free_cluster_buffer(buffer, cluster_bytes);
                        kernel::memory::free(chain);
                        return ok;
                    }
                } else {
                    free_count = 0;
                }
            }

            free_cluster_buffer(buffer, cluster_bytes);
        }

        // Need to allocate new cluster
        const uint32_t last_cluster = chain[out_cluster_count - 1];
        kernel::memory::free(chain);

        const uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) return false;

        if (!write_fat_entry(last_cluster, new_cluster)) return false;
        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return false;

        uint8_t* zero = alloc_cluster_buffer(cluster_bytes);
        memset(zero, 0, cluster_bytes);
        device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes);
        free_cluster_buffer(zero, cluster_bytes);

        return write_directory_entry_with_lfn(dir_cluster, long_name, short_name, short_entry);
    }

    // ============================================================================
    // Create/Delete Operations
    // ============================================================================

    bool FileSystem::create_directory(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name || name[0] == '\0') return false;

        const uint32_t parent_cluster = parent_dir->cluster;
        const uint32_t cluster_bytes = bytes_per_cluster();

        // Allocate new cluster
        const uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) return false;
        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return false;

        uint8_t* zero = alloc_cluster_buffer(cluster_bytes);
        if (!zero) return false;
        memset(zero, 0, cluster_bytes);

        // Create "." and ".." entries
        auto* dir = reinterpret_cast<DirectoryEntry*>(zero);

        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, ".          ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->first_cluster_low = new_cluster & 0xFFFF;
        dir->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
        update_create_time(*dir);

        dir++;
        memset(dir, 0, sizeof(DirectoryEntry));
        memcpy(dir->name, "..         ", 11);
        dir->attr = ATTR_DIRECTORY;
        dir->first_cluster_low = parent_cluster & 0xFFFF;
        dir->first_cluster_high = (parent_cluster >> 16) & 0xFFFF;
        update_create_time(*dir);

        bool write_ok = device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes);
        free_cluster_buffer(zero, cluster_bytes);
        if (!write_ok) return false;

        // Create directory entry
        char short_name[12] = {};
        if (!make_short_name(name, short_name)) return false;

        DirectoryEntry new_entry = {};
        memcpy(new_entry.name, short_name, 11);
        new_entry.attr = ATTR_DIRECTORY;
        new_entry.first_cluster_low = new_cluster & 0xFFFF;
        new_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
        new_entry.file_size = 0;
        update_create_time(new_entry);

        write_fs_info();

        return write_directory_entry_with_lfn(parent_cluster, name, short_name, &new_entry);
    }

    bool FileSystem::create_file(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name || name[0] == '\0') return false;

        const uint32_t parent_cluster = parent_dir->cluster;
        const uint32_t cluster_bytes = bytes_per_cluster();

        // Allocate cluster for file
        const uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) return false;

        // Initialize cluster
        uint8_t* zero = alloc_cluster_buffer(cluster_bytes);
        memset(zero, 0, cluster_bytes);
        if (!device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes)) {
            free_cluster_buffer(zero, cluster_bytes);
            return false;
        }
        free_cluster_buffer(zero, cluster_bytes);

        // Create directory entry
        char short_name[11];
        if (!make_short_name(name, short_name)) return false;

        DirectoryEntry new_entry = {};
        memcpy(new_entry.name, short_name, 11);
        new_entry.attr = ATTR_ARCHIVE;
        new_entry.first_cluster_low = new_cluster & 0xFFFF;
        new_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
        new_entry.file_size = 0;
        update_create_time(new_entry);

        if (!write_directory_entry_with_lfn(parent_cluster, name, short_name, &new_entry)) return false;

        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return false;

        write_fs_info();
        return true;
    }

    bool FileSystem::delete_directory_entry_in_directory(const uint32_t dir_cluster, const char* name) const {
        size_t chain_count = 0;
        uint32_t* chain = get_cluster_chain(dir_cluster, chain_count);
        if (!chain) return false;

        const uint32_t cluster_bytes = bytes_per_cluster();
        const size_t entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        // Struktur für gefundene Einträge
        struct FoundEntry {
            uint32_t cluster_index;
            size_t entry_index;
            size_t lfn_start_index;
            size_t lfn_count;
        };

        FoundEntry found = {0, 0, 0, 0};
        bool entry_found = false;

        // Phase 1: Finde den Eintrag
        for (size_t ci = 0; ci < chain_count && !entry_found; ++ci) {
            uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
            if (!buffer) continue;

            if (!read_cluster(chain[ci], buffer, cluster_bytes)) {
                free_cluster_buffer(buffer, cluster_bytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);

            size_t lfn_start_idx = 0;
            size_t lfn_count = 0;

            for (size_t j = 0; j < entries_per_cluster; ++j) {
                if (entries[j].name[0] == 0x00) break;
                if (entries[j].name[0] == 0xE5) {
                    lfn_count = 0;
                    continue;
                }
                if (entries[j].attr == ATTR_VOLUME_ID) {
                    lfn_count = 0;
                    continue;
                }

                // LFN Entry
                if (entries[j].attr == ATTR_LONG_NAME) {
                    if (lfn_count == 0) lfn_start_idx = j;
                    lfn_count++;
                    continue;
                }

                // Regular Entry - Name rekonstruieren
                char full_name[256] = {};

                if (lfn_count > 0) {
                    // LFN Name aus gesammelten Entries
                    LfnBufferEntry lfn_buffer[20];
                    const size_t actual_lfn_count = (lfn_count < 20) ? lfn_count : 20;

                    for (size_t l = 0; l < actual_lfn_count; ++l) {
                        lfn_buffer[l].lfn_entry = *reinterpret_cast<LongFileName*>(&entries[lfn_start_idx + l]);
                    }

                    // Sort by order
                    for (size_t a = 0; a < actual_lfn_count - 1; a++) {
                        for (size_t b = 0; b < actual_lfn_count - 1 - a; b++) {
                            if ((lfn_buffer[b].lfn_entry.order & 0x3F) > (lfn_buffer[b + 1].lfn_entry.order & 0x3F)) {
                                auto tmp = lfn_buffer[b];
                                lfn_buffer[b] = lfn_buffer[b + 1];
                                lfn_buffer[b + 1] = tmp;
                            }
                        }
                    }

                    size_t pos = 0;
                    for (size_t l = 0; l < actual_lfn_count; ++l)
                        copy_lfn_part(&lfn_buffer[l].lfn_entry, full_name, pos, sizeof(full_name));
                    full_name[pos] = '\0';
                } else {
                    extract_short_name(entries[j].name, full_name, sizeof(full_name));
                }

                // Match gefunden?
                if (strcasecmp(full_name, name) == 0) {
                    found.cluster_index = ci;
                    found.entry_index = j;
                    found.lfn_start_index = lfn_start_idx;
                    found.lfn_count = lfn_count;
                    entry_found = true;
                    break;
                }

                lfn_count = 0;
            }

            free_cluster_buffer(buffer, cluster_bytes);
        }

        if (!entry_found) {
            kernel::memory::free(chain);
            return false;
        }

        // Phase 2: Lösche den Eintrag
        uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
        if (!buffer) {
            kernel::memory::free(chain);
            return false;
        }

        if (!read_cluster(chain[found.cluster_index], buffer, cluster_bytes)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(chain);
            return false;
        }

        auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);

        // Markiere Short Entry als gelöscht
        entries[found.entry_index].name[0] = 0xE5;

        // Markiere alle LFN Entries als gelöscht
        for (size_t i = 0; i < found.lfn_count; ++i) {
            entries[found.lfn_start_index + i].name[0] = 0xE5;
        }

        bool success = device->write(
            cluster_to_sector(chain[found.cluster_index]), bpb.sectors_per_cluster, buffer, cluster_bytes
        );

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(chain);

        return success;
    }

    bool FileSystem::delete_file(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name) return false;

        const uint32_t parent_cluster = parent_dir->cluster;

        size_t entry_count = 0;
        FileEntry* entries = read_directory(parent_cluster, entry_count);
        if (!entries) return false;

        uint32_t start_cluster = 0;
        DirectoryEntry victim;
        bool found = false;

        for (size_t i = 0; i < entry_count; ++i) {
            if (!entries[i].is_dir() && strcmp(entries[i].get_name(), name) == 0) {
                start_cluster = entries[i].get_first_cluster();
                victim = entries[i].get_directory_entry();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);
        if (!found) return false;

        if (is_protected(victim)) return false;

        if (start_cluster != 0) free_cluster_chain(start_cluster);

        write_fs_info();
        return delete_directory_entry_in_directory(parent_cluster, name);
    }

    bool FileSystem::remove_directory(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name) return false;

        const uint32_t parent_cluster = parent_dir->cluster;

        size_t entry_count = 0;
        FileEntry* entries = read_directory(parent_cluster, entry_count);
        if (!entries) return false;

        uint32_t target_cluster = 0;
        bool found = false;

        for (size_t i = 0; i < entry_count; ++i) {
            if (entries[i].is_dir() && strcmp(entries[i].get_name(), name) == 0) {
                target_cluster = entries[i].get_first_cluster();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);
        if (!found) return false;

        // Check empty (only "." and "..")
        size_t dir_entry_count = 0;
        FileEntry* dir_entries = read_directory(target_cluster, dir_entry_count);
        if (!dir_entries || dir_entry_count > 2) {
            kernel::memory::free(dir_entries);
            return false;
        }
        kernel::memory::free(dir_entries);

        free_cluster_chain(target_cluster);

        write_fs_info();
        return delete_directory_entry_in_directory(parent_cluster, name);
    }

    // ============================================================================
    // Rename Operation
    // ============================================================================

    bool FileSystem::rename(const Fat32Node* parent_dir, const char* old_name, const char* new_name) {
        if (!parent_dir || !old_name || !new_name || old_name[0] == '\0' || new_name[0] == '\0') return false;

        const uint32_t dir_cluster = parent_dir->cluster;
        const uint32_t cluster_bytes = bytes_per_cluster();
        const size_t entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        size_t entry_count = 0;
        FileEntry* entries = read_directory(dir_cluster, entry_count);
        if (!entries || entry_count == 0) return false;

        // Find the entry to rename
        int found_index = -1;
        for (size_t i = 0; i < entry_count; ++i) {
            if (strcmp(entries[i].get_name(), old_name) == 0) {
                found_index = static_cast<int>(i);
                break;
            }
        }

        if (found_index < 0) {
            kernel::memory::free(entries);
            return false;
        }

        const DirectoryEntry old_entry = entries[found_index].get_directory_entry();
        if (is_protected(old_entry)) {
            kernel::memory::free(entries);
            return false;
        }

        size_t chain_count = 0;
        uint32_t* chain = get_cluster_chain(dir_cluster, chain_count);
        if (!chain) {
            kernel::memory::free(entries);
            return false;
        }

        // Find the actual entry on disk
        uint32_t target_cluster = 0;
        int target_entry_index = -1;

        for (size_t ci = 0; ci < chain_count && target_entry_index < 0; ++ci) {
            uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
            if (!buffer) continue;

            if (!read_cluster(chain[ci], buffer, cluster_bytes)) {
                free_cluster_buffer(buffer, cluster_bytes);
                continue;
            }

            const auto* dir_entries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (size_t i = 0; i < entries_per_cluster; ++i) {
                if (dir_entries[i].name[0] == 0x00) break;
                if (dir_entries[i].name[0] == 0xE5) continue;
                if (dir_entries[i].attr == ATTR_VOLUME_ID) continue;

                if (dir_entries[i].file_size == old_entry.file_size &&
                    dir_entries[i].first_cluster_low == old_entry.first_cluster_low &&
                    dir_entries[i].first_cluster_high == old_entry.first_cluster_high) {
                    target_cluster = chain[ci];
                    target_entry_index = static_cast<int>(i);
                    break;
                }
            }

            free_cluster_buffer(buffer, cluster_bytes);
        }

        if (target_entry_index < 0) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        uint8_t* buffer = alloc_cluster_buffer(cluster_bytes);
        if (!buffer) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        if (!read_cluster(target_cluster, buffer, cluster_bytes)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        auto* dir_entries = reinterpret_cast<DirectoryEntry*>(buffer);

        // Count old LFN entries
        int old_lfn_count = 0;
        for (int i = target_entry_index - 1; i >= 0; --i) {
            if ((dir_entries[i].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
            old_lfn_count++;
        }
        const int old_total_count = old_lfn_count + 1;
        const int old_start_index = target_entry_index - old_lfn_count;

        // Calculate new LFN entries needed
        const size_t new_name_len = strlen(new_name);
        const int new_lfn_count = static_cast<int>((new_name_len + 12) / 13);
        const int new_total_count = new_lfn_count + 1;

        // Generate new short name
        char new_short_name[12] = {};
        if (!make_short_name(new_name, new_short_name)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return false;
        }

        // Check if we can overwrite in place
        bool can_overwrite = true;
        if (new_total_count > old_total_count) {
            const int extra = new_total_count - old_total_count;
            for (int i = old_start_index - 1; i >= old_start_index - extra; --i) {
                if (i < 0 || (dir_entries[i].name[0] != 0x00 && dir_entries[i].name[0] != 0xE5)) {
                    can_overwrite = false;
                    break;
                }
            }
        }

        bool success = false;

        if (can_overwrite) {
            // Mark old entries as deleted
            for (int i = old_start_index; i < old_start_index + old_total_count; ++i) dir_entries[i].name[0] = 0xE5;

            // Write new LFN entries
            write_lfn_entries(dir_entries, old_start_index, new_name, new_short_name, new_name_len);

            // Write new short entry
            DirectoryEntry updated = old_entry;
            memcpy(updated.name, new_short_name, 11);
            dir_entries[old_start_index + new_lfn_count] = updated;

            success = device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);
        } else {
            // Delete old entry and create new one
            for (int i = old_start_index; i < old_start_index + old_total_count; ++i) dir_entries[i].name[0] = 0xE5;

            device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);

            DirectoryEntry new_entry = old_entry;
            memcpy(new_entry.name, new_short_name, 11);
            success = write_directory_entry_with_lfn(dir_cluster, new_name, new_short_name, &new_entry);
        }

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(entries);
        kernel::memory::free(chain);
        return success;
    }

    // ============================================================================
    // Path Resolution
    // ============================================================================

    uint32_t FileSystem::resolve_path_to_cluster(const char* path) const {
        if (path[0] != '/') return 0;

        uint32_t current_cluster = get_root_cluster();

        char components[16][32];
        const size_t comp_count = split_path(path, components, 16);

        for (size_t i = 0; i < comp_count; i++) {
            const uint32_t next_cluster = find_entry_cluster(current_cluster, components[i]);
            if (next_cluster == 0) return 0;
            current_cluster = next_cluster;
        }

        return current_cluster;
    }

    uint32_t FileSystem::find_entry_cluster(const uint32_t dir_cluster, const char* given_name) const {
        size_t entry_count = 0;
        FileEntry* entries = read_directory(dir_cluster, entry_count);
        if (!entries) return 0;

        uint32_t result = 0;
        for (size_t i = 0; i < entry_count; i++) {
            if (const char* entry_name = entries[i].get_name(); strcmp(entry_name, given_name) == 0) {
                result = entries[i].get_first_cluster();
                break;
            }
        }

        kernel::memory::free(entries);
        return result;
    }

    // ============================================================================
    // FileEntry Helper
    // ============================================================================

    void FileEntry::format_short_name() {
        char name[9] = {};
        char ext[4] = {};

        memcpy(name, short_name_, 8);
        for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

        memcpy(ext, short_name_ + 8, 3);
        for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';

        const size_t name_len = strlen(name);
        memcpy(formatted_short_name_, name, name_len);

        if (ext[0] != '\0') {
            formatted_short_name_[name_len] = '.';
            memcpy(formatted_short_name_ + name_len + 1, ext, strlen(ext));
            formatted_short_name_[name_len + 1 + strlen(ext)] = '\0';
        } else {
            formatted_short_name_[name_len] = '\0';
        }
    }

    bool FileSystem::is_protected(const DirectoryEntry& e) {
        return e.attr & (ATTR_READ_ONLY | ATTR_SYSTEM);
    }
}  // namespace fat32
