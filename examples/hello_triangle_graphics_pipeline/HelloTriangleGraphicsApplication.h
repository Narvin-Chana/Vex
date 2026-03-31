#pragma once

#include <ExampleApplication.h>

class GLFWwindow;

class HelloTriangleGraphicsApplication : public ExampleApplication
{
public:
    HelloTriangleGraphicsApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, int width, int height) override;

private:
    vex::Texture workingTexture;
    vex::Buffer colorBuffer;
};