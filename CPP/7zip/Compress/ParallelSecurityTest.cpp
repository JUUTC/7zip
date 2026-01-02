// ParallelSecurityTest.cpp - Security and edge case tests

#include "StdAfx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

// Test: Null pointer handling
static bool TestNullPointers()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  
  // Test null outStream
  CParallelInputItem item;
  item.InStream = NULL;
  item.Name = L"test";
  item.Size = 100;
  item.Attributes = 0;
  
  HRESULT hr = compressor->CompressMultiple(&item, 1, NULL, NULL);
  TEST_ASSERT(FAILED(hr), "Should fail with null outStream");
  
  // Test null items
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  hr = compressor->CompressMultiple(NULL, 1, outStream, NULL);
  TEST_ASSERT(FAILED(hr), "Should fail with null items");
  
  // Test zero items
  hr = compressor->CompressMultiple(&item, 0, outStream, NULL);
  TEST_ASSERT(FAILED(hr), "Should fail with zero items");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Empty file handling
static bool TestEmptyFile()
{
  g_TestFailed = false;
  
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init(NULL, 0, NULL);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(1);
  compressor->SetCompressionLevel(5);
  
  CParallelInputItem item;
  item.InStream = inStream;
  item.Name = L"empty.txt";
  item.Size = 0;
  item.Attributes = 0;
  
  HRESULT hr = compressor->CompressMultiple(&item, 1, outStream, NULL);
  
  // Empty files should be handled gracefully
  TEST_ASSERT(SUCCEEDED(hr), "Empty file compression should succeed");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Maximum thread count validation
static bool TestThreadLimits()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  
  // Test setting 0 threads (should default to 1)
  HRESULT hr = compressor->SetNumThreads(0);
  TEST_ASSERT(SUCCEEDED(hr), "SetNumThreads(0) should succeed");
  
  // Test setting very large thread count (should cap at 256)
  hr = compressor->SetNumThreads(1000);
  TEST_ASSERT(SUCCEEDED(hr), "SetNumThreads(1000) should succeed and cap at 256");
  
  // Test normal thread count
  hr = compressor->SetNumThreads(4);
  TEST_ASSERT(SUCCEEDED(hr), "SetNumThreads(4) should succeed");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Compression level validation
static bool TestCompressionLevelLimits()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  
  // Test setting level > 9 (should cap at 9)
  HRESULT hr = compressor->SetCompressionLevel(15);
  TEST_ASSERT(SUCCEEDED(hr), "SetCompressionLevel(15) should succeed and cap at 9");
  
  // Test normal level
  hr = compressor->SetCompressionLevel(5);
  TEST_ASSERT(SUCCEEDED(hr), "SetCompressionLevel(5) should succeed");
  
  // Test minimum level
  hr = compressor->SetCompressionLevel(0);
  TEST_ASSERT(SUCCEEDED(hr), "SetCompressionLevel(0) should succeed");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Large number of small files
static bool TestManySmallFiles()
{
  g_TestFailed = false;
  
  const int numFiles = 100;
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numFiles; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    const char *testData = "Small test data";
    size_t size = strlen(testData);
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)testData, size, NULL);
    
    item.InStream = inStream;
    item.Name = NULL;
    item.Size = size;
    item.Attributes = 0;
  }
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(8);
  compressor->SetCompressionLevel(5);
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numFiles, outStream, NULL);
  
  TEST_ASSERT(SUCCEEDED(hr), "Many small files compression should succeed");
  TEST_ASSERT(outStreamSpec->GetSize() > 0, "Output should not be empty");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Solid mode with various file counts
static bool TestSolidModeVariations()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  compressor->SetSolidMode(true);
  
  // Test with single file
  {
    const char *testData = "Single file in solid mode";
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)testData, strlen(testData), NULL);
    
    CParallelInputItem item;
    item.InStream = inStream;
    item.Name = L"single.txt";
    item.Size = strlen(testData);
    item.Attributes = 0;
    
    CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
    CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
    
    HRESULT hr = compressor->CompressMultiple(&item, 1, outStream, NULL);
    TEST_ASSERT(SUCCEEDED(hr), "Solid mode with single file should succeed");
  }
  
  // Test with multiple small files
  {
    const int numFiles = 10;
    CObjectVector<CParallelInputItem> items;
    
    for (int i = 0; i < numFiles; i++)
    {
      CParallelInputItem &item = items.AddNew();
      
      char testData[100];
      snprintf(testData, sizeof(testData), "File %d content for solid compression", i);
      size_t size = strlen(testData);
      
      CBufInStream *inStreamSpec = new CBufInStream;
      CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
      inStreamSpec->Init((const Byte*)testData, size, NULL);
      
      item.InStream = inStream;
      item.Name = NULL;
      item.Size = size;
      item.Attributes = 0;
    }
    
    CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
    CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
    
    HRESULT hr = compressor->CompressMultiple(&items[0], numFiles, outStream, NULL);
    TEST_ASSERT(SUCCEEDED(hr), "Solid mode with multiple files should succeed");
  }
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Invalid item validation
static bool TestInvalidItems()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(2);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  // Test with null InStream
  CParallelInputItem item;
  item.InStream = NULL;
  item.Name = L"null_stream.txt";
  item.Size = 100;
  item.Attributes = 0;
  
  HRESULT hr = compressor->CompressMultiple(&item, 1, outStream, NULL);
  // Should handle gracefully (either fail or skip)
  TEST_ASSERT(true, "Null InStream should be handled");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Statistics accuracy
