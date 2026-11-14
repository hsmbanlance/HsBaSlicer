#define BOOST_TEST_MODULE OcctModelTests
#include <boost/test/included/unit_test.hpp>

#include <string>
#include <string_view>

#include "base/error.hpp"
#include "cadmodel/OcctCsg.hpp"
#include "cadmodel/OcctModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_properties)
{
    auto box = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 2.0f, 3.0f});
    // OCCT 实体体积精确为 1×2×3 = 6.0
    BOOST_CHECK_CLOSE(box.Volume(), 6.0f, 1e-3);
    Eigen::Vector3f mn, mx;
    box.BoundingBox(mn, mx);
    // CreateBox 以原点为角点，包围盒尺寸精确为 (1, 2, 3)
    BOOST_CHECK_SMALL(mn.x(), 1e-6f);
    BOOST_CHECK_SMALL(mn.y(), 1e-6f);
    BOOST_CHECK_SMALL(mn.z(), 1e-6f);
    BOOST_CHECK_CLOSE(mx.x(), 1.0f, 1e-3);
    BOOST_CHECK_CLOSE(mx.y(), 2.0f, 1e-3);
    BOOST_CHECK_CLOSE(mx.z(), 3.0f, 1e-3);
}

BOOST_AUTO_TEST_CASE(create_sphere_and_volume)
{
    auto s = OcctModel::CreateSphere(0.5f, 2);
    // OCCT 球体体积精确为 4/3·π·r³
    const float expectedVolume = 4.0f / 3.0f * 3.14159265358979f * 0.5f * 0.5f * 0.5f;
    BOOST_CHECK_CLOSE(s.Volume(), expectedVolume, 1e-2);
}

// OCCT boolean tests guarded by build-time availability — OCCT test only added when
// `HsBaSlicerCADModel` target exists. No additional compile-time guard needed here.
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    auto a = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    auto b = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    b.Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});
    // 两个单位立方体 x 方向重叠 0.5：并集 1.5、交集 0.5、差集 0.5、异或 1.0
    auto u = Union(a, b);
    auto inter = Intersection(a, b);
    auto diff = Difference(a, b);
    auto xr = Xor(a, b);
    BOOST_CHECK_CLOSE(u.Volume(), 1.5f, 1e-2);
    BOOST_CHECK_CLOSE(inter.Volume(), 0.5f, 1e-2);
    BOOST_CHECK_CLOSE(diff.Volume(), 0.5f, 1e-2);
    BOOST_CHECK_CLOSE(xr.Volume(), 1.0f, 1e-2);
}

BOOST_AUTO_TEST_CASE(create_prime_single_polygon)
{
    // Create a simple triangle polygon
    PolygonD triangle;
    triangle.push_back({0.0, 0.0});
    triangle.push_back({1.0, 0.0});
    triangle.push_back({0.5, 1.0});

    // Extrude along Z axis with height 2.0
    auto prism = OcctModel::CreatePrime(triangle, Eigen::Vector3f{0.0f, 0.0f, 2.0f});

    // Check volume: triangle area = 0.5, height = 2.0, volume = 1.0
    float vol = prism.Volume();
    BOOST_CHECK_CLOSE(vol, 1.0f, 0.01);  // Allow 1% tolerance

    // Check bounding box
    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mn.z(), 0.0f, 0.01);
    BOOST_CHECK_CLOSE(mx.z(), 2.0f, 0.01);

    // 切片只吃网格，所以下面这两项也是三角化的守卫：三棱柱 = 上下 2 个三角面 + 3 个矩形侧面
    // （各 2 个三角形）= 8 个三角形，OCCT 不剖分时这里曾是 0 个
    const auto [vertices, faces] = prism.TriangleMesh();
    BOOST_CHECK_EQUAL(faces.rows(), 8);
    BOOST_CHECK(vertices.rows() >= 6);
}

