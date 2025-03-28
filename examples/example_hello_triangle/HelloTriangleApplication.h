#pragma once

#include <Vex.h>

class HelloTriangleApplication
{
public:
    HelloTriangleApplication();
    ~HelloTriangleApplication();
    void Run();

private:
    class GLFWwindow* window;
    vex::UniqueHandle<vex::GfxBackend> graphics;
};