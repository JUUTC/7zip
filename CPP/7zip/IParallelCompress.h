// IParallelCompress.h - Interface for parallel compression of multiple input streams

#ifndef ZIP7_INC_IPARALLEL_COMPRESS_H
#define ZIP7_INC_IPARALLEL_COMPRESS_H

#include "IStream.h"
#include "ICoder.h"

Z7_PURE_INTERFACES_BEGIN

// Structure to represent a single input item for compression
struct CParallelInputItem
{
  ISequentialInStream *InStream;     // Input stream (can be memory, file, network, etc.)
  const wchar_t *Name;               // Optional name/identifier for the stream
  UInt64 Size;                       // Size if known, 0 if unknown
  UInt32 Attributes;                 // Optional attributes
  FILETIME ModificationTime;         // Optional modification time
  void *UserData;                    // Optional user context
};

// Callback interface for parallel compression progress and results
#define Z7_IFACEM_IParallelCompressCallback(x) \
  x(OnItemStart(UInt32 itemIndex, const wchar_t *name)) \
  x(OnItemProgress(UInt32 itemIndex, UInt64 inSize, UInt64 outSize)) \
  x(OnItemComplete(UInt32 itemIndex, HRESULT result, UInt64 inSize, UInt64 outSize)) \
  x(OnError(UInt32 itemIndex, HRESULT errorCode, const wchar_t *message)) \
  x(ShouldCancel())

Z7_IFACE_CONSTR_CODER(IParallelCompressCallback, 0xA1)

// Main interface for parallel compression
#define Z7_IFACEM_IParallelCompressor(x) \
  x(SetCallback(IParallelCompressCallback *callback)) \
  x(SetNumThreads(UInt32 numThreads)) \
  x(SetCompressionLevel(UInt32 level)) \
  x(SetEncryption(const Byte *key, UInt32 keySize, const Byte *iv, UInt32 ivSize)) \
  x(SetSegmentSize(UInt64 segmentSize)) \
  x(CompressMultiple(CParallelInputItem *items, UInt32 numItems, \
      ISequentialOutStream *outStream, ICompressProgressInfo *progress)) \
  x(GetStatistics(UInt32 *itemsCompleted, UInt32 *itemsFailed, \
      UInt64 *totalInSize, UInt64 *totalOutSize))

Z7_IFACE_CONSTR_CODER(IParallelCompressor, 0xA2)
  /* 
    CompressMultiple() compresses multiple input streams in parallel
    
    Parameters:
      items      - Array of input items to compress
      numItems   - Number of items in the array
      outStream  - Output stream for compressed data
      progress   - Optional progress callback
    
    Returns:
      S_OK       - All items compressed successfully
      S_FALSE    - One or more items failed (check callback for details)
      E_ABORT    - Operation cancelled by user
      Other error codes for system failures
    
    The method will:
    1. Process multiple input streams in parallel using thread pool
    2. Compress each stream using configured compression settings
    3. Apply encryption if configured
    4. Segment output if segment size is configured
    5. Call callback methods to report progress
    6. Handle errors gracefully and continue processing other items
  */

// Interface for stream batching and queuing
#define Z7_IFACEM_IParallelStreamQueue(x) \
  x(AddStream(ISequentialInStream *inStream, const wchar_t *name, UInt64 size)) \
  x(SetMaxQueueSize(UInt32 maxSize)) \
  x(StartProcessing(ISequentialOutStream *outStream)) \
  x(WaitForCompletion()) \
  x(GetStatus(UInt32 *itemsProcessed, UInt32 *itemsFailed, UInt32 *itemsPending))

Z7_IFACE_CONSTR_CODER(IParallelStreamQueue, 0xA3)

Z7_PURE_INTERFACES_END

#endif
