#define BOOST_TEST_MODULE ImagePolygonsTests
#include <boost/test/included/unit_test.hpp>

#include "../../2D/ImageToPolygons.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(fromimage_and_toimage_roundtrip)
{
    // create a simple synthetic image (two rectangles with different intensities)
    int w = 80, h = 60;
    cv::Mat img(h, w, CV_8UC1, cv::Scalar(0));
    cv::rectangle(img, cv::Rect(5, 5, 30, 20), cv::Scalar(200), cv::FILLED);
    cv::rectangle(img, cv::Rect(40, 20, 30, 25), cv::Scalar(120), cv::FILLED);

    auto tmpPath = std::filesystem::temp_directory_path() / "hsbaslicer_test_img.png";
    cv::imwrite(tmpPath.string(), img);

    // single threshold (should detect the brighter rectangle)
    PolygonsD polys = FromImage(tmpPath.string(), 180, 1.0);
    BOOST_CHECK(!polys.empty());

    // multi-threshold: detect both layers
    std::vector<int> thresholds = {100, 180};
    auto layers = FromImageMulti(tmpPath.string(), thresholds, 1.0);
    BOOST_CHECK_EQUAL(layers.size(), 2u);
    BOOST_CHECK(!layers[0].empty());
    BOOST_CHECK(!layers[1].empty());

    // test ToImage: write PNG and SVG
    PolygonsD simple;
    // one rectangle
    PolygonD rect;
    rect.emplace_back(Point2D{10.0, 10.0}); rect.emplace_back(Point2D{30.0, 10.0}); rect.emplace_back(Point2D{30.0, 30.0}); rect.emplace_back(Point2D{10.0, 30.0});
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
}
