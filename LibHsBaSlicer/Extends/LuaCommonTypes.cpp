/**
 * @file LuaCommonTypes.cpp
 * @brief Implements the Lua <-> AnyObject table conversions for the common custom types
 *        declared in LuaCommonTypes.hpp.
 *
 * Every registered type gets a pair of Lua globals: `new_<Name>` turns a plain Lua table into
 * an `AnyObject` wrapping the native type, and `cast_<Name>` turns an `AnyObject` back into a
 * plain Lua table. See LuaCommonTypes.hpp for the exact table encoding per type family.
 */
#include "LuaCommonTypes.hpp"

#include <cmath>
#include <format>
#include <mutex>
#include <string>
#include <type_traits>

#include "LuaAddFunction.hpp"

#include "base/error.hpp"
#include "fileoperator/param_reflect.hpp"

namespace HsBa::Slicer
{
namespace
{
// ---------------------------------------------------------------------------
// Low-level Lua table helpers
// ---------------------------------------------------------------------------
static void set_number(lua_State* L, const char* key, lua_Number v)
{
    lua_pushnumber(L, v);
    lua_setfield(L, -2, key);
}

static void set_int(lua_State* L, const char* key, long long v)
{
    lua_pushinteger(L, static_cast<lua_Integer>(v));
    lua_setfield(L, -2, key);
}

// Reads coordinate @p i (0 based) from table at @p idx, preferring the named key
// (@p key) and falling back to the sequence element at index i + 1.
static lua_Number read_component(lua_State* L, int idx, int i, const char* key)
{
    lua_Number value = 0;
    lua_getfield(L, idx, key);
    if (lua_isnumber(L, -1) || lua_isinteger(L, -1))
    {
        value = lua_tonumber(L, -1);
    }
    else
    {
        lua_rawgeti(L, idx, i + 1);
        if (lua_isnumber(L, -1) || lua_isinteger(L, -1))
            value = lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return value;
}

// Reads the sequence element at 1-based @p i from the table currently on top of the stack.
static lua_Number read_element(lua_State* L, int tbl_idx, int i)
{
    lua_rawgeti(L, tbl_idx, i);
    lua_Number value = 0;
    if (lua_isnumber(L, -1) || lua_isinteger(L, -1))
        value = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}

// ---------------------------------------------------------------------------
// Eigen fixed vectors and quaternions: named coordinate maps {x, y, (z), (w)}
// ---------------------------------------------------------------------------
template <typename V>
static void push_named_vector(lua_State* L, const V& v)
{
    static constexpr const char* keys[] = {"x", "y", "z", "w"};
    lua_createtable(L, 0, V::SizeAtCompileTime);
    for (int i = 0; i < V::SizeAtCompileTime && i < 4; ++i)
        set_number(L, keys[i], static_cast<lua_Number>(v(i)));
}

template <typename V>
static V read_named_vector(lua_State* L, int idx)
{
    static constexpr const char* keys[] = {"x", "y", "z", "w"};
    V v = V::Zero();
    for (int i = 0; i < V::SizeAtCompileTime && i < 4; ++i)
        v(i) = static_cast<typename V::Scalar>(read_component(L, idx, i, keys[i]));
    return v;
}

template <typename Q>
static void push_quaternion(lua_State* L, const Q& q)
{
    lua_createtable(L, 0, 4);
    set_number(L, "x", static_cast<lua_Number>(q.x()));
    set_number(L, "y", static_cast<lua_Number>(q.y()));
    set_number(L, "z", static_cast<lua_Number>(q.z()));
    set_number(L, "w", static_cast<lua_Number>(q.w()));
}

template <typename Q>
static Q read_quaternion(lua_State* L, int idx)
{
    using S = typename Q::Scalar;
    const auto x = static_cast<S>(read_component(L, idx, 0, "x"));
    const auto y = static_cast<S>(read_component(L, idx, 1, "y"));
    const auto z = static_cast<S>(read_component(L, idx, 2, "z"));
    const auto w = static_cast<S>(read_component(L, idx, 3, "w"));
    return Q(w, x, y, z);  // Eigen quaternion ctor takes the scalar part first
}

// ---------------------------------------------------------------------------
// Eigen matrices (fixed & dynamic): nested row sequences
// ---------------------------------------------------------------------------
template <typename M>
static M make_matrix(int rows, int cols)
{
    if constexpr (M::RowsAtCompileTime == Eigen::Dynamic || M::ColsAtCompileTime == Eigen::Dynamic)
        return M(rows, cols);
    else
        return M::Zero();
}

template <typename M>
static void push_matrix(lua_State* L, const M& m)
{
    using S = typename M::Scalar;
    const int rows = static_cast<int>(m.rows());
    const int cols = static_cast<int>(m.cols());
    lua_createtable(L, rows, 0);
    for (int r = 0; r < rows; ++r)
    {
        lua_createtable(L, cols, 0);
        for (int c = 0; c < cols; ++c)
        {
            if constexpr (std::is_integral_v<S>)
                lua_pushinteger(L, static_cast<lua_Integer>(m(r, c)));
            else
                lua_pushnumber(L, static_cast<lua_Number>(m(r, c)));
            lua_rawseti(L, -2, c + 1);
        }
        lua_rawseti(L, -2, r + 1);
    }
}

template <typename M>
static M read_matrix(lua_State* L, int idx)
{
    const int rows = static_cast<int>(lua_rawlen(L, idx));
    int cols = 0;
    if (rows > 0)
    {
        lua_rawgeti(L, idx, 1);
        if (lua_istable(L, -1))
            cols = static_cast<int>(lua_rawlen(L, -1));
        lua_pop(L, 1);
    }

    M m = make_matrix<M>(rows, cols);
    const int rmax = static_cast<int>(m.rows());
    const int cmax = static_cast<int>(m.cols());
    for (int r = 0; r < rmax; ++r)
    {
        lua_rawgeti(L, idx, r + 1);
        if (lua_istable(L, -1))
        {
            for (int c = 0; c < cmax; ++c)
                m(r, c) = static_cast<typename M::Scalar>(read_element(L, -1, c + 1));
        }
        lua_pop(L, 1);
    }
    return m;
}

// ---------------------------------------------------------------------------
// Clipper2 point / polygon conversions
// ---------------------------------------------------------------------------
static void push_point2d(lua_State* L, const Point2D& p)
{
    lua_createtable(L, 0, 2);
    set_number(L, "x", static_cast<lua_Number>(p.x));
    set_number(L, "y", static_cast<lua_Number>(p.y));
}

static Point2D read_point2d(lua_State* L, int idx)
{
    return Point2D(static_cast<double>(read_component(L, idx, 0, "x")),
                   static_cast<double>(read_component(L, idx, 1, "y")));
}

static void push_point2(lua_State* L, const Point2& p)
{
    lua_createtable(L, 0, 2);
    set_int(L, "x", static_cast<long long>(p.x));
    set_int(L, "y", static_cast<long long>(p.y));
}

static Point2 read_point2(lua_State* L, int idx)
{
    return Point2(static_cast<int64_t>(std::llround(read_component(L, idx, 0, "x"))),
                  static_cast<int64_t>(std::llround(read_component(L, idx, 1, "y"))));
}

static void push_polygon_d(lua_State* L, const PolygonD& poly)
{
    lua_createtable(L, static_cast<int>(poly.size()), 0);
    int i = 1;
    for (const auto& p : poly)
    {
        push_point2d(L, p);
        lua_rawseti(L, -2, i++);
    }
}

static PolygonD read_polygon_d(lua_State* L, int idx)
{
    PolygonD poly;
    const int n = static_cast<int>(lua_rawlen(L, idx));
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, idx, i);
        if (lua_istable(L, -1))
            poly.push_back(read_point2d(L, -1));
        lua_pop(L, 1);
    }
    return poly;
}

static void push_polygons_d(lua_State* L, const PolygonsD& polys)
{
    lua_createtable(L, static_cast<int>(polys.size()), 0);
    int i = 1;
    for (const auto& poly : polys)
    {
        push_polygon_d(L, poly);
        lua_rawseti(L, -2, i++);
    }
}

static PolygonsD read_polygons_d(lua_State* L, int idx)
{
    PolygonsD polys;
    const int n = static_cast<int>(lua_rawlen(L, idx));
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, idx, i);
        if (lua_istable(L, -1))
            polys.push_back(read_polygon_d(L, -1));
        lua_pop(L, 1);
    }
    return polys;
}

