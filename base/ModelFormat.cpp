#include "ModelFormat.hpp"

#include <filesystem>
#include <regex>

namespace HsBa::Slicer
{
    class ExtRegexMatch 
    {
        //mesh
        const std::regex stl_match{ "\\.?stl",std::regex_constants::icase };
        const std::regex ply_match{ "\\.?ply",std::regex_constants::icase };
        const std::regex obj_match{ "\\.?obj",std::regex_constants::icase };
        const std::regex off_match{ "\\.?off",std::regex_constants::icase };

        bool MatchSTL(const std::string& ext) 
        {
            return std::regex_match(ext, stl_match);
        }
        bool MatchPLY(const std::string& ext) 
        {
            return std::regex_match(ext, ply_match);
        }
        bool MatchOBJ(const std::string& ext) 
        {
            return std::regex_match(ext, obj_match);
        }
        bool MatchOFF(const std::string& ext) 
        {
            return std::regex_match(ext, off_match);
        }
        //BRep
        const std::regex step_match{ "\\.?ste?p",std::regex_constants::icase };
        const std::regex vrml_match{ "\\.?vrml",std::regex_constants::icase };
        const std::regex iges_match{ "\\.?iges",std::regex_constants::icase };
        bool MatchSTEP(const std::string& ext) 
        {
            return std::regex_match(ext, step_match);
        }
        bool MatchVRML(const std::string& ext) 
        {
            return std::regex_match(ext, vrml_match);
        }
        bool MatchIGES(const std::string& ext) 
        {
            return std::regex_match(ext, iges_match);
        }
        //csg

        //point cloud
        const std::regex xyz_match{ "\\.?xyz",std::regex_constants::icase };
        bool MatchXYZ(const std::string& ext) 
        {
            return std::regex_match(ext, xyz_match);
        }
    public:
        ModelFormat GetFormat(const std::string& ext)
        {
            if (MatchSTL(ext))
            {
                return ModelFormat::UnknownSTL;
            }
            else if (MatchPLY(ext))
            {
                return ModelFormat::UnknownPLY;
            }
            else if (MatchOBJ(ext))
            {
                return ModelFormat::OBJ;
            }
            else if (MatchOFF(ext))
            {
                return ModelFormat::OFF;
            }
            else if (MatchSTEP(ext))
            {
                return ModelFormat::STEP;
            }
            else if (MatchVRML(ext))
            {
                    return ModelFormat::VRML;
            }
            else if (MatchIGES(ext))
            {
                return ModelFormat::IGES;
            }
            else if (MatchXYZ(ext))
            {
                return ModelFormat::XYZ;
            }
            else
            {
                return ModelFormat::Unknown;
            }
        }
    };

    std::string GetExtName(const std::string& file_name)
    {
        std::filesystem::path fp{ file_name };
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
}