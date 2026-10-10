/** @file EventSourceFunction.cpp
 * @brief Implementation of Zipper and database event-source callback registration.
 * @author HsBa
 */
#include "EventSourceFunction.hpp"

namespace HsBa::Slicer
{
namespace
{
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) module callback registry
std::vector<ZipperEventCallback> zipper_event_callbacks;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) module callback registry
std::vector<DBEventCallback> db_event_callbacks;
}  // namespace

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