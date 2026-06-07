#include "FullTopoModel.hpp"

#include <boost/container_hash/hash.hpp>
#include <climits>
#include <cmath>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "2D/LuaAdapter.hpp"
#include <igl/per_face_normals.h>
#include <lua.hpp>

#include "base/ModelFormat.hpp"
#include "base/error.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
FullTopoModel::FullTopoModel(const std::vector<Eigen::Vector3f>& vertices,
                             const std::vector<std::array<int, 3>>& triangles, bool use_normals)
{
    BuildTopo(vertices, triangles, use_normals);
}
FullTopoModel::FullTopoModel(const IModel& model, bool use_normals)
{
    const auto [v, f] = model.TriangleMesh();
    std::vector<Eigen::Vector3f> vertices;
    vertices.reserve(v.rows());
    for (int i = 0; i < v.rows(); ++i)
    {
        vertices.emplace_back(v(i, 0), v(i, 1), v(i, 2));
    }
    std::vector<std::array<int, 3>> triangles;
    triangles.reserve(f.rows());
    for (int i = 0; i < f.rows(); ++i)
    {
        triangles.push_back({{f(i, 0), f(i, 1), f(i, 2)}});
    }
    BuildTopo(vertices, triangles, use_normals);
}

void FullTopoModel::BuildTopo(const std::vector<Eigen::Vector3f>& vertices,
                              const std::vector<std::array<int, 3>>& triangles, bool use_normals)
{
    vertices_.clear();
    edges_.clear();
    faces_.clear();

    vertices_.reserve(vertices.size());
    for (const auto& vertex_position : vertices)
    {
        vertices_.push_back(Vertex{vertex_position, {}, {}});
    }

    std::unordered_map<std::pair<int, int>, int, boost::hash<std::pair<int, int>>> edge_map;
    edge_map.reserve(triangles.size() * 2);

    auto make_edge_key = [](int index0, int index1) -> std::pair<int, int>
    { return index0 < index1 ? std::pair<int, int>{index0, index1} : std::pair<int, int>{index1, index0}; };

    for (int face_index = 0; face_index < static_cast<int>(triangles.size()); ++face_index)
    {
        const auto& triangle = triangles[face_index];
        int v0 = triangle[0];
        int v1 = triangle[1];
        int v2 = triangle[2];
        if (v0 < 0 || v1 < 0 || v2 < 0 || v0 >= static_cast<int>(vertices_.size()) ||
            v1 >= static_cast<int>(vertices_.size()) || v2 >= static_cast<int>(vertices_.size()))
        {
            continue;
        }

        Face face;
        face.triangle = {v0, v1, v2};
        face.edges = {-1, -1, -1};

        const std::array<std::pair<int, int>, 3> edge_vertices{{{v0, v1}, {v1, v2}, {v2, v0}}};
        for (int edge_slot = 0; edge_slot < 3; ++edge_slot)
        {
            const auto& [a, b] = edge_vertices[edge_slot];
            auto key = make_edge_key(a, b);
            auto it = edge_map.find(key);
            if (it != edge_map.end())
            {
                int edge_index = it->second;
                face.edges[edge_slot] = edge_index;
                auto& edge = edges_[edge_index];
                if (edge.faces[0] == -1)
                {
                    edge.faces[0] = static_cast<int>(faces_.size());
                }
                else if (edge.faces[1] == -1)
                {
                    edge.faces[1] = static_cast<int>(faces_.size());
                }
            }
            else
            {
                Edge edge;
                edge.vertices = {a, b};
                edge.faces = {static_cast<int>(faces_.size()), -1};
                edges_.push_back(edge);
                int edge_index = static_cast<int>(edges_.size() - 1);
                edge_map.emplace(key, edge_index);
                face.edges[edge_slot] = edge_index;
            }

            auto& vertex_edges = vertices_[edge_vertices[edge_slot].first].edges;
            if (std::find(vertex_edges.begin(), vertex_edges.end(), face.edges[edge_slot]) == vertex_edges.end())
            {
                vertex_edges.push_back(face.edges[edge_slot]);
            }
            auto& vertex_edges2 = vertices_[edge_vertices[edge_slot].second].edges;
            if (std::find(vertex_edges2.begin(), vertex_edges2.end(), face.edges[edge_slot]) == vertex_edges2.end())
            {
                vertex_edges2.push_back(face.edges[edge_slot]);
            }
        }

        faces_.push_back(face);
        int inserted_face_index = static_cast<int>(faces_.size() - 1);
        vertices_[v0].faces.push_back(inserted_face_index);
        vertices_[v1].faces.push_back(inserted_face_index);
        vertices_[v2].faces.push_back(inserted_face_index);
    }

    if (use_normals)
    {
        Eigen::MatrixXf normals;
        Eigen::MatrixXf v_matrix(static_cast<int>(vertices_.size()), 3);
        Eigen::MatrixXi f_matrix(static_cast<int>(faces_.size()), 3);
        for (int i = 0; i < static_cast<int>(vertices_.size()); ++i)
        {
            v_matrix(i, 0) = vertices_[i].vertex.x();
            v_matrix(i, 1) = vertices_[i].vertex.y();
            v_matrix(i, 2) = vertices_[i].vertex.z();
        }
        for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
        {
            f_matrix(i, 0) = faces_[i].triangle[0];
            f_matrix(i, 1) = faces_[i].triangle[1];
            f_matrix(i, 2) = faces_[i].triangle[2];
        }
        igl::per_face_normals(v_matrix, f_matrix, normals);
        for (int i = 0; i < static_cast<int>(normals.rows()) && i < static_cast<int>(faces_.size()); ++i)
        {
            faces_[i].normal = Eigen::Vector3f{normals.row(i).x(), normals.row(i).y(), normals.row(i).z()};
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
    v.resize(static_cast<int>(vertices_.size()), 3);
    for (size_t i = 0; i != vertices_.size(); ++i)
    {
        v(static_cast<int>(i), 0) = vertices_[i].vertex.x();
        v(static_cast<int>(i), 1) = vertices_[i].vertex.y();
        v(static_cast<int>(i), 2) = vertices_[i].vertex.z();
    }
    Eigen::MatrixXi f;
    f.resize(static_cast<int>(faces_.size()), 3);
    for (size_t i = 0; i != faces_.size(); ++i)
    {
        f(static_cast<int>(i), 0) = faces_[i].triangle[0];
        f(static_cast<int>(i), 1) = faces_[i].triangle[1];
        f(static_cast<int>(i), 2) = faces_[i].triangle[2];
    }
    return std::make_pair(v, f);
}

int FullTopoModel::EulerCharacteristic() const
{
    return static_cast<int>(vertices_.size()) - static_cast<int>(edges_.size()) + static_cast<int>(faces_.size());
}

bool FullTopoModel::Intersection(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const float height,
                                 Eigen::Vector3f& intersection)
{
    // 不相交的情况
    if (v1.z() > height && v2.z() > height)
    {
        return false;
    }
    if (v1.z() < height && v2.z() < height)
    {
        return false;
    }
    // 平行返回其中之一
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

void PushFullTopoModelToLua(lua_State* L, const FullTopoModel& model, float height)
{
    // push V (1-based)
    lua_newtable(L);
    const auto& verts = model.GetVertices();
    for (int i = 0; i < static_cast<int>(verts.size()); ++i)
    {
        lua_newtable(L);
        lua_pushnumber(L, verts[i].vertex.x());
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, verts[i].vertex.y());
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, verts[i].vertex.z());
        lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, i + 1);
    }
    lua_setglobal(L, "V");

    // push E (edges)
    lua_newtable(L);
    const auto& edges = model.GetEdges();
    for (int i = 0; i < static_cast<int>(edges.size()); ++i)
    {
        lua_newtable(L);
        lua_pushinteger(L, edges[i].vertices[0] + 1);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, edges[i].vertices[1] + 1);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setglobal(L, "E");

    // push F (faces)
    lua_newtable(L);
    const auto& faces = model.GetFaces();
    for (int i = 0; i < static_cast<int>(faces.size()); ++i)
    {
        lua_newtable(L);
        lua_pushinteger(L, faces[i].triangle[0] + 1);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, faces[i].triangle[1] + 1);
        lua_rawseti(L, -2, 2);
        lua_pushinteger(L, faces[i].triangle[2] + 1);
        lua_rawseti(L, -2, 3);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setglobal(L, "F");

    lua_pushnumber(L, height);
    lua_setglobal(L, "height");
}

