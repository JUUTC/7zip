# Azure Storage Blob Integration Guide

This guide shows how to integrate the parallel compression API with Azure Storage Blobs.

## Overview

The parallel compression API is designed to work seamlessly with Azure Storage Blobs by accepting generic `ISequentialInStream` interfaces. This allows you to:

1. Stream blobs directly from Azure Storage
2. Compress multiple blobs in parallel
3. Minimize memory usage by streaming
4. Upload compressed results back to Azure Storage

## Architecture

```
Azure Blob Storage
    ↓
Download Streams (parallel)
    ↓
ISequentialInStream Adapters
    ↓
Parallel Compressor (7-Zip DLL)
    ↓
Compressed Output Stream
    ↓
Azure Blob Storage (upload)
```

## Prerequisites

1. Azure Storage SDK for C++
2. 7-Zip Parallel Compression DLL
3. C++17 or later compiler

## Installation

```bash
# Install Azure Storage SDK
vcpkg install azure-storage-blobs-cpp

# Build 7-Zip with parallel compression support
cd CPP/7zip/Bundles/Format7z
nmake  # or make -f makefile.gcc on Linux
```

## Implementation

### Step 1: Create ISequentialInStream Adapter for Azure Blobs

```cpp
#include <azure/storage/blobs.hpp>
#include "IStream.h"

using namespace Azure::Storage::Blobs;

class CAzureBlobInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  std::unique_ptr<Azure::Core::IO::BodyStream> _blobStream;
  UInt64 _position;
  UInt64 _size;
  
public:
  Z7_IFACES_IMP_UNK_1(ISequentialInStream)
  
  CAzureBlobInStream(std::unique_ptr<Azure::Core::IO::BodyStream> stream, UInt64 size)
    : _blobStream(std::move(stream))
    , _position(0)
    , _size(size)
  {
  }
  
  Z7_COM7F_IMF(Read(void *data, UInt32 size, UInt32 *processedSize))
  {
    if (processedSize)
      *processedSize = 0;
      
    if (size == 0)
      return S_OK;
      
    if (_position >= _size)
      return S_OK;
      
    try
    {
      size_t bytesRead = _blobStream->Read((uint8_t*)data, size);
      
      if (processedSize)
        *processedSize = (UInt32)bytesRead;
        
      _position += bytesRead;
      return S_OK;
    }
    catch (const std::exception&)
    {
      return E_FAIL;
    }
  }
};
```

### Step 2: Compress Multiple Azure Blobs in Parallel

```cpp
#include "ParallelCompressAPI.h"
#include <azure/storage/blobs.hpp>

using namespace Azure::Storage::Blobs;

void CompressAzureBlobs(
    const std::string& connectionString,
    const std::string& containerName,
    const std::wstring& outputPath)
{
  // Connect to Azure Storage
  BlobServiceClient serviceClient = BlobServiceClient::CreateFromConnectionString(connectionString);
  BlobContainerClient containerClient = serviceClient.GetBlobContainerClient(containerName);
  
  // List blobs
  auto blobList = containerClient.ListBlobs();
  
  std::vector<ParallelInputItemC> items;
  std::vector<std::unique_ptr<CAzureBlobInStream>> streams;
  
  for (const auto& blob : blobList.Blobs)
  {
    // Get blob client
    BlobClient blobClient = containerClient.GetBlobClient(blob.Name);
    
    // Download blob stream
    auto response = blobClient.Download();
    UInt64 blobSize = blob.BlobSize;
    
    // Create stream adapter
    auto stream = std::make_unique<CAzureBlobInStream>(
        std::move(response.Value.BodyStream),
        blobSize);
    
    // Add to items
    ParallelInputItemC item;
    item.Data = nullptr;
    item.DataSize = 0;
    item.FilePath = nullptr;
    item.Name = _wcsdup(std::wstring(blob.Name.begin(), blob.Name.end()).c_str());
    item.Size = blobSize;
    item.UserData = stream.get();
    
    items.push_back(item);
    streams.push_back(std::move(stream));
  }
  
  // Create compressor
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  
  // Configure for Azure workload
  ParallelCompressor_SetNumThreads(compressor, 16);  // Process 16 blobs concurrently
  ParallelCompressor_SetCompressionLevel(compressor, 5);  // Balanced compression
  
  // Set callbacks
  auto progressCallback = [](UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void* userData)
  {
    printf("Blob %u: %llu -> %llu bytes (%.1f%% compression)\n",
           itemIndex, inSize, outSize, 100.0 * (1.0 - (double)outSize / inSize));
  };
  
  ParallelCompressor_SetCallbacks(compressor, progressCallback, nullptr, nullptr);
  
  // Compress all blobs
  HRESULT hr = ParallelCompressor_CompressMultiple(
      compressor,
      items.data(),
      items.size(),
      outputPath.c_str());
  
  if (SUCCEEDED(hr))
  {
    printf("Successfully compressed %zu blobs from Azure Storage!\n", items.size());
  }
  else
  {
    printf("Compression failed: 0x%08X\n", hr);
  }
  
  // Cleanup
  for (auto& item : items)
  {
    free((void*)item.Name);
  }
  
  ParallelCompressor_Destroy(compressor);
}
```

