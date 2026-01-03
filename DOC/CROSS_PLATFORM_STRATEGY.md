# Cross-Platform Strategy for 7-Zip Parallel Compression

## Executive Summary

This document outlines a comprehensive strategy to enhance the cross-platform capabilities of the 7-Zip parallel compression library, with focus on:
1. Unified build system using CMake
2. Enhanced stream interface for cloud storage (Azure, AWS, GCP)
3. Platform-agnostic API design
4. Comprehensive testing across platforms

## Current State Analysis

### Strengths
- ✅ Already works on Linux (GNU Make + GCC)
- ✅ Platform abstraction layer exists (MyWindows.cpp, Windows/*)
- ✅ Memory stream support implemented
- ✅ Clean separation of C and C++ APIs
- ✅ Standard 7z format (no proprietary extensions)

### Limitations
- ❌ No unified build system (separate makefiles for Windows/Linux)
- ❌ No direct cloud storage stream support
- ❌ Windows-centric API design (FILETIME, HRESULT, wchar_t)
- ❌ Limited stream adapter examples
- ❌ No macOS build configuration

## Proposed Solutions

### 1. CMake Build System

#### Benefits
- Single build configuration for all platforms
- IDE integration (Visual Studio, CLion, Xcode)
- Automatic dependency detection
- Simplified CI/CD setup
- Package generation (DEB, RPM, MSI)

#### Implementation Plan

**CMakeLists.txt Structure:**
```cmake
cmake_minimum_required(VERSION 3.15)
project(7zip-parallel VERSION 1.0.0 LANGUAGES C CXX)

# Platform detection
if(WIN32)
    # Windows-specific settings
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
elseif(APPLE)
    # macOS-specific settings
endif()

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Core library
add_library(7zip-parallel
    # Source files
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

**Directory Structure:**
```
cmake/
├── FindLZMA.cmake          # Find system LZMA
├── Platform.cmake          # Platform-specific config
└── Dependencies.cmake      # External dependencies

CMakeLists.txt              # Root CMake file
CPP/7zip/Compress/CMakeLists.txt  # Compression module
C/CMakeLists.txt            # C library
tests/CMakeLists.txt        # Test suite
```

### 2. Cloud Storage Stream Adapters

#### Azure Blob Storage Integration

**Design:**
```cpp
// AzureBlobStream.h
class CAzureBlobInStream :
    public ISequentialInStream,
    public CMyUnknownImp
{
    Z7_COM_UNKNOWN_IMP_1(ISequentialInStream)
    Z7_IFACE_COM7_IMP(ISequentialInStream)

private:
    std::string _connectionString;
    std::string _containerName;
    std::string _blobName;
    uint64_t _position;
    uint64_t _size;
    
    // Azure SDK client (optional, can use REST API)
    void* _blobClient;  // Azure::Storage::Blobs::BlobClient*

public:
    HRESULT Initialize(const char* connectionString, 
                      const char* container, 
                      const char* blob);
    HRESULT ReadChunk(void* buffer, size_t size, size_t* bytesRead);
};

// Usage
CAzureBlobInStream* stream = new CAzureBlobInStream();
stream->Initialize(connectionString, "mycontainer", "myblob");

CParallelInputItem item;
item.InStream = stream;
item.Name = L"blob-data.bin";
```

**C API Extension:**
```c
// Add to ParallelCompressAPI.h
typedef struct
{
    const char* ConnectionString;
    const char* ContainerName;
    const char* BlobName;
    const wchar_t* LocalName;  // Name in archive
} ParallelInputItemAzureBlob;

HRESULT ParallelCompressor_CompressFromAzureBlobs(
    ParallelCompressorHandle handle,
    ParallelInputItemAzureBlob* items,
    UInt32 numItems,
    const wchar_t* outputPath);
```

#### Generic Network Stream

**Design:**
```cpp
// NetworkStream.h
class CNetworkInStream :
    public ISequentialInStream,
    public CMyUnknownImp
{
    Z7_COM_UNKNOWN_IMP_1(ISequentialInStream)
    Z7_IFACE_COM7_IMP(ISequentialInStream)

private:
    // Callback for reading data
    typedef size_t (*ReadCallback)(void* context, void* buffer, size_t size);
    
    ReadCallback _readCallback;
    void* _context;
    uint64_t _totalRead;

public:
    void SetReadCallback(ReadCallback callback, void* context);
};

// Usage example - HTTP stream
size_t HttpReadCallback(void* context, void* buffer, size_t size)
{
    HttpContext* http = (HttpContext*)context;
    return http_read(http->connection, buffer, size);
}

CNetworkInStream* stream = new CNetworkInStream();
stream->SetReadCallback(HttpReadCallback, &httpContext);
```

### 3. Platform-Agnostic API Extensions

#### Cross-Platform Time Representation
```cpp
// Instead of FILETIME (Windows-specific)
struct ParallelFileTime
{
    uint64_t seconds;      // Unix timestamp
    uint32_t nanoseconds;  // Nanosecond precision
};

// Conversion utilities
ParallelFileTime FromFiletime(const FILETIME& ft);
ParallelFileTime FromTimespec(const struct timespec& ts);
FILETIME ToFiletime(const ParallelFileTime& pft);
struct timespec ToTimespec(const ParallelFileTime& pft);
```

#### Unicode String Handling
```cpp
// Add UTF-8 API variants alongside wchar_t
HRESULT ParallelCompressor_CompressMultipleU8(
    ParallelCompressorHandle handle,
    ParallelInputItemC* items,    // Use UTF-8 strings
    UInt32 numItems,
    const char* outputPathUtf8);   // UTF-8 path

// Internally convert to wchar_t on Windows, use directly on Unix
```

### 4. Stream Adapter Library

**Proposed Structure:**
```
CPP/7zip/Common/StreamAdapters/
├── AzureBlobStream.h/.cpp       # Azure Blob Storage
├── AwsS3Stream.h/.cpp           # AWS S3
├── GcpStorageStream.h/.cpp      # Google Cloud Storage
├── HttpStream.h/.cpp            # Generic HTTP/HTTPS
├── FtpStream.h/.cpp             # FTP/SFTP
├── MemoryMappedStream.h/.cpp    # Memory-mapped files
└── CallbackStream.h/.cpp        # Custom callbacks

examples/
├── CompressFromAzure.cpp        # Azure example
├── CompressFromHttp.cpp         # HTTP example
└── CompressFromCustom.cpp       # Custom stream example
```

### 5. Build and Test Matrix

#### Continuous Integration (GitHub Actions)

```yaml
# .github/workflows/build-test.yml
name: Build and Test

on: [push, pull_request]

jobs:
  build-linux:
    strategy:
      matrix:
        os: [ubuntu-20.04, ubuntu-22.04]
        compiler: [gcc-9, gcc-11, clang-12]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v3
      - name: Build with CMake
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_CXX_COMPILER=${{ matrix.compiler }}
          cmake --build .
      - name: Run tests
        run: ctest --output-on-failure

  build-windows:
    strategy:
      matrix:
        arch: [x64, x86, ARM64]
        config: [Debug, Release]
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with CMake
        run: |
          mkdir build && cd build
          cmake .. -A ${{ matrix.arch }}
          cmake --build . --config ${{ matrix.config }}
      - name: Run tests
        run: ctest -C ${{ matrix.config }}

  build-macos:
    strategy:
      matrix:
        arch: [x86_64, arm64]
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with CMake
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_OSX_ARCHITECTURES=${{ matrix.arch }}
          cmake --build .
      - name: Run tests
        run: ctest
```

## Implementation Roadmap

### Phase 1: Build System Unification (2-3 weeks)
- [ ] Create root CMakeLists.txt
- [ ] Add platform detection and configuration
- [ ] Migrate existing makefiles to CMake
- [ ] Test builds on Linux, Windows, macOS
- [ ] Update documentation

### Phase 2: Stream Interface Enhancement (2-3 weeks)
- [ ] Design abstract stream adapter interface
- [ ] Implement callback-based stream adapter
- [ ] Create example adapters (HTTP, Azure mock)
- [ ] Add C API extensions for stream callbacks
- [ ] Write comprehensive examples

### Phase 3: Cloud Storage Integration (3-4 weeks)
- [ ] Azure Blob Storage adapter (full implementation)
- [ ] AWS S3 adapter (full implementation)
- [ ] Google Cloud Storage adapter
- [ ] Authentication handling
- [ ] Error handling and retry logic
- [ ] Performance optimization (chunked reads, caching)

### Phase 4: Testing and Documentation (2 weeks)
- [ ] Add stream adapter unit tests
- [ ] Add integration tests with mock cloud services
- [ ] Performance benchmarks
- [ ] Update API documentation
- [ ] Create usage guides for each stream type
- [ ] Add troubleshooting guide

### Phase 5: CI/CD and Distribution (1-2 weeks)
- [ ] Set up GitHub Actions workflows
- [ ] Add multi-platform build matrix
- [ ] Create binary distribution packages
- [ ] Publish to package managers (apt, vcpkg, conan)

## Technical Specifications

### Stream Adapter Interface

```cpp
// IStreamAdapter.h - Generic stream adapter interface
class IStreamAdapter
{
public:
    virtual ~IStreamAdapter() {}
    
    // Open/connect to stream source
    virtual HRESULT Open(const char* uri, void* options) = 0;
    
    // Read data (blocking)
    virtual HRESULT Read(void* buffer, size_t size, size_t* bytesRead) = 0;
    
    // Get stream metadata
    virtual HRESULT GetSize(uint64_t* size) = 0;
    virtual HRESULT GetLastModified(uint64_t* timestamp) = 0;
    
    // Check if seekable
    virtual bool IsSeekable() const = 0;
    
    // Seek (if supported)
    virtual HRESULT Seek(int64_t offset, int origin, uint64_t* newPosition) = 0;
    
    // Close stream
    virtual void Close() = 0;
};

// Factory function
typedef IStreamAdapter* (*StreamAdapterFactory)(const char* uri, void* options);

// Registry
void RegisterStreamAdapter(const char* scheme, StreamAdapterFactory factory);
IStreamAdapter* CreateStreamAdapter(const char* uri, void* options);
```

### Configuration File Support

```json
// compress_config.json
{
  "compression": {
    "threads": 8,
    "level": 5,
    "method": "LZMA2",
    "password": "${env:ARCHIVE_PASSWORD}"
  },
  "output": {
    "path": "backup.7z",
    "volumeSize": "100MB",
    "solid": true
  },
  "inputs": [
    {
      "type": "azure-blob",
      "connectionString": "${env:AZURE_STORAGE_CONNECTION_STRING}",
      "container": "backups",
      "blob": "data/*.log"
    },
    {
      "type": "http",
      "url": "https://example.com/data.bin",
      "headers": {
        "Authorization": "Bearer ${env:API_TOKEN}"
      }
    },
    {
      "type": "file",
      "path": "/local/data/*.txt"
    }
  ]
}
```

## Security Considerations

### Authentication
- Support for Azure Managed Identity
- AWS IAM roles and temporary credentials
- OAuth 2.0 for HTTP endpoints
- Environment variable injection (avoid hardcoded secrets)

### Data Protection
- TLS/SSL for network streams (mandatory)
- Certificate validation
- Proxy support (HTTP_PROXY environment variable)
- Timeout configuration (prevent hanging)

### Input Validation
- URL/URI parsing and validation
- Maximum stream size limits
- Rate limiting for cloud APIs
- Error handling for network failures

## Performance Optimization

### Network Stream Buffering
```cpp
class CBufferedNetworkStream
{
private:
    static const size_t BUFFER_SIZE = 1024 * 1024; // 1MB
    uint8_t _buffer[BUFFER_SIZE];
    size_t _bufferPos;
    size_t _bufferSize;
    IStreamAdapter* _adapter;

public:
    HRESULT Read(void* data, size_t size, size_t* bytesRead);
    // Implements read-ahead buffering
};
```

### Parallel Downloads
- Multiple concurrent cloud stream reads
- Chunk-based parallel downloading
- Dynamic thread allocation based on network speed

### Caching Strategy
- Local cache for frequently accessed blobs
- Cache invalidation based on ETag/Last-Modified
- Configurable cache size and location

## Compatibility Matrix

| Platform | Architecture | Compiler | Status |
|----------|-------------|----------|--------|
| Linux    | x86_64      | GCC 9+   | ✅ Supported |
| Linux    | x86_64      | Clang 10+ | ✅ Supported |
| Linux    | ARM64       | GCC 9+   | 🟡 Planned |
| Windows  | x64         | MSVC 2019+ | ✅ Supported |
| Windows  | x86         | MSVC 2019+ | ✅ Supported |
| Windows  | ARM64       | MSVC 2019+ | 🟡 Planned |
| macOS    | x86_64      | Clang    | 🟡 Planned |
| macOS    | ARM64 (M1+) | Clang    | 🟡 Planned |

## Dependencies

### Core Dependencies (existing)
- pthreads (Linux/macOS)
- Windows API (Windows)
- C++11 compiler minimum

### Optional Dependencies (for cloud support)
- Azure Storage SDK for C++ (Azure Blob)
- AWS SDK for C++ (S3)
- Google Cloud C++ SDK (GCS)
- libcurl (generic HTTP/HTTPS)
- OpenSSL (TLS/SSL, if not using system libraries)

### Dependency Management
- Use CMake FetchContent for optional dependencies
- Provide pkg-config files for system integration
- Support vcpkg and conan package managers

## Testing Strategy

### Unit Tests
- Stream adapter interface tests
- Platform abstraction layer tests
- Compression/decompression correctness
- Error handling and edge cases

### Integration Tests
- End-to-end compression from cloud sources
- Multi-platform compatibility tests
- Performance benchmarks
- Memory leak detection (valgrind, AddressSanitizer)

### Mock Services
- Mock Azure Blob Storage (in-memory)
- Mock S3 service (localstack)
- Mock HTTP server (test fixtures)

## Documentation Plan

### User Documentation
1. **Getting Started Guide**
   - Installation on each platform
   - First compression example
   - Common use cases

2. **API Reference**
   - C++ API documentation (Doxygen)
   - C API documentation
   - Stream adapter interface

3. **Cloud Integration Guides**
   - Azure Blob Storage setup
   - AWS S3 setup
   - Google Cloud Storage setup
   - Custom stream adapters

4. **Build Guide**
   - CMake configuration options
   - Platform-specific instructions
   - Troubleshooting

### Developer Documentation
1. **Architecture Overview**
   - Component diagram
   - Stream processing pipeline
   - Threading model

2. **Contribution Guide**
   - Code style guidelines
   - Testing requirements
   - Pull request process

3. **Porting Guide**
   - Adding new stream adapters
   - Platform-specific optimizations
   - Codec integration

## Success Metrics

### Functional Goals
- ✅ Build successfully on Linux, Windows, macOS
- ✅ Compress from Azure Blob Storage
- ✅ Compress from AWS S3
- ✅ Compress from HTTP/HTTPS sources
- ✅ All existing tests pass on all platforms

### Performance Goals
- Compression from cloud: <10% overhead vs local files
- Network buffering: >50MB/s throughput on 100Mbps link
- Memory usage: <50MB per thread + network buffers

### Quality Goals
- Test coverage: >85%
- Zero memory leaks (valgrind clean)
- Zero undefined behavior (UBSan clean)
- Documentation completeness: 100% of public APIs

## Conclusion

This strategy provides a clear path to making the 7-Zip parallel compression library truly cross-platform with excellent support for cloud storage streams. The phased approach allows for incremental delivery while maintaining stability and compatibility with existing code.

Key advantages of this approach:
1. **Backward Compatibility**: Existing code continues to work
2. **Incremental Adoption**: Users can adopt new features gradually
3. **Extensibility**: Easy to add new stream adapters
4. **Platform Independence**: True "write once, build anywhere"
5. **Modern Tooling**: CMake, CI/CD, package managers

## References
- CMake Documentation: https://cmake.org/documentation/
- Azure Storage SDK: https://github.com/Azure/azure-sdk-for-cpp
- AWS SDK for C++: https://github.com/aws/aws-sdk-cpp
- Google Cloud C++ SDK: https://github.com/googleapis/google-cloud-cpp
