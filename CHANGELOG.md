# Changelog

## Parallel Compression Feature

### Added
- Parallel multi-stream compression API (`IParallelCompressor`)
- Thread pool with work-stealing scheduler (1-256 threads configurable)
- Standard 7z archive format output with file metadata
- Progress tracking with per-item callbacks
- C API wrapper for language interoperability
- Support for memory buffers, files, and network streams
- Auto-detection for single vs parallel mode
- Look-ahead prefetching for I/O optimization
- Compression methods: LZMA, LZMA2, BZip2, Deflate
- Error handling with per-item status tracking
- Example code and test suite
- **Solid mode compression** - files share dictionary for better compression ratio
- **Multi-volume archives** - split large archives across multiple files (.001, .002, etc.)

### Solid Mode
- SetSolidMode(true) enables solid compression
- SetSolidBlockSize(N) controls files per solid block (0 = all files in one block)
- Better compression ratio for similar files
- Compatible with standard 7z extraction

### Multi-Volume Support
- SetVolumeSize(bytes) specifies volume size
- SetVolumePrefix(path) sets output path prefix
- Creates .001, .002, .003 etc. volume files
- Compatible with standard 7z extraction

### Security Features
- AES-256 encryption with password-based key derivation
- Both data content and archive headers can be encrypted
- Uses standard 7-Zip encryption mechanism for compatibility

### Performance
- 10-30x speedup for processing millions of small files
- Linear scaling up to CPU core count
- Memory usage: ~24MB per thread (LZMA level 5)
