# Zipper (Compression Class)

The Zipper class implements ZIP format compression functionality using the miniz library, supporting multiple compression levels and file addition methods.

## Features

- Lightweight compression implementation based on miniz library
- Supports multiple compression levels (no compression, fast compression, tight compression, etc.)
- Supports adding byte arrays and files to archives
- Provides event callback mechanism to monitor compression progress
- Supports duplicate filename handling

## Method Description

### Constructor
- `Zipper()` - Default constructor
- `Zipper(MinizCompression compression)` - Constructor with specified compression level

### File Addition Functions
- `AddByteFile` - Adds a byte array as a file to the archive
- `AddFile` - Adds a file from specified path to the archive
- `AddByteFileIgnoreDuplicate` - Adds a byte array file (ignores duplicate filenames)
- `AddFileIgnoreDuplicate` - Adds a file (ignores duplicate filenames)

### Archive Saving Functions
- `Save` - Saves compressed content to specified file path

### Compression Level Enum
- `MinizCompression::Undefine` - Undefined
- `MinizCompression::No` - No compression
- `MinizCompression::Fast` - Fast compression
- `MinizCompression::Tight` - Tight compression
- `MinizCompression::Unknown` - Unknown

## Usage Examples

```cpp
#include "fileoperator/zipper.hpp"

using namespace HsBa::Slicer;

// Create archive example
Zipper zipper(MinizCompression::Tight);
zipper.AddFile("document.txt", "/path/to/document.txt");
zipper.AddByteFile("config.json", R"({"setting": "value"})");
zipper.Save("archive.zip");

// Event listening example
zipper.Subscribe([](double progress, std::string_view filename) {
    std::cout << "Progress: " << progress << ", File: " << filename << std::endl;
});
```

## Notes

- Compression level selection requires balancing between compression ratio and processing speed
- Pay attention to memory usage when processing large files
- Release resources promptly after use to avoid memory leaks
- Pay attention to thread safety when using in multithreaded environments