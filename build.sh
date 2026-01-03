#!/bin/bash

# *****************************************************************************
# * Copyright (C) 2023 MX Authors
# *
# * Authors: Adrian <adrian@mxlinux.org>
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of xdelta3-gui.
# *
# * xdelta3-gui is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * xdelta3-gui is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with xdelta3-gui.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************/

set -e

# Default values
BUILD_DIR="build"
BUILD_TYPE="Release"
USE_CLANG=false
CLEAN=false
DEBIAN_BUILD=false
ARCH_BUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clang)
            USE_CLANG=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --debian)
            DEBIAN_BUILD=true
            shift
            ;;
        --arch)
            ARCH_BUILD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -c, --clang     Use clang compiler"
            echo "  --clean         Clean build directory before building"
            echo "  --debian        Build Debian package"
            echo "  --arch          Build Arch Linux tar.zst package"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Build Debian package
if [ "$DEBIAN_BUILD" = true ]; then
    echo "Building Debian package..."
    debuild -us -uc

    echo "Creating debs directory and moving debian artifacts..."
    mkdir -p debs
    mv ../*.deb debs/ 2>/dev/null || true
    mv ../*.changes debs/ 2>/dev/null || true  
    mv ../*.dsc debs/ 2>/dev/null || true
    mv ../*.tar.* debs/ 2>/dev/null || true
    mv ../*.buildinfo debs/ 2>/dev/null || true
    mv ../*build* debs/ 2>/dev/null || true

    echo "Cleaning build directory and debian artifacts..."
    rm -rf "$BUILD_DIR"
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/xdelta3-gui/ obj-*/
    rm -f translations/*.qm version.h
    rm -f ../*build* ../*.buildinfo 2>/dev/null || true

    echo "Debian package build completed!"
    echo "Debian artifacts moved to debs/ directory"
    exit 0
fi

# Get version from debian/changelog for packaging
get_version() {
    if command -v dpkg-parsechangelog >/dev/null 2>&1; then
        dpkg-parsechangelog -SVersion 2>/dev/null || true
        return
    fi
    if [ -f debian/changelog ]; then
        head -n 1 debian/changelog | awk '{print $2}' | tr -d '()'
        return
    fi
    echo "0.0.0"
}

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory and debian artifacts..."
    rm -rf "$BUILD_DIR"
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/xdelta3-gui/ obj-*/
    rm -f translations/*.qm version.h
    rm -f ../*build* ../*.buildinfo 2>/dev/null || true
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure CMake with Ninja
echo "Configuring CMake with Ninja generator..."
CMAKE_ARGS=(
    -G Ninja
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [ "$USE_CLANG" = true ]; then
    CMAKE_ARGS+=(-DUSE_CLANG=ON)
    echo "Using clang compiler"
fi

cmake "${CMAKE_ARGS[@]}"

# Build the project
echo "Building project with Ninja..."
cmake --build "$BUILD_DIR" --parallel

echo "Build completed successfully!"
echo "Executable: $BUILD_DIR/xdelta3-gui"

# Build Arch Linux package if requested
if [ "$ARCH_BUILD" = true ]; then
    echo "Building Arch Linux tar.zst package..."
    VERSION="$(get_version)"
    STAGING_DIR="$BUILD_DIR/arch-root"
    PKG_NAME="xdelta3-gui-${VERSION}-1-arch.tar.zst"

    rm -rf "$STAGING_DIR"
    mkdir -p "$STAGING_DIR"

    DESTDIR="$STAGING_DIR" cmake --install "$BUILD_DIR" --prefix /usr --strip

    mkdir -p "$STAGING_DIR/usr/share/applications"
    mkdir -p "$STAGING_DIR/usr/share/pixmaps"
    mkdir -p "$STAGING_DIR/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "$STAGING_DIR/usr/share/mime/packages"
    mkdir -p "$STAGING_DIR/usr/share/xdelta3-gui/locale"

    install -m 0644 xdelta3-gui.desktop "$STAGING_DIR/usr/share/applications/"
    install -m 0644 xdelta3-gui.png "$STAGING_DIR/usr/share/pixmaps/"
    install -m 0644 xdelta3-gui.svg "$STAGING_DIR/usr/share/icons/hicolor/scalable/apps/"
    install -m 0644 x-xdelta3.xml "$STAGING_DIR/usr/share/mime/packages/"

    while IFS= read -r -d '' qm_file; do
        install -m 0644 "$qm_file" "$STAGING_DIR/usr/share/xdelta3-gui/locale/"
    done < <(find "$BUILD_DIR" -type f -name "*.qm" -print0 2>/dev/null)

    tar --zstd -C "$STAGING_DIR" -cf "$BUILD_DIR/$PKG_NAME" .
    echo "Arch package created: $BUILD_DIR/$PKG_NAME"
fi
