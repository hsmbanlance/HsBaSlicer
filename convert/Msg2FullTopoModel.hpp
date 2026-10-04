/** @file Msg2FullTopoModel.hpp
 * @brief Converter from a HsbaProto msg_topo_trimeshes message to a FullTopoModel.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_MSG2FULLTOPOMODEL_HPP
#define HSBA_SLICER_MSG2FULLTOPOMODEL_HPP

#include <array>
#include <vector>

#include "mesh.pb.h"
#include "meshmodel/FullTopoModel.hpp"

namespace HsBa::Slicer
{

/// @brief Rebuild a FullTopoModel from a msg_topo_trimeshes protobuf message.
/// @param msg Source protobuf message holding the triangulated meshes.
/// @param use_normals Whether to reuse per-vertex normals from the message.
/// @return The reconstructed FullTopoModel.
FullTopoModel MsgTopoTrimeshes2FullTopoModel(const HsbaProto::msg_topo_trimeshes& msg, bool use_normals = false);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MSG2FULLTOPOMODEL_HPP
