cmake_minimum_required(VERSION 3.20) # 4.0 gibt es nicht bei CMake
project(VesperaOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -Wall")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -g -O0 -Wall")

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
)

file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp)
file(GLOB_RECURSE ASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/arch/**/*.asm ${CMAKE_CURRENT_SOURCE_DIR}/kernel/**/*.asm)
list(FILTER CPP_SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")

set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)

add_executable(kernel.elf ${CPP_SOURCES} ${ASM_SOURCES})
set_target_properties(kernel.elf PROPERTIES
        LINK_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib -static"
)

# Bootloader
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

# elfs
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/bin/shell.elf
        COMMAND gcc -nostdlib -ffreestanding -fno-stack-protector -fPIE -pie
        -o ${CMAKE_SOURCE_DIR}/bin/shell.elf ${CMAKE_SOURCE_DIR}/bin/shell.c
        DEPENDS ${CMAKE_SOURCE_DIR}/bin/shell.c
)
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/bin/shell1.elf
        COMMAND gcc -nostdlib -ffreestanding -fno-stack-protector -fPIE -pie
        -o ${CMAKE_SOURCE_DIR}/bin/shell1.elf ${CMAKE_SOURCE_DIR}/bin/shell1.c
        DEPENDS ${CMAKE_SOURCE_DIR}/bin/shell1.c
)
add_custom_target(elfs DEPENDS
        ${CMAKE_SOURCE_DIR}/bin/shell.elf
        ${CMAKE_SOURCE_DIR}/bin/shell1.elf
)

# version
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/kernel/version.h
        COMMAND bash ${CMAKE_SOURCE_DIR}/generate_version_header.sh
        DEPENDS ${CMAKE_SOURCE_DIR}/generate_version_header.sh
)
add_custom_target(version DEPENDS ${CMAKE_SOURCE_DIR}/kernel/version.h)

# Disk image
set(BOOTLOADER_EFI ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader/boot.efi)
set(IMG_SIZE 131072)
set(EFI_DIR mnt/EFI/BOOT)

add_custom_target(disk_image
        COMMAND ${CMAKE_COMMAND} -E echo "Creating disk image..."
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/boot.img
        COMMAND dd if=/dev/zero of=${CMAKE_BINARY_DIR}/boot.img bs=512 count=${IMG_SIZE}
        COMMAND sudo mkfs.fat -F 32 -n "VesperaOS" ${CMAKE_BINARY_DIR}/boot.img
        COMMAND sudo mkdir -p mnt ${EFI_DIR}
        COMMAND sudo mount -o loop ${CMAKE_BINARY_DIR}/boot.img mnt
        COMMAND sudo mkdir -p mnt/bin ${EFI_DIR}
        COMMAND sudo cp ${CMAKE_SOURCE_DIR}/bin/shell.elf mnt/bin/shell.elf
        COMMAND sudo cp ${CMAKE_SOURCE_DIR}/bin/shell1.elf mnt/bin/shell1.elf
        COMMAND sudo cp ${BOOTLOADER_EFI} ${EFI_DIR}/BOOTX64.EFI
        COMMAND sudo cp ${CMAKE_BINARY_DIR}/kernel.elf ${EFI_DIR}/kernel.elf
        COMMAND sudo cp ${CMAKE_SOURCE_DIR}/build/zap-light16.psf mnt/zap-light16.psf
        COMMAND sudo cp ${CMAKE_SOURCE_DIR}/build/startup.nsh mnt/
        COMMAND sudo umount mnt
        COMMAND sudo ${CMAKE_COMMAND} -E rm -rf mnt

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
        -drive file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw,id=host0
        -device nvme,drive=host0,serial=deadbeef
        -drive if=pflash,format=raw,unit=0,file=${CMAKE_SOURCE_DIR}/OVMF/OVMF.fd
        -net none
        -monitor stdio
        -device qemu-xhci,id=xhci0
        -device qemu-xhci,id=xhci1
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot1.img,if=none,format=raw
        -device usb-storage,drive=usbstick,bus=xhci1.0
        -device usb-kbd,bus=xhci1.0
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
        -drive file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw,id=host0
        -device nvme,drive=host0,serial=deadbeef
        -net none
        -monitor stdio
        -device qemu-xhci
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot1.img,if=none,format=raw
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
