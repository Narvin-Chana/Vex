#pragma once

#include <Vex.h>

#include "../ExampleApplication.h"

class GLFWwindow;

class HelloTriangleApplication : public ExampleApplication
{
public:
    HelloTriangleApplication();
    virtual ~HelloTriangleApplication() override;
    void Run();
};