# Parallel Compression API for 7-Zip

## Overview

This enhancement adds parallel input stream support to the 7-Zip compression DLL, enabling efficient compression of multiple files, memory buffers, or network streams simultaneously.

## Key Features

- **Parallel Processing**: Compress multiple input streams concurrently using a configurable thread pool
- **Flexible Input Sources**: Support for memory buffers, files, network streams, and Azure Storage blobs
- **Generic Data Support**: Works with any data type via `ISequentialInStream` interface
- **Encryption Support**: Built-in encryption capabilities for secure compression
- **Segmentation**: Optional output segmentation for large archives
- **Progress Tracking**: Real-time progress callbacks for each item
- **Error Handling**: Graceful error handling with per-item status reporting
- **C and C++ APIs**: Both native C++ interfaces and C API for maximum compatibility

## Architecture

### Core Components

1. **IParallelCompressor**: Main interface for parallel compression operations
2. **CParallelCompressor**: Implementation with thread pool management
3. **IParallelStreamQueue**: Queue-based interface for batched processing
4. **C API**: Wrapper for use in C applications and language bindings

### Processing Pipeline

```
Input Streams → Thread Pool → Compression → Encryption → Segmentation → Output
     ↓              ↓              ↓            ↓             ↓            ↓
   Memory      Worker 1       LZMA/etc      AES-256      Split files   Archive
   Files       Worker 2       Parallel      Optional     Optional      Stream
   Network     Worker N       Per-thread
```

## C++ API Usage

### Basic Example

```cpp
#include "IParallelCompress.h"
#include "ParallelCompressor.h"

// Create items to compress
CObjectVector<CParallelInputItem> items;

for (int i = 0; i < numStreams; i++)
{
  CParallelInputItem &item = items.AddNew();
  item.InStream = myInputStream;  // Your ISequentialInStream
  item.Name = L"stream_name";
  item.Size = streamSize;
}

// Create and configure compressor
CMyComPtr<IParallelCompressor> compressor = new CParallelCompressor();
compressor->SetNumThreads(8);
compressor->SetCompressionLevel(5);

// Compress
HRESULT hr = compressor->CompressMultiple(
    &items[0], 
    items.Size(), 
    outputStream, 
    NULL);
```

### With Encryption

```cpp
// Set encryption key and IV
Byte key[32] = { /* your key */ };
Byte iv[16] = { /* your IV */ };
compressor->SetEncryption(key, 32, iv, 16);
```

### Using Stream Queue

```cpp
CMyComPtr<IParallelStreamQueue> queue = new CParallelStreamQueue();
queue->SetMaxQueueSize(1000);

// Add streams dynamically
for (auto& blob : azureBlobs)
{
  queue->AddStream(blobStream, blobName, blobSize);
}

// Process all queued items
queue->StartProcessing(outputStream);
queue->WaitForCompletion();

// Get status
UInt32 processed, failed, pending;
queue->GetStatus(&processed, &failed, &pending);
```

## C API Usage

### Basic Example

```c
#include "ParallelCompressAPI.h"

// Create items
ParallelInputItemC items[10];
for (int i = 0; i < 10; i++)
{
  items[i].Data = dataBuffers[i];
  items[i].DataSize = dataSizes[i];
  items[i].Name = L"item_name";
}

// Create and configure compressor
ParallelCompressorHandle compressor = ParallelCompressor_Create();
ParallelCompressor_SetNumThreads(compressor, 4);
ParallelCompressor_SetCompressionLevel(compressor, 5);

// Set callbacks
ParallelCompressor_SetCallbacks(compressor, MyProgressCallback, MyErrorCallback, userData);

// Compress
HRESULT hr = ParallelCompressor_CompressMultiple(
    compressor,
    items,
    10,
    L"output.7z");

// Cleanup
ParallelCompressor_Destroy(compressor);
```

### Progress Callback

```c
void MyProgressCallback(UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void *userData)
{
  printf("Item %u: %llu -> %llu bytes\n", itemIndex, inSize, outSize);
}

void MyErrorCallback(UInt32 itemIndex, HRESULT errorCode, const wchar_t *message, void *userData)
{
  wprintf(L"Error on item %u: %s\n", itemIndex, message);
}
```

