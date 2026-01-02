// ParallelCompressAPI.cpp - C API implementation

#include "StdAfx.h"

#include "ParallelCompressAPI.h"
#include "ParallelCompressor.h"

#include "../../Common/MyString.h"
#include "../Common/FileStreams.h"
#include "../Common/StreamObjects.h"

using namespace NCompress::NParallel;

// Internal callback wrapper
class CCallbackWrapper : 
  public IParallelCompressCallback,
  public CMyUnknownImp
{
public:
  Z7_IFACES_IMP_UNK_1(IParallelCompressCallback)
  
  ParallelProgressCallback _progressCallback;
  ParallelErrorCallback _errorCallback;
  ParallelLookAheadCallback _lookAheadCallback;
  void *_userData;
  
  CCallbackWrapper(): _progressCallback(NULL), _errorCallback(NULL), 
      _lookAheadCallback(NULL), _userData(NULL) {}
  
  Z7_COM7F_IMF(OnItemStart(UInt32 itemIndex, const wchar_t *name))
  {
    return S_OK;
  }
  
  Z7_COM7F_IMF(OnItemProgress(UInt32 itemIndex, UInt64 inSize, UInt64 outSize))
  {
    if (_progressCallback)
      _progressCallback(itemIndex, inSize, outSize, _userData);
    return S_OK;
  }
  
  Z7_COM7F_IMF(OnItemComplete(UInt32 itemIndex, HRESULT result, UInt64 inSize, UInt64 outSize))
  {
    if (_progressCallback)
      _progressCallback(itemIndex, inSize, outSize, _userData);
    return S_OK;
  }
  
  Z7_COM7F_IMF(OnError(UInt32 itemIndex, HRESULT errorCode, const wchar_t *message))
  {
    if (_errorCallback)
      _errorCallback(itemIndex, errorCode, message, _userData);
    return S_OK;
  }
  
  Z7_COM7F_IMF2(Bool, ShouldCancel())
  {
    return false;
  }
  
  Z7_COM7F_IMF(GetNextItems(UInt32 currentIndex, UInt32 lookAheadCount, 
      CParallelInputItem *items, UInt32 *itemsReturned))
  {
    if (itemsReturned)
      *itemsReturned = 0;
    if (!_lookAheadCallback || !items)
      return S_OK;
    ParallelInputItemC cItems[16];
    UInt32 count = 0;
    HRESULT hr = _lookAheadCallback(currentIndex, lookAheadCount < 16 ? lookAheadCount : 16, 
        cItems, &count, _userData);
    if (SUCCEEDED(hr) && count > 0)
    {
      for (UInt32 i = 0; i < count; i++)
      {
        items[i].InStream = NULL;
        items[i].Name = cItems[i].Name;
        items[i].Size = cItems[i].Size;
        items[i].Attributes = 0;
        items[i].UserData = cItems[i].UserData;
        if (cItems[i].Data && cItems[i].DataSize > 0)
        {
          CBufInStream *streamSpec = new CBufInStream;
          CMyComPtr<ISequentialInStream> stream = streamSpec;
          streamSpec->Init((const Byte*)cItems[i].Data, cItems[i].DataSize, NULL);
          items[i].InStream = stream.Detach();
        }
      }
      if (itemsReturned)
        *itemsReturned = count;
    }
    return hr;
  }
};

// Internal wrapper structure
struct ParallelCompressorWrapper
{
  CMyComPtr<CParallelCompressor> Compressor;
  CMyComPtr<CCallbackWrapper> Callback;
};

struct ParallelStreamQueueWrapper
{
  CMyComPtr<CParallelStreamQueue> Queue;
};

extern "C" {

ParallelCompressorHandle ParallelCompressor_Create()
{
  ParallelCompressorWrapper *wrapper = new ParallelCompressorWrapper;
  wrapper->Compressor = new CParallelCompressor();
  wrapper->Callback = new CCallbackWrapper();
  return (ParallelCompressorHandle)wrapper;
}

void ParallelCompressor_Destroy(ParallelCompressorHandle handle)
{
  if (handle)
  {
    ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
    delete wrapper;
  }
}

HRESULT ParallelCompressor_SetNumThreads(ParallelCompressorHandle handle, UInt32 numThreads)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetNumThreads(numThreads);
}

HRESULT ParallelCompressor_SetCompressionLevel(ParallelCompressorHandle handle, UInt32 level)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetCompressionLevel(level);
}

HRESULT ParallelCompressor_SetCompressionMethod(ParallelCompressorHandle handle, UInt64 methodId)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  CMethodId mid = methodId;
  return wrapper->Compressor->SetCompressionMethod(&mid);
}

HRESULT ParallelCompressor_SetEncryption(
    ParallelCompressorHandle handle,
    const Byte *key,
    UInt32 keySize,
    const Byte *iv,
    UInt32 ivSize)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetEncryption(key, keySize, iv, ivSize);
}

HRESULT ParallelCompressor_SetPassword(ParallelCompressorHandle handle, const wchar_t *password)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetPassword(password);
}

