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

#include "../framework/test_framework.h"
#include "mock_blockdevice.h"
#include "../../filesystem/fat32/fat32.h"
#include "../../filesystem/fat32/fat32_vfs_adapter.h"
#include "../include/kernel/memory.h"

#include <cstdio>
#include <cstring>
#include <vector>

struct Fat32Fixture {
    BlockDevice*       dev = nullptr;
    FAT32::FileSystem* fs  = nullptr;

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
        fs  = new FAT32::FileSystem(dev);
    }

    ~Fat32Fixture() {
        delete fs;
        delete dev;
    }

    [[nodiscard]] bool valid() const { return fs && fs->is_valid(); }


    [[nodiscard]] Fat32Node root_node() const {
        Fat32Node n{};
        n.cluster       = fs->GetRootCluster();
        n.parentCluster = n.cluster;
        n.fs            = fs;
        return n;
    }

    Fat32Node create_file(const char* name) {
        Fat32Node parent = root_node();
        if (!fs->CreateFile(&parent, name)) {
            TestFramework::fail_test(__FILE__, __LINE__,
                (std::string("CreateFile failed: ") + name).c_str());
            return {};
        }
        return find_file_node(name);
    }

    Fat32Node create_dir(const char* name) {
        Fat32Node parent = root_node();
        if (!fs->CreateDirectory(&parent, name)) {
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
        return fs->WriteFile(&node, data, len, offset);
    }


    std::vector<uint8_t> read(Fat32Node& node, size_t len, size_t offset = 0) {
        std::vector<uint8_t> buf(len, 0);
        size_t actual = 0;
        if (!fs->ReadFile(&node, buf.data(), len, actual, offset)) return {};
        buf.resize(actual);
        return buf;
    }

    // Returns all entry names in the root directory.
    std::vector<std::string> list_root() const {
        size_t count = 0;
        FAT32::FileEntry* entries = fs->ReadDirectory(fs->GetRootCluster(), count);
        if (!entries) return {};

        std::vector<std::string> names;
        for (size_t i = 0; i < count; i++)
            names.emplace_back(entries[i].GetName());

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
        uint32_t root = fs->GetRootCluster();
        size_t count = 0;
        FAT32::FileEntry* entries = fs->ReadDirectory(root, count);
        if (!entries) return {};

        Fat32Node node{};
        for (size_t i = 0; i < count; i++) {
            if (strcmp(entries[i].GetName(), name) == 0 && entries[i].isDir() == is_dir) {
                auto de = entries[i].GetDirectoryEntry();
                node.cluster = entries[i].GetFirstCluster();
                node.fileSize = de.fileSize;
                node.dirEntry = de;
                node.parentCluster = root;
                node.currentIndex = entries[i].GetIndexInCluster();
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