static void push_polygon(lua_State* L, const Polygon& poly)
{
    lua_createtable(L, static_cast<int>(poly.size()), 0);
    int i = 1;
    for (const auto& p : poly)
    {
        push_point2(L, p);
        lua_rawseti(L, -2, i++);
    }
}

static Polygon read_polygon(lua_State* L, int idx)
{
    Polygon poly;
    const int n = static_cast<int>(lua_rawlen(L, idx));
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, idx, i);
        if (lua_istable(L, -1))
            poly.push_back(read_point2(L, -1));
        lua_pop(L, 1);
    }
    return poly;
}

static void push_polygons(lua_State* L, const Polygons& polys)
{
    lua_createtable(L, static_cast<int>(polys.size()), 0);
    int i = 1;
    for (const auto& poly : polys)
    {
        push_polygon(L, poly);
        lua_rawseti(L, -2, i++);
    }
}

static Polygons read_polygons(lua_State* L, int idx)
{
    Polygons polys;
    const int n = static_cast<int>(lua_rawlen(L, idx));
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, idx, i);
        if (lua_istable(L, -1))
            polys.push_back(read_polygon(L, -1));
        lua_pop(L, 1);
    }
    return polys;
}

// ---------------------------------------------------------------------------
// Table-based adapter glue: bridges a Lua table and an AnyObject wrapping T.
// ---------------------------------------------------------------------------
template <typename T, Utils::TemplateString Name, void (*Push)(lua_State*, const T&), T (*Read)(lua_State*, int)>
class TableAdapter final : public LuaAnyObjectNewCastBase
{
public:
    LuaFuncPair GetNewFuncPair() const override { return {"new_" + std::string(Name.ToStringView()), &New}; }
    LuaFuncPair GetCastFuncPair() const override { return {"cast_" + std::string(Name.ToStringView()), &Cast}; }

private:
    static int New(lua_State* L)
    {
        if (!lua_istable(L, 1))
        {
            lua_pushstring(L, std::format("new_{} expects a table argument", Name.ToStringView()).c_str());
            return lua_error(L);
        }
        T value = Read(L, 1);
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, value);
        return 1;
    }

    static int Cast(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, "Invalid AnyObject object");
            return lua_error(L);
        }
        try
        {
            // cast_new copies the value out, leaving the AnyObject responsible for its own storage.
            T value = obj->template cast_new<T>();
            Push(L, value);
            return 1;
        }
        catch (const RuntimeError& e)
        {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
};

