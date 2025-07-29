# Vex ImGui Example

This example shows off how to use Vex's RHIAccessor to get access to the device, command queue, descriptor heap and command buffer that have been created by Vex's RHI. In this case we use these to setup [ImGui](https://github.com/ocornut/imgui), a graphics user interface library which requires many hooks into the API with which you use it.
We use GLFW for windowing, but Win32/SDL solutions are very similar. 

We recommend following ImGui's [Getting Started](https://github.com/ocornut/imgui/wiki/Getting-Started) guide at the same time.