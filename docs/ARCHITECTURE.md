# Architecture

This document describes the architecture of paqetN, a Qt 6 desktop application for managing paqet proxy configurations.

## Overview

paqetN follows a Model-View-Controller (MVC) architecture with Qt/QML:
- **Model**: C++ backend classes handling business logic and data
- **View**: QML UI components using FluentUI design system
- **Controller**: `PaqetController` orchestrating interactions between models and views

## Technology Stack

- **Qt 6.8+**: Application framework
- **QML**: Declarative UI
- **C++17**: Backend implementation
- **FluentUI**: Microsoft Fluent Design System for Qt
- **QuaZip**: Cross-platform ZIP archive library
- **CMake**: Build system
- **Qt Test**: Testing framework

## Component Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         QML UI Layer                        │
│  (Main.qml, HostsPage, LogPage, ConfigEditorDialog, etc.)  │
└────────────────────┬────────────────────────────────────────┘
                     │ Q_PROPERTY / Q_INVOKABLE
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                     PaqetController                         │
│              (Orchestrator / Facade)                        │
└──┬──────────┬──────────┬──────────┬──────────┬──────┬──────┘
   │          │          │          │          │      │
   ▼          ▼          ▼          ▼          ▼      ▼
┌──────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌──────┐ ┌────────┐
│Config│ │ Config │ │ Paqet  │ │ Log    │ │Settings│ │Update │
│List  │ │Repo.   │ │ Runner │ │ Buffer │ │Repo.   │ │Manager│
│Model │ │        │ │        │ │        │ │        │ │       │
└──────┘ └────────┘ └────────┘ └────────┘ └────────┘ └───┬───┘
   │          │          │                                  │
   │          │          └─────> QProcess (paqet)          │
   │          │                                             │
   │          └─────> JSON Persistence                     │
   │                                                        │
   └─────> QAbstractListModel                              │
                                                            │
                                    ┌───────────────────────┴────┐
                                    ▼                            ▼
                              GitHub API                   QuaZip/ZIP
                            (Update Check)                 Extraction
```

## Backend Components

### Core Classes

#### PaqetController
**File**: `src/PaqetController.h/cpp`

The main orchestrator that:
- Exposes unified API to QML via `Q_PROPERTY` and `Q_INVOKABLE`
- Manages config lifecycle (add, edit, delete, import, export)
- Controls paqet process (start, stop, status)
- Coordinates between ConfigRepository, ConfigListModel, PaqetRunner, LogBuffer
- Handles clipboard operations and file I/O

**Key Properties**:
- `configModel`: ConfigListModel for UI binding
- `currentConfigId`: Currently selected/running config
- `isConnected`: Proxy connection state
- `logs`: LogBuffer for real-time log viewing

**Key Methods**:
- `connectConfig(id)`: Start paqet with specified config
- `disconnect()`: Stop paqet process
- `testLatency(id)`: Check proxy latency
- `importFromClipboard()`, `importFromFile()`: Config import
- `exportConfigAsLink()`, `exportConfigAsYaml()`: Config export

#### ConfigRepository
**File**: `src/ConfigRepository.h/cpp`

Manages persistent storage of configurations:
- JSON serialization/deserialization
- CRUD operations (add, update, remove, get)
- Loads from `QStandardPaths::AppDataLocation/configs.json`
- Thread-safe file operations
- Emits signals on data changes

#### ConfigListModel
**File**: `src/ConfigListModel.h/cpp`

`QAbstractListModel` implementation for QML:
- Exposes configs as Qt model with roles (configId, name, serverAddr, group, etc.)
- Supports dynamic updates (add, remove, update operations)
- Integrates with QML ListView, GridView, Repeater
- Role-based data access for QML bindings

**Roles**:
- ConfigIdRole, NameRole, ServerAddrRole, GroupRole
- KcpBlockRole, KcpModeRole, LatencyRole, etc.

#### PaqetRunner
**File**: `src/PaqetRunner.h/cpp`

Manages paqet process lifecycle:
- `QProcess` wrapper for paqet binary
- Starts paqet with config parameters
- Captures stdout/stderr to LogBuffer
- Process state monitoring
- Graceful shutdown handling

#### LogBuffer
**File**: `src/LogBuffer.h/cpp`

Circular buffer for log management:
- Stores stdout/stderr from paqet
- Fixed-size buffer (prevents memory bloat)
- Real-time log updates via signals
- Export to file, clear, copy operations
- Word wrap support for UI

#### SettingsRepository
**File**: `src/SettingsRepository.h/cpp`

Application settings persistence:
- Uses QSettings (organization: paqetN, app: paqetN)
- Theme preference (dark/light/system)
- SOCKS port, connection check URL
- Log level, paqet binary path
- UI preferences (window size, etc.)

#### LatencyChecker
**File**: `src/LatencyChecker.h/cpp`

Proxy latency testing:
- Connects through SOCKS5 proxy
- HTTP GET request to check URL
- Measures round-trip time
- Async operation with timeout

#### SingleInstanceGuard
**File**: `src/SingleInstanceGuard.h/cpp`

Ensures single app instance:
- Uses `QSharedMemory` for instance detection
- Prevents multiple instances running simultaneously

#### UpdateManager
**File**: `src/UpdateManager.h/cpp`

Manages application and binary updates:
- **GitHub API Integration**: Checks latest releases for paqetN and paqet
- **Version Detection**: Detects installed paqet version via `paqet version` command
- **Binary Download**: Downloads paqet from GitHub releases with progress tracking
- **ZIP Extraction**: Uses QuaZip (via ZipExtractor) for cross-platform archive extraction
- **Auto-Update**: Self-update mechanism for paqetN with .exe replacement
- **Settings Integration**: Auto-download, auto-check, and auto-update preferences

**Key Features**:
- Async downloads with QNetworkAccessManager
- Platform detection (Windows/macOS/Linux)
- Progress signals for UI updates
- Error handling and retry logic
- Skips example files during extraction

#### ZipExtractor
**File**: `src/ZipExtractor.h`

Cross-platform ZIP extraction:
- **QuaZip Integration**: Uses JlCompress for reliable ZIP handling
- **All Compression Methods**: Supports stored, deflate, and other ZIP formats
- **Large File Support**: Handles multi-megabyte binaries
- **Auto-Cleanup**: Removes unwanted files (e.g., example/ folder)

#### PaqetConfig
**File**: `src/PaqetConfig.h/cpp`

Configuration data model:
- Represents a single paqet config
- Serialization: JSON, YAML, URI (`paqet://...`)
- Validation and parsing
- Round-trip conversion between formats

