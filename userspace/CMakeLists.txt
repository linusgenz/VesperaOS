set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_C_FLAGS
        "-ffreestanding -nostdlib -fno-stack-protector -fPIE -g -O0 -Wall"
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

add_subdirectory(lib)

set(USER_LD_FLAGS
        "-nostdlib -ffreestanding -fno-stack-protector -pie"
)

add_executable(shell ${CMAKE_SOURCE_DIR}/userspace/bin/shell.c)
add_executable(lsusb ${CMAKE_SOURCE_DIR}/userspace/bin/lsusb.c)
add_executable(logd ${CMAKE_SOURCE_DIR}/userspace/bin/logd.c)
add_executable(uptime ${CMAKE_SOURCE_DIR}/userspace/bin/uptime.c)
add_executable(memstat ${CMAKE_SOURCE_DIR}/userspace/bin/memstat.c)

foreach(bin IN ITEMS shell lsusb logd uptime memstat)
    target_link_libraries(${bin} PRIVATE vesplib)

    set_target_properties(${bin} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/userspace/bin
            LINK_FLAGS "${USER_LD_FLAGS}"
    )
endforeach()

add_custom_target(elfs DEPENDS shell lsusb logd uptime memstat)
