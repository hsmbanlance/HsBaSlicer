#pragma once
#ifndef HSBA_SLICER_MSG2FULLTOPOMODEL_HPP
#define HSBA_SLICER_MSG2FULLTOPOMODEL_HPP

#include <array>
#include <vector>

#include "mesh.pb.h"
#include "meshmodel/FullTopoModel.hpp"

namespace HsBa::Slicer
{

FullTopoModel MsgTopoTrimeshes2FullTopoModel(const HsbaProto::msg_topo_trimeshes& msg, bool use_normals = false);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MSG2FULLTOPOMODEL_HPP
