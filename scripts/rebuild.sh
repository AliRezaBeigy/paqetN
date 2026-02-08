#!/bin/bash

# Rebuild script for paqetN
# This script cleans and rebuilds the project

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}paqetN Rebuild Script${NC}"
echo "===================="

# Detect build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory not found, creating...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# Clean build directory (but keep it for faster rebuild)
echo -e "${YELLOW}Cleaning build directory...${NC}"
cd "$BUILD_DIR"
rm -rf CMakeFiles CMakeCache.txt *.cmake paqetN/ apppaqetN* tst_paqetn* .qt/

# Detect Qt path
if [ -z "$CMAKE_PREFIX_PATH" ]; then
    echo -e "${RED}Warning: CMAKE_PREFIX_PATH not set${NC}"
    echo "Please set it to your Qt installation, e.g.:"
    echo "  export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64"
    echo ""
    echo "Or on Windows (Git Bash/MSYS2):"
    echo "  export CMAKE_PREFIX_PATH=/c/Qt/6.8/mingw1310_64"
    echo ""
    read -p "Press Enter to continue anyway or Ctrl+C to exit..."
fi

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cd ..
cmake -B "$BUILD_DIR"

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build "$BUILD_DIR"

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"
echo ""
echo "Run the application with:"
echo "  ./$BUILD_DIR/apppaqetN"
echo ""
echo "Run tests with:"
echo "  cd $BUILD_DIR && ctest -V"
