#pragma once

#include <ExampleApplication.h>

struct PixEventsApplication : public ExampleApplication
{
    PixEventsApplication();
    ~PixEventsApplication();
    void Run();

protected:
    virtual void OnResize(int width, int height) override;
};