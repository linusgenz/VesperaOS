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
CC = gcc
LD = ld
CFLAGS = -ffreestanding -mno-red-zone -fshort-wchar -fno-exceptions -pedantic -g -fno-rtti  # -O0 optimierung nur für prod
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

$(KERNEL_ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Create boot.img for QEMU (FAT32 EFI image)
img: $(DISK_IMG)

$(DISK_IMG): bootloader $(KERNEL_ELF)
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

# Create ISO for VirtualBox (UEFI bootable)
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

flash: $(DISK_IMG)
	@echo "!!! WARNUNG: USB-Stick $(USB_DEV) wird komplett überschrieben !!!"
	#@read -p "Fortfahren? (y/N): " ans; \
	#if [ "$$ans" != "y" ]; then echo "Abgebrochen."; exit 1; fi

	# 1. Stick komplett löschen
	sudo wipefs -a $(USB_DEV)
	sudo dd if=/dev/zero of=$(USB_DEV) bs=1M count=10 status=progress conv=fsync

	# 2. GPT anlegen
	sudo parted $(USB_DEV) --script mklabel gpt

	# 3. Partition anlegen (Größe passend zu efi.img)
	IMG_SIZE_BYTES=$$(stat -c %s $(DISK_IMG)); \
	SECTOR_SIZE=512; \
	SECTORS=$$(((IMG_SIZE_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE)); \
	END_SECTOR=$$((2048 + SECTORS - 1)); \
	END_MB=$$(((END_SECTOR * SECTOR_SIZE) / 1024 / 1024)); \
	echo "Partition von 1MiB bis $${END_MB}MiB anlegen..."; \
	sudo parted $(USB_DEV) --script mkpart ESP fat32 1MiB $${END_MB}MiB; \
	sudo parted $(USB_DEV) --script set 1 boot on; \
	sudo parted $(USB_DEV) --script set 1 esp on

	# 4. efi.img in Partition schreiben
	sudo dd if=$(DISK_IMG) of=$(USB_DEV)1 bs=4M status=progress conv=fsync

	# 5. Partitionstabelle neu laden
	sudo partprobe $(USB_DEV)

	@echo "Fertig. USB-Stick $(USB_DEV) ist bereit zum Booten."

# QEMU Test (optional target)
test: $(DISK_IMG)
	qemu-system-x86_64 -drive file=$(DISK_IMG),format=raw -m 256m -machine q35 -enable-kvm -cpu host \
	-drive if=pflash,format=raw,unit=0,file="OVMF/OVMF_CODE-pure-efi.fd",readonly=on \
	-drive if=pflash,format=raw,unit=1,file="OVMF/OVMF_VARS-pure-efi.fd",readonly=on \
	-net none -device qemu-xhci,id=xhci -device usb-mouse

debug:
	qemu-system-x86_64 -drive file=$(DISK_IMG),format=raw -m 500m -machine q35 -enable-kvm -cpu host \
	-drive if=pflash,format=raw,unit=0,file="OVMF/OVMF_CODE-pure-efi.fd",readonly=on \
	-drive if=pflash,format=raw,unit=1,file="OVMF/OVMF_VARS-pure-efi.fd" \
	-net none -drive file=blank.img -device qemu-xhci,id=xhci -device usb-mouse -s -S -no-reboot -no-shutdown

clean:
	$(MAKE) -C gnu-efi/bootloader clean
	rm -f $(OBJS) $(KERNEL_ELF) $(DISK_IMG) $(ISO)
	rm -rf mnt isofiles
