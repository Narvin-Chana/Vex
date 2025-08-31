#include "SynchronizationTorture.h"

#include <random>
#include <span>

#include <magic_enum/magic_enum.hpp>

#include <Vex.h>
#include <Vex/Logger.h>

namespace vex
{

void SynchronizationTortureTest(GfxBackend& graphics)
{
    VEX_LOG(Info, "Starting Synchronization Torture Test...");

    // Test 1: Basic Immediate vs Deferred Submission
    VEX_LOG(Info, "Test 1: Basic Immediate vs Deferred Submission");
    {
        auto ctx1 = graphics.BeginScopedCommandContext(CommandQueueType::Graphics, SubmissionPolicy::Immediate);
        auto ctx2 = graphics.BeginScopedCommandContext(CommandQueueType::Graphics, SubmissionPolicy::DeferToPresent);

        // Both contexts will submit at different times - immediate vs at present
        VEX_LOG(Info, "Created immediate and deferred contexts");
    } // ctx1 submits immediately, ctx2 waits for Present()

    // Test 2: Cross-Queue Dependencies
    VEX_LOG(Info, "Test 2: Cross-Queue Dependencies");
    {
        std::vector<SyncToken> tokens;
        std::vector<SyncToken> graphicsTokens;

        // Submit work on compute queue
        {
            auto computeCtx =
                graphics.BeginScopedCommandContext(CommandQueueType::Compute, SubmissionPolicy::Immediate);
            tokens = computeCtx.Submit();
            VEX_LOG(Info,
                    "Submitted compute work, token: {}/{}",
                    magic_enum::enum_name(tokens[0].queueType),
                    tokens[0].value);
        }

        // Submit work on graphics queue that depends on compute
        {
            auto graphicsCtx =
                graphics.BeginScopedCommandContext(CommandQueueType::Graphics, SubmissionPolicy::Immediate, tokens);
            graphicsTokens = graphicsCtx.Submit();
            VEX_LOG(Info,
                    "Submitted graphics work dependent on compute, token: {}/{}",
                    magic_enum::enum_name(graphicsTokens[0].queueType),
                    graphicsTokens[0].value);
        }

        // Submit copy work that depends on graphics
        {
            auto copyCtx =
                graphics.BeginScopedCommandContext(CommandQueueType::Copy, SubmissionPolicy::Immediate, graphicsTokens);
            auto copyTokens = copyCtx.Submit();
            VEX_LOG(Info,
                    "Submitted copy work dependent on graphics, token: {}/{}",
                    magic_enum::enum_name(copyTokens[0].queueType),
                    copyTokens[0].value);
        }
    }

    // Test 3: Heavy Resource Creation and Usage
    VEX_LOG(Info, "Test 3: Heavy Resource Creation and Usage");
    {
        std::vector<Texture> textures;
        std::vector<Buffer> buffers;
        std::vector<SyncToken> allTokens;

        // Create a bunch of resources
        for (int i = 0; i < 10; ++i)
        {
            TextureDescription texDesc{};
            texDesc.name = std::format("Test3 Tex_{}", i);
            texDesc.width = 512;
            texDesc.height = 512;
            texDesc.format = TextureFormat::RGBA8_UNORM;
            texDesc.usage = TextureUsage::ShaderRead;
            textures.push_back(graphics.CreateTexture(texDesc, ResourceLifetime::Static));

            BufferDescription bufDesc{};
            bufDesc.name = std::format("Test3 Buf_{}", i);
            bufDesc.byteSize = 1024 * 1024; // 1MB
            bufDesc.usage = BufferUsage::GenericBuffer;
            bufDesc.memoryLocality = ResourceMemoryLocality::GPUOnly;
            buffers.push_back(graphics.CreateBuffer(bufDesc, ResourceLifetime::Static));

            VEX_LOG(Verbose, "Created texture {} and buffer {}", i, i);
        }

        // Perform random operations on different queues
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> queueDis(0, 2);
        std::uniform_int_distribution<> resourceDis(0, 9);

        for (int iteration = 0; iteration < 20; ++iteration)
        {
            CommandQueueType queueType = static_cast<CommandQueueType>(queueDis(gen));
            int srcIdx = resourceDis(gen);
            int dstIdx = resourceDis(gen);

            while (dstIdx == srcIdx)
            {
                dstIdx = resourceDis(gen);
            }

            // Use some dependencies from previous iterations
            std::span<SyncToken> deps = allTokens.size() > 3
                                            ? std::span<SyncToken>(allTokens.end() - 3, allTokens.end())
                                            : std::span<SyncToken>();

            auto ctx = graphics.BeginScopedCommandContext(queueType, SubmissionPolicy::Immediate, deps);

            if (queueType == CommandQueueType::Graphics)
            {
                // Graphics operations
                ctx.Copy(textures[srcIdx], textures[dstIdx]);
                VEX_LOG(Verbose, "Graphics: Copied texture {} to {}", srcIdx, dstIdx);
            }
            else if (queueType == CommandQueueType::Copy && srcIdx != dstIdx)
            {
                // Copy operations
                ctx.Copy(buffers[srcIdx], buffers[dstIdx]);
                VEX_LOG(Verbose, "Copy: Copied buffer {} to {}", srcIdx, dstIdx);
            }

            auto tokens = ctx.Submit();
            allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());

            VEX_LOG(Verbose, "Iteration {}: Submitted to {} queue", iteration, magic_enum::enum_name(queueType));
        }

        // Wait for some random tokens to complete
        for (int i = 0; i < std::min(5, static_cast<int>(allTokens.size())); ++i)
        {
            int tokenIdx = std::uniform_int_distribution<>(0, allTokens.size() - 1)(gen);
            VEX_LOG(Info,
                    "Waiting for token {}/{}",
                    magic_enum::enum_name(allTokens[tokenIdx].queueType),
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

    // Test 4: Rapid Context Creation/Destruction
    VEX_LOG(Info, "Test 4: Rapid Context Creation/Destruction");
    {
        std::vector<SyncToken> tokens;

        for (int i = 0; i < 50; ++i)
        {
            CommandQueueType queueType = static_cast<CommandQueueType>(i % 3);

            // Randomly use dependencies
            std::span<SyncToken> deps;
            if (!tokens.empty() && (i % 3 == 0))
            {
                deps = std::span<SyncToken>(tokens.end() - 1, tokens.end());
            }

            {
                auto ctx = graphics.BeginScopedCommandContext(queueType, SubmissionPolicy::Immediate, deps);
                auto newTokens = ctx.Submit();
                tokens.insert(tokens.end(), newTokens.begin(), newTokens.end());
            }

            // Occasionally flush GPU
            if (i % 10 == 0)
            {
                VEX_LOG(Verbose, "Flushing GPU at iteration {}", i);
                graphics.FlushGPU();
            }
        }

        VEX_LOG(Info, "Created and destroyed 50 contexts rapidly");
    }

    // Test 5: Mixed Submission Policies with Dependencies
    VEX_LOG(Info, "Test 5: Mixed Submission Policies with Dependencies");
    {
        std::vector<SyncToken> immediateTokens;

        // Create some immediate work
        {
            auto ctx1 = graphics.BeginScopedCommandContext(CommandQueueType::Compute, SubmissionPolicy::Immediate);
            immediateTokens = ctx1.Submit();
        }

        // Create deferred work that depends on immediate work
        {
            auto ctx2 = graphics.BeginScopedCommandContext(CommandQueueType::Graphics,
                                                           SubmissionPolicy::DeferToPresent,
                                                           immediateTokens);
            // This won't submit until Present() is called
        }

        // Create more immediate work
        {
            auto ctx3 = graphics.BeginScopedCommandContext(CommandQueueType::Copy, SubmissionPolicy::Immediate);
            auto moreTokens = ctx3.Submit();
            immediateTokens.insert(immediateTokens.end(), moreTokens.begin(), moreTokens.end());
        }

        // Wait for immediate work
        for (const auto& token : immediateTokens)
        {
            graphics.WaitForTokenOnCPU(token);
        }

        VEX_LOG(Info, "Mixed submission policies completed");
    }

    // Test 6: Resource Upload Torture
    //
    // Disabled since we don't yet have buffer->texture upload.
    //
    // VEX_LOG(Info, "Test 6: Resource Upload Torture");
    //{
    //    // Create upload buffer
    //    BufferDescription uploadBufDesc{};
    //    uploadBufDesc.name = "Test6 Buf";
    //    uploadBufDesc.byteSize = 1024 * 1024; // 1MB
    //    uploadBufDesc.usage = BufferUsage::None;
    //    uploadBufDesc.memoryLocality = ResourceMemoryLocality::CPUWrite;
    //    auto uploadBuffer = graphics.CreateBuffer(uploadBufDesc, ResourceLifetime::Static);

    //    // Create target texture
    //    TextureDescription texDesc{};
    //    texDesc.name = "Test6 Tex";
    //    texDesc.width = 256;
    //    texDesc.height = 256;
    //    texDesc.format = TextureFormat::RGBA8_UNORM;
    //    texDesc.usage = TextureUsage::ShaderRead;
    //    auto targetTexture = graphics.CreateTexture(texDesc, ResourceLifetime::Static);

    //    std::vector<SyncToken> uploadTokens;

    //    // Perform multiple uploads
    //    for (int i = 0; i < 10; ++i)
    //    {
    //        std::span<SyncToken> deps = uploadTokens.size() > 2
    //                                        ? std::span<SyncToken>(uploadTokens.end() - 2, uploadTokens.end())
    //                                        : std::span<SyncToken>();

    //        auto ctx = graphics.BeginScopedCommandContext(CommandQueueType::Copy, SubmissionPolicy::Immediate, deps);

    //        // Generate dummy data
    //        std::vector<u8> dummyData(1024, static_cast<u8>(i));
    //        ctx.EnqueueDataUpload(uploadBuffer, dummyData);
    //        ctx.Copy(uploadBuffer, targetTexture);

    //        auto tokens = ctx.Submit();
    //        uploadTokens.insert(uploadTokens.end(), tokens.begin(), tokens.end());

    //        VEX_LOG(Verbose, "Upload iteration {}", i);
    //    }

    //    // Wait for all uploads
    //    for (const auto& token : uploadTokens)
    //    {
    //        graphics.WaitForTokenOnCPU(token);
    //    }

    //    // Cleanup
    //    graphics.DestroyBuffer(uploadBuffer);
    //    graphics.DestroyTexture(targetTexture);
    //}

    // Test 7: Final Stress Test - Everything at Once
    VEX_LOG(Info, "Test 7: Final Stress Test");
    {
        std::vector<SyncToken> allTokens;
        std::vector<Texture> textures;
        std::vector<Buffer> buffers;

        // Create resources
        for (int i = 0; i < 5; ++i)
        {
            TextureDescription texDesc{};
            texDesc.name = std::format("Test7 Tex_{}", i);
            texDesc.width = 128;
            texDesc.height = 128;
            texDesc.format = TextureFormat::RGBA8_UNORM;
            texDesc.usage = TextureUsage::ShaderRead;
            textures.push_back(graphics.CreateTexture(texDesc, ResourceLifetime::Static));

            BufferDescription bufDesc{};
            bufDesc.name = std::format("Test7 Buf_{}", i);
            bufDesc.byteSize = 64 * 1024;
            bufDesc.usage = BufferUsage::GenericBuffer;
            bufDesc.memoryLocality = ResourceMemoryLocality::GPUOnly;
            buffers.push_back(graphics.CreateBuffer(bufDesc, ResourceLifetime::Static));
        }

        // Chaotic submission pattern
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int i = 0; i < 30; ++i)
        {
            CommandQueueType queueType = static_cast<CommandQueueType>(i % 3);
            SubmissionPolicy policy = (i % 4 == 0) ? SubmissionPolicy::DeferToPresent : SubmissionPolicy::Immediate;

            // Random dependencies
            std::span<SyncToken> deps;
            if (allTokens.size() > 5)
            {
                int startIdx = std::uniform_int_distribution<>(0, allTokens.size() - 3)(gen);
                deps = std::span<SyncToken>(allTokens.begin() + startIdx, allTokens.begin() + startIdx + 2);
            }

            {
                auto ctx = graphics.BeginScopedCommandContext(queueType, policy, deps);

                // Random operations
                int opType = i % 4;
                if (opType == 0 && queueType != CommandQueueType::Copy)
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

                if (policy == SubmissionPolicy::Immediate)
                {
                    auto tokens = ctx.Submit();
                    allTokens.insert(allTokens.end(), tokens.begin(), tokens.end());
                }
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

    // Test Present to trigger any deferred submissions
    VEX_LOG(Info, "Testing Present to trigger deferred submissions...");
    graphics.Present(false);

    // Final flush to ensure everything is done
    VEX_LOG(Info, "Final GPU flush...");
    graphics.FlushGPU();
    // Test Present to trigger any deferred submissions
    VEX_LOG(Info, "Testing Present to trigger deferred submissions...");
    graphics.Present(false);
    // Test Present to trigger any deferred submissions
    VEX_LOG(Info, "Testing Present to trigger deferred submissions...");
    graphics.Present(false);
    // Test Present to trigger any deferred submissions
    VEX_LOG(Info, "Testing Present to trigger deferred submissions...");
    graphics.Present(false);

    VEX_LOG(Info, "Synchronization Torture Test completed successfully!");
}

} // namespace vex