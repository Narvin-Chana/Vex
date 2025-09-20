#include <Vex.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static const std::filesystem::path WorkingDir =
    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path() /
    "examples/hello_upload_download";

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
    vex::GfxBackend backend = vex::GfxBackend(vex::BackendDescription{ .useSwapChain = false,
                                                                       .frameBuffering = vex::FrameBuffering::Triple,
                                                                       .enableGPUDebugLayer = !VEX_SHIPPING,
                                                                       .enableGPUBasedValidation = !VEX_SHIPPING });

    Image srcImg = ReadImage(WorkingDir / "Input.jpg");
    vex::Texture texture = backend.CreateTexture({ .name = "Input Image",
                                                   .type = vex::TextureType::Texture2D,
                                                   .width = srcImg.width,
                                                   .height = srcImg.height,
                                                   .depthOrArraySize = 1,
                                                   .mips = 1,
                                                   .format = vex::TextureFormat::RGBA8_UNORM,
                                                   .usage = vex::TextureUsage::ShaderReadWrite },
                                                 vex::ResourceLifetime::Static);

    Image dstImg{ .data = std::vector<vex::byte>(srcImg.data.size()),
                  .width = srcImg.width,
                  .height = srcImg.height,
                  .nbComponents = srcImg.nbComponents };

    auto regions = vex::TextureUploadRegion::UploadAllMips(texture.description);

    std::vector<vex::SyncToken> computeTokens;
    vex::ReadBackContext readbackContext;
    {
        vex::CommandContext ctx =
            backend.BeginScopedCommandContext(vex::CommandQueueType::Compute, vex::SubmissionPolicy::Immediate);

        ctx.EnqueueDataUpload(texture, srcImg.data, regions);

        std::array<vex::ResourceBinding, 1> bindings{
            vex::TextureBinding{
                .texture = texture,
                .usage = vex::TextureBindingUsage::ShaderReadWrite,
            },
        };
        std::vector<vex::BindlessHandle> handles = ctx.GetBindlessHandles(bindings);

        ctx.Dispatch(
            vex::ShaderKey{
                .path = WorkingDir / "Transform.hlsl",
                .entryPoint = "CSMain",
                .type = vex::ShaderType::ComputeShader,
            },
            vex::ConstantBinding{ std::span{ handles } },
            std::array{
                (srcImg.width + 7u) / 8u,
                (srcImg.height + 7u) / 8u,
                1u,
            });

        computeTokens = ctx.Submit();
    }

    std::vector<vex::SyncToken> readbackTokens;
    {
        vex::CommandContext ctx = backend.BeginScopedCommandContext(vex::CommandQueueType::Compute,
                                                                    vex::SubmissionPolicy::Immediate,
                                                                    computeTokens);

        readbackContext = ctx.EnqueueDataReadback(texture, regions);

        readbackTokens = ctx.Submit();
    }

    // TODO: Need to find a better API for this
    readbackContext.Readback(backend, dstImg.data, readbackTokens[0]);

    WriteImage(dstImg, WorkingDir / "Output.png");
}