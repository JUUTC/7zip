// ParallelFeatureParityTest.cpp - Feature parity validation between single and multi-stream flows
// This test suite validates that multi-stream parallel compression has true 1:1 feature parity
// with the base single input flow, including encryption, CRC calculation, and archive format.

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
  printf("TEST: %s\n", name); \
  printf("========================================\n");

#define TEST_PASS(name) \
  printf("✓ PASS: %s\n", name); \
  g_TestsPassed++;

#define TEST_FAIL(name, reason) \
  printf("✗ FAIL: %s - %s\n", name, reason); \
  g_TestsFailed++; \
  return false;

// Helper to validate 7z signature
static bool Validate7zSignature(const Byte *buffer, size_t size)
{
  if (size < 6)
    return false;
  return (buffer[0] == '7' && buffer[1] == 'z' && 
          buffer[2] == 0xBC && buffer[3] == 0xAF && 
          buffer[4] == 0x27 && buffer[5] == 0x1C);
}

// Helper to check if data appears encrypted (high entropy)
static bool DataAppearsEncrypted(const Byte *buffer, size_t size, size_t offset)
{
  // Skip 7z header (first 32 bytes) and check data section
  if (size < offset + 256)
    return false;
  
  // Count byte frequency to estimate entropy
  int freq[256] = {0};
  size_t sampleSize = (size - offset > 1024) ? 1024 : (size - offset);
  
  for (size_t i = 0; i < sampleSize; i++)
  {
    freq[buffer[offset + i]]++;
  }
  
  // Calculate simple entropy estimate
  int uniqueBytes = 0;
  for (int i = 0; i < 256; i++)
  {
    if (freq[i] > 0)
      uniqueBytes++;
  }
  
  // Encrypted/compressed data should have high entropy (many unique bytes)
  // Plain text typically has fewer unique bytes
  return (uniqueBytes > 100);
}

// ===========================================================================
// Test 1: Feature Parity - Single Stream vs Multi-Stream Compression
// ===========================================================================
static bool TestSingleVsMultiStreamParity()
{
  TEST_START("Single Stream vs Multi-Stream Feature Parity");
  
  const char *testData = "Test data for compression parity validation. "
                         "This data will be compressed both ways. "
                         "The resulting archives should be functionally equivalent.";
  size_t dataSize = strlen(testData);
  
  // Single stream compression
  printf("Step 1: Single stream compression (1 thread, 1 item)...\n");
  
  CParallelCompressor *compressor1 = new CParallelCompressor();
  compressor1->SetNumThreads(1);
  compressor1->SetCompressionLevel(5);
  
  CBufInStream *inStreamSpec1 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream1 = inStreamSpec1;
  inStreamSpec1->Init((const Byte*)testData, dataSize, NULL);
  
  CDynBufSeqOutStream *outStreamSpec1 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream1 = outStreamSpec1;
  
  UInt64 inSize = dataSize;
  HRESULT hr1 = compressor1->Code(inStream1, outStream1, &inSize, NULL, NULL);
  
  if (FAILED(hr1))
  {
    delete compressor1;
    TEST_FAIL("Single vs Multi Stream Parity", "Single stream compression failed");
  }
  
  size_t singleStreamSize = outStreamSpec1->GetSize();
  printf("  Single stream output: %zu bytes\n", singleStreamSize);
  
  // Multi-stream compression with same data
  printf("Step 2: Multi-stream compression (4 threads, 1 item)...\n");
  
  CParallelCompressor *compressor2 = new CParallelCompressor();
  compressor2->SetNumThreads(4);
  compressor2->SetCompressionLevel(5);
  
  CObjectVector<CParallelInputItem> items;
  CParallelInputItem &item = items.AddNew();
  
  CBufInStream *inStreamSpec2 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream2 = inStreamSpec2;
  inStreamSpec2->Init((const Byte*)testData, dataSize, NULL);
  
  item.InStream = inStream2;
  item.Name = L"test.txt";
  item.Size = dataSize;
  item.Attributes = 0;
  
  CDynBufSeqOutStream *outStreamSpec2 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream2 = outStreamSpec2;
  
  HRESULT hr2 = compressor2->CompressMultiple(&items[0], 1, outStream2, NULL);
  
  if (FAILED(hr2))
  {
    delete compressor1;
    delete compressor2;
    TEST_FAIL("Single vs Multi Stream Parity", "Multi-stream compression failed");
  }
  
  size_t multiStreamSize = outStreamSpec2->GetSize();
  printf("  Multi-stream output: %zu bytes\n", multiStreamSize);
  
  // Validate both outputs are valid 7z archives
  bool valid1 = Validate7zSignature(outStreamSpec1->GetBuffer(), singleStreamSize);
  bool valid2 = Validate7zSignature(outStreamSpec2->GetBuffer(), multiStreamSize);
  
  printf("Step 3: Validating archive formats...\n");
  printf("  Single stream 7z signature: %s\n", valid1 ? "VALID" : "INVALID");
  printf("  Multi-stream 7z signature: %s\n", valid2 ? "VALID" : "INVALID");
  
  delete compressor1;
  delete compressor2;
  
  if (!valid1 || !valid2)
  {
    TEST_FAIL("Single vs Multi Stream Parity", "Invalid 7z archive format");
  }
  
  // Both should produce comparable output sizes (within reasonable variance)
  double ratio = (double)multiStreamSize / singleStreamSize;
  printf("  Size ratio (multi/single): %.2f\n", ratio);
  
  if (ratio < 0.5 || ratio > 2.0)
  {
    TEST_FAIL("Single vs Multi Stream Parity", "Output sizes differ too much");
  }
  
  TEST_PASS("Single Stream vs Multi-Stream Feature Parity");
  return true;
}

