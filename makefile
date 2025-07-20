BUILD_DIR = ./build
ASM_SRC = $(shell find ./ -name '*.asm' ! -path './gnu-efi/*')
KERNEL_SRC = $(shell find ./ -name '*.cpp*' ! -path './gnu-efi/*')

OBJS = $(KERNEL_SRC:.cpp=.o)
OBJS += $(ASM_SRC:.asm=_asm.o)

KERNEL_ELF = $(BUILD_DIR)/kernel.elf
BOOTLOADER_EFI = gnu-efi/bootloader/boot.efi
DISK_IMG = $(BUILD_DIR)/boot.img
ISO = $(BUILD_DIR)/LuminOS.iso
USB_DEV := /dev/sdc

LDS = linker.ld
ASM = nasm
CC = g++
LD = ld
CFLAGS = -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -pedantic -g -fno-rtti   # -O0 optimierung nur für prod
ASMFLAGS = -f elf64
LDFLAGS = -T $(LDS) -Bsymbolic -nostdlib -g

MKFS_FAT = mkfs.fat
EFI_DIR = mnt/EFI/BOOT
IMG_SIZE = 131072

all: bootloader $(DISK_IMG) $(ISO)

bootloader:
	$(MAKE) -C gnu-efi/bootloader

./arch/x86_64/interrupts/interrupts.o: ./arch/x86_64/interrupts/interrupts.cpp
	$(CC) -mno-red-zone -mgeneral-regs-only -ffreestanding -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%_asm.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

#kernel/cpu/ap_trampoline.bin: kernel/cpu/ap_trampoline.asm
#	$(ASM) -f bin $< -o $@

#kernel/cpu/ap_trampoline.h: kernel/cpu/ap_trampoline.bin
#	xxd -i $< > $@

$(KERNEL_ELF): $(OBJS) #kernel/cpu/ap_trampoline.h
	$(LD) $(LDFLAGS) -o $@ $(OBJS)



version:
	bash generate_version_header.sh

img: $(DISK_IMG)

$(DISK_IMG): bootloader version $(KERNEL_ELF)
	$(RM) $@
	dd if=/dev/zero of=$@ bs=512 count=$(IMG_SIZE)
	$(MKFS_FAT) -F 32 -n "LuminOS" $@
	mkdir -p $(EFI_DIR)
	sudo mount -o loop $@ mnt
	sudo mkdir -p $(EFI_DIR)
	sudo cp build/t.txt mnt/t.txt
	sudo cp $(BOOTLOADER_EFI) $(EFI_DIR)/BOOTX64.EFI
	sudo cp $(KERNEL_ELF) $(EFI_DIR)/kernel.elf
	sudo cp build/zap-light16.psf mnt/zap-light16.psf
	sudo cp $(BUILD_DIR)/startup.nsh mnt/
	sudo umount mnt
	$(RM) -r mnt

	sudo dd if=./build/boot.img of=/dev/sdc bs=4M status=progress conv=fsync

iso: $(ISO)

$(ISO): bootloader $(KERNEL_ELF)
	mkdir -p build/esp/EFI/BOOT
	cp $(BOOTLOADER_EFI) build/esp/EFI/BOOT/BOOTX64.EFI
	cp $(KERNEL_ELF) build/esp/EFI/BOOT/kernel.elf
	cp build/zap-light16.psf build/esp/
	cp $(BUILD_DIR)/startup.nsh build/esp/

	truncate -s 64M build/efi.img
	$(MKFS_FAT) -F 32 build/efi.img
	mmd -i build/efi.img ::/EFI
	mmd -i build/efi.img ::/EFI/BOOT
	mcopy -i build/efi.img build/esp/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/
	mcopy -i build/efi.img build/esp/EFI/BOOT/kernel.elf ::/EFI/BOOT/
	mcopy -i build/efi.img build/esp/zap-light16.psf ::/
	mcopy -i build/efi.img build/esp/startup.nsh ::/

	mkdir -p build/iso_root
	cp build/efi.img build/iso_root/

	xorriso -as mkisofs \
		-iso-level 3 \
		-V "UEFI_BOOT" \
		-o $(ISO) \
		-e efi.img \
		-no-emul-boot \
		-isohybrid-gpt-basdat \
		build/iso_root

	rm -rf build/esp build/iso_root

# QEMU Test (optional target)
test: $(DISK_IMG)
	qemu-system-x86_64 -drive file=$(DISK_IMG),if=none,id=host0,format=raw \
	-smp cores=8 \
	-m 4G \
	-machine q35 \
	-enable-kvm \
	-cpu host \
	-drive if=pflash,format=raw,unit=0,file="OVMF/OVMF_CODE-pure-efi.fd",readonly=on \
	-drive if=pflash,format=raw,unit=1,file="OVMF/OVMF_VARS-pure-efi.fd",readonly=on \
	-net none \
	-device qemu-xhci,id=xhci \
	-device usb-mouse \
	-device nvme,drive=host0,serial=deadbeef \
	-monitor stdio \

#-d int,guest_errors,cpu_reset \

debug:
	qemu-system-x86_64 \
	  -m 500M \
	  -machine q35 \
	  -enable-kvm \
	  -cpu host \
	  -smp cores=8 \
	  -drive if=pflash,format=raw,unit=0,file="OVMF/OVMF_CODE-pure-efi.fd",readonly=on \
	  -drive if=pflash,format=raw,unit=1,file="OVMF/OVMF_VARS-pure-efi.fd" \
	  -drive file=$(DISK_IMG),format=raw \
	  -drive file=nvme.img,if=none,id=nvme0,format=raw \
	  -device pcie-root-port,id=rp0,port=0x10,chassis=1 \
	  -device nvme,drive=nvme0,serial=deadbeef,bus=rp0 \
	  -net none \
	  -device qemu-xhci,id=xhci \
	  -device usb-mouse \
	  -s -S \
	  -no-reboot \
	  -no-shutdown


clean:
	$(MAKE) -C gnu-efi/bootloader clean
	rm -f $(OBJS) $(KERNEL_ELF) $(DISK_IMG) $(ISO)
	rm -f ./kernel/cpu/ap_trampoline.bin
	rm -f ./kernel/cpu/ap_trampoline.h
	rm -f -rf mnt isofiles
