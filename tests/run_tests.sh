#!/usr/bin/env bash

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
TEST_IMG="$SCRIPT_DIR/test.img"

# --- Argument parsing ---
NO_BUILD=0
TARGET=""
SUITE=""

for arg in "$@"; do
    case "$arg" in
        --no-build) NO_BUILD=1 ;;
        fat32|heap|vector) TARGET="$arg" ;;
        *)          SUITE="$arg" ;;
    esac
done

# --- Build ---
if [ "$NO_BUILD" -eq 0 ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Build"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    mkdir -p "$BUILD_DIR"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -Wno-dev > /dev/null
    cmake --build "$BUILD_DIR" -- -j"$(nproc)"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Create test image"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

rm -f "$TEST_IMG"

dd if=/dev/zero of="$TEST_IMG" bs=512 count=70000 status=none
mkfs.fat -F 32 -S 512 "$TEST_IMG"

echo "Created fresh FAT32 image: $TEST_IMG"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
cd "$SCRIPT_DIR"

run_binary() {
    local bin="$BUILD_DIR/$1"
    local suite="$2"
    if [ -n "$suite" ]; then
        "$bin" "$suite"
    else
        "$bin"
    fi
}

case "$TARGET" in
    fat32)  run_binary fat32_tests  "$SUITE" ;;
    heap)   run_binary heap_tests   "$SUITE" ;;
    vector) run_binary vector_tests "$SUITE" ;;
    *)
        run_binary fat32_tests  "$SUITE"
        echo ""
        run_binary heap_tests   "$SUITE"
        echo ""
        run_binary vector_tests "$SUITE"
        ;;
esac