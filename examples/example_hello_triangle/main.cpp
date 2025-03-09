#include <HelloTriangleApplication.h>
#include <Vex.h>

#ifndef VEX_DX12
#define VEX_DX12 0
#endif

#ifndef VEX_VULKAN
#define VEX_VULKAN 0
#endif

int main()
{
    // Really nice, glfw allows you to also use the standard console funcs.
    // No more OutputTextA!
    std::cout << "Hello... world?" << vex::func() << "\nDX12: " << VEX_DX12 << "\nVulkan: " << VEX_VULKAN;

    // TODO: make the world a triangle...
    HelloTriangleApplication application;
    application.Run();
}