## UI Architecture

### FluentUI Integration

All UI components use FluentUI from `3rdparty/FluentUI/`:
- **Theme System**: FluTheme (dark/light modes, colors, typography)
- **Components**: FluButton, FluTextBox, FluComboBox, FluScrollBar, etc.
- **Navigation**: FluWindow + FluNavigationView (sidebar navigation)
- **Dialogs**: FluPopup base for modals
- **Icons**: FluentIcons enumeration

### Page Structure

#### Main.qml
**File**: `Main.qml`

Root window using `FluWindow`:
- FluNavigationView in Compact mode
- Pages: Hosts, Logs, Settings
- NoStack navigation (no page history)
- Custom title bar with theme toggle
- Acrylic effects (Windows only)

#### HostsPage.qml
**File**: `HostsPage.qml`

Main config management page:
- Card grid layout for configs (grouped by `group` field)
- GroupCard components for each group
- DetailPanel for selected config (right sidebar)
- Actions: Connect, Test, Edit, Delete, Add, Import, Export

#### LogPage.qml
**File**: `LogPage.qml`

Full-page log viewer:
- Real-time log display from LogBuffer
- Word wrap toggle
- Actions: Copy all, Export to file, Clear
- Monospace font, auto-scroll to bottom

#### SettingsPage.qml
**File**: `SettingsPage.qml`

Application settings (replaces old SettingsDialog):
- Theme selection (Dark, Light, System)
- SOCKS port configuration
- Connection check URL
- Log level (Debug, Info, Warn, Error)
- Optional paqet binary path

### Component Structure

#### GroupCard.qml
Collapsible card for a group of configs:
- Displays group name with expand/collapse
- Contains HostCard grid
- Group rename functionality
- Fluent card styling

#### HostCard.qml
Individual config display card:
- Config name, server address
- Status indicator (connected, idle)
- Latency display
- Click to select, shows in DetailPanel

#### DetailPanel.qml
Right sidebar panel:
- Displays selected config details
- Actions: Connect, Test, Edit, Delete
- Shows connection status
- Export options (link, YAML)

#### ConfigEditorDialog.qml
Modal dialog for config add/edit:
- FluPopup-based modal
- Form fields: name, server, port, password, KCP settings, etc.
- Validation and save/cancel actions

## Data Flow

### Config Creation Flow
```
User clicks "Add" → ConfigEditorDialog opens → User fills form →
Click "Add" → PaqetController.addConfig(data) → ConfigRepository.add(config) →
Emits configAdded signal → ConfigListModel.add(config) → UI updates
```

### Connection Flow
```
User clicks "Connect" on config → PaqetController.connectConfig(id) →
ConfigRepository.get(id) → PaqetRunner.start(config) → QProcess starts →
Stdout/stderr → LogBuffer.append() → UI log updates → Status: Connected
```

