#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"

# Clean up existing build directory if it exists
if [ -d "build" ]; then
    echo "Cleaning existing build folder..."
    rm -rf build
fi
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Compiling..."
make -j$(nproc)

echo "Build complete! Output: build/mod.so"
