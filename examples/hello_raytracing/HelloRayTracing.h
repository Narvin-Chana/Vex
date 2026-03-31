#pragma once

#include <ExampleApplication.h>

class HelloRayTracing : public ExampleApplication
{
public:
    HelloRayTracing();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, int newWidth, int newHeight) override;

private:
    vex::Texture workingTexture;
    vex::AccelerationStructure triangleBLAS;
    vex::AccelerationStructure tlas;
};