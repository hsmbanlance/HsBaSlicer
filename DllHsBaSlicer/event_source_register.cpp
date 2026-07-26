#include "event_source_register.h"

#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp"

void HsBaAddZipperEventCallback(const char* event_name, void (*func)(double, const char*))
{
    HsBa::Slicer::AddZipperEventCallback(
        [event_name, func](double progress, std::string_view message)
        {
            if (std::string(event_name) == "zipper.on_add")
            {
                func(progress, message.data());
            }
        });
}

void HsBaAddDBEventCallback(const char* event_name, void (*func)(const char*, const char*))
{
    HsBa::Slicer::AddDBEventCallback(
        [event_name, func](std::string_view query, std::string_view result)
        {
            if (std::string(event_name) == "db.on_query")
            {
                func(query.data(), result.data());
            }
        });
}