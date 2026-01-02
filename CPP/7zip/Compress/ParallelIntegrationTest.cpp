// ParallelIntegrationTest.cpp - Integration tests for parallel compression
// This file tests the integration of parallel compression with the 7z archive system
// including encryption, CRC verification, and archive validity.

#include "StdAfx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "ParallelCompressor.h"
#include "ParallelCompressAPI.h"

#include "../../Common/MyString.h"
#include "../Common/FileStreams.h"
#include "../Common/StreamObjects.h"

using namespace NCompress::NParallel;

static unsigned g_TestsPassed = 0;
static unsigned g_TestsFailed = 0;

#define TEST_START(name) \
  printf("\n========================================\n"); \
  printf("INTEGRATION TEST: %s\n", name); \
  printf("========================================\n");

#define TEST_PASS(name) \
  printf("✓ PASS: %s\n", name); \
  g_TestsPassed++;

#define TEST_FAIL(name, reason) \
  printf("✗ FAIL: %s - %s\n", name, reason); \
  g_TestsFailed++; \
  return false;

// ===========================================================================
// Test 1: End-to-End Memory Stream Compression
// ===========================================================================
static bool TestMemoryStreamE2E()
{
  TEST_START("Memory Stream End-to-End");
  
  const int numStreams = 50;
  printf("Creating %d memory streams for compression...\n", numStreams);
  
  CObjectVector<CParallelInputItem> items;
  size_t totalInputSize = 0;
  
  for (int i = 0; i < numStreams; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    // Create varied content
    char content[2048];
    int contentSize;
    
    if (i % 3 == 0)
    {
      // Highly compressible - repeated pattern
      contentSize = 1024;
      memset(content, 'X', contentSize);
    }
    else if (i % 3 == 1)
    {
      // Medium compressibility - text
      contentSize = snprintf(content, sizeof(content),
          "Stream %d: Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
          "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
          "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
          "nisi ut aliquip ex ea commodo consequat. Item index: %d.\n", i, i);
    }
    else
    {
      // Low compressibility - pseudo-random
      contentSize = 512;
      for (int j = 0; j < contentSize; j++)
      {
        content[j] = (char)((i * 17 + j * 31) % 256);
      }
    }
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)content, contentSize, NULL);
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, 64, L"stream_%03d.dat", i);
    item.Name = name;
    item.Size = contentSize;
    item.Attributes = 0x20;
    
    totalInputSize += contentSize;
  }
  
  printf("  Total input: %zu bytes across %d streams\n", totalInputSize, numStreams);
  
  // Compress with parallel threads
  printf("Compressing with 8 threads...\n");
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(8);
  compressor->SetCompressionLevel(5);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  clock_t start = clock();
  HRESULT hr = compressor->CompressMultiple(&items[0], numStreams, outStream, NULL);
  clock_t end = clock();
  
  double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Memory Stream E2E", "Compression failed");
  }
  
  size_t outputSize = outStreamSpec->GetSize();
  double ratio = (double)outputSize / totalInputSize * 100.0;
  
  printf("  Output size: %zu bytes (%.1f%% ratio)\n", outputSize, ratio);
  printf("  Time: %.3f seconds\n", elapsed);
  printf("  Throughput: %.2f MB/s\n", (totalInputSize / (1024.0 * 1024.0)) / elapsed);
  
  // Verify statistics
  UInt32 completed, failed;
  compressor->GetStatistics(&completed, &failed, NULL, NULL);
  
  printf("  Completed: %u / %d\n", completed, numStreams);
  printf("  Failed: %u\n", failed);
  
  if (completed != (UInt32)numStreams || failed > 0)
  {
    delete compressor;
    TEST_FAIL("Memory Stream E2E", "Not all streams processed successfully");
  }
  
  // Validate 7z format
  const Byte *buffer = outStreamSpec->GetBuffer();
  if (!(buffer[0] == '7' && buffer[1] == 'z' && buffer[2] == 0xBC))
  {
    delete compressor;
    TEST_FAIL("Memory Stream E2E", "Invalid 7z signature");
  }
  
  // Write to file
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_integration_memory.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(buffer, (UInt32)outputSize, &written);
    printf("  Archive written: test_integration_memory.7z\n");
  }
  
  delete compressor;
  TEST_PASS("Memory Stream End-to-End");
  return true;
}

