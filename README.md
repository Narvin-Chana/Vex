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

Here is the current roadmap for Vex (subject to change at any point): https://trello.com/b/ey0HR3aB

## Requirements

C++23

CMake: 3.27+

DirectX: Windows 10/11 SDK, with feature level 12_1 and DirectX Raytracing (DXR) tier 1.1 for inline RayTracing and Bindless Resources.

Vulkan: Vulkan LunarG SDK, with compatibility for Vulkan 1.3 for Dynamic Rendering purposes.

You can use DirectX and/or Vulkan separately, by changing the `VEX_GRAPHICS_BACKEND` CMake property to either `DX12` or `VULKAN`. The default value is `AUTO` which will default to DX12 on Windows and Vulkan on Linux.
Unused APIs will not be linked and compiled (and thus will not incur any compile-time/runtime costs for your desired platform).

## Getting Started (Build System)

Vex provides the user with a CMakeLists.txt file to facilitate including it in your projet. Including Vex is as simple as obtaining Vex in a subfolder (be it via `git clone`, `git submodules`, `CMake_FetchContent` or other) and then calling `add_subdirectory(path/to/Vex)` in your CMakeLists.txt file.

Vex provides several configuration options that can be set when configuring the project with CMake:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `VEX_GRAPHICS_BACKEND` | STRING | `AUTO` | Graphics backend to use. Options: `AUTO`, `DX12`, `VULKAN`. `AUTO` selects DX12 on Windows, Vulkan elsewhere. |
| `VEX_ENABLE_SLANG` | BOOL | `OFF` (or `ON` when building examples or tests) | Enable Slang shader compiler backend support. |
| `VEX_BUILD_EXAMPLES` | BOOL | `ON` when building Vex directly, `OFF` when used as dependency | Build example programs. |
| `VEX_BUILD_TESTS` | BOOL | `ON` when building Vex directly, `OFF` when used as dependency | Build test suite. |

You can override the defaults in either of the following 3 ways:

### Command Line
```bash
cmake -DVEX_GRAPHICS_BACKEND=VULKAN -DVEX_ENABLE_SLANG=ON -B build
```

### CMake Presets (`CMakePresets.json`)
```json
{
  "configurePresets": [
    {
      "name": "my-config",
      "cacheVariables": {
        "VEX_GRAPHICS_BACKEND": "DX12",
        "VEX_ENABLE_SLANG": "ON"
      }
    }
  ]
}
```

### In CMakeLists.txt (before `add_subdirectory(Vex)`)
```cmake
set(VEX_ENABLE_SLANG ON CACHE BOOL "" FORCE)
add_subdirectory(path/to/Vex)
```

### Examples:
```bash
# Configure Vex for DirectX 12 with Slang support:
cmake -DVEX_GRAPHICS_BACKEND=DX12 -DVEX_ENABLE_SLANG=ON ..

# Configure Vex for Vulkan without examples:
cmake -DVEX_GRAPHICS_BACKEND=VULKAN -DVEX_BUILD_EXAMPLES=OFF ..
```

## Getting Started (Vex)

TODO, for now we suggest looking at the hello_cube example as it provides most of the tools needed to get started.

## About us

Vex is being worked on with love by (in order of last name):
- Narvin Chana
- Alexandre Lemarbre-Barrett
