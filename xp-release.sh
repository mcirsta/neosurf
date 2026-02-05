#!/bin/bash
set -e

echo "Configuring Windows XP Release build..."
# Note: Ensure you are running this with a 32-bit (i686) toolchain environment!
cmake -S . -B build-xp -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DWISP_TARGET_WINXP=ON

echo "Building..."
ninja -C build-xp

echo "Creating Installer..."
cd build-xp
cpack -G NSIS
echo "Installer located in: build-xp/"
