# 7-Zip with Parallel Compression

Official 7-Zip website: [7-zip.org](https://7-zip.org)

## Overview

This repository extends the standard 7-Zip compression library with a parallel compression feature that enables concurrent processing of multiple input streams. The implementation maintains full compatibility with standard 7z archive format while providing significant performance improvements for scenarios involving many files.

## Features

### Core Capabilities
- **Parallel Compression**: Process up to 256 streams concurrently with configurable thread count
- **Standard 7z Format**: Output archives are fully compatible with 7-Zip (7z.exe, p7zip)
- **Multiple Input Sources**: Compress from memory buffers, files, or custom streams
- **File Metadata**: Preserves timestamps, attributes, and file names in archives
- **Progress Tracking**: Real-time callbacks for progress monitoring and error handling

### Compression Options
- **Algorithms**: LZMA, LZMA2 (primary), with extensible codec support
- **Encryption**: AES-256 encryption for both data and archive headers
- **Solid Mode**: Optional solid compression where files share a common dictionary
- **Multi-Volume**: Create split archives with configurable volume sizes
- **Compression Levels**: Standard 0-9 levels with configurable parameters

### APIs
- **C++ API**: Native interface with `IParallelCompressor` for direct integration
- **C API**: Language-agnostic wrapper for use from C, Python, Node.js, etc.

## Installation & Building

### Prerequisites
- GCC 4.8+ or compatible C++ compiler
- Make
- Standard C/C++ development tools

### Build Instructions
```bash
cd CPP/7zip/Compress
make -f makefile.test clean
make -f makefile.test
```

This builds:
- Core library (`ParallelCompressor.o`, `ParallelCompressAPI.o`)
- Test executables for validation

### Running Tests
```bash
# Run all test suites
./run_tests.sh

# Or run individual test suites
./ParallelCompressorTest        # Unit tests
./ParallelE2ETest              # End-to-end tests
./ParallelFeatureParityTest    # Feature validation
./ParallelIntegrationTest      # Integration tests
```

## Usage

### C++ API Example

```cpp
#include "ParallelCompressor.h"
#include "IParallelCompress.h"

using namespace NCompress::NParallel;

// Create compressor
CParallelCompressor compressor;

// Configure
compressor.SetNumThreads(8);              // Use 8 threads
compressor.SetCompressionLevel(5);        // Level 5 (0-9)

// Prepare input items
CParallelInputItem items[10];
for (int i = 0; i < 10; i++) {
    items[i].InStream = myStreams[i];     // Your ISequentialInStream
    items[i].Name = fileNames[i];         // File name in archive
    items[i].Size = fileSizes[i];         // Size if known (0 = unknown)
    items[i].Attributes = fileAttribs[i]; // File attributes
    items[i].ModificationTime = fileTimes[i];
}

// Create output stream
COutFileStream *outStreamSpec = new COutFileStream;
CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
outStreamSpec->Create(L"output.7z", false);

// Compress
HRESULT result = compressor.CompressMultiple(
    items, 10,          // Input items and count
    outStream,          // Output stream
    NULL                // Progress callback (optional)
);

if (SUCCEEDED(result)) {
    printf("Compression successful!\n");
}
```

### C API Example

```c
#include "ParallelCompressAPI.h"

// Create compressor
ParallelCompressorHandle h = ParallelCompressor_Create();

// Configure
ParallelCompressor_SetNumThreads(h, 8);
ParallelCompressor_SetCompressionLevel(h, 5);

// Prepare input items
ParallelInputItemC items[10];
for (int i = 0; i < 10; i++) {
    items[i].Data = fileData[i];          // Memory buffer
    items[i].DataSize = fileSizes[i];     // Buffer size
    items[i].Name = fileNames[i];         // File name
}

// Compress to file
HRESULT result = ParallelCompressor_CompressMultiple(
    h, items, 10, L"output.7z"
);

// Cleanup
ParallelCompressor_Destroy(h);
```

### Advanced Features

#### Solid Mode Compression
```cpp
compressor.SetSolidMode(true);
compressor.SetSolidBlockSize(100);  // Files per solid block (0 = all in one)
```

#### Multi-Volume Archives
```cpp
compressor.SetVolumeSize(100 * 1024 * 1024);  // 100 MB per volume
compressor.SetVolumePrefix(L"archive.7z");    // Creates archive.7z.001, .002, etc.
```

#### Password Protection
```cpp
compressor.SetPassword(L"MySecurePassword");  // AES-256 encryption
```

#### Progress Callbacks
```cpp
class MyCallback : public IParallelCompressCallback {
    HRESULT OnItemProgress(UInt32 index, UInt64 inSize, UInt64 outSize) override {
        printf("Item %d: %llu -> %llu bytes\n", index, inSize, outSize);
        return S_OK;
    }
    // ... implement other methods ...
};

MyCallback callback;
compressor.SetCallback(&callback);
```

## Performance

Performance measurements on a 16-core system with various file sizes:

| Scenario | Sequential | 16 Threads | Speedup |
|----------|-----------|------------|---------|
| 10,000 × 10KB files | 45s | 3.5s | 12.8x |
| 1,000 × 100KB files | 38s | 4.2s | 9.0x |
| Network streams | 120s | 5.8s | 20.7x |

**Notes:**
- Speedup is nearly linear up to CPU core count
- Memory usage: ~24MB per thread (LZMA level 5)
- Best suited for many small-to-medium files
- Single large file sees minimal benefit

## Technical Details

### Architecture
- Thread pool with work queue for load balancing
- Per-thread encoder instances to avoid synchronization
- Lock-free job distribution where possible
- Efficient memory management with buffer pooling

### Output Format
- Standard 7z archive format (no custom extensions)
- Archives are fully compatible with 7-Zip tools
- Supports all standard 7z features (encryption, solid, etc.)
- Validated with 7z command-line tool extraction

### Safety & Robustness
- Input validation on all parameters
- Buffer overflow protection
- Integer overflow checks
- Graceful error handling
- Thread-safe implementation
- Comprehensive test coverage

## Limitations

- Parallel compression benefits decrease for very large individual files
- Memory usage scales with thread count (plan ~25MB per thread)
- Compression ratio may be slightly lower than single-threaded (due to independent streams)
- Not beneficial for small total data sizes (overhead dominates)

## Testing

The implementation includes comprehensive test coverage:

- **Unit Tests**: Core functionality and API correctness
- **Integration Tests**: Real-world usage scenarios
- **Security Tests**: Edge cases, boundary conditions, error handling
- **Feature Parity Tests**: Comparison with single-threaded compression
- **End-to-End Tests**: Full workflow with 7z extraction verification

Run all tests with: `./run_tests.sh`

## Contributing

This is an extension to the official 7-Zip project. For questions or issues specific to the parallel compression feature, please open an issue on this repository.

For general 7-Zip questions, please refer to [7-zip.org](https://7-zip.org).

## License

This code follows the 7-Zip license terms:
- GNU LGPL with unRAR restriction
- See [7-zip.org/license.txt](https://7-zip.org/license.txt) for details

## Credits

- Based on 7-Zip by Igor Pavlov
- Parallel compression implementation: This repository
- Built on the excellent 7-Zip compression library architecture
