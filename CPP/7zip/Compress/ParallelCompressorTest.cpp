// ParallelCompressorTest.cpp - Comprehensive test suite

#include "StdAfx.h"

#include <stdio.h>
#include <string.h>

#include "ParallelCompressor.h"
#include "ParallelCompressAPI.h"

#include "../../Common/MyString.h"
#include "../Common/FileStreams.h"
#include "../Common/StreamObjects.h"

using namespace NCompress::NParallel;

static bool g_TestFailed = false;
static unsigned g_TestsPassed = 0;
static unsigned g_TestsFailed = 0;

#define TEST_ASSERT(condition, message) \
  if (!(condition)) { \
    printf("FAIL: %s - %s\n", __FUNCTION__, message); \
    g_TestFailed = true; \
    g_TestsFailed++; \
    return false; \
  }

#define TEST_SUCCESS() \
  if (!g_TestFailed) { \
    printf("PASS: %s\n", __FUNCTION__); \
    g_TestsPassed++; \
    return true; \
  } \
  return false;

static bool TestBasicCompression()
{
  g_TestFailed = false;
  
  const char *testData = "Hello, this is test data for parallel compression!";
  size_t dataSize = strlen(testData);
  
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init((const Byte*)testData, dataSize, NULL);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(1);
  compressor->SetCompressionLevel(5);
  
  UInt64 inSize = dataSize;
  HRESULT hr = compressor->Code(inStream, outStream, &inSize, NULL, NULL);
  
  TEST_ASSERT(SUCCEEDED(hr), "Compression failed");
  TEST_ASSERT(outStreamSpec->GetSize() > 0, "Output size is zero");
  TEST_ASSERT(outStreamSpec->GetSize() < dataSize, "Compressed size not smaller");
  
  delete compressor;
  TEST_SUCCESS();
}

static bool TestMultipleStreams()
{
  g_TestFailed = false;
  
  const int numStreams = 5;
  const char *testData[] = {
    "Stream 1: First test data",
    "Stream 2: Second test data with more content",
    "Stream 3: Third stream",
    "Stream 4: Fourth stream with lots of data to compress efficiently",
    "Stream 5: Final test stream"
  };
  
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numStreams; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    size_t size = strlen(testData[i]);
    inStreamSpec->Init((const Byte*)testData[i], size, NULL);
    
    item.InStream = inStream;
    item.Name = NULL;
    item.Size = size;
    item.Attributes = 0;
  }
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numStreams, outStream, NULL);
  
  TEST_ASSERT(SUCCEEDED(hr), "Multi-stream compression failed");
  TEST_ASSERT(outStreamSpec->GetSize() > 0, "Output size is zero");
  
  UInt32 completed, failed;
  compressor->GetStatistics(&completed, &failed, NULL, NULL);
  TEST_ASSERT(completed == numStreams, "Not all streams completed");
  TEST_ASSERT(failed == 0, "Some streams failed");
  
  delete compressor;
  TEST_SUCCESS();
}

static bool TestAutoDetection()
{
  g_TestFailed = false;
  
  const char *testData = "Test data for auto-detection";
  size_t dataSize = strlen(testData);
  
  CParallelCompressor *compressor = new CParallelCompressor();
  
  compressor->SetNumThreads(1);
  
  CBufInStream *inStreamSpec1 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream1 = inStreamSpec1;
  inStreamSpec1->Init((const Byte*)testData, dataSize, NULL);
  
  CDynBufSeqOutStream *outStreamSpec1 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream1 = outStreamSpec1;
  
  UInt64 inSize = dataSize;
  HRESULT hr1 = compressor->Code(inStream1, outStream1, &inSize, NULL, NULL);
  TEST_ASSERT(SUCCEEDED(hr1), "Single-thread compression failed");
  
  size_t singleThreadSize = outStreamSpec1->GetSize();
  
  compressor->SetNumThreads(4);
  
  CBufInStream *inStreamSpec2 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream2 = inStreamSpec2;
  inStreamSpec2->Init((const Byte*)testData, dataSize, NULL);
  
  CDynBufSeqOutStream *outStreamSpec2 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream2 = outStreamSpec2;
  
  HRESULT hr2 = compressor->Code(inStream2, outStream2, &inSize, NULL, NULL);
  TEST_ASSERT(SUCCEEDED(hr2), "Multi-thread compression failed");
  
  TEST_ASSERT(outStreamSpec2->GetSize() > 0, "Multi-thread output is zero");
  
  delete compressor;
  TEST_SUCCESS();
}

