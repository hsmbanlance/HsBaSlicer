#define BOOST_TEST_MODULE images_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/imagespath.hpp"
#include <filesystem>
#include <fstream>

using namespace HsBa::Slicer;

struct DisableCrt
{
    DisableCrt()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF);
#endif // defined(_MSC_VER) && defined(_DEBUG)
    }
};

BOOST_AUTO_TEST_SUITE(images_path_test)


BOOST_AUTO_TEST_CASE(test_to_string_with_encoding)
{
    [[maybe_unused]]
    static DisableCrt crt_;
    // create ImagesPath with dummy config
    ImagesPath ip("cfgfile", "{}", [](double, std::string_view){});
    // add two small images as base64 strings ("abc" -> "YWJj", bytes {0x01,0x02} -> "AQI=")
    std::string img1 = "YWJj"; // base64 of "abc"
    std::string img2 = "AQI="; // base64 of bytes 0x01,0x02
    ip.AddImage("img1.png", img1);
    ip.AddImage("img2.bin", img2);

    // script to decode base64 images and return combined string
    std::string script = R"lua(
local out = {}
for i=1, #images do
  local it = images[i]
  local dec = Cipher.base64_decode(it.data)
  table.insert(out, "#" .. it.path)
  table.insert(out, dec)
end
return table.concat(out, "\n")
)lua";

    auto out = ip.ToString(script);
    std::cout << "Images ToString(script decoded) =>\n" << out << "\n";
    // Expect decoded payload 'abc' to appear
    BOOST_CHECK_NE(out.find("abc"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_save_creates_file)
{
    ImagesPath ip("cfgfile", "{}", [](double rate, std::string_view path){
        std::cout << "Callback: " << rate << "%, " << path << "\n";
    });
    std::string img = "eA=="; // base64 of 'x'
    ip.AddImage("one.png", img);

    auto tmp = std::filesystem::temp_directory_path() / "images_out_test.zip";
    std::error_code ec; std::filesystem::remove(tmp, ec);

    // script that creates a zip using the provided zipper and writes to output_path
    std::string script = R"lua(
local z = Zipper.new()
Zipper.AddByteFile(z,"one.png", Cipher.base64_decode(images[1].data))
Zipper.Save(z,output_path)
return output_path
)lua";

    ip.Save(tmp, script);
    BOOST_CHECK(std::filesystem::exists(tmp));

    // cleanup
    std::filesystem::remove(tmp, ec);
}

BOOST_AUTO_TEST_SUITE_END()
