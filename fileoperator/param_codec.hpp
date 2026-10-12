/** @file param_codec.hpp
 * @brief Compact self-describing binary codec for a reflected column map.
 *
 * The ParamStore at-rest encryption path serializes the whole wide-table column map
 * (name -> converged std::any) into a single binary buffer before encrypting it into the
 * dedicated param_blob column; the reverse buffer is decoded back into the column map on
 * Load. Only the ISQLAdapter whitelist scalar kinds produced by FieldToAny are supported:
 * nullptr / int64_t / double / std::string.
 */
#pragma once
#ifndef HSBA_SLICER_PARAM_CODEC_HPP
#define HSBA_SLICER_PARAM_CODEC_HPP

#include <any>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace HsBa::Slicer
{
/**
 * @brief Serialize a reflected column map into a self-describing little-endian binary buffer.
 * @param columns Column name -> converged whitelist std::any (nullptr / int64_t / double / string).
 * @return Raw bytes: [magic "HSB1"][uint32 count]{[uint32 name_len][name][uint8 tag][payload]}...
 * @throws SQL::SQLAdapterInvalidArgumentError when a value holds an unsupported type.
 */
std::vector<unsigned char> EncodeColumns(const std::unordered_map<std::string, std::any>& columns);

/**
 * @brief Reverse of EncodeColumns: rebuild the column map from a binary buffer.
 * @param bytes Buffer produced by EncodeColumns.
 * @throws SQL::SQLAdapterInvalidArgumentError when the buffer is malformed or the magic mismatches.
 */
std::unordered_map<std::string, std::any> DecodeColumns(const std::vector<unsigned char>& bytes);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_CODEC_HPP
