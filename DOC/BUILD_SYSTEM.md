# 7-Zip Parallel Compression Build System Documentation

## Overview

This document provides comprehensive information about the build system configuration for the 7-Zip parallel compression library, including how to build on both Windows and Linux platforms.

## Current Build Configuration

### Linux Build System (Makefiles)

The project uses GNU Make for Linux builds with several makefiles:

#### Primary Build Files

1. **CPP/7zip/Compress/makefile.test**
   - Purpose: Build test executables for parallel compression
   - Compiler: g++ (GCC)
   - Targets: Multiple test executables
   - Dependencies: pthread, LZMA, compression modules

2. **C/Util/7z/makefile.gcc**
   - Purpose: Build 7z decoder utility
   - Compiler: GCC
   - Output: 7zdec (decoder binary)

3. **C/Util/Lzma/makefile.gcc**
   - Purpose: Build LZMA library
   - Output: LZMA compression library

### Windows Build System (.def Files)

The project uses module definition (.def) files for Windows DLL exports:

#### Module Definition Files

1. **CPP/7zip/Compress/ParallelCompress.def**
   ```
   Purpose: Exports parallel compression C API
   Exports:
   - ParallelCompressor_Create/Destroy
   - ParallelCompressor_SetNumThreads
   - ParallelCompressor_SetCompressionLevel
   - ParallelCompressor_CompressMultiple
   - ParallelStreamQueue_* functions
   ```

2. **CPP/7zip/Compress/Codec.def**
   ```
   Purpose: Standard 7-Zip codec interface
   Exports:
   - CreateObject
   - GetNumberOfMethods
   - GetMethodProperty
   - CreateDecoder/CreateEncoder
   ```

3. **CPP/7zip/Archive/Archive.def** and **Archive2.def**
   ```
   Purpose: Archive format handlers
   Used by: 7-Zip archive processing modules
   ```

4. **C/Util/LzmaLib/LzmaLib.def**
   ```
   Purpose: LZMA library C API
   Exports:
   - LzmaCompress
   - LzmaUncompress
   ```

## Building on Linux

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential g++ make

# Fedora/RHEL
sudo dnf install gcc-c++ make

