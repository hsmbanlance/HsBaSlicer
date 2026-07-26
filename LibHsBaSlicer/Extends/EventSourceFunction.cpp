#include "EventSourceFunction.hpp"

namespace HsBa::Slicer
{
static std::vector<ZipperEventCallback> zipper_event_callbacks;
static std::vector<DBEventCallback> db_event_callbacks;
void AddZipperEventCallback(ZipperEventCallback func)
{
    zipper_event_callbacks.push_back(std::move(func));
}
void AddDBEventCallback(DBEventCallback func)
{
    db_event_callbacks.push_back(std::move(func));
}
std::vector<ZipperEventCallback>& GetZipperEventCallback()
{
    return zipper_event_callbacks;
}
std::vector<DBEventCallback>& GetDBEventCallback()
{
    return db_event_callbacks;
}
}  // namespace HsBa::Slicer