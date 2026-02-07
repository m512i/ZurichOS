#!/bin/bash
# ZurichOS Cross-Compiler Toolchain Build Script
# Builds i686-elf cross-compiler for bare-metal development

set -e

# Configuration
export TARGET=i686-elf
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

# Versions
BINUTILS_VERSION=2.40
GCC_VERSION=13.2.0

# Source directories
SRC_DIR="$HOME/src"
BUILD_DIR="$HOME/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== ZurichOS Cross-Compiler Build Script ===${NC}"
echo "Target: $TARGET"
echo "Prefix: $PREFIX"
echo ""

# Create directories
mkdir -p "$SRC_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$PREFIX"

# Check for required tools
echo -e "${YELLOW}Checking dependencies...${NC}"
for tool in gcc g++ make bison flex texinfo wget tar; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        echo "Please install: $tool"
        exit 1
    fi
done
echo -e "${GREEN}All dependencies found.${NC}"
echo ""

# Download sources
cd "$SRC_DIR"

if [ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]; then
    echo -e "${YELLOW}Downloading Binutils $BINUTILS_VERSION...${NC}"
    wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi

if [ ! -f "gcc-$GCC_VERSION.tar.xz" ]; then
    echo -e "${YELLOW}Downloading GCC $GCC_VERSION...${NC}"
    wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi

# Extract sources
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    echo -e "${YELLOW}Extracting Binutils...${NC}"
    tar xf "binutils-$BINUTILS_VERSION.tar.xz"
fi

if [ ! -d "gcc-$GCC_VERSION" ]; then
    echo -e "${YELLOW}Extracting GCC...${NC}"
    tar xf "gcc-$GCC_VERSION.tar.xz"
fi

# Build Binutils
echo -e "${GREEN}=== Building Binutils ===${NC}"
mkdir -p "$BUILD_DIR/binutils"
cd "$BUILD_DIR/binutils"

"$SRC_DIR/binutils-$BINUTILS_VERSION/configure" \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make -j$(nproc)
make install

echo -e "${GREEN}Binutils installed successfully.${NC}"
echo ""

# Build GCC
echo -e "${GREEN}=== Building GCC ===${NC}"

# Download GCC prerequisites
cd "$SRC_DIR/gcc-$GCC_VERSION"
if [ ! -d "mpfr" ]; then
    ./contrib/download_prerequisites
fi

mkdir -p "$BUILD_DIR/gcc"
cd "$BUILD_DIR/gcc"

"$SRC_DIR/gcc-$GCC_VERSION/configure" \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers

make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

echo -e "${GREEN}GCC installed successfully.${NC}"
echo ""

# Verify installation
echo -e "${GREEN}=== Verification ===${NC}"
echo "Binutils:"
"$PREFIX/bin/$TARGET-as" --version | head -1
"$PREFIX/bin/$TARGET-ld" --version | head -1

echo ""
echo "GCC:"
"$PREFIX/bin/$TARGET-gcc" --version | head -1

echo ""
echo -e "${GREEN}=== Installation Complete ===${NC}"
echo ""
echo "Add the following to your shell profile (.bashrc, .zshrc, etc.):"
echo ""
echo "  export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
echo ""
echo "Then run: source ~/.bashrc (or restart your terminal)"
echo ""
echo "To build ZurichOS, run 'make' in the project directory."
