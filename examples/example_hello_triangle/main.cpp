#include <HelloTriangleApplication.h>

#include <iostream>

#include <Vex.h>

int main()
{
    // Testing flags:
    std::cout << "DX12: " << VEX_DX12 << "\nVulkan: " << VEX_VULKAN << '\n';
    std::cout << "Debug: " << VEX_DEBUG << "\nDevelopment: " << VEX_DEVELOPMENT << "\nShipping: " << VEX_SHIPPING
              << '\n';

    // TODO: make the world a triangle...
    HelloTriangleApplication application;
    application.Run();
}