/** @file upload_bridge.cpp
 * @brief Implements the upload name/path helpers and the model_path injection; see upload_bridge.hpp.
 * @author HsBa
 */
#include "upload_bridge.hpp"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

namespace HsBa::Slicer::RestApi
{
namespace
{
// Only these characters survive sanitizing: an allowlist is used instead of a
// denylist so platform-specific separators, drive colons and shell metacharacters
// are all rejected without enumerating them.
bool IsSafeNameChar(char ch)
{
    const unsigned char uc = static_cast<unsigned char>(ch);
    return std::isalnum(uc) != 0 || ch == '.' || ch == '_' || ch == '-';
}

// Store a copy of @p text in the arena and point the reflected field at it.
void StoreString(const std::string& text, char* field, StringArena& arena)
{
    arena.push_back(std::make_unique<char[]>(text.size() + 1));
    std::memcpy(arena.back().get(), text.c_str(), text.size() + 1);
    *reinterpret_cast<const char**>(field) = arena.back().get();
}
}  // namespace

std::string SanitizeUploadName(const std::string& client_name)
{
    // Strip any directory component first: multipart clients occasionally send a
    // full Windows ("C:\\models\\part.stl") or POSIX path instead of a bare name.
    const std::size_t separator = client_name.find_last_of("/\\");
    const std::string base = (separator == std::string::npos) ? client_name : client_name.substr(separator + 1);

    std::string safe;
    safe.reserve(base.size());
    for (const char ch : base)
    {
        if (IsSafeNameChar(ch))
        {
            safe.push_back(ch);
        }
    }
    // "." and ".." are directory references, never usable file names.
    if (safe == "." || safe == "..")
    {
        safe.clear();
    }
    if (safe.size() > kUploadNameMaxBytes)
    {
        safe.resize(kUploadNameMaxBytes);
    }
    return safe.empty() ? std::string(kUploadFallbackName) : safe;
}

std::string RenderUploadPath(const std::string& upload_dir, const std::string& job_id, const std::string& safe_name)
{
    std::filesystem::path path = std::filesystem::u8path(upload_dir);
    path /= std::filesystem::u8path(job_id);
    path /= std::filesystem::u8path(safe_name);
    // path::u8string() yields std::u8string under C++20; the REST layer keeps plain
    // std::string throughout, so convert explicitly instead of relying on the locale.
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

bool BindUploadedModel(Utils::TypeInfo* cfgTi, void* cfg, const std::string& server_path,
                       const std::string& display_name, StringArena& arena)
{
    if (!cfgTi || !cfg)
    {
        return false;
    }

    bool has_model_path = false;
    for (const auto& [name, field] : cfgTi->fields)
    {
        if (field.first != Utils::GetTypeInfo<const char*>())
        {
            continue;
        }
        if (name == "model_path")
        {
            StoreString(server_path, static_cast<char*>(cfg) + field.second, arena);
            has_model_path = true;
        }
        else if (name == "model_name")
        {
            StoreString(display_name, static_cast<char*>(cfg) + field.second, arena);
        }
    }
    return has_model_path;
}

}  // namespace HsBa::Slicer::RestApi
