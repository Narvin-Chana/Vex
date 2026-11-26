#pragma once

#include <Vex/CommandContext.h>
#include <Vex/Graphics.h>

namespace vex
{

template <class T, void (Graphics::*DestroyFn)(const T&)>
    requires(std::is_same_v<T, AccelerationStructure> || std::is_same_v<T, Buffer> || std::is_same_v<T, Texture>)
class UniqueResource
{
public:
    UniqueResource() = default;
    UniqueResource(Graphics& graphics, T resource)
        : graphics(&graphics)
        , resource(std::move(resource))
    {
    }
    UniqueResource(UniqueResource&& other)
        : graphics(other.graphics)
        , resource(std::exchange(other.resource, {}))
    {
    }
    UniqueResource& operator=(UniqueResource&& other)
    {
        if (this != &other)
        {
            Reset();
            graphics = other.graphics;
            resource = std::exchange(other.resource, {});
        }
        return *this;
    }
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
    ~UniqueResource()
    {
        Reset();
    }

    void Reset()
    {
        if (IsValid())
        {
            (graphics->*DestroyFn)(std::move(resource));
        }
        resource = T{};
    }

    void Reset(CommandContext& commandContext)
    {
        if (IsValid())
        {
            commandContext.DestroyResourceAfterExecution(std::move(resource));
        }
        resource = T{};
    }

    [[nodiscard]] const T& Get() const
    {
        return resource;
    }
    const T* operator->() const
    {
        return &resource;
    }

    [[nodiscard]] bool IsValid() const
    {
        return graphics && resource.handle.IsValid();
    }

private:
    Graphics* graphics = nullptr;
    T resource{};
};

using UniqueAccelerationStructure = UniqueResource<AccelerationStructure, &Graphics::DestroyAccelerationStructure>;
using UniqueBuffer = UniqueResource<Buffer, &Graphics::DestroyBuffer>;
using UniqueTexture = UniqueResource<Texture, &Graphics::DestroyTexture>;

} // namespace vex