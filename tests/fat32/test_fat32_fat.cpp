// test_fat32_fat.cpp
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
// FAT32 Tests — FAT Table & Cluster Chain
// =============================================================================
// Covers: GetFATEntry / WriteFATEntry / GetClusterChain / FreeClusterChain /
//         FindFreeCluster / HasFATLoop / NextCluster
// =============================================================================

#include "fat32_fixture.h"

// =============================================================================
// GetFATEntry
// =============================================================================

TEST(FAT32_FAT, GetEntryReservedClusters, "Clusters 0 and 1 are reserved; GetFATEntry returns EOF") {
    WITH_FAT32(f);
    ASSERT_EQ(static_cast<uint32_t>(0x0FFFFFFF), f.fs->get_fat_entry(0));
    ASSERT_EQ(static_cast<uint32_t>(0x0FFFFFFF), f.fs->get_fat_entry(1));
}

TEST(FAT32_FAT, GetEntryOutOfRange, "Cluster number beyond clusterCount returns EOF") {
    WITH_FAT32(f);
    ASSERT_EQ(static_cast<uint32_t>(0x0FFFFFFF), f.fs->get_fat_entry(0x0FFFFF00));
}

// =============================================================================
// WriteFATEntry / ReadBack
// =============================================================================

TEST(FAT32_FAT, WriteAndReadBack, "Written FAT entry can be read back correctly") {
    WITH_FAT32(f);

    uint32_t c1 = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c1);
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);  // mark allocated

    uint32_t c2 = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c2);
    ASSERT_NE(c1, c2);

    ASSERT_TRUE(f.fs->write_fat_entry(c1, c2));
    ASSERT_EQ(c2, f.fs->get_fat_entry(c1) & 0x0FFFFFFF);

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
}

TEST(FAT32_FAT, FreeCountUpdatedAfterDelete, "Free cluster count increases after a file is deleted") {
    WITH_FAT32(f);

    auto node = f.create_file("COUNTTEST.TXT");
    ASSERT_TRUE(f.write(node, "X", 1));

    uint32_t before = f.fs->get_free_cluster_count();
    Fat32Node parent = f.root_node();
    f.fs->delete_file(&parent, "COUNTTEST.TXT");
    uint32_t after = f.fs->get_free_cluster_count();

    ASSERT_GE(after, before);
}

// =============================================================================
// FindFreeCluster
// =============================================================================

TEST(FAT32_FAT, FindFreeClusterValid, "FindFreeCluster returns a valid cluster number >= 2") {
    WITH_FAT32(f);
    uint32_t c = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c);
    ASSERT_GE(c, static_cast<uint32_t>(2));
}

TEST(FAT32_FAT, FindFreeClusterDistinct, "Two consecutive FindFreeCluster calls return different clusters") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c1);
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);

    uint32_t c2 = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c2);
    ASSERT_NE(c1, c2);

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
}

// =============================================================================
// NextCluster
// =============================================================================

TEST(FAT32_FAT, NextClusterEOF, "NextCluster on an EOF cluster returns 0") {
    WITH_FAT32(f);
    uint32_t c = f.fs->find_free_cluster();
    ASSERT_NE(static_cast<uint32_t>(0), c);
    f.fs->write_fat_entry(c, 0x0FFFFFFF);

    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(c));

    f.fs->write_fat_entry(c, 0);
}

TEST(FAT32_FAT, NextClusterFree, "NextCluster on a free cluster (value 0) returns 0") {
    WITH_FAT32(f);
    uint32_t c = f.fs->find_free_cluster();
    // cluster is free, do not allocate it
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(c));
}

TEST(FAT32_FAT, NextClusterChain, "NextCluster traverses a manually built three-cluster chain") {
    WITH_FAT32(f);

    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    uint32_t c3 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c3, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);
    f.fs->write_fat_entry(c2, c3);
    // c3 -> EOF

    ASSERT_EQ(c2, f.fs->next_cluster(c1));
    ASSERT_EQ(c3, f.fs->next_cluster(c2));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(c3));

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
    f.fs->write_fat_entry(c3, 0);
}

TEST(FAT32_FAT, NextClusterInvalidInputs, "NextCluster returns 0 for cluster 0, 1, and 0x0FFFFFFF") {
    WITH_FAT32(f);
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(0));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(1));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->next_cluster(0x0FFFFFFF));
}

// =============================================================================
// GetClusterChain
// =============================================================================

