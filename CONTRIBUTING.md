# Contributing to paqetN

Thank you for your interest in contributing to paqetN! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

We are committed to providing a welcoming and inclusive environment. Please:
- Be respectful and considerate
- Welcome newcomers and help them get started
- Focus on constructive feedback
- Accept criticism gracefully

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/paqetN.git
   cd paqetN
   ```
3. **Set up the upstream remote**:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/paqetN.git
   ```

## Development Setup

See [docs/BUILDING.md](docs/BUILDING.md) and [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup instructions.

### Quick Setup

1. Install Qt 6.8+ with required modules
2. Install CMake 3.20+
3. Clone FluentUI submodule:
   ```bash
   git submodule update --init --recursive
   ```
4. Build:
   ```bash
   cmake -B build
   cmake --build build
   ```

## How to Contribute

### Reporting Bugs

When filing a bug report, please include:
- **Clear title** describing the issue
- **Steps to reproduce** the bug
- **Expected behavior** vs actual behavior
- **Environment details**: OS, Qt version, compiler
- **Screenshots** or logs if applicable

Use the bug report template when creating an issue.

### Suggesting Features

For feature requests, please:
- Check existing issues to avoid duplicates
- Clearly describe the use case
- Explain why this feature would be valuable
- Consider implementation complexity

### Improving Documentation

Documentation improvements are always welcome:
- Fix typos and clarify instructions
- Add examples or tutorials
- Improve API documentation
- Translate documentation (if applicable)

### Submitting Code Changes

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/my-new-feature
   ```
2. **Make your changes** following coding standards
3. **Write/update tests** for your changes
4. **Run tests** to ensure nothing breaks:
   ```bash
   cd build
   ctest -V
   ```
5. **Commit your changes** following commit guidelines
6. **Push to your fork**:
   ```bash
   git push origin feature/my-new-feature
   ```
7. **Create a Pull Request** on GitHub

## Coding Standards

### C++ Code

- **C++17** standard
- **Qt conventions**: Use Qt types (QString, QList, etc.) in Qt code
- **Naming**:
  - Classes: PascalCase (e.g., `PaqetController`)
  - Functions: camelCase (e.g., `connectConfig()`)
  - Private members: camelCase with `m_` prefix (e.g., `m_configRepo`)
  - Constants: UPPER_CASE or kPascalCase
- **Include guards**: Use `#pragma once`
- **Header includes**: Group and order (Qt headers, system headers, local headers)
- **Signals/Slots**: Prefer new-style connect syntax
- **Memory management**: Use smart pointers (QPointer, unique_ptr, shared_ptr)

Example:
```cpp
#pragma once

#include <QObject>
#include <QString>

class ConfigRepository;

class PaqetController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit PaqetController(QObject *parent = nullptr);
    ~PaqetController() override;

    QString status() const { return m_status; }

public slots:
    void connectConfig(const QString &configId);

signals:
    void statusChanged();

private:
    QString m_status;
    ConfigRepository *m_configRepo;
};
```

### QML Code

- **Indentation**: 4 spaces (no tabs)
- **Property order**:
  1. id
  2. Required properties (width, height, anchors, etc.)
  3. Custom properties
  4. Signal handlers
  5. Child items
- **Naming**:
  - IDs: camelCase (e.g., `connectButton`)
  - Custom properties: camelCase
- **Use FluentUI components**: Don't create custom UI elements if FluentUI provides them
- **Avoid inline JavaScript**: Extract complex logic to separate .js files or C++

Example:
```qml
import QtQuick
import FluentUI

FluButton {
    id: connectButton
    text: "Connect"
    enabled: !controller.isConnected

    onClicked: {
        controller.connectConfig(selectedConfigId)
    }
}
```

### CMake

- Use modern CMake (3.20+)
- Prefer target-based commands (`target_link_libraries`, not `link_libraries`)
- Group related files together
- Comment complex logic

## Testing Guidelines

### Writing Tests

- **Unit tests** for all new backend classes
- **Qt Test framework** for C++ tests
- Test file naming: `tst_<component>.cpp/h`
- Test class naming: `Tst<Component>`
- Use descriptive test names: `testConfigParsing_ValidJson_Success()`

Example:
```cpp
class TstPaqetConfig : public QObject {
    Q_OBJECT

private slots:
    void testFromJson_ValidData_Success();
    void testFromJson_InvalidData_Failure();
    void testToYaml_RoundTrip_Preserves();
};
```

### Running Tests

```bash
cd build
ctest -V

# Or run directly with verbose output
./tst_paqetn -v2
```

### Test Coverage

- Aim for >80% code coverage on new code
- Cover edge cases and error paths
- Mock external dependencies (files, network, processes)

## Commit Guidelines

### Commit Message Format

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, no logic change)
- `refactor`: Code refactoring (no feature change)
- `test`: Adding or updating tests
- `chore`: Build, CI, or tooling changes

**Examples**:
```
feat(controller): add latency testing for configs

Implement LatencyChecker class to test proxy connection
speed via SOCKS5. Adds testLatency() method to controller.

Closes #42
```

```
fix(ui): correct theme toggle state persistence

Settings dialog wasn't saving theme preference correctly.
Fixed by updating SettingsRepository key.

Fixes #58
```

### Commit Best Practices

- **Atomic commits**: One logical change per commit
- **Clear messages**: Describe what and why, not how
- **Test before committing**: Ensure code compiles and tests pass
- **Sign commits** (optional but encouraged):
  ```bash
  git commit -s
  ```

## Pull Request Process

### Before Creating a PR

1. **Update from upstream**:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```
2. **Run tests**: Ensure all tests pass
3. **Check code style**: Follow coding standards
4. **Update documentation**: If you changed behavior or added features

### PR Description

Include:
- **Summary** of changes
- **Motivation**: Why is this change needed?
- **Related issues**: Closes #123, Fixes #456
- **Testing**: How did you test this?
- **Screenshots**: If UI changes

### Review Process

- Maintainers will review your PR
- Address feedback by pushing new commits
- Once approved, a maintainer will merge

### After Merge

- Delete your feature branch:
  ```bash
  git branch -d feature/my-new-feature
  git push origin --delete feature/my-new-feature
  ```

## Project Structure

```
paqetN/
├── .github/          # GitHub-specific files (workflows, templates)
├── 3rdparty/         # Third-party dependencies (FluentUI)
├── docs/             # Documentation
├── qml/              # QML UI files
│   ├── components/   # Reusable UI components
│   ├── dialogs/      # Dialog windows
│   ├── pages/        # Main pages
│   └── Main.qml      # Root window
├── scripts/          # Build and utility scripts
├── src/              # C++ source files
├── tests/            # Test files
├── CMakeLists.txt    # Root CMake configuration
└── README.md         # Project overview
```

## Getting Help

- **Documentation**: Check [docs/](docs/) directory
- **Issues**: Search existing issues or create a new one
- **Discussions**: Use GitHub Discussions for questions
- **Contact**: Reach out to maintainers

## License

By contributing, you agree that your contributions will be licensed under the MIT License (see [LICENSE](LICENSE)).

---

Thank you for contributing to paqetN!
