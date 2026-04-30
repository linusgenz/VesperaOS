// test_ext4_rename.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.03.26.
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
#include <cstring>

TEST(Ext4_Rename, RenameBasic,
     "Renamed file appears under the new name, old name is gone") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "old.txt"));
    ASSERT_EQ(0, f.rename(f.root, "old.txt", f.root, "new.txt"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "new.txt"));
    ASSERT_FALSE(f.list_contains(names, "old.txt"));
}

TEST(Ext4_Rename, RenamePreservesContent,
     "File content is intact after rename") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "before.txt"));

    VfsNode* before = f.find_root("before.txt");
    ASSERT_NOT_NULL(before);
    constexpr char data[] = "preserve this";
    constexpr usize len   = sizeof(data) - 1;
    ASSERT_GE(f.write(before, 0, len, data), static_cast<isize>(1));
    Ext4Fixture::free_node(before);

    ASSERT_EQ(0, f.rename(f.root, "before.txt", f.root, "after.txt"));

    VfsNode* after = f.find_root("after.txt");
    ASSERT_NOT_NULL(after);
    NodeGuard guard(after);

    char buf[32] = {};
    isize r = f.read(after, 0, len, buf);
    ASSERT_EQ(static_cast<isize>(len), r);
    ASSERT_MEM_EQ(data, buf, len);
}

TEST(Ext4_Rename, RenameImageFile,
     "An original image file can be renamed") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.rename(f.root, "hello.txt", f.root, "hello_renamed.txt"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "hello_renamed.txt"));
    ASSERT_FALSE(f.list_contains(names, "hello.txt"));
}

TEST(Ext4_Rename, RenamePreservesImageFileContent,
     "Content of an image file is intact after renaming it") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.rename(f.root, "hello.txt", f.root, "renamed_hello.txt"));

    VfsNode* node = f.find_root("renamed_hello.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    char buf[32] = {};
    isize r = f.read(node, 0, HELLO_TXT_SIZE, buf);
    ASSERT_EQ(static_cast<isize>(HELLO_TXT_SIZE), r);
    ASSERT_MEM_EQ("hello from ext4\n", buf, HELLO_TXT_SIZE);
}

TEST(Ext4_Rename, RenameDirectory,
     "A directory can be renamed and is still findable") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "orig_dir"));
    ASSERT_EQ(0, f.rename(f.root, "orig_dir", f.root, "moved_dir"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "moved_dir"));
    ASSERT_FALSE(f.list_contains(names, "orig_dir"));
}

TEST(Ext4_Rename, RenameDirectoryContentPreserved,
     "Files inside a renamed directory are still accessible") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "dirsrc"));

    VfsNode* src = f.find_root("dirsrc");
    ASSERT_NOT_NULL(src);
    ASSERT_EQ(0, f.create(src, "inner.txt"));
    Ext4Fixture::free_node(src);

    ASSERT_EQ(0, f.rename(f.root, "dirsrc", f.root, "dirdst"));

    VfsNode* dst = f.find_root("dirdst");
    ASSERT_NOT_NULL(dst);
    NodeGuard guard(dst);

    auto names = f.list(dst);
    ASSERT_TRUE(f.list_contains(names, "inner.txt"));
}

TEST(Ext4_Rename, RenameCrossDirectory,
     "Renaming a file from root into a subdirectory succeeds") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "crossfile.txt"));

    EXT4_FIND(f, "subdir", subdir);
    NodeGuard guard(subdir);

    ASSERT_EQ(0, f.rename(f.root, "crossfile.txt", subdir, "crossfile.txt"));

    ASSERT_FALSE(f.list_contains(f.list_root(), "crossfile.txt"));
    ASSERT_TRUE (f.list_contains(f.list(subdir), "crossfile.txt"));
}

TEST(Ext4_Rename, RenameCrossDirectoryPreservesContent,
     "Cross-directory rename preserves file content") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "cross_content.txt"));

    VfsNode* src = f.find_root("cross_content.txt");
    ASSERT_NOT_NULL(src);
    constexpr char data[] = "cross dir content";
    constexpr usize len   = sizeof(data) - 1;
    ASSERT_GE(f.write(src, 0, len, data), static_cast<isize>(1));
    Ext4Fixture::free_node(src);

    EXT4_FIND(f, "subdir", subdir);

    ASSERT_EQ(0, f.rename(f.root, "cross_content.txt", subdir, "cross_content.txt"));

    VfsNode* dst = subdir->ops->find(subdir, "cross_content.txt");
    ASSERT_NOT_NULL(dst);
    NodeGuard dg(dst);
    Ext4Fixture::free_node(subdir);

    char buf[32] = {};
    isize r = f.read(dst, 0, len, buf);
    ASSERT_EQ(static_cast<isize>(len), r);
    ASSERT_MEM_EQ(data, buf, len);
}

TEST(Ext4_Rename, RenameMissingSourceReturnsError,
     "rename() with a non-existent source name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rename(f.root, "ghost.txt", f.root, "new.txt"));
}

TEST(Ext4_Rename, RenameNullOldParent,
     "rename() with null old_parent returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rename(nullptr, "hello.txt", f.root, "new.txt"));
}

TEST(Ext4_Rename, RenameNullOldName,
     "rename() with null old_name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rename(f.root, nullptr, f.root, "new.txt"));
}

TEST(Ext4_Rename, RenameNullNewParent,
     "rename() with null new_parent returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rename(f.root, "hello.txt", nullptr, "new.txt"));
}

TEST(Ext4_Rename, RenameNullNewName,
     "rename() with null new_name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rename(f.root, "hello.txt", f.root, nullptr));
}

TEST(Ext4_Rename, RenameFromFileNodeFails,
     "rename() with a regular file as old_parent returns an error") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", file);
    NodeGuard guard(file);

    ASSERT_NE(0, f.rename(file, "hello.txt", f.root, "new.txt"));
}

TEST(Ext4_Rename, FsRenameRoundTrip,
     "fs->rename() same-directory rename is consistent with find()") {
    WITH_EXT4(f);

    u32 dir_inode = ext4::EXT4_ROOT_INODE;
    ASSERT_TRUE(f.fs->create_file(dir_inode, "fs_rename_src.txt").is_ok());
    ASSERT_TRUE(f.fs->rename(dir_inode, "fs_rename_src.txt",
                              dir_inode, "fs_rename_dst.txt"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "fs_rename_dst.txt"));
    ASSERT_FALSE(f.list_contains(names, "fs_rename_src.txt"));
}