#pragma once

#include <ExampleApplication.h>

#include <Vex.h>

class GLFWwindow;

class HelloTriangleApplication : public ExampleApplication
{
public:
    HelloTriangleApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight) override;

private:
    vex::Texture workingTexture;
    vex::Texture finalOutputTexture;
    vex::Texture uvGuideTexture;

    vex::Buffer colorBuffer;
    vex::Buffer commBuffer;
};