HRESULT ParallelCompressor_SetSegmentSize(ParallelCompressorHandle handle, UInt64 segmentSize)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetSegmentSize(segmentSize);
}

HRESULT ParallelCompressor_SetVolumeSize(ParallelCompressorHandle handle, UInt64 volumeSize)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetVolumeSize(volumeSize);
}

HRESULT ParallelCompressor_SetVolumePrefix(ParallelCompressorHandle handle, const wchar_t *prefix)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetVolumePrefix(prefix);
}

HRESULT ParallelCompressor_SetSolidMode(ParallelCompressorHandle handle, int enabled)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetSolidMode(enabled != 0);
}

HRESULT ParallelCompressor_SetSolidBlockSize(ParallelCompressorHandle handle, UInt32 numFilesPerBlock)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetSolidBlockSize(numFilesPerBlock);
}

HRESULT ParallelCompressor_SetCallbacks(
    ParallelCompressorHandle handle,
    ParallelProgressCallback progressCallback,
    ParallelErrorCallback errorCallback,
    ParallelLookAheadCallback lookAheadCallback,
    void *userData)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  wrapper->Callback->_progressCallback = progressCallback;
  wrapper->Callback->_errorCallback = errorCallback;
  wrapper->Callback->_lookAheadCallback = lookAheadCallback;
  wrapper->Callback->_userData = userData;
  
  return wrapper->Compressor->SetCallback(wrapper->Callback);
}

HRESULT ParallelCompressor_CompressMultiple(
    ParallelCompressorHandle handle,
    ParallelInputItemC *items,
    UInt32 numItems,
    const wchar_t *outputPath)
{
  if (!handle || !items || numItems == 0 || !outputPath)
    return E_INVALIDARG;
    
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  
  // Create output file stream
  COutFileStream *outStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  if (!outStreamSpec->Create(outputPath, false))
    return E_FAIL;
  
  // Convert C items to C++ items
  CObjectVector<CParallelInputItem> cppItems;
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    CParallelInputItem &item = cppItems.AddNew();
    
    // Create input stream from data or file
    if (items[i].Data && items[i].DataSize > 0)
    {
      // Memory stream
      CBufInStream *streamSpec = new CBufInStream;
      CMyComPtr<ISequentialInStream> stream = streamSpec;
      streamSpec->Init((const Byte*)items[i].Data, items[i].DataSize, NULL);
      item.InStream = stream;
      item.Size = items[i].DataSize;
    }
    else if (items[i].FilePath)
    {
      // File stream
      CInFileStream *streamSpec = new CInFileStream;
      CMyComPtr<ISequentialInStream> stream = streamSpec;
      
      if (!streamSpec->Open(items[i].FilePath))
        return E_FAIL;
        
      UInt64 fileSize = 0;
      streamSpec->GetSize(&fileSize);
      item.InStream = stream;
      item.Size = fileSize;
    }
    else
    {
      return E_INVALIDARG;
    }
    
    item.Name = items[i].Name;
    item.Attributes = 0;
    item.UserData = items[i].UserData;
  }
  
  return wrapper->Compressor->CompressMultiple(&cppItems[0], numItems, outStream, NULL);
}

HRESULT ParallelCompressor_CompressMultipleToMemory(
    ParallelCompressorHandle handle,
    ParallelInputItemC *items,
    UInt32 numItems,
    void **outputBuffer,
    size_t *outputSize)
{
  if (!handle || !items || numItems == 0 || !outputBuffer || !outputSize)
    return E_INVALIDARG;
    
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  
  // Create dynamic memory output stream
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  // Convert C items to C++ items (same as above)
  CObjectVector<CParallelInputItem> cppItems;
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    CParallelInputItem &item = cppItems.AddNew();
    
    if (items[i].Data && items[i].DataSize > 0)
    {
      CBufInStream *streamSpec = new CBufInStream;
      CMyComPtr<ISequentialInStream> stream = streamSpec;
      streamSpec->Init((const Byte*)items[i].Data, items[i].DataSize, NULL);
      item.InStream = stream;
      item.Size = items[i].DataSize;
    }
    else if (items[i].FilePath)
    {
      CInFileStream *streamSpec = new CInFileStream;
      CMyComPtr<ISequentialInStream> stream = streamSpec;
      
      if (!streamSpec->Open(items[i].FilePath))
        return E_FAIL;
        
      UInt64 fileSize = 0;
      streamSpec->GetSize(&fileSize);
      item.InStream = stream;
      item.Size = fileSize;
    }
    else
    {
      return E_INVALIDARG;
    }
    
    item.Name = items[i].Name;
    item.Attributes = 0;
    item.UserData = items[i].UserData;
  }
  
  HRESULT result = wrapper->Compressor->CompressMultiple(&cppItems[0], numItems, outStream, NULL);
  
  if (result == S_OK || result == S_FALSE)
  {
    *outputSize = outStreamSpec->GetSize();
    // Allocate buffer and copy data
    *outputBuffer = malloc(*outputSize);
    if (*outputBuffer)
    {
      memcpy(*outputBuffer, outStreamSpec->GetBuffer(), *outputSize);
    }
    else
    {
      *outputSize = 0;
      return E_OUTOFMEMORY;
    }
  }
  
  return result;
}

