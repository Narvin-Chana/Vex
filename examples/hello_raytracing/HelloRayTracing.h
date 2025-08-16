#pragma once

#include <Vex.h>

#include "../ExampleApplication.h"

class GLFWwindow;

class HelloRayTracing : public ExampleApplication
{
public:
    HelloRayTracing();
    virtual void HandleKeyInput(int key, int scancode, int action, int mods) override;

    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight) override;

private:
    vex::Texture workingTexture;
};