#pragma once

#include <ExampleApplication.h>

struct RenderDocApplication : public ExampleApplication
{
    RenderDocApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;
};