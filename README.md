# Vex

Vex (combination of "Vulkan" and "DirectX") is a graphics API abstraction built on top of DirectX 12 and Vulkan. It serves as a way to simplify high-level rendering code, allowing you to focus on the actual rendering code, instead of implementation details and interactions with the underlying API.

Vex was created to power our own toy renderers and in order to experiment with various graphics features and techniques. Our philosophy is to make Vex as light as possible, we use C++23, with a specific focus on avoiding heavy headers that negatively affect compile-times.

Use Vex if you're searching for a lightweight, performant and easy-to-use API abstraction for modern graphics API features!

Vex is supported on Windows (DirectX 12 and Vulkan) and Linux (Vulkan only), it also has few requirements (the main one being C++23) and is compatible with any arbitrary windowing solution (GLFW, Win32 or others). We use GLFW for our example projects, but this won't affect your project.

Check out our [examples](https://github.com/Narvin-Chana/Vex/tree/main/examples) to get started!

Features:
- Custom memory allocator providing resource allocation in dynamically growing/shrinking heap pages.
- Bindless resources using SM6_6+ (ResourceDescriptorHeap).
- Ray Tracing via DirectX Raytracing (DXR) and VK_KHR_ray_tracing_pipeline/.
- Native HLSL shader compilation support using DXC (on both Windows and Linux).
- Native Slang shader compilation support using SlangAPI (on both Windows and Linux).
- Automatic resource lifespan handling, and under-the-hood optimizations to make our abstraction layer as thin as possible.
- Multi-queue synchronization using custom `SyncToken` logic.

## Requirements

C++23

CMake: 3.27+

DirectX: Windows 10 SDK, with feature level 12_1 and DirectX Raytracing (DXR) tier 1.1 for inline RayTracing and Bindless Resources.

Vulkan: Vulkan LunarG SDK, with compatibility for version 1.3 for Dynamic Rendering purposes.

You can use DirectX and/or Vulkan separately, by changing the `VEX_GRAPHICS_BACKEND` CMake property to either `DX12` or `VULKAN`. The default value is `AUTO` which will default to DX12 on Windows and Vulkan on Linux.
Unused APIs will not be linked and compiled (and thus will not incur any compile-time/runtime costs for your desired platform).