BOOST_AUTO_TEST_CASE(create_prime_polygon_with_hole)
{
    // Outer square
    PolygonD outer;
    outer.push_back({0.0, 0.0});
    outer.push_back({2.0, 0.0});
    outer.push_back({2.0, 2.0});
    outer.push_back({0.0, 2.0});

    // Inner hole (square) - NOTE: For OCCT, holes should have opposite orientation
    PolygonD hole;
    hole.push_back({0.5, 0.5});
    hole.push_back({0.5, 1.5});  // Reversed order for proper hole detection
    hole.push_back({1.5, 1.5});
    hole.push_back({1.5, 0.5});

    PolygonsD paths;
    paths.push_back(outer);
    paths.push_back(hole);

    // Extrude with height 1.5
    auto prism = OcctModel::CreatePrime(paths, Eigen::Vector3f{0.0f, 0.0f, 1.5f});

    // 外方 2×2 减去洞 1×1，底面积 3.0 × 高度 1.5 = 体积精确 4.5
    BOOST_CHECK_CLOSE(prism.Volume(), 4.5f, 1e-2);

    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mx.x() - mn.x(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.y() - mn.y(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.z() - mn.z(), 1.5f, 1e-2);

    // 有洞的顶/底面必须把洞保留到网格里：网格顶点落在洞内部就说明洞被抹平了
    const auto [vertices, faces] = prism.TriangleMesh();
    BOOST_CHECK(faces.rows() >= 16);  // 8 个侧面三角形 + 上下两个环形面
    for (Eigen::Index i = 0; i < vertices.rows(); ++i)
    {
        const Eigen::Vector3f p(vertices.row(i));
        const bool inside_hole = p.x() > 0.5f && p.x() < 1.5f && p.y() > 0.5f && p.y() < 1.5f;
        BOOST_CHECK(!inside_hole);
    }
}

BOOST_AUTO_TEST_CASE(create_prime_non_planar_direction)
{
    // Create a rectangle
    PolygonD rect;
    rect.push_back({0.0, 0.0});
    rect.push_back({1.0, 0.0});
    rect.push_back({1.0, 1.0});
    rect.push_back({0.0, 1.0});

    // Extrude in diagonal direction
    auto prism = OcctModel::CreatePrime(rect, Eigen::Vector3f{1.0f, 1.0f, 1.0f});

    // 斜拉伸体积 = 底面积 × z 方向分量 = 1.0
    BOOST_CHECK_CLOSE(prism.Volume(), 1.0f, 1e-2);

    // 包围盒：底面 [0,1]² 沿 (1,1,1) 拉伸，范围精确为 [0,2]×[0,2]×[0,1]
    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_SMALL(mn.x(), 1e-6f);
    BOOST_CHECK_SMALL(mn.y(), 1e-6f);
    BOOST_CHECK_SMALL(mn.z(), 1e-6f);
    BOOST_CHECK_CLOSE(mx.x(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.y(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.z(), 1.0f, 1e-2);
}

// ============================================================================
// Text CSG (OCCT DRAW Test Harness)
// ============================================================================
// The DRAW command interpreter only exists when the opencascade port was built with
// its 'tcl' feature; cadmodel/CMakeLists.txt defines USE_OCCT_DRAW in that case and
// aborts the configuration otherwise, unless HSBA_REQUIRE_TEXT_CSG=OFF names the
// degraded build explicitly (then it also defines HSBA_TEXT_CSG_DISABLED).

#ifdef USE_OCCT_DRAW

/// 能力标志与报错行为必须一致，否则脚本层无法预先判断能否用 CSG
BOOST_AUTO_TEST_CASE(csg_support_flag)
{
    BOOST_CHECK(HasCsgSupport());
}

/// 图元命令与 Tcl 内建命令：确认命令组与解释器基础都真的可用
BOOST_AUTO_TEST_CASE(csg_single_command_volume)
{
    const float sphereVolume = 4.0f / 3.0f * 3.14159265358979f * 125.0f;
    const auto box = OcctModel::CreateFromCsg("set d 20\nbox csgBox $d $d $d", "csgBox");
    BOOST_CHECK_CLOSE(box.Volume(), 8000.0f, 1e-3f);

    const auto sphere = OcctModel::CreateFromCsg("psphere csgSphere 5", "csgSphere");
    BOOST_CHECK_CLOSE(sphere.Volume(), sphereVolume, 1e-3f);
}

/// 显式指定读回的 DRAW 变量名，而不是默认的 "result"
BOOST_AUTO_TEST_CASE(csg_named_result_variable)
{
    // pcylinder name R H：体积 π·r²·h = π·9·10
    const auto cylinder = OcctModel::CreateFromCsg("pcylinder csgCylinder 3 10", "csgCylinder");
    BOOST_CHECK_CLOSE(cylinder.Volume(), 3.14159265358979f * 90.0f, 1e-3f);
}

/// 多行脚本一次求值：Tcl set/expr 变量代入 + 布尔树
BOOST_AUTO_TEST_CASE(csg_script_boolean_cut)
{
    // psphere 的球心固定在原点，用 ttranslate 搬到 20³ 立方体（0..20）的中心，
    // 差集体积精确为 8000 - 4/3·π·5³
    const auto cut =
        OcctModel::CreateFromCsg("set     side 20\n"
                                 "set     r    5\n"
                                 "box     base $side $side $side\n"
                                 "psphere hole $r\n"
                                 "ttranslate hole [expr {$side / 2}] [expr {$side / 2}] [expr {$side / 2}]\n"
                                 "bcut    result base hole\n");
    const float sphereVolume = 4.0f / 3.0f * 3.14159265358979f * 125.0f;
    // bcut 的产物是一个 compound，Volume() 在整个子树上积分
    BOOST_CHECK_CLOSE(cut.Volume(), 8000.0f - sphereVolume, 1e-3f);
}

/// 并集与求交同样由 BOPTest 命令组提供
BOOST_AUTO_TEST_CASE(csg_script_fuse_and_common)
{
    const float sphereVolume = 4.0f / 3.0f * 3.14159265358979f * 125.0f;
    // r=5 球位于 10³ 立方体中心，与六个面内切：完全包含在立方体内
    const auto fuse = OcctModel::CreateFromCsg("box     csgFuseBase 10 10 10\n"
                                               "psphere csgFuseBall 5\n"
                                               "ttranslate csgFuseBall 5 5 5\n"
                                               "bfuse   result csgFuseBase csgFuseBall\n");
    BOOST_CHECK_CLOSE(fuse.Volume(), 1000.0f, 1e-3f);

    const auto common = OcctModel::CreateFromCsg("box     csgCommonBase 10 10 10\n"
                                                 "psphere csgCommonBall 5\n"
                                                 "ttranslate csgCommonBall 5 5 5\n"
                                                 "bcommon result csgCommonBase csgCommonBall\n");
    BOOST_CHECK_CLOSE(common.Volume(), sphereVolume, 1e-2f);
}

BOOST_AUTO_TEST_CASE(csg_bad_script_reports_error)
{
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg(""), InvalidArgumentError);
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("thisCommandDoesNotExist csgBad 1 1 1"), InvalidArgumentError);
    // 缺少尺寸参数：BRepPrimAPI_MakeBox 退化为零尺寸实体，OCCT 报错
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("box csgIncomplete"), InvalidArgumentError);

    // 内嵌 NUL 必须拒绝：求值走 C 字符串，静默截断会交出半截程序的几何而不报错
    std::string truncated = "box csgNul 1 1 1";
    truncated += '\0';
    truncated += "psphere csgNeverReached 5";
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg(std::string_view(truncated)), InvalidArgumentError);
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("box csgNameWithNul 1 1 1", std::string_view("csg\0Name", 5)),
                      InvalidArgumentError);
}

