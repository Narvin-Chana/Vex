#pragma once

#include <concepts>
#include <utility>
#include <variant>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

// Holds a native object that must stay alive until in-flight GPU work referencing it has completed.
class VkDeferredRelease
{
    using Object = std::variant<::vk::UniquePipeline,
                                ::vk::UniquePipelineLayout,
                                ::vk::UniqueSampler,
                                ::vk::UniqueDescriptorSet,
                                ::vk::UniqueDescriptorSetLayout>;

public:
    template <class T>
        requires std::constructible_from<Object, T&&>
    explicit VkDeferredRelease(T&& object)
        : object(std::forward<T>(object))
    {
    }

    void Release()
    {
        std::visit([](auto& handle) { handle.reset(); }, object);
    }

private:
    Object object;
};

} // namespace vex::vk
