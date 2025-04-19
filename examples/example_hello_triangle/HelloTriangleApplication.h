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

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;

private:
    vex::Texture workingTexture;
};