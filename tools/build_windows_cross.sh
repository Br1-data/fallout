#!/bin/sh
# Compila Dustfall.exe per Windows da Linux con MinGW-w64 (pacchetto "mingw-w64").
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD_DIR:-"$ROOT/build-windows"}
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake" "$@"
cmake --build "$BUILD" --parallel
mkdir -p "$ROOT/release"
cp "$BUILD/Dustfall.exe" "$ROOT/release/Dustfall.exe"
echo "Creato: $ROOT/release/Dustfall.exe"
