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

typedef HRESULT (*ParallelLookAheadCallback)(
    UInt32 currentIndex,
    UInt32 lookAheadCount,
    ParallelInputItemC *items,
    UInt32 *itemsReturned,
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
HRESULT ParallelCompressor_SetPassword(
    ParallelCompressorHandle handle,
    const wchar_t *password);
HRESULT ParallelCompressor_SetSegmentSize(ParallelCompressorHandle handle, UInt64 segmentSize);
HRESULT ParallelCompressor_SetVolumeSize(ParallelCompressorHandle handle, UInt64 volumeSize);
HRESULT ParallelCompressor_SetVolumePrefix(ParallelCompressorHandle handle, const wchar_t *prefix);
HRESULT ParallelCompressor_SetSolidMode(ParallelCompressorHandle handle, int enabled);
HRESULT ParallelCompressor_SetSolidBlockSize(ParallelCompressorHandle handle, UInt32 numFilesPerBlock);
HRESULT ParallelCompressor_SetCallbacks(
    ParallelCompressorHandle handle,
    ParallelProgressCallback progressCallback,
    ParallelErrorCallback errorCallback,
    ParallelLookAheadCallback lookAheadCallback,
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

// Extended statistics structure for C API
typedef struct
{
  UInt32 ItemsTotal;           // Total number of items to process
  UInt32 ItemsCompleted;       // Number of items completed successfully
  UInt32 ItemsFailed;          // Number of items that failed
  UInt32 ItemsInProgress;      // Number of items currently being processed
  UInt64 TotalInSize;          // Total uncompressed bytes processed
  UInt64 TotalOutSize;         // Total compressed bytes produced
  UInt64 BytesPerSecond;       // Current compression throughput (bytes/sec)
  UInt64 FilesPerSecond;       // Files completed per second (x100 for precision)
  UInt64 ElapsedTimeMs;        // Elapsed time in milliseconds
  UInt64 EstimatedTimeRemainingMs;  // Estimated time remaining in milliseconds
  UInt32 CompressionRatioX100; // Compression ratio * 100 (e.g., 42 = 42% of original)
  UInt32 ActiveThreads;        // Number of threads currently active
} ParallelStatisticsC;

// Extended progress callback with detailed statistics
typedef void (*ParallelDetailedProgressCallback)(
    const ParallelStatisticsC *stats,
    void *userData);

// Throughput update callback (bytes/sec, files/sec * 100)
typedef void (*ParallelThroughputCallback)(
    UInt64 bytesPerSecond,
    UInt64 filesPerSecondX100,
    void *userData);

// Get detailed statistics during compression
HRESULT ParallelCompressor_GetDetailedStatistics(
    ParallelCompressorHandle handle,
    ParallelStatisticsC *stats);

// Set progress update interval in milliseconds (default: 100ms)
HRESULT ParallelCompressor_SetProgressUpdateInterval(
    ParallelCompressorHandle handle,
    UInt32 intervalMs);

// Set extended callbacks for detailed progress tracking
HRESULT ParallelCompressor_SetDetailedCallbacks(
    ParallelCompressorHandle handle,
    ParallelDetailedProgressCallback detailedProgressCallback,
    ParallelThroughputCallback throughputCallback,
    void *userData);

#ifdef __cplusplus
}
#endif

#endif