BOOST_AUTO_TEST_CASE(csg_missing_variable_not_found)
{
    // 脚本本身合法，只是没有在请求的变量名下留下 shape
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("box csgStored 1 1 1", "csgNeverAssigned"), NotFoundError);
}

/// CSG 造出的 B-Rep 必须能三角化：切片只吃网格，缺这一步每层轮廓都是空的，作业却照样“成功”
BOOST_AUTO_TEST_CASE(csg_shape_carries_a_triangulation)
{
    const auto box = OcctModel::CreateFromCsg("box result 6 4 2");
    const auto [vertices, faces] = box.TriangleMesh();

    // 长方体的表面三角形数与尺寸无关，恒为 12；这里曾经返回 0 个，因为没人调用 BRepMesh
    BOOST_CHECK_EQUAL(faces.rows(), 12);

    Eigen::Vector3f mn, mx;
    box.BoundingBox(mn, mx);
    for (Eigen::Index i = 0; i < vertices.rows(); ++i)
    {
        // 网格必须贴合精确几何，不能跑到包围盒之外
        const Eigen::Vector3f p(vertices.row(i));
        BOOST_CHECK((p.array() >= mn.array() - 1e-3f).all());
        BOOST_CHECK((p.array() <= mx.array() + 1e-3f).all());
    }

    // 曲面按相对挠度细分，三角形数必然超过长方体：这才证明剖分参数真的在起作用
    const auto cylinder = OcctModel::CreateFromCsg("pcylinder result 5 10");
    BOOST_CHECK(cylinder.TriangleMesh().second.rows() > 12);
}

