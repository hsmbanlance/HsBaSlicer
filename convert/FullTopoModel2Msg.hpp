#pragma once
#ifndef HSBA_SLICER_FULLTOPOMODEL2MSG_HPP
#define HSBA_SLICER_FULLTOPOMODEL2MSG_HPP

#include "mesh.pb.h"
#include "meshmodel/FullTopoModel.hpp"

namespace HsBa::Slicer
{

void FullTopoModel2Msg(const FullTopoModel& model, HsbaProto::msg_topo_trimeshes* msg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_FULLTOPOMODEL2MSG_HPP
