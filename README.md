# Vex

Vex (combination of "Vulkan" and "DirectX") is a graphics API abstraction built on top of DirectX 12 and Vulkan. It serves as a way to simplify high-level rendering code, allowing you to focus on the actual rendering code, instead of implementation details and interaction with the underlying API.

Vex is supported on Windows (DirectX12 and Vulkan) and Linux (Vulkan only), it also has few requirements and is compatible with any arbitrary windowing solution. Internally we use GLFW for our test projects, but this won't affect your project.

We aim to support modern rendering features such as bindless resources and Ray Tracing via DXR and VK_KHR_ray_tracing_pipeline. Vex also requires C++23, since we aim to use modern C++ features.

Vex will natively support HLSL shader compilation, although we do intend to expose a way to pass in raw bytecode in the case you wish to use a custom shader language/compiler.  We will use the DirectX Compiler for both DirectX and Vulkan (via the SPIRV target).

## Requirements

CMake: 3.27

DirectX: Windows 10 SDK, we require feature level 12_1 and DirectX Raytracing (DXR) tier 1.1.

Vulkan: Vulkan LunarG SDK, version TBD

You can use DirectX and/or Vulkan separately, unsupported APIs will not be compiled.