// Named-vector adapters
using Vec2fAdapter =
    TableAdapter<Eigen::Vector2f, "Vector2f", &push_named_vector<Eigen::Vector2f>, &read_named_vector<Eigen::Vector2f>>;
using Vec3fAdapter =
    TableAdapter<Eigen::Vector3f, "Vector3f", &push_named_vector<Eigen::Vector3f>, &read_named_vector<Eigen::Vector3f>>;
using Vec4fAdapter =
    TableAdapter<Eigen::Vector4f, "Vector4f", &push_named_vector<Eigen::Vector4f>, &read_named_vector<Eigen::Vector4f>>;
using Vec2dAdapter =
    TableAdapter<Eigen::Vector2d, "Vector2d", &push_named_vector<Eigen::Vector2d>, &read_named_vector<Eigen::Vector2d>>;
using Vec3dAdapter =
    TableAdapter<Eigen::Vector3d, "Vector3d", &push_named_vector<Eigen::Vector3d>, &read_named_vector<Eigen::Vector3d>>;
using Vec4dAdapter =
    TableAdapter<Eigen::Vector4d, "Vector4d", &push_named_vector<Eigen::Vector4d>, &read_named_vector<Eigen::Vector4d>>;
using Vec2iAdapter =
    TableAdapter<Eigen::Vector2i, "Vector2i", &push_named_vector<Eigen::Vector2i>, &read_named_vector<Eigen::Vector2i>>;
using Vec3iAdapter =
    TableAdapter<Eigen::Vector3i, "Vector3i", &push_named_vector<Eigen::Vector3i>, &read_named_vector<Eigen::Vector3i>>;
using Vec4iAdapter =
    TableAdapter<Eigen::Vector4i, "Vector4i", &push_named_vector<Eigen::Vector4i>, &read_named_vector<Eigen::Vector4i>>;

// Matrix adapters
using Mat2dAdapter =
    TableAdapter<Eigen::Matrix2d, "Matrix2d", &push_matrix<Eigen::Matrix2d>, &read_matrix<Eigen::Matrix2d>>;
using Mat3dAdapter =
    TableAdapter<Eigen::Matrix3d, "Matrix3d", &push_matrix<Eigen::Matrix3d>, &read_matrix<Eigen::Matrix3d>>;
using Mat4dAdapter =
    TableAdapter<Eigen::Matrix4d, "Matrix4d", &push_matrix<Eigen::Matrix4d>, &read_matrix<Eigen::Matrix4d>>;
using MatXfAdapter =
    TableAdapter<Eigen::MatrixXf, "MatrixXf", &push_matrix<Eigen::MatrixXf>, &read_matrix<Eigen::MatrixXf>>;
using MatXiAdapter =
    TableAdapter<Eigen::MatrixXi, "MatrixXi", &push_matrix<Eigen::MatrixXi>, &read_matrix<Eigen::MatrixXi>>;

