//
// Created by linus on 03.07.25.
//

#include "fat32.h"

#include <klib/path.h>
#include <klib/sort.h>
#include <klib/string.h>
#include <vespera/log.h>

#include "fat32_lfn.h"
#include "fat32_time.h"
#include "fat32_vfs_adapter.h"
#include "vespera_errno.h"

namespace fat32 {
    // ============================================================================
    // Helper Functions - Memory Management
    // ============================================================================

    static u8* alloc_cluster_buffer(const u32 cluster_bytes) {
        const usize pages = (cluster_bytes + 0xFFF) / 0x1000;
        const virt_addr_t page = kernel::memory::request_pages(pages);
        if (!virt_null(page)) memset(page, 0, pages * 0x1000);
        return virt_as<u8>(page);
    }

    static void free_cluster_buffer(const u8* ptr, const u32 cluster_bytes) {
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
        u8 sector[512];
        if (!device->read(0, 1, sector, 512)) {
            Log::error("[FAT32] Failed to read first sector");
            return;
        }

        memcpy(&bpb, sector, sizeof(BPB_FAT32));

        if (bpb.table_count < 1 || bpb.sectors_per_cluster == 0) return;

        const u32 total_sectors = (bpb.total_sectors16 != 0) ? bpb.total_sectors16 : bpb.total_sectors32;

        const u32 data_sectors = total_sectors - (bpb.reserved_sector_count + (bpb.table_count * bpb.fat_size32));

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
        device_lost_ = false;
    }

    FileSystem::~FileSystem() {
        if (!device_lost_) {
            write_fs_info();
        }
    }

    bool FileSystem::probe_fs() const {
        return bpb.root_entry_count == 0 && bpb.fat_size16 == 0 && cluster_count >= 65525;
    }

    bool FileSystem::is_valid() const {
        return fs_valid;
    }

    u32 FileSystem::get_root_cluster() const {
        return bpb.root_cluster;
    }

    u32 FileSystem::bytes_per_cluster() const {
        return bpb.bytes_per_sector * bpb.sectors_per_cluster;
    }

    u32 FileSystem::cluster_to_sector(const u32 cluster) const {
        return data_start + (cluster - 2) * bpb.sectors_per_cluster;
    }

    // ============================================================================
    // Cache
    // ============================================================================

    bool FileSystem::read_fat_sector(const u32 fat_sector, u8* buffer) const {
        cache_access_counter++;

        for (auto& [sector, data, lastUsed, valid] : fat_cache) {
            if (valid && sector == fat_sector) {
                cache_stats.hits++;
                lastUsed = cache_access_counter;
                memcpy(buffer, data, bpb.bytes_per_sector);
                return true;
            }
        }

        cache_stats.misses++;
        if (!device->read(fat_sector, 1, buffer, bpb.bytes_per_sector)) return false;

        usize replace_idx = 0;
        u32 oldest_access = fat_cache[0].valid ? fat_cache[0].last_used : 0;

        for (usize i = 0; i < FAT_CACHE_SIZE; ++i) {
            if (!fat_cache[i].valid) {
                replace_idx = i;
                break;
            }
            if (fat_cache[i].last_used < oldest_access) {
                oldest_access = fat_cache[i].last_used;
                replace_idx = i;
            }
        }

        fat_cache[replace_idx].sector = fat_sector;
        memcpy(fat_cache[replace_idx].data, buffer, bpb.bytes_per_sector);
        fat_cache[replace_idx].last_used = cache_access_counter;
        fat_cache[replace_idx].valid = true;

        return true;
    }

    void FileSystem::invalidate_fat_cache() const {
        for (auto& i : fat_cache) i.valid = false;
    }

