// ParallelCompressExample.cpp - Example usage of parallel compression API

#include "StdAfx.h"

#include "ParallelCompressAPI.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example callback for progress
void OnProgress(UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void *userData)
{
  printf("Item %u: Compressed %llu -> %llu bytes (%.2f%% ratio)\n",
         itemIndex,
         (unsigned long long)inSize,
         (unsigned long long)outSize,
         outSize > 0 ? (100.0 * outSize / inSize) : 0.0);
}

// Example callback for errors
void OnError(UInt32 itemIndex, HRESULT errorCode, const wchar_t *message, void *userData)
{
  wprintf(L"Error on item %u (0x%08X): %s\n", itemIndex, errorCode, message);
}

// Example 1: Compress multiple memory buffers
int Example_CompressMultipleBuffers()
{
  printf("\n=== Example 1: Compress Multiple Memory Buffers ===\n");
  
  // Create some sample data
  const int numItems = 5;
  const size_t dataSize = 1024 * 1024; // 1 MB each
  
  ParallelInputItemC items[numItems];
  char **buffers = (char**)malloc(numItems * sizeof(char*));
  
  for (int i = 0; i < numItems; i++)
  {
    buffers[i] = (char*)malloc(dataSize);
    
    // Fill with sample data
    for (size_t j = 0; j < dataSize; j++)
    {
      buffers[i][j] = (char)(i * 100 + j % 256);
    }
    
    items[i].Data = buffers[i];
    items[i].DataSize = dataSize;
    items[i].FilePath = NULL;
    
    wchar_t name[256];
    swprintf(name, 256, L"Buffer_%d", i);
    items[i].Name = _wcsdup(name);
    items[i].Size = dataSize;
    items[i].UserData = NULL;
  }
  
  // Create compressor
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  
  // Configure
  ParallelCompressor_SetNumThreads(compressor, 4);
  ParallelCompressor_SetCompressionLevel(compressor, 5);
  ParallelCompressor_SetCallbacks(compressor, OnProgress, OnError, NULL, NULL);
  
  // Compress to file
  HRESULT hr = ParallelCompressor_CompressMultiple(
      compressor,
      items,
      numItems,
      L"output_buffers.7z");
  
  if (SUCCEEDED(hr))
  {
    printf("Successfully compressed %d buffers!\n", numItems);
  }
  else
  {
    printf("Compression failed with error 0x%08X\n", hr);
  }
  
  // Cleanup
  for (int i = 0; i < numItems; i++)
  {
    free(buffers[i]);
    free((void*)items[i].Name);
  }
  free(buffers);
  
  ParallelCompressor_Destroy(compressor);
  
  return SUCCEEDED(hr) ? 0 : 1;
}

// Example 2: Compress multiple files
int Example_CompressMultipleFiles()
{
  printf("\n=== Example 2: Compress Multiple Files ===\n");
  
  // List of files to compress
  const wchar_t *files[] = {
    L"file1.txt",
    L"file2.txt",
    L"file3.txt"
  };
  const int numFiles = 3;
  
  ParallelInputItemC items[numFiles];
  
  for (int i = 0; i < numFiles; i++)
  {
    items[i].Data = NULL;
    items[i].DataSize = 0;
    items[i].FilePath = files[i];
    items[i].Name = files[i];
    items[i].Size = 0; // Will be determined automatically
    items[i].UserData = NULL;
  }
  
  // Create compressor
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  
  // Configure with encryption
  Byte key[32] = {0}; // Example key (all zeros - don't use in production!)
  Byte iv[16] = {0};  // Example IV
  
  ParallelCompressor_SetNumThreads(compressor, 4);
  ParallelCompressor_SetCompressionLevel(compressor, 7);
  ParallelCompressor_SetEncryption(compressor, key, 32, iv, 16);
  ParallelCompressor_SetCallbacks(compressor, OnProgress, OnError, NULL, NULL);
  
  // Compress to file
  HRESULT hr = ParallelCompressor_CompressMultiple(
      compressor,
      items,
      numFiles,
      L"output_files.7z");
  
  if (SUCCEEDED(hr))
  {
    printf("Successfully compressed %d files!\n", numFiles);
  }
  else
  {
    printf("Compression failed with error 0x%08X\n", hr);
  }
  
  ParallelCompressor_Destroy(compressor);
  
  return SUCCEEDED(hr) ? 0 : 1;
}