### Step 3: Upload Compressed Archive Back to Azure

```cpp
void CompressAndUploadToAzure(
    const std::string& connectionString,
    const std::string& sourceContainer,
    const std::string& destContainer,
    const std::string& archiveName)
{
  // Compress to memory
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  
  // ... setup items from Azure blobs as above ...
  
  void* outputBuffer = nullptr;
  size_t outputSize = 0;
  
  HRESULT hr = ParallelCompressor_CompressMultipleToMemory(
      compressor,
      items.data(),
      items.size(),
      &outputBuffer,
      &outputSize);
  
  if (SUCCEEDED(hr))
  {
    // Upload to Azure
    BlobServiceClient serviceClient = BlobServiceClient::CreateFromConnectionString(connectionString);
    BlobContainerClient containerClient = serviceClient.GetBlobContainerClient(destContainer);
    BlobClient blobClient = containerClient.GetBlobClient(archiveName);
    
    // Create upload stream
    Azure::Core::IO::MemoryBodyStream uploadStream(
        (const uint8_t*)outputBuffer,
        outputSize);
    
    // Upload
    blobClient.UploadFrom(&uploadStream, outputSize);
    
    printf("Uploaded %zu bytes to Azure Storage\n", outputSize);
    
    // Free memory
    free(outputBuffer);
  }
  
  ParallelCompressor_Destroy(compressor);
}
```

## Advanced Scenarios

### Streaming Compression with Batching

For very large numbers of blobs, use the queue API:

```cpp
void StreamCompressAzureBlobs(
    const std::string& connectionString,
    const std::string& containerName)
{
  // Create queue
  ParallelStreamQueueHandle queue = ParallelStreamQueue_Create();
  ParallelStreamQueue_SetMaxQueueSize(queue, 100);
  
  // Connect to Azure
  BlobServiceClient serviceClient = BlobServiceClient::CreateFromConnectionString(connectionString);
  BlobContainerClient containerClient = serviceClient.GetBlobContainerClient(containerName);
  
  // Stream blobs in batches
  auto blobList = containerClient.ListBlobs();
  
  for (const auto& blob : blobList.Blobs)
  {
    // Download blob to memory (for small blobs)
    BlobClient blobClient = containerClient.GetBlobClient(blob.Name);
    std::vector<uint8_t> blobData = blobClient.DownloadToBuffer();
    
    // Add to queue
    std::wstring blobName(blob.Name.begin(), blob.Name.end());
    ParallelStreamQueue_AddStream(
        queue,
        blobData.data(),
        blobData.size(),
        blobName.c_str());
  }
  
  // Process queue
  ParallelStreamQueue_StartProcessing(queue, L"azure_archive.7z");
  ParallelStreamQueue_WaitForCompletion(queue);
  
  // Get status
  UInt32 processed, failed, pending;
  ParallelStreamQueue_GetStatus(queue, &processed, &failed, &pending);
  
  printf("Processed: %u, Failed: %u, Pending: %u\n", processed, failed, pending);
  
  ParallelStreamQueue_Destroy(queue);
}
```

