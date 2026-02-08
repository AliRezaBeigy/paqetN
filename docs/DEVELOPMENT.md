# Development Guide

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Qt | 6.8+ | Quick, QuickControls2, Network, Widgets, Test, Svg, PrintSupport |
| CMake | 3.20+ | |
| C++ compiler | C++17 | MinGW 13.1 on Windows, GCC/Clang on Linux/macOS |
| Go | 1.21+ | Only needed to build the `paqet` binary |

### Windows-specific: compiler selection

On Windows, **Strawberry Perl** installs its own GCC into `C:\Strawberry\c\bin\` which often shadows Qt's MinGW on `PATH`. Using the wrong compiler causes linker errors like `undefined reference to '__imp___argc'`.

Always point CMake at Qt's MinGW explicitly:

```powershell
cmake -B build -G "MinGW Makefiles" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" ^
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"
```

Adjust paths to match your Qt installation.

---

## Building

### Configure

```bash
# Linux / macOS
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64

# Windows (MinGW Makefiles)
cmake -B build -G "MinGW Makefiles" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" ^
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" ^
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"
```

### Compile

```bash
cmake --build build
```

The app binary is `build/apppaqetN` (Linux/macOS) or `build\apppaqetN.exe` (Windows).

### Clean rebuild

```bash
cmake --build build --clean-first
```

---

## Project structure

```
paqetN/
├── main.cpp                    # App entry point, single-instance guard, QML engine setup
├── CMakeLists.txt              # Build configuration
├── 3rdparty/FluentUI/          # FluentUI library (Microsoft Fluent Design for Qt)
├── src/                        # C++ backend
│   ├── PaqetConfig.cpp/.h      # Config data model, YAML/URI serialization
│   ├── ConfigRepository.cpp/.h # JSON persistence (AppDataLocation/configs.json)
│   ├── ConfigListModel.cpp/.h  # QAbstractListModel for QML
│   ├── SettingsRepository.cpp/.h # App settings (QSettings)
│   ├── LogBuffer.cpp/.h        # Bounded log storage
│   ├── PaqetRunner.cpp/.h      # QProcess wrapper for paqet binary
│   ├── LatencyChecker.cpp/.h   # SOCKS proxy latency measurement
│   ├── PaqetController.cpp/.h  # Orchestrator, QML-facing API
│   └── SingleInstanceGuard.cpp/.h # Prevents duplicate app instances
├── Main.qml                    # Root window (FluWindow + FluNavigationView)
├── HostsPage.qml               # Config card grid + detail panel
├── LogPage.qml                 # Full-page log viewer
├── ConfigEditorDialog.qml      # Add/edit config form (FluPopup)
├── SettingsDialog.qml          # App settings (FluPopup)
├── GroupCard.qml               # Group summary card component
├── HostCard.qml                # Host config card component
├── DetailPanel.qml             # Selected config detail + actions
├── tests/                      # Qt Test suites
│   ├── main.cpp                # Test runner (executes all suites sequentially)
│   ├── tst_paqetconfig.*       # PaqetConfig tests
│   ├── tst_configrepository.*  # ConfigRepository tests
│   ├── tst_logbuffer.*         # LogBuffer tests
│   ├── tst_configlistmodel.*   # ConfigListModel tests
│   ├── tst_settingsrepository.* # SettingsRepository tests
│   ├── tst_controller.*        # PaqetController tests
│   └── tst_ui.*                # QML loading smoke test
└── scripts/
    ├── build-paqet.ps1         # Build paqet binary (Windows)
    └── build-paqet.sh          # Build paqet binary (Linux/macOS)
```

---

## FluentUI dependency

FluentUI is vendored in `3rdparty/FluentUI/` and built as a CMake subdirectory. No separate install step is needed.

CMake integration:

```cmake
set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENTUI_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/FluentUI/src)

# Link against fluentuiplugin
target_link_libraries(apppaqetN PRIVATE ... fluentuiplugin)
```

FluentUI requires `Qt6::PrintSupport` in the `find_package` call.

---

## Tests

### Running tests

**Via CTest:**

```bash
cd build
ctest -V
```

**Directly (recommended on Windows/MSYS):**

```bash
# Linux / macOS
./build/tst_paqetn

