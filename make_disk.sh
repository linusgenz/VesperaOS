#!/usr/bin/env bash
set -euo pipefail

IMG_FILE="$1"
BOOTLOADER_EFI="$2"
KERNEL_ELF="$3"
SRC_DIR="$4"

# Größen
IMG_SIZE_MB=256
EFI_SIZE_MB=128

# Temporäre Mountpoints
MNT_DIR=$(mktemp -d)
EFI_MNT="$MNT_DIR/efi"
ROOT_MNT="$MNT_DIR/root"

echo "[make_disk] Creating disk image: $IMG_FILE"
rm -f "$IMG_FILE"
dd if=/dev/zero of="$IMG_FILE" bs=1M count=$IMG_SIZE_MB

# Partitionstabelle anlegen
parted --script "$IMG_FILE" mklabel gpt
parted --script "$IMG_FILE" mkpart EFI fat32 1MiB ${EFI_SIZE_MB}MiB
parted --script "$IMG_FILE" set 1 esp on
parted --script "$IMG_FILE" mkpart ROOT fat32 ${EFI_SIZE_MB}MiB 100%

# Loopdevice setzen
LOOPDEV=$(sudo losetup -fP --show "$IMG_FILE")
echo "[make_disk] Using loop device: $LOOPDEV"

# Partitionen formatieren
sudo mkfs.fat -F 32 -n "VesperaEFI" ${LOOPDEV}p1
sudo mkfs.fat -F 32 -n "VesperaRoot" ${LOOPDEV}p2

# EFI Partition mounten und Dateien kopieren
sudo mkdir -p "$EFI_MNT"
sudo mount ${LOOPDEV}p1 "$EFI_MNT"
sudo mkdir -p "$EFI_MNT/EFI/BOOT"
sudo cp "$BOOTLOADER_EFI" "$EFI_MNT/EFI/BOOT/BOOTX64.EFI"
sudo cp "$KERNEL_ELF" "$EFI_MNT/kernel.elf"
sudo cp "$SRC_DIR/build/startup.nsh" "$EFI_MNT/"
sudo cp "$SRC_DIR/build/zap-light16.psf" "$EFI_MNT/"
sudo cp "$SRC_DIR/build/zap-vga16.psf" "$EFI_MNT/"
sudo cp "$SRC_DIR/build/zap-light32.psf" "$EFI_MNT/"
sudo cp "$SRC_DIR/build/zap-light24.psf" "$EFI_MNT/"
sudo umount "$EFI_MNT"

# RootFS Partition mounten und Dateien kopieren
sudo mkdir -p "$ROOT_MNT"
sudo mount ${LOOPDEV}p2 "$ROOT_MNT"
sudo mkdir -p "$ROOT_MNT/bin" "$ROOT_MNT/lib" "$ROOT_MNT/etc" "$ROOT_MNT/tmp" "$ROOT_MNT/mnt"
sudo cp "$SRC_DIR/userspace/bin/shell.elf" "$ROOT_MNT/bin/shell.elf"
sudo cp "$SRC_DIR/userspace/bin/lsusb" "$ROOT_MNT/bin/lsusb"

sudo umount "$ROOT_MNT"

# Loopdevice trennen
sudo losetup -d "$LOOPDEV"
rm -rf "$MNT_DIR"

echo "[make_disk] Disk image created successfully: $IMG_FILE"
