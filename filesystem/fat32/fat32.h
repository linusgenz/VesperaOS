//
// Created by linus on 03.07.25.
//

#ifndef FAT32_CPP_H
#define FAT32_CPP_H

#include <klib/string.h>
#include <vespera/devices/block.h>
#include <vespera/mm/memory.h>
#include <vespera/types.h>
// https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf

struct Fat32Node;

namespace fat32 {
    inline constexpr u8 ATTR_READ_ONLY = 0x01;
    inline constexpr u8 ATTR_HIDDEN = 0x02;
    inline constexpr u8 ATTR_SYSTEM = 0x04;
    inline constexpr u8 ATTR_VOLUME_ID = 0x08;
    inline constexpr u8 ATTR_DIRECTORY = 0x10;
    inline constexpr u8 ATTR_ARCHIVE = 0x20;
    inline constexpr u8 ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
    inline constexpr u8 LAST_LONG_ENTRY = 0x40;
    inline constexpr u32 FAT32_EOC = 0x0FFFFFF8;
    inline constexpr usize READ_DIR_MAX_ENTRIES = 256;

    struct BPB_FAT32 {
        u8 jmp_boot[3];             // 0x00
        u8 oem_name[8];             // 0x03
        u16 bytes_per_sector;       // 0x0B
        u8 sectors_per_cluster;     // 0x0D
        u16 reserved_sector_count;  // 0x0E
        u8 table_count;             // 0x10
        u16 root_entry_count;       // 0x11
        u16 total_sectors16;        // 0x13
        u8 media_type;              // 0x15
        u16 fat_size16;             // 0x16
        u16 sectors_per_track;      // 0x18
        u16 head_side_count;        // 0x1A
        u32 hidden_sectors;         // 0x1C
        u32 total_sectors32;        // 0x20

        // FAT32 Extended BIOS Parameter Block
        u32 fat_size32;          // 0x24
        u16 ext_flags;           // 0x28
        u16 fs_version;          // 0x2A
        u32 root_cluster;        // 0x2C
        u16 fs_info;             // 0x30
        u16 backup_boot_sector;  // 0x32
        u8 reserved[12];         // 0x34
        u8 drive_number;         // 0x40
        u8 reserved1;            // 0x41
        u8 boot_signature;       // 0x42
        u32 volume_id;           // 0x43
        u8 volume_label[11];     // 0x47
        u8 fs_type[8];           // 0x52
    } __attribute__((packed));

    // BPB_FSInfo
    struct FSINFO {
        u32 lead_sig;
        u64 reserved1[60];
        u32 struc_sig;
        u32 free_count;
        u32 nxt_free;
        u32 reserved2[3];
        u32 trail_sig;
    } __attribute__((packed));

    //  Section 6: Directory Structure FAT spec
    struct DirectoryEntry {
        unsigned char name[11];  // 8 + 3 Bytes
        u8 attr;
        u8 nt_res;
        u8 creation_time_tenths;
        u16 creation_time;
        u16 creation_date;
        u16 last_access_date;
        u16 first_cluster_high;
        u16 write_time;
        u16 write_date;
        u16 first_cluster_low;
        u32 file_size;
    } __attribute__((packed));

    class FileEntry {
        char short_name_[13] = {};
        char long_name_[256] = {};
        char formatted_short_name_[13] = {};
        DirectoryEntry dir_entry_ = {};
        bool is_dir_ = false;
        usize index_in_cluster_ = 0;

       public:
        void set_long_name(const char* name) {
            if (!name || name[0] == '\0') {
                long_name_[0] = '\0';
            } else {
                strncpy(long_name_, name, sizeof(long_name_)-1);
                long_name_[sizeof(long_name_) - 1] = '\0';
            }
        }

        void set_short_name(const char* name) {
            memset(short_name_, 0, sizeof(short_name_));

            strncpy(short_name_, name, sizeof(short_name_) - 1);

            format_short_name();
        }

        void set_directory_entry(const DirectoryEntry& entry) {
            dir_entry_ = entry;
            format_short_name();
        }

        void set_is_dir(const bool value) {
            is_dir_ = value;
        }

        [[nodiscard]] bool is_dir() const {
            return is_dir_;
        }

        [[nodiscard]] const char* get_long_name() const {
            return long_name_;
        }

        [[nodiscard]] const char* get_name() const {
            if (long_name_[0] != '\0') return long_name_;
            return formatted_short_name_;
        }

        [[nodiscard]] const char* get_formatted_short_name() const {
            return formatted_short_name_;
        }

        [[nodiscard]] DirectoryEntry get_directory_entry() const {
            return dir_entry_;
        }

        [[nodiscard]] u32 get_first_cluster() const {
            return (static_cast<u32>(dir_entry_.first_cluster_high) << 16) | dir_entry_.first_cluster_low;
        }

        [[nodiscard]] u32 get_file_size() const {
            return dir_entry_.file_size;
        }

