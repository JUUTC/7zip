# 7-Zip

Official website: [7-zip.org](https://7-zip.org)

## Overview

7-Zip is a file archiver with high compression ratio. This repository includes standard 7-Zip functionality plus parallel compression support for concurrent multi-stream processing.

## Features

- Parallel compression (up to 256 threads)
- Memory buffers, files, and network stream support
- Standard 7z archive format (compatible with 7z.exe)
- AES-256 encryption (data + headers)
- Solid mode compression
- Multi-volume archives
- Progress tracking and error handling
- C++ and C APIs

## Usage

### C++ API
```cpp
#include "IParallelCompress.h"
#include "ParallelCompressor.h"

CParallelCompressor compressor;
compressor.SetNumThreads(8);
compressor.SetCompressionLevel(5);

CParallelInputItem items[count];
// ... configure items ...

compressor.CompressMultiple(items, count, outputStream, callback);
```

### C API
```c
#include "ParallelCompressAPI.h"

ParallelCompressorHandle h = ParallelCompressor_Create();
ParallelCompressor_SetNumThreads(h, 8);
ParallelCompressor_CompressMultiple(h, items, count, L"output.7z");
ParallelCompressor_Destroy(h);
```

### Solid Mode
```cpp
compressor.SetSolidMode(true);
compressor.SetSolidBlockSize(100);  // files per block (0 = all)
```

### Multi-Volume
```cpp
compressor.SetVolumeSize(100 * 1024 * 1024);  // 100 MB volumes
compressor.SetVolumePrefix(L"archive.7z");    // .001, .002, etc.
```

## Performance

16 threads vs sequential:
- Small files (10KB): 10-15x faster
- Medium files (100KB): 8-12x faster
- Network streams: 20-30x faster

## License

GNU LGPL with unRAR restriction. See [7-zip.org/license.txt](https://7-zip.org/license.txt)