static bool TestCompressionMethods()
{
  g_TestFailed = false;
  
  const char *testData = "Test data for different compression methods";
  size_t dataSize = strlen(testData);
  
  CMethodId methods[] = {
    0x030101,  // LZMA
    0x030901   // LZMA2
  };
  
  for (int i = 0; i < 2; i++)
  {
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)testData, dataSize, NULL);
    
    CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
    CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
    
    CParallelCompressor *compressor = new CParallelCompressor();
    compressor->SetNumThreads(2);
    compressor->SetCompressionLevel(5);
    compressor->SetCompressionMethod(&methods[i]);
    
    UInt64 inSize = dataSize;
    HRESULT hr = compressor->Code(inStream, outStream, &inSize, NULL, NULL);
    
    TEST_ASSERT(SUCCEEDED(hr), "Compression with different method failed");
    TEST_ASSERT(outStreamSpec->GetSize() > 0, "Output size is zero");
    
    delete compressor;
  }
  
  TEST_SUCCESS();
}

static bool TestFileCompression()
{
  g_TestFailed = false;
  
  const wchar_t *testFile = L"test_input.txt";
  const wchar_t *compressedFile = L"test_output.7z";
  
  {
    COutFileStream *outFileSpec = new COutFileStream;
    CMyComPtr<ISequentialOutStream> outFile = outFileSpec;
    
    if (!outFileSpec->Create(testFile, false))
    {
      TEST_ASSERT(false, "Failed to create test input file");
    }
    
    const char *testData = "This is test file content for compression testing.\n"
                          "It has multiple lines.\n"
                          "And various content to test real file compression.\n";
    
    UInt32 written = 0;
    outFile->Write(testData, (UInt32)strlen(testData), &written);
  }
  
  CInFileStream *inFileSpec = new CInFileStream;
  CMyComPtr<ISequentialInStream> inFile = inFileSpec;
  
  if (!inFileSpec->Open(testFile))
  {
    TEST_ASSERT(false, "Failed to open test input file");
  }
  
  UInt64 fileSize = 0;
  inFileSpec->GetSize(&fileSize);
  
  COutFileStream *outFileSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> outFile = outFileSpec;
  
  if (!outFileSpec->Create(compressedFile, false))
  {
    TEST_ASSERT(false, "Failed to create compressed output file");
  }
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(2);
  compressor->SetCompressionLevel(5);
  
  HRESULT hr = compressor->Code(inFile, outFile, &fileSize, NULL, NULL);
  
  TEST_ASSERT(SUCCEEDED(hr), "File compression failed");
  
  delete compressor;
  
  TEST_SUCCESS();
}

static bool TestCAPI()
{
  g_TestFailed = false;
  
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  TEST_ASSERT(handle != NULL, "Failed to create compressor via C API");
  
  HRESULT hr = ParallelCompressor_SetNumThreads(handle, 2);
  TEST_ASSERT(SUCCEEDED(hr), "SetNumThreads failed");
  
  hr = ParallelCompressor_SetCompressionLevel(handle, 5);
  TEST_ASSERT(SUCCEEDED(hr), "SetCompressionLevel failed");
  
  ParallelCompressor_Destroy(handle);
  
  TEST_SUCCESS();
}

static bool TestMemoryBuffer()
{
  g_TestFailed = false;
  
  const int bufferCount = 3;
  char *buffers[bufferCount];
  size_t sizes[bufferCount];
  
  for (int i = 0; i < bufferCount; i++)
  {
    sizes[i] = 1024;
    buffers[i] = (char*)malloc(sizes[i]);
    memset(buffers[i], 'A' + i, sizes[i]);
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
  ParallelCompressor_SetNumThreads(handle, 2);
  ParallelCompressor_SetCompressionLevel(handle, 5);
  
  void *outputBuffer = NULL;
  size_t outputSize = 0;
  
  HRESULT hr = ParallelCompressor_CompressMultipleToMemory(
      handle, items, bufferCount, &outputBuffer, &outputSize);
  
  TEST_ASSERT(SUCCEEDED(hr), "Memory buffer compression failed");
  TEST_ASSERT(outputBuffer != NULL, "Output buffer is NULL");
  TEST_ASSERT(outputSize > 0, "Output size is zero");
  
  free(outputBuffer);
  ParallelCompressor_Destroy(handle);
  
  for (int i = 0; i < bufferCount; i++)
  {
    free(buffers[i]);
  }
  
  TEST_SUCCESS();
}

int main(int argc, char* argv[])
{
  printf("===========================================\n");
  printf("Parallel Compressor Test Suite\n");
  printf("===========================================\n\n");
  
  printf("Running Unit Tests...\n");
  printf("-------------------------------------------\n");
  
  TestBasicCompression();
  TestAutoDetection();
  TestCompressionMethods();
  TestCAPI();
  
  printf("\nRunning Integration Tests...\n");
  printf("-------------------------------------------\n");
  
  TestMultipleStreams();
  TestMemoryBuffer();
  
  printf("\nRunning End-to-End Tests...\n");
  printf("-------------------------------------------\n");
  
  TestFileCompression();
  
  printf("\n===========================================\n");
  printf("Test Results\n");
  printf("===========================================\n");
  printf("Passed: %u\n", g_TestsPassed);
  printf("Failed: %u\n", g_TestsFailed);
  printf("Total:  %u\n", g_TestsPassed + g_TestsFailed);
  printf("===========================================\n");
  
  return g_TestsFailed == 0 ? 0 : 1;
}