        void set_file_size(u32 size) {
            dir_entry_.file_size = size;
        }

        void set_index_in_cluster(usize index) {
            index_in_cluster_ = index;
        }

        [[nodiscard]] usize get_index_in_cluster() const {
            return index_in_cluster_;
        }

        void format_short_name();
    };

    struct LongFileName {
        u8 order;
        u16 name1[5];
        u8 attr;
        u8 type;
        u8 checksum;
        u16 name2[6];
        u16 first_cluster_low;
        u16 name3[2];
    } __attribute__((packed));

    struct LfnBufferEntry {
        LongFileName lfn_entry;
    };

    class FileSystem {
       public:
        explicit FileSystem(BlockDevice* device);

        ~FileSystem();

        [[nodiscard]] bool is_valid() const;

        [[nodiscard]] u32 get_root_cluster() const;

        u32 resolve_path_to_cluster(const char* path) const;

        bool read_file(Fat32Node* node, void* buffer, usize len, usize& out_actual, usize offset = 0, bool update_atime = true) const;

        bool write_file(Fat32Node* node, const void* buffer, usize len, usize offset);

        FileEntry* read_directory(const char* path, usize& out_count) const;

        FileEntry* read_directory(u32 cluster, usize& out_count) const;

        bool create_file(const Fat32Node* parent_dir, const char* name);

        bool write_directory_entry_with_lfn(
            u32 dir_cluster, const char* long_name, const char* short_name, const DirectoryEntry* short_entry
        );
        bool delete_directory_entry_in_directory(u32 dir_cluster, const char* name) const;

        bool create_directory(const Fat32Node* parent_dir, const char* name);

        bool remove_directory(const Fat32Node* parent_dir, const char* name);

        bool rename(const Fat32Node* parent_dir, const char* old_name, const char* new_name);

        bool delete_file(const Fat32Node* parent_dir, const char* name);

        [[nodiscard]] BPB_FAT32* get_bpb() {
            return &bpb;
        }

        void write_fs_info() const;

        void mark_device_lost() {
            device_lost_ = true;
        }

        void set_device_id(u32 id) { device_id_ = id; }
        u32  get_device_id() const  { return device_id_; }

        //   private:
        BlockDevice* device;
        BPB_FAT32 bpb{};
        bool fs_valid;
        u32 device_id_ = 0;

        bool device_lost_ = false;

        u32 sector_size;
        u32 data_start;

        u32 cluster_count;
        u32 free_cluster_count;
        u32 next_free_cluster;

        struct CacheStats {
            u32 hits;
            u32 misses;
            u32 invalidations;

            void reset() {
                hits = misses = invalidations = 0;
            }

            [[nodiscard]] float hit_rate() const {
                const u32 total = hits + misses;
                return total > 0 ? (100.0f * hits / total) : 0.0f;
            }
        };

        mutable CacheStats cache_stats;

        struct CacheEntry {
            u32 sector;
            u8 data[512];
            u32 last_used;  // LRU counter
            bool valid;
        };

        struct Sector {
            u32 sector = U32_MAX;
            u8 buf[512]{};
        };

        static constexpr usize FAT_CACHE_SIZE = 10;
        mutable CacheEntry fat_cache[FAT_CACHE_SIZE];
        mutable u32 cache_access_counter;

        bool read_fat_sector(u32 fat_sector, u8* buffer) const;
        void invalidate_fat_cache() const;
        void invalidate_fat_cache_sector(u32 sector) const;

        bool probe_fs() const;

        [[nodiscard]] u32 cluster_to_sector(u32 cluster) const;
        bool load_fs_info();
        u32 get_free_cluster_count();

        isize read_cluster(u32 cluster, void* buffer, usize buffer_size) const;

        bool write_cluster(u32 cluster, const void* data, usize len, usize offset = 0) const;
        bool is_valid_fat_entry(u32 value) const;
        u32 read_fat_entry_raw(u32 fat_sector, u32 offset) const;

        [[nodiscard]] u32 bytes_per_cluster() const;

        [[nodiscard]] u32 get_fat_entry(u32 cluster) const;
        u32 read_fat_entry(u32 cluster, Sector& sec) const;
        bool write_fat_entry_raw(u32 fat_sector, u32 offset, u32 value) const;

        u32* get_cluster_chain(u32 start_cluster, usize& out_count) const;
        bool free_cluster_chain(u32 start_cluster);
        void trim_cluster_chain(u32 start_cluster) const;

        bool write_fat_entry(u32 cluster, u32 value);
        [[nodiscard]] u32 next_cluster(u32 c) const;
        bool has_fat_loop(u32 start) const;
        u32 find_free_cluster();

        bool overwrite_directory_entry(u32 parent_cluster, usize entry_index, const DirectoryEntry* new_entry) const;

        u32 find_entry_cluster(u32 dir_cluster, const char* given_name) const;
        static bool is_protected(const DirectoryEntry& e);
    };
}  // namespace fat32

#endif  // FAT32_CPP_H
