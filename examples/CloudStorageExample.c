/**
 * Example: Compressing from Memory Streams / Cloud Storage
 * 
 * This example demonstrates how to compress data from memory buffers,
 * which is the foundation for integrating with cloud storage services
 * like Azure Blob Storage, AWS S3, or any other memory-based data source.
 * 
 * Use cases:
 * - Compress data received from Azure Blob Storage downloads
 * - Compress data from HTTP/REST API responses
 * - Compress in-memory data structures
 * - Compress data from network sockets
 */

#include "ParallelCompressAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simulates downloading data from Azure Blob Storage
// In a real implementation, this would call Azure SDK
void* SimulateAzureBlobDownload(const char* blobName, size_t* dataSize)
{
    printf("Simulating download from Azure Blob: %s\n", blobName);
    
    // Simulate different blob sizes
    size_t size = 0;
    if (strstr(blobName, "small"))
        size = 100 * 1024; // 100 KB
    else if (strstr(blobName, "medium"))
        size = 1024 * 1024; // 1 MB
    else if (strstr(blobName, "large"))
        size = 10 * 1024 * 1024; // 10 MB
    else
        size = 500 * 1024; // Default 500 KB
    
    // Allocate and fill with sample data
    void* buffer = malloc(size);
    if (!buffer) return NULL;
    
    // Fill with pseudo-random data (simulating real blob content)
    unsigned char* ptr = (unsigned char*)buffer;
    for (size_t i = 0; i < size; i++)
    {
        ptr[i] = (unsigned char)((i * 7 + 131) % 256);
    }
    
    *dataSize = size;
    return buffer;
}

// Callback for compression progress
void OnProgressCallback(UInt32 itemIndex, UInt64 inSize, UInt64 outSize, void* userData)
{
    double ratio = (inSize > 0) ? (100.0 * outSize / inSize) : 0.0;
    printf("  [Item %u] %llu bytes -> %llu bytes (%.1f%% compression)\n",
           itemIndex,
           (unsigned long long)inSize,
           (unsigned long long)outSize,
           ratio);
}

// Callback for errors
void OnErrorCallback(UInt32 itemIndex, HRESULT errorCode, const wchar_t* message, void* userData)
{
    wprintf(L"  [ERROR] Item %u failed (0x%08X): %s\n", itemIndex, errorCode, message);
}

/**
 * Example 1: Compress Multiple Memory Buffers
 * Simulates compressing data downloaded from Azure Blob Storage
 */
int Example_CompressFromMemoryStreams()
{
    printf("\n========================================\n");
    printf("Example 1: Compress from Memory Streams\n");
    printf("========================================\n\n");
    
    // Simulate blob names from Azure container
    const char* blobNames[] = {
        "data/small-log-file.txt",
        "data/medium-config.json",
        "backups/large-database-dump.sql",
        "images/medium-photo.jpg",
        "documents/small-readme.md"
    };
    const int numBlobs = sizeof(blobNames) / sizeof(blobNames[0]);
    
    // Allocate input items
    ParallelInputItemC* items = (ParallelInputItemC*)calloc(numBlobs, sizeof(ParallelInputItemC));
    void** buffers = (void**)malloc(numBlobs * sizeof(void*));
    
    printf("Downloading %d blobs from Azure...\n\n", numBlobs);
    
    // Download each blob into memory
    UInt64 totalSize = 0;
    for (int i = 0; i < numBlobs; i++)
    {
        size_t blobSize = 0;
        buffers[i] = SimulateAzureBlobDownload(blobNames[i], &blobSize);
        
        if (!buffers[i])
        {
            printf("ERROR: Failed to download blob %s\n", blobNames[i]);
            // Cleanup and return
            for (int j = 0; j < i; j++)
                free(buffers[j]);
            free(buffers);
            free(items);
            return 1;
        }
        
        // Set up input item
        items[i].Data = buffers[i];
        items[i].DataSize = blobSize;
        items[i].FilePath = NULL;
        
        // Convert blob name to wide string for archive
        size_t nameLen = strlen(blobNames[i]) + 1;
        wchar_t* wideName = (wchar_t*)malloc(nameLen * sizeof(wchar_t));
        mbstowcs(wideName, blobNames[i], nameLen);
        items[i].Name = wideName;
        
        items[i].Size = blobSize;
        items[i].UserData = NULL;
        
        totalSize += blobSize;
        printf("  Downloaded: %s (%zu KB)\n", blobNames[i], blobSize / 1024);
    }
    
    printf("\nTotal data size: %.2f MB\n", totalSize / (1024.0 * 1024.0));
    printf("\nCompressing to archive...\n\n");
    
    // Create compressor
    ParallelCompressorHandle compressor = ParallelCompressor_Create();
    
    // Configure for optimal cloud-based compression
    ParallelCompressor_SetNumThreads(compressor, 4);       // Use 4 threads
    ParallelCompressor_SetCompressionLevel(compressor, 5); // Balanced level
    ParallelCompressor_SetCallbacks(compressor, OnProgressCallback, OnErrorCallback, NULL, NULL);
    
    // Start compression timer
    clock_t startTime = clock();
    
    // Compress all memory buffers to 7z archive
    HRESULT hr = ParallelCompressor_CompressMultiple(
        compressor,
        items,
        numBlobs,
        L"azure_backup.7z"
    );
    
    clock_t endTime = clock();
    double elapsedTime = (double)(endTime - startTime) / CLOCKS_PER_SEC;
    
    // Report results
    printf("\n");
    if (SUCCEEDED(hr))
    {
        printf("✓ SUCCESS: Compressed %d blobs in %.2f seconds\n", numBlobs, elapsedTime);
        printf("  Output file: azure_backup.7z\n");
        printf("  Throughput: %.2f MB/s\n", (totalSize / (1024.0 * 1024.0)) / elapsedTime);
    }
    else
    {
        printf("✗ FAILED: Compression failed with error 0x%08X\n", hr);
    }
    
    // Cleanup
    for (int i = 0; i < numBlobs; i++)
    {
        free(buffers[i]);
        free((void*)items[i].Name);
    }
    free(buffers);
    free(items);
    ParallelCompressor_Destroy(compressor);
    
    return SUCCEEDED(hr) ? 0 : 1;
}

