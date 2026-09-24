#!/usr/bin/env bash
# Build script for MSYS2 MINGW64.
# Assumes libmtp has been built locally at $HOME/libmtp-1.1.21.
set -e

OUT="${1:-mtp_traveler.exe}"
LIBMTP_DIR="${LIBMTP_DIR:-$HOME/libmtp-1.1.21}"

if [ ! -d "${LIBMTP_DIR}/src" ]; then
    echo "libmtp source tree not found at ${LIBMTP_DIR}"
    echo "Set LIBMTP_DIR to the correct path, e.g.:"
    echo "  LIBMTP_DIR=/path/to/libmtp ./build_msys2.sh"
    exit 1
fi

echo "Building ${OUT}..."
g++ -O2 -Wall \
    -o "${OUT}" \
    src/mtp_traveler.c \
    -I/mingw64/include/libusb-1.0 \
    -I"${LIBMTP_DIR}/src" \
    -L"${LIBMTP_DIR}/src/.libs" \
    -lmtp -lusb-1.0 -liconv -lws2_32 \
    -static -static-libgcc -static-libstdc++

echo "Done: ${OUT}"