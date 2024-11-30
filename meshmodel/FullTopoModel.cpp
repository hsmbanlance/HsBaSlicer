#include "FullTopoModel.hpp"

#include <set>

#include <igl/per_face_normals.h>

namespace HsBa::Slicer
{
	FullTopoModel::FullTopoModel(const IModel& model, bool use_normals)
	{
		const auto [v, f] = model.TriangleMesh();
		//建立完整的拓扑关系
		//添加点
		for (int i = 0; i != v.rows(); ++i)
		{
			Vertex vertex;
			vertex.vertex = Eigen::Vector3f{ v(i,0),v(i,1),v(i,2) };
			vertices_.emplace_back(vertex);
		}
		const auto same_edge = [](const Edge& l, const int index0,const int index1)
			{
				return (l.vertices[0] == index0 && l.vertices[1] == index1) ||
					(l.vertices[0] == index1 && l.vertices[1] == index0);
			};
		//构造面和边
		for (int i = 0; i != f.rows(); ++i)
		{
			int v0 = f(i, 0);
			int v1 = f(i, 1);
			int v2 = f(i, 2);
			if (v0 <= -1 || v1 <= -1 || v2 <= -1 || v0 >= v.rows() || v1 >= v.rows() || v2 >= v.rows())
			{
				continue;//忽略存在不存在点的面
			}
			Face face;
			face.triangle = { v0,v1,v2 };
			//edge 0 1
			auto it01 = std::find_if(edges_.begin(), edges_.end(), [&face, &v0, &v1, &same_edge](const Edge& e)
				{
					return same_edge(e, v0, v1);
				});
			if (it01 != edges_.end())
			{
				face.edges[0] = static_cast<int>(it01 - edges_.begin());
				if (it01->faces[0] <= -1)
				{
					it01->faces[0] = i;
				}
				else
				{
					it01->faces[1] = i;
				}
			}
			else
			{
				Edge edge;
				edge.vertices = { v0,v1 };
				edge.faces = { i,-1 };
				edges_.emplace_back(edge);
				face.edges[0] = static_cast<int>(edges_.size() - 1);
			}
			//edge 1 2
			auto it12 = std::find_if(edges_.begin(), edges_.end(), [&face, &v1, &v2, &same_edge](const Edge& e)
				{
					return same_edge(e, v1, v2);
				});
			if (it12 != edges_.end())
			{
				face.edges[1] = static_cast<int>(it12 - edges_.begin());
				if (it12->faces[0] < -1)
				{
					it12->faces[0] = i;
				}
				else
				{
					it12->faces[1] = i;
				}
			}
			else
			{
				Edge edge;
				edge.vertices = { v1,v2 };
				edge.faces = { i,-1 };
				edges_.emplace_back(edge);
				face.edges[1] = static_cast<int>(edges_.size() - 1);
			}
			//edge 2 0
			auto it20 = std::find_if(edges_.begin(), edges_.end(), [&face, &v2, &v0, &same_edge](const Edge& e)
				{
					return same_edge(e, v2, v0);
				});
			if (it20 != edges_.end())
			{
				face.edges[2] = static_cast<int>(it20 - edges_.begin());
				if (it20->faces[0] < -1)
				{
					it20->faces[0] = i;
				}
				else
				{
					it20->faces[1] = i;
				}
			}
			else
			{
				Edge edge;
				edge.vertices = { v2,v0 };
				edge.faces = { i,-1 };
				edges_.emplace_back(edge);
				face.edges[2] = static_cast<int>(edges_.size() - 1);
			}
			faces_.emplace_back(face);
			//添加点与面的关系
			for (int i = 0; i != f.rows(); ++i)
			{

				vertices_[v0].faces.push_back(i);
				vertices_[v1].faces.push_back(i);
				vertices_[v2].faces.push_back(i);
			}
		}
		if (use_normals)
		{
			Eigen::MatrixXf normals;
			igl::per_face_normals(v, f, normals);
			for (int i = 0; i != normals.rows(); ++i)
			{
				if (i < faces_.size())
				{
					faces_[i].normal = Eigen::Vector3f{ normals.row(i).x(),normals.row(i).y(),normals.row(i).z() };
				}
			}
		}
	}
	bool FullTopoModel::CheckTopo() const
	{
		size_t vsize = vertices_.size();
		size_t esize = edges_.size();
		size_t fsize = faces_.size();
		const auto check_faces = [this, vsize, esize, fsize]() { //所有面的点和边被定义
			std::set<int> vertex_set;
			std::set<int> edge_set;
			for (const auto& f : faces_)
			{
				int v0 = f.triangle[0];
				int v1 = f.triangle[1];
				int v2 = f.triangle[2];
				if (v0 <= -1 || v1 <= -1 || v2 <= -1 || v0 >= vsize || v1 >= vsize || v2 >= vsize)
				{
					return false;
				}
				int e0 = f.edges[0];
				int e1 = f.edges[1];
				int e2 = f.edges[2];
				if (e0 <= -1 || e1 <= -1 || e2 <= -1 || e0 >= esize || e1 >= esize || e2 >= esize)
				{
					return false;
				}
				vertex_set.insert(v0);
				vertex_set.insert(v1);
				vertex_set.insert(v2);
				edge_set.insert(e0);
				edge_set.insert(e1);
				edge_set.insert(e2);
			}
			return vertex_set.size() == vsize && edge_set.size() == esize;
			};
		const auto check_edges = [this, vsize, esize, fsize]() {//所有边的点和面被定义
			std::set<int> vertex_set;
			std::set<int> face_set;
			for (const auto& e : edges_)
			{
				int v0 = e.vertices[0];
				int v1 = e.vertices[1];
				if (v0 <= -1 || v1 <= -1 || v0 >= vsize || v1 >= vsize)
				{
					return false;
				}
				int f0 = e.faces[0];
				int f1 = e.faces[1];
				if (f0 <= -1 || f1 <= -1 || f0 >= fsize || f1 >= fsize)
				{
					return false;
				}
				vertex_set.insert(v0);
				vertex_set.insert(v1);
				face_set.insert(f0);
				face_set.insert(f1);
			}
			return vertex_set.size() == vsize && face_set.size() == fsize;
			};
		return check_faces() && check_edges();
	}

