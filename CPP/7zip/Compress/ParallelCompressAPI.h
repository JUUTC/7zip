// ParallelCompressAPI.h - C API for parallel compression

#ifndef ZIP7_INC_PARALLEL_COMPRESS_API_H
#define ZIP7_INC_PARALLEL_COMPRESS_API_H

#include "../../Common/MyTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef void* ParallelCompressorHandle;
typedef void* ParallelStreamQueueHandle;

// Input item structure for C API
typedef struct
{
  const void *Data;              // Pointer to data in memory (if DataSize > 0)
  size_t DataSize;               // Size of data in memory (0 if using stream)
  const wchar_t *FilePath;       // File path to read (if Data is NULL)
  const wchar_t *Name;           // Name/identifier for the item
  UInt64 Size;                   // Size if known
  void *UserData;                // User context
} ParallelInputItemC;

// Callback function types
typedef void (*ParallelProgressCallback)(
    UInt32 itemIndex,
    UInt64 inSize,
    UInt64 outSize,
    void *userData);

typedef void (*ParallelErrorCallback)(
    UInt32 itemIndex,
    HRESULT errorCode,
    const wchar_t *message,
    void *userData);

// Create/destroy parallel compressor
ParallelCompressorHandle ParallelCompressor_Create();
void ParallelCompressor_Destroy(ParallelCompressorHandle handle);

// Configuration
HRESULT ParallelCompressor_SetNumThreads(ParallelCompressorHandle handle, UInt32 numThreads);
HRESULT ParallelCompressor_SetCompressionLevel(ParallelCompressorHandle handle, UInt32 level);
HRESULT ParallelCompressor_SetCompressionMethod(ParallelCompressorHandle handle, UInt64 methodId);
HRESULT ParallelCompressor_SetEncryption(
    ParallelCompressorHandle handle,
    const Byte *key,
    UInt32 keySize,
    const Byte *iv,
    UInt32 ivSize);
HRESULT ParallelCompressor_SetSegmentSize(ParallelCompressorHandle handle, UInt64 segmentSize);
HRESULT ParallelCompressor_SetCallbacks(
    ParallelCompressorHandle handle,
    ParallelProgressCallback progressCallback,
    ParallelErrorCallback errorCallback,
    void *userData);

// Compression
HRESULT ParallelCompressor_CompressMultiple(
    ParallelCompressorHandle handle,
    ParallelInputItemC *items,
    UInt32 numItems,
    const wchar_t *outputPath);

HRESULT ParallelCompressor_CompressMultipleToMemory(
    ParallelCompressorHandle handle,
    ParallelInputItemC *items,
    UInt32 numItems,
    void **outputBuffer,
    size_t *outputSize);

// Stream queue API
ParallelStreamQueueHandle ParallelStreamQueue_Create();
void ParallelStreamQueue_Destroy(ParallelStreamQueueHandle handle);

HRESULT ParallelStreamQueue_SetMaxQueueSize(ParallelStreamQueueHandle handle, UInt32 maxSize);
HRESULT ParallelStreamQueue_AddStream(
    ParallelStreamQueueHandle handle,
    const void *data,
    size_t dataSize,
    const wchar_t *name);
HRESULT ParallelStreamQueue_StartProcessing(
    ParallelStreamQueueHandle handle,
    const wchar_t *outputPath);
HRESULT ParallelStreamQueue_WaitForCompletion(ParallelStreamQueueHandle handle);
HRESULT ParallelStreamQueue_GetStatus(
    ParallelStreamQueueHandle handle,
    UInt32 *itemsProcessed,
    UInt32 *itemsFailed,
    UInt32 *itemsPending);

#ifdef __cplusplus
}
#endif

#endif
