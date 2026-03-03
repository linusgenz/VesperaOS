// test_fat32_directory.cpp
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

// =============================================================================
// FAT32 Tests — Directory Operations
// =============================================================================
// Covers: CreateFile / CreateDirectory / DeleteFile / RemoveDirectory /
//         ReadDirectory / LFN names / Rename / Path resolution
// =============================================================================

#include "fat32_fixture.h"

// =============================================================================
// CreateFile
// =============================================================================

TEST(FAT32_Dir, CreateFileAppearsInListing, "Created file appears in directory listing") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "HELLO.TXT"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "HELLO.TXT"));
}

TEST(FAT32_Dir, CreateFileShortName, "Create a file with a short 8.3 name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "README.MD"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "README.MD"));
}

TEST(FAT32_Dir, CreateFileLFN, "Create a file with a long LFN name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "this_is_a_long_filename.txt"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "this_is_a_long_filename.txt"));
}

TEST(FAT32_Dir, CreateFileLFNWithSpace, "Create a file whose LFN contains a space") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "hello world.txt"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "hello world.txt"));
}

TEST(FAT32_Dir, CreateFileLFNVeryLong, "Create a file whose name spans more than one LFN entry block") {
    WITH_FAT32(f);
    Fat32Node  parent   = f.root_node();
    const char* name    = "this_filename_is_very_very_very_very_very_long_indeed_for_lfn.txt";
    ASSERT_TRUE(f.fs->CreateFile(&parent, name));
    ASSERT_TRUE(f.list_contains(f.list_root(), name));
}

TEST(FAT32_Dir, CreateFileNullRejected, "CreateFile rejects null parent, null name, and empty name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->CreateFile(nullptr,  "test.txt"));
    ASSERT_FALSE(f.fs->CreateFile(&parent,  nullptr));
    ASSERT_FALSE(f.fs->CreateFile(&parent,  ""));
}

TEST(FAT32_Dir, CreateFileInitialSizeZero, "Newly created file has fileSize == 0") {
    WITH_FAT32(f);
    auto node = f.create_file("ZERO.TXT");
    ASSERT_EQ(static_cast<size_t>(0), node.fileSize);
}

// =============================================================================
// CreateDirectory
// =============================================================================

TEST(FAT32_Dir, CreateDirAppearsInListing, "Created directory appears in directory listing") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateDirectory(&parent, "MYDIR"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "MYDIR"));
}

TEST(FAT32_Dir, CreateDirHasDotEntries, "New directory contains '.' and '..' entries") {
    WITH_FAT32(f);
    auto dir = f.create_dir("DOTTEST");

    size_t count = 0;
    FAT32::FileEntry* entries = f.fs->ReadDirectory(dir.cluster, count);
    ASSERT_NOT_NULL(entries);

    bool dot = false, dotdot = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].GetName(), ".")  == 0) dot    = true;
        if (strcmp(entries[i].GetName(), "..") == 0) dotdot = true;
    }
    kernel::memory::free(entries);

    ASSERT_TRUE(dot);
    ASSERT_TRUE(dotdot);
}

TEST(FAT32_Dir, CreateDirLFN, "Create a directory with a long LFN name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateDirectory(&parent, "my_long_directory_name"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "my_long_directory_name"));
}

TEST(FAT32_Dir, CreateDirNullRejected, "CreateDirectory rejects null parent, null name, and empty name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->CreateDirectory(nullptr,  "dir"));
    ASSERT_FALSE(f.fs->CreateDirectory(&parent,  nullptr));
    ASSERT_FALSE(f.fs->CreateDirectory(&parent,  ""));
}

// =============================================================================
// DeleteFile
// =============================================================================

TEST(FAT32_Dir, DeleteFileRemovesFromListing, "Deleted file disappears from directory listing") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "DEL.TXT"));
    ASSERT_TRUE(f.fs->DeleteFile(&parent, "DEL.TXT"));
    ASSERT_FALSE(f.list_contains(f.list_root(), "DEL.TXT"));
}

