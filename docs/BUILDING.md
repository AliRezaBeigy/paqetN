# Building paqetN

This guide covers building paqetN from source on different platforms.

## Prerequisites

### All Platforms
- Qt 6.8+ (or 6.4+) with the following modules:
  - Quick
  - QuickControls2
  - Network
  - Widgets
  - Svg
  - PrintSupport
  - Core5Compat
- CMake 3.20+
- C++17 compatible compiler
- zlib development library (for QuaZip)

### Platform-Specific Requirements

#### Windows
- Qt with MinGW 13.1.0+ (64-bit)
- MSYS2 (optional, for Unix-like build environment)
- zlib library (automatically detected from Strawberry Perl installation at `C:/Strawberry/c/lib/libz.a` if available)

**Important:** Use Qt's MinGW compiler, NOT Strawberry Perl's GCC or other system GCC installations.

#### Linux
- GCC 9+ or Clang 10+
- Qt 6.8+ from official Qt installer or distribution packages
- zlib development package:
  ```bash
  # Ubuntu/Debian
  sudo apt-get install zlib1g-dev

  # Fedora/RHEL
  sudo dnf install zlib-devel

  # Arch Linux
  sudo pacman -S zlib
  ```

#### macOS
- Xcode 13+
- Qt 6.8+ from official Qt installer

## Build Instructions

### Quick Start

1. **Set Qt path** (adjust to your Qt installation):

   ```bash
   # Linux/macOS
   export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64

   # Windows (PowerShell)
   $env:CMAKE_PREFIX_PATH = "C:\Qt\6.8\mingw1310_64"

   # Windows (MSYS2/Bash)
   export CMAKE_PREFIX_PATH=/c/Qt/6.8/mingw1310_64
   ```

2. **Configure the project**:

   ```bash
   cmake -B build -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH
   ```

   On Windows with MinGW, you may need to explicitly specify the compiler:

   ```bash
   cmake -B build \
     -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" \
     -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe" \
     -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH
   ```

3. **Build**:

   ```bash
   cmake --build build
   ```

4. **Run**:

   ```bash
   # Linux/macOS
   ./build/apppaqetN

   # Windows
   build\apppaqetN.exe
   ```

### Build Types

#### Debug Build (default)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

#### Release Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Running Tests

The project includes comprehensive unit and UI tests using Qt Test framework.

```bash
# Build the project first
cmake --build build

# Run tests via CTest
cd build
ctest -V

# Or run the test executable directly
./tst_paqetn          # Linux/macOS
tst_paqetn.exe        # Windows

# On Windows/MSYS, capture output to file
./tst_paqetn.exe -o test_results.txt,txt
```

Test coverage includes:
- PaqetConfig (parsing, YAML, URI serialization)
- ConfigRepository (CRUD operations)
- LogBuffer (log management)
- ConfigListModel (Qt model interface)
- SettingsRepository (settings persistence)
- PaqetController (main orchestrator)
- UI smoke tests (QML loading, state verification)

## Building paqet Binary

paqetN requires the `paqet` proxy binary to function. You can either:

1. **Use an existing binary**: Place `paqet` (or `paqet.exe` on Windows) next to the executable or in your PATH
2. **Build from source**: See instructions in the [paqet repository](https://github.com/hanselime/paqet)

### Build Scripts

The project includes helper scripts to build paqet:

#### Windows (PowerShell)
```powershell
.\scripts\build-paqet.ps1 -PaqetSrc "C:\path\to\paqet"
# Then copy build\paqet\paqet.exe next to apppaqetN.exe
```

#### Linux/macOS (Bash)
```bash
./scripts/build-paqet.sh /path/to/paqet
# Copy build/paqet/paqet next to your app
```

Alternatively, set **Settings → paqet binary path** in the application to point to your paqet executable.

## Deployment

### Linux
Use `linuxdeploy` or Qt's deployment tool:
```bash
/path/to/Qt/6.8/gcc_64/bin/linuxdeploy-x86_64.AppImage --appdir=AppDir --executable=build/apppaqetN --plugin qt
```

### Windows
Use `windeployqt`:
```powershell
C:\Qt\6.8\mingw1310_64\bin\windeployqt.exe build\apppaqetN.exe
```

### macOS
Use `macdeployqt`:
```bash
/path/to/Qt/6.8/macos/bin/macdeployqt build/apppaqetN.app
```

## Troubleshooting

### Common Build Issues

1. **FluentUI submodule missing**
   ```bash
   git submodule update --init --recursive
   ```

2. **CMake can't find Qt**
   - Ensure `CMAKE_PREFIX_PATH` points to your Qt installation
   - On Windows, use forward slashes in paths: `C:/Qt/6.8/mingw1310_64`

3. **Wrong compiler on Windows**
   - Explicitly specify MinGW compiler paths in cmake command
   - Avoid having Strawberry Perl or other GCC in PATH during build

4. **Test executable crashes or doesn't capture output**
   - On Windows/MSYS: Use `-o file,txt` flag when running tests
   - Ensure QML import path is set: `QT_QML_IMPORT_PATH=$<TARGET_FILE_DIR:apppaqetN>`

5. **Qt 6 compatibility issues**
   - The project requires Qt 6.4+ but is developed with Qt 6.8+
   - Some features may require newer Qt versions
   - Qt5Compat module is required for FluentUI (GraphicalEffects)

## Development Setup

For IDE integration and development workflows, see [DEVELOPMENT.md](DEVELOPMENT.md).

## License

See [LICENSE](../LICENSE) file in the root directory.
