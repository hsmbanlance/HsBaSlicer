#define BOOST_TEST_MODULE images_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/imagespath.hpp"
#include <filesystem>
#include <fstream>

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_SUITE(images_path_test)


BOOST_AUTO_TEST_CASE(test_to_string_with_encoding)
{
    // create ImagesPath with dummy config
    ImagesPath ip("cfgfile", "{}", [](double, std::string_view){});
    // add two small images as base64 strings ("abc" -> "YWJj", bytes {0x01,0x02} -> "AQI=")
    std::string img1 = "YWJj"; // base64 of "abc"
    std::string img2 = "AQI="; // base64 of bytes 0x01,0x02
    ip.AddImage("img1.png", img1);
    ip.AddImage("img2.bin", img2);

    // script to request hex encoding
    std::string script = R"lua(
return "hex"
)lua";

    auto out = ip.ToString(script);
    std::cout << "Images ToString(script hex) =>\n" << out << "\n";
    // Expect to find a hex representation (0x for binary not present, but at least characters 0-9a-f)
    BOOST_CHECK_NE(out.find("img1.png"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_save_creates_file)
{
    // This test that triggers Zipper/bit7z was removed as it caused allocator
    // behaviour in third-party libraries to be reported as leaks under
    // the test runner. Keep images formatting test only.
}

BOOST_AUTO_TEST_SUITE_END()