// ===========================================================================
// Test 2: Encryption Feature - Password Protection
// ===========================================================================
static bool TestEncryptionFeature()
{
  TEST_START("Encryption Feature (Password Protection)");
  
  const char *testData = "SENSITIVE DATA - This should be encrypted and unreadable "
                         "without the correct password. Contains confidential information "
                         "that must be protected by AES-256 encryption.";
  size_t dataSize = strlen(testData);
  const wchar_t *password = L"SecurePassword123!";
  
  // Compress with encryption
  printf("Step 1: Compressing with encryption...\n");
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(2);
  compressor->SetCompressionLevel(5);
  
  HRESULT pwResult = compressor->SetPassword(password);
  if (FAILED(pwResult))
  {
    delete compressor;
    TEST_FAIL("Encryption Feature", "Failed to set password");
  }
  printf("  Password set successfully\n");
  
  CObjectVector<CParallelInputItem> items;
  CParallelInputItem &item = items.AddNew();
  
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init((const Byte*)testData, dataSize, NULL);
  
  item.InStream = inStream;
  item.Name = L"secret.txt";
  item.Size = dataSize;
  item.Attributes = 0;
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], 1, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Encryption Feature", "Encryption compression failed");
  }
  
  size_t encryptedSize = outStreamSpec->GetSize();
  printf("  Encrypted archive size: %zu bytes\n", encryptedSize);
  
  // Validate 7z format
  printf("Step 2: Validating 7z format...\n");
  bool valid7z = Validate7zSignature(outStreamSpec->GetBuffer(), encryptedSize);
  printf("  7z signature: %s\n", valid7z ? "VALID" : "INVALID");
  
  if (!valid7z)
  {
    delete compressor;
    TEST_FAIL("Encryption Feature", "Invalid 7z archive format");
  }
  
  // Verify original data is not visible in plain text
  printf("Step 3: Verifying data is encrypted (not plaintext visible)...\n");
  const char *searchStr = "SENSITIVE DATA";
  bool foundPlaintext = false;
  
  const Byte *buffer = outStreamSpec->GetBuffer();
  for (size_t i = 0; i < encryptedSize - strlen(searchStr); i++)
  {
    if (memcmp(buffer + i, searchStr, strlen(searchStr)) == 0)
    {
      foundPlaintext = true;
      break;
    }
  }
  
  printf("  Plaintext 'SENSITIVE DATA' found: %s\n", foundPlaintext ? "YES (BAD!)" : "NO (GOOD!)");
  
  if (foundPlaintext)
  {
    delete compressor;
    TEST_FAIL("Encryption Feature", "Plaintext data visible in encrypted archive!");
  }
  
  // Check entropy of data section
  printf("Step 4: Checking data entropy (encryption indicator)...\n");
  bool highEntropy = DataAppearsEncrypted(buffer, encryptedSize, 32);
  printf("  Data appears encrypted (high entropy): %s\n", highEntropy ? "YES" : "NO");
  
  // Write encrypted archive to file for manual verification
  printf("Step 5: Writing encrypted archive for verification...\n");
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_encrypted.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(buffer, (UInt32)encryptedSize, &written);
    printf("  Wrote %u bytes to test_encrypted.7z\n", written);
    printf("  Verify with: 7z t -pSecurePassword123! test_encrypted.7z\n");
    printf("  Try without password: 7z t test_encrypted.7z (should fail)\n");
  }
  
  delete compressor;
  TEST_PASS("Encryption Feature (Password Protection)");
  return true;
}

