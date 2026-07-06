#pragma once
#ifndef HSBA_SLICER_OCCT_TYPE_ALIASES_HPP
#define HSBA_SLICER_OCCT_TYPE_ALIASES_HPP

#include <cstdint>

#include <Standard.hxx>
#include <Standard_Version.hxx>

// OCC_VERSION_MAJOR and OCC_VERSION_MINOR defined in Standard_Version.hxx
#if defined(OCC_VERSION_MAJOR) && (OCC_VERSION_MAJOR > 8 || (OCC_VERSION_MAJOR == 8 && OCC_VERSION_MINOR >= 0))
#define HSBA_OCCT_VERSION_8_PLUS 1
#else
#define HSBA_OCCT_VERSION_8_PLUS 0
#endif

namespace HsBa::Slicer::OcctTypes
{
#if HSBA_OCCT_VERSION_8_PLUS
// OCCT 8.0.0+ 使用标准C++类型
using Integer = int;
using Boolean = bool;
using Real = double;
using Byte = uint8_t;
using Character = char;
using ExtCharacter = char16_t;
#else
// use Standard_XX before OCCT 8.0.0
using Integer = Standard_Integer;
using Boolean = Standard_Boolean;
using Real = Standard_Real;
using Byte = Standard_Byte;
using Character = Standard_Character;
using ExtCharacter = Standard_ExtCharacter;
#endif
}  // namespace HsBa::Slicer::OcctTypes

#endif  // HSBA_SLICER_OCCT_TYPE_ALIASES_HPP
