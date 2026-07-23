/// @file module_anchor.cpp
/// @brief Anchor source to ensure CMake generates the static library archive.
///
/// A static library with ONLY CXX_MODULES FILE_SET sources may not trigger
/// the archiver (lib.exe) in some CMake versions. This empty TU guarantees
/// ModuleHsBaSlicer.lib is produced.

namespace HsBa::Slicer::detail
{
// Intentionally empty - forces the archiver to run.
inline constexpr int kModuleAnchor = 1;
}  // namespace HsBa::Slicer::detail