TEST(FAT32_Dir, DeleteFileFreesCluster, "Deleting a file marks its cluster as free in the FAT") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "FREECLUS.TXT"));

    auto     node       = f.find_file_node("FREECLUS.TXT");
    ASSERT_TRUE(f.write(node, "DATA", 4));
    uint32_t old_cluster = node.cluster;

    ASSERT_TRUE(f.fs->DeleteFile(&parent, "FREECLUS.TXT"));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->GetFATEntry(old_cluster));
}

TEST(FAT32_Dir, DeleteFileMissingReturnsFalse, "Deleting a non-existent file returns false") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->DeleteFile(&parent, "GHOST.TXT"));
}

TEST(FAT32_Dir, DeleteReadOnlyRejected, "Deleting a read-only file is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("PROT.TXT");
    node.dirEntry.attr |= FAT32::ATTR_READ_ONLY;
    f.fs->OverwriteDirectoryEntry(node.parentCluster, node.currentIndex, &node.dirEntry);

    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->DeleteFile(&parent, "PROT.TXT"));
}

TEST(FAT32_Dir, DeleteFileNullRejected, "DeleteFile rejects null parent and null name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->DeleteFile(nullptr,  "X.TXT"));
    ASSERT_FALSE(f.fs->DeleteFile(&parent,  nullptr));
}

TEST(FAT32_Dir, DeleteLFNFileFullyRemoved, "Deleting an LFN file removes all its directory entries") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "long_lfn_delete_test.txt"));
    ASSERT_TRUE(f.fs->DeleteFile(&parent, "long_lfn_delete_test.txt"));
    ASSERT_FALSE(f.list_contains(f.list_root(), "long_lfn_delete_test.txt"));
}

// =============================================================================
// RemoveDirectory
// =============================================================================

TEST(FAT32_Dir, RemoveDirEmpty, "Removing an empty directory succeeds") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateDirectory(&parent, "EMPTYDIR"));
    ASSERT_TRUE(f.fs->RemoveDirectory(&parent, "EMPTYDIR"));
    ASSERT_FALSE(f.list_contains(f.list_root(), "EMPTYDIR"));
}

TEST(FAT32_Dir, RemoveDirNonEmptyRejected, "Removing a non-empty directory is rejected") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateDirectory(&parent, "FULLDIR"));

    auto dir = f.find_dir_node("FULLDIR");
    ASSERT_TRUE(f.fs->CreateFile(&dir, "INSIDE.TXT"));

    ASSERT_FALSE(f.fs->RemoveDirectory(&parent, "FULLDIR"));
}

TEST(FAT32_Dir, RemoveDirMissingReturnsFalse, "Removing a non-existent directory returns false") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->RemoveDirectory(&parent, "NOPE"));
}

TEST(FAT32_Dir, RemoveDirFreesCluster, "Removing a directory marks its cluster as free in the FAT") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateDirectory(&parent, "FREETEST"));

    auto     dir     = f.find_dir_node("FREETEST");
    uint32_t cluster = dir.cluster;

    ASSERT_TRUE(f.fs->RemoveDirectory(&parent, "FREETEST"));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->GetFATEntry(cluster));
}

// =============================================================================
// ReadDirectory
// =============================================================================

TEST(FAT32_Dir, ReadDirInvalidPathReturnsNull, "ReadDirectory with invalid path returns nullptr") {
    WITH_FAT32(f);
    size_t count = 0;
    ASSERT_NULL(f.fs->ReadDirectory("/NONEXISTENT", count));
}

TEST(FAT32_Dir, ReadDirRootPathValid, "ReadDirectory on root path '/' succeeds") {
    WITH_FAT32(f);
    size_t count = 0;
    auto*  entries = f.fs->ReadDirectory("/", count);
    ASSERT_NOT_NULL(entries);
    kernel::memory::free(entries);
}

TEST(FAT32_Dir, ReadDirMultipleEntries, "Multiple created files all appear in the listing") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    f.fs->CreateFile(&parent, "ALPHA.TXT");
    f.fs->CreateFile(&parent, "BETA.TXT");
    f.fs->CreateFile(&parent, "GAMMA.TXT");

    auto names = f.list_root();
    ASSERT_TRUE(f.list_contains(names, "ALPHA.TXT"));
    ASSERT_TRUE(f.list_contains(names, "BETA.TXT"));
    ASSERT_TRUE(f.list_contains(names, "GAMMA.TXT"));
}

