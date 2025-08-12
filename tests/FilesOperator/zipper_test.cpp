#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#include "fileoperator/zipper.hpp"

#if _WIN32
#include <Windows.h>
#endif // _WIN32

#include <iostream>

void PrintZipLog(double rate, std::string_view filename)
{
	std::cout << "Zipper Log: " << filename << " - " << rate * 100.0 << "% compressed" << std::endl;
}

BOOST_AUTO_TEST_SUITE(zipper_check)

BOOST_AUTO_TEST_CASE(test_zipper)
{
	BOOST_TEST_MESSAGE("Running Zipper test");
	// Set console output to UTF-8 for Windows
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	if (std::filesystem::exists("test.zip")) {
		std::filesystem::remove("test.zip");
	}
	// Create a Zipper instance
	HsBa::Slicer::Zipper zipper(HsBa::Slicer::MinizCompression::Tight);
	zipper += PrintZipLog;
	// Add files to the zipper
	zipper.AddByteFile("test1.txt", "This is a test file 1.");
	zipper.AddByteFile("test2.txt", "This is a test file 2.");
	// Save the zipped file
	std::string zip_path = "test.zip";
	BOOST_REQUIRE_NO_THROW(zipper.Save(zip_path));
	// Check if the zip file was created
	BOOST_REQUIRE(std::filesystem::exists(zip_path));
	// Extract the zip file
	std::unordered_map<std::string, std::string> extracted_files;
	BOOST_REQUIRE_NO_THROW(extracted_files = HsBa::Slicer::MiniZExtractFileToBuffer(zip_path));
	// Check if the extracted files match the original files
	BOOST_REQUIRE_EQUAL(extracted_files.size(), 2);
	BOOST_REQUIRE_EQUAL(extracted_files["test1.txt"], "This is a test file 1.");
	BOOST_REQUIRE_EQUAL(extracted_files["test2.txt"], "This is a test file 2.");
	// Clean up
	std::filesystem::remove(zip_path);
	BOOST_TEST_MESSAGE("Zipper test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()