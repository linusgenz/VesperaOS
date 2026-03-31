#!/usr/bin/env bash
# create_ext4_img.sh
# Builds a minimal ext4 test image

set -euo pipefail

IMG="${1:?Usage: $0 <output_image>}"
TMPDIR_LOCAL="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

printf 'hello from ext4\n'             > "$TMPDIR_LOCAL/hello.txt"
printf ''                              > "$TMPDIR_LOCAL/empty.txt"
printf 'nested content\n'             > "$TMPDIR_LOCAL/nested.txt"
printf 'another file\n'               > "$TMPDIR_LOCAL/another.txt"
printf 'deep leaf\n'                  > "$TMPDIR_LOCAL/leaf.txt"
printf 'truncation test content here\n' > "$TMPDIR_LOCAL/trunctest.txt"

python3 -c "
import sys
sys.stdout.buffer.write(bytes(i % 256 for i in range(8192)))
" > "$TMPDIR_LOCAL/binary.bin"

python3 -c "
import sys
sys.stdout.buffer.write(bytes((i * 7 + 13) % 256 for i in range(65536)))
" > "$TMPDIR_LOCAL/big.bin"

dd if=/dev/zero of="$IMG" bs=1M count=16 status=none
mkfs.ext4 -q \
    -L "vesp_test" \
    -O "^metadata_csum,^64bit" \
    -b 4096 \
    "$IMG"

debugfs -w "$IMG" <<EOF
mkdir subdir
mkdir deep
mkdir deep/level1
write $TMPDIR_LOCAL/hello.txt     hello.txt
write $TMPDIR_LOCAL/empty.txt     empty.txt
write $TMPDIR_LOCAL/binary.bin    binary.bin
write $TMPDIR_LOCAL/big.bin       big.bin
write $TMPDIR_LOCAL/trunctest.txt trunctest.txt
write $TMPDIR_LOCAL/nested.txt    subdir/nested.txt
write $TMPDIR_LOCAL/another.txt   subdir/another.txt
write $TMPDIR_LOCAL/leaf.txt      deep/level1/leaf.txt
EOF