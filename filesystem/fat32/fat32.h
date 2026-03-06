//
// Created by linus on 03.07.25.
//

#ifndef FAT32_CPP_H
#define FAT32_CPP_H
#include <string.h>
#include "../../kernel/devices/blockdevice.h"
#include <stdint.h>
#include <stddef.h>
// https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf

struct Fat32Node;

namespace fat32
{
    inline constexpr uint8_t ATTR_READ_ONLY   = 0x01;
    inline constexpr uint8_t ATTR_HIDDEN      = 0x02;
    inline constexpr uint8_t ATTR_SYSTEM      = 0x04;
    inline constexpr uint8_t ATTR_VOLUME_ID   = 0x08;
    inline constexpr uint8_t ATTR_DIRECTORY   = 0x10;
    inline constexpr uint8_t ATTR_ARCHIVE     = 0x20;
    inline constexpr uint8_t ATTR_LONG_NAME   = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
    inline constexpr uint8_t LAST_LONG_ENTRY  = 0x40;

    inline constexpr size_t READ_DIR_MAX_ENTRIES = 256;

    struct BPB_FAT32
    {
        uint8_t jmp_boot[3]; // 0x00
        uint8_t oem_name[8]; // 0x03
        uint16_t bytes_per_sector; // 0x0B
        uint8_t sectors_per_cluster; // 0x0D
        uint16_t reserved_sector_count; // 0x0E
        uint8_t table_count; // 0x10
        uint16_t root_entry_count; // 0x11
        uint16_t total_sectors16; // 0x13
        uint8_t media_type; // 0x15
        uint16_t fat_size16; // 0x16
        uint16_t sectors_per_track; // 0x18
        uint16_t head_side_count; // 0x1A
        uint32_t hidden_sectors; // 0x1C
        uint32_t total_sectors32; // 0x20

        // FAT32 Extended BIOS Parameter Block
        uint32_t fat_size32; // 0x24
        uint16_t ext_flags; // 0x28
        uint16_t fs_version; // 0x2A
        uint32_t root_cluster; // 0x2C
        uint16_t fs_info; // 0x30
        uint16_t backup_boot_sector; // 0x32
        uint8_t reserved[12]; // 0x34
        uint8_t drive_number; // 0x40
        uint8_t reserved1; // 0x41
        uint8_t boot_signature; // 0x42
        uint32_t volume_id; // 0x43
        uint8_t volume_label[11]; // 0x47
        uint8_t fs_type[8]; // 0x52
    }__attribute__((packed));

    // BPB_FSInfo
    struct FSINFO
    {
        uint32_t lead_sig;
        uint64_t reserved1[60];
        uint32_t struc_sig;
        uint32_t free_count;
        uint32_t nxt_free;
        uint32_t reserved2[3];
        uint32_t trail_sig;
    }__attribute__((packed));

    //  Section 6: Directory Structure FAT spec
    struct DirectoryEntry
    {
        unsigned char name[11]; // 8 + 3 Bytes
        uint8_t attr;
        uint8_t nt_res;
        uint8_t creation_time_tenths;
        uint16_t creation_time;
        uint16_t creation_date;
        uint16_t last_access_date;
        uint16_t first_cluster_high;
        uint16_t write_time;
        uint16_t write_date;
        uint16_t first_cluster_low;
        uint32_t file_size;
    } __attribute__((packed));

    class FileEntry
    {
        char short_name_[13] = {};
        char long_name_[256] = {};
        char formatted_short_name_[13] = {};
        DirectoryEntry dir_entry_ = {};
        bool is_dir_ = false;
        size_t index_in_cluster_ = 0;

    public:
        void set_long_name(const char* name)
        {
            if (!name || name[0] == '\0')
            {
                long_name_[0] = '\0';
            }
            else
            {
                strncpy(long_name_, name, sizeof(long_name_));
            }
        }

        void set_short_name(const char* name)
        {
            strncpy(short_name_, name, sizeof(short_name_));
            format_short_name();
        }

        void set_directory_entry(const DirectoryEntry& entry)
        {
            dir_entry_ = entry;
            format_short_name();
        }

        void set_is_dir(const bool value)
        {
            is_dir_ = value;
        }

        [[nodiscard]] bool is_dir() const
        {
            return is_dir_;
        }

        [[nodiscard]] const char* get_long_name() const
        {
            return long_name_;
        }

        [[nodiscard]] const char* get_name() const
        {
            if (long_name_[0] != '\0') return long_name_;
            return formatted_short_name_;
        }

        [[nodiscard]] const char* get_formatted_short_name() const
        {
            return formatted_short_name_;
        }

        [[nodiscard]] DirectoryEntry get_directory_entry() const
        {
            return dir_entry_;
        }

        [[nodiscard]] uint32_t get_first_cluster() const
        {
            return (static_cast<uint32_t>(dir_entry_.first_cluster_high) << 16) | dir_entry_.first_cluster_low;
        }

        [[nodiscard]] uint32_t get_file_size() const
        {
            return dir_entry_.file_size;
        }

