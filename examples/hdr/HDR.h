#pragma once

#include <GLFWIncludes.h>

#include <ExampleApplication.h>

class HDRApplication : public ExampleApplication
{
public:
    HDRApplication();
    void Run();

    virtual void HandleKeyInput(int key, int scancode, int action, int mods) override;

protected:
    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;

private:
    vex::Texture hdrTexture;
    vex::ColorSpace preferredColorSpace = vex::ColorSpace::sRGB;

    bool logSwapChainColorSpace = false;
};