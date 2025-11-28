#include "VexTest.h"

#include <cstddef>
#include <random>
#include <span>

#include <gtest/gtest.h>
#include <magic_enum/magic_enum.hpp>

#include <Vex.h>

namespace vex
{

struct SynchronizationTest : public VexTest
{
};

TEST_F(SynchronizationTest, GraphicsCreationFlush)
{
    // Simple submit then flush
    auto ctx = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    ctx.Submit();
    graphics.FlushGPU();
}

TEST_F(SynchronizationTest, ImmediateSubmission)
{
    auto ctx1 = graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate);
    auto ctx2 = graphics.BeginScopedCommandContext(QueueType::Compute, SubmissionPolicy::Immediate);
    auto ctx3 = graphics.BeginScopedCommandContext(QueueType::Copy, SubmissionPolicy::Immediate);
}

TEST_F(SynchronizationTest, CrossQueueDependecy)
{
    SyncToken tokens;
    SyncToken graphicsTokens;

    // Submit work on compute queue
    {
        auto computeCtx = graphics.BeginScopedCommandContext(QueueType::Compute, SubmissionPolicy::Immediate);
        tokens = computeCtx.Submit();
        VEX_LOG(Info, "Submitted compute work, token: {}/{}", tokens.queueType, tokens.value);
    }

    // Submit work on graphics queue that depends on compute
    {
        auto graphicsCtx =
            graphics.BeginScopedCommandContext(QueueType::Graphics, SubmissionPolicy::Immediate, { &tokens, 1 });
        graphicsTokens = graphicsCtx.Submit();
        VEX_LOG(Info,
                "Submitted graphics work dependent on compute, token: {}/{}",
                graphicsTokens.queueType,
                graphicsTokens.value);
    }

    // Submit copy work that depends on graphics
    {
        auto copyCtx = graphics.BeginScopedCommandContext(QueueType::Copy,
                                                          SubmissionPolicy::Immediate,
                                                          { &graphicsTokens, 1 });
        auto copyTokens = copyCtx.Submit();
        VEX_LOG(Info,
                "Submitted copy work dependent on graphics, token: {}/{}",
                copyTokens.queueType,
                copyTokens.value);
    }
}

TEST_F(SynchronizationTest, HeavyResouceCreationAndUsage)
{
    std::vector<Texture> textures;
    std::vector<Buffer> buffers;
    std::vector<SyncToken> allTokens;

    // Create a bunch of resources
    for (int i = 0; i < 10; ++i)
    {
        TextureDesc texDesc{};
        texDesc.name = std::format("Test3 Tex_{}", i);
        texDesc.width = 512;
        texDesc.height = 512;
        texDesc.format = TextureFormat::RGBA8_UNORM;
        texDesc.usage = TextureUsage::ShaderRead;
        textures.push_back(graphics.CreateTexture(texDesc));

        BufferDesc bufDesc{};
        bufDesc.name = std::format("Test3 Buf_{}", i);
        bufDesc.byteSize = 1024ull * 1024ull; // 1MB
        bufDesc.usage = BufferUsage::GenericBuffer;
        bufDesc.memoryLocality = ResourceMemoryLocality::GPUOnly;
        buffers.push_back(graphics.CreateBuffer(bufDesc));

        VEX_LOG(Verbose, "Created texture {} and buffer {}", i, i);
    }

    // Perform random operations on different queues
    std::mt19937 gen(1234567);
    std::uniform_int_distribution<> queueDis(0, 2);
    std::uniform_int_distribution<> resourceDis(0, 9);

    for (int iteration = 0; iteration < 20; ++iteration)
    {
        QueueType queueType = static_cast<QueueType>(queueDis(gen));
        int srcIdx = resourceDis(gen);
        int dstIdx = resourceDis(gen);

        while (dstIdx == srcIdx)
        {
            dstIdx = resourceDis(gen);
        }

        // Use some dependencies from previous iterations
        std::span<SyncToken> deps =
            allTokens.size() > 3 ? std::span<SyncToken>(allTokens.end() - 3, allTokens.end()) : std::span<SyncToken>();

        auto ctx = graphics.BeginScopedCommandContext(queueType, SubmissionPolicy::Immediate, deps);

        if (queueType == QueueType::Graphics)
        {
            // Graphics operations
            ctx.Copy(textures[srcIdx], textures[dstIdx]);
            VEX_LOG(Verbose, "Graphics: Copied texture {} to {}", srcIdx, dstIdx);
        }
        else if (queueType == QueueType::Copy && srcIdx != dstIdx)
        {
            // Copy operations
            ctx.Copy(buffers[srcIdx], buffers[dstIdx]);
            VEX_LOG(Verbose, "Copy: Copied buffer {} to {}", srcIdx, dstIdx);
        }

        allTokens.push_back(ctx.Submit());

        VEX_LOG(Verbose, "Iteration {}: Submitted to {} queue", iteration, queueType);
    }

    // Wait for some random tokens to complete
    for (int i = 0; i < std::min(5, static_cast<int>(allTokens.size())); ++i)
    {
        int tokenIdx = std::uniform_int_distribution<>(0, allTokens.size() - static_cast<std::size_t>(1))(gen);
        VEX_LOG(Info,
                "Waiting for token {}/{}",
                allTokens[tokenIdx].queueType,
                allTokens[tokenIdx].value);
        graphics.WaitForTokenOnCPU(allTokens[tokenIdx]);
        VEX_LOG(Info, "Token completed!");
    }

    // Cleanup
    for (auto& tex : textures)
    {
        graphics.DestroyTexture(tex);
    }
    for (auto& buf : buffers)
    {
        graphics.DestroyBuffer(buf);
    }
}

