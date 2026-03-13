#!/usr/bin/env bash
# flash.sh — Write VesperaOS boot image to a USB drive
# Usage: ./flash.sh [path/to/boot.img]

set -euo pipefail

# ────────────────────────────────────────────────────────────────
# Helpers
# ────────────────────────────────────────────────────────────────

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
success() { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
error()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
die()     { error "$*"; exit 1; }

# ────────────────────────────────────────────────────────────────
# Locate the disk image
# ────────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -ge 1 ]]; then
    IMAGE="$1"
else
    # Try to find boot.img relative to this script (project root)
    CANDIDATE="${SCRIPT_DIR}/cmake-build-debug/boot.img"
    if [[ -f "$CANDIDATE" ]]; then
        IMAGE="$CANDIDATE"
        info "Found disk image: ${IMAGE}"
    else
        die "No disk image specified and cmake-build-debug/boot.img not found.\n       Usage: $0 [path/to/boot.img]"
    fi
fi

[[ -f "$IMAGE" ]] || die "Disk image not found: $IMAGE"

# ────────────────────────────────────────────────────────────────
# Detect USB block devices
# ────────────────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}VesperaOS USB Flash Tool${RESET}"
echo "────────────────────────────────────────"
info "Scanning for USB block devices..."
echo ""

declare -a DEVICES
declare -a DESCRIPTIONS

MAX_SIZE=$((100 * 1024 * 1024 * 1024))

while IFS= read -r line; do
    DEV=$(echo "$line" | awk '{print $1}')
    SIZE_BYTES=$(echo "$line" | awk '{print $4}')

    # Skip drives larger than 100 GB
    if (( SIZE_BYTES > MAX_SIZE )); then
        continue
    fi

    SIZE=$(lsblk -dn -o SIZE "/dev/${DEV}")
    MODEL=$(cat "/sys/block/${DEV}/device/model" 2>/dev/null | xargs || echo "Unknown")

    SYSPATH=$(readlink -f "/sys/block/${DEV}" 2>/dev/null || true)
    if echo "$SYSPATH" | grep -q "usb"; then
        DEVICES+=("/dev/${DEV}")
        DESCRIPTIONS+=("${DEV}   ${SIZE}   ${MODEL}")
    fi
done < <(lsblk -bdno NAME,TYPE,HOTPLUG,SIZE | awk '$2=="disk" && $3=="1"')

if [[ ${#DEVICES[@]} -eq 0 ]]; then
    warn "No USB drives detected automatically."
    warn "Make sure your drive is plugged in, or specify it manually:"
    echo ""
    echo "  $0 cmake-build-debug/boot.img /dev/sdX"
    echo ""
    # Fallback: show all disks and let the user pick
    echo -e "${BOLD}All available block devices:${RESET}"
    lsblk -dno NAME,SIZE,MODEL | awk '{printf "  /dev/%-8s %s  %s\n", $1, $2, $3}'
    echo ""
    die "No USB device selected. Aborting."
fi

# ────────────────────────────────────────────────────────────────
# Interactive device selection
# ────────────────────────────────────────────────────────────────

echo -e "${BOLD}Detected USB drives:${RESET}"
echo ""
for i in "${!DEVICES[@]}"; do
    printf "  ${CYAN}[%d]${RESET}  %s\n" "$((i+1))" "${DESCRIPTIONS[$i]}"
done
echo ""

SELECTED=""
while [[ -z "$SELECTED" ]]; do
    read -rp "$(echo -e "${BOLD}Select drive [1-${#DEVICES[@]}]:${RESET} ")" CHOICE
    if [[ "$CHOICE" =~ ^[0-9]+$ ]] && (( CHOICE >= 1 && CHOICE <= ${#DEVICES[@]} )); then
        SELECTED="${DEVICES[$((CHOICE-1))]}"
        SELECTED_DESC="${DESCRIPTIONS[$((CHOICE-1))]}"
    else
        warn "Invalid selection. Enter a number between 1 and ${#DEVICES[@]}."
    fi
done

# ────────────────────────────────────────────────────────────────
# Safety confirmation
# ────────────────────────────────────────────────────────────────

echo ""
echo -e "${RED}${BOLD}⚠  WARNING — DATA LOSS${RESET}"
echo "────────────────────────────────────────"
echo -e "  Image : ${BOLD}${IMAGE}${RESET}"
echo -e "  Target: ${BOLD}${SELECTED}${RESET}  (${SELECTED_DESC})"
echo ""
echo -e "${RED}ALL DATA ON THIS DRIVE WILL BE PERMANENTLY ERASED.${RESET}"
echo ""
read -rp "$(echo -e "${BOLD}Type \"yes\" to confirm: ${RESET}")" CONFIRM

if [[ "$CONFIRM" != "yes" ]]; then
    info "Aborted. No data was written."
    exit 0
fi

# ────────────────────────────────────────────────────────────────
# Flash
# ────────────────────────────────────────────────────────────────

echo ""
info "Writing image to ${SELECTED} — this may take a while..."
echo ""

sudo dd \
    if="${IMAGE}" \
    of="${SELECTED}" \
    bs=4M \
    status=progress \
    conv=fsync

echo ""
success "Done! VesperaOS has been written to ${SELECTED}."
info "You can now safely remove the drive and boot from it."
echo ""