### Import from Clipboard Flow
```
User clicks "Import from Clipboard" → PaqetController.importFromClipboard() →
QClipboard.text() → PaqetConfig.fromString(text) → Parse (URI or JSON) →
ConfigRepository.add(config) → ConfigListModel updates → UI shows new config
```

## Configuration Storage

### configs.json
**Location**: `QStandardPaths::AppDataLocation/configs.json`

**Format**:
```json
{
  "configs": [
    {
      "id": "unique-uuid",
      "name": "My Server",
      "serverAddr": "example.com",
      "serverPort": 8080,
      "password": "secret",
      "group": "Production",
      "localAddr": "127.0.0.1",
      "localPort": 1284,
      "kcpBlock": "aes",
      "kcpMode": "fast",
      "kcpMtu": 1350,
      "kcpSndwnd": 1024,
      "kcpRcvwnd": 1024,
      "kcpDatashard": 10,
      "kcpParityshard": 3,
      "latency": 0
    }
  ]
}
```

### Settings
**Stored via QSettings** (platform-specific location):
- Windows: Registry or INI file
- Linux: `~/.config/paqetN/paqetN.conf`
- macOS: `~/Library/Preferences/paqetN.plist`

## Testing Architecture

### Test Structure
**Directory**: `tests/`

**Test Files**:
- `tst_paqetconfig.cpp`: PaqetConfig parsing, serialization
- `tst_configrepository.cpp`: Repository CRUD operations
- `tst_logbuffer.cpp`: Log buffer operations
- `tst_configlistmodel.cpp`: Qt model interface
- `tst_settingsrepository.cpp`: Settings persistence
- `tst_controller.cpp`: Controller orchestration
- `tst_ui.cpp`: UI smoke tests (QML loading)

### Test Execution
- Uses Qt Test framework
- Run via `ctest -V` or direct executable
- Mocks: Temporary directories for configs.json
- Coverage: ~90% of backend code

## Build System

### CMake Structure
- Root `CMakeLists.txt`: Main project configuration
- FluentUI: `add_subdirectory(3rdparty/FluentUI/src)`
- QML module: `qt_add_qml_module(apppaqetN URI paqetN)`
- Tests: Separate executable with shared sources

### Key CMake Targets
- `apppaqetN`: Main executable
- `tst_paqetn`: Test executable
- `fluentuiplugin`: FluentUI library

## Threading Model

- **Main Thread**: UI (QML) and Qt event loop
- **QProcess Thread**: paqet subprocess I/O (managed by Qt)
- **Network Operations**: Async via Qt signals/slots (LatencyChecker)
- **File I/O**: Synchronous on main thread (configs.json is small)

## Key Design Patterns

1. **Facade Pattern**: PaqetController as unified API for QML
2. **Repository Pattern**: ConfigRepository, SettingsRepository for data access
3. **Observer Pattern**: Qt signals/slots for event propagation
4. **Model-View Pattern**: ConfigListModel for Qt Model/View
5. **Factory Pattern**: PaqetConfig parsing from multiple formats

## Security Considerations

- **Passwords**: Stored in plaintext in configs.json (consider encryption in future)
- **Single Instance**: Prevents multiple instances (avoid port conflicts)
- **Input Validation**: Config validation before process start
- **Process Isolation**: paqet runs as separate process (limited damage if compromised)

## Future Architecture Improvements

1. **Password Encryption**: Encrypt passwords in configs.json
2. **Plugin System**: Support for custom proxy protocols
3. **Config Sync**: Cloud sync for configs (optional)
4. **Logging Service**: Separate thread for log processing
5. **Dependency Injection**: Improve testability with DI framework

## Dependencies

### Qt Modules
- Qt6::Quick, Qt6::QuickControls2
- Qt6::Network
- Qt6::Widgets (for QSystemTrayIcon potential)
- Qt6::Svg (for icons)
- Qt6::PrintSupport (FluentUI requirement)
- Qt5Compat (FluentUI GraphicalEffects)

### 3rd Party
- **FluentUI**: `3rdparty/FluentUI/` (zhuzichu520/FluentUI)

### External Binaries
- **paqet**: Proxy binary (not bundled, user-provided or built separately)

## Platform-Specific Notes

### Windows
- Uses MSYS2/MinGW for build
- Requires Qt's MinGW, NOT system GCC
- Acrylic effects available (FluAcrylic)

### Linux
- Standard Qt build process
- XDG directories for config storage

### macOS
- App bundle packaging
- macOS-specific QSettings storage

---

For build instructions, see [BUILDING.md](BUILDING.md).
For development workflow, see [DEVELOPMENT.md](DEVELOPMENT.md).
