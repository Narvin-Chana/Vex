#include "VexTest.h"

#include <cctype>
#include <fstream>
#include <sstream>

#if __has_include(<ShaderCompiler/Generated/EmbeddedShaders.h>)
#include <ShaderCompiler/Generated/EmbeddedShaders.h>
#define VEX_HAS_EMBEDDED_SHADERS 1
#else
#define VEX_HAS_EMBEDDED_SHADERS 0
#endif

#include <gtest/gtest.h>

namespace vex
{

namespace EmbeddedShaderTests
{

#if VEX_HAS_EMBEDDED_SHADERS

std::string_view Trim(std::string_view str)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!str.empty() && isSpace(str.front()))
    {
        str.remove_prefix(1);
    }
    while (!str.empty() && isSpace(str.back()))
    {
        str.remove_suffix(1);
    }
    return str;
}

std::string ReadFileToString(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

#endif // VEX_HAS_EMBEDDED_SHADERS

// Verifies that when Vex is built with VEX_USE_EMBEDDED_SHADERS, the shader source embedded into the binary at
// configure-time is exactly the same as the shader source on disk.
TEST(EmbeddedShaderTest, EmbeddedVexHLSLMatchesSourceFile)
{
#if !VEX_HAS_EMBEDDED_SHADERS
    GTEST_SKIP() << "Vex was not built with VEX_USE_EMBEDDED_SHADERS, skipping.";
#elif !VEX_DXC
    GTEST_SKIP() << "Vex was not built with DXC support, skipping.";
#else
    std::string diskSource = ReadFileToString(VexRootPath / "shaders" / "Vex.hlsli");
    EXPECT_FALSE(embedded::VexHLSL.empty());
    EXPECT_EQ(Trim(embedded::VexHLSL), Trim(diskSource));
#endif
}

TEST(EmbeddedShaderTest, EmbeddedVexSlangMatchesSourceFile)
{
#if !VEX_HAS_EMBEDDED_SHADERS
    GTEST_SKIP() << "Vex was not built with VEX_USE_EMBEDDED_SHADERS, skipping.";
#elif !VEX_SLANG
    GTEST_SKIP() << "Vex was not built with Slang support, skipping.";
#else
    std::string diskSource = ReadFileToString(VexRootPath / "shaders" / "Vex.slang");
    EXPECT_FALSE(embedded::VexSlang.empty());
    EXPECT_EQ(Trim(embedded::VexSlang), Trim(diskSource));
#endif
}

} // namespace EmbeddedShaderTests

} // namespace vex
