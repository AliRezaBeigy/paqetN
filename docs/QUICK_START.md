# Quick Start Guide

This guide will help you get paqetN up and running quickly after reorganizing the project structure.

## After Reorganization: Rebuild Required

After the project structure reorganization, you **must rebuild** the project to update the QML resource paths.

### Option 1: Use Rebuild Scripts (Recommended)

#### Linux/macOS
```bash
./scripts/rebuild.sh
```

#### Windows (PowerShell)
```powershell
.\scripts\rebuild.ps1
```

Or with custom Qt path:
```powershell
.\scripts\rebuild.ps1 -QtPath "C:\Qt\6.8\mingw1310_64"
```

### Option 2: Manual Rebuild

#### 1. Set Qt Path
```bash
# Linux/macOS
export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64

# Windows (PowerShell)
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.8\mingw1310_64"

# Windows (Git Bash/MSYS2)
export CMAKE_PREFIX_PATH=/c/Qt/6.8/mingw1310_64
```

#### 2. Clean and Reconfigure
```bash
# Clean CMake cache
cd build
rm -rf CMakeFiles CMakeCache.txt *.cmake paqetN/ .qt/
cd ..

# Reconfigure
cmake -B build
```

On Windows with MinGW, specify compiler:
```bash
cmake -B build \
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"
```

#### 3. Build
```bash
cmake --build build
```

#### 4. Run
```bash
# Linux/macOS
./build/apppaqetN

# Windows
build\apppaqetN.exe
```

## Understanding the New Structure

### QML File Organization

The QML files are now organized in source:
```
qml/
├── Main.qml              # Root window
├── components/           # Reusable components
│   ├── DetailPanel.qml
│   ├── GroupCard.qml
│   └── HostCard.qml
├── dialogs/              # Dialog windows
│   ├── ConfigEditorDialog.qml
│   └── SettingsPage.qml
└── pages/                # Main pages
    ├── HostsPage.qml
    └── LogPage.qml
```

### Qt Resource System

Qt's `qt_add_qml_module` automatically flattens the QML files, so all files are accessible at:
```
qrc:/qt/qml/paqetN/FileName.qml
```

This means:
- `qml/pages/HostsPage.qml` → `qrc:/qt/qml/paqetN/HostsPage.qml`
- `qml/dialogs/ConfigEditorDialog.qml` → `qrc:/qt/qml/paqetN/ConfigEditorDialog.qml`
- `qml/components/GroupCard.qml` → `qrc:/qt/qml/paqetN/GroupCard.qml`

The source organization helps developers find files, but at runtime they're all in the same module namespace.

### Import Statements

In your QML files, you can now use:
```qml
import paqetN 1.0

// Use components directly
GroupCard {
    // ...
}

HostCard {
    // ...
}
```

Or reference pages by URL:
```qml
nav_view.push("qrc:/qt/qml/paqetN/HostsPage.qml")
```

## Troubleshooting

### "No such file or directory" Error

**Problem**: `qrc:/qt/qml/paqetN/HostsPage.qml: No such file or directory`

**Solution**: The build directory still has old cached files. Clean and rebuild:
```bash
cd build
rm -rf CMakeFiles CMakeCache.txt *.cmake paqetN/ .qt/
cd ..
cmake -B build
cmake --build build
```

### FluentUI "addWindow is not a function" Error

**Problem**: `TypeError: Property 'addWindow' of object FluRouter is not a function`

**Solution**: This is a FluentUI initialization issue. Make sure:
1. FluentUI submodule is properly initialized:
   ```bash
   git submodule update --init --recursive
   ```
2. Clean rebuild as shown above

### Qt Not Found

**Problem**: `Could not find a package configuration file provided by "Qt6"`

**Solution**: Set `CMAKE_PREFIX_PATH` to your Qt installation:
```bash
# Find your Qt installation path
# Common locations:
# Linux: /opt/Qt/6.8/gcc_64
# macOS: ~/Qt/6.8/macos
# Windows: C:\Qt\6.8\mingw1310_64 or C:\Qt\6.8\msvc2019_64

export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64
```

### Wrong Compiler on Windows

**Problem**: Build fails with compiler errors on Windows

**Solution**: Ensure you're using Qt's MinGW, not system GCC:
```bash
cmake -B build \
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"
```

## Testing

After rebuilding, verify everything works:

```bash
# Run tests
cd build
ctest -V

# Or run test executable directly
./tst_paqetn -v2                    # Linux/macOS
.\tst_paqetn.exe -o results.txt,txt # Windows
```

## Next Steps

1. **Read Documentation**: See [docs/](../docs/) for detailed guides
2. **Explore Code**: The organized structure makes navigation easier
3. **Start Developing**: See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines

## Need Help?

- **Build Issues**: See [docs/BUILDING.md](BUILDING.md)
- **Development**: See [docs/DEVELOPMENT.md](DEVELOPMENT.md)
- **Architecture**: See [docs/ARCHITECTURE.md](ARCHITECTURE.md)
- **Report Issues**: [GitHub Issues](https://github.com/AliRezaBeigy/paqetN/issues)
