# Developer Guide

Complete guide for developers working on the 7-Zip Parallel Compression Library.

## Table of Contents

1. [Building the Project](#building-the-project)
2. [Cross-Platform Development](#cross-platform-development)
3. [Creating Releases](#creating-releases)
4. [Architecture Overview](#architecture-overview)
5. [Cloud Storage Integration](#cloud-storage-integration)

---

## Building the Project

### Prerequisites

- C++11 compiler (GCC 4.8+, Clang, or MSVC 2015+)
- CMake 3.15+ (recommended) or Make
- pthread (Linux/macOS)

### Quick Build

**Linux/macOS (Make)**:
```bash
cd CPP/7zip/Compress
make -f makefile.test
./run_tests.sh
```

**All Platforms (CMake)**:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build .
ctest
```

See [BUILD_SYSTEM.md](BUILD_SYSTEM.md) for detailed build instructions.

---

## Cross-Platform Development

### Platform Support

| Platform | Arch | Build | Status |
|----------|------|-------|--------|
| Linux | x86_64 | Make/CMake | ✅ Tested |
| Windows | x64/x86 | CMake | ✅ Supported |
| macOS | x64/ARM64 | CMake | ✅ CI Enabled |

### Platform Abstraction

The codebase uses a platform abstraction layer:
- `CPP/Windows/` - Windows API wrappers
- `CPP/Common/MyWindows.cpp` - Unix emulation
- `C/Threads.c` - Portable threading

### Adding New Platforms

1. Update `CMakeLists.txt` with platform detection
2. Add platform-specific code in `CPP/Windows/` if needed
3. Test build and functionality
4. Add CI workflow in `.github/workflows/`

---

## Creating Releases

### Automated Release Process

Releases are created automatically via GitHub Actions.

**Method 1: Tag-based (Recommended)**
```bash
git tag v1.0.0
git push origin v1.0.0
```

**Method 2: Manual Trigger**
1. Go to Actions → "Build and Release"
2. Click "Run workflow"
3. Enter version number

### What Gets Built

Each release includes binaries for:
- Linux x86_64 (`.tar.gz`)
- Windows x64 (`.zip`)
- macOS x86_64 (`.tar.gz`)
- macOS ARM64 (`.tar.gz`)

Each package contains:
- Static library
- Headers (ParallelCompressAPI.h, ParallelCompressor.h)
- Example programs
- Documentation

### Version Numbering

Follow [Semantic Versioning](https://semver.org/):
- **Major** (1.0.0 → 2.0.0): Breaking changes
- **Minor** (1.0.0 → 1.1.0): New features, backward compatible
- **Patch** (1.0.0 → 1.0.1): Bug fixes

### Pre-Release Checklist

- [ ] All tests pass
- [ ] CI builds are green
- [ ] CHANGELOG.md updated
- [ ] Version follows semantic versioning
- [ ] Documentation reflects changes

---

## Architecture Overview

### Core Components

**Parallel Compression Engine**
- `ParallelCompressor.cpp` - Main compression coordinator
- `ParallelCompressAPI.cpp` - C API wrapper
- Thread pool with work queue
- Per-thread encoder instances

**Stream Interface**
```cpp
ISequentialInStream   // Sequential read
ISequentialOutStream  // Sequential write
IInStream            // Seekable input (extends ISequentialInStream)
```

**Memory Stream Support**

The library already supports compression from memory buffers:
```c
ParallelInputItemC item = {
    .Data = buffer,        // Memory buffer pointer
    .DataSize = size,      // Buffer size
    .Name = L"file.dat"    // Name in archive
};
```

This enables direct integration with cloud storage (Azure, AWS, GCP) by downloading blobs to memory and passing them to the compressor.

### Performance Characteristics

- **Speedup**: 10-30x on multi-core systems (measured)
- **Memory**: ~24MB per thread (LZMA level 5)
- **Scaling**: Linear up to CPU core count
- **Overhead**: <0.1% from safety checks

### Module Definition Files (.def)

Windows DLL exports are controlled by `.def` files:

| File | Purpose | Exports |
|------|---------|---------|
| ParallelCompress.def | Parallel compression C API | 12 functions |
| Codec.def | Standard codec interface | 7 functions |
| Archive.def | Archive format handlers | Multiple |
| LzmaLib.def | LZMA library | 2 functions |

These files are used by the Windows linker to create import libraries (.lib) for DLLs.

---

## Cloud Storage Integration

### Current Support

The library **already supports** compressing data from memory streams, which enables immediate cloud storage integration.

### Basic Integration

```cpp
#include "ParallelCompressAPI.h"
#include <azure/storage/blobs.hpp>  // Or AWS/GCP SDK

// 1. Download from cloud
std::vector<uint8_t> blobData;
azureBlobClient.DownloadTo(blobData);

// 2. Compress with existing API
ParallelInputItemC item = {
    .Data = blobData.data(),
    .DataSize = blobData.size(),
    .Name = L"data.bin"
};

ParallelCompressorHandle c = ParallelCompressor_Create();
ParallelCompressor_SetNumThreads(c, 8);
ParallelCompressor_SetCompressionLevel(c, 5);
ParallelCompressor_CompressMultiple(c, &item, 1, L"backup.7z");
ParallelCompressor_Destroy(c);
```

### Advanced: Custom Stream Adapter

For very large files or streaming scenarios, implement a custom `ISequentialInStream`:

```cpp
class CAzureBlobInStream : public ISequentialInStream {
    STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize) {
        // Download chunk on-demand from Azure
        // This avoids loading entire blob into memory
    }
};
```

See [AZURE_INTEGRATION.md](AZURE_INTEGRATION.md) for complete examples.

---

## CI/CD System

### Continuous Integration

Every push triggers builds and tests on:
- Linux: GCC + Clang (Debug + Release)
- Windows: x86 + x64 (Debug + Release)
- macOS: x86_64 + ARM64 (Debug + Release)

Workflow: `.github/workflows/ci.yml`

### Release Pipeline

Version tags trigger automated releases:
1. Build all platforms in parallel
2. Run tests
3. Package binaries
4. Create GitHub Release
5. Upload artifacts

Workflow: `.github/workflows/build-release.yml`

### Monitoring

- **Build Status**: Check Actions tab
- **Release Downloads**: View in Releases page
- **CI Logs**: Available for 90 days

---

## Contributing

### Code Style

- Follow existing code conventions
- Use platform abstractions (don't add platform-specific code without wrappers)
- Add tests for new features
- Update documentation

### Pull Request Process

1. Create feature branch
2. Make changes with tests
3. Ensure CI passes
4. Update CHANGELOG.md
5. Submit PR

### Testing

Run tests before submitting:
```bash
cd CPP/7zip/Compress
make -f makefile.test
./run_tests.sh
```

Or with CMake:
```bash
cd build
ctest --output-on-failure
```

---

## Troubleshooting

### Build Issues

**CMake configuration fails**:
- Check CMake version (3.15+)
- Verify compiler is installed
- Check error messages in output

**Link errors**:
- Ensure all source files exist
- Check library dependencies
- Verify platform-specific code

### Runtime Issues

**Compression fails**:
- Check input buffer validity
- Verify thread count (1-256)
- Check available memory

**Crashes**:
- Enable debug build
- Use valgrind (Linux) or AddressSanitizer
- Check thread safety

### CI/CD Issues

**Workflow fails**:
- Check workflow logs in Actions tab
- Verify all platforms build locally
- Review error messages

**Release not created**:
- Ensure tag matches `v*.*.*` pattern
- Check repository permissions
- Verify all build jobs succeeded

---

## References

- Main README: [../README.md](../README.md)
- Build System: [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
- Azure Integration: [AZURE_INTEGRATION.md](AZURE_INTEGRATION.md)
- Workflows: [../.github/workflows/README.md](../.github/workflows/README.md)
- 7-Zip Official: https://7-zip.org
- LZMA SDK: https://www.7-zip.org/sdk.html

---

**Last Updated**: 2026-01-03

For questions or issues, please open a GitHub issue.