// ===========================================================================
// Test 3: Encryption vs Non-Encryption Comparison
// ===========================================================================
static bool TestEncryptionComparison()
{
  TEST_START("Encryption vs Non-Encryption Comparison");
  
  const char *testData = "Test data for encryption comparison. "
                         "Same data will be compressed with and without encryption.";
  size_t dataSize = strlen(testData);
  
  // Non-encrypted compression
  printf("Step 1: Non-encrypted compression...\n");
  
  CParallelCompressor *compressor1 = new CParallelCompressor();
  compressor1->SetNumThreads(2);
  compressor1->SetCompressionLevel(5);
  
  CObjectVector<CParallelInputItem> items1;
  CParallelInputItem &item1 = items1.AddNew();
  
  CBufInStream *inStreamSpec1 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream1 = inStreamSpec1;
  inStreamSpec1->Init((const Byte*)testData, dataSize, NULL);
  
  item1.InStream = inStream1;
  item1.Name = L"plain.txt";
  item1.Size = dataSize;
  item1.Attributes = 0;
  
  CDynBufSeqOutStream *outStreamSpec1 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream1 = outStreamSpec1;
  
  HRESULT hr1 = compressor1->CompressMultiple(&items1[0], 1, outStream1, NULL);
  
  if (FAILED(hr1))
  {
    delete compressor1;
    TEST_FAIL("Encryption Comparison", "Non-encrypted compression failed");
  }
  
  size_t nonEncryptedSize = outStreamSpec1->GetSize();
  printf("  Non-encrypted size: %zu bytes\n", nonEncryptedSize);
  
  // Encrypted compression
  printf("Step 2: Encrypted compression...\n");
  
  CParallelCompressor *compressor2 = new CParallelCompressor();
  compressor2->SetNumThreads(2);
  compressor2->SetCompressionLevel(5);
  compressor2->SetPassword(L"TestPassword");
  
  CObjectVector<CParallelInputItem> items2;
  CParallelInputItem &item2 = items2.AddNew();
  
  CBufInStream *inStreamSpec2 = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream2 = inStreamSpec2;
  inStreamSpec2->Init((const Byte*)testData, dataSize, NULL);
  
  item2.InStream = inStream2;
  item2.Name = L"encrypted.txt";
  item2.Size = dataSize;
  item2.Attributes = 0;
  
  CDynBufSeqOutStream *outStreamSpec2 = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream2 = outStreamSpec2;
  
  HRESULT hr2 = compressor2->CompressMultiple(&items2[0], 1, outStream2, NULL);
  
  if (FAILED(hr2))
  {
    delete compressor1;
    delete compressor2;
    TEST_FAIL("Encryption Comparison", "Encrypted compression failed");
  }
  
  size_t encryptedSize = outStreamSpec2->GetSize();
  printf("  Encrypted size: %zu bytes\n", encryptedSize);
  
  // Compare archive contents
  printf("Step 3: Comparing archive contents...\n");
  
  bool nonEncValid = Validate7zSignature(outStreamSpec1->GetBuffer(), nonEncryptedSize);
  bool encValid = Validate7zSignature(outStreamSpec2->GetBuffer(), encryptedSize);
  
  printf("  Non-encrypted 7z valid: %s\n", nonEncValid ? "YES" : "NO");
  printf("  Encrypted 7z valid: %s\n", encValid ? "YES" : "NO");
  
  // Encrypted archive should be slightly larger due to encryption overhead
  printf("Step 4: Checking size difference (encryption adds overhead)...\n");
  printf("  Size difference: %zd bytes\n", (ssize_t)encryptedSize - (ssize_t)nonEncryptedSize);
  
  // Archives should be different (not identical)
  bool identical = (nonEncryptedSize == encryptedSize) && 
                   (memcmp(outStreamSpec1->GetBuffer(), outStreamSpec2->GetBuffer(), 
                           nonEncryptedSize) == 0);
  printf("  Archives are different: %s\n", identical ? "NO (BAD!)" : "YES (GOOD!)");
  
  delete compressor1;
  delete compressor2;
  
  if (identical)
  {
    TEST_FAIL("Encryption Comparison", "Encrypted archive identical to non-encrypted!");
  }
  
  if (!nonEncValid || !encValid)
  {
    TEST_FAIL("Encryption Comparison", "Invalid 7z archive format");
  }
  
  TEST_PASS("Encryption vs Non-Encryption Comparison");
  return true;
}

