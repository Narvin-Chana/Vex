#pragma once

#include <ExampleApplication.h>

class GLFWwindow;

class HelloMeshCubeApplication : public ExampleApplication
{
public:
    HelloMeshCubeApplication();
    void Run();

protected:
    virtual void OnResize(int width, int height) override;

private:
    vex::Texture depthTexture;

    vex::Buffer vertexBuffer;
    vex::Buffer indexBuffer;
};