# Vex HDR Example

This example will show you how to leverage HDR (High Dynamic Range) swapchains with Vex. Vex's D3D12 (DXGI) and Vulkan RHIs expose various Color Spaces that you can select from:

By default Vex will use a LDR (Low Dynamic Range aka non-HDR) swapchain, with format BGRA8_UNORM (or RGBA8_UNORM, depending on hardware compatibility).

To choose a specific color space, in your `GraphicsCreateDesc`'s `swapChainDesc`, change the values of `useHDRIfSupported` and `preferredColorSpace`.

With `useHDRIfSupported` disabled, the swapchain will never use an HDR color space, with it enabled your `preferredColorSpace` will be attempted to be used. If the preferred color space is not available, Vex falls back down to an sRGB (LDR) swapchain.

Here's a brief explanation of each color space:
- sRGB: Sometimes also called Rec709 (although this is technically a mistake since the two color spaces are slightly different). This color space is the default used in most displays nowadays, its typicaly represented by swapchain formats `BGRA8_UNORM` and `RGBA8_UNORM`. 

> Warning: The sRGB color space should not be confused with the gamma correction found in graphics API texture formats (eg: `pow(col, 2.2f)`). The former is a color space, whereas the latter is actually the gamma curve (also called transfer function) that the sRGB color space uses to improve encoding when writing to 8 bit channel images.

- scRGB: This is an HDR color space that uses a FP16 (`RGBA16_FLOAT`) swapchain format. It represents a larger portion of the CIE chromaticity diagram of human-visible light, with the caveat that this color space is 80% composed of "imaginary" colors (ones outside of the visible space). This color space is less frequently usedd.

- HDR10: This is the most widespread HDR color space (also called Rec2020), it uses the `ST2084` transfer function (also called PQ/Perceptual Quantizer) and the corresponding swapchain will have the `RG11B10_UNORM` format.

Each color space has its own color primaries/white points allowing for conversion from one space to another by passing coordinates through a RGB/XYZ matrix.
Vex only exposes what D3D12 and Vulkan's swapchains provide in terms of color space handling, it's the user's responsibility to correctly handle color spaces in shaders (eg: applying `ST2084` to images before presenting in the case of `HDR10`).

## Content

The example shows `memorial.hdr` drawn twice on the screen, a widely used photo of a cathedral, typically used to demonstrate various forms of color grading / tonemapping.
The left image is the raw image without any treatment in the shader, the right image is the post-treatment version.

Pressing the spacebar will change the preferred swapchain format, allowing you to see how the image looks on your monitor when the data is adapted to various color spaces.