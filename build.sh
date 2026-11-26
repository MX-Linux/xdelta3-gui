#!/bin/bash

# *****************************************************************************
# * Copyright (C) 2023-2026 MX Authors
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
            BUILD_DIR="build-clang"
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
            echo "  --arch          Build an Arch Linux package with makepkg (always Release)"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Debian source package name. The parent directory is shared with other
# projects, so only files named after this package may be moved or removed there.
get_source_name() {
    sed -n 's/^Source:[[:space:]]*//; s/[[:space:]]*$//p' debian/control | head -n 1
}

# Build Debian package
if [ "$DEBIAN_BUILD" = true ]; then
    SOURCE_NAME="$(get_source_name)"
    PKG_VERSION="$(dpkg-parsechangelog -SVersion)"
    PKG_VERSION="${PKG_VERSION#*:}" # file names omit the epoch

    echo "Building Debian package..."
    debuild -us -uc

    echo "Creating debs directory and moving debian artifacts..."
    mkdir -p debs
    shopt -s nullglob
    # The .changes manifest lists every file this build produced (.debs, .dsc,
    # source tarball, .buildinfo).
    for changes in ../"${SOURCE_NAME}_${PKG_VERSION}"_*.changes; do
        awk '/^Files:/ { f = 1; next } /^[^ ]/ { f = 0 } f && NF { print $NF }' "$changes" |
            while read -r file; do
                if [ -f "../$file" ]; then mv "../$file" debs/; fi
            done
        mv "$changes" debs/
    done
    for log in ../"${SOURCE_NAME}_${PKG_VERSION}"_*.build; do
        mv "$log" debs/
    done
    shopt -u nullglob

    echo "Cleaning build directory and debian artifacts..."
    rm -rf build # debuild ignores --clang; keep any build-clang/ tree
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/xdelta3-gui/ obj-*/
    rm -f translations/*.qm version.h

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

# Build an installable Arch Linux package with makepkg, from the current
# working tree, using arch/PKGBUILD
if [ "$ARCH_BUILD" = true ]; then
    if ! command -v makepkg >/dev/null 2>&1; then
        echo "Error: makepkg not found; --arch needs pacman's makepkg."
        exit 1
    fi
    if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "Error: --arch packs the git working tree; run it from a git checkout."
        exit 1
    fi
    PKG_DIR="build-arch"
    VERSION="$(get_version)"
    VERSION="${VERSION#*:}"    # no epoch in file names
    PKGVER="${VERSION//-/_}"   # pkgver may not contain '-'
    echo "Building Arch Linux package $PKGVER..."

    rm -rf "$PKG_DIR"
    mkdir -p "$PKG_DIR"
    # arch/PKGBUILD consumes a source tarball that unpacks to src/; build it
    # from the tracked files as they are now (uncommitted edits included).
    # Untracked files stay out so local build trees and notes never ship.
    git ls-files -z -- . ':!debs' |
        while IFS= read -r -d '' f; do [ -e "$f" ] && printf '%s\0' "$f"; done |
        tar --null -T - --transform 's,^,src/,' -cJf "$PKG_DIR/xdelta3-gui_${PKGVER}.tar.xz"
    sed "s/^pkgver=.*/pkgver=${PKGVER}/" arch/PKGBUILD > "$PKG_DIR/PKGBUILD"

    MAKEPKG_ARGS=(-f)
    if [ ! -f /etc/arch-release ]; then
        echo "Warning: not on Arch Linux; skipping dependency checks. The package is"
        echo "built against this system's libraries and may not run on Arch."
        MAKEPKG_ARGS+=(--nodeps)
    fi
    # PKGDEST from makepkg.conf would put the package elsewhere
    (cd "$PKG_DIR" && PKGDEST="$PWD" makepkg "${MAKEPKG_ARGS[@]}")

    echo "Arch package created:"
    ls "$PKG_DIR"/*.pkg.tar.*
    exit 0
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory and debian artifacts..."
    rm -rf "$BUILD_DIR" build-arch
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/xdelta3-gui/ obj-*/
    rm -f translations/*.qm version.h
    SOURCE_NAME="$(get_source_name)"
    if [ -n "$SOURCE_NAME" ]; then
        rm -f ../"${SOURCE_NAME}"_*.build ../"${SOURCE_NAME}"_*.buildinfo
    fi
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

# The compiler is fixed when a build directory is first configured, so each
# compiler gets its own directory instead of switching a cached one.
if [ "$USE_CLANG" = true ]; then
    echo "Using clang compiler"
    CC=clang CXX=clang++ cmake "${CMAKE_ARGS[@]}"
else
    cmake "${CMAKE_ARGS[@]}"
fi

# Build the project
echo "Building project with Ninja..."
cmake --build "$BUILD_DIR" --parallel

echo "Build completed successfully!"
echo "Executable: $BUILD_DIR/xdelta3-gui"
