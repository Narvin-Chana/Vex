#pragma once

#include <Vex.h>

#include "../ExampleApplication.h"

class GLFWwindow;

class HelloTriangleApplication : public ExampleApplication
{
public:
    HelloTriangleApplication();
    virtual ~HelloTriangleApplication() override;
    virtual void HandleKeyInput(int key, int scancode, int action, int mods) override;

    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;

private:
    vex::Texture workingTexture;
    vex::Texture finalOutputTexture;

    vex::Buffer colorBuffer;
    vex::Buffer commBuffer;
};