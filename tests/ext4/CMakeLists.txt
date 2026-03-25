# ext4 Tests Module

add_executable(ext4_tests
        ${PROJECT_ROOT}/filesystem/ext4/ext4.cpp
        ${PROJECT_ROOT}/filesystem/ext4/ext4_vfs_adapter.cpp
        ext4_tests_main.cpp
)
target_link_libraries(ext4_tests PRIVATE kernel_stubs)
target_compile_options(ext4_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
)

set(EXT4_IMG    ${CMAKE_CURRENT_BINARY_DIR}/test_ext4.img)
set(EXT4_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/create_ext4_img.sh)

add_custom_command(
        OUTPUT  ${EXT4_IMG}
        COMMAND bash ${EXT4_SCRIPT} ${EXT4_IMG}
        DEPENDS ${EXT4_SCRIPT}
        COMMENT "Generating test_ext4.img for ext4 VFS adapter tests"
)

add_custom_target(generate_ext4_test_img ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_ext4.img)

add_dependencies(ext4_tests generate_ext4_test_img)

function(add_ext4_test SUITE NAME)
    add_test(
            NAME ${SUITE}.${NAME}
            COMMAND ext4_tests ${SUITE} ${NAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

add_ext4_test(Ext4_Find RootFindExistingEntry)
add_ext4_test(Ext4_Find RootFindExistingDirectory)
add_ext4_test(Ext4_Find FindMissingEntryReturnsNull)
add_ext4_test(Ext4_Find FindOnNullParentReturnsNull)
add_ext4_test(Ext4_Find FindOnFileNodeReturnsNull)
add_ext4_test(Ext4_Find FindInSubdirectory)
add_ext4_test(Ext4_Find FindChildPathIsCorrect)
add_ext4_test(Ext4_Find FindChildPathInSubdir)
add_ext4_test(Ext4_Find FindDotEntryReturnsNode)

add_ext4_test(Ext4_Opendir RootOpendirReturnsHandle)
add_ext4_test(Ext4_Opendir OpendirOnNullNodeReturnsNull)
add_ext4_test(Ext4_Opendir OpendirOnFileReturnsNull)
add_ext4_test(Ext4_Opendir SubdirOpendirReturnsHandle)
add_ext4_test(Ext4_Opendir MultipleOpendirCallsIndependent)

add_ext4_test(Ext4_Readdir ReadsAtLeastOneName)
add_ext4_test(Ext4_Readdir ReturnsZeroAtEnd)
add_ext4_test(Ext4_Readdir FindsHelloTxt)
add_ext4_test(Ext4_Readdir FindsSubdir)
add_ext4_test(Ext4_Readdir FindsDotAndDotDot)
add_ext4_test(Ext4_Readdir EntryCountMatchesDirect)
add_ext4_test(Ext4_Readdir NamesAreNullTerminated)
add_ext4_test(Ext4_Readdir SubdirContainsNestedTxt)
add_ext4_test(Ext4_Readdir IndependentHandlesProduceSameEntries)

add_ext4_test(Ext4_Closedir ClosedirOnNullIsNoop)
add_ext4_test(Ext4_Closedir ClosedirFreesHandle)
add_ext4_test(Ext4_Closedir ClosedirAfterFullDrain)
add_ext4_test(Ext4_Closedir ClosedirOnSubdirHandle)