#pragma once
#ifndef HSBA_FULLTOPOMODEL_HPP
#define HSBA_FULLTOPOMODEL_HPP

#include <vector>
#include <array>
#include <algorithm>

#include <Eigen/Core>

#include "base/IModel.hpp"
#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"

namespace HsBa::Slicer
{
	//可以不封闭的轮廓
	struct UnSafePolygon
	{
		Polygon path;
		bool closed = true;
	};
	//可以不封闭的轮廓集合
	using UnSafePolygons = std::vector<UnSafePolygon>;

	//可以不封闭的轮廓
	struct UnSafePolygonD
	{
		PolygonD path;
		bool closed = true;
	};
	//可以不封闭的轮廓集合
	using UnSafePolygonsD = std::vector<UnSafePolygonD>;

	//只用于切片的完全拓扑重建的网格模型，不提供公开的修改（构造除外）
	//构造函数中会进行拓扑关系的重建
	//实际上可以许可仿射变换，但是没有必要性
	//使用IglModel或CgalModel进行网格处理，使用CADModel的类和方法处理CAD模型
	//这个类可以用于重建拓扑流形，但是重建的拓扑流形不一定是完备的拓扑流形，不提供完善的拓扑流形检查
	//重建的拓扑流形可能存在错误
	class FullTopoModel final
	{
	public:
		struct Face
		{
			std::array<int, 3> triangle{ -1,-1,-1 };
			std::array<int, 3> edges{ -1,-1,-1 };
			Eigen::Vector3f normal;
		};
		struct Vertex
		{
			Eigen::Vector3f vertex;
			std::vector<int> faces;
			std::vector<int> edges;
		};
		struct Edge
		{
			std::array<int, 2> vertices{-1,-1};
			std::array<int, 2> faces{ -1,-1 };
		};

		FullTopoModel(const IModel& model, bool use_normals = false);
		FullTopoModel(IModel&& model, bool use_normals = false) = delete;
		~FullTopoModel() = default;

		//检查拓扑完整性，拓扑不完整的模型一般不是拓扑流形，因此会影响一些算法
		//拓扑不完整的模型可能存在错误
		bool CheckTopo() const;

		inline const std::vector<Vertex>& GetVertices() const { return vertices_; }
		inline const std::vector<Edge>& GetEdges() const { return edges_; }
		inline const std::vector<Face>& GetFaces() const { return faces_; }

		inline const Vertex& GetVertex(int index) const { return vertices_[index]; }
		inline const Edge& GetEdge(int index) const { return edges_[index]; }
		inline const Face& GetFace(int index) const { return faces_[index]; }

		std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const;

		//欧拉示性数，如果不是偶数或者大于2，则可能不是拓扑流形
		//可以用来判断模型的穿洞情况
		//这个函数不检查拓扑完整性
		int EulerCharacteristic() const;

		//线和Z方向平面的交点
		static bool Intersetion(const Eigen::Vector3f& v1,const Eigen::Vector3f& v2,const float height,Eigen::Vector3f& intersection);

		//Z方向切片，实际上常见的切片算法有相同的时间复杂度，除非不计算拓扑重建的时间复杂度
		//因为构造FullTopoModel已经重建拓扑关系，不需要重建拓扑关系
		
		//安全切片，只包含封闭轮廓，不封闭轮廓会被丢弃
		Polygons Slice(const float height) const;
		//不安全切片，包含不封闭轮廓
		UnSafePolygons UnSafeSlice(const float height) const;

		// Run a custom Lua script to produce polygons from vertex/edge/face data.
		// The script receives globals: V (1-based array of {x,y,z}),
		// E (1-based array of {v1,v2}), F (1-based array of {v1,v2,v3}), and 'height'.
		// The script should return a table of polygons: polys = { { {x=..,y=..}, ... }, ... }
		Polygons SliceLua(const std::string& script, const float height) const;

		// Same but returns potentially open polylines with closed flag
		UnSafePolygons UnSafeSliceLua(const std::string& script, const float height) const;


	private:
		std::vector<Vertex> vertices_;
		std::vector<Edge> edges_;
		std::vector<Face> faces_;
	};
}// namespace HsBa::Slicer

#endif // !HSBA_FULLTOPOMODEL_HPP
