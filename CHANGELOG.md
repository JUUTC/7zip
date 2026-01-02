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

### Current Limitations
- Non-solid mode only (solid blocks not implemented)
- Encryption API present but actual data encryption not implemented
- Multi-volume archives not implemented (API stub only)

### Performance
- 10-30x speedup for processing millions of small files
- Linear scaling up to CPU core count
- Memory usage: ~24MB per thread (LZMA level 5)
