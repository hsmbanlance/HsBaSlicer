﻿#pragma once
#ifndef HSBA_SLICER_MESH_SLICE_HPP
#define HSBA_SLICER_MESH_SLICE_HPP

namespace HsBa::Slicer
{
	//Z方向平面切片，在层间路径规划不干涉的情况下可以考虑在一个协程内处理一层的路径

	//安全切片，忽略不封闭轮廓
	Polygons Slice(const IModel& model, const float height);
	//不安全的切片，包含不封闭轮廓。如果需要封闭的轮廓，请使用Slice。
	//在送丝的工艺下可以考虑使用不安全切片，使用SLA等面成型工艺时不考虑使用
	UnSafePolygons UnSafeSlice(const IModel& model, const float height);
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_MESH_SLICE_HPP
