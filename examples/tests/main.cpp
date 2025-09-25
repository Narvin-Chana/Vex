#include <ExampleApplication.h>
#include <GLFWIncludes.h>
#include <Tests/SynchronizationTorture.h>
#include <Tests/TextureUploadReadback.h>

#include <Vex/GfxBackend.h>

// TODO: Add start and end capture once we have render doc natively setup for better analysis of results.
// We use a stub window with present for now. Would be cleaner as a windowless app
// See: https://trello.com/c/eGi09y1Y
struct TestExample final : public ExampleApplication
{
    explicit TestExample()
        : ExampleApplication("TestExample")
    {
#if defined(_WIN32)
        vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
        vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif
        vex::GfxBackend graphics = vex::GfxBackend(vex::BackendDescription{
            .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
            .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
            .enableGPUDebugLayer = !VEX_SHIPPING,
            .enableGPUBasedValidation = !VEX_SHIPPING });

        vex::TextureUploadDownladTests(graphics);
        vex::SynchronizationTortureTest(graphics);

        // TODO: We need to clear before presenting if we havent touched the present texture before present on DX12
        // See: https://trello.com/c/k8hUPouM
        vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0.2f, 0.2f, 0.2f, 1 } };

        graphics.BeginScopedCommandContext(vex::CommandQueueType::Graphics, vex::SubmissionPolicy::Immediate)
            .ClearTexture(
                vex::TextureBinding{
                    .texture = graphics.GetCurrentPresentTexture(),
                },
                clearValue);

        graphics.Present(false);
    }
};

int main()
{
    TestExample example;
}