        void set_file_size(uint32_t size)
        {
            dir_entry_.file_size = size;
        }

        void set_index_in_cluster(size_t index)
        {
            index_in_cluster_ = index;
        }

        [[nodiscard]] size_t get_index_in_cluster() const
        {
            return index_in_cluster_;
        }

        void format_short_name();
    };

    struct LongFileName
    {
        uint8_t order;
        uint16_t name1[5];
        uint8_t attr;
        uint8_t type;
        uint8_t checksum;
        uint16_t name2[6];
        uint16_t first_cluster_low;
        uint16_t name3[2];
    } __attribute__((packed));

    struct LfnBufferEntry
    {
        LongFileName lfn_entry;
    };

    class FileSystem
    {
    public:
        explicit FileSystem(BlockDevice* device);

        ~FileSystem();

        [[nodiscard]] bool is_valid() const;

        [[nodiscard]] uint32_t get_root_cluster() const;

        uint32_t resolve_path_to_cluster(const char* path) const;

        bool read_file(Fat32Node* node, void* buffer, size_t len, size_t& out_actual, size_t offset = 0) const;

        bool write_file(Fat32Node* node, const void* buffer, size_t len, size_t offset);

        FileEntry* read_directory(const char* path, size_t& out_count) const;

        FileEntry* read_directory(uint32_t cluster, size_t& out_count) const;

        bool create_file(const Fat32Node* parent_dir, const char* name);

        bool write_directory_entry_with_lfn(uint32_t dir_cluster, const char* long_name, const char* short_name,
                                        const DirectoryEntry* short_entry);
        bool delete_directory_entry_in_directory(uint32_t dir_cluster, const char* name) const;

        bool create_directory(const Fat32Node* parent_dir, const char* name);

        bool remove_directory(const Fat32Node* parent_dir, const char* name);

        bool rename(const Fat32Node* parent_dir, const char* old_name, const char* new_name);

        bool delete_file(const Fat32Node* parent_dir, const char* name);

        [[nodiscard]] BPB_FAT32* get_bpb()
        {
            return &bpb;
        }

 //   private:
        BlockDevice* device;
        BPB_FAT32 bpb{};
        bool fs_valid;

        uint32_t sector_size;
        uint32_t data_start;

        uint32_t cluster_count;
        uint32_t free_cluster_count;
        uint32_t next_free_cluster;

        struct CacheStats
        {
            uint32_t hits;
            uint32_t misses;
            uint32_t invalidations;

            void reset() { hits = misses = invalidations = 0; }

            [[nodiscard]] float hit_rate() const
            {
                uint32_t total = hits + misses;
                return total > 0 ? (100.0f * hits / total) : 0.0f;
            }
        };

        mutable CacheStats cache_stats;

        struct CacheEntry
        {
            uint32_t sector;
            uint8_t data[512];
            uint32_t last_used; // LRU counter
            bool valid;
        };

        struct Sector
        {
            uint32_t sector = UINT32_MAX;
            uint8_t  buf[512]{};
        };

        static constexpr size_t FAT_CACHE_SIZE = 10;
        mutable CacheEntry fat_cache[FAT_CACHE_SIZE];
        mutable uint32_t cache_access_counter;

        bool read_fat_sector(uint32_t fat_sector, uint8_t* buffer) const;
        void invalidate_fat_cache() const;
        void invalidate_fat_cache_sector(uint32_t sector) const;

        bool probe_fs() const;

        [[nodiscard]] uint32_t cluster_to_sector(uint32_t cluster) const;
        bool load_fs_info();
        void write_fs_info() const;
        uint32_t get_free_cluster_count();

        ssize_t read_cluster(uint32_t cluster, void* buffer, size_t buffer_size) const;

        bool write_cluster(uint32_t cluster, const void* data, size_t len, size_t offset = 0) const;
        bool is_valid_fat_entry(uint32_t value) const;
        uint32_t read_fat_entry_raw(uint32_t fat_sector, uint32_t offset) const;

        [[nodiscard]] uint32_t bytes_per_cluster() const;

        [[nodiscard]] uint32_t get_fat_entry(uint32_t cluster) const;
        uint32_t read_fat_entry(uint32_t cluster, Sector& sec) const;
        bool write_fat_entry_raw(uint32_t fat_sector, uint32_t offset, uint32_t value) const;

        uint32_t* get_cluster_chain(uint32_t start_cluster, size_t& out_count) const;
        bool free_cluster_chain(uint32_t start_cluster);

        bool write_fat_entry(uint32_t cluster, uint32_t value);
        [[nodiscard]] uint32_t next_cluster(uint32_t c) const;
        bool has_fat_loop(uint32_t start) const;
        uint32_t find_free_cluster();

        bool overwrite_directory_entry(uint32_t parent_cluster, size_t entry_index, const DirectoryEntry* new_entry) const;

        uint32_t find_entry_cluster(uint32_t dir_cluster, const char* given_name) const;
        static bool is_protected(const DirectoryEntry& e);
    };
}

#endif //FAT32_CPP_H
