# userspace/CMakeLists.txt

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fno-stack-protector -fPIC -fPIE -nostdlib -static -g -O0 -Wall")

add_subdirectory(lib)

set(USER_LD_FLAGS "-nostdlib -static -ffreestanding -fno-stack-protector -fPIC -fPIE")

add_executable(shell ${CMAKE_SOURCE_DIR}/userspace/bin/shell.c)
add_executable(lsusb ${CMAKE_SOURCE_DIR}/userspace/bin/lsusb.c)
add_executable(logd ${CMAKE_SOURCE_DIR}/userspace/bin/logd.c)


# Link against vesplib
foreach(bin IN ITEMS shell lsusb logd)
    target_link_libraries(${bin} PRIVATE vesplib)
    set_target_properties(${bin} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/userspace/bin
            LINK_FLAGS "${USER_LD_FLAGS}"
    )
endforeach()

add_custom_target(elfs DEPENDS shell lsusb logd)