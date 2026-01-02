# Changelog

## Parallel Compression

### Added
- Parallel multi-stream compression (1-256 threads)
- Thread pool with work-stealing scheduler
- Standard 7z archive format with file metadata
- Progress tracking with per-item callbacks
- C API wrapper
- Memory buffers, files, and network stream support
- LZMA, LZMA2, BZip2, Deflate compression
- CRC32 integrity calculation
- AES-256 encryption (data + headers)
- Solid mode compression
- Multi-volume archives (.001, .002, etc.)
- Comprehensive test suite

### Performance
- 10-30x speedup for millions of small files
- Linear scaling up to CPU core count
- ~24MB memory per thread (LZMA level 5)
