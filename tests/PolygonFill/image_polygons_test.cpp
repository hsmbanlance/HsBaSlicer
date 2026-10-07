#define BOOST_TEST_MODULE ImagePolygonsTests
#include <boost/test/included/unit_test.hpp>

#include "../../2D/ImageToPolygons.hpp"
#include "../../base/error.hpp"
#ifdef HAS_OPENCV
#include <opencv2/opencv.hpp>
#endif
#include <filesystem>
#include <fstream>

using namespace HsBa::Slicer;

struct DisableCrt
{
    DisableCrt()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF);
#endif  // defined(_MSC_VER) && defined(_DEBUG)
    }
};

BOOST_AUTO_TEST_CASE(fromimage_and_toimage_roundtrip)
{
#ifdef HAS_OPENCV
    [[maybe_unused]]
    static DisableCrt crt_;
    // create a simple synthetic image (two rectangles with different intensities)
    int w = 80, h = 60;
    cv::Mat img(h, w, CV_8UC1, cv::Scalar(0));
    cv::rectangle(img, cv::Rect(5, 5, 30, 20), cv::Scalar(200), cv::FILLED);
    cv::rectangle(img, cv::Rect(40, 20, 30, 25), cv::Scalar(120), cv::FILLED);

    auto tmpPath = std::filesystem::temp_directory_path() / "hsbaslicer_test_img.png";
    cv::imwrite(tmpPath.string(), img);

    // single threshold (should detect the brighter rectangle only)
    PolygonsD polys = FromImage(tmpPath.string(), 180, 1.0);
    // 只有亮度 200 的矩形超过阈值 180 → 恰好 1 个轮廓
    BOOST_REQUIRE_EQUAL(polys.size(), 1u);
    BOOST_CHECK_GE(polys[0].size(), 4u);

    // multi-threshold: detect both layers
    std::vector<int> thresholds = {100, 180};
    auto layers = FromImageMulti(tmpPath.string(), thresholds, 1.0);
    BOOST_REQUIRE_EQUAL(layers.size(), 2u);
    // 阈值 100：两个矩形（200 与 120）均被检出 → 2 个轮廓
    BOOST_CHECK_EQUAL(layers[0].size(), 2u);
    // 阈值 180：仅亮度 200 的矩形被检出 → 1 个轮廓
    BOOST_CHECK_EQUAL(layers[1].size(), 1u);

    // test ToImage: write PNG and SVG
    PolygonsD simple;
    // one rectangle
    PolygonD rect;
    rect.emplace_back(Point2D{10.0, 10.0});
    rect.emplace_back(Point2D{30.0, 10.0});
    rect.emplace_back(Point2D{30.0, 30.0});
    rect.emplace_back(Point2D{10.0, 30.0});
    simple.push_back(rect);

    auto outPng = std::filesystem::temp_directory_path() / "hsbaslicer_out.png";
    auto outSvg = std::filesystem::temp_directory_path() / "hsbaslicer_out.svg";
    std::error_code ec;
    std::filesystem::remove(outPng, ec);
    std::filesystem::remove(outSvg, ec);
    bool ok1 = ToImage(simple, 100, 100, 1.0, outPng.string());
    bool ok2 = ToImage(simple, 100, 100, 1.0, outSvg.string());
    BOOST_CHECK(ok1);
    BOOST_CHECK(ok2);
    BOOST_CHECK(std::filesystem::exists(outPng));
    BOOST_CHECK(std::filesystem::exists(outSvg));
    std::filesystem::remove(tmpPath, ec);
    std::filesystem::remove(outPng, ec);
    std::filesystem::remove(outSvg, ec);
#else
    BOOST_TEST_MESSAGE("OpenCV not available, skipping fromimage_and_toimage_roundtrip test");
#endif
}

