#include "eigen_translator.hpp"

#include <vector>
#include <format>
#include <regex>

#include "error.hpp"

namespace HsBa::Slicer::Utils
{
	std::string EigenVector2fTranslator::put_value(const Eigen::Vector2f& v)
	{
		return std::format("( {} , {} )", v.x(), v.y());
	}
	Eigen::Vector2f EigenVector2fTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<float, 2> arr{};
		for (size_t i = 0; i != 2; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stof(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector2f value: {}", str));
			}
		}
		return Eigen::Vector2f{ arr[0],arr[1] };
	}
	std::string EigenVector2iTranslator::put_value(const Eigen::Vector2i& v)
	{
		return std::format("( {} , {} )", v.x(), v.y());
	}
	Eigen::Vector2i EigenVector2iTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]+");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<int, 2> arr{};
		for (size_t i = 0; i != 2; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stoi(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector2f value: {}", str));
			}
		}
		return Eigen::Vector2i{ arr[0],arr[1] };
	}
	std::string EigenVector2dTranslator::put_value(const Eigen::Vector2d& v)
	{
		return std::format("( {} , {} )", v.x(), v.y());
	}
	Eigen::Vector2d EigenVector2dTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<double, 2> arr{};
		for (size_t i = 0; i != 2; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stod(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector2f value: {}", str));
			}
		}
		return Eigen::Vector2d{ arr[0],arr[1] };
	}

	std::string EigenVector3fTranslator::put_value(const Eigen::Vector3f& v)
	{
		return std::format("( {} , {} , {} )", v.x(), v.y(), v.z());
	}
	Eigen::Vector3f EigenVector3fTranslator::get_value(const std::string& str)
	{
	    
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<float, 3> arr{};
		for (size_t i = 0; i != 3; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stof(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector3f value: {}", str));
			}
		}
		return Eigen::Vector3f{ arr[0],arr[1],arr[2] };
	}
	std::string EigenVector3iTranslator::put_value(const Eigen::Vector3i& v)
	{
		return std::format("( {} , {} , {} )", v.x(), v.y(), v.z());
	}
	Eigen::Vector3i EigenVector3iTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]+");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<int, 3> arr{};
		for (size_t i = 0; i != 3; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stoi(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector3f value: {}", str));
			}
		}
		return Eigen::Vector3i{ arr[0],arr[1],arr[2] };
	}
	std::string EigenVector3dTranslator::put_value(const Eigen::Vector3d& v)
	{
			    
		return std::format("( {} , {} , {} )", v.x(), v.y(), v.z());
	}
	Eigen::Vector3d EigenVector3dTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<double, 3> arr{};
		for (size_t i = 0; i != 3; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stod(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector3f value: {}", str));
			}
		}
		return Eigen::Vector3d{ arr[0],arr[1],arr[2] };
	}
	std::string EigenVector4fTranslator::put_value(const Eigen::Vector4f& v)
	{
			    
		return std::format("( {} , {} , {} , {})", v.x(), v.y(), v.z(), v.w());
	}
	Eigen::Vector4f EigenVector4fTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<float, 4> arr{};
		for (size_t i = 0; i != 4; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stof(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector4f value: {}", str));
			}
		}
		return Eigen::Vector4f{ arr[0],arr[1],arr[2],arr[3] };
	}
	std::string EigenVector4iTranslator::put_value(const Eigen::Vector4i& v)
	{
			    
		return std::format("( {} , {} , {} , {})", v.x(), v.y(), v.z(), v.w());
	}
	Eigen::Vector4i EigenVector4iTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]+");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<int, 4> arr{};
		for (size_t i = 0; i != 4; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stoi(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector4f value: {}", str));
			}
		}
		return Eigen::Vector4i{ arr[0],arr[1],arr[2],arr[3] };
	}
	std::string EigenVector4dTranslator::put_value(const Eigen::Vector4d& v)
	{
			    
		return std::format("( {} , {} , {} , {})", v.x(), v.y(), v.z(), v.w());
	}
	Eigen::Vector4d EigenVector4dTranslator::get_value(const std::string& str)
	{
		std::regex regex_pattern("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
		std::sregex_iterator it(str.begin(), str.end(), regex_pattern);
		std::array<double, 4> arr{};
		for (size_t i = 0; i != 4; ++i)
		{
			if (it->size() >= 1)
			{
				arr[i] = std::stod(it->str());
				++it;
			}
			else
			{
				throw RuntimeError(std::format("Invalid Eigen::Vector4f value: {}", str));
			}
		}
		return Eigen::Vector4d{ arr[0],arr[1],arr[2],arr[3] };
	}
}// namespace HsBa::Slicer::Utils