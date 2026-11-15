# installed_deps

Checks that Vex builds from installed dependency packages only, without FetchContent downloading anything.

- `packages/` stands in for a package manager. It installs DXC, Slang, and SPIRV-Reflect (Vulkan) or
  WinPixEventRuntime (DX12) as CMake packages, at the versions Vex pins and with the target names vcpkg uses. It also
  installs the sources of magic_enum and the D3D12 Agility SDK under `src/`, since Vex always builds those itself.
- `CMakeLists.txt` adds Vex with `add_subdirectory`. Every package is required (`CMAKE_REQUIRE_FIND_PACKAGE_<name>`),
  and magic_enum and the Agility SDK come from `FETCHCONTENT_SOURCE_DIR_<NAME>`. Configuring fails if FetchContent
  still downloaded a dependency.
- `main.cpp` compiles a compute shader with DXC and another with Slang, and dispatches both.

```sh
# Linux (Vulkan). On Windows, DX12 is the default (-DVEX_GRAPHICS_BACKEND=VULKAN to test Vulkan instead).
cmake -S tests/installed_deps -B build-installed-deps -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-installed-deps
cd build-installed-deps && ./vex_installed_deps_test
```

The first configure stages the packages into `build-installed-deps/packages`. Pass `-DVEX_TEST_PACKAGES_DIR=<prefix>`
to use packages installed elsewhere instead. Pointing it at an empty prefix must fail the configure step.
