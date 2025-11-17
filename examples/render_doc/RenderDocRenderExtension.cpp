#include "RenderDocRenderExtension.h"

#include <GLFWIncludes.h>
#include <VexRenderDoc.h>

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/Formats.h>
#include <Vex/Graphics.h>

static void* GDevicePtr;

RenderDocRenderExtension::RenderDocRenderExtension(const vex::PlatformWindowHandle& handle)
{
    SetupRenderDoc(handle.window);
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
