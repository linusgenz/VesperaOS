# Vector Tests Module

add_executable(vector_tests
        vector_tests_main.cpp
)
target_link_libraries(vector_tests PRIVATE kernel_stubs)
target_include_directories(vector_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/include
        ${CMAKE_SOURCE_DIR}/tests/framework
        ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(vector_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
        -Wall
        -Wextra
)


function(add_vector_test SUITE NAME)
    add_test(
            NAME ${SUITE}.${NAME}
            COMMAND vector_tests ${SUITE} ${NAME}
    )
endfunction()


add_vector_test(Vector_PushBack DefaultConstruct)
add_vector_test(Vector_PushBack CustomCapacity)
add_vector_test(Vector_PushBack LargeInitialCapacity)
add_vector_test(Vector_PushBack SinglePush)
add_vector_test(Vector_PushBack ValuePreserved)
add_vector_test(Vector_PushBack MultiplePushes)
add_vector_test(Vector_PushBack ZeroValue)
add_vector_test(Vector_PushBack NegativeValue)
add_vector_test(Vector_PushBack MaxIntValue)
add_vector_test(Vector_PushBack GrowthBeyondInitialCapacity)
add_vector_test(Vector_PushBack GrowthKeepsAllValues)
add_vector_test(Vector_PushBack DataPointerValid)
add_vector_test(Vector_PushBack DataPointerMatchesIndex)
add_vector_test(Vector_PushBack NonTrivialTypeStored)
add_vector_test(Vector_PushBack NonTrivialDestructorCalledOnDestroy)
add_vector_test(Vector_PushBack NonTrivialGrowthPreservesValues)
add_vector_test(Vector_PushBack PointerType)
add_vector_test(Vector_PushBack Stress1000)

add_vector_test(Vector_Clear ClearSetsLengthToZero)
add_vector_test(Vector_Clear ClearAllowsReuse)
add_vector_test(Vector_Clear ClearEmpty)
add_vector_test(Vector_Clear ClearCallsDestructorsOnNonTrivial)
add_vector_test(Vector_Clear ClearThenPushManyTimes)

add_vector_test(Vector_Back BackReturnLastElement)
add_vector_test(Vector_Back BackAfterPopBack)
add_vector_test(Vector_Back BackIsWritable)
add_vector_test(Vector_Back ConstBack)
add_vector_test(Vector_Back SingleElement)

add_vector_test(Vector_Iterator BeginEqualsEndForEmpty)
add_vector_test(Vector_Iterator BeginPlusNEqualsEnd)
add_vector_test(Vector_Iterator RangeForWorks)
add_vector_test(Vector_Iterator CbeginCend)
add_vector_test(Vector_Iterator WriteThroughBegin)
add_vector_test(Vector_Iterator ConstIterator)

add_vector_test(Vector_Move MoveConstructor)
add_vector_test(Vector_Move MoveAssignment)
add_vector_test(Vector_Move SelfMoveAssignment)
add_vector_test(Vector_Move MoveConstructorNonTrivial)

add_vector_test(Vector_Copy CopyProducesIndependentVector)
add_vector_test(Vector_Copy CopyIsDeepNotShallow)
add_vector_test(Vector_Copy CopyOfEmpty)
add_vector_test(Vector_Copy CopyNonTrivial)

add_vector_test(Vector_Misc OperatorBracketConst)
add_vector_test(Vector_Misc OperatorBracketWrite)
add_vector_test(Vector_Misc SizeAndEmptyConsistency)
add_vector_test(Vector_Misc LargeDataType)
add_vector_test(Vector_Misc VectorOfVectorsPushBack)

add_vector_test(Vector_Lifecycle NoLeakAfterPushAndDestroy)
add_vector_test(Vector_Lifecycle NoLeakAfterClear)
add_vector_test(Vector_Lifecycle NoLeakAfterMoveConstruct)
add_vector_test(Vector_Lifecycle NoLeakAfterErase)
add_vector_test(Vector_Lifecycle NoLeakAfterPopBack)

add_vector_test(Vector_EdgeCase PushThenEraseRepeatedly)
add_vector_test(Vector_EdgeCase PushEraseAlternate)
add_vector_test(Vector_EdgeCase PushPopSymmetric)
add_vector_test(Vector_EdgeCase SingleElementLifecycle)
add_vector_test(Vector_EdgeCase ClearAfterGrowth)

add_vector_test(Vector_Erase EraseFirst)
add_vector_test(Vector_Erase EraseLast)
add_vector_test(Vector_Erase EraseMiddle)
add_vector_test(Vector_Erase EraseOnlyElement)
add_vector_test(Vector_Erase EraseAllSequentially)
add_vector_test(Vector_Erase NonTrivialDestructorCalledOnErase)

add_vector_test(Vector_EraseValue FindsAndRemovesFirst)
add_vector_test(Vector_EraseValue ReturnsFalseWhenNotFound)
add_vector_test(Vector_EraseValue EmptyVectorReturnsFalse)
add_vector_test(Vector_EraseValue RemovesOnlyElement)
add_vector_test(Vector_EraseValue RemovesLastElement)
add_vector_test(Vector_EraseValue DoesNotRemoveDuplicates)

add_vector_test(Vector_Pop PopDecreasesSize)
add_vector_test(Vector_Pop PopDoesNotAlterRemaining)
add_vector_test(Vector_Pop PopAllElements)

add_vector_test(Vector_PopBack ReturnsRemovedValue)
add_vector_test(Vector_PopBack SequentialPops)
add_vector_test(Vector_PopBack SingleElementPop)
add_vector_test(Vector_PopBack NonTrivialType)

add_vector_test(Vector_Panic EraseOutOfRangePanics)
add_vector_test(Vector_Panic BackOnEmptyPanics)
add_vector_test(Vector_Panic PopOnEmptyPanics)
add_vector_test(Vector_Panic PopBackOnEmptyPanics)
