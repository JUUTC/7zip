# Azure Blob Storage Integration Guide

## Overview

This guide explains how to integrate the 7-Zip Parallel Compression Library with Azure Blob Storage for cloud-based compression scenarios.

## Architecture

```
Azure Blob Storage
       ↓
   [Download]
       ↓
  Memory Buffer ───→ ParallelCompressor ───→ 7z Archive
       ↓
  [Optional Upload back to Azure]
```

## Prerequisites

### Required Libraries
- 7-Zip Parallel Compression Library (this project)
- Azure Storage SDK for C++ (optional, for real Azure integration)

### Installation

#### Ubuntu/Debian
```bash
# Install build tools
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Install Azure SDK (optional)
# See: https://github.com/Azure/azure-sdk-for-cpp
```

#### Windows
```powershell
# Install via vcpkg
vcpkg install azure-storage-blobs-cpp
```

## Basic Usage

### Method 1: Memory Buffer Approach (Current Implementation)

The library already supports compressing data from memory buffers, which works perfectly for Azure Blob downloads:

```c
#include "ParallelCompressAPI.h"
#include <azure/storage/blobs.hpp>

// Download blob to memory
auto blobClient = containerClient.GetBlobClient("myblob.dat");
std::vector<uint8_t> blobData;
blobClient.DownloadTo(blobData);

// Create input item
ParallelInputItemC item;
item.Data = blobData.data();
item.DataSize = blobData.size();
item.Name = L"myblob.dat";
item.Size = blobData.size();

// Compress
ParallelCompressorHandle compressor = ParallelCompressor_Create();
ParallelCompressor_SetNumThreads(compressor, 4);
ParallelCompressor_SetCompressionLevel(compressor, 5);

HRESULT hr = ParallelCompressor_CompressMultiple(
    compressor, &item, 1, L"output.7z"
);

ParallelCompressor_Destroy(compressor);
```

### Method 2: Custom Stream Adapter (Advanced)

For large blobs or streaming scenarios, implement a custom stream adapter:

```cpp
// AzureBlobStream.h
class CAzureBlobInStream : 
    public ISequentialInStream,
    public CMyUnknownImp
{
    Z7_COM_UNKNOWN_IMP_1(ISequentialInStream)
    Z7_IFACE_COM7_IMP(ISequentialInStream)

private:
    Azure::Storage::Blobs::BlobClient _blobClient;
    uint64_t _position;
    uint64_t _size;
    std::vector<uint8_t> _buffer;
    const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB chunks

public:
    CAzureBlobInStream(const Azure::Storage::Blobs::BlobClient& client)
        : _blobClient(client), _position(0)
    {
        // Get blob size
        auto properties = _blobClient.GetProperties();
        _size = properties.Value.BlobSize;
    }

    STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize)
    {
        try
        {
            if (_position >= _size)
            {
                *processedSize = 0;
                return S_OK;
            }

            // Download chunk from Azure
            Azure::Storage::Blobs::DownloadBlobToOptions options;
            options.Range = Azure::Core::Http::HttpRange();
            options.Range.Value().Offset = _position;
            options.Range.Value().Length = size;

            auto response = _blobClient.DownloadTo(
                (uint8_t*)data, size, options
            );

            *processedSize = (UInt32)response.Value.ContentRange.Length.Value();
            _position += *processedSize;

            return S_OK;
        }
        catch (const std::exception& e)
        {
            *processedSize = 0;
            return E_FAIL;
        }
    }
};
```

## Complete Example: Azure Blob Backup