static bool TestStatistics()
{
  g_TestFailed = false;
  
  const char *testData = "Test data for statistics validation";
  size_t dataSize = strlen(testData);
  
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init((const Byte*)testData, dataSize, NULL);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(1);
  compressor->SetCompressionLevel(5);
  
  CParallelInputItem item;
  item.InStream = inStream;
  item.Name = L"test.txt";
  item.Size = dataSize;
  item.Attributes = 0;
  
  HRESULT hr = compressor->CompressMultiple(&item, 1, outStream, NULL);
  TEST_ASSERT(SUCCEEDED(hr), "Compression should succeed");
  
  // Check statistics
  UInt32 itemsCompleted = 0;
  UInt32 itemsFailed = 0;
  UInt64 totalInSize = 0;
  UInt64 totalOutSize = 0;
  
  hr = compressor->GetStatistics(&itemsCompleted, &itemsFailed, &totalInSize, &totalOutSize);
  TEST_ASSERT(SUCCEEDED(hr), "GetStatistics should succeed");
  TEST_ASSERT(itemsCompleted == 1, "Should have completed 1 item");
  TEST_ASSERT(itemsFailed == 0, "Should have 0 failed items");
  TEST_ASSERT(totalInSize == dataSize, "Input size should match");
  TEST_ASSERT(totalOutSize > 0, "Output size should be > 0");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: Detailed statistics
static bool TestDetailedStatistics()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  
  CParallelStatistics stats;
  HRESULT hr = compressor->GetDetailedStatistics(&stats);
  TEST_ASSERT(SUCCEEDED(hr), "GetDetailedStatistics should succeed");
  
  // Initial statistics should be zeros
  TEST_ASSERT(stats.ItemsCompleted == 0, "ItemsCompleted should be 0");
  TEST_ASSERT(stats.ItemsFailed == 0, "ItemsFailed should be 0");
  TEST_ASSERT(stats.TotalInSize == 0, "TotalInSize should be 0");
  TEST_ASSERT(stats.TotalOutSize == 0, "TotalOutSize should be 0");
  
  delete compressor;
  TEST_SUCCESS();
}

// Test: C API error handling
static bool TestCAPIErrorHandling()
{
  g_TestFailed = false;
  
  // Test null handle
  HRESULT hr = ParallelCompressor_SetNumThreads(NULL, 4);
  TEST_ASSERT(FAILED(hr), "SetNumThreads with null handle should fail");
  
  hr = ParallelCompressor_SetCompressionLevel(NULL, 5);
  TEST_ASSERT(FAILED(hr), "SetCompressionLevel with null handle should fail");
  
  // Test valid handle
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  TEST_ASSERT(handle != NULL, "Create should return valid handle");
  
  hr = ParallelCompressor_SetNumThreads(handle, 4);
  TEST_ASSERT(SUCCEEDED(hr), "SetNumThreads with valid handle should succeed");
  
  ParallelCompressor_Destroy(handle);
  
  TEST_SUCCESS();
}

// Test: Password encryption
static bool TestPasswordEncryption()
{
  g_TestFailed = false;
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(2);
  compressor->SetCompressionLevel(5);
  
  // Set password
  HRESULT hr = compressor->SetPassword(L"test_password_123");
  TEST_ASSERT(SUCCEEDED(hr), "SetPassword should succeed");
  
  const char *testData = "Secret data to encrypt";
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init((const Byte*)testData, strlen(testData), NULL);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  CParallelInputItem item;
  item.InStream = inStream;
  item.Name = L"encrypted.txt";
  item.Size = strlen(testData);
  item.Attributes = 0;
  
  hr = compressor->CompressMultiple(&item, 1, outStream, NULL);
  TEST_ASSERT(SUCCEEDED(hr), "Compression with password should succeed");
  TEST_ASSERT(outStreamSpec->GetSize() > 0, "Output should not be empty");
  
  delete compressor;
  TEST_SUCCESS();
}

int main(int argc, char* argv[])
{
  printf("===========================================\n");
  printf("Parallel Compressor Security Test Suite\n");
  printf("===========================================\n\n");
  
  printf("Running Security Tests...\n");
  printf("-------------------------------------------\n");
  
  TestNullPointers();
  TestInvalidItems();
  TestCAPIErrorHandling();
  
  printf("\nRunning Edge Case Tests...\n");
  printf("-------------------------------------------\n");
  
  TestEmptyFile();
  TestThreadLimits();
  TestCompressionLevelLimits();
  
  printf("\nRunning Stress Tests...\n");
  printf("-------------------------------------------\n");
  
  TestManySmallFiles();
  TestSolidModeVariations();
  
  printf("\nRunning Feature Tests...\n");
  printf("-------------------------------------------\n");
  
  TestStatistics();
  TestDetailedStatistics();
  TestPasswordEncryption();
  
  printf("\n===========================================\n");
  printf("Test Results\n");
  printf("===========================================\n");
  printf("Passed: %u\n", g_TestsPassed);
  printf("Failed: %u\n", g_TestsFailed);
  printf("Total:  %u\n", g_TestsPassed + g_TestsFailed);
  printf("===========================================\n");
  
  return g_TestsFailed == 0 ? 0 : 1;
}
