# Repository Guidelines

This is the canonical guide for working in this repository (for both humans and AI agents).

## Project Overview
xdelta3-gui is a Qt6-based graphical interface for the `xdelta3` binary delta compression
tool. It provides a simple GUI for creating binary patches between two files and for applying
a patch to regenerate a file.

## Project Structure & Module Organization
- `main.cpp`, `mainwindow.cpp`, `cmd.cpp`, `droplineedit.cpp` contain the Qt6 application logic.
- `mainwindow.ui` defines the UI layout (Qt Designer).
- `translations/` holds Qt `.ts` files; compiled `.qm` files are generated during build.
- `translations-desktop-file/` contains desktop file i18n tooling and `.po` files.
- `debian/` contains packaging metadata; `debs/` is a build artifact drop.
- Assets and desktop integration live at `xdelta3-gui.png`, `xdelta3-gui.svg`, and `xdelta3-gui.desktop`.

## Build, Test, and Development Commands
- `./build.sh` builds a Release binary into `build/` using CMake + Ninja.
- `./build.sh --debug` builds a Debug binary with symbols.
- `./build.sh --clang` builds using clang instead of gcc.
- `./build.sh --clean` removes `build/` and packaging artifacts before building.
- `./build.sh --debian` builds a Debian package and moves artifacts into `debs/`.
- `./build.sh --arch` builds an Arch Linux `.tar.zst` package.
- Manual build: `cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --parallel`.
- Run locally: `./build/xdelta3-gui`.

### Build Details
- **Version Management**: Project version is read from `debian/changelog` via `dpkg-parsechangelog`. When bumping versions, update the changelog.
- **C++ Standard**: C++20 is required.
- **Qt Version**: Qt6 (Core, Gui, Widgets, LinguistTools); also uses `QtDBus` for the taskbar progress integration.
- **Compiler Flags**: Strict error checking (`-Werror`) on warnings for return types, switch statements, and uninitialized variables.
- **LTO**: Enabled for Release builds (`-flto=thin` for Clang, `-flto=auto` for GCC).
- **Runtime Dependency**: Requires the `xdelta3` binary on the system PATH. The optional `progress` utility, when present, is used as a Create-Patch progress fallback.

## Architecture

### Core Components
1. **main.cpp** — Entry point. Sets the application/organization name, prefers the XCB/X11
   Qt platform when available, loads translations, enforces non-root execution, verifies the
   `xdelta3` binary is present, and accepts an optional patch file path as a positional
   argument (which switches to the Apply Patch tab).
2. **MainWindow** (`mainwindow.h/cpp`) — `QDialog`-based main window. Visible tabs are
   "Create Patch" and "Apply Patch"; a third, normally hidden, "Progress" tab is shown for the
   duration of an operation. Handles file selection/validation, runs `xdelta3` via `Cmd`, and
   persists window geometry and preferences (compression level, secondary compression, last
   used directories) via `QSettings`.
3. **Cmd** (`cmd.h/cpp`) — Thin `QProcess` subclass. Launches a program directly with an
   argument list (`start(program, args)`) — **no shell**, so paths with spaces/special
   characters are safe. Accumulates combined stdout+stderr into `out_buffer` and emits
   `commandFinished(bool success, const QString &output)` when the process exits.
4. **DropLineEdit** (`droplineedit.h/cpp`) — `QLineEdit` subclass accepting drag-and-drop file
   paths. Used as a promoted widget in the `.ui` file for the source, target, input, and patch
   fields. Emits `fileDropped(QString)` on drop.

### Key Architectural Patterns
- **Asynchronous process execution**: `Cmd::runAsync(program, args)` starts `xdelta3`
  non-blocking. `QProcess::started` triggers `MainWindow::cmdStart()`; `Cmd::commandFinished`
  triggers `MainWindow::cmdFinished()`. A 1-second `QTimer` drives periodic UI updates
  (`updateBar()`) while the process runs. There is no nested event loop.
- **Atomic, non-destructive output**: operations never write directly to the user's target
  path. Each run writes to a unique temp file in the destination directory (`makeTempPath()`)
  and, only on success, replaces the destination with a single atomic `rename(2)`
  (`cmdFinished()`). On failure/cancel/close the temp file is removed and the original file is
  left untouched. The temp's mode is normalized before promotion (existing file's permissions
  when overwriting, otherwise `0666 & ~umask`).