Polygons FullTopoModel::Slice(const float height) const
{
    // Collect intersection segments (as integerized 2D points)
    using Key = std::pair<long long, long long>;
    auto make_key = [&](const Eigen::Vector3f& p) -> Key
    {
        long long xi = std::llround(p.x() * integerization);
        long long yi = std::llround(p.y() * integerization);
        return {xi, yi};
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
        if (Intersection(v0, v1, height, p))
            inters.push_back(p);
        if (Intersection(v1, v2, height, p))
            inters.push_back(p);
        if (Intersection(v2, v0, height, p))
            inters.push_back(p);

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
        if (visited.find(start) != visited.end())
            continue;

        // follow path
        std::vector<Key> path;
        Key cur = start;
        Key prev = {LLONG_MIN, LLONG_MIN};
        while (true)
        {
            visited.insert(cur);
            path.push_back(cur);
            auto& neis = adj[cur];
            Key next = prev;
            for (const auto& n : neis)
            {
                if (n == prev)
                    continue;
                next = n;
                break;
            }
            if (next.first == LLONG_MIN)
                break;  // dead end
            if (next == start)
            {
                // closed loop
                // form polygon
                Polygon poly;
                for (const auto& k : path)
                {
                    poly.emplace_back(Point2{k.first, k.second});
                }
                if (poly.size() >= 3)
                    result.emplace_back(std::move(poly));
                break;
            }
            prev = cur;
            cur = next;
            if (visited.find(cur) != visited.end())
                break;  // prevent infinite loop
        }
    }

    return result;
}