// =============================================================================
// Rename
// =============================================================================

TEST(FAT32_Dir, RenameBasic, "Renamed file appears under new name, old name is gone") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "OLD.TXT"));
    ASSERT_TRUE(f.fs->Rename(&parent, "OLD.TXT", "NEW.TXT"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "NEW.TXT"));
    ASSERT_FALSE(f.list_contains(names, "OLD.TXT"));
}

TEST(FAT32_Dir, RenamePreservesContent, "File content is intact after rename") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "CONTENT.TXT"));

    auto node = f.find_file_node("CONTENT.TXT");
    ASSERT_TRUE(f.write(node, "KEEPME", 6));
    ASSERT_TRUE(f.fs->Rename(&parent, "CONTENT.TXT", "RENAMED.TXT"));

    auto renamed = f.find_file_node("RENAMED.TXT");
    auto result  = f.read(renamed, 6);
    ASSERT_EQ(static_cast<size_t>(6), result.size());
    ASSERT_MEM_EQ("KEEPME", result.data(), 6);
}

TEST(FAT32_Dir, RenameMissingReturnsFalse, "Renaming a non-existent file returns false") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->Rename(&parent, "GHOST.TXT", "NEW.TXT"));
}

TEST(FAT32_Dir, RenameNullRejected, "Rename rejects null/empty arguments") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->Rename(nullptr,  "A",  "B"));
    ASSERT_FALSE(f.fs->Rename(&parent,  nullptr, "B"));
    ASSERT_FALSE(f.fs->Rename(&parent,  "A", nullptr));
    ASSERT_FALSE(f.fs->Rename(&parent,  "",  "B"));
    ASSERT_FALSE(f.fs->Rename(&parent,  "A", ""));
}

TEST(FAT32_Dir, RenameProtectedRejected, "Renaming a protected (read-only) file is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("PROT2.TXT");
    node.dirEntry.attr |= FAT32::ATTR_READ_ONLY;
    f.fs->OverwriteDirectoryEntry(node.parentCluster, node.currentIndex, &node.dirEntry);

    Fat32Node parent = f.root_node();
    ASSERT_FALSE(f.fs->Rename(&parent, "PROT2.TXT", "OTHER.TXT"));
}

TEST(FAT32_Dir, RenameLFNToLFN, "Rename from one LFN name to another LFN name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "my_old_long_name.txt"));
    ASSERT_TRUE(f.fs->Rename(&parent, "my_old_long_name.txt", "my_new_long_name.txt"));

    auto names = f.list_root();
    ASSERT_TRUE (f.list_contains(names, "my_new_long_name.txt"));
    ASSERT_FALSE(f.list_contains(names, "my_old_long_name.txt"));
}

TEST(FAT32_Dir, RenameShortToLFN, "Rename from a short 8.3 name to a long LFN name") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    ASSERT_TRUE(f.fs->CreateFile(&parent, "SHORT.TXT"));
    ASSERT_TRUE(f.fs->Rename(&parent, "SHORT.TXT", "now_a_long_name_after_rename.txt"));
    ASSERT_TRUE(f.list_contains(f.list_root(), "now_a_long_name_after_rename.txt"));
}

// =============================================================================
// Path resolution
// =============================================================================

TEST(FAT32_Dir, ResolveRelativePathReturnsZero, "Relative path (no leading slash) resolves to 0") {
    WITH_FAT32(f);
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->ResolvePathToCluster("relative/path"));
}

TEST(FAT32_Dir, ResolveMissingPathReturnsZero, "Non-existent path resolves to 0") {
    WITH_FAT32(f);
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->ResolvePathToCluster("/DOESNOTEXIST"));
}

TEST(FAT32_Dir, ResolveRootPath, "Root path '/' resolves to GetRootCluster()") {
    WITH_FAT32(f);
    ASSERT_EQ(f.fs->GetRootCluster(), f.fs->ResolvePathToCluster("/"));
}

TEST(FAT32_Dir, ResolveCreatedDirectory, "A newly created directory can be resolved by path") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    f.fs->CreateDirectory(&parent, "PATHTEST");
    ASSERT_NE(static_cast<uint32_t>(0), f.fs->ResolvePathToCluster("/PATHTEST"));
}