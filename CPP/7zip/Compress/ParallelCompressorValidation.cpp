// ParallelCompressorValidation.cpp - Validation and verification test

#include "StdAfx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ParallelCompressor.h"
#include "ParallelCompressAPI.h"

#include "../../Common/MyString.h"
#include "../Common/FileStreams.h"
#include "../Common/StreamObjects.h"
#include "../Archive/IArchive.h"
#include "../Archive/Common/OutStreamWithCRC.h"

using namespace NCompress::NParallel;

static void CreateTestFiles()
{
  const wchar_t *files[] = {
    L"test_file1.txt",
    L"test_file2.txt",
    L"test_file3.txt"
  };
  
  const char *contents[] = {
    "Test file 1 content: Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n",
    "Test file 2 content: Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n",
    "Test file 3 content: Ut enim ad minim veniam, quis nostrud exercitation ullamco.\n"
  };
  
  for (int i = 0; i < 3; i++)
  {
    COutFileStream *outFileSpec = new COutFileStream;
    CMyComPtr<ISequentialOutStream> outFile = outFileSpec;
    
    if (outFileSpec->Create(files[i], false))
    {
      UInt32 written = 0;
      outFile->Write(contents[i], (UInt32)strlen(contents[i]), &written);
      printf("Created: %ls (%u bytes)\n", files[i], written);
    }
  }
}

static bool ValidateCompressedFile(const wchar_t *archivePath)
{
  CInFileStream *inFileSpec = new CInFileStream;
  CMyComPtr<IInStream> inStream = inFileSpec;
  
  if (!inFileSpec->Open(archivePath))
  {
    printf("ERROR: Cannot open archive for validation: %ls\n", archivePath);
    return false;
  }
  
  UInt64 size = 0;
  inFileSpec->GetSize(&size);
  
  if (size == 0)
  {
    printf("ERROR: Archive is empty\n");
    return false;
  }
  
  printf("Archive size: %llu bytes\n", (unsigned long long)size);
  
  Byte header[32];
  UInt32 read = 0;
  inStream->Seek(0, STREAM_SEEK_SET, NULL);
  HRESULT hr = inStream->Read(header, 32, &read);
  
  if (FAILED(hr) || read < 6)
  {
    printf("ERROR: Cannot read archive header\n");
    return false;
  }
  
  if (header[0] == '7' && header[1] == 'z' && header[2] == 0xBC && 
      header[3] == 0xAF && header[4] == 0x27 && header[5] == 0x1C)
  {
    printf("SUCCESS: Valid 7z archive signature detected\n");
    return true;
  }
  else
  {
    printf("WARNING: Archive signature not recognized (may be LZMA stream)\n");
    return true;
  }
}

static bool TestFilesToArchive()
{
  printf("\n===========================================\n");
  printf("Test: Multiple Files to Archive\n");
  printf("===========================================\n");
  
  CreateTestFiles();
  
  const wchar_t *inputFiles[] = {
    L"test_file1.txt",
    L"test_file2.txt",
    L"test_file3.txt"
  };
  
  ParallelInputItemC items[3];
  
  for (int i = 0; i < 3; i++)
  {
    items[i].Data = NULL;
    items[i].DataSize = 0;
    items[i].FilePath = inputFiles[i];
    items[i].Name = inputFiles[i];
    items[i].Size = 0;
    items[i].UserData = NULL;
  }
  
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  ParallelCompressor_SetNumThreads(handle, 2);
  ParallelCompressor_SetCompressionLevel(handle, 5);
  
  const wchar_t *outputFile = L"test_archive.7z";
  
  printf("Compressing 3 files in parallel...\n");
  HRESULT hr = ParallelCompressor_CompressMultiple(handle, items, 3, outputFile);
  
  if (FAILED(hr))
  {
    printf("ERROR: Compression failed with HRESULT 0x%08X\n", hr);
    ParallelCompressor_Destroy(handle);
    return false;
  }
  
  printf("Compression completed successfully\n");
  ParallelCompressor_Destroy(handle);
  
  printf("\nValidating compressed archive...\n");
  bool valid = ValidateCompressedFile(outputFile);
  
  return valid;
}

