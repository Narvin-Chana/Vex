#include <filesystem>
#include <string>

#include <VexMacros.h>

#if VEX_MODULES
#include <VexMacros.h>
import Vex;
import magic_enum;
#else
#include <Vex.h>
#include <magic_enum/magic_enum.hpp>
#endif
using namespace vex;

static std::string GenerateCppHeaderForShaders(const Span<const NonNullPtr<Shader>> dxilShaders,
                                               const Span<const NonNullPtr<Shader>> spirvShaders)
{
    auto generatedFolder = std::filesystem::current_path();

    std::string generatedHeader{ "#pragma once\n\n#include <array>\n\n#include <Vex/Types.h>\n#include "
                                 "<Vex/ShaderView.h>\n#include <Vex/Texture.h>\n\nnamespace "
                                 "vex\n{\n" };

    static auto OutputShaders = [](std::string& output, const Span<const NonNullPtr<Shader>>& shaders)
    {
        int i = 0;
        for (const NonNullPtr shader : shaders)
        {
            output += std::format("static constexpr std::array<const byte, {}> {}Bytecode{} =\n{{\n    ",
                                  shader->GetBlob().size(),
                                  shader->GetKey().entryPoint,
                                  i);
            // Bytecode
            for (const vex::byte b : shader->GetBlob())
            {
                output += std::format("std::byte{{0x{:02X}}}, ", static_cast<u8>(b));
            }
            output += "\n};\n\n";

            // Add constexpr key
            output += std::format("static constexpr ShaderView MipGenerationCS{} =\n{{\n", i);
            output += std::format("\t\"{}\",\n", shader->GetKey().filepath);
            output += std::format("\t\"{}\",\n", shader->GetKey().entryPoint);
            output += std::format("\t{}Bytecode{},\n", shader->GetKey().entryPoint, i);

            output += "\t{";
            for (auto& num : shader->GetHash())
            {
                output += std::format("{}u,", num);
            }
            output += "},\n";

            output += std::format("\tShaderType::{}\n", shader->GetKey().type);
            output += "};\n";

            ++i;
        }

        output += "\nShaderView GetMipGenerationShader(TextureViewType type)\n{\n";
        output += "switch (type){\n"
                  "case TextureViewType::Texture2D:\n"
                  "\treturn MipGenerationCS0;\n"
                  "case TextureViewType::Texture2DArray:\n"
                  "\treturn MipGenerationCS1;\n"
                  "case TextureViewType::TextureCube:\n"
                  "\treturn MipGenerationCS2;\n"
                  "case TextureViewType::TextureCubeArray:\n"
                  "\treturn MipGenerationCS3;\n"
                  "case TextureViewType::Texture3D:\n"
                  "\treturn MipGenerationCS4;\n"
                  "default: std::unreachable();";
        output += "}\n}\n";
    };

    {
        generatedHeader += "namespace dxil\n{\n";
        OutputShaders(generatedHeader, dxilShaders);
        generatedHeader += "} // namespace dxil\n";
    }

    {
        generatedHeader += "namespace spirv\n{\n";
        OutputShaders(generatedHeader, spirvShaders);
        generatedHeader += "} // namespace spirv\n";
    }

    generatedHeader += "} // namespace vex\n";

    return generatedHeader;
}

int main()
{
    // Set current path to the Vex/ folder.
    std::filesystem::current_path(std::filesystem::current_path() / "../../../../../..");
    ShaderCompiler dxilSC{ { .target = CompilationTarget::DXIL,
                             .shaderIncludeDirectories = { std::filesystem::current_path() / "shaders" } } };
    ShaderCompiler spirvSC{ { .target = CompilationTarget::SPIRV,
                              .shaderIncludeDirectories = { std::filesystem::current_path() / "shaders" } } };

    ShaderKey MipGenerationKey{
        .filepath = "shaders/MipGeneration.hlsl",
        .entryPoint = "MipGenerationCS",
        .type = ShaderType::ComputeShader,
        .defines = {},
        .compiler = ShaderCompilerBackend::DXC,
    };

    std::vector<NonNullPtr<Shader>> dxilShaders;
    std::vector<NonNullPtr<Shader>> spirvShaders;

    for (TextureViewType t : magic_enum::enum_values<TextureViewType>())
    {
        MipGenerationKey.defines = { ShaderDefine{ "TEXTURE_DIM", std::to_string(std::to_underlying(t)) } };

        if (auto err = dxilSC.CompileShaderFromFilepath(MipGenerationKey))
        {
            VEX_LOG(Fatal, "Error compiling shader (dxil): {}", err.value());
        }
        if (auto err = spirvSC.CompileShaderFromFilepath(MipGenerationKey))
        {
            VEX_LOG(Fatal, "Error compiling shader (spirv): {}", err.value());
        }

        dxilShaders.push_back(dxilSC.GetShader(MipGenerationKey));
        spirvShaders.push_back(spirvSC.GetShader(MipGenerationKey));
    }

    auto cppHeader = GenerateCppHeaderForShaders(dxilShaders, spirvShaders);
    auto generatedHeadersDirectory = std::filesystem::current_path() / "src" / "Vex" / "Generated";

    std::ofstream out{ generatedHeadersDirectory / "MipGeneration.h" };
    out << cppHeader;
}