- **Progress tracking** (two mechanisms):
  - *Apply Patch* — `xdelta3 printhdrs` is run once to sum the VCDIFF target window lengths
    into the expected output size; progress is then `temp size / expected size`. The probe is
    async and tagged with an `operationId` so a late result can't apply to a later run.
  - *Create Patch* (and Apply Patch before the size is known) — falls back to the external
    `progress -c xdelta3` utility, polled via a persistent `progressProcess`; if `progress`
    isn't installed, the bar goes indeterminate.
- **Signal flow**: `QProcess::started` → `cmdStart()` (switch to the Progress tab, hide the tab
  bar, start the timer) → timer ticks call `updateBar()` → `Cmd::commandFinished` →
  `cmdFinished()` (promote temp→final, restore tabs, report result).
- **Taskbar progress**: `updateTaskbar()` emits the Unity `LauncherEntry` D-Bus signal so
  compatible docks show a progress overlay.

### xdelta3 Command Integration
- **Create Patch**: `xdelta3 -f encode -[level] [-S compression] -s <source> <target> <temp>`
- **Apply Patch**: `xdelta3 -f decode -s <input> <patch> <temp>`

`-f` is always passed because the output is a pre-created temp placeholder. `<temp>` is later
renamed to the user-chosen path. Compression levels are 1-9 (default 4); secondary compression
options are None, djw, fgk, lzma (`-S` is omitted when None).

### UI Structure
- Built with Qt Designer (`mainwindow.ui`), using `CMAKE_AUTOMOC/AUTOUIC/AUTORCC`.
- `DropLineEdit` is a promoted widget (header `droplineedit.h`).
- Three tabs: Create Patch, Apply Patch, and a hidden Progress tab shown during operations.
  Progress is rendered inline on that tab (progress bar + status labels), not a separate
  `QProgressDialog`, and updated by the `QTimer`.

## Translation System
- Translation files live in `translations/`, managed through Transifex.
- Use `translations/get_translations` to pull translations from Transifex.
- The build uses `qt6_add_translations()` with `-compress -nounfinished -removeidentical -silent`.
- Generated `.qm` files install to `/usr/share/xdelta3-gui/locale`.
- New user-facing strings must be wrapped in `tr()`; mention updated `.ts`/`.po` files in PRs.

## Debian Packaging
- Packaging files are in `debian/`; version is sourced from `debian/changelog` (required for builds).
- Build with `./build.sh --debian` or `debuild -us -uc`. Artifacts move to `debs/`.
- No test targets are defined (tests are skipped in `debian/rules`).

## Coding Style & Naming Conventions
- C++20 with Qt6; prefer idiomatic Qt types (`QString`, `QFile`, etc.).
- Indentation is 4 spaces; opening braces on the next line for classes and functions.
- Header guards use `#pragma once`.
- Member variables use `camelCase` (see `mainwindow.h`).
- UI object names are defined in `mainwindow.ui`; keep them descriptive and consistent.
- User-facing errors are reported via `QMessageBox`.

## Testing Guidelines
- No automated test suite. Validate changes by building and running the GUI.
- Cover both workflows: "Create Patch" and "Apply Patch", including overwrite and cancel paths.

## Commit & Pull Request Guidelines
- Commit messages use short, imperative summaries (e.g., "Update build system").
- PRs should include a clear description, manual test steps, and screenshots for UI changes.
- If you modify translations, mention the updated `.ts`/`.po` files and the tooling used.
- Versioning is sourced from `debian/changelog`; update it when preparing releases.

## Packaging & Dependencies
- Build requires Qt6 (Core, Gui, Widgets, LinguistTools, DBus), CMake 3.16+, and Ninja.
- Runtime requires the `xdelta3` binary on the system PATH; `progress` is an optional enhancement.

## Development Notes
- Application intentionally blocks root execution for safety.
- Window geometry and preferences persist across sessions via `QSettings`.
- File paths are validated before operations.
- The user's existing files are never modified until an operation fully succeeds (see the
  atomic-output pattern above).
