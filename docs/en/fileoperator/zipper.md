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

## Bit7zZipper (7-Zip based compressor, optional)

When the project is compiled with `HSBA_USE_BIT7Z`, the `Bit7zZipper` class (header `fileoperator/bit7z_zipper.hpp`) provides multi-format compression based on the bit7z (7-Zip) library, with formats defined in `ZipperFormat`:

| Format | Output | Description |
|--------|--------|-------------|
| `Zip` / `SevenZip` | `.zip` / `.7z` | Password support |
| `XZ` / `BZIP2` / `GZIP` / `TAR` | `.xz` / `.bz2` / `.gz` / `.tar` | Single-layer formats |
| `TarGz` | `.tar.gz` / `.tgz` | Direct two-stage compression: items are packed into a tar in memory, then gzip-compressed in one `Save` call |
| `TarXz` | `.tar.xz` / `.txz` | Direct two-stage compression: items are packed into a tar in memory, then xz-compressed in one `Save` call |

```cpp
#include "fileoperator/bit7z_zipper.hpp"

using namespace HsBa::Slicer;

// Create a .tar.gz archive directly (same applies to ZipperFormat::TarXz)
Bit7zZipper zipper{HSBA_7Z_DLL, ZipperFormat::TarGz, ""};
zipper.AddFile("document.txt", "/path/to/document.txt");
zipper.AddByteFile("config.json", R"({"setting": "value"})");
zipper.Save("archive.tar.gz");
```

In Lua scripts, the same formats are available via `Bit7zZipper.new("TarGz", dll_path)` (see the Lua Pipeline API documentation).