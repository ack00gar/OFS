# OpenFunscripter - FunGen Edition

**FunGen Edition** is a customized build of OpenFunscripter optimized for macOS Apple Silicon with enhanced features and branding.

## Features

- ✅ **Native Apple Silicon (ARM64) support** - Runs natively on M1/M2/M3 Macs
- ✅ **FunGen Edition branding** - Custom logo and About window
- ✅ **Debug features enabled** - Includes "Reset layout" and other development tools
- ✅ **macOS integration** - Native file explorer and URL opening
- ✅ **ARM64 optimizations** - Uses ARM64-specific CPU instructions

## Prerequisites

### macOS

- **macOS 11.0 (Big Sur)** or later
- **Xcode Command Line Tools**: `xcode-select --install`
- **Homebrew**: [https://brew.sh](https://brew.sh)
- **CMake**: `brew install cmake`
- **pkg-config**: `brew install pkg-config`
- **mpv** (for video playback): `brew install mpv`

### Windows

- **Visual Studio 2019** or later with C++ support
- **CMake 3.16+**: [https://cmake.org/download/](https://cmake.org/download/)
- **Git**: [https://git-scm.com/download/win](https://git-scm.com/download/win)

### Linux

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake git pkg-config libmpv-dev libgl1-mesa-dev

# Fedora
sudo dnf install gcc-c++ cmake git pkgconfig mpv-libs-devel mesa-libGL-devel

# Arch Linux
sudo pacman -S base-devel cmake git pkgconf mpv mesa
```

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/OFS-FunGen.git
cd OFS-FunGen
git submodule update --init --recursive
```

### 2. Build

#### macOS (Apple Silicon)

```bash
# Configure for ARM64 native build (Debug mode for full features)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build (use all CPU cores)
cmake --build build -j$(sysctl -n hw.ncpu)

# The app bundle will be at: bin/OpenFunscripter.app
```

#### macOS (Universal Binary - Intel + ARM64)

```bash
# Configure for universal binary
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build
cmake --build build -j$(sysctl -n hw.ncpu)
```

#### Windows

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug -j%NUMBER_OF_PROCESSORS%

# The executable will be at: bin\OpenFunscripter.exe
```

#### Linux

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# The executable will be at: bin/OpenFunscripter
```

### 3. Install (Optional)

#### macOS

```bash
# Copy to Applications folder
cp -R bin/OpenFunscripter.app /Applications/
```

#### Linux

```bash
# Copy to /usr/local/bin
sudo cp bin/OpenFunscripter /usr/local/bin/
```

## Build Types

Choose the appropriate build type based on your needs:

- **Debug** (Recommended for development)
  - Includes debug symbols and development features
  - "Reset layout" option in View menu
  - Larger file size (~24MB on macOS)
  - No optimizations (slower performance)
  - Use: `-DCMAKE_BUILD_TYPE=Debug`

- **Release** (Production)
  - Fully optimized for performance
  - Smallest file size (~5.5MB on macOS)
  - No debug features
  - Use: `-DCMAKE_BUILD_TYPE=Release`

- **RelWithDebInfo** (Balanced)
  - Optimized with debug symbols
  - Medium file size (~6MB on macOS)
  - Use: `-DCMAKE_BUILD_TYPE=RelWithDebInfo`

## SDL2 Compilation Fix (macOS ARM64)

This build includes a fix for SDL2 compilation on macOS ARM64. The fix prevents C99 declaration errors in HIDAPI by modifying `lib/SDL2/CMakeLists.txt:535-543`:

```cmake
# Skip -Werror flag on Apple to avoid HIDAPI compilation issues
if(HAVE_GCC_WERROR_DECLARATION_AFTER_STATEMENT AND NOT APPLE)
  list(APPEND EXTRA_CFLAGS "-Werror=declaration-after-statement")
endif()
```

## Dependencies

All dependencies are included as git submodules except:

- **mpv/ffmpeg** - Install via package manager (Homebrew on macOS, apt/dnf/pacman on Linux)
- **OpenGL** - System provided

### Included Submodules

- SDL2 (windowing, input, events)
- ImGui (UI framework)
- Lua 5.4 (scripting)
- glad2 (OpenGL loader)
- glm (math library)
- json (nlohmann/json)
- sol2 (Lua bindings)
- bitsery (serialization)
- tracy (profiling, optional)

## Troubleshooting

### macOS: "OpenFunscripter.app is damaged"

This happens because the app is not signed. Right-click the app, select "Open", then click "Open" in the dialog.

Or remove the quarantine flag:
```bash
xattr -cr /Applications/OpenFunscripter.app
```

### macOS: Missing video playback

Install mpv via Homebrew:
```bash
brew install mpv
```

### Linux: Missing libmpv.so

Install mpv development libraries:
```bash
# Debian/Ubuntu
sudo apt-get install libmpv-dev

# Fedora
sudo dnf install mpv-libs-devel

# Arch
sudo pacman -S mpv
```

### Windows: Missing mpv-2.dll

The Windows build includes mpv-2.dll in the bin folder. If missing, download from:
[https://mpv.io/installation/](https://mpv.io/installation/)

## Project Structure

```
OFS-FunGen/
├── bin/                      # Build output
├── build/                    # CMake build directory
├── data/                     # Assets (icons, logos)
│   ├── fungen_logo.png
│   ├── icon.icns
│   ├── logo16.png
│   └── logo64.png
├── lib/                      # Third-party libraries (submodules)
├── src/                      # Source code
├── OFS-lib/                  # Core library
├── localization/             # Translation files
├── CMakeLists.txt
└── README.md
```

## FunGen Edition Changes

This edition includes the following customizations:

1. **Branding**
   - "FunGen Edition" in window title
   - Custom FunGen logo in About window
   - macOS-specific build info (on macOS only)

2. **macOS Integration**
   - Native ARM64 support for Apple Silicon
   - Force software decoding on M1/M2 for compatibility
   - Multi-location libmpv loading (Homebrew Intel/ARM paths)
   - Native file explorer via `open` command

3. **Build System**
   - SDL2 compilation fix for macOS ARM64
   - Universal binary support
   - Debug build recommended for full feature set

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes: `git commit -m "Add my feature"`
4. Push to the branch: `git push origin feature/my-feature`
5. Open a Pull Request

## License

This project inherits the license from the original OpenFunscripter project.

## Credits

- **Original OpenFunscripter**: [https://github.com/OpenFunscripter/OFS](https://github.com/OpenFunscripter/OFS)
- **FunGen Edition Maintainer**: k00gar

## Support

For issues specific to FunGen Edition, please open an issue on this repository.
For general OpenFunscripter questions, refer to the original project.
