// fat32_fixture.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_FAT32_FIXTURE_H
#define VESPERAOS_FAT32_FIXTURE_H

#include <new>
#include "../../filesystem/fat32/fat32.h"
#include "../../filesystem/fat32/fat32_vfs_adapter.h"
#include <vespera/mm/memory.h>
#include "../framework/test_framework.h"
#include "../stub_kernel/mock_blockdevice.h"
#include <cstdio>
#include <cstring>
#include <vector>

struct Fat32Fixture {
    BlockDevice*       dev = nullptr;
    fat32::FileSystem* fs  = nullptr;

    Fat32Fixture() {
        FILE* f = fopen("test.img", "rb");
        if (!f) {
            fprintf(stderr, "ERROR: test.img not found\n");
            return;
        }

        fseek(f, 0, SEEK_END);
        size_t size = static_cast<size_t>(ftell(f));
        rewind(f);

        size_t sectors = size / MockBlockDevice::SECTOR_SIZE;
        auto*  mdev    = new MockBlockDevice(sectors);
        fread(mdev->raw(), 1, size, f);
        fclose(f);

        dev = mdev;
        fs  = new fat32::FileSystem(dev);
    }

    ~Fat32Fixture() {
        delete fs;
        delete dev;
    }

    [[nodiscard]] bool valid() const { return fs && fs->is_valid(); }


    [[nodiscard]] Fat32Node root_node() const {
        Fat32Node n{};
        n.cluster       = fs->get_root_cluster();
        n.parent_cluster = n.cluster;
        n.fs            = fs;
        return n;
    }

    Fat32Node create_file(const char* name) {
        Fat32Node parent = root_node();
        if (!fs->create_file(&parent, name)) {
            TestFramework::fail_test(__FILE__, __LINE__,
                (std::string("CreateFile failed: ") + name).c_str());
            return {};
        }
        return find_file_node(name);
    }

    Fat32Node create_dir(const char* name) {
        Fat32Node parent = root_node();
        if (fs->create_directory(&parent, name).is_err()) {
            TestFramework::fail_test(__FILE__, __LINE__,
                (std::string("CreateDirectory failed: ") + name).c_str());
            return {};
        }
        return find_dir_node(name);
    }

    Fat32Node find_file_node(const char* name) { return find_node(name, false); }

    // Look up a directory node by name in the root directory.
    Fat32Node find_dir_node(const char* name) { return find_node(name, true); }

    bool write(Fat32Node& node, const void* data, size_t len, size_t offset = 0) const {
        return fs->write_file(&node, data, len, offset).is_ok();
    }


    std::vector<uint8_t> read(Fat32Node& node, size_t len, size_t offset = 0) {
        std::vector<uint8_t> buf(len, 0);

        Result<usize> r = fs->read_file(&node, buf.data(), len, offset);
        if (r.is_err()) return {};
        buf.resize(r.value());
        return buf;
    }

    // Returns all entry names in the root directory.
    std::vector<std::string> list_root() const {
        size_t count = 0;
        Result<fat32::FileEntry*> entries_r = fs->read_directory(fs->get_root_cluster(), count);
        if (!entries_r) return {};
        fat32::FileEntry* entries = entries_r.value();

        std::vector<std::string> names;
        for (size_t i = 0; i < count; i++)
            names.emplace_back(entries[i].get_name());

        kernel::memory::free(entries);
        return names;
    }

    static bool list_contains(const std::vector<std::string>& list, const char* name) {
        for (auto& s : list)
            if (s == name) return true;
        return false;
    }

private:
    Fat32Node find_node(const char* name, bool is_dir) const {
        uint32_t root = fs->get_root_cluster();
        size_t count = 0;
        Result<fat32::FileEntry*> entries_r = fs->read_directory(root, count);
        if (!entries_r) return {};
        fat32::FileEntry* entries = entries_r.value();

        Fat32Node node{};
        for (size_t i = 0; i < count; i++) {
            if (strcmp(entries[i].get_name(), name) == 0 && entries[i].is_dir() == is_dir) {
                auto de = entries[i].get_directory_entry();
                node.cluster = entries[i].get_first_cluster();
                node.file_size = de.file_size;
                node.dir_entry = de;
                node.parent_cluster = root;
                node.current_index = entries[i].get_index_in_cluster();
                node.fs = fs;
                break;
            }
        }

        kernel::memory::free(entries);
        return node;
    }
};

#define WITH_FAT32(var)                                                                              \
    Fat32Fixture var;                                                                                \
    if (!(var).valid()) {                                                                            \
        TestFramework::fail_test(__FILE__, __LINE__, "Fat32Fixture invalid — is test.img present?"); \
        return;                                                                                      \
    }

#endif  // VESPERAOS_FAT32_FIXTURE_H
