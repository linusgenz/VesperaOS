#!/usr/bin/env bash
# create_ext4_img.sh
# Builds a minimal ext4 test image

set -euo pipefail

IMG="${1:?Usage: $0 <output_image>}"
TMPDIR_LOCAL="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

HELLO="$TMPDIR_LOCAL/hello.txt"
NESTED="$TMPDIR_LOCAL/nested.txt"

printf 'hello from ext4\n' > "$HELLO"
printf 'nested content\n'  > "$NESTED"


dd if=/dev/zero of="$IMG" bs=1M count=4 status=none
mkfs.ext4 -q \
    -L "vesp_test" \
    -O "^metadata_csum,^64bit" \
    -b 4096 \
    "$IMG"


debugfs -w "$IMG" <<EOF
mkdir subdir
write $HELLO hello.txt
write $NESTED subdir/nested.txt
EOF