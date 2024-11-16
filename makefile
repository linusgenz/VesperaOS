BUILD_DIR = ./build
ASM_SRC = $(shell find ./ -name '*.asm' ! -path './gnu-efi/*')
KERNEL_SRC = $(shell find ./ -name '*.cpp*' ! -path './gnu-efi/*')

OBJS = $(KERNEL_SRC:.cpp=.o)
OBJS += $(ASM_SRC:.asm=_asm.o)

KERNEL_ELF = $(BUILD_DIR)/kernel.elf
BOOTLOADER_EFI = gnu-efi/bootloader/boot.efi
DISK_IMG = $(BUILD_DIR)/boot.img

LDS = linker.ld
ASM = nasm
CC = gcc
LD = ld
CFLAGS = -ffreestanding -mno-red-zone -fshort-wchar -fno-exceptions -pedantic -g -O0
ASMFLAGS = -f elf64
LDFLAGS = -T $(LDS) -Bsymbolic -nostdlib -g

ISO = $(BUILD_DIR)/LuminOS.iso
MKFS_FAT = mkfs.fat
EFI_DIR = mnt/EFI/BOOT
IMG_SIZE = 131072

# Make all target
all: bootloader $(ISO) clean

# Compile the bootloader using the separate makefile in gnu-efi/bootloader
bootloader:
	$(MAKE) -C gnu-efi/bootloader

./arch/x86_64/interrupts/interrupts.o: ./arch/x86_64/interrupts/interrupts.cpp
	$(CC) -mno-red-zone -mgeneral-regs-only -ffreestanding -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the asm files into _asm.o object files
%_asm.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Link all object files into the final ELF binary
$(KERNEL_ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Create the FAT32 EFI System Partition image
$(DISK_IMG): bootloader $(KERNEL_ELF)
	$(RM) $@
	dd if=/dev/zero of=$@ bs=512 count=$(IMG_SIZE)
	$(MKFS_FAT) -F 32 -n "LuminOS" $@
	mkdir -p $(EFI_DIR)
	sudo mount -o loop $@ mnt
	sudo mkdir -p $(EFI_DIR)
	sudo cp $(BOOTLOADER_EFI) $(EFI_DIR)/BOOTX64.EFI
	sudo cp $(KERNEL_ELF) $(EFI_DIR)/kernel.elf
	sudo cp build/zap-light16.psf mnt/zap-light16.psf
	sudo cp $(BUILD_DIR)/startup.nsh mnt/
	sudo umount mnt
	$(RM) -r mnt

# Create the ISO image with the FAT32 EFI partition
$(ISO): $(DISK_IMG)
	dd if=$(DISK_IMG) of=$(ISO) bs=4M

# Test the ISO image with QEMU
test: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 256m -machine q35 -enable-kvm -cpu host -drive if=pflash,format=raw,unit=0,file="OVMF/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="OVMF/OVMF_VARS-pure-efi.fd" -net none \
    -drive file=blank.img \
    -device qemu-xhci,id=xhci \
	-device usb-mouse \

# Clean up build artifacts
clean:
	$(MAKE) -C gnu-efi/bootloader clean
	rm -f $(OBJS) $(KERNEL_ELF) $(DISK_IMG)
	rm -f -r mnt
