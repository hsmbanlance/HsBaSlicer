/** @file FullTopoModel2Msg.hpp
 * @brief Converter from a FullTopoModel to a HsbaProto msg_topo_trimeshes message.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_FULLTOPOMODEL2MSG_HPP
#define HSBA_SLICER_FULLTOPOMODEL2MSG_HPP

#include "mesh.pb.h"
#include "meshmodel/FullTopoModel.hpp"

namespace HsBa::Slicer
{

/// @brief Serialize a FullTopoModel into a msg_topo_trimeshes protobuf message.
void FullTopoModel2Msg(const FullTopoModel& model, HsbaProto::msg_topo_trimeshes* msg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_FULLTOPOMODEL2MSG_HPP
