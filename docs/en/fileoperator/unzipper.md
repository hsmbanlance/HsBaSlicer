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