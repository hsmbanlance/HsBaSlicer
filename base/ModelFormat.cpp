/** @file ModelFormat.cpp
 * @brief Implementations of the model format extension/enum helpers.
 * @author HsBa
 */
#include "ModelFormat.hpp"

#include <filesystem>
#include <regex>

namespace HsBa::Slicer
{
namespace
{
class ExtRegexMatch
{
    // mesh
    std::regex stl_match{"\\.?stl", std::regex_constants::icase};
    std::regex ply_match{"\\.?ply", std::regex_constants::icase};
    std::regex obj_match{"\\.?obj", std::regex_constants::icase};
    std::regex off_match{"\\.?off", std::regex_constants::icase};

    bool MatchSTL(const std::string& ext) { return std::regex_match(ext, stl_match); }
    bool MatchPLY(const std::string& ext) { return std::regex_match(ext, ply_match); }
    bool MatchOBJ(const std::string& ext) { return std::regex_match(ext, obj_match); }
    bool MatchOFF(const std::string& ext) { return std::regex_match(ext, off_match); }
    // BRep
    std::regex step_match{"\\.?ste?p", std::regex_constants::icase};
    std::regex vrml_match{"\\.?(vrml|wrl)", std::regex_constants::icase};
    std::regex iges_match{"\\.?iges", std::regex_constants::icase};
    std::regex brep_match{"\\.?brep", std::regex_constants::icase};
    bool MatchSTEP(const std::string& ext) { return std::regex_match(ext, step_match); }
    bool MatchVRML(const std::string& ext) { return std::regex_match(ext, vrml_match); }
    bool MatchIGES(const std::string& ext) { return std::regex_match(ext, iges_match); }
    bool MatchBRep(const std::string& ext) { return std::regex_match(ext, brep_match); }
    // csg
    std::regex sldprt_match{"\\.?sldprt", std::regex_constants::icase};
    std::regex catpart_match{"\\.?catpart", std::regex_constants::icase};
    bool MatchSLDPRT(const std::string& ext) { return std::regex_match(ext, sldprt_match); }
    bool MatchCATPART(const std::string& ext) { return std::regex_match(ext, catpart_match); }

    // point cloud
    std::regex xyz_match{"\\.?xyz", std::regex_constants::icase};
    bool MatchXYZ(const std::string& ext) { return std::regex_match(ext, xyz_match); }

public:
    ModelFormat GetFormat(const std::string& ext)
    {
        if (MatchSTL(ext))
        {
            return ModelFormat::UnknownSTL;
        }
        if (MatchPLY(ext))
        {
            return ModelFormat::UnknownPLY;
        }
        if (MatchOBJ(ext))
        {
            return ModelFormat::OBJ;
        }
        if (MatchOFF(ext))
        {
            return ModelFormat::OFF;
        }
        if (MatchSTEP(ext))
        {
            return ModelFormat::STEP;
        }
        if (MatchVRML(ext))
        {
            return ModelFormat::VRML;
        }
        if (MatchIGES(ext))
        {
            return ModelFormat::IGES;
        }
        if (MatchBRep(ext))
        {
            return ModelFormat::BREP;
        }
        if (MatchXYZ(ext))
        {
            return ModelFormat::XYZ;
        }
        return ModelFormat::Unknown;
    }
};
}  // namespace

std::string GetExtName(const std::string& file_name)
{
    std::filesystem::path const fp{file_name};
    return fp.extension().string();
}

ModelFormat ModelTypeFromExtName(const std::string& file_name)
{
    ExtRegexMatch match;
    return match.GetFormat(GetExtName(file_name));
}

bool IsMeshFormat(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::UnknownPLY:
    case ModelFormat::BinaryPLY:
    case ModelFormat::ASCIIPLY:
    case ModelFormat::OBJ:
    case ModelFormat::UnknownSTL:
    case ModelFormat::BinarySTL:
    case ModelFormat::ASCIISTL:
    case ModelFormat::OFF:
        return true;
    default:
        return false;
    }
}

bool IsBrepFormat(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::VRML:
    case ModelFormat::STEP:
    case ModelFormat::IGES:
    case ModelFormat::BREP:
        return true;
    default:
        return false;
    }
}

bool IsCSGFormat(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::SLDPRT:
    case ModelFormat::CATPART:
        return true;
    default:
        return false;
    }
}

bool IsPointCloudFormat(ModelFormat format)
{
    switch (format)
    {
    case ModelFormat::XYZ:
    case ModelFormat::VDB:
        return true;
    default:
        return false;
    }
}
bool IsMeshFormat(const std::string& file_name)
{
    auto type = ModelTypeFromExtName(file_name);
    return IsMeshFormat(type);
}
bool IsBrepFormat(const std::string& file_name)
{
    auto type = ModelTypeFromExtName(file_name);
    return IsBrepFormat(type);
}
bool IsPointCloudFormat(const std::string& file_name)
{
    auto type = ModelTypeFromExtName(file_name);
    return IsPointCloudFormat(type);
}
bool IsCSGFormat(const std::string& file_name)
{
    auto type = ModelTypeFromExtName(file_name);
    return IsCSGFormat(type);
}
}  // namespace HsBa::Slicer