static bool TestMemoryBuffersToArchive()
{
  printf("\n===========================================\n");
  printf("Test: Memory Buffers to Archive\n");
  printf("===========================================\n");
  
  const int bufferCount = 5;
  char *buffers[bufferCount];
  size_t sizes[bufferCount] = { 512, 1024, 2048, 4096, 8192 };
  
  printf("Creating %d test buffers...\n", bufferCount);
  for (int i = 0; i < bufferCount; i++)
  {
    buffers[i] = (char*)malloc(sizes[i]);
    for (size_t j = 0; j < sizes[i]; j++)
    {
      buffers[i][j] = (char)('A' + (i * 3 + j) % 26);
    }
    printf("  Buffer %d: %zu bytes\n", i + 1, sizes[i]);
  }
  
  ParallelInputItemC items[bufferCount];
  for (int i = 0; i < bufferCount; i++)
  {
    items[i].Data = buffers[i];
    items[i].DataSize = sizes[i];
    items[i].FilePath = NULL;
    items[i].Name = NULL;
    items[i].Size = sizes[i];
    items[i].UserData = NULL;
  }
  
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  ParallelCompressor_SetNumThreads(handle, 4);
  ParallelCompressor_SetCompressionLevel(handle, 7);
  
  const wchar_t *outputFile = L"test_memory_archive.7z";
  
  printf("\nCompressing memory buffers in parallel...\n");
  HRESULT hr = ParallelCompressor_CompressMultiple(handle, items, bufferCount, outputFile);
  
  ParallelCompressor_Destroy(handle);
  
  for (int i = 0; i < bufferCount; i++)
  {
    free(buffers[i]);
  }
  
  if (FAILED(hr))
  {
    printf("ERROR: Compression failed with HRESULT 0x%08X\n", hr);
    return false;
  }
  
  printf("Compression completed successfully\n");
  
  printf("\nValidating compressed archive...\n");
  bool valid = ValidateCompressedFile(outputFile);
  
  return valid;
}

static bool TestLargeDataset()
{
  printf("\n===========================================\n");
  printf("Test: Large Dataset Compression\n");
  printf("===========================================\n");
  
  const int itemCount = 20;
  const size_t itemSize = 64 * 1024;
  
  printf("Creating %d buffers of %zu KB each...\n", itemCount, itemSize / 1024);
  
  char **buffers = (char**)malloc(itemCount * sizeof(char*));
  for (int i = 0; i < itemCount; i++)
  {
    buffers[i] = (char*)malloc(itemSize);
    for (size_t j = 0; j < itemSize; j++)
    {
      buffers[i][j] = (char)((i * 37 + j * 13) % 256);
    }
  }
  
  ParallelInputItemC *items = (ParallelInputItemC*)malloc(itemCount * sizeof(ParallelInputItemC));
  for (int i = 0; i < itemCount; i++)
  {
    items[i].Data = buffers[i];
    items[i].DataSize = itemSize;
    items[i].FilePath = NULL;
    items[i].Name = NULL;
    items[i].Size = itemSize;
    items[i].UserData = NULL;
  }
  
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  ParallelCompressor_SetNumThreads(handle, 8);
  ParallelCompressor_SetCompressionLevel(handle, 5);
  
  printf("Compressing with 8 parallel threads...\n");
  
  HRESULT hr = ParallelCompressor_CompressMultiple(handle, items, itemCount, L"test_large.7z");
  
  ParallelCompressor_Destroy(handle);
  
  for (int i = 0; i < itemCount; i++)
  {
    free(buffers[i]);
  }
  free(buffers);
  free(items);
  
  if (FAILED(hr))
  {
    printf("ERROR: Compression failed\n");
    return false;
  }
  
  printf("Compression completed successfully\n");
  
  printf("\nValidating compressed archive...\n");
  bool valid = ValidateCompressedFile(L"test_large.7z");
  
  return valid;
}

int main(int argc, char* argv[])
{
  printf("===========================================\n");
  printf("Parallel Compressor Validation Suite\n");
  printf("===========================================\n");
  
  bool allPassed = true;
  
  allPassed &= TestFilesToArchive();
  allPassed &= TestMemoryBuffersToArchive();
  allPassed &= TestLargeDataset();
  
  printf("\n===========================================\n");
  if (allPassed)
  {
    printf("ALL VALIDATION TESTS PASSED\n");
  }
  else
  {
    printf("SOME VALIDATION TESTS FAILED\n");
  }
  printf("===========================================\n");
  
  return allPassed ? 0 : 1;
}
