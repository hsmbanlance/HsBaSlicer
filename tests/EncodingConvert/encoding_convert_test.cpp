#define BOOST_TEST_MODULE encoding_convert_test
#include <boost/test/included/unit_test.hpp>

#include "base/encoding_convert.hpp"

#include <string>

// Tests for the explicit-encoding conversions in base/encoding_convert.cpp.
// encoding_convert(str, from, to) is documented as "source encoding = from,
// target encoding = to". On desktop (boost.locale backend) it must therefore call
// boost::locale::conv::between(str, /*to=*/to, /*from=*/from); boost's `between`
// takes the destination encoding first. ISO-8859-1 <-> UTF-8 for 'é' gives a short,
// unambiguous, locale-independent golden pair to pin the direction down.
BOOST_AUTO_TEST_SUITE(encoding_convert)

// Identical source/target encodings must be a pure passthrough (no conversion run).
BOOST_AUTO_TEST_CASE(same_encoding_passthrough)
{
    const std::string text = "plain ascii 123";
    BOOST_CHECK_EQUAL(HsBa::Slicer::encoding_convert(text, "UTF-8", "UTF-8"), text);
}

// UTF-8 -> ISO-8859-1: 0xC3 0xA9 (é in UTF-8) collapses to the single byte 0xE9.
BOOST_AUTO_TEST_CASE(utf8_to_latin1)
{
    const std::string utf8 = std::string("\xC3\xA9", 2);
    const std::string expected_latin1 = std::string("\xE9", 1);
    BOOST_CHECK_EQUAL(HsBa::Slicer::encoding_convert(utf8, "UTF-8", "ISO-8859-1"), expected_latin1);
}

// ISO-8859-1 -> UTF-8: the reverse direction expands 0xE9 back to 0xC3 0xA9.
BOOST_AUTO_TEST_CASE(latin1_to_utf8)
{
    const std::string latin1 = std::string("\xE9", 1);
    const std::string expected_utf8 = std::string("\xC3\xA9", 2);
    BOOST_CHECK_EQUAL(HsBa::Slicer::encoding_convert(latin1, "ISO-8859-1", "UTF-8"), expected_utf8);
}

// Round-tripping a string through both directions must be lossless for characters
// present in both encodings; this also fails if the direction is inverted.
BOOST_AUTO_TEST_CASE(round_trip_is_lossless)
{
    const std::string original = std::string("caf\xC3\xA9", 5); // "café" in UTF-8
    const std::string latin1 = HsBa::Slicer::encoding_convert(original, "UTF-8", "ISO-8859-1");
    const std::string back = HsBa::Slicer::encoding_convert(latin1, "ISO-8859-1", "UTF-8");
    BOOST_CHECK_EQUAL(back, original);
}

// utf8_to_local / local_to_utf8 depend on the runtime system locale, so only pure
// ASCII has a locale-independent, guaranteed-identical result (ASCII is a subset of
// UTF-8 and of every ASCII-compatible locale encoding). These cases still execute
// the full conversion bodies plus the internal SystemLocaleStr() helper.
BOOST_AUTO_TEST_CASE(ascii_survives_local_round_trip)
{
    const std::string ascii = "report_2024.stl";
    BOOST_CHECK_EQUAL(HsBa::Slicer::utf8_to_local(ascii), ascii);
    BOOST_CHECK_EQUAL(HsBa::Slicer::local_to_utf8(ascii), ascii);
    BOOST_CHECK_EQUAL(HsBa::Slicer::local_to_utf8(HsBa::Slicer::utf8_to_local(ascii)), ascii);
}

BOOST_AUTO_TEST_SUITE_END()
