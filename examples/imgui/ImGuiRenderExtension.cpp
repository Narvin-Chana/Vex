#include "ImGuiRenderExtension.h"

#include <VexImgui.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/GfxBackend.h>

ImGuiRenderExtension::ImGuiRenderExtension(vex::GfxBackend& graphics,
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

void ImGuiRenderExtension::OnFrameStart()
{
    ImGui_ImplVex_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderExtension::OnFrameEnd()
{
    // Call all user imgui calls
    ImGui::ShowDemoWindow();

    // Render ImGui to the backbuffer.
    {
        vex::CommandContext ctx = graphics.BeginScopedCommandContext(vex::CommandQueueType::Graphics);

        vex::TextureBinding backBufferBinding = { .texture = graphics.GetCurrentBackBuffer() };
        vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };
        ctx.ClearTexture(backBufferBinding, clearValue);

        // ImGui renders to the texture that is currently set as render target.
        // We have to manually set the render target we want ImGui to render to (in this case we want to render
        // directly to the backbuffer).
        ctx.BeginRendering({ .renderTargets = std::initializer_list{ backBufferBinding } });

        ImGui::Render();
        ImGui_ImplVex_RenderDrawData(ctx);

        ctx.EndRendering();
    }
}
