# Repository Guidelines

## Project Structure & Module Organization
- `main.cpp`, `mainwindow.cpp`, `cmd.cpp` contain the Qt6 application logic.
- `mainwindow.ui` defines the UI layout (Qt Designer).
- `translations/` holds Qt `.ts` files; compiled `.qm` files are generated during build.
- `translations-desktop-file/` contains desktop file i18n tooling and `.po` files.
- `debian/` contains packaging metadata; `debs/` is a build artifact drop.
- Assets and desktop integration live at `xdelta3-gui.png`, `xdelta3-gui.svg`, and `xdelta3-gui.desktop`.

## Build, Test, and Development Commands
- `./build.sh` builds a Release binary into `build/` using CMake + Ninja.
- `./build.sh --debug` builds a Debug binary.
- `./build.sh --clean` removes `build/` and packaging artifacts before building.
- `./build.sh --debian` builds a Debian package and moves artifacts into `debs/`.
- `./build.sh --arch` builds an Arch Linux `.tar.zst` package in `build/`.
- Manual build: `cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --parallel`.
- Run locally: `./build/xdelta3-gui`.

## Coding Style & Naming Conventions
- C++20 with Qt6; prefer idiomatic Qt types (`QString`, `QFile`, etc.).
- Indentation is 4 spaces; keep braces on the next line for class members and functions.
- Header guards use `#pragma once`.
- Member variables use `camelCase` (see `mainwindow.h`).
- UI object names are defined in `mainwindow.ui`; keep names descriptive and consistent.

## Testing Guidelines
- No automated test suite is present. Validate changes by building and running the GUI.
- For manual checks, cover both workflows: "Create Patch" and "Apply Patch" tabs.

## Commit & Pull Request Guidelines
- Commit messages use short, imperative summaries (e.g., "Update build system").
- PRs should include a clear description, manual test steps, and screenshots for UI changes.
- If you modify translations, mention the updated `.ts`/`.po` files and the tooling used.
- Versioning is sourced from `debian/changelog`; update it when preparing releases.

## Packaging & Dependencies
- Build requires Qt6 (Core, Gui, Widgets, LinguistTools), CMake 3.16+, and Ninja.
- Runtime requires the `xdelta3` binary on the system PATH.
