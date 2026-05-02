// ext4_fixture.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.03.26.
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

#ifndef VESPERAOS_EXT4_FIXTURE_H
#define VESPERAOS_EXT4_FIXTURE_H

#include "../../filesystem/ext4/ext4.h"
#include "../../filesystem/ext4/ext4_vfs_adapter.h"
#include "../../filesystem/vfs/fs_registry.h"
#include "../framework/test_framework.h"
#include "../stub_kernel/mock_blockdevice.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static constexpr usize HELLO_TXT_SIZE = 16;  // "hello from ext4\n"
static constexpr usize EMPTY_TXT_SIZE = 0;
static constexpr usize BINARY_BIN_SIZE = 8192;
static constexpr usize BIG_BIN_SIZE = 65536;
static constexpr usize NESTED_TXT_SIZE = 15;   // "nested content\n"
static constexpr usize ANOTHER_TXT_SIZE = 13;  // "another file\n"
static constexpr usize LEAF_TXT_SIZE = 10;     // "deep leaf\n"
static constexpr usize TRUNCTEST_SIZE = 29;    // "truncation test content here\n"

// Forward-declare the driver symbol exposed by ext4_vfs_adapter.cpp.
extern FileSystemDriver ext4_driver;

struct Ext4Fixture {
    BlockDevice* dev = nullptr;
    ext4::FileSystem* fs = nullptr;
    VfsNode* root = nullptr;

    Ext4Fixture() {
        FILE* f = fopen("test_ext4.img", "rb");
        if (!f) {
            fprintf(stderr, "ERROR: test_ext4.img not found\n");
            return;
        }

        fseek(f, 0, SEEK_END);
        size_t size = static_cast<size_t>(ftell(f));
        rewind(f);

        constexpr size_t MIN_SIZE = 1u * 1024 * 1024;
        if (size < MIN_SIZE) {
            fprintf(stderr, "ERROR: test_ext4.img too small (%zu bytes)\n", size);
            fclose(f);
            return;
        }

        size_t sectors = size / MockBlockDevice::SECTOR_SIZE;
        auto* mdev = new MockBlockDevice(sectors);
        fread(mdev->raw(), 1, size, f);
        fclose(f);

        dev = mdev;
        root = ext4_driver.mount(dev);
        if (!root) {
            delete dev;
            dev = nullptr;
            return;
        }

        auto* nd = static_cast<Ext4Node*>(root->internal_data);
        fs = nd->fs;
    }

    ~Ext4Fixture() {
        if (root) {
            ext4_driver.unmount(root);
            auto* nd = static_cast<Ext4Node*>(root->internal_data);
            if (nd) kernel::memory::free(nd);
            kernel::memory::free(root);
        }
    }

    [[nodiscard]] bool valid() const {
        return root != nullptr && fs != nullptr && fs->is_valid();
    }

    // ── Node ownership helpers ────────────────────────────────────────────────

    // Release a VfsNode returned by find() / ops->find().
    // Does NOT free the "name" field when it matches a literal (root "/").
    static void free_node(VfsNode* node) {
        if (!node) return;
        if (node->internal_data) kernel::memory::free(node->internal_data);
        // Child nodes have strdup'd names; free them.
        if (node->name && node->name[0] != '\0') kernel::memory::free(const_cast<char*>(node->name));
        kernel::memory::free(node);
    }

    // ── VFS adapter wrappers ──────────────────────────────────────────────────

    VfsNode* find(VfsNode* parent, const char* name) const {
        if (!parent || !parent->ops || !parent->ops->find) return nullptr;
        return parent->ops->find(parent, name).value_or(nullptr);
    }

    // Convenience: find in root
    VfsNode* find_root(const char* name) const {
        return find(root, name);
    }

    usize read(VfsNode* node, usize offset, usize size, void* buf) const {
        if (!node || !node->ops || !node->ops->read) return -1;
        Result<usize> r = node->ops->read(node, offset, size, buf);
        if (r.is_err()) {
            return r.to_errno();
        }
        return r.unwrap();
    }

    usize write(VfsNode* node, usize offset, usize size, const void* buf) const {
        if (!node || !node->ops || !node->ops->write) return -1;
        Result<usize> r = node->ops->write(node, offset, size, buf);
        if (r.is_err()) {
            return r.to_errno();
        }
        return r.unwrap();
    }