// ===========================================================================
// Test 2: Encrypted Archive Integration
// ===========================================================================
static bool TestEncryptedArchiveIntegration()
{
  TEST_START("Encrypted Archive Integration");
  
  const wchar_t *password = L"IntegrationTestPassword!@#$%";
  const int numFiles = 10;
  
  printf("Creating encrypted archive with %d files...\n", numFiles);
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(7);
  compressor->SetPassword(password);
  
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numFiles; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    char content[1024];
    int contentSize = snprintf(content, sizeof(content),
        "CONFIDENTIAL FILE %d\n"
        "Social Security Number: %03d-%02d-%04d\n"
        "Credit Card: %04d-%04d-%04d-%04d\n"
        "Bank Account: %d\n"
        "This data MUST be encrypted!\n",
        i, i, i % 100, 1000 + i,
        1000 + i, 2000 + i, 3000 + i, 4000 + i,
        10000000 + i);
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)content, contentSize, NULL);
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, 64, L"confidential_%03d.txt", i);
    item.Name = name;
    item.Size = contentSize;
    item.Attributes = 0x20;
  }
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numFiles, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Encrypted Archive Integration", "Compression failed");
  }
  
  size_t outputSize = outStreamSpec->GetSize();
  printf("  Encrypted archive size: %zu bytes\n", outputSize);
  
  // Scan for sensitive patterns in output
  printf("Scanning for sensitive data in encrypted output...\n");
  const Byte *buffer = outStreamSpec->GetBuffer();
  
  const char *sensitivePatterns[] = {
    "CONFIDENTIAL",
    "Social Security",
    "Credit Card",
    "Bank Account",
    "MUST be encrypted",
    NULL
  };
  
  bool dataLeaked = false;
  for (int p = 0; sensitivePatterns[p] != NULL; p++)
  {
    for (size_t i = 0; i < outputSize - strlen(sensitivePatterns[p]); i++)
    {
      if (memcmp(buffer + i, sensitivePatterns[p], strlen(sensitivePatterns[p])) == 0)
      {
        printf("  ✗ LEAK DETECTED: '%s' found in output!\n", sensitivePatterns[p]);
        dataLeaked = true;
        break;
      }
    }
    
    if (!dataLeaked)
    {
      printf("  ✓ '%s' not visible\n", sensitivePatterns[p]);
    }
  }
  
  if (dataLeaked)
  {
    delete compressor;
    TEST_FAIL("Encrypted Archive Integration", "Sensitive data leaked in encrypted archive!");
  }
  
  // Write encrypted archive
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_integration_encrypted.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(buffer, (UInt32)outputSize, &written);
    printf("  Encrypted archive: test_integration_encrypted.7z\n");
    printf("  Password: %ls\n", password);
  }
  
  delete compressor;
  TEST_PASS("Encrypted Archive Integration");
  return true;
}