TEST_F(SynchronizationTest, RapidContextCreationDestruction)
{
    std::vector<SyncToken> tokens;

    for (int i = 0; i < 50; ++i)
    {
        QueueType queueType = static_cast<QueueType>(i % 3);

        // Randomly use dependencies
        std::span<SyncToken> deps;
        if (!tokens.empty() && (i % 3 == 0))
        {
            deps = std::span(tokens.end() - 1, tokens.end());
        }

        {
            auto ctx = graphics.BeginScopedCommandContext(queueType, SubmissionPolicy::Immediate, deps);
            tokens.push_back(ctx.Submit());
        }

        // Occasionally flush GPU
        if (i % 10 == 0)
        {
            VEX_LOG(Verbose, "Flushing GPU at iteration {}", i);
            graphics.FlushGPU();
        }
    }
}

TEST_F(SynchronizationTest, SubmissionWithDependency)
{
    std::vector<SyncToken> immediateTokens;

    // Create some immediate work
    {
        auto ctx1 = graphics.BeginScopedCommandContext(QueueType::Compute, SubmissionPolicy::Immediate);
        immediateTokens.push_back(ctx1.Submit());
    }

    // Create work that depends on immediate work
    {
        auto ctx2 = graphics.BeginScopedCommandContext(QueueType::Graphics,
                                                       SubmissionPolicy::Immediate,
                                                       immediateTokens);
    }

    // Create more immediate work
    {
        auto ctx3 = graphics.BeginScopedCommandContext(QueueType::Copy, SubmissionPolicy::Immediate);
        auto moreTokens = ctx3.Submit();
        immediateTokens.push_back(moreTokens);
    }

    // Wait for first and third immediate work
    for (const auto& token : immediateTokens)
    {
        graphics.WaitForTokenOnCPU(token);
    }
}

