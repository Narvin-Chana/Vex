#include <HelloTriangleApplication.h>

#include <iostream>

#include <Vex.h>

int main()
{
    std::cout << "DX12: " << VEX_DX12 << "\nVulkan: " << VEX_VULKAN;

    // TODO: make the world a triangle...
    HelloTriangleApplication application;
    application.Run();
}