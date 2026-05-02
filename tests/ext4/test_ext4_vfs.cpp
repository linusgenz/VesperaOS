// test_ext4_vfs.cpp
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

#include "ext4_fixture.h"

// =============================================================================
// ext4_find
// =============================================================================

TEST(Ext4_Find, RootFindExistingEntry, "find() on root returns a non-null node for an existing file") {
    WITH_EXT4(f);

    // The image is created with a file called "hello.txt" in the root.
    VfsNode* node = f.find(f.root, "hello.txt");
    ASSERT_NOT_NULL(node);
    ASSERT_FALSE(node->type == VfsNodeType::Directory);

    kernel::memory::free(node->internal_data);
    kernel::memory::free(node);
}

TEST(Ext4_Find, RootFindExistingDirectory, "find() on root returns a directory node for an existing subdirectory") {
    WITH_EXT4(f);

    VfsNode* node = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(node);
    ASSERT_EQ(static_cast<int>(VfsNodeType::Directory), static_cast<int>(node->type));

    kernel::memory::free(node->internal_data);
    kernel::memory::free(node);
}

TEST(Ext4_Find, FindMissingEntryReturnsNull, "find() returns nullptr for an entry that does not exist") {
    WITH_EXT4(f);
    VfsNode* node = f.find(f.root, "does_not_exist.txt");
    ASSERT_NULL(node);
}

TEST(Ext4_Find, FindOnNullParentReturnsNull, "find() returns nullptr when the parent node is null") {
    WITH_EXT4(f);
    // Call ops->find directly to bypass the fixture wrapper's null-guard.
    VfsNode* result = f.root->ops->find(nullptr, "hello.txt").value_or(nullptr);
    ASSERT_NULL(result);
}

TEST(Ext4_Find, FindOnFileNodeReturnsNull, "find() returns nullptr when the parent is a file, not a directory") {
    WITH_EXT4(f);

    VfsNode* file = f.find(f.root, "hello.txt");
    ASSERT_NOT_NULL(file);

    // file is not a directory — find should refuse to search it.
    VfsNode* result = f.root->ops->find(file, "anything").value_or(nullptr);
    ASSERT_NULL(result);

    kernel::memory::free(file->internal_data);
    kernel::memory::free(file);
}

TEST(Ext4_Find, FindInSubdirectory, "find() resolves an entry inside a subdirectory") {
    WITH_EXT4(f);

    // The image contains subdir/nested.txt.
    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    VfsNode* nested = f.root->ops->find(subdir, "nested.txt").value_or(nullptr);
    ASSERT_NOT_NULL(nested);

    kernel::memory::free(nested->internal_data);
    kernel::memory::free(nested);
    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

TEST(Ext4_Find, FindChildPathIsCorrect, "The child node's path is the concatenation of parent path and name") {
    WITH_EXT4(f);

    VfsNode* node = f.find(f.root, "hello.txt");
    ASSERT_NOT_NULL(node);

    auto* nd = static_cast<Ext4Node*>(node->internal_data);
    // Root path is "/" so the child path must be "/hello.txt".
    ASSERT_STR_EQ("/hello.txt", nd->path);

    kernel::memory::free(node->internal_data);
    kernel::memory::free(node);
}

TEST(Ext4_Find, FindChildPathInSubdir, "Child path inside a subdirectory is <parent_path>/<name>") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    VfsNode* nested = f.root->ops->find(subdir, "nested.txt").value_or(nullptr);
    ASSERT_NOT_NULL(nested);

    auto* nd = static_cast<Ext4Node*>(nested->internal_data);
    ASSERT_STR_EQ("/subdir/nested.txt", nd->path);

    kernel::memory::free(nested->internal_data);
    kernel::memory::free(nested);
    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

TEST(Ext4_Find, FindDotEntryReturnsNode, "find() can locate the '.' entry present in every directory") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    // "." must be present in any directory.
    VfsNode* dot = f.root->ops->find(subdir, ".").value_or(nullptr);
    ASSERT_NOT_NULL(dot);

    kernel::memory::free(dot->internal_data);
    kernel::memory::free(dot);
    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

// =============================================================================
// ext4_opendir
// =============================================================================

TEST(Ext4_Opendir, RootOpendirReturnsHandle, "opendir() on the root directory returns a non-null handle") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();
    ASSERT_NOT_NULL(handle);

    f.root->ops->closedir(handle);
}

TEST(Ext4_Opendir, OpendirOnNullNodeReturnsNull, "opendir() returns nullptr when the node is null") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(nullptr);
    ASSERT_TRUE(handle_r.is_err());
}

TEST(Ext4_Opendir, OpendirOnFileReturnsNull, "opendir() returns nullptr for a file node (not a directory)") {
    WITH_EXT4(f);

    VfsNode* file = f.find(f.root, "hello.txt");
    ASSERT_NOT_NULL(file);

    // A file node has is_dir == false; opendir must reject it.
    // The adapter reads is_dir from Ext4Node, so set it explicitly.
    auto* nd = static_cast<Ext4Node*>(file->internal_data);
    nd->is_dir = false;

    auto handle_r = f.root->ops->opendir(file);
    ASSERT_TRUE(handle_r.is_err());

    kernel::memory::free(file->internal_data);
    kernel::memory::free(file);
}

TEST(Ext4_Opendir, SubdirOpendirReturnsHandle, "opendir() on a subdirectory returns a non-null handle") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    auto handle_r = f.root->ops->opendir(subdir);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();
    ASSERT_NOT_NULL(handle);

    f.root->ops->closedir(handle);
    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