# Windows — output to file (stdout piping unreliable under MSYS)
./build/tst_paqetn.exe -o test_results.txt,txt
cat test_results.txt
```

The `-o file,txt` flag writes results to a file in plain text format. This is the reliable way to capture output on Windows/MSYS where stdout piping can lose data.

### Test suites

All suites run in a single executable (`tst_paqetn`). The test runner in `tests/main.cpp` executes them sequentially and OR's the exit codes — any failure sets a non-zero exit.

| Suite | Class | Tests | What it covers |
|-------|-------|-------|----------------|
| PaqetConfig | `TestPaqetConfig` | 13 | Default values, SOCKS port parsing, YAML export, `paqet://` URI generation/parsing, JSON import, QVariantMap round-trip, group field |
| ConfigRepository | `TestConfigRepository` | 5 | Add/load/update/remove configs from JSON file, get-by-ID, last-selected tracking |
| LogBuffer | `TestLogBuffer` | 3 | Append, clear, max-line truncation |
| ConfigListModel | `TestConfigListModel` | 5 | Role data exposure, index lookup, group role, distinct groups |
| SettingsRepository | `TestSettingsRepository` | 4 | Theme, SOCKS port, connection-check URL, log-level persistence |
| PaqetController | `TestController` | 10 | Config CRUD, import/export, clipboard, file I/O, selection state, groups |
| UI | `TestUi` | 1 | QML engine loads `Main.qml` with controller context (smoke test) |

**Total: 41 test methods across 7 suites.**

### Test environment notes

- Tests use `QGuiApplication` (not `QApplication`) — sufficient for QML but no widget support.
- `app.setOrganizationName("paqetN_test")` isolates test settings from production.
- `ConfigRepository` tests write to a temp directory; no cleanup conflicts with the real config file.
- The UI smoke test attempts to load `Main.qml` from the QML module resources. This may emit a `QQmlApplicationEngine failed to load component` warning if QML resources aren't embedded in the test binary — this is expected and the test still passes by checking `engine.rootObjects().isEmpty()`.
- `QT_QML_IMPORT_PATH` is set by CTest to the app build directory so FluentUI imports resolve correctly.

### Writing new tests

1. Create `tests/tst_yourfeature.h` and `tests/tst_yourfeature.cpp`.
2. Define a `QObject` subclass with `Q_OBJECT` macro. Put test methods in `private slots:`.
3. Add the `.cpp` file to the `tst_paqetn` sources in `CMakeLists.txt`.
4. Add the include and `QTest::qExec` call in `tests/main.cpp`:

```cpp
#include "tst_yourfeature.h"

// In main(), before return:
TestYourFeature tN;
status |= QTest::qExec(&tN, argc, argv);
```

---

## Building the paqet binary

The app needs a `paqet` binary (Go) either next to the executable or on `PATH`. You can also set the path in **Settings > paqet binary path**.

### Windows

```powershell
.\scripts\build-paqet.ps1 -PaqetSrc "C:\path\to\paqet"
copy build\paqet\paqet.exe build\
```

### Linux / macOS

```bash
./scripts/build-paqet.sh /path/to/paqet
cp build/paqet/paqet build/
```

Both scripts look for `go.mod` in the source directory to validate it's the right repo. The `PAQET_SRC` environment variable can be used as an alternative to the command-line argument.

---

## App initialization sequence

`main.cpp` performs the following on startup:

1. Sets `HighDpiScaleFactorRoundingPolicy::Floor` (Windows only)
2. Creates `QApplication` with org/app name `"paqetN"`
3. Checks `SingleInstanceGuard` — shows a warning and exits if another instance is running
4. Creates `QQmlApplicationEngine`
5. Instantiates `PaqetController` and registers it as `paqetController` context property
6. Loads `Main.qml` from QML module URI `paqetN`
7. Enters the Qt event loop

---

## QML module

The QML module URI is `paqetN` (version 1.0). All QML files are registered via `qt_add_qml_module` in CMake and accessed at:

```
qrc:/qt/qml/paqetN/<FileName>.qml
```

The `FluNavigationView` loads pages using these resource URLs. When adding a new page:

1. Create the `.qml` file in the project root.
2. Add it to `QML_FILES` in `CMakeLists.txt`.
3. Reference it as `"qrc:/qt/qml/paqetN/YourPage.qml"` in `FluNavigationView`.

---

## Common issues

| Problem | Cause | Fix |
|---------|-------|-----|
| `undefined reference to '__imp___argc'` at link time | Strawberry Perl's GCC used instead of Qt MinGW | Set `-DCMAKE_C_COMPILER` and `-DCMAKE_CXX_COMPILER` explicitly (see above) |
| `QQmlApplicationEngine failed to load component` in tests | QML resources not embedded in test binary | Expected — the UI test handles this gracefully |
| Test stdout is empty on Windows/MSYS | Pipe buffering issue in MSYS terminal | Use `-o results.txt,txt` flag to write output to file |
| FluentUI components not found at runtime | `QT_QML_IMPORT_PATH` not set | CTest sets this automatically; for manual runs, set it to the app build directory |
| `PrintSupport not found` during configure | Missing Qt module | Install the `Qt6::PrintSupport` component (required by FluentUI) |
