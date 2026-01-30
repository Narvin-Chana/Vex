#pragma once

#include <ExampleApplication.h>

struct PixEventsApplication : public ExampleApplication
{
    PixEventsApplication();
    ~PixEventsApplication();
    void Run();

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;
};