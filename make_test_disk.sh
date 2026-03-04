#!/usr/bin/env bash
set -euo pipefail

IMG_FILE="${1:-disk.img}"
IMG_SIZE_MB=64

MNT_DIR=$(mktemp -d)

cleanup() {
    sudo umount "$MNT_DIR" 2>/dev/null || true
    if [ -n "${LOOPDEV:-}" ]; then
        sudo losetup -d "$LOOPDEV" 2>/dev/null || true
    fi
    rm -rf "$MNT_DIR"
}
trap cleanup EXIT

echo "[+] Creating empty image (${IMG_SIZE_MB}MB)..."
rm -f "$IMG_FILE"
dd if=/dev/zero of="$IMG_FILE" bs=1M count="$IMG_SIZE_MB" status=progress

echo "[+] Creating GPT + FAT32 partition..."
parted --script "$IMG_FILE" mklabel gpt
parted --script "$IMG_FILE" mkpart primary fat32 1MiB 100%
parted --script "$IMG_FILE" set 1 msftdata on

LOOPDEV=$(sudo losetup -fP --show "$IMG_FILE")

echo "[+] Formatting FAT32..."
sudo mkfs.fat -F 32 -n "TESTDISK" "${LOOPDEV}p1"

sudo mount "${LOOPDEV}p1" "$MNT_DIR"

echo "[+] Creating test.txt..."
echo "Hello from inside disk.img!" | sudo tee "$MNT_DIR/test.txt" > /dev/null
echo "This is a FAT32 filesystem."  | sudo tee -a "$MNT_DIR/test.txt" > /dev/null

sync

echo "[+] Done. Image created: $IMG_FILE"