cmake_minimum_required(VERSION 4.0.2)
project(VesperaOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_BUILD_TYPE Debug)

# Subdirectories
add_subdirectory(kernel)
add_subdirectory(userspace)

# Bootloader
add_library(bootloader-lint STATIC
        ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader/boot.c
        arch/x86_64/boot/bss.cpp
        arch/x86_64/boot/bss.h
        arch/x86_64/smp/prepare_ap_trampoline.cpp
        arch/x86_64/smp/prepare_ap_trampoline.h
        include/boot.h
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

# Disk image target
set(BOOTLOADER_EFI ${CMAKE_SOURCE_DIR}/gnu-efi/bootloader/boot.efi)
set(IMG_SIZE 400000)
set(EFI_DIR mnt/EFI/BOOT)


add_custom_target(disk_image
        COMMAND ${CMAKE_COMMAND} -E echo "Creating partitioned disk image..."
        COMMAND bash ${CMAKE_SOURCE_DIR}/make_disk.sh
        ${CMAKE_BINARY_DIR}/boot.img
        ${BOOTLOADER_EFI}
        ${CMAKE_BINARY_DIR}/kernel/kernel.elf
        ${CMAKE_SOURCE_DIR}
        DEPENDS bootloader version kernel.elf elfs   # splash_header
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


# img (write to device)
add_custom_target(img
        COMMAND sudo dd if=${CMAKE_BINARY_DIR}/boot.img of=/dev/sdc bs=4M status=progress conv=fsync
        DEPENDS disk_image
)

add_custom_target(test
        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu qemu64
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
      #  -device usb-kbd
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
        -s -S
        DEPENDS disk_image
)

add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning all build artifacts..."
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/splash.h
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/kernel/kversion.h
        COMMAND ${CMAKE_COMMAND} -E echo "Clean complete."
)