#pragma once

#include <ExampleApplication.h>

class GLFWwindow;

class HelloTriangleApplication : public ExampleApplication
{
public:
    HelloTriangleApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, int newWidth, int newHeight) override;

private:
    vex::Texture workingTexture;

    vex::Buffer colorBuffer;
    vex::Buffer commBuffer;
};