UnSafePolygons FullTopoModel::UnSafeSlice(const float height) const
{
    using Key = std::pair<long long, long long>;
    auto make_key = [&](const Eigen::Vector3f& p) -> Key
    {
        long long xi = std::llround(p.x() * integerization);
        long long yi = std::llround(p.y() * integerization);
        return {xi, yi};
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
        if (Intersection(v0, v1, height, p))
            inters.push_back(p);
        if (Intersection(v1, v2, height, p))
            inters.push_back(p);
        if (Intersection(v2, v0, height, p))
            inters.push_back(p);

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
        if (visited.find(start) != visited.end())
            continue;

        std::vector<Key> path;
        Key cur = start;
        Key prev = {LLONG_MIN, LLONG_MIN};
        bool closed = false;
        while (true)
        {
            visited.insert(cur);
            path.push_back(cur);
            auto& neis = adj[cur];
            Key next = prev;
            for (const auto& n : neis)
            {
                if (n == prev)
                    continue;
                next = n;
                break;
            }
            if (next.first == LLONG_MIN)
                break;  // dead end
            if (next == start)
            {
                closed = true;
                break;
            }
            prev = cur;
            cur = next;
            if (visited.find(cur) != visited.end())
                break;
        }
        Polygon poly;
        for (const auto& k : path)
        {
            poly.emplace_back(Point2{k.first, k.second});
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

Polygons FullTopoModel::SliceFast(const float height) const
{
    if (!CheckTopo())
    {
        throw RuntimeError("FullTopoModel topology invalid");
    }

    using Key = std::pair<long long, long long>;
    auto make_key = [&](const Eigen::Vector3f& p) -> Key
    {
        long long xi = std::llround(p.x() * integerization);
        long long yi = std::llround(p.y() * integerization);
        return {xi, yi};
    };

    std::vector<bool> edge_intersects(edges_.size(), false);
    std::vector<Key> edge_keys(edges_.size());
    std::vector<Eigen::Vector3f> edge_positions(edges_.size());

    for (int edge_index = 0; edge_index < static_cast<int>(edges_.size()); ++edge_index)
    {
        const auto& edge = edges_[edge_index];
        const Eigen::Vector3f& v0 = vertices_[edge.vertices[0]].vertex;
        const Eigen::Vector3f& v1 = vertices_[edge.vertices[1]].vertex;
        Eigen::Vector3f p;
        if (Intersection(v0, v1, height, p))
        {
            edge_intersects[edge_index] = true;
            edge_positions[edge_index] = p;
            edge_keys[edge_index] = make_key(p);
        }
    }

    std::unordered_map<Key, std::vector<Key>, boost::hash<Key>> adj;
    adj.reserve(faces_.size() * 2);

    for (const auto& face : faces_)
    {
        std::vector<int> intersected_edges;
        for (int edge_slot = 0; edge_slot < 3; ++edge_slot)
        {
            int edge_index = face.edges[edge_slot];
            if (edge_index >= 0 && edge_index < static_cast<int>(edge_intersects.size()) && edge_intersects[edge_index])
            {
                intersected_edges.push_back(edge_index);
            }
        }

        if (intersected_edges.size() == 2)
        {
            const Key& a = edge_keys[intersected_edges[0]];
            const Key& b = edge_keys[intersected_edges[1]];
            if (a != b)
            {
                adj[a].push_back(b);
                adj[b].push_back(a);
            }
        }
    }

    Polygons result;
    std::unordered_set<Key, boost::hash<Key>> visited;
    for (const auto& kv : adj)
    {
        const Key& start = kv.first;
        if (visited.find(start) != visited.end())
            continue;

        std::vector<Key> path;
        Key cur = start;
        Key prev = {LLONG_MIN, LLONG_MIN};
        while (true)
        {
            visited.insert(cur);
            path.push_back(cur);
            const auto& neis = adj[cur];
            Key next = prev;
            for (const auto& n : neis)
            {
                if (n == prev)
                    continue;
                next = n;
                break;
            }
            if (next.first == LLONG_MIN)
                break;
            if (next == start)
            {
                Polygon poly;
                for (const auto& k : path)
                    poly.emplace_back(Point2{k.first, k.second});
                if (poly.size() >= 3)
                    result.emplace_back(std::move(poly));
                break;
            }
            prev = cur;
            cur = next;
            if (visited.find(cur) != visited.end())
                break;
        }
    }

    return result;
}

Polygons FullTopoModel::SliceLua(const std::string& script, const float height) const
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Lua init failed");
    luaL_openlibs(L.get());

    // push model data to Lua
    PushFullTopoModelToLua(L.get(), *this, height);

    int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "FullTopoModelSliceScript");
    if (loadStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua load error: " + err);
    }
    int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (callStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua runtime error: " + err);
    }

    // get returned table (top of stack) or global 'polys'
    Polygons result;
    int top = lua_gettop(L.get());
    if (top == 0 || !lua_istable(L.get(), -1))
    {
        lua_getglobal(L.get(), "polys");
        if (!lua_istable(L.get(), -1))
        {
            return result;
        }
    }

    // convert using LuaAdapter helper
    result = LuaTableToPolygons(L.get(), -1);

    return result;
}

UnSafePolygons FullTopoModel::UnSafeSliceLua(const std::string& script, const float height) const
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Lua init failed");
    luaL_openlibs(L.get());

    // push model data to Lua
    PushFullTopoModelToLua(L.get(), *this, height);

    int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "FullTopoModelSliceScript");
    if (loadStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua load error: " + err);
    }
    int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (callStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua runtime error: " + err);
    }

    UnSafePolygons result;
    int top = lua_gettop(L.get());
    if (top == 0 || !lua_istable(L.get(), -1))
    {
        lua_getglobal(L.get(), "polys");
        if (!lua_istable(L.get(), -1))
        {
            return result;
        }
    }

    // convert using helper and accept open polylines
    Polygons polys = LuaTableToPolygons(L.get(), -1);
    for (auto& p : polys)
    {
        if (p.size() >= 2)
        {
            UnSafePolygon up;
            up.path = std::move(p);
            up.closed = (up.path.size() >= 3);
            result.emplace_back(std::move(up));
        }
    }

    return result;
}

