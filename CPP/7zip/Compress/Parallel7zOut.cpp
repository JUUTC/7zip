// Parallel7zOut.cpp

#include "StdAfx.h"

#include "Parallel7zOut.h"
#include "../Archive/7z/7zHeader.h"
#include "../../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

namespace NCompress {
namespace NParallel {

using namespace NArchive::N7z;

HRESULT CParallel7zArchive::Create(ISequentialOutStream *stream)
{
  RINOK(_outArchive.Create_and_WriteStartPrefix(stream))
  _db.Clear();
  return S_OK;
}

void CParallel7zArchive::AddCompressedItem(
    const CCompressionJob &job,
    const CFileItem &fileItem,
    const CFileItem2 &fileItem2)
{
  _db.AddFile(fileItem, fileItem2, job.Name);
  _db.PackSizes.Add(job.OutSize);
  _db.PackCRCs.Vals.Add(0);
  _db.PackCRCs.Defs.Add(false);
}

HRESULT CParallel7zArchive::WriteDatabase(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CCompressionJob> &jobs,
    const CCompressionMethodMode *method)
{
  CHeaderOptions headerOptions;
  headerOptions.CompressMainHeader = true;
  
  RINOK(_outArchive.WriteDatabase(
      EXTERNAL_CODECS_LOC_VARS
      _db,
      method,
      headerOptions))
  
  return S_OK;
}

void CParallel7zArchive::Close()
{
  _outArchive.Close();
  _db.Clear();
}

HRESULT Create7zArchiveFromJobs(
    DECL_EXTERNAL_CODECS_LOC_VARS
    ISequentialOutStream *outStream,
    const CObjectVector<CCompressionJob> &jobs,
    const CCompressionMethodMode *method,
    const CHeaderOptions *headerOptions)
{
  CParallel7zArchive archive;
  RINOK(archive.Create(outStream))
  
  CFolder folder;
  folder.Coders.AddNew();
  
  CRecordVector<UInt64> unpackSizes;
  
  for (unsigned i = 0; i < jobs.Size(); i++)
  {
    const CCompressionJob &job = jobs[i];
    
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
    
    archive.AddCompressedItem(job, fileItem, fileItem2);
    unpackSizes.Add(job.InSize);
  }
  
  RINOK(archive.WriteDatabase(
      EXTERNAL_CODECS_LOC_VARS
      jobs,
      method))
  
  archive.Close();
  return S_OK;
}

}}