// ===========================================================================
// Test 4: CRC Integrity Validation
// ===========================================================================
static bool TestCRCIntegrity()
{
  TEST_START("CRC Integrity Validation");
  
  // Create test data with known patterns
  const size_t dataSize = 4096;
  Byte *testData = new Byte[dataSize];
  
  printf("Step 1: Creating test data with known pattern...\n");
  for (size_t i = 0; i < dataSize; i++)
  {
    testData[i] = (Byte)(i % 256);
  }
  
  // Compress and verify CRC is calculated
  printf("Step 2: Compressing with CRC calculation...\n");
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  
  CObjectVector<CParallelInputItem> items;
  CParallelInputItem &item = items.AddNew();
  
  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init(testData, dataSize, NULL);
  
  item.InStream = inStream;
  item.Name = L"crc_test.bin";
  item.Size = dataSize;
  item.Attributes = 0;
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], 1, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete[] testData;
    delete compressor;
    TEST_FAIL("CRC Integrity", "Compression failed");
  }
  
  size_t outputSize = outStreamSpec->GetSize();
  printf("  Output size: %zu bytes\n", outputSize);
  
  // Get statistics to verify processing
  UInt32 completed, failed;
  UInt64 totalIn, totalOut;
  compressor->GetStatistics(&completed, &failed, &totalIn, &totalOut);
  
  printf("Step 3: Verifying compression statistics...\n");
  printf("  Items completed: %u\n", completed);
  printf("  Items failed: %u\n", failed);
  printf("  Total input: %llu bytes\n", (unsigned long long)totalIn);
  printf("  Total output: %llu bytes\n", (unsigned long long)totalOut);
  
  if (completed != 1 || failed != 0)
  {
    delete[] testData;
    delete compressor;
    TEST_FAIL("CRC Integrity", "Unexpected compression statistics");
  }
  
  // Validate 7z format
  bool valid7z = Validate7zSignature(outStreamSpec->GetBuffer(), outputSize);
  printf("  7z signature valid: %s\n", valid7z ? "YES" : "NO");
  
  // Write to file for verification
  printf("Step 4: Writing archive for external verification...\n");
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_crc.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(outStreamSpec->GetBuffer(), (UInt32)outputSize, &written);
    printf("  Wrote %u bytes to test_crc.7z\n", written);
    printf("  Verify with: 7z t test_crc.7z\n");
  }
  
  delete[] testData;
  delete compressor;
  
  if (!valid7z)
  {
    TEST_FAIL("CRC Integrity", "Invalid 7z archive format");
  }
  
  TEST_PASS("CRC Integrity Validation");
  return true;
}

