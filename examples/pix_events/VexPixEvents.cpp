#include "VexPixEvents.h"

#include <string>

#include <WinPixEventRuntime/pix3.h>
#include <Windows.h>

void SetupPixEvents()
{
    PIXLoadLatestWinPixGpuCapturerLibrary();
}

void StartPixEventsCapture(std::wstring_view captureName)
{
    std::wstring captureNameStr{ captureName };
    PIXCaptureParameters Params{ .GpuCaptureParameters = { .FileName = captureNameStr.c_str() } };
    PIXBeginCapture(PIX_CAPTURE_GPU, &Params);
}

void EndPixEventsCapture()
{
    PIXEndCapture(false);
}
