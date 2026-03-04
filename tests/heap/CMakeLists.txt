# Heap Tests Module

add_executable(heap_tests
        ${PROJECT_ROOT}/kernel/memory/heap.cpp
        heap_tests_main.cpp
)
target_link_libraries(heap_tests PRIVATE kernel_stubs)
target_compile_options(heap_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
        # -fsanitize=address
)
target_include_directories(heap_tests PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/../
        ${PROJECT_ROOT}/include
        .
)

function(add_heap_test SUITE NAME)
    add_test(
            NAME ${SUITE}.${NAME}
            COMMAND heap_tests ${SUITE} ${NAME}
    )
endfunction()


add_heap_test(Heap_Malloc BasicAlloc)
add_heap_test(Heap_Malloc NullOnZeroSize)
add_heap_test(Heap_Malloc ReturnedPointerIsAligned)
add_heap_test(Heap_Malloc WriteAndReadBack)
add_heap_test(Heap_Malloc AllByteValues)
add_heap_test(Heap_Malloc MultipleDistinctPointers)
add_heap_test(Heap_Malloc ManySmallAllocations)
add_heap_test(Heap_Malloc SizeOneAllocated)
add_heap_test(Heap_Malloc LargeAllocation)
add_heap_test(Heap_Malloc ExhaustionReturnsNull)
add_heap_test(Heap_Malloc HeapValidAfterManyAllocs)
add_heap_test(Heap_Malloc AllocSizeRoundsUp)
add_heap_test(Heap_Malloc AdjacentBlocksDoNotOverlap)
add_heap_test(Heap_Malloc UsageIncreasesAfterAlloc)

add_heap_test(Heap_Free BasicFree)
add_heap_test(Heap_Free NullFreeIsNoop)
add_heap_test(Heap_Free FreeSpaceIncreasesAfterFree)
add_heap_test(Heap_Free FreedBlockIsReusable)
add_heap_test(Heap_Free ForwardCoalescing)
add_heap_test(Heap_Free BackwardCoalescing)
add_heap_test(Heap_Free AllAllocsFreedRestoresSpace)
add_heap_test(Heap_Free HeapValidAfterInterleavedAllocFree)
add_heap_test(Heap_Free IsValidPointerAfterFreeReturnsFalse)

add_heap_test(Heap_Realloc NullPtrActsAsMalloc)
add_heap_test(Heap_Realloc ZeroNewSizeActsAsFree)
add_heap_test(Heap_Realloc GrowPreservesData)
add_heap_test(Heap_Realloc ShrinkReturnsOriginalPointer)
add_heap_test(Heap_Realloc ShrinkPreservesData)
add_heap_test(Heap_Realloc GrowThenShrink)
add_heap_test(Heap_Realloc SameSize)

add_heap_test(Heap_Segment SplitProducesCorrectLengths)
add_heap_test(Heap_Segment SplitCreatesReuseableRemainder)
add_heap_test(Heap_Segment SplitDoesNotCorruptNeighbours)
add_heap_test(Heap_Segment SplitPreservesLinkedList)
add_heap_test(Heap_Segment CoalesceForwardMergesTwoBlocks)
add_heap_test(Heap_Segment CoalesceThreeBlocks)
add_heap_test(Heap_Segment CoalesceSkipsUsedBlock)
add_heap_test(Heap_Segment CoalesceBackwardMergesWithPrev)
add_heap_test(Heap_Segment CoalescePreservesLiveData)
add_heap_test(Heap_Segment AlternateAllocFreeCoalesces)
add_heap_test(Heap_Segment IsValidReturnsTrueForFreshBlock)
add_heap_test(Heap_Segment GuardBytesIntact)
add_heap_test(Heap_Segment MagicIsUsedAfterAlloc)
add_heap_test(Heap_Segment MagicIsFreeAfterFree)
add_heap_test(Heap_Segment FreeFieldClearedAfterFree)

add_heap_test(Heap_Aligned Align16)
add_heap_test(Heap_Aligned Align32)
add_heap_test(Heap_Aligned Align64)
add_heap_test(Heap_Aligned Align256)
add_heap_test(Heap_Aligned Align4096)
add_heap_test(Heap_Aligned MultipleAlignedAllocs)
add_heap_test(Heap_Aligned DataIntegrity)
add_heap_test(Heap_Aligned AlignedBlockDoesNotOverlapNeighbours)
add_heap_test(Heap_Aligned BoundaryConstraint64)
add_heap_test(Heap_Aligned BoundaryConstraint4096)
add_heap_test(Heap_Aligned BoundaryAndAlignmentBothSatisfied)
add_heap_test(Heap_Aligned ZeroSizeReturnsNull)
add_heap_test(Heap_Aligned ZeroAlignmentReturnsNull)
add_heap_test(Heap_Aligned NonPowerOfTwoAlignmentReturnsNull)
add_heap_test(Heap_Aligned NonPowerOfTwoBoundaryReturnsNull)
add_heap_test(Heap_Aligned FreeAlignedRestoresSpace)
add_heap_test(Heap_Aligned FreeAlignedHeapValid)

add_heap_test(Heap_Validate IsValidPtrReturnsTrueForLiveBlock)
add_heap_test(Heap_Validate IsValidPtrReturnsFalseForNull)
add_heap_test(Heap_Validate IsValidPtrReturnsFalseForHeapStart)
add_heap_test(Heap_Validate IsValidPtrReturnsFalseForArbitraryAddress)
add_heap_test(Heap_Validate IsValidPtrReturnsFalseAfterFree)
add_heap_test(Heap_Validate ValidateEmptyHeap)
add_heap_test(Heap_Validate ValidateAfterSingleAlloc)
add_heap_test(Heap_Validate ValidateAfterSingleFree)
add_heap_test(Heap_Validate ValidateAfterManyOps)
add_heap_test(Heap_Validate GuardByteIntactAfterWrite)
add_heap_test(Heap_Validate ChecksumMatchesAfterAlloc)
add_heap_test(Heap_Validate CorruptedGuardDetected)
add_heap_test(Heap_Validate CorruptedChecksumDetected)

add_heap_test(Heap_Edge AllocMinAllocSize)
add_heap_test(Heap_Edge AllocSizeJustBelowHeader)
add_heap_test(Heap_Edge FreeSamePointerTwiceHandledGracefully)
add_heap_test(Heap_Edge ReallocSameSizeReturnsSameOrNewValid)
add_heap_test(Heap_Edge GetFreeSpaceDecreasesOnAlloc)
add_heap_test(Heap_Edge GetHeapUsageIsConsistent)

add_heap_test(Heap_Stress RandomAllocFreePattern)
add_heap_test(Heap_Stress VariousSizes)
add_heap_test(Heap_Stress FragmentationRecovery)
add_heap_test(Heap_Stress AllocFillVerifyFreeRepeat)
add_heap_test(Heap_Stress MixedAlignedAndRegular)