// Example 3: Using stream queue for dynamic batching
int Example_StreamQueue()
{
  printf("\n=== Example 3: Stream Queue for Dynamic Batching ===\n");
  
  // Create stream queue
  ParallelStreamQueueHandle queue = ParallelStreamQueue_Create();
  
  ParallelStreamQueue_SetMaxQueueSize(queue, 100);
  
  // Simulate streaming data from network or memory
  for (int i = 0; i < 10; i++)
  {
    // Create some data
    size_t dataSize = 1024 * 100; // 100 KB
    char *data = (char*)malloc(dataSize);
    
    for (size_t j = 0; j < dataSize; j++)
    {
      data[j] = (char)(i * 10 + j % 256);
    }
    
    wchar_t name[256];
    swprintf(name, 256, L"Stream_%d", i);
    
    // Add to queue
    HRESULT hr = ParallelStreamQueue_AddStream(queue, data, dataSize, name);
    
    if (FAILED(hr))
    {
      printf("Failed to add stream %d\n", i);
      free(data);
    }
    else
    {
      printf("Added stream %d to queue\n", i);
      free(data);
    }
  }
  
  // Start processing
  printf("Starting processing...\n");
  HRESULT hr = ParallelStreamQueue_StartProcessing(queue, L"output_queue.7z");
  
  if (SUCCEEDED(hr))
  {
    // Wait for completion
    ParallelStreamQueue_WaitForCompletion(queue);
    
    // Get status
    UInt32 processed, failed, pending;
    ParallelStreamQueue_GetStatus(queue, &processed, &failed, &pending);
    
    printf("Processed: %u, Failed: %u, Pending: %u\n", processed, failed, pending);
  }
  else
  {
    printf("Processing failed with error 0x%08X\n", hr);
  }
  
  ParallelStreamQueue_Destroy(queue);
  
  return SUCCEEDED(hr) ? 0 : 1;
}

// Example 4: Compress to memory buffer
int Example_CompressToMemory()
{
  printf("\n=== Example 4: Compress to Memory Buffer ===\n");
  
  const int numItems = 3;
  const size_t dataSize = 512 * 1024; // 512 KB each
  
  ParallelInputItemC items[numItems];
  char **buffers = (char**)malloc(numItems * sizeof(char*));
  
  for (int i = 0; i < numItems; i++)
  {
    buffers[i] = (char*)malloc(dataSize);
    memset(buffers[i], i + 65, dataSize); // Fill with 'A', 'B', 'C'
    
    items[i].Data = buffers[i];
    items[i].DataSize = dataSize;
    items[i].FilePath = NULL;
    items[i].Name = NULL;
    items[i].Size = dataSize;
    items[i].UserData = NULL;
  }
  
  // Create compressor
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  ParallelCompressor_SetNumThreads(compressor, 2);
  ParallelCompressor_SetCompressionLevel(compressor, 5);
  
  // Compress to memory
  void *outputBuffer = NULL;
  size_t outputSize = 0;
  
  HRESULT hr = ParallelCompressor_CompressMultipleToMemory(
      compressor,
      items,
      numItems,
      &outputBuffer,
      &outputSize);
  
  if (SUCCEEDED(hr))
  {
    printf("Successfully compressed to memory: %zu bytes\n", outputSize);
    
    // You can now use outputBuffer (e.g., send over network)
    // Remember to free it when done
    free(outputBuffer);
  }
  else
  {
    printf("Compression to memory failed with error 0x%08X\n", hr);
  }
  
  // Cleanup
  for (int i = 0; i < numItems; i++)
  {
    free(buffers[i]);
  }
  free(buffers);
  
  ParallelCompressor_Destroy(compressor);
  
  return SUCCEEDED(hr) ? 0 : 1;
}

// Main example runner
int main(int argc, char* argv[])
{
  printf("7-Zip Parallel Compression API Examples\n");
  printf("========================================\n");
  
  int result = 0;
  
  // Run examples
  result |= Example_CompressMultipleBuffers();
  result |= Example_CompressMultipleFiles();
  result |= Example_StreamQueue();
  result |= Example_CompressToMemory();
  
  printf("\n========================================\n");
  if (result == 0)
  {
    printf("All examples completed successfully!\n");
  }
  else
  {
    printf("Some examples failed. Check output above.\n");
  }
  
  return result;
}
