#include "VexPixEvents.h"

#include <string>

// clang-format off
#include <Windows.h>
#include <WinPixEventRuntime/pix3.h>
// clang-format on

namespace PIX
{
void Setup()
{
    PIXLoadLatestWinPixGpuCapturerLibrary();
}

void StartCapture(std::wstring_view captureName)
{
    std::wstring captureNameStr{ captureName };
    PIXCaptureParameters Params{ .GpuCaptureParameters = { .FileName = captureNameStr.c_str() } };
    PIXBeginCapture(PIX_CAPTURE_GPU, &Params);
}

void EndCapture()
{
    PIXEndCapture(false);
}
} // namespace PIX
