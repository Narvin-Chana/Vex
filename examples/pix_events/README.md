# Vex RenderDoc Example

This example shows an integration with Pix in Vex using RenderExtensions.

Launching this alone will capture the first frame and store to file in the working directory of the example.
This allows the developer to control when the capture is taken instead of having it be on a frame by frame basis. 
This example only works on DX12 as PIX is not supported on Linux or using Vulkan
If you launch it with Pix, it will capture it on the tool. 