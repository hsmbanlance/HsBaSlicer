/** @file upload_bridge.hpp
 * @brief Name sanitizing, path rendering and config injection for uploaded model files.
 * @author HsBa
 *
 * Deliberately free of any HTTP dependency so this translation unit can be
 * compiled straight into the unit tests (the same approach as config_bridge).
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_UPLOAD_BRIDGE_HPP
#define HSBA_SLICER_RESTAPI_UPLOAD_BRIDGE_HPP

#include <string>

#include "base/any_object.hpp"
#include "fileoperator/param_convert.hpp"

namespace HsBa::Slicer::RestApi
{
/** @brief Longest accepted file name, in bytes, after sanitizing. */
constexpr std::size_t kUploadNameMaxBytes = 128;

/** @brief File name used when the client supplies nothing usable. */
constexpr const char* kUploadFallbackName = "model.bin";

/**
 * @brief Reduce a client-supplied file name to a safe single path component.
 *
 * Drops any directory component (clients may send full Windows or POSIX paths),
 * keeps only [A-Za-z0-9._-], rejects the "." / ".." special names, truncates to
 * kUploadNameMaxBytes and falls back to kUploadFallbackName when nothing survives.
 *
 * @param client_name Raw file name as received in the multipart part.
 * @return A single path component that is safe to append to a directory.
 */
std::string SanitizeUploadName(const std::string& client_name);

/**
 * @brief Join the upload root, the owning job id and the sanitized file name.
 * @param upload_dir Upload root from [upload] dir (relative to the working directory allowed).
 * @param job_id     Id of the job the file belongs to ("job-<n>").
 * @param safe_name  Output of SanitizeUploadName.
 * @return upload_dir/job_id/safe_name using the platform separator.
 */
std::string RenderUploadPath(const std::string& upload_dir, const std::string& job_id, const std::string& safe_name);

/**
 * @brief Write the server-side model path into the config's reflected string fields.
 *
 * model_path is required (every slicing kind declares it); model_name receives the
 * client file name when the struct declares such a field. Both are copied into
 * @p arena, matching the ownership model of DeserializeConfig: the arena backs the
 * buffer and DestroyConfig only releases the struct itself, so the strings must
 * never be malloc'ed.
 *
 * @param cfgTi        Reflective TypeInfo* of the config struct.
 * @param cfg          Pointer to the config struct memory.
 * @param server_path  Where the uploaded file was stored.
 * @param display_name Client file name (may be empty).
 * @param arena        Backing storage; must outlive every use of @p cfg.
 * @return false when the struct has no model_path field (e.g. the file-transfer kind).
 */
bool BindUploadedModel(Utils::TypeInfo* cfgTi, void* cfg, const std::string& server_path,
                       const std::string& display_name, StringArena& arena);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_UPLOAD_BRIDGE_HPP
