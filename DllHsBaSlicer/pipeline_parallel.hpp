/** @file pipeline_parallel.hpp
 * @brief Compatibility shim: the layer-parallel helpers now live in base/parallel.hpp.
 *
 * Kept so existing Dll pipeline sources can continue to `#include "pipeline_parallel.hpp"`
 * unchanged. New code should include "base/parallel.hpp" directly.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_PIPELINE_PARALLEL_HPP
#define HSBA_SLICER_PIPELINE_PARALLEL_HPP

#include "base/parallel.hpp"

#endif  // !HSBA_SLICER_PIPELINE_PARALLEL_HPP