```cpp
#include "ParallelCompressAPI.h"
#include <azure/storage/blobs.hpp>
#include <azure/identity.hpp>
#include <iostream>
#include <vector>

using namespace Azure::Storage::Blobs;

int main()
{
    try
    {
        // 1. Connect to Azure Blob Storage
        std::string connectionString = 
            std::getenv("AZURE_STORAGE_CONNECTION_STRING");
        
        auto containerClient = 
            BlobContainerClient::CreateFromConnectionString(
                connectionString, "my-container"
            );

        // 2. List blobs to compress
        std::vector<std::string> blobNames;
        for (auto blobItem : containerClient.ListBlobs())
        {
            blobNames.push_back(blobItem.Name);
        }

        std::cout << "Found " << blobNames.size() << " blobs to compress\n";

        // 3. Download blobs to memory
        std::vector<std::vector<uint8_t>> blobData;
        std::vector<ParallelInputItemC> items;

        for (const auto& name : blobNames)
        {
            auto blobClient = containerClient.GetBlobClient(name);
            
            std::vector<uint8_t> data;
            blobClient.DownloadTo(data);
            blobData.push_back(std::move(data));

            ParallelInputItemC item = {};
            item.Data = blobData.back().data();
            item.DataSize = blobData.back().size();
            
            // Convert to wide string
            std::wstring wideName(name.begin(), name.end());
            item.Name = _wcsdup(wideName.c_str());
            item.Size = blobData.back().size();
            
            items.push_back(item);
            
            std::cout << "  Downloaded: " << name 
                      << " (" << data.size() / 1024 << " KB)\n";
        }

        // 4. Compress all blobs
        std::cout << "\nCompressing to archive...\n";

        ParallelCompressorHandle compressor = ParallelCompressor_Create();
        ParallelCompressor_SetNumThreads(compressor, 8);
        ParallelCompressor_SetCompressionLevel(compressor, 5);
        ParallelCompressor_SetPassword(compressor, L"SecureBackup!");

        HRESULT hr = ParallelCompressor_CompressMultiple(
            compressor,
            items.data(),
            (UInt32)items.size(),
            L"azure_backup.7z"
        );

        if (SUCCEEDED(hr))
        {
            std::cout << "✓ Compression successful!\n";
            std::cout << "  Output: azure_backup.7z\n";
        }
        else
        {
            std::cerr << "✗ Compression failed: 0x" 
                      << std::hex << hr << "\n";
            return 1;
        }

        // 5. (Optional) Upload compressed archive back to Azure
        auto archiveBlobClient = 
            containerClient.GetBlockBlobClient("backups/archive.7z");
        
        archiveBlobClient.UploadFrom("azure_backup.7z");
        std::cout << "✓ Uploaded compressed archive to Azure\n";

        // Cleanup
        for (auto& item : items)
        {
            free((void*)item.Name);
        }
        ParallelCompressor_Destroy(compressor);

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

## Authentication Methods

### Method 1: Connection String
```cpp
std::string connectionString = 
    std::getenv("AZURE_STORAGE_CONNECTION_STRING");

auto client = BlobContainerClient::CreateFromConnectionString(
    connectionString, "container-name"
);
```

### Method 2: Managed Identity (Azure VMs/App Service)
```cpp
#include <azure/identity.hpp>

auto credential = std::make_shared<Azure::Identity::DefaultAzureCredential>();

auto client = BlobContainerClient(
    "https://myaccount.blob.core.windows.net/container",
    credential
);
```

### Method 3: SAS Token
```cpp
std::string sasToken = "?sv=2020-08-04&ss=b&srt=sco&sp=rwdlac...";
std::string url = "https://myaccount.blob.core.windows.net/container" + sasToken;

auto client = BlobContainerClient(url);
```

## Best Practices

### 1. Memory Management
For large blobs, consider chunked processing:

```cpp
// Download in chunks instead of loading entire blob
const size_t CHUNK_SIZE = 10 * 1024 * 1024; // 10 MB

auto properties = blobClient.GetProperties();
uint64_t blobSize = properties.Value.BlobSize;

for (uint64_t offset = 0; offset < blobSize; offset += CHUNK_SIZE)
{
    uint64_t length = std::min(CHUNK_SIZE, blobSize - offset);
    
    Azure::Storage::Blobs::DownloadBlobToOptions options;
    options.Range = Azure::Core::Http::HttpRange();
    options.Range.Value().Offset = offset;
    options.Range.Value().Length = length;
    
    // Download and process chunk
}
```

### 2. Parallel Downloads
Download multiple blobs concurrently:

```cpp
#include <thread>
#include <future>

std::vector<std::future<std::vector<uint8_t>>> downloads;

for (const auto& blobName : blobNames)
{
    downloads.push_back(std::async(std::launch::async, [&]()
    {
        auto blobClient = containerClient.GetBlobClient(blobName);
        std::vector<uint8_t> data;
        blobClient.DownloadTo(data);
        return data;
    }));
}

// Wait for all downloads
for (auto& download : downloads)
{
    auto data = download.get();
    // Process data
}
```

### 3. Error Handling
```cpp
#include <azure/core/exception.hpp>

try
{
    blobClient.DownloadTo(buffer);
}
catch (const Azure::Core::RequestFailedException& e)
{
    if (e.StatusCode == Azure::Core::Http::HttpStatusCode::NotFound)
    {
        std::cerr << "Blob not found\n";
    }
    else if (e.StatusCode == Azure::Core::Http::HttpStatusCode::Forbidden)
    {
        std::cerr << "Access denied - check permissions\n";
    }
    else
    {
        std::cerr << "Azure error: " << e.what() << "\n";
    }
}
```

### 4. Compression Settings for Cloud Data

```c
// For logs/text (high compression ratio)
ParallelCompressor_SetCompressionLevel(compressor, 9);

// For already compressed data (images, videos)
ParallelCompressor_SetCompressionLevel(compressor, 1);

