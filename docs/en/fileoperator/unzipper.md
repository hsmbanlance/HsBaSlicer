# Unzipper (Decompression Class)

The Unzipper class implements ZIP format decompression functionality using the miniz library, supporting file reading and streaming access.

## Features

- Lightweight decompression implementation based on miniz library
- Supports direct reading of archive content from file
- Provides streaming access to decompressed files
- Supports memory caching and temporary file caching
- Configurable maximum memory usage

## Method Description

### Instance Creation
- `Create()` - Static method to create Unzipper instance

### Decompression Functions
- `ReadFromFileImpl` - Reads archive content from file
- `GetStreamImpl` - Gets stream object for specified file

### Memory Management
- `SetMaxMemSize` - Sets maximum memory usage (default 1GB)
- `max_mem_size_` - Static member variable controlling memory usage limit

## Usage Examples

```cpp
#include "fileoperator/unzipper.hpp"

using namespace HsBa::Slicer;

// Extract archive example
auto unzipper = Unzipper::Create();
unzipper->ReadFromFileImpl("archive.zip", false);

// Get decompressed file stream
auto stream = unzipper->GetStreamImpl("document.txt");

// Set maximum memory usage
Unzipper::SetMaxMemSize(512 * 1024 * 1024); // 512MB
```

## Notes

- Verify file integrity during decompression to prevent malicious file attacks
- Pay attention to memory usage when processing large files, set caching strategies appropriately
- Release resources promptly after use to avoid memory leaks
- Pay attention to thread safety when using in multithreaded environments

## Bit7ZUnzipper (7-Zip based extractor, optional)

When the project is compiled with `HSBA_USE_BIT7Z`, the `Bit7ZUnzipper` class (header `fileoperator/bit7z_unzipper.hpp`) provides extraction based on the bit7z (7-Zip) library, supporting archives such as ZIP, 7Z and RAR. Compressed tar archives — `.tar.gz` / `.tgz` / `.tar.xz` / `.txz` — are unpacked **directly in one step**: the inner tar is recognized from the archive extension, transparently unpacked to a temporary file and opened, so `GetStream` gives access to the real files inside the tar.

```cpp
#include "fileoperator/bit7z_unzipper.hpp"

using namespace HsBa::Slicer;

auto unzipper = Bit7ZUnzipper::Create(HSBA_7Z_DLL);
unzipper->SetPassword("");                       // optional, for encrypted archives
unzipper->ReadFromFile("archive.tar.gz");        // inner tar unpacked transparently
auto stream = unzipper->GetStream("document.txt"); // read the real file inside the tar
```

The free functions `Bit7zExtract(archive, outdir, ...)` and `Bit7zExtract(archive, bufs, ...)` (header `fileoperator/bit7z_zipper.hpp`) accept `.tar.gz` / `.tar.xz` paths as well and extract all inner files in one call.

### Notes for compressed tars

- Detection is extension-based; if the file is not a real compressed tar, the reader automatically falls back to reading the outer archive itself.
- The unpacked inner tar lives in a unique temporary file under the system temp directory; it is removed automatically when reopening another archive or destroying the unzipper.