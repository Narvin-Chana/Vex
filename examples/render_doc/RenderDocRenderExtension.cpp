#include "RenderDocRenderExtension.h"

#include <VexRenderDoc.h>

#include <Vex/Graphics.h>

static void* GDevicePtr;

RenderDocRenderExtension::RenderDocRenderExtension(const vex::PlatformWindowHandle& handle)
{
#if defined(_WIN32)
    SetupRenderDoc(handle.window);
#elif defined(__linux__)
    SetupRenderDoc(handle.display);
#endif
}

void RenderDocRenderExtension::Initialize()
{
    GDevicePtr = data.rhi->GetNativeDevicePtr();
}

void RenderDocRenderExtension::StartCapture()
{
    StartRenderDocCapture(GDevicePtr);
}

void RenderDocRenderExtension::EndCapture()
{
    EndRenderDocCapture(GDevicePtr);
}
