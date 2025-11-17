#include "PixEventsRenderExtension.h"

#include <VexPixEvents.h>

PixEventsRenderExtension::PixEventsRenderExtension()
{
    SetupPixEvents();
}

void PixEventsRenderExtension::StartCapture()
{
    StartPixEventsCapture();
}

void PixEventsRenderExtension::EndCapture()
{
    EndPixEventsCapture();
}