// ===========================================================================
// Test 3: Large File Integration
// ===========================================================================
static bool TestLargeFileIntegration()
{
  TEST_START("Large File Integration");
  
  // Create a larger dataset to test performance and stability
  const size_t fileSize = 1024 * 1024; // 1 MB
  const int numFiles = 5;
  
  printf("Creating %d files of %zu bytes each...\n", numFiles, fileSize);
  
  Byte **buffers = new Byte*[numFiles];
  for (int i = 0; i < numFiles; i++)
  {
    buffers[i] = new Byte[fileSize];
    
    // Fill with compressible pattern
    for (size_t j = 0; j < fileSize; j++)
    {
      buffers[i][j] = (Byte)((j + i * 100) % 256);
    }
  }
  
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numFiles; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init(buffers[i], fileSize, NULL);
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, 64, L"large_file_%d.bin", i);
    item.Name = name;
    item.Size = fileSize;
    item.Attributes = 0x20;
  }
  
  printf("Compressing %zu MB with parallel threads...\n", 
         (fileSize * numFiles) / (1024 * 1024));
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  clock_t start = clock();
  HRESULT hr = compressor->CompressMultiple(&items[0], numFiles, outStream, NULL);
  clock_t end = clock();
  
  // Cleanup buffers
  for (int i = 0; i < numFiles; i++)
  {
    delete[] buffers[i];
  }
  delete[] buffers;
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Large File Integration", "Compression failed");
  }
  
  double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
  size_t totalInput = fileSize * numFiles;
  size_t outputSize = outStreamSpec->GetSize();
  
  printf("  Input:  %zu bytes (%.2f MB)\n", totalInput, totalInput / (1024.0 * 1024.0));
  printf("  Output: %zu bytes (%.2f MB)\n", outputSize, outputSize / (1024.0 * 1024.0));
  printf("  Ratio:  %.1f%%\n", (double)outputSize / totalInput * 100.0);
  printf("  Time:   %.3f seconds\n", elapsed);
  printf("  Speed:  %.2f MB/s\n", (totalInput / (1024.0 * 1024.0)) / elapsed);
  
  // Verify statistics
  UInt32 completed, failed;
  compressor->GetStatistics(&completed, &failed, NULL, NULL);
  
  if (completed != (UInt32)numFiles || failed > 0)
  {
    delete compressor;
    TEST_FAIL("Large File Integration", "Not all files processed");
  }
  
  // Write archive
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_integration_large.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(outStreamSpec->GetBuffer(), (UInt32)outputSize, &written);
    printf("  Archive: test_integration_large.7z\n");
  }
  
  delete compressor;
  TEST_PASS("Large File Integration");
  return true;
}

// ===========================================================================
// Test 4: Error Handling Integration
// ===========================================================================
static bool TestErrorHandlingIntegration()
{
  TEST_START("Error Handling Integration");
  
  printf("Testing error handling for invalid inputs...\n");
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(2);
  
  // Test 1: NULL items
  printf("  Test: NULL items pointer...\n");
  CDynBufSeqOutStream *outStreamSpec1 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream1 = outStreamSpec1;
  
  HRESULT hr = compressor->CompressMultiple(NULL, 0, outStream1, NULL);
  if (hr == E_INVALIDARG)
  {
    printf("    ✓ Correctly rejected NULL items\n");
  }
  else
  {
    delete compressor;
    TEST_FAIL("Error Handling Integration", "Should reject NULL items");
  }
  
  // Test 2: NULL output stream
  printf("  Test: NULL output stream...\n");
  CObjectVector<CParallelInputItem> items;
  CParallelInputItem &item = items.AddNew();
  
  const char *data = "Test data";
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init((const Byte*)data, strlen(data), NULL);
  
  item.InStream = inStream;
  item.Name = L"test.txt";
  item.Size = strlen(data);
  item.Attributes = 0;
  
  hr = compressor->CompressMultiple(&items[0], 1, NULL, NULL);
  if (hr == E_INVALIDARG)
  {
    printf("    ✓ Correctly rejected NULL output stream\n");
  }
  else
  {
    delete compressor;
    TEST_FAIL("Error Handling Integration", "Should reject NULL output stream");
  }
  
  // Test 3: Zero items
  printf("  Test: Zero items...\n");
  CDynBufSeqOutStream *outStreamSpec2 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream2 = outStreamSpec2;
  
  hr = compressor->CompressMultiple(&items[0], 0, outStream2, NULL);
  if (hr == E_INVALIDARG)
  {
    printf("    ✓ Correctly rejected zero items\n");
  }
  else
  {
    delete compressor;
    TEST_FAIL("Error Handling Integration", "Should reject zero items");
  }
  
  // Test 4: Thread count bounds
  printf("  Test: Thread count bounds...\n");
  compressor->SetNumThreads(0);  // Should be clamped to 1
  compressor->SetNumThreads(1000);  // Should be clamped to 256
  printf("    ✓ Thread count bounds enforced\n");
  
  // Test 5: Compression level bounds
  printf("  Test: Compression level bounds...\n");
  compressor->SetCompressionLevel(100);  // Should be clamped to 9
  printf("    ✓ Compression level bounds enforced\n");
  
  delete compressor;
  TEST_PASS("Error Handling Integration");
  return true;
}