/**
 * Example 2: Compress with Encryption (for secure cloud backups)
 */
int Example_CompressWithEncryption()
{
    printf("\n========================================\n");
    printf("Example 2: Encrypted Cloud Backup\n");
    printf("========================================\n\n");
    
    const char* sensitiveBlobs[] = {
        "secrets/api-keys.json",
        "secrets/database-credentials.txt",
        "secrets/certificates.pem"
    };
    const int numBlobs = sizeof(sensitiveBlobs) / sizeof(sensitiveBlobs[0]);
    
    // Allocate input items
    ParallelInputItemC* items = (ParallelInputItemC*)calloc(numBlobs, sizeof(ParallelInputItemC));
    void** buffers = (void**)malloc(numBlobs * sizeof(void*));
    
    printf("Downloading %d sensitive blobs...\n\n", numBlobs);
    
    for (int i = 0; i < numBlobs; i++)
    {
        size_t blobSize = 0;
        buffers[i] = SimulateAzureBlobDownload(sensitiveBlobs[i], &blobSize);
        
        if (!buffers[i])
        {
            printf("ERROR: Failed to download blob %s\n", sensitiveBlobs[i]);
            for (int j = 0; j < i; j++)
                free(buffers[j]);
            free(buffers);
            free(items);
            return 1;
        }
        
        items[i].Data = buffers[i];
        items[i].DataSize = blobSize;
        items[i].FilePath = NULL;
        
        size_t nameLen = strlen(sensitiveBlobs[i]) + 1;
        wchar_t* wideName = (wchar_t*)malloc(nameLen * sizeof(wchar_t));
        mbstowcs(wideName, sensitiveBlobs[i], nameLen);
        items[i].Name = wideName;
        items[i].Size = blobSize;
        
        printf("  Downloaded: %s (%zu KB)\n", sensitiveBlobs[i], blobSize / 1024);
    }
    
    printf("\nCompressing with AES-256 encryption...\n\n");
    
    // Create compressor with encryption
    ParallelCompressorHandle compressor = ParallelCompressor_Create();
    ParallelCompressor_SetNumThreads(compressor, 2);
    ParallelCompressor_SetCompressionLevel(compressor, 7); // Higher compression for sensitive data
    ParallelCompressor_SetPassword(compressor, L"SecureCloudBackup2024!");
    ParallelCompressor_SetCallbacks(compressor, OnProgressCallback, OnErrorCallback, NULL, NULL);
    
    // Compress with encryption
    HRESULT hr = ParallelCompressor_CompressMultiple(
        compressor,
        items,
        numBlobs,
        L"secure_azure_backup.7z"
    );
    
    printf("\n");
    if (SUCCEEDED(hr))
    {
        printf("✓ SUCCESS: Encrypted archive created\n");
        printf("  Output: secure_azure_backup.7z\n");
        printf("  Encryption: AES-256\n");
        printf("  Note: Archive can only be extracted with the correct password\n");
    }
    else
    {
        printf("✗ FAILED: Encryption failed with error 0x%08X\n", hr);
    }
    
    // Cleanup
    for (int i = 0; i < numBlobs; i++)
    {
        free(buffers[i]);
        free((void*)items[i].Name);
    }
    free(buffers);
    free(items);
    ParallelCompressor_Destroy(compressor);
    
    return SUCCEEDED(hr) ? 0 : 1;
}

