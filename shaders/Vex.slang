module Vex;

// All texture and buffer handles are defined in descriptor set 0.
[vk::binding(0, 1)]
__DynamicResource<__DynamicResourceKind.General> resourceHandles[];

export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle) where T : IOpaqueDescriptor
{
    __target_switch
    {
    case spirv:
        return resourceHandles[((uint2)handle).x].asOpaqueDescriptor<T>();
    default:
        return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions.VkMutable);
    }
}

__generic<T>
public DescriptorHandle<T> GetBindlessResource(uint index) where T : IOpaqueDescriptor
{
    return { index & 0x00FFFFFF };
}

static const uint GInvalidBindlessHandle = ~0U;