// Stream queue implementation
ParallelStreamQueueHandle ParallelStreamQueue_Create()
{
  ParallelStreamQueueWrapper *wrapper = new ParallelStreamQueueWrapper;
  wrapper->Queue = new CParallelStreamQueue();
  return (ParallelStreamQueueHandle)wrapper;
}

void ParallelStreamQueue_Destroy(ParallelStreamQueueHandle handle)
{
  if (handle)
  {
    ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
    delete wrapper;
  }
}

HRESULT ParallelStreamQueue_SetMaxQueueSize(ParallelStreamQueueHandle handle, UInt32 maxSize)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
  return wrapper->Queue->SetMaxQueueSize(maxSize);
}

HRESULT ParallelStreamQueue_AddStream(
    ParallelStreamQueueHandle handle,
    const void *data,
    size_t dataSize,
    const wchar_t *name)
{
  if (!handle || !data || dataSize == 0)
    return E_INVALIDARG;
    
  ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
  
  // Create memory stream
  CBufInStream *streamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> stream = streamSpec;
  streamSpec->Init((const Byte*)data, dataSize, NULL);
  
  return wrapper->Queue->AddStream(stream, name, dataSize);
}

HRESULT ParallelStreamQueue_StartProcessing(
    ParallelStreamQueueHandle handle,
    const wchar_t *outputPath)
{
  if (!handle || !outputPath)
    return E_INVALIDARG;
    
  ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
  
  // Create output file stream
  COutFileStream *outStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  if (!outStreamSpec->Create(outputPath, false))
    return E_FAIL;
  
  return wrapper->Queue->StartProcessing(outStream);
}

HRESULT ParallelStreamQueue_WaitForCompletion(ParallelStreamQueueHandle handle)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
  return wrapper->Queue->WaitForCompletion();
}

HRESULT ParallelStreamQueue_GetStatus(
    ParallelStreamQueueHandle handle,
    UInt32 *itemsProcessed,
    UInt32 *itemsFailed,
    UInt32 *itemsPending)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelStreamQueueWrapper *wrapper = (ParallelStreamQueueWrapper*)handle;
  return wrapper->Queue->GetStatus(itemsProcessed, itemsFailed, itemsPending);
}

HRESULT ParallelCompressor_GetDetailedStatistics(
    ParallelCompressorHandle handle,
    ParallelStatisticsC *stats)
{
  if (!handle || !stats)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  
  CParallelStatistics cppStats;
  HRESULT result = wrapper->Compressor->GetDetailedStatistics(&cppStats);
  if (result == S_OK)
  {
    stats->ItemsTotal = cppStats.ItemsTotal;
    stats->ItemsCompleted = cppStats.ItemsCompleted;
    stats->ItemsFailed = cppStats.ItemsFailed;
    stats->ItemsInProgress = cppStats.ItemsInProgress;
    stats->TotalInSize = cppStats.TotalInSize;
    stats->TotalOutSize = cppStats.TotalOutSize;
    stats->BytesPerSecond = cppStats.BytesPerSecond;
    stats->FilesPerSecond = cppStats.FilesPerSecond;
    stats->ElapsedTimeMs = cppStats.ElapsedTimeMs;
    stats->EstimatedTimeRemainingMs = cppStats.EstimatedTimeRemainingMs;
    stats->CompressionRatioX100 = cppStats.CompressionRatioX100;
    stats->ActiveThreads = cppStats.ActiveThreads;
  }
  return result;
}

HRESULT ParallelCompressor_SetProgressUpdateInterval(
    ParallelCompressorHandle handle,
    UInt32 intervalMs)
{
  if (!handle)
    return E_INVALIDARG;
  ParallelCompressorWrapper *wrapper = (ParallelCompressorWrapper*)handle;
  return wrapper->Compressor->SetProgressUpdateInterval(intervalMs);
}

HRESULT ParallelCompressor_SetDetailedCallbacks(
    ParallelCompressorHandle handle,
    ParallelDetailedProgressCallback detailedProgressCallback,
    ParallelThroughputCallback throughputCallback,
    void *userData)
{
  if (!handle)
    return E_INVALIDARG;
  
  // Note: This is a placeholder for future callback support.
  // Callers should use GetDetailedStatistics() for polling-based progress tracking.
  // Full callback implementation would require:
  // 1. Adding callback storage to ParallelCompressorWrapper
  // 2. Modifying NotifyJobComplete to invoke callbacks
  // 3. Adding periodic throughput update timer
  // The polling API via GetDetailedStatistics() is the recommended approach.
  (void)detailedProgressCallback;
  (void)throughputCallback;
  (void)userData;
  return S_OK;
}

} // extern "C"
