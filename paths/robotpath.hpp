#pragma once
#ifndef HSBA_SLICER_ROBOT_PATH_HPP
#define HSBA_SLICER_ROBOT_PATH_HPP

#include <vector>

#include "IPath.hpp"

namespace HsBa::Slicer
{
    // Robot path constants
    constexpr int ROBOT_UNDEFINED_TYPE = 255;  // Used for undefined robot type
    constexpr float DEFAULT_ROBOT_VELOCITY = 100.0f;  // Default velocity for robot movements
	enum class RLPointType
	{
		MoveJ,
		MoveL,
		MoveC,
		ProgramLStart,
		ProgramStart,
		ProgramCStart,
		ProgramL,
		ProgramC,
		ProgramLEnd,
		ProgramCEnd,
	};

	enum class RLType
	{
		Unknown = -1,
		Abb,
		Kuka,
		Fanuc,
		Undefine = ROBOT_UNDEFINED_TYPE,
	};

	struct RLPoint
	{
		OutPoints3 end;
		OutPoints3 middle;
		float velocity = DEFAULT_ROBOT_VELOCITY;
		RLPointType type = RLPointType::MoveL;
		size_t programIndex = 0;
	};

	class RobotPath : public IPath
	{
		public:
			RobotPath(RLType robotType = RLType::Unknown, OutPoints3 startPoint = {}, std::string startProgramFunc = "", std::string endProgramFunc = "");
			void push_back(const RLPoint& point);
			virtual ~RobotPath() = default;
			RLType getRobotType() const;
			virtual void Save(const std::filesystem::path&) const override;
			virtual void Save(const std::filesystem::path&, std::string_view script,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			virtual std::string ToString() const override;
			virtual std::string ToString(std::string_view script,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			virtual std::string ToString(const std::string_view script, const std::string_view funcName,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
				const std::function<void(lua_State*)>& lua_reg = {}) const override;
			inline virtual RLPoint operator[](size_t i)
			{
				return points_[i];
			}
		private:
			RLType robotType_;
			OutPoints3 startPoint_;
			std::vector<RLPoint> points_;
			std::string startProgramFunc_;
			std::string endProgramFunc_;
			std::string GenerateAbbCode() const;
			std::string GenerateKukaCode() const;
			std::string GenerateFanucCode() const;
	};
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_ROBOT_PATH_HPP
