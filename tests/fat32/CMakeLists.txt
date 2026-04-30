# FAT32 Tests Module

add_executable(fat32_tests
        ${PROJECT_ROOT}/filesystem/fat32/fat32.cpp
        ${PROJECT_ROOT}/filesystem/fat32/fat32_lfn.cpp
        ${PROJECT_ROOT}/filesystem/fat32/fat32_time.cpp
        ${PROJECT_ROOT}/lib/path.cpp
        ${PROJECT_ROOT}/lib/time.cpp
        fat32_tests_main.cpp
)
target_link_libraries(fat32_tests PRIVATE kernel_stubs)
target_compile_options(fat32_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test.img
        COMMAND dd if=/dev/zero of=${CMAKE_CURRENT_BINARY_DIR}/test.img bs=512 count=70000 status=none
        COMMAND mkfs.fat -F 32 -S 512 ${CMAKE_CURRENT_BINARY_DIR}/test.img
        COMMENT "Generating test.img for FAT32 tests"
)

add_custom_target(generate_test_img ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test.img)

add_dependencies(fat32_tests generate_test_img)

function(add_fat32_test SUITE NAME)
    add_test(
            NAME ${SUITE}.${NAME}
            COMMAND fat32_tests ${SUITE} ${NAME}
    )
endfunction()


add_fat32_test(FAT32_Dir CreateFileAppearsInListing)
add_fat32_test(FAT32_Dir CreateFileShortName)
add_fat32_test(FAT32_Dir CreateFileLFN)
add_fat32_test(FAT32_Dir CreateFileLFNWithSpace)
add_fat32_test(FAT32_Dir CreateFileLFNVeryLong)
add_fat32_test(FAT32_Dir CreateFileNullRejected)
add_fat32_test(FAT32_Dir CreateFileInitialSizeZero)
add_fat32_test(FAT32_Dir CreateDirAppearsInListing)
add_fat32_test(FAT32_Dir CreateDirHasDotEntries)
add_fat32_test(FAT32_Dir CreateDirLFN)
add_fat32_test(FAT32_Dir CreateDirNullRejected)
add_fat32_test(FAT32_Dir DeleteFileRemovesFromListing)
add_fat32_test(FAT32_Dir DeleteFileFreesCluster)
add_fat32_test(FAT32_Dir DeleteFileMissingReturnsFalse)
add_fat32_test(FAT32_Dir DeleteReadOnlyRejected)
add_fat32_test(FAT32_Dir DeleteFileNullRejected)
add_fat32_test(FAT32_Dir DeleteLFNFileFullyRemoved)
add_fat32_test(FAT32_Dir RemoveDirEmpty)
add_fat32_test(FAT32_Dir RemoveDirNonEmptyRejected)
add_fat32_test(FAT32_Dir RemoveDirMissingReturnsFalse)
add_fat32_test(FAT32_Dir RemoveDirFreesCluster)
add_fat32_test(FAT32_Dir ReadDirRootPathValid)
add_fat32_test(FAT32_Dir ReadDirMultipleEntries)
add_fat32_test(FAT32_Dir RenameBasic)
add_fat32_test(FAT32_Dir RenamePreservesContent)
add_fat32_test(FAT32_Dir RenameMissingReturnsFalse)
add_fat32_test(FAT32_Dir RenameNullRejected)
add_fat32_test(FAT32_Dir RenameProtectedRejected)
add_fat32_test(FAT32_Dir RenameLFNToLFN)
add_fat32_test(FAT32_Dir RenameShortToLFN)
add_fat32_test(FAT32_Dir ResolveRelativePathReturnsZero)
add_fat32_test(FAT32_Dir ResolveMissingPathReturnsZero)
add_fat32_test(FAT32_Dir ResolveRootPath)
add_fat32_test(FAT32_Dir ResolveCreatedDirectory)

add_fat32_test(FAT32_FAT GetEntryReservedClusters)
add_fat32_test(FAT32_FAT GetEntryOutOfRange)
add_fat32_test(FAT32_FAT WriteAndReadBack)
add_fat32_test(FAT32_FAT FreeCountUpdatedAfterDelete)
add_fat32_test(FAT32_FAT FindFreeClusterValid)
add_fat32_test(FAT32_FAT FindFreeClusterDistinct)
add_fat32_test(FAT32_FAT NextClusterEOF)
add_fat32_test(FAT32_FAT NextClusterFree)
add_fat32_test(FAT32_FAT NextClusterChain)
add_fat32_test(FAT32_FAT NextClusterInvalidInputs)
add_fat32_test(FAT32_FAT GetChainSingleCluster)
add_fat32_test(FAT32_FAT GetChainThreeClusters)
add_fat32_test(FAT32_FAT GetChainInvalidCluster)
add_fat32_test(FAT32_FAT GetChainLoopDetected)
add_fat32_test(FAT32_FAT HasLoopNormalChain)
add_fat32_test(FAT32_FAT HasLoopSelfReference)
add_fat32_test(FAT32_FAT HasLoopAtEnd)
add_fat32_test(FAT32_FAT FreeChainZerosAllEntries)
add_fat32_test(FAT32_FAT FreeChainInvalidStartReturnsFalse)
add_fat32_test(FAT32_FAT FreeChainIncreasesCounter)

add_fat32_test(FAT32_FileIO BasicWriteRead)
add_fat32_test(FAT32_FileIO WriteEmptyRejected)
add_fat32_test(FAT32_FileIO ReadEmptyFile)
add_fat32_test(FAT32_FileIO ReadExactSize)
add_fat32_test(FAT32_FileIO ReadOffsetAtEOF)
add_fat32_test(FAT32_FileIO PartialOverwrite)
add_fat32_test(FAT32_FileIO WriteOffsetBeyondEOF)
add_fat32_test(FAT32_FileIO MultiClusterWrite)
add_fat32_test(FAT32_FileIO ExactlyTwoClusters)
add_fat32_test(FAT32_FileIO ClusterBoundaryPlusOne)
add_fat32_test(FAT32_FileIO AllByteValues)
add_fat32_test(FAT32_FileIO SequentialAppend)
add_fat32_test(FAT32_FileIO FileSizePersisted)
add_fat32_test(FAT32_FileIO ReadClampedToFileSize)
add_fat32_test(FAT32_FileIO OverwriteSameSize)
add_fat32_test(FAT32_FileIO ReadAtOffset)
add_fat32_test(FAT32_FileIO ReadOnlyProtection)
add_fat32_test(FAT32_FileIO SystemFileProtection)
add_fat32_test(FAT32_FileIO LargeFile128KB)

add_fat32_test(FAT32_FS ValidImageIsValid)
add_fat32_test(FAT32_FS RootClusterAtLeastTwo)
add_fat32_test(FAT32_FS BytesPerClusterIsPowerOfTwo)
add_fat32_test(FAT32_FS ClusterToSectorRoot)
add_fat32_test(FAT32_FS AllZeroImageInvalid)
add_fat32_test(FAT32_FS AllFFImageInvalid)
add_fat32_test(FAT32_FS FreeCountDecreasesAfterCreateFile)
add_fat32_test(FAT32_FS FreeCountIncreasesAfterDeleteFile)
add_fat32_test(FAT32_FS OverwriteDirEntryAttributePersists)
add_fat32_test(FAT32_FS OverwriteDirEntryBadIndexReturnsFalse)
add_fat32_test(FAT32_FS Stress10FilesCreateWriteReadDelete)
add_fat32_test(FAT32_FS StressVariousSizes)
add_fat32_test(FAT32_FS StressDirectoryOverflow)