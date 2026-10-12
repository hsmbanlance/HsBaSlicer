/** @file param_codec.cpp
 * @brief Implements the compact self-describing binary codec for reflected column maps.
 */
#include "param_codec.hpp"

#include <cstdint>
#include <cstring>

#include "sql_adapter.hpp"

namespace HsBa::Slicer
{
namespace
{
// Magic header identifying a ParamStore column buffer.
constexpr char kMagic[4] = {'H', 'S', 'B', '1'};

// Per-value type tags carried in the stream.
constexpr unsigned char kTagNull = 0;
constexpr unsigned char kTagInt64 = 1;
constexpr unsigned char kTagDouble = 2;
constexpr unsigned char kTagString = 3;

void PutU32(std::vector<unsigned char>& out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFFU));
    }
}

void PutU64(std::vector<unsigned char>& out, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
    {
        out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFFU));
    }
}

std::uint32_t GetU32(const unsigned char* p)
{
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
        v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
    }
    return v;
}

std::uint64_t GetU64(const unsigned char* p)
{
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return v;
}
}  // namespace

std::vector<unsigned char> EncodeColumns(const std::unordered_map<std::string, std::any>& columns)
{
    std::vector<unsigned char> out;
    out.insert(out.end(), std::begin(kMagic), std::end(kMagic));
    PutU32(out, static_cast<std::uint32_t>(columns.size()));

    for (const auto& [name, value] : columns)
    {
        PutU32(out, static_cast<std::uint32_t>(name.size()));
        out.insert(out.end(), name.begin(), name.end());

        if (!value.has_value() || value.type() == typeid(std::nullptr_t))
        {
            out.push_back(kTagNull);
            continue;
        }
        if (value.type() == typeid(int64_t))
        {
            out.push_back(kTagInt64);
            std::int64_t const v = std::any_cast<std::int64_t>(value);
            std::uint64_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            PutU64(out, bits);
            continue;
        }
        if (value.type() == typeid(double))
        {
            out.push_back(kTagDouble);
            double const v = std::any_cast<double>(value);
            std::uint64_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            PutU64(out, bits);
            continue;
        }
        if (value.type() == typeid(std::string))
        {
            const auto& s = std::any_cast<const std::string&>(value);
            out.push_back(kTagString);
            PutU32(out, static_cast<std::uint32_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
            continue;
        }
        throw SQL::SQLAdapterInvalidArgumentError("EncodeColumns: unsupported value type for column '" + name + "'");
    }
    return out;
}

std::unordered_map<std::string, std::any> DecodeColumns(const std::vector<unsigned char>& bytes)
{
    auto fail = [](const char* msg) { throw SQL::SQLAdapterInvalidArgumentError(std::string("DecodeColumns: ") + msg); };

    std::unordered_map<std::string, std::any> columns;
    // Minimum: 4 magic + 4 count.
    if (bytes.size() < 8)
    {
        fail("buffer too small");
    }
    if (std::memcmp(bytes.data(), kMagic, 4) != 0)
    {
        fail("magic mismatch");
    }

    size_t pos = 4;
    std::uint32_t const count = GetU32(bytes.data() + pos);
    pos += 4;

    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (pos + 4 > bytes.size())
        {
            fail("truncated name length");
        }
        std::uint32_t const name_len = GetU32(bytes.data() + pos);
        pos += 4;
        if (pos + name_len + 1 > bytes.size())
        {
            fail("truncated name");
        }
        std::string name(reinterpret_cast<const char*>(bytes.data() + pos), name_len);
        pos += name_len;

        unsigned char const tag = bytes[pos];
        ++pos;
        switch (tag)
        {
        case kTagNull:
            columns[std::move(name)] = std::any(nullptr);
            break;
        case kTagInt64:
        {
            if (pos + 8 > bytes.size())
            {
                fail("truncated int64");
            }
            std::uint64_t bits = GetU64(bytes.data() + pos);
            pos += 8;
            std::int64_t v = 0;
            std::memcpy(&v, &bits, sizeof(v));
            columns[std::move(name)] = std::any(v);
            break;
        }
        case kTagDouble:
        {
            if (pos + 8 > bytes.size())
            {
                fail("truncated double");
            }
            std::uint64_t bits = GetU64(bytes.data() + pos);
            pos += 8;
            double v = 0.0;
            std::memcpy(&v, &bits, sizeof(v));
            columns[std::move(name)] = std::any(v);
            break;
        }
        case kTagString:
        {
            if (pos + 4 > bytes.size())
            {
                fail("truncated string length");
            }
            std::uint32_t const str_len = GetU32(bytes.data() + pos);
            pos += 4;
            if (pos + str_len > bytes.size())
            {
                fail("truncated string body");
            }
            columns[std::move(name)] = std::string(reinterpret_cast<const char*>(bytes.data() + pos), str_len);
            pos += str_len;
            break;
        }
        default:
            fail("unknown value tag");
        }
    }
    return columns;
}

}  // namespace HsBa::Slicer
