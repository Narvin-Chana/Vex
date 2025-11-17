#include "VexPixEvents.h"

#if defined(_WIN32)
#define USE_PIX
#include <WinPixEventRuntime/pix3.h>
#endif

void SetupPixEvents()
{
    PIXLoadLatestWinPixGpuCapturerLibrary();
}

void StartPixEventsCapture()
{
    PIXBeginCapture(PIX_CAPTURE_GPU, Params);
}

void EndPixEventsCapture()
{
    PIXEndCapture(false);
}
