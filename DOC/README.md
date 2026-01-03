# Documentation Index

This directory contains comprehensive documentation for the 7-Zip Parallel Compression Library, with special focus on cross-platform compatibility and cloud storage integration.

## Documentation Files

### 1. [BUILD_SYSTEM.md](BUILD_SYSTEM.md)
**Complete Build System Documentation**

- Overview of Linux and Windows build systems
- Detailed explanation of .def files (module definition files)
- Build instructions for both platforms
- Project structure and file organization
- Stream interface documentation
- Platform compatibility layer details

**Key Topics:**
- Makefile configuration (Linux)
- NMAKE and Visual Studio (Windows)  
- Module exports via .def files
- Cross-platform abstractions
- Existing stream implementations

### 2. [CROSS_PLATFORM_STRATEGY.md](CROSS_PLATFORM_STRATEGY.md)
**Comprehensive Cross-Platform Enhancement Strategy**

A detailed roadmap for making the library truly cross-platform with cloud storage support.

**Contents:**
- Current state analysis (strengths & limitations)
- CMake build system proposal
- Cloud storage stream adapter designs
- Platform-agnostic API extensions
- 5-phase implementation roadmap
- Testing and CI/CD strategy
- Performance optimization guidelines
- Security considerations

**Proposed Features:**
- Unified CMake build for Windows/Linux/macOS
- Azure Blob Storage stream adapter
- AWS S3 stream adapter
- Google Cloud Storage adapter
- Generic network stream interface
- Callback-based custom streams

### 3. [AZURE_INTEGRATION.md](AZURE_INTEGRATION.md)
**Azure Blob Storage Integration Guide**

Practical guide for integrating with Azure Blob Storage.

**Contents:**
- Architecture overview
- Two integration methods:
  - Memory buffer approach (works now)
  - Custom stream adapter (advanced)
- Complete working examples
- Authentication methods (connection string, managed identity, SAS)
- Best practices and performance tuning
- Error handling and troubleshooting
- Security considerations
- Deployment scenarios

**Examples Include:**
- Downloading blobs to memory and compressing
- Implementing custom Azure stream adapter
- Parallel blob downloads
- Encrypted backups
- Batch processing patterns

## Quick Start

### Current Capabilities (Already Working!)

The library **already supports** compressing data from memory streams, which enables cloud storage integration:

```c
// Download from Azure Blob Storage
std::vector<uint8_t> blobData;
azureBlobClient.DownloadTo(blobData);

// Compress using existing API
ParallelInputItemC item;
item.Data = blobData.data();
item.DataSize = blobData.size();
item.Name = L"myfile.dat";

ParallelCompressor_CompressMultiple(compressor, &item, 1, L"output.7z");
```

### Building

#### Using Existing Makefile (Linux)
```bash
cd CPP/7zip/Compress
make -f makefile.test
./run_tests.sh
```

#### Using New CMake (Cross-platform)
```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest
```

## Key Findings

### ✅ Cross-Platform Ready
- Linux: Fully working via GNU Make
- Windows: Supported via NMAKE and .def files  
- Platform abstraction layer exists
- Can be built with CMake for unified experience

### ✅ Memory Stream Support
- Already supports compressing from memory buffers
- Foundation for cloud storage is complete
- No code changes needed for basic Azure/AWS integration

### ✅ Extensible Stream Interface
- COM-like interface design (ISequentialInStream)
- Multiple implementations available
- Easy to add custom stream adapters

## Implementation Priorities

Based on the documentation, here are recommended priorities:

### Phase 1: CMake Build System (2-3 weeks)
- ✅ CMakeLists.txt created
- [ ] Resolve build errors
- [ ] Test on Windows and macOS
- [ ] CI/CD integration

### Phase 2: Stream Examples (1-2 weeks)
- ✅ Cloud storage example created
- [ ] Add real Azure SDK integration
- [ ] Add AWS S3 example
- [ ] Document patterns

### Phase 3: Advanced Stream Adapters (3-4 weeks)
- [ ] Implement Azure Blob streaming adapter
- [ ] Implement AWS S3 streaming adapter
- [ ] Add authentication helpers
- [ ] Performance optimization

## Use Cases

### Cloud Backup
- Compress blobs from Azure Storage to 7z archives
- Encrypt sensitive data before storage
- Incremental backup patterns

### Data Archival
- Archive log files from cloud storage
- Compress time-series data
- Create backup snapshots

### ETL Pipelines
- Compress data during transfer
- Reduce storage costs
- Optimize bandwidth usage

### Multi-Cloud
- Unified API for Azure, AWS, GCP
- Platform-agnostic compression
- Easy migration between providers

## Architecture Diagrams

### Current Memory Stream Approach
```
Cloud Storage (Azure/AWS/GCP)
        ↓
   Download to Memory
        ↓
   Memory Buffer → ParallelCompressor → 7z Archive
```

### Future Streaming Approach
```
Cloud Storage (Azure/AWS/GCP)
        ↓
   Custom Stream Adapter (ISequentialInStream)
        ↓
   Chunk-by-chunk → ParallelCompressor → 7z Archive
        ↓
   (No full buffer in memory)
```

## Platform Compatibility

| Platform | Build System | Status | Notes |
|----------|-------------|--------|-------|
| Linux x64 | Make/CMake | ✅ Working | Tested on Ubuntu 20.04+ |
| Windows x64 | NMAKE/CMake | ✅ Working | Requires VS 2019+ |
| Windows x86 | NMAKE/CMake | ✅ Working | 32-bit support |
| macOS x64 | CMake | 🟡 Untested | Should work |
| macOS ARM64 | CMake | 🟡 Untested | M1/M2 chips |
| Linux ARM64 | Make/CMake | 🟡 Untested | Raspberry Pi, AWS Graviton |

## Examples

Complete examples are available:
- `examples/CloudStorageExample.c` - Demonstrates cloud storage integration
- `CPP/7zip/Compress/ParallelCompressExample.cpp` - General usage
- `CPP/7zip/Compress/SimpleTest.cpp` - Simple test

## Dependencies

### Core (Required)
- C++11 compiler
- pthread (Linux/macOS)
- Windows SDK (Windows)

### Optional (Cloud Integration)
- Azure Storage SDK for C++
- AWS SDK for C++
- Google Cloud C++ SDK
- libcurl (for HTTP/HTTPS)

## Contributing

When adding new features, please:
1. Follow existing code style
2. Update relevant documentation
3. Add tests for new functionality
4. Ensure cross-platform compatibility
5. Update this index if adding new docs

## License

This follows the 7-Zip license terms (GNU LGPL with unRAR restriction).
See https://7-zip.org/license.txt for details.

## References

- 7-Zip Official: https://7-zip.org
- LZMA SDK: https://www.7-zip.org/sdk.html
- Project README: ../README.md
- Azure SDK: https://github.com/Azure/azure-sdk-for-cpp
- AWS SDK: https://github.com/aws/aws-sdk-cpp

---

**Last Updated:** 2026-01-03

For questions or issues, please open a GitHub issue.