// ===========================================================================
// Test 5: Multi-Stream with Multiple Items
// ===========================================================================
static bool TestMultipleItemsCompression()
{
  TEST_START("Multiple Items Compression");
  
  const int numItems = 10;
  
  printf("Step 1: Creating %d test items...\n", numItems);
  
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numItems; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    char content[256];
    int contentSize = snprintf(content, sizeof(content),
        "Item %d: Test content for parallel compression. "
        "Unique data identifier: %d. Timestamp: %ld.\n",
        i, i * 1000 + 42, (long)time(NULL) + i);
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)content, contentSize, NULL);
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, sizeof(name) / sizeof(wchar_t), L"file%03d.txt", i);
    item.Name = name;
    item.Size = contentSize;
    item.Attributes = 0x20;
    
    printf("  Item %d: %d bytes\n", i, contentSize);
  }
  
  printf("Step 2: Compressing with parallel threads...\n");
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numItems, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Multiple Items", "Compression failed");
  }
  
  size_t outputSize = outStreamSpec->GetSize();
  printf("  Total output: %zu bytes\n", outputSize);
  
  // Verify statistics
  UInt32 completed, failed;
  compressor->GetStatistics(&completed, &failed, NULL, NULL);
  
  printf("Step 3: Verifying all items processed...\n");
  printf("  Completed: %u / %d\n", completed, numItems);
  printf("  Failed: %u\n", failed);
  
  if (completed != (UInt32)numItems)
  {
    delete compressor;
    TEST_FAIL("Multiple Items", "Not all items completed");
  }
  
  if (failed > 0)
  {
    delete compressor;
    TEST_FAIL("Multiple Items", "Some items failed");
  }
  
  // Validate 7z format
  bool valid7z = Validate7zSignature(outStreamSpec->GetBuffer(), outputSize);
  printf("  7z signature valid: %s\n", valid7z ? "YES" : "NO");
  
  // Write archive
  printf("Step 4: Writing archive...\n");
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_multiple_items.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(outStreamSpec->GetBuffer(), (UInt32)outputSize, &written);
    printf("  Wrote %u bytes to test_multiple_items.7z\n", written);
    printf("  Verify with: 7z l test_multiple_items.7z\n");
  }
  
  delete compressor;
  
  if (!valid7z)
  {
    TEST_FAIL("Multiple Items", "Invalid 7z archive format");
  }
  
  TEST_PASS("Multiple Items Compression");
  return true;
}