// Balanced (general purpose)
ParallelCompressor_SetCompressionLevel(compressor, 5);

// Use solid mode for similar files
ParallelCompressor_SetSolidMode(compressor, 1);
ParallelCompressor_SetSolidBlockSize(compressor, 100);
```

## Performance Optimization

### Tuning Thread Count
```c
// Get optimal thread count based on CPU cores
#include <thread>
unsigned int cores = std::thread::hardware_concurrency();
unsigned int threads = cores > 0 ? cores : 4;

ParallelCompressor_SetNumThreads(compressor, threads);
```

### Memory Budget
```
Memory per thread ≈ 24 MB (LZMA level 5)

Example:
- 8 threads × 24 MB = 192 MB compression memory
- + blob data in memory
- + output buffer

Total ≈ 200-300 MB for typical scenario
```

### Network Optimization
```cpp
// Enable HTTP/2 for better throughput
Azure::Core::Http::CurlTransportOptions options;
options.HttpVersion = Azure::Core::Http::HttpVersion::Http2;

// Increase timeout for large blobs
options.ConnectionTimeout = std::chrono::minutes(5);

BlobClientOptions clientOptions;
clientOptions.Transport.Transport = 
    std::make_shared<Azure::Core::Http::CurlTransport>(options);
```

## Monitoring and Logging

```c
void ProgressCallback(UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void* userData)
{
    double ratio = (inSize > 0) ? (100.0 * outSize / inSize) : 0.0;
    
    // Log to Application Insights or Azure Monitor
    printf("[METRIC] blob=%u in=%llu out=%llu ratio=%.1f%%\n",
           itemIndex, inSize, outSize, ratio);
}

void ErrorCallback(UInt32 itemIndex, HRESULT errorCode, 
                   const wchar_t* message, void* userData)
{
    // Log error to Azure Monitor
    wprintf(L"[ERROR] blob=%u code=0x%08X msg=%s\n",
            itemIndex, errorCode, message);
}

ParallelCompressor_SetCallbacks(
    compressor, ProgressCallback, ErrorCallback, NULL, NULL
);
```

## Deployment Scenarios

### Scenario 1: Azure Function (Event-Driven)
```
Blob Created Event → Azure Function → Download → Compress → Upload
```

### Scenario 2: Azure VM (Scheduled)
```
Cron Job → List Blobs → Download → Compress → Upload → Delete Old
```

### Scenario 3: Azure Container Instance (Batch)
```
ACI Job → Process Blob List → Parallel Compress → Archive to Cool Tier
```

## Security Considerations

1. **Always encrypt sensitive data**
   ```c
   ParallelCompressor_SetPassword(compressor, L"StrongPassword123!");
   ```

2. **Use Managed Identity** (avoid connection strings in code)

3. **Validate blob checksums**
   ```cpp
   auto properties = blobClient.GetProperties();
   std::string md5 = properties.Value.HttpHeaders.ContentHash.ToString();
   ```

4. **Use Private Endpoints** for Azure Blob Storage

5. **Enable Soft Delete** to recover from accidental deletion

## Troubleshooting

### Issue: Out of Memory
**Solution:** Process blobs in batches or use streaming approach

### Issue: Slow Downloads
**Solution:** Use parallel downloads, check network bandwidth

### Issue: Authentication Failures
**Solution:** Verify connection string, check SAS token expiration

### Issue: Compression Too Slow
**Solution:** Reduce compression level, increase thread count

## Complete CMakeLists.txt with Azure Support

```cmake
cmake_minimum_required(VERSION 3.15)
project(AzureBlobCompressor)

set(CMAKE_CXX_STANDARD 17)

# Find Azure SDK
find_package(azure-storage-blobs-cpp CONFIG REQUIRED)
find_package(azure-identity-cpp CONFIG REQUIRED)

# Find 7zip-parallel
find_package(7zip-parallel REQUIRED)

add_executable(azure_blob_compressor
    azure_blob_example.cpp
)

target_link_libraries(azure_blob_compressor
    PRIVATE
        Azure::azure-storage-blobs
        Azure::azure-identity
        7zip-parallel
)
```

## Next Steps

1. Review the example code in `examples/CloudStorageExample.c`
2. Install Azure SDK for C++ if needed
3. Set up Azure Storage account and container
4. Configure authentication (connection string or managed identity)
5. Test with small blobs first, then scale up
6. Monitor performance and adjust thread count/compression level

## References

- Azure Storage SDK for C++: https://github.com/Azure/azure-sdk-for-cpp
- 7-Zip Parallel Compression: ../README.md
- Azure Blob Storage Documentation: https://docs.microsoft.com/azure/storage/blobs/
