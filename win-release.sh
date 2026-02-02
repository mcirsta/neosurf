#!/bin/bash
set -e

echo "Configuring Release build..."
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc

echo "Building..."
ninja -C build-release

echo "Creating Installer..."
cd build-release
cpack -G NSIS
echo "Installer located in: build-release/"
