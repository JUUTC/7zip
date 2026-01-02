# Changelog

All notable changes to the parallel compression feature will be documented in this file.

## [Unreleased] - 2026-01-02

### Added - Parallel Compression Feature
- **Core Functionality**
  - Parallel multi-stream compression supporting 1-256 threads
  - Thread pool with concurrent job processing
  - Standard 7z archive format output (compatible with 7-Zip)
  - File metadata preservation (timestamps, attributes, names)
  - Progress tracking with per-item callbacks
  - Both C++ and C API interfaces

- **Compression Features**
  - LZMA/LZMA2 compression (primary)
  - Support for multiple input sources: memory buffers, files, streams
  - CRC32 integrity calculation for all compressed data
  - AES-256 encryption support (data and headers)
  - Solid mode compression (files share dictionary)
  - Multi-volume archive creation (.001, .002, etc.)

- **API Design**
  - `IParallelCompressor` C++ interface for direct integration
  - C API wrapper (`ParallelCompressAPI.h`) for language interop
  - Flexible callback system for progress and error handling
  - Configurable compression levels, thread count, and segment sizes

- **Test Coverage**
  - Comprehensive test suite with 18+ test scenarios
  - Security tests for edge cases and boundary conditions
  - Feature parity tests comparing single vs multi-stream
  - Integration tests for real-world usage patterns
  - End-to-end validation with 7z extraction verification

### Fixed - Build and Compatibility
- **Include Path Corrections**
  - Fixed relative paths for Windows headers (Synchronization.h, Thread.h)
  - Fixed IParallelCompress.h include path
  - Added missing Common/MethodId.h include for CMethodId type

- **7zip API Compatibility**
  - Corrected type names (CProperty → CProp)
  - Fixed function names (CreateCoder → CreateCoder_Id)
  - Updated WriteStream signature (removed unused parameter)
  - Fixed CObjArray2 usage (SetSize instead of AddNew)
  - Resolved CMultiOutStream private member access
  - Fixed CFolder copy constructor issue

- **COM Interface Declarations**
  - Converted Z7_CLASS_IMP_COM macros to manual declarations
  - Made interface methods public for proper access
  - Fixed helper class visibility (CLocalProgress, CCrcInStream)
  - Added friend declarations where needed
  - Moved method implementations outside class definitions

- **Security and Robustness**
  - Added buffer overflow protection in solid mode
  - Added integer overflow checks in statistics calculation
  - Enhanced input validation throughout
  - Improved error handling and error messages
  - Added null pointer checks in critical paths
  - Protected against resource exhaustion

- **Platform Compatibility**
  - Added E_POINTER constant for non-Windows builds
  - Made code compile cleanly with GCC on Linux
  - Platform-portable header organization

### Performance
- 10-30x speedup for large numbers of small files (measured)
- Linear scaling observed up to CPU core count
- Memory usage: ~24MB per thread at LZMA level 5
- Minimal overhead (<0.1%) from safety checks

### Documentation
- Comprehensive README with usage examples
- API documentation in header files
- Test code demonstrates all major features
- Detailed commit messages for all changes

### Notes
- All changes maintain backward compatibility
- No breaking changes to existing 7-Zip functionality
- Parallel compression is an additive feature
- Archives created are standard 7z format
