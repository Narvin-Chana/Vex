#include "HDR.h"

#include <GLFWIncludes.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Vex/Formattable.h>

float* hdrData;
vex::i32 hdrWidth, hdrHeight, hdrChannels;

static constexpr vex::u32 FloatRGBANumChannels = 4;

HDRApplication::HDRApplication()
    : ExampleApplication("HDRApplication", hdrWidth * 1.5f, hdrHeight * 1.5f, false)
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = vex::MakeUnique<vex::Graphics>(vex::GraphicsCreateDesc{
        .platformWindow = { .windowHandle = platformWindow,
                            .width = static_cast<vex::u32>(width),
                            .height = static_cast<vex::u32>(height) },
        .useSwapChain = true,
        .swapChainDesc = { .useHDRIfSupported = true, .preferredColorSpace = preferredColorSpace },
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });
    SetupShaderErrorHandling();

    hdrTexture = graphics->CreateTexture(vex::TextureDesc::CreateTexture2DDesc("Memorial.png",
                                                                               vex::TextureFormat::RGBA32_FLOAT,
                                                                               hdrWidth,
                                                                               hdrHeight,
                                                                               1,
                                                                               vex::TextureUsage::ShaderRead));

    // Upload the HDR image to the GPU.
    {
        vex::CommandContext ctx =
            graphics->BeginScopedCommandContext(vex::QueueType::Graphics, vex::SubmissionPolicy::Immediate);

        ctx.EnqueueDataUpload(
            hdrTexture,
            std::span<const vex::byte>{ reinterpret_cast<vex::byte*>(hdrData),
                                        hdrWidth * hdrHeight * FloatRGBANumChannels * sizeof(float) });

        // Now keep the texture in a shader read state.
        ctx.Barrier(hdrTexture,
                    vex::RHIBarrierSync::AllCommands,
                    vex::RHIBarrierAccess::ShaderRead,
                    vex::RHITextureLayout::ShaderResource);
    }

    std::array samplers{
        vex::TextureSampler::CreateSampler(vex::FilterMode::Linear, vex::AddressMode::Clamp),
    };
    graphics->SetSamplers(samplers);
}

void HDRApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        {
            vex::u32 texWidth = hdrTexture.desc.width * 0.75f;
            vex::u32 texHeight = hdrTexture.desc.height * 0.75f;

            vex::CommandContext ctx = graphics->BeginScopedCommandContext(vex::QueueType::Graphics);

            vex::TextureBinding renderTarget = { .texture = graphics->GetCurrentPresentTexture(), .isSRGB = false };
            ctx.ClearTexture(renderTarget);

            ctx.SetScissor(0, 0, width, height);
            ctx.SetViewport(0, 0, texWidth, texHeight);

            vex::DrawDesc drawDesc;
            drawDesc.vertexShader = vex::ShaderKey{
                .path = ExamplesDir / "hdr" / "HDR.hlsl",
                .entryPoint = "FullscreenTriangleVS",
                .type = vex::ShaderType::VertexShader,
            };
            drawDesc.pixelShader = vex::ShaderKey{
                .path = ExamplesDir / "hdr" / "HDR.hlsl",
                .entryPoint = "TonemapPS",
                .type = vex::ShaderType::PixelShader,
            };

            vex::TextureBinding shaderRead = { .texture = hdrTexture,
                                               .usage = vex::TextureBindingUsage::ShaderRead,
                                               .isSRGB = false };

            struct PassConstants
            {
                vex::BindlessHandle hdrTextureHandle;
            } data{ .hdrTextureHandle = graphics->GetBindlessHandle(shaderRead) };

            ctx.Draw(drawDesc, { .renderTargets = { &renderTarget, 1 } }, vex::ConstantBinding{ data }, 3);

            drawDesc.pixelShader.defines.emplace_back(
                "COLOR_SPACE",
                std::to_string(std::to_underlying(graphics->GetCurrentHDRColorSpace()) + 1));

            ctx.SetViewport(texWidth, 0, texWidth, texHeight);
            ctx.Draw(drawDesc, { .renderTargets = { &renderTarget, 1 } }, vex::ConstantBinding{ data }, 3);

#if VEX_SLANG
            drawDesc.vertexShader.path = ExamplesDir / "hdr" / "HDR.slang";
            drawDesc.pixelShader.path = ExamplesDir / "hdr" / "HDR.slang";

            drawDesc.pixelShader.defines = {};

            ctx.SetViewport(0, texHeight, texWidth, texHeight);
            ctx.Draw(drawDesc, { .renderTargets = { &renderTarget, 1 } }, vex::ConstantBinding{ data }, 3);

            drawDesc.pixelShader.defines.emplace_back(
                "COLOR_SPACE",
                std::to_string(std::to_underlying(graphics->GetCurrentHDRColorSpace()) + 1));

            ctx.SetViewport(texWidth, texHeight, texWidth, texHeight);
            ctx.Draw(drawDesc, { .renderTargets = { &renderTarget, 1 } }, vex::ConstantBinding{ data }, 3);
#endif
        }

        graphics->Present(windowMode == Fullscreen);

        if (logSwapChainColorSpace)
        {
            if (graphics->GetCurrentHDRColorSpace() == graphics->GetPreferredHDRColorSpace())
            {
                VEX_LOG(vex::Info, "Color space successfully changed to {}.", graphics->GetCurrentHDRColorSpace());
            }
            else
            {
                VEX_LOG(vex::Info,
                        "Color space change failed due to being unsupported. Preferred: {}, Actual: {}.",
                        graphics->GetPreferredHDRColorSpace(),
                        graphics->GetCurrentHDRColorSpace());
            }
            logSwapChainColorSpace = false;
        }
    }
}

void HDRApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
    if (graphics && action == GLFW_PRESS && key == GLFW_KEY_SPACE)
    {
        vex::ColorSpace newColorSpace =
            static_cast<vex::ColorSpace>((std::to_underlying(graphics->GetPreferredHDRColorSpace()) + 1) %
                                         magic_enum::enum_count<vex::ColorSpace>());
        // Changing the preferred color space will only actually happen upon the next present.
        graphics->SetPreferredHDRColorSpace(newColorSpace);
        // Since the preferred color space might not actually be applied, we will look at the state of the swapchain
        // colorspace after the next present.
        logSwapChainColorSpace = true;
    }
    ExampleApplication::HandleKeyInput(key, scancode, action, mods);
}

void HDRApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}

int main()
{
    // Read the HDR image from the filesystem with stb_image.
    std::filesystem::path hdrImagePath = ExamplesDir / "memorial.hdr";

    // stbi returns the number of channels of actual data, we're interested in the total number of channels including
    // padding for upload purposes.
    hdrData = stbi_loadf(hdrImagePath.string().c_str(), &hdrWidth, &hdrHeight, &hdrChannels, FloatRGBANumChannels);
    if (!hdrData)
    {
        const char* error = stbi_failure_reason();
        VEX_LOG(vex::Fatal, "Failed to load HDR: {}", error);
    }

    HDRApplication hdr;
    hdr.Run();

    stbi_image_free(hdrData);
}