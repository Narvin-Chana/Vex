#include "PixEventsRenderExtension.h"

#include <VexPixEvents.h>

PixEventsRenderExtension::PixEventsRenderExtension()
{
    SetupPixEvents();
}

void PixEventsRenderExtension::StartCapture(std::wstring_view captureName)
{
    StartPixEventsCapture(captureName);
}

void PixEventsRenderExtension::EndCapture()
{
    EndPixEventsCapture();
}