	std::pair<Eigen::MatrixXf, Eigen::MatrixXi> FullTopoModel::TriangleMesh() const
	{
		Eigen::MatrixXf v;
		v.resize(vertices_.size(), 3);
		for (size_t i = 0; i != vertices_.size(); ++i)
		{
			v(i, 0) = vertices_[i].vertex.x();
			v(i, 1) = vertices_[i].vertex.y();
			v(i, 2) = vertices_[i].vertex.z();
		}
		Eigen::MatrixXi f;
		for (size_t i = 0; i != faces_.size(); ++i)
		{
			f(i, 0) = faces_[i].triangle[0];
			f(i, 1) = faces_[i].triangle[1];
			f(i, 2) = faces_[i].triangle[2];
		}
		return std::make_pair(v, f);
	}

	int FullTopoModel::EulerCharacteristic() const
	{
		return static_cast<int>(vertices_.size()) - static_cast<int>(edges_.size()) + static_cast<int>(faces_.size());
	}

	bool FullTopoModel::Intersetion(const Eigen::Vector3f& v1,const Eigen::Vector3f& v2,const float height,Eigen::Vector3f& intersection)
	{
		//不相交的情况
		if (v1.z() > height && v2.z() > height)
		{
			return false;
		}
		if (v1.z() < height && v2.z() < height)
		{
			return false;
		}
		//平行返回其中之一
		if (v1.z() == height && v2.z() == height)
		{
			if (v1.x() == v2.x() && v1.y() == v2.y())
			{
				intersection = v1;
				return true;
			}
			intersection = v1;
			return false;
		}
		float t = (height - v1.z()) / (v2.z() - v1.z());
		intersection = v1 + t * (v2 - v1);
		return true;
	}

	Polygons FullTopoModel::Slice(const float height) const
	{
		Polygons res;
		//Todo:拓扑重建的切片算法
		size_t slow_face = 0, fast_face = 0;
		std::set<size_t> face_set;
		while (face_set.size() != faces_.size())
		{

		}
		return res;
	}

}// namespace HsBa::Slicer