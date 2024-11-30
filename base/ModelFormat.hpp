#pragma once
#ifndef HSBA_SLICER_MODELFORMAT_HPP
#define HSBA_SLICER_MODELFORMAT_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{
    enum class ModelFormat
    {
        //mesh
        UnknownPLY,
        ASCIIPLY,
        BinaryPLY,
        OBJ,
        UnknownSTL,
        BinarySTL,
        ASCIISTL,
        OFF,
        //Brep
        VRML,
        STEP,
        IGES,
        //csg
        
        //point cloud
        XYZ,
        //Unknown
        Unknown
    };

    //convert to string
    inline const char* ToString(ModelFormat format)
    {
        switch (format)
        {
        case ModelFormat::UnknownPLY:return "PLY";
        case ModelFormat::BinaryPLY: return "BinaryPLY";
        case ModelFormat::ASCIIPLY:return "ASCIIPLY";
        case ModelFormat::OBJ: return "OBJ";
        case ModelFormat::UnknownSTL: return "STL";
        case ModelFormat::BinarySTL: return "BinarySTL";
        case ModelFormat::ASCIISTL: return "ASCIISTL";
        case ModelFormat::OFF: return "OFF";
        case ModelFormat::VRML: return "VRML";
        case ModelFormat::STEP: return "STEP";
        case ModelFormat::IGES: return "IGES";
        case ModelFormat::XYZ: return "XYZ";
        default: return "Unknown";
        }
    }

    std::string GetExtName(const std::string& file_name);

    ModelFormat ModelTypeFromExtName(const std::string& file_name);

    bool IsMeshFormat(ModelFormat format);
    bool IsBrepFormat(ModelFormat format);
    bool IsPointCloudFormat(ModelFormat format);

    bool IsMeshFormat(const std::string& file_name);
    bool IsBrepFormat(const std::string& file_name);
    bool IsPointCloudFormat(const std::string& file_name);
}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_MODELFORMAT_HPP