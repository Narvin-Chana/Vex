#include "ImGuiRenderExtension.h"

#include <VexImgui.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/Formats.h>
#include <Vex/Graphics.h>

ImGuiRenderExtension::ImGuiRenderExtension(vex::Graphics& graphics,
                                           GLFWwindow* window,
                                           vex::FrameBuffering buffering,
                                           vex::TextureFormat swapchainFormat)
    : graphics(graphics)
    , window(window)
    , buffering(buffering)
    , swapchainFormat(swapchainFormat)
{
}

ImGuiRenderExtension::~ImGuiRenderExtension()
{
}

void ImGuiRenderExtension::Initialize()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOther(window, true);

    ImGui_ImplVex_InitInfo initInfo{ .rhi = vex::NonNullPtr(data.rhi),
                                     .descriptorPool = vex::NonNullPtr(data.descriptorPool),
                                     .buffering = buffering,
                                     .swapchainFormat = swapchainFormat };
    ImGui_ImplVex_Init(initInfo);
}

void ImGuiRenderExtension::Destroy()
{
    ImGui_ImplVex_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiRenderExtension::OnPrePresent()
{
    ImGui_ImplVex_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Call all user imgui calls
    ImGui::ShowDemoWindow();

    // Render resolves all internal ImGui code.
    // It does not touch the graphics API at all.
    ImGui::Render();

    // Render ImGui to the backbuffer.
    {
        vex::CommandContext ctx = graphics.BeginScopedCommandContext(vex::QueueType::Graphics);

        vex::TextureBinding backBufferBinding = { .texture = graphics.GetCurrentPresentTexture() };
        vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };
        ctx.ClearTexture(backBufferBinding, clearValue);

        // ImGui renders to the texture that is currently set as render target. In this case we want to render
        // directly to the backbuffer. For this we use the ExecuteInDrawContext function, which will take care of
        // binding the render targets/depth stencil and then execute the passed in callback.
        ctx.ExecuteInDrawContext({ &backBufferBinding, 1 },
                                 std::nullopt,
                                 [&ctx]() { ImGui_ImplVex_RenderDrawData(ctx); });
    }
}
