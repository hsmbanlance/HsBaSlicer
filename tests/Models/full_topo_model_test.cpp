#define BOOST_TEST_MODULE full_topo_model_test
// Use the header-only variant to provide the test runner (avoids linking issues)
#include <boost/test/included/unit_test.hpp>

#include "base/IModel.hpp"
#include "meshmodel/FullTopoModel.hpp"
#include "2D/IntPolygon.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_SUITE(full_topo_model_test)

// A minimal IModel implementation that returns a cube mesh
class SimpleCubeModel : public IModel
{
public:
	SimpleCubeModel()
	{
		// 8 vertices
		v_.resize(8, 3);
		v_ << -1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f;

		// 12 triangles (2 per face)
		f_.resize(12, 3);
		f_ << 4, 5, 6,
			4, 6, 7,
			0, 1, 2,
			0, 2, 3,
			0, 3, 7,
			0, 7, 4,
			1, 5, 6,
			1, 6, 2,
			3, 2, 6,
			3, 6, 7,
			0, 1, 5,
			0, 5, 4;
	}

	// IModel interface (minimal implementations)
	bool Load(std::string_view) override { return false; }
	bool Save(std::string_view, const ModelFormat) const override { return false; }
	void Translate(const Eigen::Vector3f&) override {}
	void Rotate(const Eigen::Quaternionf&) override {}
	void Scale(const float) override {}
	void Scale(const Eigen::Vector3f&) override {}
	void Transform(const Eigen::Isometry3f&) override {}
	void Transform(const Eigen::Matrix4f&) override {}
	void Transform(const Eigen::Transform<float, 3, Eigen::Affine>&) override {}
	void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override { min = v_.colwise().minCoeff(); max = v_.colwise().maxCoeff(); }
	float Volume() const override { return 0.0f; }
	std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override { return { v_, f_ }; }

private:
	Eigen::MatrixXf v_;
	Eigen::MatrixXi f_;
};

BOOST_AUTO_TEST_CASE(slice_cube_at_zero)
{
	SimpleCubeModel model;
	FullTopoModel topo(model);

	auto polys = topo.Slice(0.0f);
	BOOST_CHECK(!polys.empty());

	// Expect at least one closed polygon with >= 4 vertices (square)
	bool found = false;
	for (const auto& poly : polys)
	{
		if (poly.size() >= 4)
		{
			found = true;
			double area = Area(poly);
			BOOST_CHECK_GT(area, 0.0);
			break;
		}
	}
	BOOST_CHECK(found);

	// Also test UnSafeSlice returns some closed polygon
	auto ups = topo.UnSafeSlice(0.0f);
	bool has_closed = false;
	for (const auto& up : ups)
	{
		if (up.closed && up.path.size() >= 4) { has_closed = true; break; }
	}
	BOOST_CHECK(has_closed);
}

BOOST_AUTO_TEST_CASE(slice_cube_with_lua)
{
	SimpleCubeModel model;
	FullTopoModel topo(model);

	// Lua script: compute intersection points from faces and return convex hull as single polygon
	std::string script = R"lua(
local function intersect_edge(a,b)
    local z1 = V[a].z
    local z2 = V[b].z
    if (z1>height and z2>height) or (z1<height and z2<height) then return nil end
    if z1==height and z2==height then return nil end
    local t = (height - z1) / (z2 - z1)
    local x = V[a].x + t*(V[b].x - V[a].x)
    local y = V[a].y + t*(V[b].y - V[a].y)
    return { x = x, y = y }
end

local inters = {}
for i=1,#F do
    local f = F[i]
    local a,b,c = f[1], f[2], f[3]
    local p = intersect_edge(a,b)
    if p then table.insert(inters, p) end
    p = intersect_edge(b,c)
    if p then table.insert(inters, p) end
    p = intersect_edge(c,a)
    if p then table.insert(inters, p) end
end

if #inters == 0 then return {} end

table.sort(inters, function(a,b) if a.x==b.x then return a.y<b.y else return a.x<b.x end end)
local function cross(o,a,b) return (a.x-o.x)*(b.y-o.y)-(a.y-o.y)*(b.x-o.x) end
local lower = {}
for i=1,#inters do
    while #lower>=2 and cross(lower[#lower-1], lower[#lower], inters[i])<=0 do table.remove(lower) end
    table.insert(lower, inters[i])
end
local upper = {}
for i=#inters,1,-1 do
    while #upper>=2 and cross(upper[#upper-1], upper[#upper], inters[i])<=0 do table.remove(upper) end
    table.insert(upper, inters[i])
end
local hull = {}
for i=1,#lower do table.insert(hull, lower[i]) end
for i=2,#upper-1 do table.insert(hull, upper[i]) end

local polys = {}
polys[1] = hull
return polys
 )lua";

	auto polys = topo.SliceLua(script, 0.0f);
	BOOST_CHECK(!polys.empty());
	bool ok = false;
	for (const auto& poly : polys)
	{
		if (poly.size() >= 4) { ok = true; break; }
	}
	BOOST_CHECK(ok);

	// test UnSafeSliceLua as well
	auto ups = topo.UnSafeSliceLua(script, 0.0f);
	BOOST_CHECK(!ups.empty());
	bool has_closed = false;
	for (const auto& up : ups) if (up.closed && up.path.size() >= 4) { has_closed = true; break; }
	BOOST_CHECK(has_closed);
}

BOOST_AUTO_TEST_SUITE_END()