TEST(
    Ext4_Opendir, MultipleOpendirCallsIndependent,
    "Two independent opendir handles on the same directory are independent"
) {
    WITH_EXT4(f);

    auto h1_r = f.root->ops->opendir(f.root);
    auto h2_r = f.root->ops->opendir(f.root);

    ASSERT_TRUE(h1_r.is_ok());
    ASSERT_TRUE(h2_r.is_ok());

    void* h1 = h1_r.unwrap();
    void* h2 = h2_r.unwrap();

    ASSERT_NOT_NULL(h1);
    ASSERT_NOT_NULL(h2);
    ASSERT_NE(h1, h2);

    f.root->ops->closedir(h1);
    f.root->ops->closedir(h2);
}

// =============================================================================
// ext4_readdir
// =============================================================================

TEST(Ext4_Readdir, ReadsAtLeastOneName, "readdir() returns at least one entry from the root directory") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();

    dirent_t de{};
    int rc = f.root->ops->readdir(handle, &de).value_or(0);

    ASSERT_EQ(1, rc);
    ASSERT_TRUE(de.name[0] != '\0');

    f.root->ops->closedir(handle);
}

TEST(Ext4_Readdir, ReturnsZeroAtEnd, "readdir() returns 0 after all entries have been consumed") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();

    dirent_t de{};
    while (f.root->ops->readdir(handle, &de).unwrap() == 1) {
    }

    ASSERT_EQ(0, f.root->ops->readdir(handle, &de).unwrap());

    f.root->ops->closedir(handle);
}

TEST(Ext4_Readdir, FindsHelloTxt, "readdir() over root yields an entry named 'hello.txt'") {
    WITH_EXT4(f);
    auto names = f.list_root();
    ASSERT_TRUE(f.list_contains(names, "hello.txt"));
}

TEST(Ext4_Readdir, FindsSubdir, "readdir() over root yields an entry named 'subdir'") {
    WITH_EXT4(f);
    auto names = f.list_root();
    ASSERT_TRUE(f.list_contains(names, "subdir"));
}

TEST(Ext4_Readdir, FindsDotAndDotDot, "readdir() over a subdirectory yields '.' and '..' entries") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    auto names = f.list(subdir);
    ASSERT_TRUE(f.list_contains(names, "."));
    ASSERT_TRUE(f.list_contains(names, ".."));

    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

TEST(Ext4_Readdir, EntryCountMatchesDirect, "Entry count via readdir matches count from FileSystem::read_directory") {
    WITH_EXT4(f);

    auto names = f.list_root();

    usize direct_count = 0;
    auto entries_r = f.fs->read_directory(ext4::EXT4_ROOT_INODE, direct_count);

    ASSERT_TRUE(entries_r.is_ok());

    ext4::FileEntry* entries = entries_r.unwrap();
    ASSERT_NOT_NULL(entries);

    kernel::memory::free(entries);

    ASSERT_EQ(direct_count, names.size());
}

TEST(Ext4_Readdir, NamesAreNullTerminated, "Every name returned by readdir is null-terminated within dirent_t") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();

    dirent_t de{};
    while (f.root->ops->readdir(handle, &de).unwrap() == 1) {
        bool found = false;
        for (size_t i = 0; i < sizeof(de.name); ++i) {
            if (de.name[i] == '\0') {
                found = true;
                break;
            }
        }

        ASSERT_TRUE(found);
    }

    f.root->ops->closedir(handle);
}

TEST(Ext4_Readdir, SubdirContainsNestedTxt, "readdir() over 'subdir' yields 'nested.txt'") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    auto names = f.list(subdir);
    ASSERT_TRUE(f.list_contains(names, "nested.txt"));

    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);
}

TEST(
    Ext4_Readdir, IndependentHandlesProduceSameEntries,
    "Two independent handles on the same directory yield the same entry set"
) {
    WITH_EXT4(f);

    auto names1 = f.list_root();
    auto names2 = f.list_root();

    ASSERT_EQ(names1.size(), names2.size());
    for (auto& n : names1) ASSERT_TRUE(f.list_contains(names2, n.c_str()));
}

// =============================================================================
// ext4_closedir
// =============================================================================

TEST(Ext4_Closedir, ClosedirOnNullIsNoop, "closedir(nullptr) does not crash") {
    WITH_EXT4(f);
    // Must not crash.
    f.root->ops->closedir(nullptr);
    ASSERT_TRUE(true);
}

TEST(Ext4_Closedir, ClosedirFreesHandle, "After closedir the handle is released (no crash on normal close)") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();
    ASSERT_NOT_NULL(handle);

    f.root->ops->closedir(handle);
    ASSERT_TRUE(true);
}

TEST(Ext4_Closedir, ClosedirAfterFullDrain, "closedir after completely draining a directory does not crash") {
    WITH_EXT4(f);

    auto handle_r = f.root->ops->opendir(f.root);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();
    ASSERT_NOT_NULL(handle);

    dirent_t de{};
    while (f.root->ops->readdir(handle, &de).value_or(false) == 1) { /* drain */
    }

    f.root->ops->closedir(handle);
    ASSERT_TRUE(true);
}

TEST(Ext4_Closedir, ClosedirOnSubdirHandle, "closedir works correctly on a handle obtained from a subdirectory") {
    WITH_EXT4(f);

    VfsNode* subdir = f.find(f.root, "subdir");
    ASSERT_NOT_NULL(subdir);

    auto handle_r = f.root->ops->opendir(subdir);
    ASSERT_TRUE(handle_r.is_ok());

    void* handle = handle_r.unwrap();
    ASSERT_NOT_NULL(handle);

    f.root->ops->closedir(handle);

    kernel::memory::free(subdir->internal_data);
    kernel::memory::free(subdir);

    ASSERT_TRUE(true);
}