# Arch Linux
sudo pacman -S base-devel
```

### Build Steps

1. **Build Parallel Compression Tests**
   ```bash
   cd CPP/7zip/Compress
   make -f makefile.test clean
   make -f makefile.test
   ```

   This builds:
   - ParallelCompressorTest (unit tests)
   - ParallelE2ETest (end-to-end tests)
   - ParallelFeatureParityTest (feature validation)
   - ParallelIntegrationTest (integration tests)
   - ParallelSecurityTest (security tests)
   - ParallelSolidMultiVolumeTest (advanced features)

2. **Run Tests**
   ```bash
   ./run_tests.sh
   # or run individual tests:
   ./ParallelCompressorTest
   ./ParallelE2ETest
   ```

3. **Build 7z Decoder**
   ```bash
   cd C/Util/7z
   make -f makefile.gcc
   ```

### Build Configuration

The Linux makefile uses these compiler flags:
```makefile
CXXFLAGS = -O2 -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DNDEBUG
LDFLAGS = -lpthread
```

Key defines:
- `_FILE_OFFSET_BITS=64`: Large file support (>2GB)
- `_LARGEFILE_SOURCE`: Additional large file APIs
- `NDEBUG`: Release build (no assertions)

## Building on Windows

### Prerequisites

- Visual Studio 2015 or later (with C++ toolchain)
- Windows SDK
- NMAKE (included with Visual Studio)

### Build Steps (Command Line)

1. **Open Developer Command Prompt for Visual Studio**

2. **Navigate to project directory**
   ```cmd
   cd CPP\7zip\Compress
   ```

3. **Build using NMAKE** (if makefile exists)
   ```cmd
   nmake /f makefile
   ```

### Build Steps (Visual Studio)

1. Open `.sln` solution file (if available) in Visual Studio
2. Select configuration (Debug/Release, x86/x64)
3. Build → Build Solution

### Module Definition Files

The .def files are used during the link phase to control DLL exports:

```cmd
link /DLL /DEF:ParallelCompress.def /OUT:ParallelCompress.dll [object files...]
```

This creates:
- ParallelCompress.dll (runtime library)
- ParallelCompress.lib (import library)

## Project Structure

```
7zip/
├── C/                          # C implementation
│   ├── Util/
│   │   ├── 7z/                # 7z decoder utility
│   │   │   ├── makefile       # Windows build
│   │   │   └── makefile.gcc   # Linux build
│   │   ├── Lzma/              # LZMA utility
│   │   └── LzmaLib/           # LZMA library
│   │       └── LzmaLib.def    # Windows DLL exports
│   ├── Alloc.c                # Memory allocation
│   ├── LzFind.c               # LZMA find
│   ├── LzmaEnc.c              # LZMA encoder
│   └── Threads.c              # Threading primitives
│
├── CPP/                       # C++ implementation
│   ├── Common/                # Common utilities
│   │   ├── MyString.cpp       # String handling
│   │   ├── MyVector.cpp       # Vector container
│   │   └── MyWindows.cpp      # Windows compatibility
│   ├── Windows/               # Windows-specific code
│   │   ├── FileIO.cpp         # File I/O
│   │   ├── Thread.cpp         # Thread wrapper
│   │   └── Synchronization.cpp # Sync primitives
│   └── 7zip/
│       ├── Common/            # 7z common code
│       │   ├── StreamUtils.cpp
│       │   ├── FileStreams.cpp
│       │   └── StreamObjects.cpp
│       ├── Compress/          # Compression codecs
│       │   ├── ParallelCompressor.cpp
│       │   ├── ParallelCompressAPI.cpp
│       │   ├── ParallelCompress.def    # Windows exports
│       │   ├── Codec.def               # Codec exports
│       │   ├── LzmaEncoder.cpp
│       │   ├── Lzma2Encoder.cpp
│       │   ├── makefile.test           # Linux test build
│       │   └── run_tests.sh
│       └── Archive/           # Archive formats
│           ├── 7z/            # 7z format
│           ├── Archive.def    # Windows exports
│           └── Archive2.def
│
├── Asm/                       # Assembly optimizations
├── DOC/                       # Documentation
├── README.md                  # Project overview
└── CHANGELOG.md              # Change log
```

## Cross-Platform Considerations

### Current Status

- **Linux**: Fully supported via GNU Make
- **Windows**: Supported via NMAKE/.def files and Visual Studio
- **Build System**: Platform-specific (separate makefiles)

### Platform Differences

1. **File I/O**
   - Windows: Uses Windows API (CreateFile, ReadFile, WriteFile)
   - Linux: POSIX APIs wrapped in compatibility layer

2. **Threading**
   - Windows: Native Windows threads
   - Linux: pthreads (POSIX threads)

3. **Synchronization**
   - Windows: Critical sections, Events
   - Linux: pthread mutexes, condition variables

4. **Path Separators**
   - Windows: Backslash (\)
   - Linux: Forward slash (/)

### Compatibility Layer

The project includes compatibility abstractions:
- `CPP/Common/MyWindows.cpp`: Windows API emulation on Unix
- `CPP/Windows/*`: Platform abstraction layer
- Type definitions in `Common/MyTypes.h`

## Stream Interface

### Core Stream Types

The library uses COM-like stream interfaces defined in `CPP/7zip/IStream.h`:

1. **ISequentialInStream**: Sequential read stream
   ```cpp
   STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize)
   ```

2. **ISequentialOutStream**: Sequential write stream
   ```cpp
   STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize)
   ```

3. **IInStream**: Seekable input stream (extends ISequentialInStream)
   ```cpp
   STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
   ```

4. **IOutStream**: Seekable output stream (extends ISequentialOutStream)
   ```cpp
   STDMETHOD(SetSize)(UInt64 newSize)
   ```

### Stream Implementations

Available in `CPP/7zip/Common/StreamObjects.h`:

1. **CBufferInStream**: Read from memory buffer
2. **CBufInStream**: Read from byte array with reference counting
3. **CDynBufSeqOutStream**: Write to dynamic buffer
4. **FileStreams**: File-based streams (in FileStreams.h)

### Memory Stream Support

The parallel compressor already supports memory streams:

```cpp
// C++ API - Memory buffer input
CMyComPtr<ISequentialInStream> stream;
Create_BufInStream_WithNewBuffer(data, size, &stream);

// C API - Direct memory input
ParallelInputItemC item;
item.Data = bufferPtr;
item.DataSize = bufferSize;
item.Name = L"filename";
```

## Next Steps for Cross-Platform Enhancement

### Recommended Improvements

1. **Unified Build System**
   - Add CMakeLists.txt for cross-platform builds
   - Support: Linux, Windows, macOS
   - Benefits: Single build configuration, IDE integration

2. **Enhanced Stream Support**
   - Azure Blob Storage stream adapter
   - Network stream implementations
   - Custom stream callback interface

3. **Build Automation**
   - CI/CD integration (GitHub Actions)
   - Automated testing on multiple platforms
   - Binary distribution packaging

4. **Documentation**
   - Platform-specific build guides
   - Stream interface usage examples
   - Cloud storage integration examples

## References

- 7-Zip Official Site: https://7-zip.org
- LZMA SDK: https://www.7-zip.org/sdk.html
- Project README: ../README.md
- Changelog: ../CHANGELOG.md
