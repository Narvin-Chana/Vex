#pragma once

class HelloTriangleApplication
{
public:
    HelloTriangleApplication();
    ~HelloTriangleApplication();
    void Run();

    HelloTriangleApplication(const HelloTriangleApplication&) = default;
    HelloTriangleApplication& operator=(const HelloTriangleApplication&) = default;
    HelloTriangleApplication(HelloTriangleApplication&&) = default;
    HelloTriangleApplication& operator=(HelloTriangleApplication&&) = default;

private:
    class GLFWwindow* window;
};