#pragma once
#ifndef HSBA_SLICER_POINTS_PATH_HPP
#define HSBA_SLICER_POINTS_PATH_HPP

#include <optional>
#include <vector>

#include "IPath.hpp"

namespace HsBa::Slicer
{
	enum class GcodeType
	{
		G0,
		G1,
		G2,
		G3,
		G17 = 17,
		G18,
		G19,
		G20,
		G21,
		G90 = 90,
		G91
	};

	enum class GCodeUnits
	{
		Inch,
		mm
	};

	struct GPoint
	{
		GcodeType type = GcodeType::G1;
		OutPoints3 p1;
		OutPoints3 center; // 圆弧运动的过渡点
		float velocity = 100.0f;
		double extrusion = 0.0;
	};

	class PointsPath : public IPath
	{
	public:
		PointsPath(GCodeUnits units = GCodeUnits::mm,OutPoints3 p = {0.0,0.0,0.0});
		void push_back(const GPoint& point);
		virtual ~PointsPath() = default;
		virtual void Save(const std::filesystem::path&) const override;
		virtual void Save(const std::filesystem::path&, std::string_view script) const override;
		virtual std::string ToString() const override;
		virtual std::string ToString(std::string_view script) const override;
		virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const override;
		virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const override;
		virtual std::string ToString(const std::string_view script, const std::string_view funcName) const override;
		virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName) const override;
		inline virtual GPoint operator[](size_t i)
		{
			return points_[i];
		}
	private:
		std::vector<GPoint> points_;
		OutPoints3 startPoint_;
		GCodeUnits units_ = GCodeUnits::mm;
	};

} // namepace HsBa::Slicer

#endif // !HSBA_SLICER_POINTS_PATH_HPP
