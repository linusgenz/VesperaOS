#!/usr/bin/env bash
set -euo pipefail

IMG_FILE="$1"
LIMINE_DIR="$2"
KERNEL_ELF="$3"
SRC_DIR="$4"

# Größen
IMG_SIZE_MB=128
EFI_SIZE_MB=64

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
#   /EFI/BOOT/BOOTX64.EFI   ← Limine EFI Binary
#   /limine.conf             ← Boot config
#   /kernel.elf              ← Kernel
# ────────────────────────────────────────────────────────────────

sudo mkdir -p "$EFI_MNT"
sudo mount "${LOOPDEV}p1" "$EFI_MNT"
sudo mkdir -p "$EFI_MNT/EFI/BOOT"

sudo cp "$SRC_DIR/assets/startup.nsh"    "$EFI_MNT/startup.nsh"
sudo cp "$LIMINE_DIR/BOOTX64.EFI"       "$EFI_MNT/EFI/BOOT/BOOTX64.EFI"
sudo cp "$SRC_DIR/limine.conf"          "$EFI_MNT/limine.conf"
sudo cp "$KERNEL_ELF"                   "$EFI_MNT/kernel.elf"

sudo cp "$LIMINE_DIR/limine-bios.sys"   "$EFI_MNT/" 2>/dev/null || true

sudo umount "$EFI_MNT"

# ────────────────────────────────────────────────────────────────
# RootFS Partition
# ────────────────────────────────────────────────────────────────


SYSROOT_DIR="$SRC_DIR/sysroot"

sudo mkdir -p "$ROOT_MNT"
sudo mount "${LOOPDEV}p2" "$ROOT_MNT"

sudo cp -r "$SYSROOT_DIR/bin"     "$ROOT_MNT/"
sudo cp -r "$SYSROOT_DIR/lib"     "$ROOT_MNT/"
sudo cp -r "$SYSROOT_DIR/etc"     "$ROOT_MNT/"
sudo cp -r "$SYSROOT_DIR/tmp"     "$ROOT_MNT/"
sudo cp -r "$SYSROOT_DIR/mnt"     "$ROOT_MNT/"
sudo cp -r "$SYSROOT_DIR/var"     "$ROOT_MNT/"

sudo cp "$SRC_DIR/assets/test.jpg" "$ROOT_MNT/"
sudo cp "$SRC_DIR/assets/CaskaydiaCoveNerdFontMono.ttf" "$ROOT_MNT/etc/fonts/"

sudo umount "$ROOT_MNT"

sudo losetup -d "$LOOPDEV"
LOOPDEV=""

echo "[make_disk] Disk image created successfully: $IMG_FILE"