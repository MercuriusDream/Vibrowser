# Installation

## Requirements

- **OS**: macOS 13+ (development currently targets macOS)
- **Compiler**: Clang 15+ or GCC 12+
- **CMake**: 3.20+
- **Make**: BSD Make or GNU Make

Optional:
- **Node.js**: 18+ (for running tests)
- **Python**: 3.10+ (for build scripts)

## Build from source

```bash
# Clone the repository
git clone https://github.com/MercuriusDream/Vibrowser.git
cd Vibrowser

# Create build directory
cmake -S . -B build

# Build
cmake --build build

# Run tests
ctest --test-dir build
```

## Running

Currently, Vibrowser renders to static .ppm files:

```bash
./build/vibrowser --input test.html --output render.ppm
```

Interactive rendering is on the roadmap.
