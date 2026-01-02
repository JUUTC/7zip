// IParallelCompress.h

#ifndef ZIP7_INC_IPARALLEL_COMPRESS_H
#define ZIP7_INC_IPARALLEL_COMPRESS_H

#include "IStream.h"
#include "ICoder.h"
#include "Common/MethodId.h"

Z7_PURE_INTERFACES_BEGIN

struct CParallelInputItem
{
  ISequentialInStream *InStream;
  const wchar_t *Name;
  UInt64 Size;
  UInt32 Attributes;
  FILETIME ModificationTime;
  void *UserData;
};

// Extended statistics structure for detailed progress tracking
struct CParallelStatistics
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
};

#define Z7_IFACEM_IParallelCompressCallback(x) \
  x(OnItemStart(UInt32 itemIndex, const wchar_t *name)) \
  x(OnItemProgress(UInt32 itemIndex, UInt64 inSize, UInt64 outSize)) \
  x(OnItemComplete(UInt32 itemIndex, HRESULT result, UInt64 inSize, UInt64 outSize)) \
  x(OnError(UInt32 itemIndex, HRESULT errorCode, const wchar_t *message)) \
  x(ShouldCancel()) \
  x(GetNextItems(UInt32 currentIndex, UInt32 lookAheadCount, CParallelInputItem *items, UInt32 *itemsReturned))

Z7_IFACE_CONSTR_CODER(IParallelCompressCallback, 0xA1)

// Extended callback interface with detailed statistics
#define Z7_IFACEM_IParallelCompressCallback2(x) \
  x(OnProgressWithStats(const CParallelStatistics *stats)) \
  x(OnThroughputUpdate(UInt64 bytesPerSecond, UInt64 filesPerSecondX100))

Z7_IFACE_CONSTR_CODER(IParallelCompressCallback2, 0xA4)

#define Z7_IFACEM_IParallelCompressor(x) \
  x(SetCallback(IParallelCompressCallback *callback)) \
  x(SetNumThreads(UInt32 numThreads)) \
  x(SetCompressionLevel(UInt32 level)) \
  x(SetCompressionMethod(const CMethodId *methodId)) \
  x(SetEncryption(const Byte *key, UInt32 keySize, const Byte *iv, UInt32 ivSize)) \
  x(SetPassword(const wchar_t *password)) \
  x(SetSegmentSize(UInt64 segmentSize)) \
  x(SetVolumeSize(UInt64 volumeSize)) \
  x(SetVolumePrefix(const wchar_t *prefix)) \
  x(SetSolidMode(bool solid)) \
  x(SetSolidBlockSize(UInt32 numFilesPerBlock)) \
  x(CompressMultiple(CParallelInputItem *items, UInt32 numItems, \
      ISequentialOutStream *outStream, ICompressProgressInfo *progress)) \
  x(GetStatistics(UInt32 *itemsCompleted, UInt32 *itemsFailed, \
      UInt64 *totalInSize, UInt64 *totalOutSize)) \
  x(GetDetailedStatistics(CParallelStatistics *stats)) \
  x(SetProgressUpdateInterval(UInt32 intervalMs))

Z7_IFACE_CONSTR_CODER(IParallelCompressor, 0xA2)

#define Z7_IFACEM_IParallelStreamQueue(x) \
  x(AddStream(ISequentialInStream *inStream, const wchar_t *name, UInt64 size)) \
  x(SetMaxQueueSize(UInt32 maxSize)) \
  x(StartProcessing(ISequentialOutStream *outStream)) \
  x(WaitForCompletion()) \
  x(GetStatus(UInt32 *itemsProcessed, UInt32 *itemsFailed, UInt32 *itemsPending))

Z7_IFACE_CONSTR_CODER(IParallelStreamQueue, 0xA3)

Z7_PURE_INTERFACES_END

#endif