// Quaternion adapters
using QuatfAdapter = TableAdapter<Eigen::Quaternionf, "Quaternionf", &push_quaternion<Eigen::Quaternionf>,
                                  &read_quaternion<Eigen::Quaternionf>>;
using QuatdAdapter = TableAdapter<Eigen::Quaterniond, "Quaterniond", &push_quaternion<Eigen::Quaterniond>,
                                  &read_quaternion<Eigen::Quaterniond>>;

// Clipper2 geometry adapters
using Point2DAdapter = TableAdapter<Point2D, "Point2D", &push_point2d, &read_point2d>;
using Point2Adapter = TableAdapter<Point2, "Point2", &push_point2, &read_point2>;
using PolygonDAdapter = TableAdapter<PolygonD, "PolygonD", &push_polygon_d, &read_polygon_d>;
using PolygonsDAdapter = TableAdapter<PolygonsD, "PolygonsD", &push_polygons_d, &read_polygons_d>;
using PolygonAdapter = TableAdapter<Polygon, "Polygon", &push_polygon, &read_polygon>;
using PolygonsAdapter = TableAdapter<Polygons, "Polygons", &push_polygons, &read_polygons>;
}  // namespace

std::vector<LuaAnyObjectNewCastBase*> GetCommonAnyObjectTypes()
{
    static Vec2fAdapter vec2f;
    static Vec3fAdapter vec3f;
    static Vec4fAdapter vec4f;
    static Vec2dAdapter vec2d;
    static Vec3dAdapter vec3d;
    static Vec4dAdapter vec4d;
    static Vec2iAdapter vec2i;
    static Vec3iAdapter vec3i;
    static Vec4iAdapter vec4i;
    static Mat2dAdapter mat2d;
    static Mat3dAdapter mat3d;
    static Mat4dAdapter mat4d;
    static MatXfAdapter matxf;
    static MatXiAdapter matxi;
    static QuatfAdapter quatf;
    static QuatdAdapter quatd;
    static Point2DAdapter point2d;
    static Point2Adapter point2;
    static PolygonDAdapter polygon_d;
    static PolygonsDAdapter polygons_d;
    static PolygonAdapter polygon;
    static PolygonsAdapter polygons;

    return {
        &vec2f, &vec3f, &vec4f, &vec2d, &vec3d, &vec4d,   &vec2i,  &vec3i,     &vec4i,      &mat2d,   &mat3d,
        &mat4d, &matxf, &matxi, &quatf, &quatd, &point2d, &point2, &polygon_d, &polygons_d, &polygon, &polygons,
    };
}

void RegisterCommonAnyObjectTypes(lua_State* L)
{
    // Per-state idempotency guard: pipeline stages may execute several pools on the same
    // Lua state (Support: 2D+3D, SLA: 2D+File), and RegisterAnyObject rebuilds the global
    // AnyObject table on every call. Mark the state in the registry so only the first call
    // for a given lua_State performs the registration.
    static constexpr const char kRegistryKey[] = "HsBa.CommonAnyObjectTypesRegistered";
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey);
    if (!lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);

    // Built-in scalar adapters
    static LuaInt int_type;
    static LuaLong long_type;
    static LuaLongLong longlong_type;
    static LuaSize_t size_t_type;
    static LuaDouble double_type;
    static LuaFloat float_type;
    static LuaBool bool_type;
    static LuaString string_type;
    static LuaCString cstring_type;

    std::vector<LuaAnyObjectNewCastBase*> types = GetCommonAnyObjectTypes();
    types.insert(types.end(), {
                                  &int_type,
                                  &long_type,
                                  &longlong_type,
                                  &size_t_type,
                                  &double_type,
                                  &float_type,
                                  &bool_type,
                                  &string_type,
                                  &cstring_type,
                              });

    RegisterAnyObject(L, types);

    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey);
}

void InstallCommonAnyObjectTypes()
{
    static std::once_flag once;
    std::call_once(once,
                   []
                   {
                       // Complete PipelineConfig reflection registration at pipeline start (idempotent via an internal call_once),
                       // so ParamStore and any stage consuming GetFileFunctions() can traverse the fields immediately.
                       RegisterPipelineConfigTypes();
                       // One LuaRegFunc per generic pool; the per-state guard above keeps stages that
                       // consume several pools from registering the same Lua state twice.
                       const LuaRegFunc reg = [](lua_State* L) { RegisterCommonAnyObjectTypes(L); };
                       Add2DFunctions(reg);
                       Add3DFunctions(reg);
                       AddFileFunctions(reg);
                   });
}

}  // namespace HsBa::Slicer
