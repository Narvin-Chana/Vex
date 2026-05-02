#pragma once

#include <string>
#include <string_view>

// clang-format off
// pix3.h needs Windows.h to work properly, the include order matters here!
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
// clang-format on

#if VEX_MODULES
#include <VexMacros.h>
import Vex;
#else
#include <Vex.h>
#endif

namespace PIX
{

inline HMODULE GPIXLibrary = nullptr;

inline void Setup()
{
    GPIXLibrary = PIXLoadLatestWinPixGpuCapturerLibrary();
    if (!GPIXLibrary)
    {
        VEX_LOG(vex::Fatal, "Unable to load PIX library...");
    }
}

inline void StartCapture(std::wstring_view captureName)
{
    std::wstring captureNameStr{ captureName };
    PIXCaptureParameters Params{ .GpuCaptureParameters = { .FileName = captureNameStr.c_str() } };
    if (FAILED(PIXBeginCapture(PIX_CAPTURE_GPU, &Params)))
    {
        VEX_LOG(vex::Fatal, "Unable to begin PIX capture...");
    }
}

inline void EndCapture()
{
    if (FAILED(PIXEndCapture(false)))
    {
        VEX_LOG(vex::Fatal, "Unable to end PIX capture...");
    }
}

inline void Teardown()
{
    if (GPIXLibrary)
    {
        FreeLibrary(GPIXLibrary);
    }
}
} // namespace PIX