## Azure Storage Blob Example

```cpp
// Pseudo-code for Azure Storage integration
#include <azure/storage/blobs.hpp>

using namespace Azure::Storage::Blobs;

// Get blob client
BlobContainerClient containerClient = /* ... */;

// Create parallel compressor
CMyComPtr<IParallelCompressor> compressor = new CParallelCompressor();
compressor->SetNumThreads(16);  // Process 16 blobs concurrently

CObjectVector<CParallelInputItem> items;

// Iterate through blobs
auto blobList = containerClient.ListBlobs();
for (auto& blob : blobList)
{
  CParallelInputItem &item = items.AddNew();
  
  // Create stream from blob
  BlobClient blobClient = containerClient.GetBlobClient(blob.Name);
  auto blobStream = blobClient.Download();
  
  // Wrap in ISequentialInStream adapter
  item.InStream = new CAzureBlobStream(blobStream);
  item.Name = blob.Name.c_str();
  item.Size = blob.Properties.ContentLength;
}

// Compress all blobs in parallel
compressor->CompressMultiple(&items[0], items.Size(), outputStream, NULL);
```

## Performance Considerations

### Thread Count
- Set to number of CPU cores for CPU-bound workloads
- Can exceed CPU count for I/O-bound workloads (network/disk)
- Recommended: 2x CPU cores for mixed workloads

### Memory Usage
- Each thread maintains compression dictionaries
- Estimate: (dictionary_size + input_buffer) × num_threads
- For LZMA level 5: ~24 MB per thread

### Compression Level
- Level 0-3: Fast, lower compression (good for already compressed data)
- Level 5: Balanced (default)
- Level 7-9: Maximum compression, slower

## Integration with Existing Code

The parallel compression API is fully compatible with existing 7-Zip code:

1. **Backward Compatible**: Single-stream compression still works
2. **Uses Existing Codecs**: Leverages all existing compression algorithms
3. **Standard Interfaces**: Built on standard `ISequentialInStream` and `ICompressCoder`
4. **No Breaking Changes**: Existing DLL exports remain unchanged

## Building

### Windows (Visual Studio)

```bash
cd CPP\7zip\Bundles\Format7z
nmake
```

### Linux/macOS (GCC/Clang)

```bash
cd CPP/7zip/Bundles/Format7z
make -f makefile.gcc
```

## API Reference

### Interfaces

#### IParallelCompressor
- `SetCallback(IParallelCompressCallback *callback)` - Set progress callback
- `SetNumThreads(UInt32 numThreads)` - Configure thread pool size
- `SetCompressionLevel(UInt32 level)` - Set compression level (0-9)
- `SetEncryption(const Byte *key, UInt32 keySize, const Byte *iv, UInt32 ivSize)` - Enable encryption
- `SetSegmentSize(UInt64 segmentSize)` - Set output segment size
- `CompressMultiple(...)` - Compress multiple items

#### IParallelStreamQueue
- `AddStream(...)` - Add stream to queue
- `SetMaxQueueSize(UInt32 maxSize)` - Set queue capacity
- `StartProcessing(...)` - Begin processing queued items
- `WaitForCompletion()` - Wait for all items to complete
- `GetStatus(...)` - Get processing statistics

### C API Functions
See `ParallelCompressAPI.h` for complete C API reference.

## Error Handling

- `S_OK`: All items compressed successfully
- `S_FALSE`: Some items failed (check callbacks for details)
- `E_ABORT`: User cancelled operation
- `E_OUTOFMEMORY`: Insufficient memory
- Other HRESULT codes indicate specific failures

## Thread Safety

- `IParallelCompressor`: Thread-safe after initialization
- Multiple compressor instances can run concurrently
- Callbacks may be called from multiple threads

## Limitations

- Maximum 256 concurrent threads
- Input streams must be independent (no shared state)
- Output order may differ from input order (by design for parallelism)

## Future Enhancements

- Async/await pattern support
- Built-in Azure Storage Blob integration
- Streaming output segmentation
- Load balancing across heterogeneous inputs
- GPU acceleration for encryption

## License

Same as 7-Zip: GNU LGPL with unRAR restriction

## Contact

For issues and feature requests, please use the GitHub issue tracker.
