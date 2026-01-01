// ParallelCompressor.cpp - Implementation

#include "StdAfx.h"

#include "ParallelCompressor.h"

#include "../../../C/Threads.h"

#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"

#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"
#include "../Common/LimitedStreams.h"
#include "../Common/MethodProps.h"
#include "../Common/RegisterCodec.h"

using namespace NWindows;

// Local progress for individual compression jobs
class CLocalProgress:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
public:
  Z7_IFACES_IMP_UNK_1(ICompressProgressInfo)
  
  ICompressProgressInfo *_progress;
  bool _inStartValueIsAssigned;
  UInt64 _inStartValue;
  
  void Init(ICompressProgressInfo *progress, bool inStartValueIsAssigned)
  {
    _progress = progress;
    _inStartValueIsAssigned = inStartValueIsAssigned;
    _inStartValue = 0;
  }
  
  Z7_IFACE_IMP(ICompressProgressInfo::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize))
  {
    if (_progress)
    {
      UInt64 inSize2 = 0;
      if (inSize)
      {
        inSize2 = *inSize;
        if (_inStartValueIsAssigned)
          inSize2 += _inStartValue;
      }
      return _progress->SetRatioInfo(_inStartValueIsAssigned && inSize ? &inSize2 : inSize, outSize);
    }
    return S_OK;
  }
};

