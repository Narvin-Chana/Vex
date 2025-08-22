#pragma once

#include <ExampleApplication.h>

class HelloRayTracing : public ExampleApplication
{
public:
    HelloRayTracing();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight) override;

private:
    vex::Texture workingTexture;
};