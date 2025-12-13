#include "FullTopoModel.hpp"

#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <climits>
#include <boost/container_hash/hash.hpp>
#include <lua.hpp>

#include <igl/per_face_normals.h>

#include "base/error.hpp"

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
		const auto same_edge = [](const Edge& l, const int index0, const int index1)
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

	bool FullTopoModel::Intersetion(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const float height, Eigen::Vector3f& intersection)
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
		// Collect intersection segments (as integerized 2D points)
		using Key = std::pair<long long, long long>;
		auto make_key = [&](const Eigen::Vector3f& p) -> Key {
			long long xi = std::llround(p.x() * integerization);
			long long yi = std::llround(p.y() * integerization);
			return { xi, yi };
			};

		std::unordered_map<Key, std::vector<Key>, boost::hash<Key>> adj;
		adj.reserve(faces_.size() * 2);

		for (const auto& f : faces_)
		{
			const Eigen::Vector3f& v0 = vertices_[f.triangle[0]].vertex;
			const Eigen::Vector3f& v1 = vertices_[f.triangle[1]].vertex;
			const Eigen::Vector3f& v2 = vertices_[f.triangle[2]].vertex;

			std::vector<Eigen::Vector3f> inters;
			Eigen::Vector3f p;
			if (Intersetion(v0, v1, height, p)) inters.push_back(p);
			if (Intersetion(v1, v2, height, p)) inters.push_back(p);
			if (Intersetion(v2, v0, height, p)) inters.push_back(p);

			// keep unique points (by integerized key)
			std::vector<Key> keys;
			for (const auto& ip : inters)
			{
				Key k = make_key(ip);
				if (keys.end() == std::find(keys.begin(), keys.end(), k))
				{
					keys.push_back(k);
				}
			}
			if (keys.size() == 2)
			{
				adj[keys[0]].push_back(keys[1]);
				adj[keys[1]].push_back(keys[0]);
			}
		}

		// traverse adjacency to build closed loops only
		Polygons result;
		std::unordered_set<Key, boost::hash<Key>> visited;

		for (const auto& kv : adj)
		{
			const Key& start = kv.first;
			if (visited.find(start) != visited.end()) continue;

			// follow path
			std::vector<Key> path;
			Key cur = start;
			Key prev = { LLONG_MIN, LLONG_MIN };
			while (true)
			{
				visited.insert(cur);
				path.push_back(cur);
				auto& neis = adj[cur];
				Key next = prev;
				for (const auto& n : neis)
				{
					if (n == prev) continue;
					next = n; break;
				}
				if (next.first == LLONG_MIN) break; // dead end
				if (next == start)
				{
					// closed loop
					// form polygon
					Polygon poly;
					for (const auto& k : path)
					{
						poly.emplace_back(Point2{ k.first, k.second });
					}
					if (poly.size() >= 3) result.emplace_back(std::move(poly));
					break;
				}
				prev = cur;
				cur = next;
				if (visited.find(cur) != visited.end()) break; // prevent infinite loop
			}
		}

		return result;
	}

	UnSafePolygons FullTopoModel::UnSafeSlice(const float height) const
	{
		using Key = std::pair<long long, long long>;
		auto make_key = [&](const Eigen::Vector3f& p) -> Key {
			long long xi = std::llround(p.x() * integerization);
			long long yi = std::llround(p.y() * integerization);
			return { xi, yi };
			};

		std::unordered_map<Key, std::vector<Key>, boost::hash<Key>> adj;
		adj.reserve(faces_.size() * 2);

		for (const auto& f : faces_)
		{
			const Eigen::Vector3f& v0 = vertices_[f.triangle[0]].vertex;
			const Eigen::Vector3f& v1 = vertices_[f.triangle[1]].vertex;
			const Eigen::Vector3f& v2 = vertices_[f.triangle[2]].vertex;

			std::vector<Eigen::Vector3f> inters;
			Eigen::Vector3f p;
			if (Intersetion(v0, v1, height, p)) inters.push_back(p);
			if (Intersetion(v1, v2, height, p)) inters.push_back(p);
			if (Intersetion(v2, v0, height, p)) inters.push_back(p);

			std::vector<Key> keys;
			for (const auto& ip : inters)
			{
				Key k = make_key(ip);
				if (keys.end() == std::find(keys.begin(), keys.end(), k))
				{
					keys.push_back(k);
				}
			}
			if (keys.size() == 2)
			{
				adj[keys[0]].push_back(keys[1]);
				adj[keys[1]].push_back(keys[0]);
			}
		}

		UnSafePolygons result;
		std::unordered_set<Key, boost::hash<Key>> visited;

		for (const auto& kv : adj)
		{
			const Key& start = kv.first;
			if (visited.find(start) != visited.end()) continue;

			std::vector<Key> path;
			Key cur = start;
			Key prev = { LLONG_MIN, LLONG_MIN };
			bool closed = false;
			while (true)
			{
				visited.insert(cur);
				path.push_back(cur);
				auto& neis = adj[cur];
				Key next = prev;
				for (const auto& n : neis)
				{
					if (n == prev) continue;
					next = n; break;
				}
				if (next.first == LLONG_MIN) break; // dead end
				if (next == start)
				{
					closed = true;
					break;
				}
				prev = cur;
				cur = next;
				if (visited.find(cur) != visited.end()) break;
			}
			Polygon poly;
			for (const auto& k : path)
			{
				poly.emplace_back(Point2{ k.first, k.second });
			}
			if (poly.size() >= 2)
			{
				UnSafePolygon up;
				up.path = std::move(poly);
				up.closed = closed;
				result.emplace_back(std::move(up));
			}
		}

		return result;
	}

	Polygons FullTopoModel::SliceLua(const std::string& script, const float height) const
	{
		lua_State* L = luaL_newstate();
		if (!L) 
			throw RuntimeError("Lua init failed");
		luaL_openlibs(L);

		// push V (1-based)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(vertices_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushnumber(L, vertices_[i].vertex.x()); lua_setfield(L, -2, "x");
			lua_pushnumber(L, vertices_[i].vertex.y()); lua_setfield(L, -2, "y");
			lua_pushnumber(L, vertices_[i].vertex.z()); lua_setfield(L, -2, "z");
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "V");

		// push E (edges)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(edges_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, edges_[i].vertices[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, edges_[i].vertices[1] + 1); lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "E");

		// push F (faces)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, faces_[i].triangle[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, faces_[i].triangle[1] + 1); lua_rawseti(L, -2, 2);
			lua_pushinteger(L, faces_[i].triangle[2] + 1); lua_rawseti(L, -2, 3);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "F");

		lua_pushnumber(L, height);
		lua_setglobal(L, "height");

		int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "FullTopoModelSliceScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua load error: " + err);
		}

		int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua runtime error: " + err);
		}

		// get returned table (top of stack) or global 'polys'
		Polygons result;
		int top = lua_gettop(L);
		if (top == 0 || !lua_istable(L, -1))
		{
			lua_getglobal(L, "polys");
			if (!lua_istable(L, -1))
			{
				lua_close(L);
				return result;
			}
		}

		// Iterate polygons
		int nPolys = (int)lua_rawlen(L, -1);
		for (int i = 1; i <= nPolys; ++i)
		{
			lua_rawgeti(L, -1, i); // push polygon table
			if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
			Polygon poly;
			int nPts = (int)lua_rawlen(L, -1);
			for (int j = 1; j <= nPts; ++j)
			{
				lua_rawgeti(L, -1, j); // push point
				if (lua_istable(L, -1))
				{
					lua_getfield(L, -1, "x");
					lua_getfield(L, -2, "y");
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					lua_pop(L, 2);
					long long xi = std::llround(x * integerization);
					long long yi = std::llround(y * integerization);
					poly.emplace_back(Point2{ xi, yi });
				}
				lua_pop(L, 1); // pop point
			}
			lua_pop(L, 1); // pop polygon
			if (poly.size() >= 3) result.emplace_back(std::move(poly));
		}

		lua_close(L);
		return result;
	}

	UnSafePolygons FullTopoModel::UnSafeSliceLua(const std::string& script, const float height) const
	{
		// reuse SliceLua to get polygons, but also accept open polylines returned via 'polys' where points may be 2 or more
		lua_State* L = luaL_newstate();
		if (!L) 
			throw RuntimeError("Lua init failed");
		luaL_openlibs(L);

		// push V, E, F and height (same as above)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(vertices_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushnumber(L, vertices_[i].vertex.x()); lua_setfield(L, -2, "x");
			lua_pushnumber(L, vertices_[i].vertex.y()); lua_setfield(L, -2, "y");
			lua_pushnumber(L, vertices_[i].vertex.z()); lua_setfield(L, -2, "z");
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "V");

		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(edges_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, edges_[i].vertices[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, edges_[i].vertices[1] + 1); lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "E");

		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, faces_[i].triangle[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, faces_[i].triangle[1] + 1); lua_rawseti(L, -2, 2);
			lua_pushinteger(L, faces_[i].triangle[2] + 1); lua_rawseti(L, -2, 3);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "F");

		lua_pushnumber(L, height);
		lua_setglobal(L, "height");

		int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "FullTopoModelSliceScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua load error: " + err);
		}
		int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua runtime error: " + err);
		}

		UnSafePolygons result;
		int top = lua_gettop(L);
		if (top == 0 || !lua_istable(L, -1))
		{
			lua_getglobal(L, "polys");
			if (!lua_istable(L, -1))
			{
				lua_close(L);
				return result;
			}
		}

		int nPolys = (int)lua_rawlen(L, -1);
		for (int i = 1; i <= nPolys; ++i)
		{
			lua_rawgeti(L, -1, i); // polygon
			if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
			Polygon poly;
			int nPts = (int)lua_rawlen(L, -1);
			for (int j = 1; j <= nPts; ++j)
			{
				lua_rawgeti(L, -1, j);
				if (lua_istable(L, -1))
				{
					lua_getfield(L, -1, "x");
					lua_getfield(L, -2, "y");
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					lua_pop(L, 2);
					long long xi = std::llround(x * integerization);
					long long yi = std::llround(y * integerization);
					poly.emplace_back(Point2{ xi, yi });
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
			if (poly.size() >= 2)
			{
				UnSafePolygon up;
				up.path = std::move(poly);
				up.closed = (up.path.size() >= 3);
				result.emplace_back(std::move(up));
			}
		}

		lua_close(L);
		return result;
	}

	Polygons FullTopoModel::SliceLua(const std::string& script, const std::string& funcName, const float height) const
	{
		lua_State* L = luaL_newstate();
		if (!L) 
			throw RuntimeError("Lua init failed");
		luaL_openlibs(L);
		// push V, E, F and height (same as above)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(vertices_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushnumber(L, vertices_[i].vertex.x()); lua_setfield(L, -2, "x");
			lua_pushnumber(L, vertices_[i].vertex.y()); lua_setfield(L, -2, "y");
			lua_pushnumber(L, vertices_[i].vertex.z()); lua_setfield(L, -2, "z");
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "V");
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(edges_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, edges_[i].vertices[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, edges_[i].vertices[1] + 1); lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "E");
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, faces_[i].triangle[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, faces_[i].triangle[1] + 1); lua_rawseti(L, -2, 2);
			lua_pushinteger(L, faces_[i].triangle[2] + 1); lua_rawseti(L, -2, 3);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "F");
		lua_pushnumber(L, height);
		lua_setglobal(L, "height");
		int loadStatus = luaL_loadbuffer(L, script.data(), script.size(), "FullTopoModelSliceScript");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua load error: " + err);
		}
		int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua runtime error: " + err);
		}
		lua_getglobal(L, funcName.c_str());
		if (!lua_isfunction(L, -1))
		{
			lua_close(L);
			throw RuntimeError("Lua function '" + funcName + "' not found");
		}
		int pcallStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (pcallStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua function '" + funcName + "' runtime error: " + err);
		}
		Polygons result;
		int top = lua_gettop(L);
		if (top == 0 || !lua_istable(L, -1))
		{
			lua_getglobal(L, "polys");
			if (!lua_istable(L, -1))
			{
				lua_close(L);
				return result;
			}
		}
		int nPolys = (int)lua_rawlen(L, -1);
		for (int i = 1; i <= nPolys; ++i)
		{
			lua_rawgeti(L, -1, i); // push polygon table
			if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
			Polygon poly;
			int nPts = (int)lua_rawlen(L, -1);
			for (int j = 1; j <= nPts; ++j)
			{
				lua_rawgeti(L, -1, j); // push point
				if (lua_istable(L, -1))
				{
					lua_getfield(L, -1, "x");
					lua_getfield(L, -2, "y");
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					lua_pop(L, 2);
					long long xi = std::llround(x * integerization);
					long long yi = std::llround(y * integerization);
					poly.emplace_back(Point2{ xi, yi });
				}
				lua_pop(L, 1); // pop point
			}
			lua_pop(L, 1); // pop polygon
			if (poly.size() >= 3) result.emplace_back(std::move(poly));
		}
		lua_close(L);
		return result;
	}

	Polygons FullTopoModel::SliceLua(const std::filesystem::path& script_file, const std::string& funcName, const float height) const
	{
		lua_State* L = luaL_newstate();
		if (!L)
			throw RuntimeError("Lua init failed");
		luaL_openlibs(L);
		// push V, E, F and height (same as above)
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(vertices_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushnumber(L, vertices_[i].vertex.x()); lua_setfield(L, -2, "x");
			lua_pushnumber(L, vertices_[i].vertex.y()); lua_setfield(L, -2, "y");
			lua_pushnumber(L, vertices_[i].vertex.z()); lua_setfield(L, -2, "z");
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "V");
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(edges_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, edges_[i].vertices[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, edges_[i].vertices[1] + 1); lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "E");
		lua_newtable(L);
		for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
		{
			lua_newtable(L);
			lua_pushinteger(L, faces_[i].triangle[0] + 1); lua_rawseti(L, -2, 1);
			lua_pushinteger(L, faces_[i].triangle[1] + 1); lua_rawseti(L, -2, 2);
			lua_pushinteger(L, faces_[i].triangle[2] + 1); lua_rawseti(L, -2, 3);
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "F");
		lua_pushnumber(L, height);
		lua_setglobal(L, "height");
		int loadStatus = luaL_loadfile(L, script_file.string().c_str());
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua load error: " + err);
		}
		int callStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua runtime error: " + err);
		}
		lua_getglobal(L, funcName.c_str());
		if (!lua_isfunction(L, -1))
		{
			lua_close(L);
			throw RuntimeError("Lua function '" + funcName + "' not found");
		}
		int pcallStatus = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (pcallStatus != LUA_OK)
		{
			std::string err = lua_tostring(L, -1);
			lua_close(L);
			throw RuntimeError("Lua function '" + funcName + "' runtime error: " + err);
		}
		Polygons result;
		int top = lua_gettop(L);
		if (top == 0 || !lua_istable(L, -1))
		{
			lua_getglobal(L, "polys");
			if (!lua_istable(L, -1))
			{
				lua_close(L);
				return result;
			}
		}
		int nPolys = (int)lua_rawlen(L, -1);
		for (int i = 1; i <= nPolys; ++i)
		{
			lua_rawgeti(L, -1, i); // push polygon table
			if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
			Polygon poly;
			int nPts = (int)lua_rawlen(L, -1);
			for (int j = 1; j <= nPts; ++j)
			{
				lua_rawgeti(L, -1, j); // push point
				if (lua_istable(L, -1))
				{
					lua_getfield(L, -1, "x");
					lua_getfield(L, -2, "y");
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					lua_pop(L, 2);
					long long xi = std::llround(x * integerization);
					long long yi = std::llround(y * integerization);
					poly.emplace_back(Point2{ xi, yi });
				}
				lua_pop(L, 1); // pop point
			}
			lua_pop(L, 1); // pop polygon
			if (poly.size() >= 3) result.emplace_back(std::move(poly));
		}
		lua_close(L);
		return result;
	}

}// namespace HsBa::Slicer