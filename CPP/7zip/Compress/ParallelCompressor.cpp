// ParallelCompressor.cpp

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

THREAD_FUNC_DECL CCompressWorker::ThreadFunc(void *param)
{
  CCompressWorker *worker = (CCompressWorker *)param;
  
  for (;;)
  {
    worker->StartEvent.Lock();
    
    if (worker->StopFlag)
      break;
    while (!worker->StopFlag)
    {
      if (worker->CurrentJob)
      {
        worker->CurrentJob->Result = worker->ProcessJob();
        worker->Compressor->NotifyJobComplete(worker->CurrentJob);
        worker->CurrentJob = NULL;
      }
      CCompressionJob *nextJob = worker->Compressor->GetNextJob();
      if (!nextJob)
        break;
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

CParallelCompressor::CParallelCompressor():
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
  for (UInt32 i = 0; i < _workers.Size(); i++)
  {
    _workers[i].Stop();
  }
  for (UInt32 i = 0; i < _workers.Size(); i++)
  {
    _workers[i].Thread.Wait_Close();
  }
  _workers.Clear();
  _jobs.Clear();
}

Z7_COM7F_IMF(CParallelCompressor::Code(
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize,
    ICompressProgressInfo *progress))
{
  if (!inStream || !outStream)
    return E_INVALIDARG;
  if (_numThreads <= 1)
    return CompressSingleStream(inStream, outStream, inSize, progress);
  CParallelInputItem item;
  item.InStream = inStream;
  item.Name = NULL;
  item.Size = inSize ? *inSize : 0;
  item.Attributes = 0;
  item.UserData = NULL;
  return CompressMultiple(&item, 1, outStream, progress);
}

HRESULT CParallelCompressor::CompressSingleStream(
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, ICompressProgressInfo *progress)
{
  CMyComPtr<ICompressCoder> encoder;
  RINOK(CreateEncoder(&encoder));
  return encoder->Code(inStream, outStream, inSize, NULL, progress);
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

Z7_COM7F_IMF(CParallelCompressor::SetCompressionMethod(const CMethodId *methodId))
{
  if (methodId)
    _methodId = *methodId;
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
    if (job.OutSize < estimatedSize)
    {
      job.CompressedData.ChangeSize_KeepData((size_t)job.OutSize, (size_t)job.OutSize);
    }
    if (_encryptionEnabled && _encryptionKey.Size() > 0)
    {
    }
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
    if (_callback)
      _callback->OnItemComplete(job->ItemIndex, job->Result, job->InSize, job->OutSize);
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

void CParallelCompressor::PrepareCompressionMethod(NArchive::N7z::CCompressionMethodMode &method)
{
  method.Bonds.Clear();
  method.Methods.Clear();
  
  NArchive::N7z::CMethodFull &methodFull = method.Methods.AddNew();
  methodFull.Id = _methodId;
  methodFull.NumStreams = 1;
  
  PROPID propIDs[2] = { NCoderPropID::kLevel, NCoderPropID::kNumThreads };
  PROPVARIANT propValues[2];
  
  propValues[0].vt = VT_UI4;
  propValues[0].ulVal = _compressionLevel;
  
  propValues[1].vt = VT_UI4;
  propValues[1].ulVal = 1;
  
  for (UInt32 i = 0; i < 2; i++)
  {
    NArchive::N7z::CProp &prop = methodFull.Props.AddNew();
    prop.Id = propIDs[i];
    prop.Value = propValues[i];
  }
  
  method.NumThreads = _numThreads;
}

HRESULT CParallelCompressor::Create7zArchive(ISequentialOutStream *outStream,
    const CObjectVector<CCompressionJob> &jobs)
{
  using namespace NArchive::N7z;
  
  COutArchive outArchive;
  RINOK(outArchive.Create_and_WriteStartPrefix(outStream))
  
  CArchiveDatabaseOut db;
  db.Clear();
  
  // Write compressed data first, track successful jobs
  CRecordVector<UInt32> successJobIndices;
  CRecordVector<UInt64> packSizes;
  CRecordVector<UInt64> unpackSizes;
  
  for (unsigned i = 0; i < jobs.Size(); i++)
  {
    const CCompressionJob &job = jobs[i];
    if (job.Result != S_OK || !job.Completed)
      continue;
    
    // Write compressed data
    RINOK(WriteStream(outStream, job.CompressedData, (size_t)job.OutSize))
    
    successJobIndices.Add(i);
    packSizes.Add(job.OutSize);
    unpackSizes.Add(job.InSize);
  }
  
  // Now build metadata for successful jobs only
  for (unsigned idx = 0; idx < successJobIndices.Size(); idx++)
  {
    const CCompressionJob &job = jobs[successJobIndices[idx]];
    
    CFileItem fileItem;
    fileItem.Size = job.InSize;
    fileItem.HasStream = (job.InSize > 0);
    fileItem.IsDir = false;
    fileItem.CrcDefined = false;
    
    CFileItem2 fileItem2;
    fileItem2.MTime = job.ModTime.dwLowDateTime | ((UInt64)job.ModTime.dwHighDateTime << 32);
    fileItem2.MTimeDefined = true;
    fileItem2.AttribDefined = (job.Attributes != 0);
    fileItem2.Attrib = job.Attributes;
    fileItem2.CTimeDefined = false;
    fileItem2.ATimeDefined = false;
    fileItem2.StartPosDefined = false;
    fileItem2.IsAnti = false;
    
    db.AddFile(fileItem, fileItem2, job.Name);
    
    // Create folder for each file
    CFolder folder;
    CCoderInfo &coder = folder.Coders.AddNew();
    coder.MethodID = _methodId;
    coder.NumStreams = 1;
    db.Folders.Add(folder);
    
    db.PackSizes.Add(packSizes[idx]);
    
    // CRC calculation: Currently disabled for performance
    // To enable: calculate CRC32 during compression in CompressJob()
    // Store in job.Crc, then use: db.PackCRCs.Defs.Add(true); db.PackCRCs.Vals.Add(job.Crc);
    db.PackCRCs.Defs.Add(false);
    db.PackCRCs.Vals.Add(0);
    
    db.NumUnpackStreamsVector.Add(1);
    db.CoderUnpackSizes.Add(unpackSizes[idx]);
  }
  
  CCompressionMethodMode method;
  PrepareCompressionMethod(method);
  
  CHeaderOptions headerOptions;
  headerOptions.CompressMainHeader = true;
  
  RINOK(outArchive.WriteDatabase(
      EXTERNAL_CODECS_LOC_VARS
      db,
      &method,
      headerOptions))
  
  outArchive.Close();
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::CompressMultiple(
    CParallelInputItem *items, UInt32 numItems,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress))
{
  if (!items || numItems == 0 || !outStream)
    return E_INVALIDARG;
  if (numItems == 1 && _numThreads <= 1)
    return CompressSingleStream(items[0].InStream, outStream, 
        items[0].Size > 0 ? &items[0].Size : NULL, progress);
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
  if (_callback)
  {
    const UInt32 lookAheadCount = _numThreads * 2;
    CParallelInputItem lookAheadItems[16];
    UInt32 itemsReturned = 0;
    _callback->GetNextItems(0, lookAheadCount < 16 ? lookAheadCount : 16, 
        lookAheadItems, &itemsReturned);
    for (UInt32 i = 0; i < itemsReturned; i++)
    {
      CCompressionJob &job = _jobs.AddNew();
      job.ItemIndex = numItems + i;
      job.InStream = lookAheadItems[i].InStream;
      job.Name = lookAheadItems[i].Name ? lookAheadItems[i].Name : L"";
      job.InSize = lookAheadItems[i].Size;
      job.Attributes = lookAheadItems[i].Attributes;
      job.ModTime = lookAheadItems[i].ModificationTime;
      job.UserData = lookAheadItems[i].UserData;
    }
  }
  for (UInt32 i = 0; i < _workers.Size() && i < _jobs.Size(); i++)
  {
    CCompressionJob *job = GetNextJob();
    if (job)
    {
      _workers[i].CurrentJob = job;
      _workers[i].StartEvent.Set();
    }
  }
  _completeEvent.Lock();
  
  // Check for failed jobs and report
  UInt32 successCount = 0;
  for (UInt32 i = 0; i < _jobs.Size(); i++)
  {
    if (_jobs[i].Completed && _jobs[i].Result == S_OK)
      successCount++;
  }
  
  if (successCount == 0)
  {
    // All jobs failed
    if (_callback)
      _callback->OnError(0, E_FAIL, L"All compression jobs failed");
    return E_FAIL;
  }
  
  // Create 7z archive with successfully compressed data
  HRESULT archiveResult = Create7zArchive(outStream, _jobs);
  if (archiveResult != S_OK)
  {
    if (_callback)
      _callback->OnError(0, archiveResult, L"Failed to create 7z archive");
    return archiveResult;
  }
  
  // Return S_FALSE if some jobs failed, S_OK if all succeeded
  if (_itemsFailed > 0)
    return S_FALSE;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetCoderProperties(
    const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps))
{
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

Z7_COM7F_IMF(CParallelCompressor::WriteCoderProperties(ISequentialOutStream *outStream))
{
  if (!outStream)
    return E_POINTER;
  CMyComPtr<ICompressCoder> encoder;
  RINOK(CreateEncoder(&encoder));
  CMyComPtr<ICompressWriteCoderProperties> writeProps;
  encoder.QueryInterface(IID_ICompressWriteCoderProperties, &writeProps);
  if (writeProps)
    return writeProps->WriteCoderProperties(outStream);
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetCoderPropertiesOpt(
    const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps))
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    PROPID propID = propIDs[i];
    const PROPVARIANT &prop = props[i];
    if (propID == NCoderPropID::kExpectedDataSize)
    {
      if (prop.vt == VT_UI8)
      {
      }
    }
  }
  return SetCoderProperties(propIDs, props, numProps);
}

Z7_COM7F_IMF(CParallelCompressor::GetInStreamProcessedSize(UInt64 *value))
{
  if (!value)
    return E_POINTER;
  NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
  *value = _totalInSize;
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

CParallelStreamQueue::CParallelStreamQueue():
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