TEST_F(SynchronizationTest, ResourceUploadTorture)
{
    // Create upload buffer
    BufferDesc uploadBufDesc{};
    uploadBufDesc.name = "Test6 Buf";
    uploadBufDesc.byteSize = 1024 * 1024; // 1MB
    uploadBufDesc.usage = BufferUsage::None;
    uploadBufDesc.memoryLocality = ResourceMemoryLocality::CPUWrite;
    auto uploadBuffer = graphics.CreateBuffer(uploadBufDesc);

    // Create target texture
    TextureDesc texDesc{};
    texDesc.name = "Test6 Tex";
    texDesc.width = 256;
    texDesc.height = 256;
    texDesc.format = TextureFormat::RGBA8_UNORM;
    texDesc.usage = TextureUsage::ShaderRead;
    auto targetTexture = graphics.CreateTexture(texDesc);

    std::vector<SyncToken> uploadTokens;

    // Perform multiple uploads
    for (int i = 0; i < 10; ++i)
    {
        std::span<SyncToken> deps = uploadTokens.size() > 2
                                        ? std::span<SyncToken>(uploadTokens.end() - 2, uploadTokens.end())
                                        : std::span<SyncToken>();

        auto ctx = graphics.BeginScopedCommandContext(QueueType::Copy, SubmissionPolicy::Immediate, deps);

        // Generate dummy data and upload a 1024 section of the buffer.
        std::vector<byte> dummyData(1024, static_cast<byte>(i));
        ctx.EnqueueDataUpload(uploadBuffer, dummyData, BufferRegion{ .offset = 1024ull * i, .byteSize = 1024 });
        ctx.Copy(uploadBuffer, targetTexture);

        uploadTokens.push_back(ctx.Submit());

        VEX_LOG(Verbose, "Upload iteration {}", i);
    }

    // Wait for all uploads
    for (const auto& token : uploadTokens)
    {
        graphics.WaitForTokenOnCPU(token);
    }

    // Cleanup
    graphics.DestroyBuffer(uploadBuffer);
    graphics.DestroyTexture(targetTexture);
}

TEST_F(SynchronizationTest, FinalStressTest)
{
    std::vector<SyncToken> allTokens;
    std::vector<Texture> textures;
    std::vector<Buffer> buffers;

    // Create resources
    for (int i = 0; i < 5; ++i)
    {
        TextureDesc texDesc{};
        texDesc.name = std::format("Test7 Tex_{}", i);
        texDesc.width = 128;
        texDesc.height = 128;
        texDesc.format = TextureFormat::RGBA8_UNORM;
        texDesc.usage = TextureUsage::ShaderRead;
        textures.push_back(graphics.CreateTexture(texDesc));

        BufferDesc bufDesc{};
        bufDesc.name = std::format("Test7 Buf_{}", i);
        bufDesc.byteSize = 64ull * 1024ull;
        bufDesc.usage = BufferUsage::GenericBuffer;
        bufDesc.memoryLocality = ResourceMemoryLocality::GPUOnly;
        buffers.push_back(graphics.CreateBuffer(bufDesc));
    }

    // Chaotic submission pattern
    std::mt19937 gen(123498351);

    for (int i = 0; i < 30; ++i)
    {
        QueueType queueType = static_cast<QueueType>(i % 3);

        // Random dependencies
        std::span<SyncToken> deps;
        if (allTokens.size() > 5)
        {
            int startIdx = std::uniform_int_distribution<>(0, allTokens.size() - 3)(gen);
            deps = std::span<SyncToken>(allTokens.begin() + startIdx, allTokens.begin() + startIdx + 2);
        }

        {
            auto ctx = graphics.BeginScopedCommandContext(queueType, SubmissionPolicy::Immediate, deps);

            // Random operations
            int opType = i % 4;
            if (opType == 0 && queueType != QueueType::Copy)
            {
                // Texture copy
                int src = i % textures.size();
                int dst = (i + 1) % textures.size();
                if (src != dst)
                {
                    ctx.Copy(textures[src], textures[dst]);
                }
            }
            else if (opType == 1)
            {
                // Buffer copy
                int src = i % buffers.size();
                int dst = (i + 1) % buffers.size();
                if (src != dst)
                {
                    ctx.Copy(buffers[src], buffers[dst]);
                }
            }

            allTokens.push_back(ctx.Submit());
        }

        // Random flushes
        if (i % 7 == 0)
        {
            graphics.FlushGPU();
            VEX_LOG(Verbose, "Random flush at iteration {}", i);
        }

        // Random waits
        if (!allTokens.empty() && i % 5 == 0)
        {
            int tokenIdx = std::uniform_int_distribution<>(0, allTokens.size() - 1)(gen);
            graphics.WaitForTokenOnCPU(allTokens[tokenIdx]);
        }
    }

    // Cleanup
    for (auto& tex : textures)
    {
        graphics.DestroyTexture(tex);
    }
    for (auto& buf : buffers)
    {
        graphics.DestroyBuffer(buf);
    }
}

} // namespace vex