    void FileSystem::invalidate_fat_cache_sector(const u32 sector) const {
        for (auto& i : fat_cache) {
            if (i.valid && i.sector == sector) i.valid = false;
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

    u32 FileSystem::get_free_cluster_count() {
        if (free_cluster_count != 0xFFFFFFFF) return free_cluster_count;

        u32 count = 0;
        u8 buf[512];
        u32 last_sector = U32_MAX;

        for (u32 c = 2; c < cluster_count + 2; ++c) {
            const u32 fat_offset = c * 4;
            const u32 sector_offset = fat_offset / bpb.bytes_per_sector;

            if (const u32 fat_sector = bpb.reserved_sector_count + sector_offset; fat_sector != last_sector) {
                if (!read_fat_sector(fat_sector, buf)) continue;
                last_sector = fat_sector;
            }

            const u32 offset_in_sector = fat_offset % bpb.bytes_per_sector;

            if (const u32 entry = *reinterpret_cast<u32*>(buf + offset_in_sector) & 0x0FFFFFFF; entry == 0) {
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

    isize FileSystem::read_cluster(const u32 cluster, void* buffer, const usize buffer_size) const {
        const u32 sector = cluster_to_sector(cluster);
        return device->read(sector, bpb.sectors_per_cluster, buffer, buffer_size);
    }

    bool FileSystem::write_cluster(const u32 cluster, const void* data, const usize len, const usize offset) const {
        if (!data || len == 0) return false;

        const u32 cluster_bytes = bytes_per_cluster();
        if (len + offset > cluster_bytes) return false;

        u8* cluster_buffer = alloc_cluster_buffer(cluster_bytes);
        if (!cluster_buffer) return false;

        if (len < cluster_bytes || offset > 0) {
            if (!read_cluster(cluster, cluster_buffer, cluster_bytes)) {
                free_cluster_buffer(cluster_buffer, cluster_bytes);
                return false;
            }
        }

        memcpy(cluster_buffer + offset, data, len);

        const u32 sector = cluster_to_sector(cluster);
        const bool ok = device->write(sector, bpb.sectors_per_cluster, cluster_buffer, cluster_bytes);

        free_cluster_buffer(cluster_buffer, cluster_bytes);
        return ok;
    }

    // ============================================================================
    // FAT Table Operations
    // ============================================================================

    bool FileSystem::is_valid_fat_entry(u32 value) const {
        value &= 0x0FFFFFFF;
        if (value == 0) return true;            // free
        if (value >= 0x0FFFFFF8) return true;   // EOF
        if (value == 0x0FFFFFF7) return false;  // bad
        if (value < 2) return false;
        if (value >= cluster_count + 2) return false;
        return true;
    }

    u32 FileSystem::read_fat_entry_raw(const u32 fat_sector, const u32 offset) const {
        u8 buf[1024];

        if (offset <= sector_size - 4) {
            if (!read_fat_sector(fat_sector, buf)) return 0x0FFFFFFF;
            return *reinterpret_cast<u32*>(buf + offset);
        }

        if (!read_fat_sector(fat_sector, buf)) return 0x0FFFFFFF;
        if (!read_fat_sector(fat_sector + 1, buf + sector_size)) return 0x0FFFFFFF;

        return *reinterpret_cast<u32*>(buf + offset);
    }

    u32 FileSystem::get_fat_entry(const u32 cluster) const {
        if (cluster < 2 || cluster >= cluster_count + 2) return 0x0FFFFFFF;

        const u32 fat_offset = cluster * 4;
        const u32 sector_offset = fat_offset / bpb.bytes_per_sector;
        const u32 offset = fat_offset % bpb.bytes_per_sector;

        const u32 fat0_sector = bpb.reserved_sector_count + sector_offset;
        const u32 entry0 = read_fat_entry_raw(fat0_sector, offset) & 0x0FFFFFFF;

        if (is_valid_fat_entry(entry0)) return entry0;

        if (bpb.table_count < 2) return entry0;

        const u32 fat1_sector = bpb.reserved_sector_count + bpb.fat_size32 + sector_offset;

        if (const u32 entry1 = read_fat_entry_raw(fat1_sector, offset) & 0x0FFFFFFF; is_valid_fat_entry(entry1)) {
            const_cast<FileSystem*>(this)->write_fat_entry(cluster, entry1);
            return entry1;
        }

        return 0x0FFFFFFF;
    }

    bool FileSystem::write_fat_entry_raw(const u32 fat_sector, const u32 offset, const u32 value) const {
        u8 buf[1024];

        if (offset <= sector_size - 4) {
            if (!device->read(fat_sector, 1, buf, sector_size)) return false;
            *reinterpret_cast<u32*>(buf + offset) = value;
            if (!device->write(fat_sector, 1, buf, sector_size)) return false;
            invalidate_fat_cache_sector(fat_sector);
            return true;
        }

        if (!device->read(fat_sector, 2, buf, sector_size * 2)) return false;
        *reinterpret_cast<u32*>(buf + offset) = value;
        if (!device->write(fat_sector, 2, buf, sector_size * 2)) return false;
        invalidate_fat_cache_sector(fat_sector);
        invalidate_fat_cache_sector(fat_sector + 1);
        return true;
    }

    bool FileSystem::write_fat_entry(const u32 cluster, u32 value) {
        value &= 0x0FFFFFFF;

        const u32 old = get_fat_entry(cluster) & 0x0FFFFFFF;
        const u32 fat_offset = cluster * 4;
        const u32 sector_offset = fat_offset / bpb.bytes_per_sector;
        const u32 offset_in_sector = fat_offset % bpb.bytes_per_sector;

        for (u32 fat = 0; fat < bpb.table_count; ++fat) {
            const u32 fat_base = bpb.reserved_sector_count + fat * bpb.fat_size32;
            if (const u32 sector = fat_base + sector_offset; !write_fat_entry_raw(sector, offset_in_sector, value)) {
                return false;
            }
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

    u32 FileSystem::next_cluster(const u32 c) const {
        if (c < 2 || c >= cluster_count + 2) return 0;

        const u32 next = get_fat_entry(c);

        if (next >= 0x0FFFFFF8) return 0;      // EOF
        if (next == 0 || next == 1) return 0;  // free / invalid
        if (next == 0x0FFFFFF7) return 0;      // bad

        return next;
    }

    bool FileSystem::has_fat_loop(const u32 start) const {
        u32 tortoise = start;
        u32 hare = start;

        while (true) {
            tortoise = next_cluster(tortoise);
            if (tortoise == 0) return false;

            hare = next_cluster(hare);
            if (hare == 0) return false;

            hare = next_cluster(hare);
            if (hare == 0) return false;

            if (tortoise == hare) return true;
        }
    }

    u32 FileSystem::find_free_cluster() {
        if (cluster_count < 2) return 0;

        const u32 start = (next_free_cluster >= 2 && next_free_cluster < cluster_count + 2) ? next_free_cluster : 2;

        for (u32 c = start; c < cluster_count + 2; ++c) {
            if (get_fat_entry(c) == 0) {
                next_free_cluster = c + 1;
                return c;
            }
        }

        for (u32 c = 2; c < start; ++c) {
            if (get_fat_entry(c) == 0) {
                next_free_cluster = c + 1;
                return c;
            }
        }

        return 0;
    }

    u32* FileSystem::get_cluster_chain(const u32 start_cluster, usize& out_count) const {
        out_count = 0;

        if (start_cluster < 2 || start_cluster >= cluster_count + 2) return nullptr;
        if (has_fat_loop(start_cluster)) return nullptr;

        const u32 entries_per_sector = bpb.bytes_per_sector / 4;
        constexpr u32 sectors_per_read = 128;
        const u32 bytes_needed = sectors_per_read * bpb.bytes_per_sector;
        const u32 pages = (bytes_needed + 0xFFF) / 0x1000;

        const virt_addr_t batch_virt = kernel::memory::request_pages(pages);
        if (virt_null(batch_virt)) return nullptr;
        auto* batch_buffer = virt_as<u32>(batch_virt);

        u32 cluster = start_cluster;
        usize count = 0;
        u32 current_batch_sector = U32_MAX;

        while (cluster >= 2 && cluster < cluster_count + 2) {
            ++count;

            const u32 fat_offset = cluster * 4;
            const u32 sector_offset = fat_offset / bpb.bytes_per_sector;
            const u32 fat_sector = bpb.reserved_sector_count + sector_offset;
            const u32 batch_start = (fat_sector / sectors_per_read) * sectors_per_read;

            if (batch_start != current_batch_sector) {
                const usize sectors_to_read =
                    ((bpb.reserved_sector_count + bpb.fat_size32) - batch_start < sectors_per_read)
                        ? (bpb.reserved_sector_count + bpb.fat_size32) - batch_start
                        : sectors_per_read;

                if (!device->read(batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector)) {
                    kernel::memory::free_pages(batch_virt, pages);
                    return nullptr;
                }
                current_batch_sector = batch_start;
            }

            const u32 index_in_batch =
                (fat_sector - batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
            const u32 entry = batch_buffer[index_in_batch] & 0x0FFFFFFF;

            if (entry >= 0x0FFFFFF8 || entry == 0 || entry == 1 || entry == 0x0FFFFFF7) break;

            cluster = entry;
        }

        if (count == 0) {
            kernel::memory::free_pages(batch_virt, pages);
            return nullptr;
        }

        auto* chain = static_cast<u32*>(kernel::memory::malloc(count * sizeof(u32)));
        if (!chain) {
            kernel::memory::free_pages(batch_virt, pages);
            return nullptr;
        }

        cluster = start_cluster;
        current_batch_sector = U32_MAX;

        for (usize i = 0; i < count; ++i) {
            chain[i] = cluster;

            const u32 fat_offset = cluster * 4;
            const u32 sector_offset = fat_offset / bpb.bytes_per_sector;
            const u32 fat_sector = bpb.reserved_sector_count + sector_offset;
            const u32 batch_start = (fat_sector / sectors_per_read) * sectors_per_read;

            if (batch_start != current_batch_sector) {
                const usize sectors_to_read =
                    ((bpb.reserved_sector_count + bpb.fat_size32) - batch_start < sectors_per_read)
                        ? (bpb.reserved_sector_count + bpb.fat_size32) - batch_start
                        : sectors_per_read;

                device->read(batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector);
                current_batch_sector = batch_start;
            }

            const u32 index_in_batch =
                (fat_sector - batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
            cluster = batch_buffer[index_in_batch] & 0x0FFFFFFF;
        }

        kernel::memory::free_pages(batch_virt, pages);
        out_count = count;
        return chain;
    }

    bool FileSystem::free_cluster_chain(const u32 start_cluster) {
        if (start_cluster < 2 || start_cluster >= cluster_count + 2) return false;

        usize count = 0;
        u32* chain = get_cluster_chain(start_cluster, count);
        if (!chain || count == 0) return false;

        klib::sort(chain, chain + count);

        const u32 entries_per_sector = bpb.bytes_per_sector / 4;
        constexpr u32 sectors_per_batch = 128;
        const u32 bytes_needed = sectors_per_batch * bpb.bytes_per_sector;
        const u32 pages = (bytes_needed + 0xFFF) / 0x1000;

        const virt_addr_t batch_virt = kernel::memory::request_pages(pages);
        if (virt_null(batch_virt)) {
            kernel::memory::free(chain);
            return false;
        }
        auto* batch_buffer = virt_as<u32>(batch_virt);

        for (u32 fat = 0; fat < bpb.table_count; ++fat) {
            const u32 fat_base = bpb.reserved_sector_count + fat * bpb.fat_size32;

            u32 current_batch_start = U32_MAX;
            u32 current_batch_end = 0;
            bool batch_dirty = false;

            for (usize i = 0; i < count; ++i) {
                const u32 cluster = chain[i];
                const u32 fat_offset = cluster * 4;
                const u32 sector_offset = fat_offset / bpb.bytes_per_sector;
                const u32 fat_sector = fat_base + sector_offset;
                const u32 batch_start = (fat_sector / sectors_per_batch) * sectors_per_batch;
                const u32 batch_end = batch_start + sectors_per_batch;

                if (batch_start != current_batch_start) {
                    if (batch_dirty) {
                        const usize sectors_to_write = current_batch_end - current_batch_start;
                        device->write(
                            current_batch_start, sectors_to_write, batch_buffer, sectors_to_write * bpb.bytes_per_sector
                        );
                        for (u32 s = current_batch_start; s < current_batch_end; ++s) invalidate_fat_cache_sector(s);
                    }

                    current_batch_start = batch_start;
                    current_batch_end = (batch_end > fat_base + bpb.fat_size32) ? fat_base + bpb.fat_size32 : batch_end;

                    if (const usize sectors_to_read = current_batch_end - current_batch_start; !device->read(
                            current_batch_start, sectors_to_read, batch_buffer, sectors_to_read * bpb.bytes_per_sector
                        )) {
                        kernel::memory::free_pages(batch_virt, pages);
                        kernel::memory::free(chain);
                        return false;
                    }

                    batch_dirty = false;
                }

                const u32 index_in_batch =
                    (fat_sector - current_batch_start) * entries_per_sector + (fat_offset % bpb.bytes_per_sector) / 4;
                batch_buffer[index_in_batch] = 0;
                batch_dirty = true;
            }

            if (batch_dirty) {
                const usize sectors_to_write = current_batch_end - current_batch_start;
                device->write(
                    current_batch_start, sectors_to_write, batch_buffer, sectors_to_write * bpb.bytes_per_sector
                );
                for (u32 s = current_batch_start; s < current_batch_end; ++s) invalidate_fat_cache_sector(s);
            }
        }

        kernel::memory::free_pages(batch_virt, pages);

        if (free_cluster_count != 0xFFFFFFFF) free_cluster_count += count;

        kernel::memory::free(chain);
        return true;
    }

    void FileSystem::trim_cluster_chain(const u32 start_cluster) const {
        if (!device->supports_trim()) return;

        usize count = 0;
        u32* chain = get_cluster_chain(start_cluster, count);
        if (!chain || count == 0) return;

        klib::sort(chain, chain + count);

        auto* ranges = static_cast<TrimRange*>(kernel::memory::malloc(count * sizeof(TrimRange)));
        if (!ranges) {
            kernel::memory::free(chain);
            return;
        }

        usize range_count = 0;

        for (usize i = 0; i < count; i++) {
            const u64 lba = cluster_to_sector(chain[i]);
            const u32 sector_count = bpb.sectors_per_cluster;

            if (range_count > 0 && ranges[range_count - 1].lba + ranges[range_count - 1].sector_count == lba) {
                ranges[range_count - 1].sector_count += sector_count;
            } else {
                ranges[range_count++] = {lba, sector_count};
            }
        }

        device->trim(ranges, range_count);

        kernel::memory::free(ranges);
        kernel::memory::free(chain);
    }

    // ============================================================================
    // Directory Operations
    // ============================================================================

    Result<FileEntry*> FileSystem::read_directory(const char* path, usize& out_count) const {
        out_count = 0;

        const u32 cluster = resolve_path_to_cluster(path);
        if (cluster == 0) return Result<FileEntry*>::err(Error::ENOENT);

        return read_directory(cluster, out_count);
    }

    Result<FileEntry*> FileSystem::read_directory(const u32 cluster, usize& out_count) const {
        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));

        if (!entries) {
            out_count = 0;
            return Result<FileEntry*>::err(Error::ENOMEM);
        }

        out_count = 0;

        usize chain_count = 0;
        u32* chain = get_cluster_chain(cluster, chain_count);
        if (!chain) {
            kernel::memory::free(entries);
            return Result<FileEntry*>::err(Error::EIO);
        }

        const u32 cluster_bytes = bytes_per_cluster();
        const usize entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        LfnBufferEntry lfn_buffer[20];
        usize lfn_count = 0;

        usize ci = 0;
        while (ci < chain_count) {
            usize run_end = ci;
            while (run_end + 1 < chain_count && chain[run_end] + 1 == chain[run_end + 1]) run_end++;

            const usize run_clusters = run_end - ci + 1;
            const usize run_bytes = run_clusters * cluster_bytes;
            const u32 run_lba = cluster_to_sector(chain[ci]);
            const u32 run_sectors = static_cast<u32>(run_clusters) * bpb.sectors_per_cluster;
            const usize run_pages = (run_bytes + 0xFFF) / 0x1000;

            const virt_addr_t run_virt = kernel::memory::request_pages(run_pages);
            if (virt_null(run_virt)) {
                kernel::memory::free(chain);
                kernel::memory::free(entries);
                return Result<FileEntry*>::err(Error::ENOMEM);
            }

            auto* run_buffer = virt_as<u8>(run_virt);

            if (!device->read(run_lba, run_sectors, run_buffer, run_bytes)) {
                kernel::memory::free_pages(run_virt, run_pages);
                kernel::memory::free(chain);
                kernel::memory::free(entries);
                return Result<FileEntry*>::err(Error::EIO);
            }

            bool stop = false;

            for (usize r = 0; r < run_clusters && !stop; ++r) {
                const usize actual_ci = ci + r;
                const u8* cluster_buffer = run_buffer + r * cluster_bytes;

                for (usize i = 0; i < entries_per_cluster; i++) {
                    const auto entry =
                        reinterpret_cast<const DirectoryEntry*>(cluster_buffer + i * sizeof(DirectoryEntry));

                    if (entry->name[0] == 0x00) {
                        stop = true;
                        break;
                    }

                    if (entry->name[0] == 0xE5 || entry->attr == ATTR_VOLUME_ID) continue;

                    if (entry->attr == ATTR_LONG_NAME) {
                        if (lfn_count < 20)
                            lfn_buffer[lfn_count++].lfn_entry = *reinterpret_cast<const LongFileName*>(entry);
                        continue;
                    }

                    entries[out_count].set_is_dir((entry->attr & ATTR_DIRECTORY) != 0);

                    if (lfn_count > 0) {
                        char name_buffer[256];
                        usize pos = 0;

                        for (int j = static_cast<int>(lfn_count) - 1; j >= 0; --j)
                            copy_lfn_part(&lfn_buffer[j].lfn_entry, name_buffer, pos, sizeof(name_buffer));

                        name_buffer[pos] = '\0';
                        entries[out_count].set_long_name(name_buffer);
                        lfn_count = 0;
                    } else {
                        entries[out_count].set_long_name(nullptr);
                    }

                    char short_name[13];
                    extract_short_name(entry->name, short_name, sizeof(short_name));

                    entries[out_count].set_directory_entry(*entry);
                    entries[out_count].set_short_name(short_name);
                    entries[out_count].set_index_in_cluster(actual_ci * entries_per_cluster + i);

                    out_count++;

                    if (out_count >= READ_DIR_MAX_ENTRIES) {
                        stop = true;
                        break;
                    }
                }
            }

            kernel::memory::free_pages(run_virt, run_pages);
            ci = run_end + 1;

            if (out_count >= READ_DIR_MAX_ENTRIES) break;
        }

        kernel::memory::free(chain);
        return Result<FileEntry*>::ok(entries);
    }

    bool FileSystem::overwrite_directory_entry(
        const u32 parent_cluster, const usize entry_index, const DirectoryEntry* new_entry
    ) const {
        usize out_cluster_count = 0;
        u32* clusters = get_cluster_chain(parent_cluster, out_cluster_count);
        if (!clusters) return false;

        const u32 cluster_bytes = bytes_per_cluster();
        const usize entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);
        const usize cluster_idx = entry_index / entries_per_cluster;
        const usize offset_in_cluster = entry_index % entries_per_cluster;

        if (cluster_idx >= out_cluster_count) {
            kernel::memory::free(clusters);
            return false;
        }

        const u32 target_cluster = clusters[cluster_idx];
        u8* buffer = alloc_cluster_buffer(cluster_bytes);
        if (!buffer) {
            kernel::memory::free(clusters);
            return false;
        }

        if (!read_cluster(target_cluster, buffer, cluster_bytes)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(clusters);
            return false;
        }

        auto* dir_entries = reinterpret_cast<DirectoryEntry*>(buffer);
        dir_entries[offset_in_cluster] = *new_entry;

        const bool ok =
            device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(clusters);
        return ok;
    }

    // ============================================================================
    // File Operations
    // ============================================================================

    Result<usize> FileSystem::read_file(
        Fat32Node* node, void* buffer, const usize len, const usize offset, const bool update_atime
    ) const {
        if (!node || !buffer || len == 0) return Result<usize>::err(Error::EINVAL);

        if (offset >= node->file_size) return Result<usize>::ok(0);

        const u32 start_cluster = node->cluster;
        if (start_cluster == 0) return Result<usize>::ok(0);

        const usize cluster_bytes = bytes_per_cluster();
        const usize remaining_in_file = node->file_size - offset;
        const usize to_read = (len < remaining_in_file) ? len : remaining_in_file;
        const usize cluster_index = offset / cluster_bytes;
        const usize offset_in_cluster = offset % cluster_bytes;

        usize out_cluster_count = 0;
        u32* cluster_chain = get_cluster_chain(start_cluster, out_cluster_count);
        if (!cluster_chain) return Result<usize>::err(Error::EIO);

        if (cluster_index >= out_cluster_count) {
            kernel::memory::free(cluster_chain);
            return Result<usize>::ok(0);
        }

        auto* dest = static_cast<u8*>(buffer);
        usize bytes_read = 0;
        usize i = cluster_index;

        while (i < out_cluster_count && bytes_read < to_read) {
            usize run_end = i;
            while (run_end + 1 < out_cluster_count && cluster_chain[run_end] + 1 == cluster_chain[run_end + 1])
                run_end++;

            const usize run_clusters = run_end - i + 1;
            const usize run_bytes = run_clusters * cluster_bytes;
            const u32 run_lba = cluster_to_sector(cluster_chain[i]);
            const u32 run_sectors = static_cast<u32>(run_clusters) * bpb.sectors_per_cluster;

            const usize start_pos = (i == cluster_index) ? offset_in_cluster : 0;
            const usize available = run_bytes - start_pos;
            const usize to_copy = (to_read - bytes_read < available) ? (to_read - bytes_read) : available;

            constexpr usize MAX_TRANSFER_BYTES = 64 * 1024;

            if (start_pos == 0 && to_copy == run_bytes) {
                usize rem = run_bytes;
                u32 cur_lba = run_lba;
                u8* cur_dst = dest + bytes_read;

                while (rem > 0) {
                    usize chunk_bytes = rem > MAX_TRANSFER_BYTES ? MAX_TRANSFER_BYTES : rem;
                    u32 chunk_sectors = chunk_bytes / bpb.bytes_per_sector;

                    if (!device->read(cur_lba, chunk_sectors, cur_dst, chunk_bytes)) {
                        kernel::memory::free(cluster_chain);
                        return Result<usize>::err(Error::EIO);
                    }

                    cur_lba += chunk_sectors;
                    cur_dst += chunk_bytes;
                    rem -= chunk_bytes;
                }
            } else {
                const usize pages = (run_bytes + 0xFFF) / 0x1000;
                const virt_addr_t virt = kernel::memory::request_pages(pages);
                if (virt_null(virt)) {
                    kernel::memory::free(cluster_chain);
                    return Result<usize>::err(Error::ENOMEM);
                }

                auto* tmp = virt_as<u8>(virt);
                usize rem = run_bytes;
                u32 cur_lba = run_lba;
                u8* cur_tmp = tmp;

                while (rem > 0) {
                    usize chunk_bytes = rem > MAX_TRANSFER_BYTES ? MAX_TRANSFER_BYTES : rem;
                    u32 chunk_sectors = chunk_bytes / bpb.bytes_per_sector;

                    if (!device->read(cur_lba, chunk_sectors, cur_tmp, chunk_bytes)) {
                        kernel::memory::free_pages(virt, pages);
                        kernel::memory::free(cluster_chain);
                        return Result<usize>::err(Error::EIO);
                    }

                    cur_lba += chunk_sectors;
                    cur_tmp += chunk_bytes;
                    rem -= chunk_bytes;
                }

                memcpy(dest + bytes_read, tmp + start_pos, to_copy);
                kernel::memory::free_pages(virt, pages);
            }

            bytes_read += to_copy;
            i = run_end + 1;
        }

        if (update_atime) {
            update_access_time(node->dir_entry);
            overwrite_directory_entry(node->parent_cluster, node->current_index, &node->dir_entry);
        }

        kernel::memory::free(cluster_chain);
        return Result<usize>::ok(bytes_read);
    }

    Result<void> FileSystem::write_file(Fat32Node* node, const void* buffer, const usize len, const usize offset) {
        if (!node || !buffer || len == 0) return Result<void>::err(Error::EINVAL);
        if (is_protected(node->dir_entry)) return Result<void>::err(Error::EROFS);
        if (offset > node->file_size) return Result<void>::err(Error::EINVAL);

        const usize cluster_bytes = bytes_per_cluster();
        const usize new_size = (offset + len > node->file_size) ? (offset + len) : node->file_size;
        const usize needed_clusters = (new_size + cluster_bytes - 1) / cluster_bytes;

        u32 start_cluster = node->cluster;
        usize existing_clusters_count = 0;
        u32* cluster_chain = nullptr;

        if (start_cluster != 0) {
            cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
            if (!cluster_chain || existing_clusters_count == 0) return Result<void>::err(Error::EIO);
        }

        if (existing_clusters_count < needed_clusters) {
            const usize additional = needed_clusters - existing_clusters_count;

            if (start_cluster == 0) {
                start_cluster = find_free_cluster();
                if (start_cluster == 0) return Result<void>::err(Error::ENOSPC);
                write_fat_entry(start_cluster, 0x0FFFFFFF);
                node->cluster = start_cluster;
                cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
                if (!cluster_chain) return Result<void>::err(Error::EIO);
            }

            u32 last_cluster = cluster_chain[existing_clusters_count - 1];
            for (usize i = 0; i < additional; i++) {
                const u32 free_cluster = find_free_cluster();
                if (free_cluster == 0) {
                    kernel::memory::free(cluster_chain);
                    return Result<void>::err(Error::ENOSPC);
                }
                write_fat_entry(last_cluster, free_cluster);
                write_fat_entry(free_cluster, 0x0FFFFFFF);
                last_cluster = free_cluster;
            }

            kernel::memory::free(cluster_chain);
            cluster_chain = get_cluster_chain(start_cluster, existing_clusters_count);
            if (!cluster_chain || existing_clusters_count < needed_clusters) {
                return Result<void>::err(Error::EIO);
            }
        }

        if (!cluster_chain) return Result<void>::err(Error::EIO);

        auto* src = static_cast<const u8*>(buffer);
        usize remaining = len;
        usize current_offset = offset;
        usize cluster_index = offset / cluster_bytes;

        u8 cluster_buf[cluster_bytes];

        while (remaining > 0 && cluster_index < existing_clusters_count) {
            const usize offset_in_cluster = current_offset % cluster_bytes;
            const usize space_in_cluster = cluster_bytes - offset_in_cluster;
            const usize to_write = (remaining < space_in_cluster) ? remaining : space_in_cluster;

            if ((offset_in_cluster != 0) || (to_write < cluster_bytes)) {
                if (read_cluster(cluster_chain[cluster_index], cluster_buf, cluster_bytes) < 0) {
                    kernel::memory::free(cluster_chain);
                    return Result<void>::err(Error::EIO);
                }
                memcpy(cluster_buf + offset_in_cluster, src, to_write);
                if (!write_cluster(cluster_chain[cluster_index], cluster_buf, cluster_bytes)) {
                    kernel::memory::free(cluster_chain);
                    return Result<void>::err(Error::EIO);
                }
            } else {
                if (!write_cluster(cluster_chain[cluster_index], src, cluster_bytes)) {
                    kernel::memory::free(cluster_chain);
                    return Result<void>::err(Error::EIO);
                }
            }

            src += to_write;
            remaining -= to_write;
            current_offset += to_write;
            cluster_index++;
        }

        kernel::memory::free(cluster_chain);

        node->file_size = new_size;
        node->dir_entry.file_size = static_cast<u32>(new_size);
        node->dir_entry.first_cluster_low = static_cast<u16>(start_cluster & 0xFFFF);
        node->dir_entry.first_cluster_high = static_cast<u16>((start_cluster >> 16) & 0xFFFF);
        update_write_time(node->dir_entry);

        if (!overwrite_directory_entry(node->parent_cluster, node->current_index, &node->dir_entry)) {
            return Result<void>::err(Error::EIO);
        }

        return Result<void>::ok();
    }

    // ============================================================================
    // LFN Support Functions
    // ============================================================================

    bool FileSystem::write_directory_entry_with_lfn(
        const u32 dir_cluster, const char* long_name, const char* short_name, const DirectoryEntry* short_entry
    ) {
        const usize name_len = strlen(long_name);
        const usize entries_needed = (12 + name_len) / 13;
        const usize total_needed = entries_needed + 1;
        const usize cluster_bytes = bytes_per_cluster();
        const usize entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        usize out_cluster_count = 0;
        u32* chain = get_cluster_chain(dir_cluster, out_cluster_count);
        if (!chain) return false;

        for (usize ci = 0; ci < out_cluster_count; ++ci) {
            const u32 cluster = chain[ci];
            u8* buf = alloc_cluster_buffer(cluster_bytes);
            if (!buf) continue;

            if (!read_cluster(cluster, buf, cluster_bytes)) {
                free_cluster_buffer(buf, cluster_bytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buf);
            usize free_count = 0;
            usize start_idx = 0;

            for (usize i = 0; i < entries_per_cluster; ++i) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    if (entries[i].attr == ATTR_VOLUME_ID) {
                        free_count = 0;
                        continue;
                    }
                    if (free_count == 0) start_idx = i;
                    free_count++;

                    if (free_count >= total_needed) {
                        write_lfn_entries(entries, start_idx, long_name, short_name, name_len);
                        memcpy(&entries[start_idx + entries_needed], short_entry, sizeof(DirectoryEntry));

                        const bool ok =
                            device->write(cluster_to_sector(cluster), bpb.sectors_per_cluster, buf, cluster_bytes);
                        free_cluster_buffer(buf, cluster_bytes);
                        kernel::memory::free(chain);
                        return ok;
                    }
                } else {
                    free_count = 0;
                }
            }

            free_cluster_buffer(buf, cluster_bytes);
        }

        const u32 last_cluster = chain[out_cluster_count - 1];
        kernel::memory::free(chain);

        const u32 new_cluster = find_free_cluster();
        if (new_cluster == 0) return false;

        if (!write_fat_entry(last_cluster, new_cluster)) return false;
        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return false;

        u8* zero = alloc_cluster_buffer(cluster_bytes);
        memset(zero, 0, cluster_bytes);
        device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes);
        free_cluster_buffer(zero, cluster_bytes);

        return write_directory_entry_with_lfn(dir_cluster, long_name, short_name, short_entry);
    }

    // ============================================================================
    // Create/Delete Operations
    // ============================================================================

    Result<void> FileSystem::create_directory(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name || name[0] == '\0') return Result<void>::err(Error::EINVAL);

        const u32 parent_cluster = parent_dir->cluster;
        const u32 cluster_bytes = bytes_per_cluster();

        if (find_entry_cluster(parent_cluster, name) != 0) return Result<void>::err(Error::EEXIST);

        const u32 new_cluster = find_free_cluster();
        if (new_cluster == 0) return Result<void>::err(Error::ENOSPC);

        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return Result<void>::err(Error::EIO);

        u8* zero = alloc_cluster_buffer(cluster_bytes);
        if (!zero) return Result<void>::err(Error::ENOMEM);
        memset(zero, 0, cluster_bytes);

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

        const bool write_ok =
            device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes);
        free_cluster_buffer(zero, cluster_bytes);
        if (!write_ok) return Result<void>::err(Error::EIO);

        char short_name[12] = {};
        if (!make_short_name(name, short_name)) return Result<void>::err(Error::EINVAL);

        DirectoryEntry new_entry = {};
        memcpy(new_entry.name, short_name, 11);
        new_entry.attr = ATTR_DIRECTORY;
        new_entry.first_cluster_low = new_cluster & 0xFFFF;
        new_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
        new_entry.file_size = 0;
        update_create_time(new_entry);

        write_fs_info();

        if (!write_directory_entry_with_lfn(parent_cluster, name, short_name, &new_entry)) {
            return Result<void>::err(Error::EIO);
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::create_file(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name || name[0] == '\0') return Result<void>::err(Error::EINVAL);

        const u32 parent_cluster = parent_dir->cluster;
        const u32 cluster_bytes = bytes_per_cluster();

        const u32 new_cluster = find_free_cluster();
        if (new_cluster == 0) return Result<void>::err(Error::ENOSPC);

        u8* zero = alloc_cluster_buffer(cluster_bytes);
        memset(zero, 0, cluster_bytes);
        if (!device->write(cluster_to_sector(new_cluster), bpb.sectors_per_cluster, zero, cluster_bytes)) {
            free_cluster_buffer(zero, cluster_bytes);
            return Result<void>::err(Error::EIO);
        }
        free_cluster_buffer(zero, cluster_bytes);

        char short_name[11];
        if (!make_short_name(name, short_name)) return Result<void>::err(Error::EINVAL);

        DirectoryEntry new_entry = {};
        memcpy(new_entry.name, short_name, 11);
        new_entry.attr = ATTR_ARCHIVE;
        new_entry.first_cluster_low = new_cluster & 0xFFFF;
        new_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
        new_entry.file_size = 0;
        update_create_time(new_entry);

        if (!write_directory_entry_with_lfn(parent_cluster, name, short_name, &new_entry)) {
            return Result<void>::err(Error::EIO);
        }

        if (!write_fat_entry(new_cluster, 0x0FFFFFFF)) return Result<void>::err(Error::EIO);

        write_fs_info();
        return Result<void>::ok();
    }

    bool FileSystem::delete_directory_entry_in_directory(const u32 dir_cluster, const char* name) const {
        usize chain_count = 0;
        u32* chain = get_cluster_chain(dir_cluster, chain_count);
        if (!chain) return false;

        const u32 cluster_bytes = bytes_per_cluster();
        const usize entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        struct FoundEntry {
            u32 cluster_index;
            usize entry_index;
            usize lfn_start_index;
            usize lfn_count;
        };

        FoundEntry found = {0, 0, 0, 0};
        bool entry_found = false;

        for (usize ci = 0; ci < chain_count && !entry_found; ++ci) {
            u8* buffer = alloc_cluster_buffer(cluster_bytes);
            if (!buffer) continue;

            if (!read_cluster(chain[ci], buffer, cluster_bytes)) {
                free_cluster_buffer(buffer, cluster_bytes);
                continue;
            }

            auto* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            usize lfn_start_idx = 0;
            usize lfn_count = 0;

            for (usize j = 0; j < entries_per_cluster; ++j) {
                if (entries[j].name[0] == 0x00) break;
                if (entries[j].name[0] == 0xE5) {
                    lfn_count = 0;
                    continue;
                }
                if (entries[j].attr == ATTR_VOLUME_ID) {
                    lfn_count = 0;
                    continue;
                }

                if (entries[j].attr == ATTR_LONG_NAME) {
                    if (lfn_count == 0) lfn_start_idx = j;
                    lfn_count++;
                    continue;
                }

                char full_name[256] = {};

                if (lfn_count > 0) {
                    LfnBufferEntry lfn_buffer[20];
                    const usize actual_lfn_count = (lfn_count < 20) ? lfn_count : 20;

                    for (usize l = 0; l < actual_lfn_count; ++l)
                        lfn_buffer[l].lfn_entry = *reinterpret_cast<LongFileName*>(&entries[lfn_start_idx + l]);

                    // Sort by order field
                    for (usize a = 0; a < actual_lfn_count - 1; a++) {
                        for (usize b = 0; b < actual_lfn_count - 1 - a; b++) {
                            if ((lfn_buffer[b].lfn_entry.order & 0x3F) > (lfn_buffer[b + 1].lfn_entry.order & 0x3F)) {
                                const LfnBufferEntry tmp = lfn_buffer[b];
                                lfn_buffer[b] = lfn_buffer[b + 1];
                                lfn_buffer[b + 1] = tmp;
                            }
                        }
                    }

                    usize pos = 0;
                    for (usize l = 0; l < actual_lfn_count; ++l)
                        copy_lfn_part(&lfn_buffer[l].lfn_entry, full_name, pos, sizeof(full_name));
                    full_name[pos] = '\0';
                } else {
                    extract_short_name(entries[j].name, full_name, sizeof(full_name));
                }

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

        u8* buffer = alloc_cluster_buffer(cluster_bytes);
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

        entries[found.entry_index].name[0] = 0xE5;
        for (usize i = 0; i < found.lfn_count; ++i) entries[found.lfn_start_index + i].name[0] = 0xE5;

        const bool success = device->write(
            cluster_to_sector(chain[found.cluster_index]), bpb.sectors_per_cluster, buffer, cluster_bytes
        );

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(chain);
        return success;
    }

    Result<void> FileSystem::delete_file(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name) return Result<void>::err(Error::EINVAL);

        const u32 parent_cluster = parent_dir->cluster;

        usize entry_count = 0;
        Result<FileEntry*> entries_result = read_directory(parent_cluster, entry_count);
        if (entries_result.is_err()) return Result<void>::err(entries_result.err_code());
        FileEntry* entries = entries_result.value();

        u32 start_cluster = 0;
        DirectoryEntry victim;
        bool found = false;

        for (usize i = 0; i < entry_count; ++i) {
            if (!entries[i].is_dir() && strcmp(entries[i].get_name(), name) == 0) {
                start_cluster = entries[i].get_first_cluster();
                victim = entries[i].get_directory_entry();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);

        if (!found) return Result<void>::err(Error::ENOENT);
        if (is_protected(victim)) return Result<void>::err(Error::EROFS);

        if (start_cluster != 0) {
            trim_cluster_chain(start_cluster);
            free_cluster_chain(start_cluster);
        }

        write_fs_info();

        if (!delete_directory_entry_in_directory(parent_cluster, name)) {
            return Result<void>::err(Error::EIO);
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::remove_directory(const Fat32Node* parent_dir, const char* name) {
        if (!parent_dir || !name) return Result<void>::err(Error::EINVAL);

        const u32 parent_cluster = parent_dir->cluster;

        usize entry_count = 0;
        Result<FileEntry*> entries_result = read_directory(parent_cluster, entry_count);
        if (entries_result.is_err()) return Result<void>::err(entries_result.err_code());
        FileEntry* entries = entries_result.value();

        u32 target_cluster = 0;
        bool found = false;

        for (usize i = 0; i < entry_count; ++i) {
            if (entries[i].is_dir() && strcmp(entries[i].get_name(), name) == 0) {
                target_cluster = entries[i].get_first_cluster();
                found = true;
                break;
            }
        }

        kernel::memory::free(entries);

        if (!found) return Result<void>::err(Error::ENOENT);

        usize dir_entry_count = 0;
        Result<FileEntry*> dir_entries = read_directory(target_cluster, dir_entry_count);
        if (dir_entries.is_err()) return Result<void>::err(dir_entries.err_code());
        kernel::memory::free(dir_entries.value());

        // > 2 means directory is not empty ("." and ".." don't count)
        if (dir_entry_count > 2) return Result<void>::err(Error::ENOTEMPTY);

        trim_cluster_chain(target_cluster);
        free_cluster_chain(target_cluster);

        write_fs_info();

        if (!delete_directory_entry_in_directory(parent_cluster, name)) {
            return Result<void>::err(Error::EIO);
        }

        return Result<void>::ok();
    }

    // ============================================================================
    // Rename Operation
    // ============================================================================

    Result<void> FileSystem::rename(const Fat32Node* parent_dir, const char* old_name, const char* new_name) {
        if (!parent_dir || !old_name || !new_name || old_name[0] == '\0' || new_name[0] == '\0') {
            return Result<void>::err(Error::EINVAL);
        }

        const u32 dir_cluster = parent_dir->cluster;
        const u32 cluster_bytes = bytes_per_cluster();
        const usize entries_per_cluster = cluster_bytes / sizeof(DirectoryEntry);

        usize entry_count = 0;
        Result<FileEntry*> entries_result = read_directory(dir_cluster, entry_count);
        if (!entries_result || entry_count == 0) return Result<void>::err(entries_result.err_code());
        FileEntry* entries = entries_result.value();

        int found_index = -1;
        for (usize i = 0; i < entry_count; ++i) {
            if (strcmp(entries[i].get_name(), old_name) == 0) {
                found_index = static_cast<int>(i);
                break;
            }
        }

        if (found_index < 0) {
            kernel::memory::free(entries);
            return Result<void>::err(Error::ENOENT);
        }

        const DirectoryEntry old_entry = entries[found_index].get_directory_entry();
        if (is_protected(old_entry)) {
            kernel::memory::free(entries);
            return Result<void>::err(Error::EROFS);
        }

        usize chain_count = 0;
        u32* chain = get_cluster_chain(dir_cluster, chain_count);
        if (!chain) {
            kernel::memory::free(entries);
            return Result<void>::err(Error::EIO);
        }

        u32 target_cluster = 0;
        int target_entry_index = -1;

        for (usize ci = 0; ci < chain_count && target_entry_index < 0; ++ci) {
            u8* buffer = alloc_cluster_buffer(cluster_bytes);
            if (!buffer) continue;

            if (!read_cluster(chain[ci], buffer, cluster_bytes)) {
                free_cluster_buffer(buffer, cluster_bytes);
                continue;
            }

            const auto* dir_entries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (usize i = 0; i < entries_per_cluster; ++i) {
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
            return Result<void>::err(Error::ENOENT);
        }

        u8* buffer = alloc_cluster_buffer(cluster_bytes);
        if (!buffer) {
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return Result<void>::err(Error::ENOMEM);
        }

        if (!read_cluster(target_cluster, buffer, cluster_bytes)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return Result<void>::err(Error::EIO);
        }

        auto* dir_entries = reinterpret_cast<DirectoryEntry*>(buffer);

        int old_lfn_count = 0;
        for (int i = target_entry_index - 1; i >= 0; --i) {
            if ((dir_entries[i].attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) break;
            old_lfn_count++;
        }
        const int old_total_count = old_lfn_count + 1;
        const int old_start_index = target_entry_index - old_lfn_count;

        const usize new_name_len = strlen(new_name);
        const int new_lfn_count = static_cast<int>((new_name_len + 12) / 13);
        const int new_total_count = new_lfn_count + 1;

        char new_short_name[12] = {};
        if (!make_short_name(new_name, new_short_name)) {
            free_cluster_buffer(buffer, cluster_bytes);
            kernel::memory::free(entries);
            kernel::memory::free(chain);
            return Result<void>::err(Error::EINVAL);
        }

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
            for (int i = old_start_index; i < old_start_index + old_total_count; ++i) dir_entries[i].name[0] = 0xE5;

            write_lfn_entries(dir_entries, old_start_index, new_name, new_short_name, new_name_len);

            DirectoryEntry updated = old_entry;
            memcpy(updated.name, new_short_name, 11);
            dir_entries[old_start_index + new_lfn_count] = updated;

            success = device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);
        } else {
            for (int i = old_start_index; i < old_start_index + old_total_count; ++i) dir_entries[i].name[0] = 0xE5;

            device->write(cluster_to_sector(target_cluster), bpb.sectors_per_cluster, buffer, cluster_bytes);

            DirectoryEntry new_entry = old_entry;
            memcpy(new_entry.name, new_short_name, 11);
            success = write_directory_entry_with_lfn(dir_cluster, new_name, new_short_name, &new_entry);
        }

        free_cluster_buffer(buffer, cluster_bytes);
        kernel::memory::free(entries);
        kernel::memory::free(chain);

        if (!success) return Result<void>::err(Error::EIO);
        return Result<void>::ok();
    }

    // ============================================================================
    // Path Resolution
    // ============================================================================

    u32 FileSystem::resolve_path_to_cluster(const char* path) const {
        if (path[0] != '/') return 0;

        u32 current_cluster = get_root_cluster();

        char components[16][32];
        const usize comp_count = split_path(path, components, 16);

        for (usize i = 0; i < comp_count; i++) {
            const u32 next_cluster = find_entry_cluster(current_cluster, components[i]);
            if (next_cluster == 0) return 0;
            current_cluster = next_cluster;
        }

        return current_cluster;
    }

    u32 FileSystem::find_entry_cluster(const u32 dir_cluster, const char* given_name) const {
        usize entry_count = 0;
        Result<FileEntry*> entries_result = read_directory(dir_cluster, entry_count);
        if (entries_result.is_err()) return 0;
        FileEntry* entries = entries_result.value();

        u32 result = 0;
        for (usize i = 0; i < entry_count; i++) {
            if (const char* entry_name = entries[i].get_name(); strcmp(entry_name, given_name) == 0) {
                result = entries[i].get_first_cluster();
                break;
            }
        }

        kernel::memory::free(entries);
        return result;
    }

    Result<void> FileSystem::stat(const Fat32Node* node, vespera_stat_t* out, const u32 dev_id) const {
        if (!node || !out) return Result<void>::err(Error::EINVAL);

        out->inode_id = node->cluster;
        out->block_size = bytes_per_cluster();
        out->dev_id = dev_id;
        out->size = node->file_size;

        if (node->file_size > 0 && out->block_size > 0) {
            const u64 clusters_used = (node->file_size + out->block_size - 1) / out->block_size;
            out->blocks = clusters_used * bpb.sectors_per_cluster;
        }

        out->atime = fat32_time_to_unix(node->dir_entry.last_access_date, 0);
        out->mtime = fat32_time_to_unix(node->dir_entry.write_date, node->dir_entry.write_time);
        out->ctime = out->mtime;
        out->crtime = fat32_time_to_unix(node->dir_entry.creation_date, node->dir_entry.creation_time);

        constexpr u32 MODE_DIR = 0x4000;
        constexpr u32 MODE_REG = 0x8000;
        const u16 type = (node->dir_entry.attr & ATTR_DIRECTORY) ? MODE_DIR : MODE_REG;
        const u16 perm = (node->dir_entry.attr & ATTR_DIRECTORY) ? 0755u : 0644u;
        out->mode = type | perm;

        out->links_count = 1;
        out->uid = 0;
        out->gid = 0;

        out->flags = VSTAT_FLAG_READABLE;
        if (!(node->dir_entry.attr & ATTR_READ_ONLY)) out->flags |= VSTAT_FLAG_WRITABLE;

        out->node_type = (node->dir_entry.attr & ATTR_DIRECTORY) ? VSTAT_TYPE_DIR : VSTAT_TYPE_FILE;

        return Result<void>::ok();
    }

    Result<void> FileSystem::truncate(Fat32Node* node, const usize new_size) {
        const u32 cluster_bytes = bytes_per_cluster();

        if (new_size == 0) {
            if (node->cluster != 0) {
                trim_cluster_chain(node->cluster);
                free_cluster_chain(node->cluster);
                node->cluster = 0;
            }
        } else {
            const usize needed = (new_size + cluster_bytes - 1) / cluster_bytes;
            usize count = 0;
            u32* chain = get_cluster_chain(node->cluster, count);
            if (chain && count > needed) {
                write_fat_entry(chain[needed - 1], 0x0FFFFFFF);
                for (usize i = needed; i < count; ++i) write_fat_entry(chain[i], 0);
                kernel::memory::free(chain);
            }
        }

        node->file_size = new_size;
        node->dir_entry.file_size = static_cast<u32>(new_size);
        node->dir_entry.first_cluster_low = static_cast<u16>(node->cluster & 0xFFFF);
        node->dir_entry.first_cluster_high = static_cast<u16>((node->cluster >> 16) & 0xFFFF);
        update_write_time(node->dir_entry);

        if (!overwrite_directory_entry(node->parent_cluster, node->current_index, &node->dir_entry)) {
            return Result<void>::err(Error::EIO);
        }

        return Result<void>::ok();
    }

    // ============================================================================
    // FileEntry Helper
    // ============================================================================

    void FileEntry::format_short_name() {
        formatted_short_name_[0] = '\0';
        char name[9] = {};
        char ext[4] = {};

        memcpy(name, short_name_, 8);
        name[8] = '\0';
        for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

        memcpy(ext, short_name_ + 8, 3);
        ext[3] = '\0';
        for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';

        usize name_len = strlen(name);
        if (name_len >= sizeof(formatted_short_name_) - 1) name_len = sizeof(formatted_short_name_) - 1;

        memcpy(formatted_short_name_, name, name_len);
        formatted_short_name_[name_len] = '\0';

        if (ext[0] != '\0') {
            if (name_len + 1 < sizeof(formatted_short_name_) - 1) {
                formatted_short_name_[name_len] = '.';
                usize ext_len = strlen(ext);
                if (name_len + 1 + ext_len >= sizeof(formatted_short_name_))
                    ext_len = sizeof(formatted_short_name_) - name_len - 2;
                memcpy(formatted_short_name_ + name_len + 1, ext, ext_len);
                formatted_short_name_[name_len + 1 + ext_len] = '\0';
            }
        }
    }

    bool FileSystem::is_protected(const DirectoryEntry& e) {
        return e.attr & (ATTR_READ_ONLY | ATTR_SYSTEM);
    }

}  // namespace fat32