#pragma once

#include <ExampleApplication.h>

struct RenderDocApplication : public ExampleApplication
{
    RenderDocApplication();
    ~RenderDocApplication();
    void Run();

protected:
    virtual void OnResize(int width, int height) override;
};