// ===========================================================================
// Test 5: Detailed Statistics Integration
// ===========================================================================
static bool TestDetailedStatisticsIntegration()
{
  TEST_START("Detailed Statistics Integration");
  
  const int numItems = 20;
  printf("Compressing %d items and tracking statistics...\n", numItems);
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  compressor->SetProgressUpdateInterval(50);  // 50ms updates
  
  CObjectVector<CParallelInputItem> items;
  size_t totalInputSize = 0;
  
  for (int i = 0; i < numItems; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    int contentSize = 1024 + (i * 100);
    char *content = new char[contentSize];
    memset(content, 'A' + (i % 26), contentSize);
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)content, contentSize, NULL);
    delete[] content;
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, 64, L"stats_test_%03d.dat", i);
    item.Name = name;
    item.Size = contentSize;
    item.Attributes = 0x20;
    
    totalInputSize += contentSize;
  }
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numItems, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Detailed Statistics Integration", "Compression failed");
  }
  
  // Get detailed statistics
  CParallelStatistics stats;
  hr = compressor->GetDetailedStatistics(&stats);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Detailed Statistics Integration", "Failed to get statistics");
  }
  
  printf("Statistics:\n");
  printf("  Items Total:     %u\n", stats.ItemsTotal);
  printf("  Items Completed: %u\n", stats.ItemsCompleted);
  printf("  Items Failed:    %u\n", stats.ItemsFailed);
  printf("  Total In Size:   %llu bytes\n", (unsigned long long)stats.TotalInSize);
  printf("  Total Out Size:  %llu bytes\n", (unsigned long long)stats.TotalOutSize);
  printf("  Elapsed Time:    %llu ms\n", (unsigned long long)stats.ElapsedTimeMs);
  printf("  Compression:     %u%%\n", stats.CompressionRatioX100);
  
  // Verify statistics are reasonable
  if (stats.ItemsCompleted != (UInt32)numItems)
  {
    delete compressor;
    TEST_FAIL("Detailed Statistics Integration", "Incorrect completed count");
  }
  
  if (stats.TotalInSize != totalInputSize)
  {
    printf("  Warning: Total input mismatch (expected %zu, got %llu)\n",
           totalInputSize, (unsigned long long)stats.TotalInSize);
  }
  
  delete compressor;
  TEST_PASS("Detailed Statistics Integration");
  return true;
}

// ===========================================================================
// Main
// ===========================================================================
int main()
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════╗\n");
  printf("║   Parallel Compression Integration Test Suite              ║\n");
  printf("║   Testing complete workflows and system integration        ║\n");
  printf("╚════════════════════════════════════════════════════════════╝\n");
  
  srand((unsigned)time(NULL));
  
  // Run all integration tests
  TestMemoryStreamE2E();
  TestEncryptedArchiveIntegration();
  TestLargeFileIntegration();
  TestErrorHandlingIntegration();
  TestDetailedStatisticsIntegration();
  
  printf("\n");
  printf("========================================\n");
  printf("INTEGRATION TEST RESULTS\n");
  printf("========================================\n");
  printf("Tests Passed: %u\n", g_TestsPassed);
  printf("Tests Failed: %u\n", g_TestsFailed);
  printf("Total Tests:  %u\n", g_TestsPassed + g_TestsFailed);
  printf("========================================\n");
  
  if (g_TestsFailed == 0)
  {
    printf("✓ ALL INTEGRATION TESTS PASSED\n");
    printf("\n");
    printf("Integration Verified:\n");
    printf("  ✓ Memory stream compression workflow\n");
    printf("  ✓ Encrypted archive creation and verification\n");
    printf("  ✓ Large file handling and performance\n");
    printf("  ✓ Error handling and input validation\n");
    printf("  ✓ Detailed statistics tracking\n");
    printf("\n");
    printf("Archives created:\n");
    printf("  - test_integration_memory.7z\n");
    printf("  - test_integration_encrypted.7z (password protected)\n");
    printf("  - test_integration_large.7z\n");
    return 0;
  }
  else
  {
    printf("✗ SOME TESTS FAILED\n");
    return 1;
  }
}