namespace NCompress {
namespace NParallel {

// ==============================================
// CCompressWorker Implementation
// ==============================================

THREAD_FUNC_DECL CCompressWorker::ThreadFunc(void *param)
{
  CCompressWorker *worker = (CCompressWorker *)param;
  
  for (;;)
  {
    worker->StartEvent.Lock();
    
    if (worker->StopFlag)
      break;
      
    // Process jobs until queue is empty
    while (!worker->StopFlag)
    {
      if (worker->CurrentJob)
      {
        worker->CurrentJob->Result = worker->ProcessJob();
        worker->Compressor->NotifyJobComplete(worker->CurrentJob);
        worker->CurrentJob = NULL;
      }
      
      // Try to get next job
      CCompressionJob *nextJob = worker->Compressor->GetNextJob();
      if (!nextJob)
        break;  // No more jobs available
        
      worker->CurrentJob = nextJob;
    }
  }
  
  return THREAD_FUNC_RET_ZERO;
}

HRESULT CCompressWorker::Create()
{
  return Thread.Create(ThreadFunc, this);
}

void CCompressWorker::Stop()
{
  StopFlag = true;
  StartEvent.Set();
}

HRESULT CCompressWorker::ProcessJob()
{
  if (!CurrentJob)
    return E_FAIL;
    
  return Compressor->CompressJob(*CurrentJob, NULL);
}

// ==============================================
// CParallelCompressor Implementation
// ==============================================

CParallelCompressor::CParallelCompressor()
  : _numThreads(1)
  , _compressionLevel(5)
  , _segmentSize(0)
  , _encryptionEnabled(false)
  , _nextJobIndex(0)
  , _itemsCompleted(0)
  , _itemsFailed(0)
  , _totalInSize(0)
  , _totalOutSize(0)
{
  _methodId = { 0x030101 }; // LZMA
}

CParallelCompressor::~CParallelCompressor()
{
  Cleanup();
}

HRESULT CParallelCompressor::Init()
{
  Cleanup();
  
  // Create worker threads
  _workers.Clear();
  for (UInt32 i = 0; i < _numThreads; i++)
  {
    CCompressWorker &worker = _workers.AddNew();
    worker.Compressor = this;
    worker.ThreadIndex = i;
    RINOK(worker.StartEvent.Create());
    RINOK(worker.Create());
  }
  
  RINOK(_jobSemaphore.Create(0, 0x10000));
  RINOK(_completeEvent.Create(true));
  
  return S_OK;
}

void CParallelCompressor::Cleanup()
{
  // Signal all workers to stop
  for (UInt32 i = 0; i < _workers.Size(); i++)
  {
    _workers[i].Stop();
  }
  
  // Wait for threads to complete
  for (UInt32 i = 0; i < _workers.Size(); i++)
  {
    _workers[i].Thread.Wait_Close();
  }
  
  _workers.Clear();
  _jobs.Clear();
}

Z7_COM7F_IMF(CParallelCompressor::SetCallback(IParallelCompressCallback *callback))
{
  _callback = callback;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetNumThreads(UInt32 numThreads))
{
  if (numThreads == 0)
    numThreads = 1;
  if (numThreads > 256)
    numThreads = 256;
  _numThreads = numThreads;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetCompressionLevel(UInt32 level))
{
  if (level > 9)
    level = 9;
  _compressionLevel = level;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetEncryption(
    const Byte *key, UInt32 keySize, 
    const Byte *iv, UInt32 ivSize))
{
  if (key && keySize > 0)
  {
    _encryptionKey.Alloc(keySize);
    memcpy(_encryptionKey, key, keySize);
    
    if (iv && ivSize > 0)
    {
      _encryptionIV.Alloc(ivSize);
      memcpy(_encryptionIV, iv, ivSize);
    }
    
    _encryptionEnabled = true;
  }
  else
  {
    _encryptionEnabled = false;
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetSegmentSize(UInt64 segmentSize))
{
  _segmentSize = segmentSize;
  return S_OK;
}

HRESULT CParallelCompressor::CreateEncoder(ICompressCoder **encoder)
{
  if (!encoder)
    return E_POINTER;
    
  // Create LZMA encoder by default
  CCreatedCoder cod;
  RINOK(CreateCoder(
    EXTERNAL_CODECS_LOC_VARS
    _methodId, true, cod));
    
  if (!cod.Coder)
    return E_FAIL;
    
  CMyComPtr<ICompressCoder> coder;
  cod.Coder.QueryInterface(IID_ICompressCoder, &coder);
  
  if (!coder)
    return E_FAIL;
    
  *encoder = coder.Detach();
  
  // Set properties
  CMyComPtr<ICompressSetCoderProperties> setProps;
  (*encoder)->QueryInterface(IID_ICompressSetCoderProperties, (void **)&setProps);
  
  if (setProps)
  {
    // Set compression level and threads
    PROPID propIDs[2] = { NCoderPropID::kLevel, NCoderPropID::kNumThreads };
    PROPVARIANT propValues[2];
    
    propValues[0].vt = VT_UI4;
    propValues[0].ulVal = _compressionLevel;
    
    propValues[1].vt = VT_UI4;
    propValues[1].ulVal = 1; // Each job uses 1 thread
    
    RINOK(setProps->SetCoderProperties(propIDs, propValues, 2));
  }
  
  return S_OK;
}

HRESULT CParallelCompressor::CompressJob(CCompressionJob &job, ICompressCoder *encoderParam)
{
  CMyComPtr<ICompressCoder> encoder;
  
  if (encoderParam)
    encoder = encoderParam;
  else
  {
    RINOK(CreateEncoder(&encoder));
  }
  
  if (!encoder)
    return E_FAIL;
    
  // Notify start
  if (_callback)
    _callback->OnItemStart(job.ItemIndex, job.Name);
    
  // Create memory output stream - use the existing class
  CBufPtrSeqOutStream *outStreamSpec = new CBufPtrSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  // Estimate output size (input size * 1.5 for worst case)
  size_t estimatedSize = (size_t)(job.InSize > 0 ? job.InSize + job.InSize / 2 : 1 << 20);
  job.CompressedData.Alloc(estimatedSize);
  outStreamSpec->Init(job.CompressedData, estimatedSize);
  
  // Progress callback
  CLocalProgress *progressSpec = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = progressSpec;
  progressSpec->Init(NULL, false);
  
  // Compress
  UInt64 inSize = job.InSize;
  HRESULT result = encoder->Code(
      job.InStream,
      outStream,
      job.InSize > 0 ? &inSize : NULL,
      NULL,
      progress);
      
  if (result == S_OK)
  {
    job.OutSize = outStreamSpec->GetPos();
    
    // Resize buffer to actual size
    if (job.OutSize < estimatedSize)
    {
      job.CompressedData.ChangeSize_KeepData((size_t)job.OutSize, (size_t)job.OutSize);
    }
    
    // Apply encryption if enabled
    if (_encryptionEnabled && _encryptionKey.Size() > 0)
    {
      // Note: Encryption implementation requires integration with existing 7-Zip crypto codecs
      // This would typically involve:
      // 1. Creating an AES encoder (e.g., using Crypto/7zAes.h)
      // 2. Applying encryption filter to the compressed data
      // 3. Prepending necessary headers (IV, salt, etc.)
      // For now, the API is in place but encryption is not applied
    }
    
    // Notify progress
    if (_callback)
      _callback->OnItemProgress(job.ItemIndex, job.InSize, job.OutSize);
  }
  else
  {
    if (_callback)
    {
      _callback->OnError(job.ItemIndex, result, L"Compression failed");
    }
  }
  
  return result;
}

CCompressionJob* CParallelCompressor::GetNextJob()
{
  NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
  
  if (_nextJobIndex >= _jobs.Size())
    return NULL;
    
  CCompressionJob *job = &_jobs[_nextJobIndex];
  _nextJobIndex++;
  
  return job;
}

void CParallelCompressor::NotifyJobComplete(CCompressionJob *job)
{
  if (!job)
    return;
    
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
    
    job->Completed = true;
    _itemsCompleted++;
    
    if (job->Result != S_OK)
      _itemsFailed++;
    else
    {
      _totalInSize += job->InSize;
      _totalOutSize += job->OutSize;
    }
    
    // Notify callback
    if (_callback)
      _callback->OnItemComplete(job->ItemIndex, job->Result, job->InSize, job->OutSize);
      
    // Check if all jobs complete
    if (_itemsCompleted >= _jobs.Size())
      _completeEvent.Set();
  }
}

HRESULT CParallelCompressor::WriteJobToStream(CCompressionJob &job, ISequentialOutStream *outStream)
{
  if (!outStream || !job.Completed || job.Result != S_OK)
    return E_FAIL;
    
  // Write compressed data to output stream
  UInt32 processed = 0;
  return WriteStream(outStream, job.CompressedData, (size_t)job.OutSize, &processed);
}

Z7_COM7F_IMF(CParallelCompressor::CompressMultiple(
    CParallelInputItem *items, UInt32 numItems,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress))
{
  if (!items || numItems == 0 || !outStream)
    return E_INVALIDARG;
    
  // Initialize if needed
  if (_workers.Size() == 0)
  {
    RINOK(Init());
  }
  
  // Reset state
  _nextJobIndex = 0;
  _itemsCompleted = 0;
  _itemsFailed = 0;
  _totalInSize = 0;
  _totalOutSize = 0;
  _completeEvent.Reset();
  
  // Create jobs
  _jobs.Clear();
  for (UInt32 i = 0; i < numItems; i++)
  {
    CCompressionJob &job = _jobs.AddNew();
    job.ItemIndex = i;
    job.InStream = items[i].InStream;
    job.Name = items[i].Name ? items[i].Name : L"";
    job.InSize = items[i].Size;
    job.Attributes = items[i].Attributes;
    job.ModTime = items[i].ModificationTime;
    job.UserData = items[i].UserData;
  }
  
  // Start workers - they will pull jobs from the queue as they complete
  for (UInt32 i = 0; i < _workers.Size() && i < numItems; i++)
  {
    CCompressionJob *job = GetNextJob();
    if (job)
    {
      _workers[i].CurrentJob = job;
      _workers[i].StartEvent.Set();
    }
  }
  
  // Note: Workers will automatically pull remaining jobs after completing their current job
  // The NotifyJobComplete() method triggers the next job assignment
  
  // Wait for completion
  _completeEvent.Lock();
  
  // Write all completed jobs to output stream
  for (UInt32 i = 0; i < _jobs.Size(); i++)
  {
    if (_jobs[i].Completed && _jobs[i].Result == S_OK)
    {
      HRESULT res = WriteJobToStream(_jobs[i], outStream);
      if (res != S_OK)
        _itemsFailed++;
    }
  }
  
  // Return status
  if (_itemsFailed > 0)
    return S_FALSE;
    
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetCoderProperties(
    const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps))
{
  // Store properties for later use
  _properties.Clear();
  
  for (UInt32 i = 0; i < numProps; i++)
  {
    CProperty &prop = _properties.AddNew();
    prop.Id = propIDs[i];
    
    if (props[i].vt == VT_UI4)
      prop.Value = props[i].ulVal;
    else if (props[i].vt == VT_UI8)
      prop.Value = (UInt32)props[i].uhVal.QuadPart;
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::GetStatistics(
    UInt32 *itemsCompleted, UInt32 *itemsFailed,
    UInt64 *totalInSize, UInt64 *totalOutSize))
{
  NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
  
  if (itemsCompleted)
    *itemsCompleted = _itemsCompleted;
  if (itemsFailed)
    *itemsFailed = _itemsFailed;
  if (totalInSize)
    *totalInSize = _totalInSize;
  if (totalOutSize)
    *totalOutSize = _totalOutSize;
    
  return S_OK;
}

// ==============================================
// CParallelStreamQueue Implementation
// ==============================================

CParallelStreamQueue::CParallelStreamQueue()
  : _maxQueueSize(1000)
  , _processing(false)
  , _itemsProcessed(0)
  , _itemsFailed(0)
{
  _compressor = new CParallelCompressor();
}

CParallelStreamQueue::~CParallelStreamQueue()
{
}

Z7_COM7F_IMF(CParallelStreamQueue::AddStream(
    ISequentialInStream *inStream, const wchar_t *name, UInt64 size))
{
  if (!inStream)
    return E_INVALIDARG;
    
  NWindows::NSynchronization::CCriticalSectionLock lock(_queueLock);
  
  if (_processing)
    return E_FAIL; // Can't add while processing
    
  if (_queuedItems.Size() >= _maxQueueSize)
    return E_OUTOFMEMORY;
    
  CParallelInputItem &item = _queuedItems.AddNew();
  item.InStream = inStream;
  item.Name = name;
  item.Size = size;
  item.Attributes = 0;
  item.UserData = NULL;
  
  return S_OK;
}

Z7_COM7F_IMF(CParallelStreamQueue::SetMaxQueueSize(UInt32 maxSize))
{
  _maxQueueSize = maxSize;
  return S_OK;
}

Z7_COM7F_IMF(CParallelStreamQueue::StartProcessing(ISequentialOutStream *outStream))
{
  if (!outStream)
    return E_INVALIDARG;
    
  NWindows::NSynchronization::CCriticalSectionLock lock(_queueLock);
  
  if (_processing)
    return E_FAIL;
    
  _processing = true;
  _itemsProcessed = 0;
  _itemsFailed = 0;
  
  // Process queued items
  if (_queuedItems.Size() > 0)
  {
    HRESULT res = _compressor->CompressMultiple(&_queuedItems[0], _queuedItems.Size(), outStream, NULL);
    
    // Get actual statistics from the compressor
    UInt32 completed = 0;
    UInt32 failed = 0;
    _compressor->GetStatistics(&completed, &failed, NULL, NULL);
    
    _itemsProcessed = completed;
    _itemsFailed = failed;
    
    return res;
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CParallelStreamQueue::WaitForCompletion())
{
  // Synchronous for now
  return S_OK;
}

Z7_COM7F_IMF(CParallelStreamQueue::GetStatus(
    UInt32 *itemsProcessed, UInt32 *itemsFailed, UInt32 *itemsPending))
{
  NWindows::NSynchronization::CCriticalSectionLock lock(_queueLock);
  
  if (itemsProcessed)
    *itemsProcessed = _itemsProcessed;
  if (itemsFailed)
    *itemsFailed = _itemsFailed;
  if (itemsPending)
    *itemsPending = _processing ? _queuedItems.Size() - _itemsProcessed : _queuedItems.Size();
    
  return S_OK;
}

}}