    int stat(VfsNode* node, vespera_stat_t* out) const {
        if (!node || !node->ops || !node->ops->stat) return -1;
        return -node->ops->stat(node, out).to_errno();
    }

    int truncate(VfsNode* node, usize new_size) const {
        if (!node || !node->ops || !node->ops->truncate) return -1;
        return -node->ops->truncate(node, new_size).to_errno();
    }

    int create(VfsNode* parent, const char* name) const {
        if (!parent || !parent->ops || !parent->ops->create) return 1;
        return -parent->ops->create(parent, name).to_errno();
    }

    int mkdir(VfsNode* parent, const char* name) const {
        if (!parent || !parent->ops || !parent->ops->mkdir) return 1;
        return -parent->ops->mkdir(parent, name).to_errno();
    }

    int unlink(VfsNode* parent, const char* name) const {
        if (!parent || !parent->ops || !parent->ops->unlink) return 1;
        return -parent->ops->unlink(parent, name).to_errno();
    }

    int rmdir(VfsNode* parent, const char* name) const {
        if (!parent || !parent->ops || !parent->ops->rmdir) return 1;
        return -parent->ops->rmdir(parent, name).to_errno();
    }

    int rename(VfsNode* old_parent, const char* old_name, VfsNode* new_parent, const char* new_name) const {
        if (!old_parent || !old_parent->ops || !old_parent->ops->rename) return 1;
        return -old_parent->ops->rename(old_parent, old_name, new_parent, new_name).to_errno();
    }

    // ── Directory listing helpers ─────────────────────────────────────────────

    std::vector<std::string> list(VfsNode* node) const {
        std::vector<std::string> names;
        if (!node || !node->ops) return names;

        Result<void*> handle_r = node->ops->opendir(node);
        if (handle_r.is_err()) return names;
        void* handle = handle_r.unwrap();

        dirent_t de{};
        while (node->ops->readdir(handle, &de).value_or(false) == 1) names.emplace_back(de.name);

        node->ops->closedir(handle);
        return names;
    }

    std::vector<std::string> list_root() const {
        return list(root);
    }

    static bool list_contains(const std::vector<std::string>& lst, const char* name) {
        for (auto& s : lst)
            if (s == name) return true;
        return false;
    }

    // ── Convenience helpers for binary.bin / big.bin verification ────────────

    // Returns the expected byte at position i in binary.bin
    static uint8_t binary_pattern(usize i) {
        return static_cast<uint8_t>(i % 256);
    }

    // Returns the expected byte at position i in big.bin
    static uint8_t big_pattern(usize i) {
        return static_cast<uint8_t>((i * 7 + 13) % 256);
    }
};

// ── Macro: set up a fixture and abort on failure ──────────────────────────────
#define WITH_EXT4(var)                                                                                             \
    Ext4Fixture var;                                                                                               \
    if (!(var).valid()) {                                                                                          \
        TestFramework::fail_test(__FILE__, __LINE__, "Ext4Fixture invalid — is test_ext4.img present and valid?"); \
        return;                                                                                                    \
    }

// ── Macro: find a file/dir inside the fixture root; fail test on nullptr ──────
#define EXT4_FIND(fixture, name, var)                                                             \
    VfsNode* var = (fixture).find_root(name);                                                     \
    if (!(var)) {                                                                                 \
        TestFramework::fail_test(__FILE__, __LINE__, "find_root(\"" name "\") returned nullptr"); \
        return;                                                                                   \
    }

// ── RAII wrapper for a found VfsNode ─────────────────────────────────────────
struct NodeGuard {
    VfsNode* node;
    explicit NodeGuard(VfsNode* n)
        : node(n) {
    }
    ~NodeGuard() {
        Ext4Fixture::free_node(node);
    }
    NodeGuard(const NodeGuard&) = delete;
    NodeGuard& operator=(const NodeGuard&) = delete;
    VfsNode* operator->() const {
        return node;
    }
    explicit operator bool() const {
        return node != nullptr;
    }
};

#endif  // VESPERAOS_EXT4_FIXTURE_H