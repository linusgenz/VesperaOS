// test_fat32_fs.cpp
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
// FAT32 Tests — Filesystem Validation, BPB & Stress
// =============================================================================
// Covers: Initialization with valid/invalid images, BPB sanity,
//         FSInfo persistence, OverwriteDirectoryEntry, stress scenarios
// =============================================================================

#include "fat32_fixture.h"

// =============================================================================
// Filesystem initialization
// =============================================================================

TEST(FAT32_FS, ValidImageIsValid, "A filesystem mounted from test.img reports is_valid() == true") {
    WITH_FAT32(f);
    ASSERT_TRUE(f.fs->is_valid());
}

TEST(FAT32_FS, RootClusterAtLeastTwo, "GetRootCluster() returns a value >= 2") {
    WITH_FAT32(f);
    ASSERT_GE(f.fs->GetRootCluster(), static_cast<uint32_t>(2));
}

TEST(FAT32_FS, BytesPerClusterIsPowerOfTwo, "bytesPerCluster() is a power of two and >= 512") {
    WITH_FAT32(f);
    uint32_t bpc = f.fs->bytesPerCluster();
    ASSERT_GE(bpc, static_cast<uint32_t>(512));
    ASSERT_EQ(static_cast<uint32_t>(0), bpc & (bpc - 1));
}

TEST(FAT32_FS, ClusterToSectorRoot, "ClusterToSector for the root cluster returns a sector > 0") {
    WITH_FAT32(f);
    ASSERT_GE(f.fs->ClusterToSector(f.fs->GetRootCluster()), static_cast<uint32_t>(1));
}

TEST(FAT32_FS, AllZeroImageInvalid, "A zeroed block device is not a valid FAT32 filesystem") {
    auto* mdev = new MockBlockDevice(64);  // 64 sectors, all zero
    auto* bad_fs = new FAT32::FileSystem(mdev);
    ASSERT_FALSE(bad_fs->is_valid());
    delete bad_fs;
    delete mdev;
}

TEST(FAT32_FS, AllFFImageInvalid, "A 0xFF-filled block device is not a valid FAT32 filesystem") {
    auto* mdev = new MockBlockDevice(512);
    memset(mdev->raw(), 0xFF, 512 * MockBlockDevice::SECTOR_SIZE);
    auto* bad_fs = new FAT32::FileSystem(mdev);
    ASSERT_FALSE(bad_fs->is_valid());
    delete bad_fs;
    delete mdev;
}

// =============================================================================
// FSInfo persistence
// =============================================================================

TEST(
    FAT32_FS, FreeCountDecreasesAfterCreateFile,
    "Free cluster count does not increase after creating and writing a file"
) {
    WITH_FAT32(f);
    uint32_t before = f.fs->GetFreeClusterCount();
    if (before == 0xFFFFFFFF) return;  // count unknown — skip

    Fat32Node parent = f.root_node();
    f.fs->CreateFile(&parent, "INFOTEST.TXT");
    auto node = f.find_file_node("INFOTEST.TXT");
    f.write(node, "DATA", 4);

    ASSERT_LE(f.fs->GetFreeClusterCount(), before);
}

TEST(FAT32_FS, FreeCountIncreasesAfterDeleteFile, "Free cluster count does not decrease after deleting a file") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    f.fs->CreateFile(&parent, "DELINFO.TXT");
    auto node = f.find_file_node("DELINFO.TXT");
    f.write(node, "PAYLOAD", 7);

    uint32_t before = f.fs->GetFreeClusterCount();
    if (before == 0xFFFFFFFF) return;

    f.fs->DeleteFile(&parent, "DELINFO.TXT");
    ASSERT_GE(f.fs->GetFreeClusterCount(), before);
}

// =============================================================================
// OverwriteDirectoryEntry
// =============================================================================

