#pragma once
#ifndef HSBA_FULLTOPOMODEL_HPP
#define HSBA_FULLTOPOMODEL_HPP

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <vector>

#include <Eigen/Core>

#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "base/IModel.hpp"
#include <lua.hpp>

/**
 * @file FullTopoModel.hpp
 * @brief Fully topology-reconstructed mesh model dedicated to slicing.
 */

namespace HsBa::Slicer
{
/// A contour that may be open (not necessarily closed).
struct UnSafePolygon
{
    Polygon path;
    bool closed = true;
};
/// A collection of possibly-open contours.
using UnSafePolygons = std::vector<UnSafePolygon>;

/// A contour that may be open (not necessarily closed).
struct UnSafePolygonD
{
    PolygonD path;
    bool closed = true;
};
/// A collection of possibly-open contours.
using UnSafePolygonsD = std::vector<UnSafePolygonD>;

/**
 * @brief A mesh model dedicated to slicing with full topology reconstruction; it offers
 *        no public modification (except construction).
 *
 * Topology relations are rebuilt in the constructor.
 * Affine transforms could be permitted but are unnecessary.
 * Use IglModel or CgalModel for mesh processing, and CADModel's classes/methods for CAD models.
 * This class can rebuild a topological manifold, but the rebuilt manifold may be incomplete; no
 * exhaustive manifold check is provided, and the rebuilt manifold may contain errors.
 */
class FullTopoModel final
{
public:
    struct Face
    {
        std::array<int, 3> triangle{-1, -1, -1};
        std::array<int, 3> edges{-1, -1, -1};
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
        std::array<int, 2> vertices{-1, -1};
        std::array<int, 2> faces{-1, -1};
    };

    FullTopoModel(const IModel& model, bool use_normals = false);
    FullTopoModel(IModel&& model, bool use_normals = false) = delete;
    FullTopoModel(const std::vector<Eigen::Vector3f>& vertices, const std::vector<std::array<int, 3>>& triangles,
                  bool use_normals = false);
    ~FullTopoModel() = default;

    /**
     * @brief Check topological completeness; an incomplete model is usually not a topological
     *        manifold, which affects some algorithms and may contain errors.
     */
    bool CheckTopo() const;

    inline const std::vector<Vertex>& GetVertices() const { return vertices_; }
    inline const std::vector<Edge>& GetEdges() const { return edges_; }
    inline const std::vector<Face>& GetFaces() const { return faces_; }

    inline const Vertex& GetVertex(int index) const { return vertices_[index]; }
    inline const Edge& GetEdge(int index) const { return edges_[index]; }
    inline const Face& GetFace(int index) const { return faces_[index]; }

    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const;

    /// Euler characteristic; if it is odd or greater than 2, the model may not be a topological
    /// manifold. Can be used to detect holes in the model. This function does not check topological completeness.
    int EulerCharacteristic() const;

    /// Intersection of a line with a plane perpendicular to Z.
    static bool Intersection(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, const float height,
                             Eigen::Vector3f& intersection);

    // Slicing along Z. Common slicing algorithms have the same complexity, except when topology-rebuild
    // time is excluded; constructing a FullTopoModel already rebuilt the topology, so no rebuild is needed.

    /// Safe slice: contains only closed contours; open contours are discarded.
    Polygons Slice(const float height, double tolerance = 0.001) const;
    /// Unsafe slice: includes open contours.
    UnSafePolygons UnSafeSlice(const float height, double tolerance = 0.001) const;

    /// Fast slice: builds the result directly from topology info; checks topological completeness first and
    /// throws immediately on failure.
    Polygons SliceFast(const float height) const;

    // Run a custom Lua script to produce polygons from vertex/edge/face data.
    // The script receives globals: V (1-based array of {x,y,z}),
    // E (1-based array of {v1,v2}), F (1-based array of {v1,v2,v3}), and 'height'.
    // The script should return a table of polygons: polys = { { {x=..,y=..}, ... }, ... }
    Polygons SliceLua(const std::string& script, const float height,
                      const std::vector<std::function<void(lua_State*)>>& ext_regs = {}) const;

    Polygons SliceLua(const std::string& script, const std::string& funcName, const float height,
                      const std::vector<std::function<void(lua_State*)>>& ext_regs = {}) const;

    Polygons SliceLua(const std::filesystem::path& script_file, const std::string& funcName, const float height,
                      const std::vector<std::function<void(lua_State*)>>& ext_regs = {}) const;


    // Same but returns potentially open polylines with closed flag
    UnSafePolygons UnSafeSliceLua(const std::string& script, const float height,
                                  const std::vector<std::function<void(lua_State*)>>& ext_regs = {}) const;


private:
    void BuildTopo(const std::vector<Eigen::Vector3f>& vertices, const std::vector<std::array<int, 3>>& triangles,
                   bool use_normals = false);

    std::vector<Vertex> vertices_;
    std::vector<Edge> edges_;
    std::vector<Face> faces_;
};

// Push this model data to a Lua state as globals: V, E, F and set 'height'
void PushFullTopoModelToLua(lua_State* L, const FullTopoModel& model, float height);
}  // namespace HsBa::Slicer


#endif  // !HSBA_FULLTOPOMODEL_HPP
