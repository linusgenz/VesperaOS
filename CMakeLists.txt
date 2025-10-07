cmake_minimum_required(VERSION 3.20)
project(VesperaOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -g -O0 -Wall")

file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp)
file(GLOB_RECURSE ASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/arch/**/*.asm ${CMAKE_CURRENT_SOURCE_DIR}/kernel/**/*.asm)
list(FILTER CPP_SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")

set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)


add_executable(kernel.elf ${CPP_SOURCES} ${ASM_SOURCES}
        userspace/lib/include/string.h)
set_target_properties(kernel.elf PROPERTIES
        LINK_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib -static"
)
target_include_directories(kernel.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/kernel/include
)

# Bootloader
add_library(bootloader-lint STATIC
        ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader/boot.c
)

set_target_properties(bootloader-lint PROPERTIES
        LINKER_LANGUAGE C
)
add_custom_target(bootloader
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building bootloader"
)

# splash.h
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/splash.h
        COMMAND xxd -i ${CMAKE_SOURCE_DIR}/Splash_VesperaOS.raw > ${CMAKE_SOURCE_DIR}/splash.h
        DEPENDS ${CMAKE_SOURCE_DIR}/Splash_VesperaOS.raw
        COMMENT "Generating splash.h"
)
add_custom_target(splash_header DEPENDS ${CMAKE_SOURCE_DIR}/splash.h)


# User-Lib
file(GLOB_RECURSE USERLIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/userspace/lib/src/*.c)
add_library(userlib STATIC ${USERLIB_SOURCES}
        userspace/lib/src/malloc.c
        userspace/lib/src/crt0.c)
target_include_directories(userlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/userspace/lib/include)
set_target_properties(userlib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)


# elfs
add_executable(shell
        ${CMAKE_CURRENT_SOURCE_DIR}/userspace/bin/shell.c
)
target_link_libraries(shell PRIVATE userlib)
set_target_properties(shell PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/userspace/bin
        LINK_FLAGS "-nostdlib -ffreestanding -fno-stack-protector -fPIE -pie"
)

# lsusb
add_executable(lsusb
        ${CMAKE_CURRENT_SOURCE_DIR}/userspace/bin/lsusb.c
)
target_link_libraries(lsusb PRIVATE userlib)
set_target_properties(lsusb PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/userspace/bin
        LINK_FLAGS "-nostdlib -ffreestanding -fno-stack-protector -fPIE -pie"
)

#logd
add_executable(logd
        ${CMAKE_CURRENT_SOURCE_DIR}/userspace/bin/logd.c
)
target_link_libraries(logd PRIVATE userlib)
set_target_properties(logd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/userspace/bin
        LINK_FLAGS "-nostdlib -ffreestanding -fno-stack-protector -fPIE -pie"
)

add_custom_target(elfs DEPENDS shell lsusb logd)

# version
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/kernel/kversion.h
        COMMAND bash ${CMAKE_SOURCE_DIR}/generate_version_header.sh
        DEPENDS ${CMAKE_SOURCE_DIR}/generate_version_header.sh
)
add_custom_target(version DEPENDS ${CMAKE_SOURCE_DIR}/kernel/kversion.h)

# Disk image
set(BOOTLOADER_EFI ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader/boot.efi)
set(IMG_SIZE 400000)
set(EFI_DIR mnt/EFI/BOOT)

add_custom_target(disk_image
        COMMAND ${CMAKE_COMMAND} -E echo "Creating partitioned disk image..."
        COMMAND bash ${CMAKE_SOURCE_DIR}/make_disk.sh
        ${CMAKE_BINARY_DIR}/boot.img
        ${BOOTLOADER_EFI}
        ${CMAKE_BINARY_DIR}/kernel.elf
        ${CMAKE_SOURCE_DIR}
        DEPENDS kernel.elf elfs bootloader version splash_header
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


add_custom_target(img
        COMMAND sudo dd if=${CMAKE_BINARY_DIR}/boot.img of=/dev/sdc bs=4M status=progress conv=fsync
        DEPENDS disk_image
)

# QEMU run
add_custom_target(test
        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu host
        #   -drive file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw,id=host0
        #  -device nvme,drive=host0,serial=deadbeef
        -drive if=pflash,format=raw,unit=0,file=${CMAKE_SOURCE_DIR}/OVMF/OVMF.fd
        -net none
        -monitor stdio
        -device qemu-xhci,id=xhci1
        -device qemu-xhci,id=xhci2
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw
        -drive id=usbstick2,file=${CMAKE_SOURCE_DIR}/disk.img,if=none,format=raw
        -device usb-storage,drive=usbstick
        -device usb-storage,drive=usbstick2
        -trace *xhci*
        -D /tmp/qemu.log
        -no-reboot
        -no-shutdown
        DEPENDS disk_image
)

add_custom_target(debug
        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu host

        -net none
        -monitor stdio
        -device qemu-xhci
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw
        -device usb-storage,drive=usbstick
        -device usb-kbd
        -drive if=pflash,format=raw,unit=0,file=${CMAKE_SOURCE_DIR}/OVMF/OVMF.fd
        -no-reboot
        -no-shutdown
        -s -S
        DEPENDS disk_image
)

# Clean target
add_custom_target(clean-all
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader clean
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/boot.img
        COMMAND ${CMAKE_COMMAND} -E rm -rf mnt isofiles
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