BOOST_AUTO_TEST_CASE(to_image_normalizes_polygon_coordinates)
{
    PolygonsD poly;
    PolygonD p;
    p.emplace_back(Point2D{100.0, 200.0});
    p.emplace_back(Point2D{120.0, 200.0});
    p.emplace_back(Point2D{120.0, 220.0});
    p.emplace_back(Point2D{100.0, 220.0});
    poly.push_back(p);

    auto outSvg = std::filesystem::temp_directory_path() / "hsbaslicer_translate.svg";
    bool ok = ToImage(poly, 100, 100, 1.0, outSvg.string());
    BOOST_CHECK(ok);

    std::ifstream ifs(outSvg);
    std::string svg((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    BOOST_CHECK(svg.find("0.5,0.5") != std::string::npos);
    BOOST_CHECK(svg.find("100.5,100.5") == std::string::npos);

    std::error_code ec;
    std::filesystem::remove(outSvg, ec);
}

BOOST_AUTO_TEST_CASE(lua_to_image)
{
    // create a simple polygon
    PolygonsD poly;
    PolygonD p;
    p.emplace_back(Point2D{10.0, 10.0});
    p.emplace_back(Point2D{30.0, 10.0});
    p.emplace_back(Point2D{30.0, 30.0});
    p.emplace_back(Point2D{10.0, 30.0});
    poly.push_back(p);

    // path to Lua script that generates an image from polygons
    std::filesystem::path script_path = std::filesystem::path(__FILE__).parent_path() / "image_from_polygons.lua";
    std::string scriptPath = script_path.string();
    auto outPath = std::filesystem::temp_directory_path() / "hsbaslicer_lua_out.png";

    // call LuaToImage
    bool ok = LuaToImage(poly, scriptPath, outPath.string());
    BOOST_CHECK(ok);
    BOOST_CHECK(std::filesystem::exists(outPath));

    // cleanup
    std::error_code ec;
    std::filesystem::remove(outPath, ec);

    const char* kLuaCode = R"(
function rasterize(poly)
    local W, H = 128, 128
    local img = {}
    -- 简化的“全黑正方形”
    for y = 0, H - 1 do
        for x = 0, W - 1 do
            if x >= 32 and x < 96 and y >= 32 and y < 96 then
                table.insert(img, 0)
            else
                table.insert(img, 255)
            end
        end
    end
    return img
end
)";
    ok = LuaToImageString(poly, kLuaCode, outPath.string(), "rasterize");
    BOOST_CHECK(ok);
    BOOST_CHECK(std::filesystem::exists(outPath));

    // cleanup
    std::filesystem::remove(outPath, ec);
}

namespace
{
PolygonsD OneSquare()
{
    PolygonsD poly;
    PolygonD p;
    p.emplace_back(Point2D{10.0, 10.0});
    p.emplace_back(Point2D{30.0, 10.0});
    p.emplace_back(Point2D{30.0, 30.0});
    p.emplace_back(Point2D{10.0, 30.0});
    poly.push_back(p);
    return poly;
}
}  // namespace

// A missing image path makes LoadImageGray fail, so FromImage / FromImageMulti
// return empty layers instead of throwing.
BOOST_AUTO_TEST_CASE(fromimage_missing_input)
{
    BOOST_CHECK(FromImage("hsbaslicer_no_such_image.png", 128, 1.0).empty());
    BOOST_CHECK(FromImageMulti("hsbaslicer_no_such_image.png", {128, 200}, 1.0).empty());
}

// ToImage rejects non-positive dimensions, an empty polygon set (no bounds) and an
// unwritable output directory (the SVG stream cannot be opened).
BOOST_AUTO_TEST_CASE(toimage_input_guards)
{
    const PolygonsD poly = OneSquare();
    BOOST_CHECK(!ToImage(poly, 0, 100, 1.0, "hsbaslicer_zero.png"));
    BOOST_CHECK(!ToImage(poly, 100, -5, 1.0, "hsbaslicer_neg.png"));

    PolygonsD none;
    BOOST_CHECK(!ToImage(none, 100, 100, 1.0, "hsbaslicer_empty.png"));

    BOOST_CHECK(!ToImage(poly, 100, 100, 1.0, "hsbaslicer_no_such_dir/out.svg"));
}

// An empty ring mixed into the set is skipped in both the SVG and raster paths
// while the valid ring still produces a file.
BOOST_AUTO_TEST_CASE(toimage_skips_empty_polygons)
{
    PolygonsD poly;
    poly.push_back(PolygonD{}); // empty -> skipped
    poly.push_back(OneSquare()[0]);

    auto outSvg = std::filesystem::temp_directory_path() / "hsbaslicer_skip.svg";
    BOOST_CHECK(ToImage(poly, 100, 100, 1.0, outSvg.string()));
    std::error_code ec;
    std::filesystem::remove(outSvg, ec);

#ifdef HAS_OPENCV
    auto outPng = std::filesystem::temp_directory_path() / "hsbaslicer_skip.png";
    BOOST_CHECK(ToImage(poly, 100, 100, 1.0, outPng.string()));
    std::filesystem::remove(outPng, ec);
#endif
}

#ifdef HAS_OPENCV
// A multi-channel (BGR) image exercises the cvtColor conversion, and a block that
// touches the image border drives the out-of-range neighbour guard in the BFS.
BOOST_AUTO_TEST_CASE(fromimage_color_and_border_component)
{
    int w = 60, h = 60;
    cv::Mat bgr(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(bgr, cv::Rect(0, 5, 15, 15), cv::Scalar(255, 255, 255), cv::FILLED);

    auto path = std::filesystem::temp_directory_path() / "hsbaslicer_color.png";
    cv::imwrite(path.string(), bgr);

    PolygonsD polys = FromImage(path.string(), 128, 1.0);
    BOOST_CHECK(!polys.empty());

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
#endif

// Every failure branch of the Lua-driven renderers must surface as a RuntimeError,
// while the registration callback is invoked and an empty output path short-circuits.
BOOST_AUTO_TEST_CASE(lua_to_image_error_and_callback_paths)
{
    const PolygonsD poly = OneSquare();

    // LuaToImage: nonexistent script file.
    BOOST_CHECK_THROW(LuaToImage(poly, "hsbaslicer_missing_script.lua", "hsbaslicer_out.png"), RuntimeError);
    // LuaToImage: script loads but the requested function is absent.
    std::filesystem::path script = std::filesystem::path(__FILE__).parent_path() / "image_from_polygons.lua";
    BOOST_CHECK_THROW(LuaToImage(poly, script.string(), "hsbaslicer_out.png", "no_such_function"), RuntimeError);
    // LuaToImage: valid function but empty output path -> false, no throw.
    BOOST_CHECK(!LuaToImage(poly, script.string(), "", "generate_image"));
    // LuaToImage: registration callback is invoked on the happy path.
    auto outImg = std::filesystem::temp_directory_path() / "hsbaslicer_lua_reg.png";
    bool regCalled = false;
    BOOST_CHECK(LuaToImage(poly, script.string(), outImg.string(), "generate_image",
                           [&regCalled](lua_State*) { regCalled = true; }));
    BOOST_CHECK(regCalled);
    std::error_code ec;
    std::filesystem::remove(outImg, ec);

    // LuaToImageString failure modes.
    BOOST_CHECK_THROW(LuaToImageString(poly, "not valid lua @@#", "o.png", "f"), RuntimeError);        // load fail
    BOOST_CHECK_THROW(LuaToImageString(poly, "error('boom')", "o.png", "f"), RuntimeError);            // chunk exec fail
    BOOST_CHECK_THROW(LuaToImageString(poly, "function g() end", "o.png", "f"), RuntimeError);         // function absent
    BOOST_CHECK_THROW(LuaToImageString(poly, "function f(p) error('x') end", "o.png", "f"), RuntimeError); // call error
    BOOST_CHECK_THROW(LuaToImageString(poly, "function f(p) return 42 end", "o.png", "f"), RuntimeError);   // non-table
    BOOST_CHECK_THROW(LuaToImageString(poly, "function f(p) return {1, 'x'} end", "o.png", "f"),
                      RuntimeError); // non-integer element
    BOOST_CHECK(!LuaToImageString(poly, "function f(p) return {0, 255} end", "", "f", [&regCalled](lua_State*) { regCalled = true; })); // empty out path
    BOOST_CHECK(regCalled); // LuaToImageString registration callback runs before the empty-path short-circuit
}

// The file-based LuaToImage mirrors LuaToImageString's error handling over a script
// loaded from disk: drive its call-error, non-table and non-integer failure branches.
BOOST_AUTO_TEST_CASE(lua_to_image_script_file_error_paths)
{
    const PolygonsD poly = OneSquare();
    auto tmp = std::filesystem::temp_directory_path();
    auto write = [&](const std::string& name, const std::string& body) -> std::string
    {
        std::ofstream(tmp / name).write(body.data(), static_cast<std::streamsize>(body.size()));
        return (tmp / name).string();
    };

    const std::string callErr = write("hsba_call_err.lua", "function generate_image(p) error('boom') end");
    const std::string nonTable = write("hsba_nontable.lua", "function generate_image(p) return 5 end");
    const std::string nonInt = write("hsba_nonint.lua", "function generate_image(p) return {1, 'x'} end");

    BOOST_CHECK_THROW(LuaToImage(poly, callErr, "hsba_out.png", "generate_image"), RuntimeError);
    BOOST_CHECK_THROW(LuaToImage(poly, nonTable, "hsba_out.png", "generate_image"), RuntimeError);
    BOOST_CHECK_THROW(LuaToImage(poly, nonInt, "hsba_out.png", "generate_image"), RuntimeError);

    std::error_code ec;
    std::filesystem::remove(callErr, ec);
    std::filesystem::remove(nonTable, ec);
    std::filesystem::remove(nonInt, ec);
    std::filesystem::remove("hsba_out.png", ec);
}
