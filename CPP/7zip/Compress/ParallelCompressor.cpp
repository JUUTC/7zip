// ParallelCompressor.cpp

#include "StdAfx.h"

#include "ParallelCompressor.h"

#include "../../../C/Threads.h"
#include "../../../C/7zCrc.h"

#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"

#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"
#include "../Common/LimitedStreams.h"
#include "../Common/MethodProps.h"
#include "../Common/RegisterCodec.h"
#include "../Common/FilterCoder.h"
#include "../Common/MultiOutStream.h"

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

// CRC-calculating input stream wrapper
class CCrcInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
public:
  Z7_COM_UNKNOWN_IMP_1(ISequentialInStream)
  
  CMyComPtr<ISequentialInStream> _stream;
  UInt32 _crc;
  UInt64 _size;
  
  void Init(ISequentialInStream *stream)
  {
    _stream = stream;
    _crc = CRC_INIT_VAL;
    _size = 0;
  }
  
  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }
  UInt64 GetSize() const { return _size; }
  
  Z7_COM7F_IMF(Read(void *data, UInt32 size, UInt32 *processedSize))
  {
    UInt32 realProcessed = 0;
    HRESULT result = _stream->Read(data, size, &realProcessed);
    if (realProcessed > 0)
    {
      _crc = CrcUpdate(_crc, data, realProcessed);
      _size += realProcessed;
    }
    if (processedSize)
      *processedSize = realProcessed;
    return result;
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

// Get current time in milliseconds
static UInt64 GetCurrentTimeMs()
{
#ifdef _WIN32
  return GetTickCount64();
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;  // Return 0 on error
  return (UInt64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

CParallelCompressor::CParallelCompressor()
  : _numThreads(1)
  , _compressionLevel(5)
  , _segmentSize(0)
  , _volumeSize(0)
  , _solidMode(false)
  , _solidBlockSize(0)
  , _encryptionEnabled(false)
  , _nextJobIndex(0)
  , _methodId(NArchive::N7z::k_LZMA)
  , _itemsCompleted(0)
  , _itemsFailed(0)
  , _totalInSize(0)
  , _totalOutSize(0)
  , _itemsTotal(0)
  , _activeThreads(0)
  , _startTimeMs(0)
  , _lastProgressTimeMs(0)
  , _progressIntervalMs(100)
{
  // Initialize CRC tables
  CrcGenerateTable();
}

CParallelCompressor::~CParallelCompressor()
{
  Cleanup();
  _password.Wipe_and_Empty();
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
  _progress.Release();
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

Z7_COM7F_IMF(CParallelCompressor::SetPassword(const wchar_t *password))
{
  if (password && *password)
  {
    _password = password;
    _encryptionEnabled = true;
  }
  else
  {
    _password.Empty();
    _encryptionEnabled = false;
  }
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetSegmentSize(UInt64 segmentSize))
{
  _segmentSize = segmentSize;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetVolumeSize(UInt64 volumeSize))
{
  _volumeSize = volumeSize;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetVolumePrefix(const wchar_t *prefix))
{
  if (prefix && *prefix)
    _volumePrefix = prefix;
  else
    _volumePrefix.Empty();
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetSolidMode(bool solid))
{
  _solidMode = solid;
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetSolidBlockSize(UInt32 numFilesPerBlock))
{
  _solidBlockSize = numFilesPerBlock;
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
    
  if (_callback)
    _callback->OnItemStart(job.ItemIndex, job.Name);
  
  if (_callback)
  {
    if (_callback->ShouldCancel())
      return E_ABORT;
  }
  
  // Capture encoder properties for archive header (required for LZMA/LZMA2 decompression)
  // Some codecs (like Copy) don't have properties - this is normal
  CMyComPtr<ICompressWriteCoderProperties> writeProps;
  encoder->QueryInterface(IID_ICompressWriteCoderProperties, (void **)&writeProps);
  if (writeProps)
  {
    CDynBufSeqOutStream *propsStreamSpec = new CDynBufSeqOutStream;
    CMyComPtr<ISequentialOutStream> propsStream = propsStreamSpec;
    HRESULT propsResult = writeProps->WriteCoderProperties(propsStream);
    if (propsResult == S_OK && propsStreamSpec->GetSize() > 0)
    {
      size_t propsSize = propsStreamSpec->GetSize();
      job.EncoderProps.Alloc(propsSize);
      memcpy(job.EncoderProps, propsStreamSpec->GetBuffer(), propsSize);
    }
    // Note: If properties cannot be captured, EncoderProps remains empty
    // This is normal for codecs that don't require properties (e.g., Copy)
  }
  
  // Wrap input stream with CRC calculation
  CCrcInStream *crcStreamSpec = new CCrcInStream;
  CMyComPtr<ISequentialInStream> crcStream = crcStreamSpec;
  crcStreamSpec->Init(job.InStream);
    
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CLocalProgress *progressSpec = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = progressSpec;
  progressSpec->Init(NULL, false);
  
  UInt64 inSize = job.InSize;
  HRESULT result = encoder->Code(
      crcStream,  // Use CRC-calculating stream
      outStream,
      job.InSize > 0 ? &inSize : NULL,
      NULL,
      progress);
      
  if (result == S_OK)
  {
    job.OutSize = outStreamSpec->GetSize();
    job.CompressedData.Alloc((size_t)job.OutSize);
    if (job.OutSize > 0)
    {
      memcpy(job.CompressedData, outStreamSpec->GetBuffer(), (size_t)job.OutSize);
    }
    
    // Store CRC of uncompressed data
    job.Crc = crcStreamSpec->GetCRC();
    job.CrcDefined = true;
    
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
  _activeThreads++;  // Track active compression threads
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
    if (_activeThreads > 0)
      _activeThreads--;  // Track active compression threads
    if (job->Result != S_OK)
      _itemsFailed++;
    else
    {
      _totalInSize += job->InSize;
      _totalOutSize += job->OutSize;
    }
    if (_callback)
      _callback->OnItemComplete(job->ItemIndex, job->Result, job->InSize, job->OutSize);
    
    if (_progress)
      _progress->SetRatioInfo(&_totalInSize, &_totalOutSize);
    
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
  
  // Set encryption if password is defined
  if (_encryptionEnabled && !_password.IsEmpty())
  {
    method.PasswordIsDefined = true;
    method.Password = _password;
    
    // Add AES encryption method (same as main branch)
    NArchive::N7z::CMethodFull &aesMethod = method.Methods.AddNew();
    aesMethod.Id = NArchive::N7z::k_AES;
    aesMethod.NumStreams = 1;
  }
  else
  {
    method.PasswordIsDefined = false;
  }
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
    // Include CRC in file item (same as main branch)
    fileItem.CrcDefined = job.CrcDefined;
    fileItem.Crc = job.Crc;
    
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
    
    // Create folder for each file with encoder properties
    CFolder folder;
    CCoderInfo &coder = folder.Coders.AddNew();
    coder.MethodID = _methodId;
    coder.NumStreams = 1;
    // Copy encoder properties (required for decompression)
    if (job.EncoderProps.Size() > 0)
    {
      coder.Props.Alloc(job.EncoderProps.Size());
      memcpy(coder.Props, job.EncoderProps, job.EncoderProps.Size());
    }
    db.Folders.Add(folder);
    
    db.PackSizes.Add(packSizes[idx]);
    
    // Include CRC for pack data (same as main branch)
    db.PackCRCs.Defs.Add(job.CrcDefined);
    db.PackCRCs.Vals.Add(job.Crc);
    
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

// Solid mode compression - all files in a single compressed folder
// This provides better compression ratio at the cost of parallel processing
HRESULT CParallelCompressor::Create7zSolidArchive(ISequentialOutStream *outStream,
    CParallelInputItem *items, UInt32 numItems)
{
  using namespace NArchive::N7z;
  
  // Handle multi-volume output for solid archives
  ISequentialOutStream *finalOutStream = outStream;
  CMyComPtr<CMultiOutStream> multiStream;
  
  if (_volumeSize > 0 && !_volumePrefix.IsEmpty())
  {
    multiStream = new CMultiOutStream();
    CRecordVector<UInt64> volumeSizes;
    volumeSizes.Add(_volumeSize);
    multiStream->Init(volumeSizes);
    multiStream->Prefix = us2fs(_volumePrefix);
    multiStream->NeedDelete = false;
    finalOutStream = multiStream;
  }
  
  COutArchive outArchive;
  RINOK(outArchive.Create_and_WriteStartPrefix(finalOutStream))
  
  CArchiveDatabaseOut db;
  db.Clear();
  
  // Calculate total uncompressed size
  UInt64 totalUnpackSize = 0;
  for (UInt32 i = 0; i < numItems; i++)
  {
    totalUnpackSize += items[i].Size;
  }
  
  // Validate size to prevent excessive memory allocation
  const UInt64 kMaxSolidSize = (UInt64)4 * 1024 * 1024 * 1024;  // 4 GB limit
  if (totalUnpackSize > kMaxSolidSize)
  {
    return E_INVALIDARG;  // Size too large for solid compression
  }
  
  // Create concatenated input stream for solid compression
  // We'll use a memory buffer to concatenate all input streams
  CByteBuffer solidBuffer;
  solidBuffer.Alloc((size_t)totalUnpackSize);
  
  UInt64 offset = 0;
  
  // Use vectors instead of VLAs for C++ compatibility
  CRecordVector<UInt32> crcTable;
  CRecordVector<UInt64> sizes;
  crcTable.ClearAndSetSize(numItems);
  sizes.ClearAndSetSize(numItems);
  
  // Read all input streams into the solid buffer, computing CRCs
  const UInt32 kSolidReadBufferSize = 1 << 20;  // 1 MB
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    UInt32 crc = CRC_INIT_VAL;
    UInt64 remaining = items[i].Size;
    UInt64 itemOffset = offset;
    
    while (remaining > 0)
    {
      UInt32 toRead = (UInt32)(remaining > kSolidReadBufferSize ? kSolidReadBufferSize : remaining);
      UInt32 processed = 0;
      RINOK(items[i].InStream->Read(solidBuffer + offset, toRead, &processed))
      if (processed == 0)
        break;
      
      crc = CrcUpdate(crc, solidBuffer + offset, processed);
      offset += processed;
      remaining -= processed;
    }
    
    crcTable[i] = CRC_GET_DIGEST(crc);
    sizes[i] = offset - itemOffset;
  }
  
  // Create encoder and compress the solid block
  CMyComPtr<ICompressCoder> encoder;
  RINOK(CreateEncoder(&encoder));
  
  // Set up input stream from solid buffer
  CBufInStream *bufInStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;
  bufInStreamSpec->Init(solidBuffer, (size_t)offset, NULL);
  
  // Compress to memory buffer
  CDynBufSeqOutStream *compressedStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> compressedStream = compressedStreamSpec;
  
  UInt64 inSize = offset;
  RINOK(encoder->Code(bufInStream, compressedStream, &inSize, NULL, _progress))
  
  UInt64 compressedSize = compressedStreamSpec->GetSize();
  
  // Write compressed data to archive
  RINOK(WriteStream(finalOutStream, compressedStreamSpec->GetBuffer(), (size_t)compressedSize))
  
  // Get encoder properties
  CByteBuffer encoderProps;
  CMyComPtr<ICompressWriteCoderProperties> writeProps;
  encoder.QueryInterface(IID_ICompressWriteCoderProperties, &writeProps);
  if (writeProps)
  {
    CDynBufSeqOutStream *propsStreamSpec = new CDynBufSeqOutStream;
    CMyComPtr<ISequentialOutStream> propsStream = propsStreamSpec;
    writeProps->WriteCoderProperties(propsStream);
    encoderProps.Alloc(propsStreamSpec->GetSize());
    memcpy(encoderProps, propsStreamSpec->GetBuffer(), propsStreamSpec->GetSize());
  }
  
  // Build database with single solid folder containing all files
  CFolder folder;
  CCoderInfo &coder = folder.Coders.AddNew();
  coder.MethodID = _methodId;
  coder.NumStreams = 1;
  if (encoderProps.Size() > 0)
  {
    coder.Props.Alloc(encoderProps.Size());
    memcpy(coder.Props, encoderProps, encoderProps.Size());
  }
  
  db.Folders.Add(folder);
  db.PackSizes.Add(compressedSize);
  db.NumUnpackStreamsVector.Add(numItems);  // Multiple files in one folder = solid
  
  // Add unpack sizes for all files
  for (UInt32 i = 0; i < numItems; i++)
  {
    db.CoderUnpackSizes.Add(sizes[i]);
  }
  
  // Add file items
  for (UInt32 i = 0; i < numItems; i++)
  {
    CFileItem fileItem;
    fileItem.Size = sizes[i];
    fileItem.HasStream = (sizes[i] > 0);
    fileItem.IsDir = false;
    fileItem.CrcDefined = true;
    fileItem.Crc = crcTable[i];
    
    CFileItem2 fileItem2;
    fileItem2.MTime = items[i].ModificationTime.dwLowDateTime | 
                      ((UInt64)items[i].ModificationTime.dwHighDateTime << 32);
    fileItem2.MTimeDefined = true;
    fileItem2.AttribDefined = (items[i].Attributes != 0);
    fileItem2.Attrib = items[i].Attributes;
    fileItem2.CTimeDefined = false;
    fileItem2.ATimeDefined = false;
    fileItem2.StartPosDefined = false;
    fileItem2.IsAnti = false;
    
    UString name = items[i].Name ? items[i].Name : L"";
    db.AddFile(fileItem, fileItem2, name);
  }
  
  // Update statistics
  _itemsCompleted = numItems;
  _itemsFailed = 0;
  _totalInSize = offset;
  _totalOutSize = compressedSize;
  
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
  
  // Finalize multi-volume if used
  if (multiStream)
  {
    unsigned numVolumes = 0;
    RINOK(multiStream->FinalFlush_and_CloseFiles(numVolumes))
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::CompressMultiple(
    CParallelInputItem *items, UInt32 numItems,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress))
{
  if (!items || numItems == 0 || !outStream)
    return E_INVALIDARG;
    
  // Handle solid mode - use single-stream compression through 7z folder
  if (_solidMode)
  {
    return Create7zSolidArchive(outStream, items, numItems);
  }
    
  if (numItems == 1 && _numThreads <= 1)
    return CompressSingleStream(items[0].InStream, outStream, 
        items[0].Size > 0 ? &items[0].Size : NULL, progress);
  if (_workers.Size() == 0)
  {
    RINOK(Init());
  }
  
  _progress = progress;
  
  _nextJobIndex = 0;
  _itemsCompleted = 0;
  _itemsFailed = 0;
  _totalInSize = 0;
  _totalOutSize = 0;
  _itemsTotal = numItems;
  _activeThreads = 0;
  _startTimeMs = GetCurrentTimeMs();
  _lastProgressTimeMs = _startTimeMs;
  _completeEvent.Reset();
  
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
    _progress.Release();
    if (_callback)
      _callback->OnError(0, E_FAIL, L"All compression jobs failed");
    return E_FAIL;
  }
  
  // Handle multi-volume output
  ISequentialOutStream *finalOutStream = outStream;
  CMyComPtr<CMultiOutStream> multiStream;
  
  if (_volumeSize > 0 && !_volumePrefix.IsEmpty())
  {
    multiStream = new CMultiOutStream();
    CRecordVector<UInt64> volumeSizes;
    volumeSizes.Add(_volumeSize);  // All volumes same size
    multiStream->Init(volumeSizes);
    multiStream->Prefix = us2fs(_volumePrefix);
    multiStream->NeedDelete = false;
    finalOutStream = multiStream;
  }
  
  HRESULT archiveResult = Create7zArchive(finalOutStream, _jobs);
  
  // Finalize multi-volume if used
  if (multiStream)
  {
    unsigned numVolumes = 0;
    HRESULT volumeResult = multiStream->FinalFlush_and_CloseFiles(numVolumes);
    if (archiveResult == S_OK)
      archiveResult = volumeResult;
  }
  
  _progress.Release();
  
  if (archiveResult != S_OK)
  {
    if (_callback)
      _callback->OnError(0, archiveResult, L"Failed to create 7z archive");
    return archiveResult;
  }
  
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
    CProp &prop = _properties.AddNew();
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

void CParallelCompressor::UpdateDetailedStats(CParallelStatistics &stats)
{
  NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
  
  UInt64 currentTimeMs = GetCurrentTimeMs();
  UInt64 elapsedMs = currentTimeMs - _startTimeMs;
  
  stats.ItemsTotal = _itemsTotal;
  stats.ItemsCompleted = _itemsCompleted;
  stats.ItemsFailed = _itemsFailed;
  stats.ItemsInProgress = _activeThreads;
  stats.TotalInSize = _totalInSize;
  stats.TotalOutSize = _totalOutSize;
  stats.ElapsedTimeMs = elapsedMs;
  stats.ActiveThreads = _activeThreads;
  
  // Calculate throughput (bytes per second) with overflow protection
  if (elapsedMs > 0)
  {
    // Use division first to avoid overflow when totalInSize is large
    stats.BytesPerSecond = (_totalInSize / elapsedMs) * 1000 + 
                           ((_totalInSize % elapsedMs) * 1000) / elapsedMs;
    // Files per second * 100 for precision
    stats.FilesPerSecond = ((UInt64)_itemsCompleted * 100000) / elapsedMs;
  }
  else
  {
    stats.BytesPerSecond = 0;
    stats.FilesPerSecond = 0;
  }
  
  // Calculate compression ratio * 100
  if (_totalInSize > 0)
    stats.CompressionRatioX100 = (UInt32)((_totalOutSize * 100) / _totalInSize);
  else
    stats.CompressionRatioX100 = 100;
  
  // Estimate time remaining with overflow protection
  if (_itemsCompleted > 0 && _itemsCompleted < _itemsTotal)
  {
    UInt32 itemsRemaining = _itemsTotal - _itemsCompleted;
    // Use total elapsed time and remaining items to estimate, avoiding overflow
    stats.EstimatedTimeRemainingMs = (elapsedMs * (UInt64)itemsRemaining) / _itemsCompleted;
  }
  else
  {
    stats.EstimatedTimeRemainingMs = 0;
  }
}

Z7_COM7F_IMF(CParallelCompressor::GetDetailedStatistics(CParallelStatistics *stats))
{
  if (!stats)
    return E_POINTER;
  
  UpdateDetailedStats(*stats);
  return S_OK;
}

Z7_COM7F_IMF(CParallelCompressor::SetProgressUpdateInterval(UInt32 intervalMs))
{
  _progressIntervalMs = intervalMs > 0 ? intervalMs : 100;
  return S_OK;
}

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
