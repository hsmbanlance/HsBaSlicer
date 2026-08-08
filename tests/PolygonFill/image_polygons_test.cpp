#define BOOST_TEST_MODULE ImagePolygonsTests
#include <boost/test/included/unit_test.hpp>

#include "../../2D/ImageToPolygons.hpp"
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