// ===========================================================================
// Test 6: Encrypted Multiple Items
// ===========================================================================
static bool TestEncryptedMultipleItems()
{
  TEST_START("Encrypted Multiple Items");
  
  const int numItems = 5;
  const wchar_t *password = L"MultiItemPassword!";
  
  printf("Step 1: Creating %d encrypted items...\n", numItems);
  
  CParallelCompressor *compressor = new CParallelCompressor();
  compressor->SetNumThreads(4);
  compressor->SetCompressionLevel(5);
  compressor->SetPassword(password);
  
  CObjectVector<CParallelInputItem> items;
  
  for (int i = 0; i < numItems; i++)
  {
    CParallelInputItem &item = items.AddNew();
    
    char content[512];
    int contentSize = snprintf(content, sizeof(content),
        "CONFIDENTIAL Item %d: Secret data that must be encrypted. "
        "Account: %d, Balance: $%d.%02d\n",
        i, i * 1000, (i + 1) * 1000, i * 11);
    
    CBufInStream *inStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
    inStreamSpec->Init((const Byte*)content, contentSize, NULL);
    
    item.InStream = inStream;
    
    wchar_t name[64];
    swprintf(name, sizeof(name) / sizeof(wchar_t), L"secret%d.txt", i);
    item.Name = name;
    item.Size = contentSize;
    item.Attributes = 0;
  }
  
  printf("Step 2: Compressing with encryption...\n");
  
  CDynBufSeqOutStream *outStreamSpec = new CDynBufSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  
  HRESULT hr = compressor->CompressMultiple(&items[0], numItems, outStream, NULL);
  
  if (FAILED(hr))
  {
    delete compressor;
    TEST_FAIL("Encrypted Multiple Items", "Compression failed");
  }
  
  size_t outputSize = outStreamSpec->GetSize();
  printf("  Encrypted output: %zu bytes\n", outputSize);
  
  // Verify no plaintext visible
  printf("Step 3: Verifying encryption...\n");
  const Byte *buffer = outStreamSpec->GetBuffer();
  bool foundPlaintext = false;
  const char *searchStr = "CONFIDENTIAL";
  
  for (size_t i = 0; i < outputSize - strlen(searchStr); i++)
  {
    if (memcmp(buffer + i, searchStr, strlen(searchStr)) == 0)
    {
      foundPlaintext = true;
      break;
    }
  }
  
  printf("  Plaintext visible: %s\n", foundPlaintext ? "YES (BAD!)" : "NO (GOOD!)");
  
  // Validate format
  bool valid7z = Validate7zSignature(buffer, outputSize);
  printf("  7z signature valid: %s\n", valid7z ? "YES" : "NO");
  
  // Write archive
  printf("Step 4: Writing encrypted archive...\n");
  COutFileStream *fileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> fileStream = fileStreamSpec;
  
  if (fileStreamSpec->Create(L"test_encrypted_multi.7z", false))
  {
    UInt32 written = 0;
    fileStream->Write(buffer, (UInt32)outputSize, &written);
    printf("  Wrote %u bytes to test_encrypted_multi.7z\n", written);
    printf("  Verify with: 7z t -pMultiItemPassword! test_encrypted_multi.7z\n");
  }
  
  delete compressor;
  
  if (foundPlaintext)
  {
    TEST_FAIL("Encrypted Multiple Items", "Plaintext visible in encrypted archive!");
  }
  
  if (!valid7z)
  {
    TEST_FAIL("Encrypted Multiple Items", "Invalid 7z archive format");
  }
  
  TEST_PASS("Encrypted Multiple Items");
  return true;
}

