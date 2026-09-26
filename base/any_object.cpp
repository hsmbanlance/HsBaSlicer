#include "any_object.hpp"

namespace HsBa::Slicer::Utils
{
AnyObject::AnyObject(const AnyObject& other) : type_info(other.type_info), data(nullptr), flag(other.flag)
{
    if (other.flag & 0b1)
    {
        data = type_info ? type_info->copy(other.data) : nullptr;
    }
    else
    {
        data = other.data;
    }
}

AnyObject::AnyObject(AnyObject&& other) noexcept : type_info(other.type_info), data(other.data), flag(other.flag)
{
    other.type_info = nullptr;
    other.data = nullptr;
    other.flag = 0;
}

AnyObject& AnyObject::operator=(const AnyObject& other)
{
    if (this != &other)
    {
        if ((flag & 0b1) && type_info && data)
        {
            type_info->destroy(data);
        }
        type_info = other.type_info;
        flag = other.flag;
        if (other.flag & 0b1)
        {
            data = type_info ? type_info->copy(other.data) : nullptr;
        }
        else
        {
            data = other.data;
        }
    }
    return *this;
}

AnyObject& AnyObject::operator=(AnyObject&& other) noexcept
{
    if (this != &other)
    {
        if ((flag & 0b1) && type_info && data)
        {
            type_info->destroy(data);
        }
        type_info = other.type_info;
        data = other.data;
        flag = other.flag;
        other.type_info = nullptr;
        other.data = nullptr;
        other.flag = 0;
    }
    return *this;
}

AnyObject::~AnyObject()
{
    if ((flag & 0b1) && type_info && data)
    {
        type_info->destroy(data);
    }
}

void AnyObject::ForeachField(const std::function<void(std::string_view, AnyObject)>& callback)
{
    if (!type_info)
        return;
    for (const auto& [name, field] : type_info->fields)
    {
        AnyObject any = AnyObject{field.first, static_cast<char*>(data) + field.second};
        callback(name, any);
    }
}
AnyObject AnyObject::Invoke(std::string_view method_name, std::span<AnyObject> args)
{
    if (!type_info)
        throw RuntimeError("Invoke on empty AnyObject");
#ifdef HSBA_ANY_OBJECT_ENABLE_MOCK
    // Mockit-like hook: when mocking is enabled and a stub matches, serve the
    // call from the registry instead of dispatching to the real method.
    {
        AnyObject mocked;
        if (Mock::MockRegistry::instance().try_invoke(type_info, method_name, data, args, mocked))
        {
            return mocked;
        }
    }
#endif
    auto it = type_info->methods.find(method_name);
    if (it == type_info->methods.end())
    {
        throw RuntimeError("Method not found: " + std::string(method_name));
    }
    return it->second(data, args);
}

}  // namespace HsBa::Slicer::Utils