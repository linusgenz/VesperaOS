#!/usr/bin/env bash
set -euo pipefail

IMG_FILE="$1"
LIMINE_DIR="$2"      # Pfad zum Limine-Submodul (z.B. /src/limine)
KERNEL_ELF="$3"
SRC_DIR="$4"

# Größen
IMG_SIZE_MB=128
EFI_SIZE_MB=64

# Temporäre Mountpoints
MNT_DIR=$(mktemp -d)
EFI_MNT="$MNT_DIR/efi"
ROOT_MNT="$MNT_DIR/root"

cleanup() {
    sudo umount "$EFI_MNT"  2>/dev/null || true
    sudo umount "$ROOT_MNT" 2>/dev/null || true
    if [ -n "${LOOPDEV:-}" ]; then
        sudo losetup -d "$LOOPDEV" 2>/dev/null || true
    fi
    rm -rf "$MNT_DIR"
}
trap cleanup EXIT

echo "[make_disk] Creating disk image: $IMG_FILE"
rm -f "$IMG_FILE"
dd if=/dev/zero of="$IMG_FILE" bs=1M count=$IMG_SIZE_MB

# Partitionstabelle
parted --script "$IMG_FILE" mklabel gpt
parted --script "$IMG_FILE" mkpart EFI  fat32 1MiB     ${EFI_SIZE_MB}MiB
parted --script "$IMG_FILE" set 1 esp on
parted --script "$IMG_FILE" mkpart ROOT fat32 ${EFI_SIZE_MB}MiB 100%

LOOPDEV=$(sudo losetup -fP --show "$IMG_FILE")
echo "[make_disk] Using loop device: $LOOPDEV"

sudo mkfs.fat -F 32 -n "VesperaEFI"  "${LOOPDEV}p1"
sudo mkfs.fat -F 32 -n "VesperaRoot" "${LOOPDEV}p2"

# ────────────────────────────────────────────────────────────────
# EFI Partition
# Limine braucht:
#   /EFI/BOOT/BOOTX64.EFI   ← Limine EFI Binary
#   /limine.conf             ← Boot Konfiguration
#   /kernel.elf              ← Kernel
# ────────────────────────────────────────────────────────────────

sudo mkdir -p "$EFI_MNT"
sudo mount "${LOOPDEV}p1" "$EFI_MNT"
sudo mkdir -p "$EFI_MNT/EFI/BOOT"

sudo cp "$SRC_DIR/build/startup.nsh"    "$EFI_MNT/startup.nsh"
sudo cp "$LIMINE_DIR/BOOTX64.EFI"       "$EFI_MNT/EFI/BOOT/BOOTX64.EFI"
sudo cp "$SRC_DIR/limine.conf"          "$EFI_MNT/limine.conf"
sudo cp "$KERNEL_ELF"                   "$EFI_MNT/kernel.elf"

# Limine BIOS Support (optional bei reinem UEFI, schadet nicht)
sudo cp "$LIMINE_DIR/limine-bios.sys"   "$EFI_MNT/" 2>/dev/null || true

sudo umount "$EFI_MNT"

# ────────────────────────────────────────────────────────────────
# RootFS Partition
# ────────────────────────────────────────────────────────────────

sudo mkdir -p "$ROOT_MNT"
sudo mount "${LOOPDEV}p2" "$ROOT_MNT"

sudo mkdir -p \
    "$ROOT_MNT/bin" \
    "$ROOT_MNT/lib" \
    "$ROOT_MNT/etc" \
    "$ROOT_MNT/tmp" \
    "$ROOT_MNT/mnt" \
    "$ROOT_MNT/var" \
    "$ROOT_MNT/var/log"

sudo cp "$SRC_DIR/userspace/bin/shell"   "$ROOT_MNT/bin/shell"
sudo cp "$SRC_DIR/userspace/bin/lsusb"   "$ROOT_MNT/bin/lsusb"
sudo cp "$SRC_DIR/userspace/bin/memstat" "$ROOT_MNT/bin/memstat"
sudo cp "$SRC_DIR/userspace/bin/logd"    "$ROOT_MNT/bin/logd"
sudo cp "$SRC_DIR/userspace/bin/uptime"  "$ROOT_MNT/bin/uptime"
sudo cp "$SRC_DIR/build/test.jpg"        "$ROOT_MNT/"

sudo umount "$ROOT_MNT"

# ────────────────────────────────────────────────────────────────
# Limine BIOS MBR (für BIOS-Boot, bei UEFI-only optional)
# ────────────────────────────────────────────────────────────────

if [ -f "$LIMINE_DIR/limine" ]; then
    sudo "$LIMINE_DIR/limine" bios-install "$IMG_FILE" 2>/dev/null || true
    echo "[make_disk] Limine BIOS MBR installed"
fi

sudo losetup -d "$LOOPDEV"
LOOPDEV=""

echo "[make_disk] Disk image created successfully: $IMG_FILE"