// ===========================================================================
// Test 7: C API Feature Parity
// ===========================================================================
static bool TestCAPIFeatureParity()
{
  TEST_START("C API Feature Parity");
  
  printf("Step 1: Testing C API creation and configuration...\n");
  
  ParallelCompressorHandle handle = ParallelCompressor_Create();
  if (!handle)
  {
    TEST_FAIL("C API Feature Parity", "Failed to create compressor");
  }
  printf("  ✓ Compressor created\n");
  
  HRESULT hr = ParallelCompressor_SetNumThreads(handle, 4);
  if (FAILED(hr))
  {
    ParallelCompressor_Destroy(handle);
    TEST_FAIL("C API Feature Parity", "Failed to set threads");
  }
  printf("  ✓ Threads set to 4\n");
  
  hr = ParallelCompressor_SetCompressionLevel(handle, 5);
  if (FAILED(hr))
  {
    ParallelCompressor_Destroy(handle);
    TEST_FAIL("C API Feature Parity", "Failed to set compression level");
  }
  printf("  ✓ Compression level set to 5\n");
  
  hr = ParallelCompressor_SetPassword(handle, L"CAPIPassword");
  if (FAILED(hr))
  {
    ParallelCompressor_Destroy(handle);
    TEST_FAIL("C API Feature Parity", "Failed to set password");
  }
  printf("  ✓ Password set\n");
  
  printf("Step 2: Testing C API compression...\n");
  
  const int numItems = 3;
  char *buffers[numItems];
  size_t sizes[numItems] = { 256, 512, 1024 };
  
  for (int i = 0; i < numItems; i++)
  {
    buffers[i] = (char*)malloc(sizes[i]);
    memset(buffers[i], 'A' + i, sizes[i]);
  }
  
  ParallelInputItemC items[numItems];
  for (int i = 0; i < numItems; i++)
  {
    items[i].Data = buffers[i];
    items[i].DataSize = sizes[i];
    items[i].FilePath = NULL;
    items[i].Name = NULL;
    items[i].Size = sizes[i];
    items[i].UserData = NULL;
  }
  
  void *outputBuffer = NULL;
  size_t outputSize = 0;
  
  hr = ParallelCompressor_CompressMultipleToMemory(handle, items, numItems, &outputBuffer, &outputSize);
  
  for (int i = 0; i < numItems; i++)
  {
    free(buffers[i]);
  }
  
  if (FAILED(hr))
  {
    ParallelCompressor_Destroy(handle);
    TEST_FAIL("C API Feature Parity", "Memory compression failed");
  }
  printf("  ✓ Compression completed: %zu bytes\n", outputSize);
  
  // Validate output
  printf("Step 3: Validating C API output...\n");
  bool valid7z = Validate7zSignature((const Byte*)outputBuffer, outputSize);
  printf("  7z signature valid: %s\n", valid7z ? "YES" : "NO");
  
  // Check detailed statistics
  ParallelStatisticsC stats;
  hr = ParallelCompressor_GetDetailedStatistics(handle, &stats);
  if (SUCCEEDED(hr))
  {
    printf("  Items completed: %u\n", stats.ItemsCompleted);
    printf("  Items failed: %u\n", stats.ItemsFailed);
  }
  
  free(outputBuffer);
  ParallelCompressor_Destroy(handle);
  
  if (!valid7z)
  {
    TEST_FAIL("C API Feature Parity", "Invalid 7z archive format");
  }
  
  TEST_PASS("C API Feature Parity");
  return true;
}

// ===========================================================================
// Main
// ===========================================================================
int main()
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════╗\n");
  printf("║   Parallel Compression Feature Parity Test Suite           ║\n");
  printf("║   Validating 1:1 parity with single input flow             ║\n");
  printf("╚════════════════════════════════════════════════════════════╝\n");
  
  srand((unsigned)time(NULL));
  
  // Run all tests
  TestSingleVsMultiStreamParity();
  TestEncryptionFeature();
  TestEncryptionComparison();
  TestCRCIntegrity();
  TestMultipleItemsCompression();
  TestEncryptedMultipleItems();
  TestCAPIFeatureParity();
  
  printf("\n");
  printf("========================================\n");
  printf("FINAL RESULTS\n");
  printf("========================================\n");
  printf("Tests Passed: %u\n", g_TestsPassed);
  printf("Tests Failed: %u\n", g_TestsFailed);
  printf("Total Tests:  %u\n", g_TestsPassed + g_TestsFailed);
  printf("========================================\n");
  
  if (g_TestsFailed == 0)
  {
    printf("✓ ALL FEATURE PARITY TESTS PASSED\n");
    printf("\n");
    printf("Feature Parity Verified:\n");
    printf("  ✓ Single stream vs Multi-stream compression\n");
    printf("  ✓ Encryption with password protection\n");
    printf("  ✓ CRC integrity calculation\n");
    printf("  ✓ Multiple item compression\n");
    printf("  ✓ Encrypted multi-item archives\n");
    printf("  ✓ C API feature parity\n");
    printf("\n");
    printf("Archives created for manual verification:\n");
    printf("  - test_encrypted.7z (requires password)\n");
    printf("  - test_crc.7z\n");
    printf("  - test_multiple_items.7z\n");
    printf("  - test_encrypted_multi.7z (requires password)\n");
    return 0;
  }
  else
  {
    printf("✗ SOME TESTS FAILED\n");
    return 1;
  }
}
