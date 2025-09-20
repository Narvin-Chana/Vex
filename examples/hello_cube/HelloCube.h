#pragma once

#include <ExampleApplication.h>

class GLFWwindow;

class HelloCubeApplication : public ExampleApplication
{
public:
    HelloCubeApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;

private:
    vex::Texture depthTexture;
    vex::Texture uvGuideTexture;

    vex::Buffer vertexBuffer;
    vex::Buffer indexBuffer;
};