### Secure Compression with Encryption

```cpp
void CompressAzureBlobsWithEncryption(
    const std::string& connectionString,
    const std::string& containerName,
    const std::vector<uint8_t>& encryptionKey)
{
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  
  // Set encryption (AES-256)
  std::vector<uint8_t> iv(16, 0);  // Generate proper IV in production!
  ParallelCompressor_SetEncryption(
      compressor,
      encryptionKey.data(),
      encryptionKey.size(),
      iv.data(),
      iv.size());
  
  // ... compress as usual ...
}
```

### Progress Tracking with Azure Monitor

```cpp
void CompressWithMonitoring(
    const std::string& connectionString,
    const std::string& containerName)
{
  struct MonitoringData
  {
    std::chrono::steady_clock::time_point startTime;
    std::atomic<uint64_t> totalBytesProcessed;
    std::atomic<uint32_t> itemsCompleted;
  };
  
  MonitoringData monitorData;
  monitorData.startTime = std::chrono::steady_clock::now();
  monitorData.totalBytesProcessed = 0;
  monitorData.itemsCompleted = 0;
  
  auto progressCallback = [](UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void* userData)
  {
    MonitoringData* data = (MonitoringData*)userData;
    data->totalBytesProcessed += inSize;
    data->itemsCompleted++;
    
    auto elapsed = std::chrono::steady_clock::now() - data->startTime;
    auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    if (elapsedSec > 0)
    {
      double throughputMBps = (data->totalBytesProcessed / 1024.0 / 1024.0) / elapsedSec;
      printf("Progress: %u items, %.2f MB/s\n", data->itemsCompleted.load(), throughputMBps);
    }
  };
  
  ParallelCompressorHandle compressor = ParallelCompressor_Create();
  ParallelCompressor_SetCallbacks(compressor, progressCallback, nullptr, &monitorData);
  
  // ... compress ...
}
```

## Performance Tips

1. **Thread Count**: Use 1.5-2x the number of CPU cores for I/O-bound Azure workloads
2. **Compression Level**: Use level 3-5 for network streaming to balance speed and compression
3. **Batch Size**: Process 50-100 blobs at a time to balance memory and concurrency
4. **Network**: Use Azure regions close to your compute for best performance
5. **Blob Size**: Parallel compression works best with blobs > 1MB

## Error Handling

```cpp
auto errorCallback = [](UInt32 itemIndex, HRESULT errorCode, const wchar_t* message, void* userData)
{
  wprintf(L"Error on blob %u (0x%08X): %s\n", itemIndex, errorCode, message);
  
  // Log to Azure Monitor or Application Insights
  // ... logging code ...
};

ParallelCompressor_SetCallbacks(compressor, progressCallback, errorCallback, nullptr);
```

## Security Considerations

1. **Credentials**: Use Azure Managed Identity instead of connection strings in production
2. **Encryption**: Always use encryption for sensitive data
3. **Access Control**: Use Azure RBAC to limit blob access
4. **Audit**: Enable Azure Storage analytics for compliance

## Example: Complete Azure Integration

See `examples/azure_integration_example.cpp` for a complete working example.

## Troubleshooting

### Issue: Slow performance
- Check network latency to Azure
- Increase thread count
- Use lower compression level

### Issue: Out of memory
- Reduce batch size
- Use streaming mode instead of memory buffers
- Process blobs in smaller groups

### Issue: Authentication errors
- Verify connection string
- Check storage account permissions
- Ensure network access to Azure

## Additional Resources

- [Azure Storage Blobs SDK Documentation](https://github.com/Azure/azure-sdk-for-cpp/tree/main/sdk/storage/azure-storage-blobs)
- [7-Zip Parallel Compression API](PARALLEL_COMPRESSION.md)
- [Performance Tuning Guide](PERFORMANCE.md)
