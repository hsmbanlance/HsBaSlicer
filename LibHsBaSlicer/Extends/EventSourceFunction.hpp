#pragma once

#ifndef HSBA_SLICER_EVENTSOURCEFUNCTION_HPP
#define HSBA_SLICER_EVENTSOURCEFUNCTION_HPP

#include <functional>
#include <string>
#include <vector>

#include "LibHsBaSlicer/export.h"

namespace HsBa::Slicer
{
	using ZipperEventCallback = std::function<void(double, std::string_view)>;
	using DBEventCallback = std::function<void(std::string_view, std::string_view)>;

	/**
     * @brief Add a callback function for Zipper events.
     * @param func Callback function to be called on Zipper events.
     */
	HSBA_SLICER_LIB_API void AddZipperEventCallback(ZipperEventCallback func);
    /**
     * @brief Add a callback function for Database events.
     * @param func Callback function to be called on Database events.
     */
    HSBA_SLICER_LIB_API void AddDBEventCallback(DBEventCallback func);

    /**
     * @brief Get the list of registered Zipper event callbacks.
     * @return Reference to the vector of Zipper event callbacks.
     */
	HSBA_SLICER_LIB_API std::vector<ZipperEventCallback>& GetZipperEventCallback();
    /**
     * @brief Get the list of registered Database event callbacks.
     * @return Reference to the vector of Database event callbacks.
     */
    HSBA_SLICER_LIB_API std::vector<DBEventCallback>& GetDBEventCallback();
 }  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_EVENTSOURCEFUNCTION_HPP