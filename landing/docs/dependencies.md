# Dependencies and Requirements

## Build Dependencies

| Dependency | Minimum Version | Purpose |
|------------|-----------------|---------|
| CMake | 3.20 | Build system |
| C++ Compiler | C++17 support | Core language |
| Make | any | Build tool |

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Or use Homebrew
brew install cmake
```

### Linux

```bash
# Ubuntu/Debian
sudo apt-get install cmake build-essential

# Fedora
sudo dnf install cmake gcc-c++ make
```

## Runtime Dependencies

None. Vibrowser is self-contained.

## Optional Dependencies

| Tool | Purpose |
|------|---------|
| ImageMagick | Convert .ppm to other formats |
| Node.js | Running test fixtures |

## Hardware Requirements

- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: 500MB for build artifacts
- **Architecture**: ARM64 (Apple Silicon) or x86_64
