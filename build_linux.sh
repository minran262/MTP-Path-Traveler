#!/usr/bin/env bash
# Build script for Linux hosts (including WSL2).
set -e

OUT="${1:-mtp_traveler}"

if ! command -v pkg-config >/dev/null 2>&1; then
    echo "pkg-config not found. Install libmtp-dev and libusb-1.0-0-dev first."
    exit 1
fi

echo "Building ${OUT}..."
gcc -O2 -Wall -Wextra \
    -o "${OUT}" \
    src/mtp_traveler.c \
    $(pkg-config --cflags --libs libmtp)

echo "Done: ${OUT}"