Polygons FullTopoModel::SliceLua(const std::string& script, const std::string& funcName, const float height) const
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Lua init failed");
    luaL_openlibs(L.get());
    // push model data to Lua
    PushFullTopoModelToLua(L.get(), *this, height);
    int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "FullTopoModelSliceScript");
    if (loadStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua load error: " + err);
    }
    int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (callStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua runtime error: " + err);
    }
    lua_getglobal(L.get(), funcName.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua function '" + funcName + "' not found");
    }
    int pcallStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (pcallStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua function '" + funcName + "' runtime error: " + err);
    }
    Polygons result;
    int top = lua_gettop(L.get());
    if (top == 0 || !lua_istable(L.get(), -1))
    {
        lua_getglobal(L.get(), "polys");
        if (!lua_istable(L.get(), -1))
        {
            return result;
        }
    }
    // convert using LuaAdapter helper
    result = LuaTableToPolygons(L.get(), -1);
    // L will be closed by UniqueLua deleter
    return result;
}

Polygons FullTopoModel::SliceLua(const std::filesystem::path& script_file, const std::string& funcName,
                                 const float height) const
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Lua init failed");
    luaL_openlibs(L.get());
    // push model data to Lua
    PushFullTopoModelToLua(L.get(), *this, height);
    int loadStatus = luaL_loadfile(L.get(), script_file.string().c_str());
    if (loadStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua load error: " + err);
    }
    int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (callStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua runtime error: " + err);
    }
    lua_getglobal(L.get(), funcName.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua function '" + funcName + "' not found");
    }
    int pcallStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (pcallStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("Lua function '" + funcName + "' runtime error: " + err);
    }
    Polygons result;
    int top = lua_gettop(L.get());
    if (top == 0 || !lua_istable(L.get(), -1))
    {
        lua_getglobal(L.get(), "polys");
        if (!lua_istable(L.get(), -1))
        {
            return result;
        }
    }
    result = LuaTableToPolygons(L.get(), -1);
    // L will be closed by UniqueLua deleter
    return result;
}

}  // namespace HsBa::Slicer