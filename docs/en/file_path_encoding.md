# File Path Encoding & Non-ASCII Support

This guide explains how HsBaSlicer handles file paths that contain non-ASCII
characters (CJK, accented letters, emoji, spaces) when **reading model files**
and **saving result files**, and how to keep that support correct across
platforms.

## 1. The core rule: internal paths are UTF-8

Everywhere inside HsBaSlicer a file path is carried as a **UTF-8 `std::string`**.
This is also the contract of the public C ABI (`DllHsBaSlicer`, e.g. the
`file_path` argument of the model-loading entry points is documented as
"Model file path (UTF-8)").

The problem is only at the **boundary** where a UTF-8 string meets a platform
API that expects some *other* encoding:

- On Windows a narrow `char*` path passed to `fopen` / `CreateFileA` is
  interpreted using the **process ANSI code page** (`GetACP()`), which for a
  legacy locale (e.g. 936/GBK, 1252) cannot represent every Unicode name.
- `std::filesystem::path` on Windows has a **wide** (`std::wstring`) native
  representation. Building one directly from a UTF-8 `std::string`
  (`std::filesystem::path(utf8Str)`) re-parses the bytes through the ANSI code
  page and **mangles** characters outside it.

## 2. Conversion helpers (`base/encoding_convert.hpp`)

```cpp
namespace HsBa::Slicer {
std::string           utf8_to_local(const std::string& str);   // UTF-8 -> local (ANSI) bytes
std::string           local_to_utf8(const std::string& str);   // local (ANSI) bytes -> UTF-8
std::filesystem::path utf8_to_path(const std::string& utf8Path);// UTF-8 -> native wide path
std::string           path_to_utf8(const std::filesystem::path& path); // native path -> UTF-8
}
```

- `utf8_to_path` decodes UTF-8 into the wide (UTF-16) representation on Windows,
  so the result is independent of the ANSI code page and carries any Unicode
  name. On POSIX/Android/iOS it is a pass-through (native encoding is UTF-8).
- `path_to_utf8` is its inverse; on non-Windows it equals `path.string()`.
- `utf8_to_local` / `local_to_utf8` convert to/from the local narrow encoding
  for APIs that can *only* take a narrow `char*`. When the process code page is
  already UTF-8 (see §4) they become pass-throughs.

## 3. Which helper to use, by API kind

Pick the conversion according to what the *callee* expects — never mix two
conversions and never do a `path.string()` round-trip:

| Target API kind                                   | Example                                | Use                                   |
|---------------------------------------------------|----------------------------------------|---------------------------------------|
| Wide / `std::filesystem` native                    | `std::ofstream(fs::path)`, `std::ifstream`, `std::filesystem::*` | `utf8_to_path(utf8Str)` → pass the `fs::path` |
| UTF-8-expecting narrow API                         | `sqlite3_open`, `Zipper::Save`, Lua `output_path` | `path_to_utf8(path)` (or the raw UTF-8 string) |
| Narrow `char*` interpreted by the ANSI code page   | libigl/CGAL/OpenCASCADE/OpenVDB/miniz (`fopen`)    | `utf8_to_local(utf8Str)`               |

**Anti-pattern to avoid:** `std::filesystem::path(utf8Str).string()` — this
parses UTF-8 bytes as ANSI (Windows) and then hands back an ANSI string to a
UTF-8-expecting API, double-corrupting the path. The ZIP package writers
(`Zipper`, `Bit7zZipper`) and the `ImagesPath`/`LayersPath` result savers
previously did exactly this and have been corrected to use `path_to_utf8` /
`utf8_to_local` once, as appropriate.

This rule is applied across:

- **Model reading**: `IglModel`, `CgalModel`, `OcctModel`, `OpenVdbModel`
  (narrow backends go through `utf8_to_local`; the OpenVDB XYZ reader/writer now
  opens through a wide `std::filesystem::path`).
- **Result saving**: `Zipper` / `Bit7zZipper` (archive path), `ImagesPath`
  (ZIP + Lua), `LayersPath` (SQLite + Lua), and the export callers in
  `LibHsBaSlicer` (`sla_floor`, `sls_export`, `waam_export`) that build the
  output `std::filesystem::path` via `utf8_to_path`.

## 4. Windows UTF-8 active code page manifest (the complete fix)

Because libigl / CGAL / OpenCASCADE / OpenVDB / miniz accept **only narrow**
paths, §3 alone cannot guarantee full Unicode for them under a legacy ANSI code
page. HsBaSlicer instead elevates the whole process code page to UTF-8 by
embedding an application manifest:

```xml
<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
<longPathAware   xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
```

- File: **`cmake/hsba_utf8.manifest`**; applied by the reusable function
  `hsba_apply_utf8_codepage(<target>)` in **`cmake/HsBaWindowsUtf8.cmake`**
  (merges the fragment into the already-linked executable with the manifest
  tool `mt.exe` in a post-build step, so it works with the Ninja / MSBuild /
  Makefile generators; a no-op on non-MSVC / non-Windows toolchains).
  It is applied per-target from each executable's own `CMakeLists.txt`.
- It is applied to every desktop executable we own (the `HsBaSlicer` app and
  all samples).

Effect: `GetACP()` returns 65001, so `utf8_to_local` becomes a pass-through
(`windows_chcp_utf8()` is true) and the CRT converts the UTF-8 narrow path to
UTF-16 correctly — **all** narrow-API formats gain full Unicode support (and
long paths, via `longPathAware`) with a single build-config change.

## 5. Limitations and integration notes

- A process manifest comes from the **executable that owns the process**. When
  the library is consumed as **`DllHsBaSlicer`** from an external C#/Java/Python
  host, the *host executable's* manifest governs the code page — HsBaSlicer
  cannot change it. In that scenario the wide-API handling (`utf8_to_path` /
  `path_to_utf8`) still protects our own file I/O; for full Unicode on the
  narrow third-party paths, the host should declare the same `activeCodePage`
  UTF-8 manifest in its own executable.
- On POSIX/Android/iOS no manifest is needed: the native path encoding is UTF-8.
- To read/print non-ASCII text on the Windows console, optionally call
  `set_windows_console_utf8()` at startup.

## See also

- [Base module](./base/) — encoding and general utilities
- [FileOperator module](./fileoperator/) — Zipper / Unzipper
- [C++ Usage Guide (CMake Integration)](./cpp_cmake_usage.md)
