#!/bin/bash

# Build script for Unix-like systems (Linux, macOS)

echo "Building CSS Checker C++..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
if ! cmake .. -DCMAKE_BUILD_TYPE=Release; then
    echo "CMake configuration failed. Please check your CMake installation."
    exit 1
fi

# Build the project
echo "Building project..."
if ! make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4); then
    echo "Build failed. Please check the error messages above."
    exit 1
fi

echo
echo "Build completed successfully!"
echo "Executable location: build/bin/css_checker"
echo

echo "Usage examples:"
echo "  ./css_checker --help"
echo "  ./css_checker --src your-project-directory"
echo "  ./css_checker --verbose --dry-run"
echo

# Make the script executable
chmod +x bin/css_checker 2>/dev/null || true