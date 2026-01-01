# Parallel Compressor Test Suite

Comprehensive test suite for the parallel multi-stream compression implementation.

## Test Categories

### 1. Unit Tests (`ParallelCompressorTest.cpp`)
Tests individual components and basic functionality:

- **TestBasicCompression** - Single stream compression with basic data
- **TestAutoDetection** - Automatic single/parallel mode detection
- **TestCompressionMethods** - Different compression algorithms (LZMA, LZMA2)
- **TestCAPI** - C API wrapper functionality

### 2. Integration Tests
Tests interaction between components:

- **TestMultipleStreams** - Concurrent compression of multiple streams
- **TestMemoryBuffer** - Compression from memory buffers via C API
- **TestFileCompression** - Real file I/O compression

### 3. Validation Tests (`ParallelCompressorValidation.cpp`)
End-to-end tests with verification:

- **TestFilesToArchive** - Compress multiple files to archive
- **TestMemoryBuffersToArchive** - Compress memory buffers to archive
- **TestLargeDataset** - Large-scale parallel compression (20 items × 64KB)

## Building Tests

### Using Make:
```bash
cd CPP/7zip/Compress
make -f makefile.test
```

### Manual Build:
```bash
g++ -O2 -Wall -D_FILE_OFFSET_BITS=64 -DNDEBUG \
    ParallelCompressorTest.cpp \
    ParallelCompressor.cpp \
    ParallelCompressAPI.cpp \
    ParallelCompressorRegister.cpp \
    [additional object files] \
    -lpthread -o ParallelCompressorTest
```

## Running Tests

### Quick Test:
```bash
./ParallelCompressorTest
```

### Full Test Suite:
```bash
./run_tests.sh
```

### Individual Test:
```bash
./ParallelCompressorValidation
```

## Test Output

Tests provide detailed output including:
- Test name and status (PASS/FAIL)
- Error messages for failures
- Statistics (passed/failed counts)
- Archive validation results

Example output:
```
===========================================
Parallel Compressor Test Suite
===========================================

Running Unit Tests...
-------------------------------------------
PASS: TestBasicCompression
PASS: TestAutoDetection
PASS: TestCompressionMethods
PASS: TestCAPI

Running Integration Tests...
-------------------------------------------
PASS: TestMultipleStreams
PASS: TestMemoryBuffer

Running End-to-End Tests...
-------------------------------------------
PASS: TestFileCompression

===========================================
Test Results
===========================================
Passed: 7
Failed: 0
Total:  7
===========================================
```

## Validation with 7z Command

The test suite can optionally validate compressed archives using the `7z` command:

```bash
7z t test_archive.7z
```

This verifies:
- Archive integrity
- Correct compression format
- Extractability of compressed data

## Test Files Created

During testing, the following files may be created:
- `test_input.txt` - Test input file
- `test_output.7z` - Compressed output
- `test_file1.txt`, `test_file2.txt`, `test_file3.txt` - Multi-file test data
- `test_archive.7z` - Multi-file archive
- `test_memory_archive.7z` - Memory buffer archive
- `test_large.7z` - Large dataset archive

## Troubleshooting

### Build Errors
- Ensure all dependencies are compiled
- Check include paths for headers
- Verify threading library is linked (`-lpthread`)

### Test Failures
- Check file permissions for test file creation
- Ensure sufficient disk space
- Verify threading support on platform

### Archive Validation Errors
- Install 7z command: `sudo apt-get install p7zip-full`
- Check archive format compatibility
- Verify compression method support

## Adding New Tests

To add new tests:

1. Add test function to `ParallelCompressorTest.cpp`:
```cpp
static bool TestNewFeature()
{
  g_TestFailed = false;
  
  // Test implementation
  TEST_ASSERT(condition, "Error message");
  
  TEST_SUCCESS();
}
```

2. Call from `main()`:
```cpp
TestNewFeature();
```

3. Rebuild and run tests

## CI/CD Integration

Tests can be integrated into CI/CD pipelines:

```yaml
- name: Build Tests
  run: make -f makefile.test

- name: Run Tests
  run: ./run_tests.sh
```

## Performance Testing

For performance benchmarks:
```bash
time ./ParallelCompressorValidation
```

Compare single-thread vs multi-thread performance by adjusting thread count.
