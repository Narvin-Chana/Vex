# Vex

Vex (combination of "Vulkan" and "DirectX") is a graphics API abstraction built on top of DirectX 12 and Vulkan. It
serves as a way to simplify high-level rendering code, allowing you to focus on the actual rendering code, instead of
implementation details and interactions with the underlying API.

Vex was created to power our own renderers in order to experiment with various graphics features and techniques. Our
philosophy is to make Vex as light as possible, we use C++23, with a specific focus on compilation times.

Use Vex if you're searching for a lightweight, performant and easy-to-use API abstraction for modern graphics API
features!

Vex is supported on Windows (DirectX 12 and Vulkan) and Linux (Vulkan only), it also has few requirements (the main one
being C++23) and is compatible with any arbitrary windowing solution (GLFW, Win32 or others). We use GLFW for our
example projects, but this won't affect your project.

Check out our [examples](https://github.com/Narvin-Chana/Vex/tree/main/examples) to get started!

Features:

- Custom memory allocator providing resource allocation in dynamically growing/shrinking heap pages.
- Bindless resources using SM6_6+ (ResourceDescriptorHeap).
- Optional Ray Tracing support via DirectX Raytracing (DXR) and VK_KHR_ray_tracing_pipeline/.
- Optional Shader Compiler supporting Hot-Reload and offline compilation:
    - Native HLSL shader compilation support using DXC (on both Windows and Linux).
    - Native Slang shader compilation support using SlangAPI (on both Windows and Linux).
- Automatic resource lifespan handling, and under-the-hood optimizations to make our abstraction layer as thin as
  possible.
- Multi-queue synchronization using custom `SyncToken` logic.

[Here is the current roadmap for Vex (subject to change at any point).](https://trello.com/b/ey0HR3aB)

If you encounter any problems don't hesitate to open a GitHub issue, it will be our pleasure to help!

## Requirements

Any C++23 compatible compiler should suffice. We guarantee support for the following toolchains, any older versions are
subject to risk.

- MSVC 19.50.35717
- Clang 20.1.8
- GCC 14

**CMake**: 4.2.2+

**Git** (for using CMake's FetchContent)

**DirectX**: Windows 10/11 SDK, with feature level 12_1 and DirectX Raytracing (DXR) tier 1.1 for inline RayTracing and
Bindless Resources.

**Vulkan**: Vulkan LunarG SDK (1.4.321+), with compatibility for Vulkan 1.3 for Dynamic Rendering purposes and the
`VK_KHR_unified_image_layouts` extension.

You can use DirectX and/or Vulkan separately, by changing the `VEX_GRAPHICS_BACKEND` CMake property to either `DX12` or
`VULKAN`. The default value is `AUTO` which will default to DX12 on Windows and Vulkan on Linux.
Unused APIs will not be linked and compiled (and thus will not incur any compile-time/runtime costs for your desired
platform).

### Shading Language

Vex's shader compiler provides two compiler implementations, `DXC 1.9.2026` with the H202x spec and/or `Slang 2026.3.1`.
If the shader compiler is not specified Vex will infer it from the file extension (`.slang` = Slang otherwise DXC).

## Getting Started (Build System)

Vex ships with a CMakeLists.txt file to facilitate including it in your project. Here's the recommended way of using it
if your project also uses CMake:

```
include(FetchContent)
# Fetch from either a local directory (using SOURCE_DIR "path/to/vex") or a remote git url to a Vex release (using GIT_REPOSITORY "url")
FetchContent_Declare(
  Vex
  ...
)
FetchContent_MakeAvailable(Vex)

# Later on once your target executable has been created:
target_link_libraries(YourTarget PRIVATE Vex::Vex)
vex_setup_runtime(YourTarget)
```

`vex_setup_runtime` must be called to correctly setup the runtime dependencies of Vex.

Vex can also be installed (`cmake --install`) and consumed with `find_package(Vex REQUIRED)`, which provides the same
`Vex::Vex` target and `vex_setup_runtime` function.

### Dependencies

Vex's dependencies are declared with CMake's
[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html). An installed package is used when
`find_package` finds one that is at least the version Vex pins, otherwise the pinned version is downloaded:

| Dependency          | Package (eg: from vcpkg)              | FetchContent name   | Notes                                          |
|---------------------|---------------------------------------|---------------------|------------------------------------------------|
| DXC                 | `directx-dxc` 1.9.2602+               | `dxc`               |                                                |
| Slang               | `slang` 2026.18+                      | `slang`             |                                                |
| WinPixEventRuntime  | `winpixevent` 1.0.240308001+          | `winpixevent`       | DX12 only.                                     |
| SPIRV-Reflect       | `unofficial-spirv-reflect` 1.4.321.0+ | `spirv_reflect`     | Vulkan only, compiled into Vex when downloaded. |
| D3D12 Agility SDK   | -                                     | `d3d12_agility_sdk` | DX12 only, always the `Microsoft.Direct3D.D3D12` NuGet package (1.618.4+). |
| magic_enum          | -                                     | `magic_enum`        | Always built from source, installed with Vex.  |
| Vulkan SDK          | `Vulkan` 1.4.321+                     | -                   | Vulkan only, must be installed.                |

Packages which do not report their version are not used. A project using an installed Vex finds or downloads the same
dependencies when calling `find_package(Vex)`. All of this is controlled by the standard CMake variables:

- `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`: never look for installed packages, always use the pinned versions.
- `CMAKE_REQUIRE_FIND_PACKAGE_<name>=ON`, with the FetchContent name (eg: `CMAKE_REQUIRE_FIND_PACKAGE_dxc`): fail
  instead of downloading when the package is not found.
- `FETCHCONTENT_SOURCE_DIR_<NAME>` (eg: `FETCHCONTENT_SOURCE_DIR_D3D12_AGILITY_SDK`): use a local copy (an extracted
  release, NuGet package or checkout) instead of downloading it.
- `FETCHCONTENT_FULLY_DISCONNECTED=ON`: never download or update anything, once the dependencies have been populated.

Vex provides several configuration options that can be set when configuring the project with CMake:

| Option                 | Type   | Default                                         | Description                                                                                                                                                         |
|------------------------|--------|-------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `VEX_GRAPHICS_BACKEND` | STRING | `AUTO`                                          | Graphics backend to use. Options: `AUTO`, `DX12`, `VULKAN`. `AUTO` selects DX12 on Windows, Vulkan elsewhere.                                                       |
| `VEX_ENABLE_DXC`       | BOOL   | `ON`                                            | Enable DXC shader compiler backend support.                                                                                                                         |
| `VEX_ENABLE_SLANG`     | BOOL   | `OFF` (or `ON` when building examples or tests) | Enable Slang shader compiler backend support.                                                                                                                       |
| `VEX_USE_MODULES`      | BOOL   | `OFF`                                           | Will build the Vex.cppm C++20 module. When using this option your parent CMake file must set CMAKE_CXX_SCAN_FOR_MODULES to ON: "set(CMAKE_CXX_SCAN_FOR_MODULES ON)" |
| `VEX_BUILD_EXAMPLES`   | BOOL   | `OFF`                                           | Advanced: Build example programs.                                                                                                                                   |
| `VEX_BUILD_TESTS`      | BOOL   | `OFF`                                           | Advanced: Build test suite.                                                                                                                                                   |
| `VEX_BUILD_TOOLS`      | BOOL   | `OFF`                                           | Advanced: Build internal Vex tool suite.                                                                                                                                      |

You can override the defaults in any of the following ways:

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

Then to build a specific preset run:

```bash
cmake -B build --preset my-config
```

### In CMakeLists.txt (before calling `FetchContent_MakeAvailable(Vex)`):

```cmake
set(VEX_ENABLE_SLANG ON)
```

### Examples of possible configurations:

```bash
# Configure Vex for DirectX 12 with Slang support:
cmake -DVEX_GRAPHICS_BACKEND=DX12 -DVEX_ENABLE_SLANG=ON ..

# Configure Vex for Vulkan without examples:
cmake -DVEX_GRAPHICS_BACKEND=VULKAN -DVEX_BUILD_EXAMPLES=OFF ..
```

## Getting Started (Vex)

TODO, for now we suggest looking at
the [hello_cube example](https://github.com/Narvin-Chana/Vex/tree/main/examples/hello_cube) as it provides most of the
tools needed to get started.

## About us

Vex is being worked on with love by (in order of last name):

- Narvin Chana
- Alexandre Lemarbre-Barrett
