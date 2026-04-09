#pragma once

#include <ExampleApplication.h>

struct RenderDocApplication : public ExampleApplication
{
    RenderDocApplication();
    ~RenderDocApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, int width, int height) override;
};