/// DRAW 仓库是进程级的，而 OCCT 不提供擦除接口：不清名时本次调用会静默读回上一次的几何
BOOST_AUTO_TEST_CASE(csg_stale_bank_name_is_cleared)
{
    const auto first = OcctModel::CreateFromCsg("box result 1 2 3");  // 落在默认的 result 上
    BOOST_CHECK_CLOSE(first.Volume(), 6.0f, 1e-3f);

    // 第二段脚本什么都不产出，只能报 NotFoundError，不能返回上面那个 6mm³ 的盒子
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("set unrelated 1"), NotFoundError);
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("set unrelated 2", "result"), NotFoundError);
}

/// DRAW 变量在整个进程里累积，中间变量也不例外：不清理时第二段脚本会读到上一段留下的操作数
BOOST_AUTO_TEST_CASE(csg_intermediate_variables_do_not_leak_across_calls)
{
    const auto first = OcctModel::CreateFromCsg("box csgLeakA 3 3 3\n"
                                                "box csgLeakB 4 4 4\n"
                                                "box result 1 1 1");
    BOOST_CHECK_CLOSE(first.Volume(), 1.0f, 1e-3f);

    // 本段脚本自带不了 csgLeakA/csgLeakB，只能报错；缺变量时 OCCT 的具体异常类型不作为断言目标
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("bfuse result csgLeakA csgLeakB"), RuntimeError);
}

/// 几何只在变量存活期间被 DRAW 仓库持有：每次调用都用新名字，也不能让仓库无限增长
BOOST_AUTO_TEST_CASE(csg_bank_does_not_grow_with_repeated_calls)
{
    const std::size_t baseline = CsgBankExtent();
    for (int i = 0; i < 12; ++i)
    {
        const std::string name = "csgGrow" + std::to_string(i);
        const auto box = OcctModel::CreateFromCsg("box " + name + " 1 1 1", name);
        BOOST_CHECK_CLOSE(box.Volume(), 1.0f, 1e-3f);
    }
    // 失败路径同样要清理，否则报错的脚本会成为泄漏的主要来源
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("box csgGrowBad 1 1 1\nthisCommandDoesNotExist csgGrowBad"),
                      InvalidArgumentError);
    BOOST_CHECK_EQUAL(CsgBankExtent(), baseline);
}

/// 清理基线不能放在脚本可及的命名空间里：外部可控的 shapeName 和脚本本身都能占用同名变量，
/// 基线一旦被覆写，清理就会反过来删掉属于后续调用的状态
BOOST_AUTO_TEST_CASE(csg_internal_state_is_out_of_reach_of_scripts)
{
    // setup 设置的进程级标志：任何调用都不能把它清掉（probe 无产出，正常应报 NotFoundError）
    const char* const probe = "if {![info exists Draw_TOPOLOGY]} { error flagGone }\n";
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg(probe), NotFoundError);

    const std::size_t baseline = CsgBankExtent();
    // 把调用方可控的 shapeName 设成桥接层曾经用过的内部变量名
    BOOST_CHECK_CLOSE(OcctModel::CreateFromCsg("box HSBA_CSG_SNAPSHOT 3 3 3", "HSBA_CSG_SNAPSHOT").Volume(), 27.0f,
                      1e-3f);
    // 脚本自己覆写那两个内部名，同样不得干扰清理
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("set HSBA_CSG_SNAPSHOT junk\n"
                                               "box HSBA_CSG_V 1 1 1",
                                               "csgAbsent"),
                      NotFoundError);

    BOOST_CHECK_THROW(OcctModel::CreateFromCsg(probe), NotFoundError);
    BOOST_CHECK_EQUAL(CsgBankExtent(), baseline);
}

#else  // !USE_OCCT_DRAW

#if !defined(HSBA_TEXT_CSG_DISABLED)
#error "No USE_OCCT_DRAW although text CSG was not disabled on purpose: the CMake gate did not hold."
#endif

// Only a build configured with -DHSBA_REQUIRE_TEXT_CSG=OFF compiles this branch.
BOOST_AUTO_TEST_CASE(csg_requires_draw_build)
{
    BOOST_CHECK(!HasCsgSupport());
    BOOST_CHECK_THROW(OcctModel::CreateFromCsg("box csgUnavailable 1 1 1"), NotSupportedError);
}

#endif  // USE_OCCT_DRAW
