// Parallel7zOut.h - 7z Archive Format Integration for Parallel Compressor

#ifndef ZIP7_INC_PARALLEL_7Z_OUT_H
#define ZIP7_INC_PARALLEL_7Z_OUT_H

#include "../Archive/7z/7zOut.h"
#include "../Archive/7z/7zItem.h"
#include "ParallelCompressor.h"

namespace NCompress {
namespace NParallel {

class CParallel7zArchive
{
  NArchive::N7z::COutArchive _outArchive;
  NArchive::N7z::CArchiveDatabaseOut _db;
  
public:
  HRESULT Create(ISequentialOutStream *stream);
  HRESULT WriteDatabase(
      DECL_EXTERNAL_CODECS_LOC_VARS
      const CObjectVector<CCompressionJob> &jobs,
      const NArchive::N7z::CCompressionMethodMode *method);
  void Close();
  
  void AddCompressedItem(
      const CCompressionJob &job,
      const NArchive::N7z::CFileItem &fileItem,
      const NArchive::N7z::CFileItem2 &fileItem2);
};

HRESULT Create7zArchiveFromJobs(
    DECL_EXTERNAL_CODECS_LOC_VARS
    ISequentialOutStream *outStream,
    const CObjectVector<CCompressionJob> &jobs,
    const NArchive::N7z::CCompressionMethodMode *method,
    const NArchive::N7z::CHeaderOptions *headerOptions);

}}

#endif
