#pragma once

#include <ExampleApplication.h>

class GLFWwindow;

class HelloTriangleGraphicsApplication : public ExampleApplication
{
public:
    HelloTriangleGraphicsApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;

private:
    vex::Texture workingTexture;
    vex::Buffer colorBuffer;
};