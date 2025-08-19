# xdelta3-gui

A Qt6-based graphical interface for the xdelta3 binary delta compression tool. Provides a user-friendly GUI for creating and applying binary patches between files, making binary difference operations accessible through an intuitive interface.

[![latest packaged version(s)](https://repology.org/badge/latest-versions/xdelta3-gui.svg)](https://repology.org/project/xdelta3-gui/versions)
[![build result](https://build.opensuse.org/projects/home:mx-packaging/packages/xdelta3-gui/badge.svg?type=default)](https://software.opensuse.org//download.html?project=home%3Amx-packaging&package=xdelta3-gui)

## Features

- **Binary Patch Creation**: Generate delta patches between two binary files
- **Patch Application**: Apply existing patches to create new versions of files
- **Progress Tracking**: Real-time progress display during operations
- **User-Friendly Interface**: Clean Qt6-based GUI with intuitive controls
- **File Validation**: Integrity checking and error handling

## Installation

### Prerequisites

- Qt6 (Core, Gui, Widgets, LinguistTools)
- C++20 compatible compiler (GCC or Clang)
- CMake 3.16+
- Ninja build system (recommended)
- xdelta3 binary (for actual delta operations)

### Building from Source

#### Quick Build
```bash
./build.sh           # Release build
./build.sh --debug   # Debug build
./build.sh --clean   # Clean rebuild
```

#### Manual CMake Build
```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

#### Debian Package Build
```bash
./build.sh --debian  # Build Debian package
```

### Package Installation

On MX Linux and compatible Debian-based systems:
```bash
sudo apt update
sudo apt install xdelta3-gui
```

## Usage

1. Launch the application from the application menu or run `xdelta3-gui` in terminal
2. Choose the operation type:
   - **Create Patch**: Generate a binary patch from two files
   - **Apply Patch**: Apply an existing patch to create a new file
3. Select your input files using the file browsers
4. Specify the output file location
5. Click the appropriate button to start the operation
6. Monitor progress through the progress bar and status messages

### Creating a Binary Patch
1. Select the original file (source)
2. Select the new file (target)
3. Choose where to save the patch file
4. Click "Create Patch"

### Applying a Patch
1. Select the original file
2. Select the patch file
3. Choose where to save the patched file
4. Click "Apply Patch"

## Technical Details

- **Language**: C++20
- **Framework**: Qt6
- **Build System**: CMake with Ninja generator
- **Backend**: xdelta3 binary
- **License**: GPL v3
- **Standards**: VCDIFF (RFC 3284) binary diff format

## Development

### Code Style
- Modern C++20 with Qt6 framework
- Header guards using `#pragma once`
- Member variables use camelCase

### Contributing

1. Fork the repository
2. Create a feature branch
3. Follow the existing code style
4. Test your changes thoroughly
5. Submit a pull request

### Translation Contributions

- Please join Translation Forum: https://forum.mxlinux.org/viewforum.php?f=96
- Please register on Transifex: https://forum.mxlinux.org/viewtopic.php?t=38671
- Choose your language and start translating: https://app.transifex.com/anticapitalista/antix-development

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See [LICENSE](LICENSE) for the full license text.

## Authors

- **Adrian** <adrian@mxlinux.org>
- **MX Linux Team** <http://mxlinux.org>

## Links

- [MX Linux](http://mxlinux.org)
- [Source Repository](https://github.com/mx-linux/xdelta3-gui)
- [Issue Tracker](https://github.com/mx-linux/xdelta3-gui/issues)
- [OpenSUSE Build Service](https://build.opensuse.org/projects/home:mx-packaging/packages/xdelta3-gui)
- [xdelta3 Project](http://xdelta.org/)