/**
 * Example 3: Incremental Backup Pattern
 * Shows how to compress new blobs as they become available
 */
int Example_IncrementalBackup()
{
    printf("\n========================================\n");
    printf("Example 3: Incremental Cloud Backup\n");
    printf("========================================\n\n");
    
    // Simulate processing blobs in batches (like Azure Event Grid triggers)
    const int batchCount = 3;
    const int blobsPerBatch = 3;
    
    for (int batch = 0; batch < batchCount; batch++)
    {
        printf("Processing batch %d of %d...\n", batch + 1, batchCount);
        
        ParallelInputItemC* items = (ParallelInputItemC*)calloc(blobsPerBatch, sizeof(ParallelInputItemC));
        void** buffers = (void**)malloc(blobsPerBatch * sizeof(void*));
        
        for (int i = 0; i < blobsPerBatch; i++)
        {
            char blobName[256];
            snprintf(blobName, sizeof(blobName), "batch%d/file%d.dat", batch, i);
            
            size_t blobSize = 0;
            buffers[i] = SimulateAzureBlobDownload(blobName, &blobSize);
            
            items[i].Data = buffers[i];
            items[i].DataSize = blobSize;
            items[i].FilePath = NULL;
            
            size_t nameLen = strlen(blobName) + 1;
            wchar_t* wideName = (wchar_t*)malloc(nameLen * sizeof(wchar_t));
            mbstowcs(wideName, blobName, nameLen);
            items[i].Name = wideName;
            items[i].Size = blobSize;
        }
        
        // Create archive name with batch number
        wchar_t archiveName[256];
        swprintf(archiveName, 256, L"incremental_backup_%d.7z", batch);
        
        // Compress batch
        ParallelCompressorHandle compressor = ParallelCompressor_Create();
        ParallelCompressor_SetNumThreads(compressor, 2);
        ParallelCompressor_SetCompressionLevel(compressor, 5);
        
        HRESULT hr = ParallelCompressor_CompressMultiple(
            compressor,
            items,
            blobsPerBatch,
            archiveName
        );
        
        if (SUCCEEDED(hr))
        {
            wprintf(L"  ✓ Created: %s\n", archiveName);
        }
        else
        {
            wprintf(L"  ✗ Failed: %s (error 0x%08X)\n", archiveName, hr);
        }
        
        // Cleanup batch
        for (int i = 0; i < blobsPerBatch; i++)
        {
            free(buffers[i]);
            free((void*)items[i].Name);
        }
        free(buffers);
        free(items);
        ParallelCompressor_Destroy(compressor);
        
        printf("\n");
    }
    
    printf("Incremental backup complete.\n");
    printf("Created %d archive files.\n", batchCount);
    
    return 0;
}

/**
 * Main function
 */
int main(int argc, char* argv[])
{
    printf("=========================================================\n");
    printf("7-Zip Parallel Compression - Cloud Storage Examples\n");
    printf("=========================================================\n");
    printf("\n");
    printf("This demonstrates compressing data from memory buffers,\n");
    printf("which is the foundation for cloud storage integration.\n");
    printf("\n");
    printf("Real-world applications:\n");
    printf("  • Azure Blob Storage backups\n");
    printf("  • AWS S3 data archival\n");
    printf("  • Google Cloud Storage compression\n");
    printf("  • HTTP/REST API data compression\n");
    printf("  • In-memory data structure archival\n");
    printf("\n");
    
    int result = 0;
    
    // Run examples
    result |= Example_CompressFromMemoryStreams();
    result |= Example_CompressWithEncryption();
    result |= Example_IncrementalBackup();
    
    printf("\n=========================================================\n");
    if (result == 0)
    {
        printf("✓ All examples completed successfully!\n");
        printf("\nGenerated archives:\n");
        printf("  • azure_backup.7z (multi-threaded compression)\n");
        printf("  • secure_azure_backup.7z (AES-256 encrypted)\n");
        printf("  • incremental_backup_0.7z (batch 1)\n");
        printf("  • incremental_backup_1.7z (batch 2)\n");
        printf("  • incremental_backup_2.7z (batch 3)\n");
        printf("\nTo integrate with real Azure Blob Storage:\n");
        printf("  1. Add Azure Storage SDK for C++\n");
        printf("  2. Replace SimulateAzureBlobDownload() with:\n");
        printf("     BlobClient::DownloadTo() API calls\n");
        printf("  3. Handle authentication (connection string or managed identity)\n");
        printf("  4. Add error handling and retry logic\n");
    }
    else
    {
        printf("✗ Some examples failed!\n");
    }
    printf("=========================================================\n");
    
    return result;
}
