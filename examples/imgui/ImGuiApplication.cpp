// TODO: Get this working with modules (secondary VexImgui include doesn't currently support them, and is the first case
// of requiring access to raw RHI types... will we have to export them as well?? preferably not...)
#undef VEX_MODULES
#define VEX_MODULES 0
#include "ImGuiApplication.h"

#include <GLFWIncludes.h>
#include <VexImgui.h>
#include <imgui_impl_glfw.h>

ImGuiApplication::ImGuiApplication()
    : ExampleApplication("ImGuiApplication")
{
    graphics = std::make_unique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = GetPlatformWindowHandle(), .width = DefaultWidth, .height = DefaultHeight },
        .useSwapChain = true,
        .swapChainDesc = { .frameBuffering = FrameBuffering },
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    lastFrameTexture = graphics->CreateTexture(vex::TextureDesc::CreateTexture2DDesc("PrevFrame",
                                                                                     vex::TextureFormat::BGRA8_UNORM,
                                                                                     DefaultWidth,
                                                                                     DefaultHeight));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOther(window, true);

    ImGui_ImplVex_InitInfo initInfo{
        .graphics = vex::NonNullPtr{ graphics.get() },
        .buffering = FrameBuffering,
        .swapchainFormat = SwapchainFormat,
    };
    ImGui_ImplVex_Init(initInfo);
}

ImGuiApplication::~ImGuiApplication()
{
    ImGui_ImplVex_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        vex::Texture presentTexture = graphics->GetCurrentPresentTexture();

        auto ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

        graphics->Submit(ctx);

        RenderImGui();

        graphics->Present();
    }
}
void ImGuiApplication::RenderImGui()
{
    ImGui_ImplVex_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Call all user imgui calls
    ImGui::ShowDemoWindow();

    if (ImGui::Begin("Last Frame"))
    {
        ImGui::Image(lastFrameTexture, ImVec2(100, 100));
    }
    ImGui::End();

    // Render resolves all internal ImGui code.
    // It does not touch the graphics API at all.
    ImGui::Render();

    // Render ImGui to the present texture.
    vex::CommandContext ctx = graphics->CreateCommandContext(vex::QueueType::Graphics);

    vex::Texture presentTexture = graphics->GetCurrentPresentTexture();
    ctx.Copy(presentTexture, lastFrameTexture);

    vex::TextureClearValue clearValue{ .color = { 0, 0, 0, 0 } };
    ctx.ClearTexture(presentTexture, clearValue);

    vex::TextureBinding presentBinding = { .texture = presentTexture };
    // ImGui renders to the texture that is currently set as render target. In this case we want to render
    // directly to the present texture. For this we use the ExecuteInDrawContext function, which will take care of
    // binding the render targets/depth stencil and then execute the passed in callback.
    ctx.ExecuteInDrawContext({ &presentBinding, 1 },
                             std::nullopt,
                             { vex::TextureBinding{ lastFrameTexture, vex::TextureBindingUsage::ShaderRead } },
                             [&ctx]() { ImGui_ImplVex_RenderDrawData(ctx); });

    // Submit our command context.
    graphics->Submit(ctx);
}

void ImGuiApplication::OnResize(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    graphics->DestroyTexture(lastFrameTexture);

    ExampleApplication::OnResize(window, width, height);

    lastFrameTexture = graphics->CreateTexture(
        vex::TextureDesc::CreateTexture2DDesc("PrevFrame", vex::TextureFormat::BGRA8_UNORM, width, height));
}

int main()
{
    ImGuiApplication application;
    application.Run();
}