TEST(FAT32_FAT, GetChainSingleCluster, "GetClusterChain on a one-cluster file returns a one-element array") {
    WITH_FAT32(f);
    uint32_t c = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c, 0x0FFFFFFF);

    size_t count = 0;
    uint32_t* chain = f.fs->get_cluster_chain(c, count);
    ASSERT_NOT_NULL(chain);
    ASSERT_EQ(static_cast<size_t>(1), count);
    ASSERT_EQ(c, chain[0]);

    kernel::memory::free(chain);
    f.fs->write_fat_entry(c, 0);
}

TEST(FAT32_FAT, GetChainThreeClusters, "GetClusterChain returns all three clusters in order") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    uint32_t c3 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c3, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);
    f.fs->write_fat_entry(c2, c3);

    size_t count = 0;
    uint32_t* chain = f.fs->get_cluster_chain(c1, count);
    ASSERT_NOT_NULL(chain);
    ASSERT_EQ(static_cast<size_t>(3), count);
    ASSERT_EQ(c1, chain[0]);
    ASSERT_EQ(c2, chain[1]);
    ASSERT_EQ(c3, chain[2]);

    kernel::memory::free(chain);
    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
    f.fs->write_fat_entry(c3, 0);
}

TEST(FAT32_FAT, GetChainInvalidCluster, "GetClusterChain returns nullptr for clusters 0, 1, and EOF") {
    WITH_FAT32(f);
    size_t count = 0;
    ASSERT_NULL(f.fs->get_cluster_chain(0, count));
    ASSERT_NULL(f.fs->get_cluster_chain(1, count));
    ASSERT_NULL(f.fs->get_cluster_chain(0x0FFFFFFF, count));
}

TEST(FAT32_FAT, GetChainLoopDetected, "GetClusterChain returns nullptr when a FAT loop is present") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);
    f.fs->write_fat_entry(c2, c1);  // loop: c2 -> c1

    size_t count = 0;
    uint32_t* chain = f.fs->get_cluster_chain(c1, count);
    ASSERT_NULL(chain);

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
}

// =============================================================================
// HasFATLoop (Floyd's Cycle Detection)
// =============================================================================

TEST(FAT32_FAT, HasLoopNormalChain, "HasFATLoop returns false for a normal chain") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);

    ASSERT_FALSE(f.fs->has_fat_loop(c1));

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
}

TEST(FAT32_FAT, HasLoopSelfReference, "HasFATLoop detects a self-referencing cluster (c -> c)") {
    WITH_FAT32(f);
    uint32_t c = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c, c);  // self-loop

    ASSERT_TRUE(f.fs->has_fat_loop(c));

    f.fs->write_fat_entry(c, 0);
}

TEST(FAT32_FAT, HasLoopAtEnd, "HasFATLoop detects a loop at the end of a chain") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    uint32_t c3 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c3, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);
    f.fs->write_fat_entry(c2, c3);
    f.fs->write_fat_entry(c3, c2);  // loop: c3 -> c2

    ASSERT_TRUE(f.fs->has_fat_loop(c1));

    f.fs->write_fat_entry(c1, 0);
    f.fs->write_fat_entry(c2, 0);
    f.fs->write_fat_entry(c3, 0);
}

// =============================================================================
// FreeClusterChain
// =============================================================================

TEST(FAT32_FAT, FreeChainZerosAllEntries, "FreeClusterChain sets all FAT entries in the chain to 0") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    uint32_t c3 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c3, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);
    f.fs->write_fat_entry(c2, c3);

    ASSERT_TRUE(f.fs->free_cluster_chain(c1));

    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->get_fat_entry(c1));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->get_fat_entry(c2));
    ASSERT_EQ(static_cast<uint32_t>(0), f.fs->get_fat_entry(c3));
}

TEST(FAT32_FAT, FreeChainInvalidStartReturnsFalse, "FreeClusterChain returns false for clusters 0 and 1") {
    WITH_FAT32(f);
    ASSERT_FALSE(f.fs->free_cluster_chain(0));
    ASSERT_FALSE(f.fs->free_cluster_chain(1));
}

TEST(FAT32_FAT, FreeChainIncreasesCounter, "FreeClusterChain increases the free cluster count") {
    WITH_FAT32(f);
    uint32_t c1 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c1, 0x0FFFFFFF);
    uint32_t c2 = f.fs->find_free_cluster();
    f.fs->write_fat_entry(c2, 0x0FFFFFFF);
    f.fs->write_fat_entry(c1, c2);

    uint32_t before = f.fs->get_free_cluster_count();
    if (before == 0xFFFFFFFF) return;  // count unknown — skip

    f.fs->free_cluster_chain(c1);

    ASSERT_GE(f.fs->get_free_cluster_count(), before + 2);
}