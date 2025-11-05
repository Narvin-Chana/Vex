#include <Vex.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <ExamplePaths.h>

static const std::filesystem::path WorkingDir =
    ExamplesDir / "hello_upload_download";

struct Image
{
    std::vector<vex::byte> data;
    vex::u32 width = 0;
    vex::u32 height = 0;
    vex::u8 nbComponents = 0;
};

Image ReadImage(const std::filesystem::path& path)
{
    int w, h, c;
    stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &c, 4);
    c = 4;

    Image img;
    img.height = h;
    img.width = w;
    img.nbComponents = c;
    img.data.resize(w * h * c);
    std::transform(data, data + w * h * c, img.data.begin(), [](stbi_uc ub) { return static_cast<vex::byte>(ub); });

    stbi_image_free(data);

    return img;
}

void WriteImage(const Image& img, const std::filesystem::path& path)
{
    stbi_write_png(path.string().c_str(), img.width, img.height, img.nbComponents, img.data.data(), 0);
}

int main()
{
    vex::Graphics backend{ vex::GraphicsCreateDesc{ .useSwapChain = false,
                                                      .enableGPUDebugLayer = !VEX_SHIPPING,
                                                      .enableGPUBasedValidation = !VEX_SHIPPING } };

    Image srcImg = ReadImage(WorkingDir / "Input.jpg");
    vex::Texture srcTexture = backend.CreateTexture({ .name = "Input Image",
                                                      .type = vex::TextureType::Texture2D,
                                                      .format = vex::TextureFormat::RGBA8_UNORM,
                                                      .width = srcImg.width,
                                                      .height = srcImg.height,
                                                      .depthOrSliceCount = 1,
                                                      .mips = 1,
                                                      .usage = vex::TextureUsage::ShaderReadWrite });
    vex::Texture dstTexture = backend.CreateTexture({ .name = "Output Image",
                                                      .type = vex::TextureType::Texture2D,
                                                      .format = vex::TextureFormat::RGBA8_UNORM,
                                                      .width = srcImg.width,
                                                      .height = srcImg.height,
                                                      .depthOrSliceCount = 1,
                                                      .mips = 2,
                                                      .usage = vex::TextureUsage::ShaderReadWrite });

    vex::CommandContext ctx =
        backend.BeginScopedCommandContext(vex::QueueType::Compute, vex::SubmissionPolicy::Immediate);

    ctx.EnqueueDataUpload(srcTexture, srcImg.data, vex::TextureRegion::AllMips());

    std::array<vex::ResourceBinding, 2> bindings{
        vex::TextureBinding{
            .texture = srcTexture,
            .usage = vex::TextureBindingUsage::ShaderReadWrite,
        },
        // Write output to mip 1
        vex::TextureBinding{
            .texture = dstTexture,
            .usage = vex::TextureBindingUsage::ShaderReadWrite,
            .subresource = { .startMip = 1, .mipCount = 1 },
        },
    };
    std::vector<vex::BindlessHandle> handles = ctx.GetBindlessHandles(bindings);

    ctx.TransitionBindings(bindings);

    ctx.Dispatch(
        vex::ShaderKey{
            .path = WorkingDir / "BoxBlur.hlsl",
            .entryPoint = "CSMain",
            .type = vex::ShaderType::ComputeShader,
        },
        vex::ConstantBinding{ std::span{ handles } },
        std::array{
            (srcImg.width + 7u) / 8u,
            (srcImg.height + 7u) / 8u,
            1u,
        });

    // Pull only mip 1 with output
    vex::TextureReadbackContext readbackContext = ctx.EnqueueDataReadback(dstTexture, vex::TextureRegion::SingleMip(1));

    // Wait on the GPU to do its readback copy operations
    backend.WaitForTokenOnCPU(ctx.Submit());

    Image dstImg{ .data = std::vector<vex::byte>(srcImg.data.size() / 4),
                  .width = srcImg.width / 2,
                  .height = srcImg.height / 2,
                  .nbComponents = srcImg.nbComponents };

    readbackContext.ReadData(dstImg.data);

    WriteImage(dstImg, WorkingDir / "Output.png");
}