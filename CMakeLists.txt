cmake_minimum_required(VERSION 4.0.2)
project(VesperaOS C CXX ASM_NASM)

enable_language(ASM_NASM)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

set(VESPERA_UAPI_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/uapi)

# Subdirectories
add_subdirectory(kernel)
add_subdirectory(userspace)

# ────────────────────────────────────────────────────────────────
# Limine Bootloader
# ────────────────────────────────────────────────────────────────

set(LIMINE_DIR ${CMAKE_SOURCE_DIR}/limine)

add_custom_target(bootloader
        COMMAND make -C ${LIMINE_DIR}
        COMMENT "Building Limine bootloader"
)

# ────────────────────────────────────────────────────────────────
# Convenience aliases
# ────────────────────────────────────────────────────────────────

# Build kernel only (kernel.elf is already defined in kernel/CMakeLists.txt,
# this target just gives it a short top-level name)
add_custom_target(build-kernel
        DEPENDS kernel.elf
        COMMENT "Building kernel only"
)

# Build userspace only
add_custom_target(build-userspace
        DEPENDS elfs
        COMMENT "Building userspace only"
)

# ────────────────────────────────────────────────────────────────
# Disk Image
# ────────────────────────────────────────────────────────────────

add_custom_target(disk_image
        COMMAND ${CMAKE_COMMAND} -E echo "Creating partitioned disk image..."
        COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/make_disk.sh
        ${CMAKE_BINARY_DIR}/boot.img
        ${LIMINE_DIR}
        ${CMAKE_BINARY_DIR}/kernel/kernel.elf
        ${CMAKE_SOURCE_DIR}
        DEPENDS bootloader version kernel.elf elfs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ────────────────────────────────────────────────────────────────
# img (write to device)
#
# Usage:
#   make img                        # uses flash.sh — interactive USB picker
#   make img DEVICE=/dev/sdX        # write directly to the given device
# ────────────────────────────────────────────────────────────────

if(DEFINED DEVICE)
    add_custom_target(img
            COMMAND ${CMAKE_COMMAND} -E echo "Writing to ${DEVICE}..."
            COMMAND sudo dd if=${CMAKE_BINARY_DIR}/boot.img of=${DEVICE} bs=4M status=progress conv=fsync
            DEPENDS disk_image
            COMMENT "Flashing boot image to ${DEVICE}"
    )
else()
    add_custom_target(img
            COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/flash.sh ${CMAKE_BINARY_DIR}/boot.img
            DEPENDS disk_image
            COMMENT "Launching interactive USB flash tool"
    )
endif()

# ────────────────────────────────────────────────────────────────
# QEMU test target
# ────────────────────────────────────────────────────────────────

add_custom_target(test
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/OVMF/OVMF_VARS-pure-efi.fd
        ${CMAKE_BINARY_DIR}/OVMF_VARS.fd

        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu host
        -vga std
        -drive if=pflash,format=raw,unit=0,readonly=on,file=${CMAKE_SOURCE_DIR}/OVMF/OVMF_CODE-pure-efi.fd
        -drive if=pflash,format=raw,unit=1,file=${CMAKE_BINARY_DIR}/OVMF_VARS.fd
        -net none
        -monitor stdio
        -device qemu-xhci,id=xhci1
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw
        -drive id=fat32drive,file=${CMAKE_SOURCE_DIR}/disk.img,if=none,format=raw
        -device usb-storage,drive=usbstick
        -device nvme,id=nvme0,serial=deadbeef
        -device nvme-ns,drive=fat32drive,bus=nvme0
        #  -device ahci,id=ahci0
        # -device ide-hd,drive=usbstick2,bus=ahci0.0
        -device usb-kbd
        -trace *msi*
        -D qemu.log
        -no-reboot
        -no-shutdown
        DEPENDS disk_image
)

# ────────────────────────────────────────────────────────────────
# QEMU run target (no rebuild — just launch what's already built)
# ────────────────────────────────────────────────────────────────

add_custom_target(run
        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu qemu64
        -vga std
        -drive if=pflash,format=raw,unit=0,file=${CMAKE_SOURCE_DIR}/OVMF/OVMF.fd
        -net none
        -monitor stdio
        -device qemu-xhci,id=xhci1
        -drive id=usbstick,file=${CMAKE_BINARY_DIR}/boot.img,if=none,format=raw
        -drive id=fat32drive,file=${CMAKE_SOURCE_DIR}/disk.img,if=none,format=raw
        -device usb-storage,drive=usbstick
        -device nvme,id=nvme0,serial=deadbeef
        -device nvme-ns,drive=fat32drive,bus=nvme0
        -device usb-kbd
        -trace *msi*
        -D qemu.log
        -no-reboot
        -no-shutdown
        COMMENT "Launching QEMU (no rebuild)"
)

# ────────────────────────────────────────────────────────────────
# QEMU debug target
# ────────────────────────────────────────────────────────────────

add_custom_target(debug
        COMMAND qemu-system-x86_64
        -m 4G
        -smp 8
        -machine q35
        -enable-kvm
        -cpu host
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
        -D qemu.log
        -no-reboot
        -no-shutdown
        -s -S
        DEPENDS disk_image
)

# ────────────────────────────────────────────────────────────────
# Clean
# ────────────────────────────────────────────────────────────────

file(GLOB USERSPACE_BINS
        "${CMAKE_SOURCE_DIR}/userspace/bin/*"
)
set(FINAL_BINS "")
foreach(f ${USERSPACE_BINS})
    get_filename_component(ext ${f} EXT)
    if(NOT ext STREQUAL ".c" AND NOT ext STREQUAL ".h")
        list(APPEND FINAL_BINS ${f})
    endif()
endforeach()

add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning all build artifacts..."
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/splash.h
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/kernel/kversion.h
        COMMAND ${CMAKE_COMMAND} -E rm -f ${FINAL_BINS}
        COMMAND ${CMAKE_COMMAND} -E echo "Clean complete."
)