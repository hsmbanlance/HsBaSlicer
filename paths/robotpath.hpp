#pragma once
#ifndef HSBA_SLICER_ROBOT_PATH_HPP
#define HSBA_SLICER_ROBOT_PATH_HPP

#include <vector>

#include "IPath.hpp"

namespace HsBa::Slicer
{
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
		Undefine = 255,
	};

	struct RLPoint
	{
		OutPoints3 end;
		OutPoints3 middle;
		float velocity = 100.0f;
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
			virtual void Save(const std::filesystem::path&) override;
			virtual void Save(const std::filesystem::path&, std::string_view script) override;
			virtual std::string ToString() override;
			virtual std::string ToString(std::string_view script) override;
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
			std::string GenerateAbbCode();
			std::string GenerateKukaCode();
			std::string GenerateFanucCode();
	};
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_ROBOT_PATH_HPP