TEST(
    FAT32_FS, OverwriteDirEntryAttributePersists,
    "Attribute change written via OverwriteDirectoryEntry is visible after reload"
) {
    WITH_FAT32(f);
    auto node = f.create_file("ATTRTEST.TXT");
    node.dirEntry.attr |= FAT32::ATTR_READ_ONLY;
    ASSERT_TRUE(f.fs->OverwriteDirectoryEntry(node.parentCluster, node.currentIndex, &node.dirEntry));

    auto fresh = f.find_file_node("ATTRTEST.TXT");
    ASSERT_TRUE(fresh.dirEntry.attr & FAT32::ATTR_READ_ONLY);
}

TEST(
    FAT32_FS, OverwriteDirEntryBadIndexReturnsFalse, "OverwriteDirectoryEntry returns false for an out-of-range index"
) {
    WITH_FAT32(f);
    auto node = f.create_file("BADIDX.TXT");
    ASSERT_FALSE(f.fs->OverwriteDirectoryEntry(node.parentCluster, 999999, &node.dirEntry));
}

// =============================================================================
// Stress tests
// =============================================================================

TEST(FAT32_FS, Stress10FilesCreateWriteReadDelete, "Create, write, read, and delete 10 files without errors") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();

    constexpr int N = 10;
    char names[N][16];

    for (int i = 0; i < N; i++) {
        snprintf(names[i], sizeof(names[i]), "STRESS%02d.TXT", i);
        ASSERT_TRUE(f.fs->CreateFile(&parent, names[i]));
    }

    for (int i = 0; i < N; i++) {
        auto node = f.find_file_node(names[i]);
        char buf[32];
        snprintf(buf, sizeof(buf), "Data for file %d", i);
        ASSERT_TRUE(f.write(node, buf, strlen(buf)));
    }

    for (int i = 0; i < N; i++) {
        auto node = f.find_file_node(names[i]);
        char expected[32];
        snprintf(expected, sizeof(expected), "Data for file %d", i);
        size_t len = strlen(expected);

        auto result = f.read(node, len);
        ASSERT_EQ(len, result.size());
        ASSERT_MEM_EQ(expected, result.data(), len);
    }

    for (auto & name : names) ASSERT_TRUE(f.fs->DeleteFile(&parent, name));

    auto listing = f.list_root();
    for (auto & name : names) ASSERT_FALSE(f.list_contains(listing, name));
}

TEST(FAT32_FS, StressVariousSizes, "Write and read files of various sizes spanning boundary values") {
    WITH_FAT32(f);
    const size_t sizes[] = {1, 511, 512, 513, 4095, 4096, 4097, 8192, 16384};
    constexpr int n = std::size(sizes);

    for (int i = 0; i < n; i++) {
        char name[32];
        snprintf(name, sizeof(name), "SIZE%05zu.BIN", sizes[i]);

        auto node = f.create_file(name);
        std::vector<uint8_t> data(sizes[i], static_cast<uint8_t>(i + 1));
        ASSERT_TRUE(f.write(node, data.data(), sizes[i]));

        auto result = f.read(node, sizes[i]);
        ASSERT_EQ(sizes[i], result.size());
        ASSERT_MEM_EQ(data.data(), result.data(), sizes[i]);

        Fat32Node parent = f.root_node();
        f.fs->DeleteFile(&parent, name);
    }
}

TEST(FAT32_FS, StressDirectoryOverflow, "Creating >128 files forces the directory to span multiple clusters") {
    WITH_FAT32(f);
    Fat32Node parent = f.root_node();
    constexpr int N = 200;

    for (int i = 0; i < N; i++) {
        char name[16];
        snprintf(name, sizeof(name), "F%03d.TXT", i);
        ASSERT_TRUE(f.fs->CreateFile(&parent, name));
    }

    // All files must appear in the listing
    auto listing = f.list_root();
    ASSERT_GE(listing.size(), static_cast<size_t>(N));

    for (int i = 0; i < N; i++) {
        char name[16];
        snprintf(name, sizeof(name), "F%03d.TXT", i);
        f.fs->DeleteFile(&parent, name);
    }
}