// ParallelCompressor.h

#ifndef ZIP7_INC_PARALLEL_COMPRESSOR_H
#define ZIP7_INC_PARALLEL_COMPRESSOR_H

#include "../../Common/MyCom.h"
#include "../../Common/MyVector.h"
#include "../../Common/MyString.h"

#include "../Windows/Synchronization.h"
#include "../Windows/Thread.h"

#include "IParallelCompress.h"
#include "ICoder.h"
#include "IPassword.h"
#include "../Common/CreateCoder.h"
#include "../Archive/7z/7zOut.h"
#include "../Archive/7z/7zItem.h"
#include "../Archive/7z/7zCompressionMode.h"
#include "../Archive/7z/7zHeader.h"

namespace NCompress {
namespace NParallel {

class CParallelCompressor;

struct CCompressionJob
{
  UInt32 ItemIndex;
  CMyComPtr<ISequentialInStream> InStream;
  UString Name;
  UInt64 InSize;
  UInt64 OutSize;
  UInt32 Attributes;
  FILETIME ModTime;
  void *UserData;
  HRESULT Result;
  bool Completed;
  CByteBuffer CompressedData;
  CCompressionJob(): ItemIndex(0), InSize(0), OutSize(0),
      Attributes(0), UserData(NULL), Result(S_OK), Completed(false)
  {
    ModTime.dwLowDateTime = 0;
    ModTime.dwHighDateTime = 0;
  }
};

class CCompressWorker
{
public:
  CParallelCompressor *Compressor;
  UInt32 ThreadIndex;
  NWindows::NSynchronization::CAutoResetEvent StartEvent;
  NWindows::CThread Thread;
  CCompressionJob *CurrentJob;
  volatile bool StopFlag;
  
  CCompressWorker(): Compressor(NULL), ThreadIndex(0), CurrentJob(NULL), StopFlag(false) {}
  
  HRESULT Create();
  void Stop();
  static THREAD_FUNC_DECL ThreadFunc(void *param);
  HRESULT ProcessJob();
};

Z7_CLASS_IMP_COM_6(
  CParallelCompressor,
  IParallelCompressor,
  ICompressCoder,
  ICompressSetCoderProperties,
  ICompressWriteCoderProperties,
  ICompressSetCoderPropertiesOpt,
  ICompressGetInStreamProcessedSize
)
  UInt32 _numThreads;
  UInt32 _compressionLevel;
  UInt64 _segmentSize;
  bool _encryptionEnabled;
  CByteBuffer _encryptionKey;
  CByteBuffer _encryptionIV;
  CMyComPtr<IParallelCompressCallback> _callback;
  CMyComPtr<ICompressProgressInfo> _progress;
  CObjectVector<CCompressWorker> _workers;
  CObjectVector<CCompressionJob> _jobs;
  UInt32 _nextJobIndex;
  NWindows::NSynchronization::CCriticalSection _criticalSection;
  NWindows::NSynchronization::CManualResetEvent _completeEvent;
  CMethodId _methodId;
  CObjectVector<CProperty> _properties;
  UInt32 _itemsCompleted;
  UInt32 _itemsFailed;
  UInt64 _totalInSize;
  UInt64 _totalOutSize;
  DECL_EXTERNAL_CODECS_LOC_VARS
  HRESULT CreateEncoder(ICompressCoder **encoder);
  HRESULT CompressJob(CCompressionJob &job, ICompressCoder *encoder);
  HRESULT CompressSingleStream(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, ICompressProgressInfo *progress);
  CCompressionJob* GetNextJob();
  void NotifyJobComplete(CCompressionJob *job);
  HRESULT WriteJobToStream(CCompressionJob &job, ISequentialOutStream *outStream);
  HRESULT Create7zArchive(ISequentialOutStream *outStream,
      const CObjectVector<CCompressionJob> &jobs);
  void PrepareCompressionMethod(NArchive::N7z::CCompressionMethodMode &method);
public:
  CParallelCompressor();
  ~CParallelCompressor();
  HRESULT Init();
  void Cleanup();
};

Z7_CLASS_IMP_COM_1(
  CParallelStreamQueue,
  IParallelStreamQueue
)
  CMyComPtr<IParallelCompressor> _compressor;
  CObjectVector<CParallelInputItem> _queuedItems;
  UInt32 _maxQueueSize;
  bool _processing;
  NWindows::NSynchronization::CCriticalSection _queueLock;
  UInt32 _itemsProcessed;
  UInt32 _itemsFailed;
public:
  CParallelStreamQueue();
  ~CParallelStreamQueue();
};

}}

#endif
