/** @file ModelFormat.hpp
 * @brief A header file containing the definition of model format enums and related functions for the HsBa Slicer
 * project. This file defines an enum for representing different model formats and provides functions for converting
 * between format identifiers and their string representations.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_MODELFORMAT_HPP
#define HSBA_SLICER_MODELFORMAT_HPP

#include <string>
#include <string_view>

#include "IModel.hpp"

namespace HsBa::Slicer
{

/** @brief Convert a ModelFormat enum value to its corresponding string representation.
 * @param format The ModelFormat enum value to convert.
 * @return A string representation of the given ModelFormat value.
 */
inline const char* ToString(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::UnknownPLY:
        return "PLY";
    case ModelFormat::BinaryPLY:
        return "BinaryPLY";
    case ModelFormat::ASCIIPLY:
        return "ASCIIPLY";
    case ModelFormat::OBJ:
        return "OBJ";
    case ModelFormat::UnknownSTL:
        return "STL";
    case ModelFormat::BinarySTL:
        return "BinarySTL";
    case ModelFormat::ASCIISTL:
        return "ASCIISTL";
    case ModelFormat::OFF:
        return "OFF";
    case ModelFormat::VRML:
        return "VRML";
    case ModelFormat::STEP:
        return "STEP";
    case ModelFormat::IGES:
        return "IGES";
    case ModelFormat::SLDPRT:
        return "SLDPRT";
    case ModelFormat::CATPART:
        return "CATPART";
    case ModelFormat::XYZ:
        return "XYZ";
    default:
        return "Unknown";
    }
}

/** @brief Get the file extension name from a file name.
 * @param file_name The file name.
 * @return The file extension name.
 */
std::string GetExtName(const std::string& file_name);

/** @brief Determine the model format based on the file extension name.
 * @param file_name The file name.
 * @return The model format corresponding to the file extension.
 */
ModelFormat ModelTypeFromExtName(const std::string& file_name);

/** @brief Check if a model format is a mesh format.
 * @param format The model format to check.
 * @return true if the format is a mesh format, false otherwise.
 */
bool IsMeshFormat(ModelFormat format);
/** @brief Check if a model format is a BRep format.
 * @param format The model format to check.
 * @return true if the format is a BRep format, false otherwise.
 */
bool IsBrepFormat(ModelFormat format);
/** @brief Check if a model format is a CSG format.
 * @param format The model format to check.
 * @return true if the format is a CSG format, false otherwise.
 */
bool IsCSGFormat(ModelFormat format);
/** @brief Check if a model format is a point cloud format.
 * @param format The model format to check.
 * @return true if the format is a point cloud format, false otherwise.
 */
bool IsPointCloudFormat(ModelFormat format);

/** @brief Check if a file name corresponds to a mesh format.
 * @param file_name The file name to check.
 * @return true if the file name corresponds to a mesh format, false otherwise.
 */
bool IsMeshFormat(const std::string& file_name);
/** @brief Check if a file name corresponds to a BRep format.
 * @param file_name The file name to check.
 * @return true if the file name corresponds to a BRep format, false otherwise.
 */
bool IsBrepFormat(const std::string& file_name);
bool IsCSGFormat(const std::string& file_name);
/** @brief Check if a file name corresponds to a point cloud format.
 * @param file_name The file name to check.
 * @return true if the file name corresponds to a point cloud format, false otherwise.
 */
bool IsPointCloudFormat(const std::string& file_name);
}  // namespace HsBa::Slicer
#endif  // !HSBA_SLICER_MODELFORMAT_HPP