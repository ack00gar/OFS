# OpenFunscripter - FunGen Edition

**FunGen Edition** is a customized build of OpenFunscripter optimized to fully support macOS Apple Silicon with enhanced upcoming features.

---

## 💚 Support Development

**Your donations help support the development of [FunGen](https://github.com/ack00gar/FunGen-AI-Powered-Funscript-Generator) and contribute to potential future enhancements of OFS FunGen Edition.**

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/k00gar)

### About FunGen

**FunGen is a Python-based tool that uses AI to generate Funscript files from VR and 2D POV videos.** It enables fully automated funscript creation for individual scenes or entire folders of videos.

**Join the Discord community for discussions and support:** [Discord Community](https://discord.gg/WYkjMbtCZA)

---

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

## Platform Support

- ✅ **macOS (Apple Silicon & Intel)** - Fully tested and supported
- ⚠️ **Windows** - Should work, but untested in this fork
- ⚠️ **Linux** - Should work, but untested in this fork

For Windows and Linux, follow the build instructions below. They should work as the codebase is cross-platform, but please report any issues you encounter.

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/ack00gar/OFS.git
cd OFS
git submodule update --init --recursive
```

### 2. Build

#### macOS (Apple Silicon) - TESTED ✅

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

#### Windows - UNTESTED ⚠️

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug -j%NUMBER_OF_PROCESSORS%

# The executable will be at: bin\OpenFunscripter.exe
```

**Note:** Windows build is untested in this fork. The build should work as the original OFS supports Windows, but please report any issues.

#### Linux - UNTESTED ⚠️

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# The executable will be at: bin/OpenFunscripter
```

**Note:** Linux build is untested in this fork. The build should work as the original OFS supports Linux, but please report any issues.

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

### macOS

**mpv is required for video playback.** Install via Homebrew:
```bash
brew install mpv
```

### Building from Source

All dependencies are included as git submodules except:

- **mpv/ffmpeg** - Required for video playback (Homebrew on macOS, apt/dnf/pacman on Linux)
- **OpenGL** - System provided

**Included Submodules:**
- SDL2 (windowing, input, events)
- ImGui (UI framework)
- Lua 5.4 (scripting)
- glad2 (OpenGL loader)
- glm (math library)
- json (nlohmann/json)
- sol2 (Lua bindings)
- bitsery (serialization)
- tracy (profiling, optional)

## macOS Installation & Code Signing

### ⚠️ Important: App is Not Code Signed

This app is **not code signed** with an Apple Developer certificate. macOS will show a security warning on first launch.

### How to Open the App

**Method 1: Right-Click Open (Recommended)**
1. Download and extract `OpenFunscripter.app`
2. **Right-click** (or Control-click) the app
3. Select **"Open"** from the menu
4. Click **"Open"** in the security dialog
5. The app will now run normally

**Method 2: Remove Quarantine Flag (Terminal)**
```bash
xattr -cr /Applications/OpenFunscripter.app
```

**Note:** You only need to do this **once**. After the first launch, the app will open normally.

## Troubleshooting

### macOS: Missing video playback

If video playback doesn't work, install mpv via Homebrew:
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

### Windows: mpv-2.dll

The Windows build **automatically downloads the latest mpv build** during compilation from [zhongfly/mpv-winbuild](https://github.com/zhongfly/mpv-winbuild/releases) (updated daily).

**If automatic download fails:**
1. Download the latest `mpv-dev-x86_64-v3-*.7z` from [GitHub Releases](https://github.com/zhongfly/mpv-winbuild/releases/latest)
2. Extract `mpv-2.dll` from the archive
3. Place it in your `build/` directory before compiling

The build system fetches the latest version automatically via GitHub API.

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
