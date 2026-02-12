#pragma once

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

#include <renderdoc_app.h>

#include <Vex/Logger.h>
#include <Vex/Utility/Validation.h>

namespace RenderDoc
{
inline RENDERDOC_API_1_1_2* GRDoc_api = nullptr;
#if defined(_WIN32)
inline HMODULE GModule = nullptr;
#elif defined(__linux__)
inline void* GModule = nullptr;
#endif

inline void LoadRenderDocAPI()
{
#if defined(_WIN32)
    if (GModule = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(GModule, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&GRDoc_api);
        if (ret == 1)
        {
            VEX_LOG(vex::Warning, "Unable to get RenderDoc API");
        }
    }
#elif defined(__linux__)
    if (GModule = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(GModule, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&GRDoc_api);
        if (ret == 1)
        {
            VEX_LOG(vex::Warning, "Unable to get RenderDoc API");
        }
    }
#endif
}

inline void Setup()
{
    if (!GRDoc_api)
    {
        LoadRenderDocAPI();
    }
}

inline void StartCapture()
{
    if (GRDoc_api)
        GRDoc_api->StartFrameCapture(nullptr, nullptr);
}

inline void EndCapture()
{
    if (GRDoc_api)
    {
        // Confusingly enough, this returns 1 on success and 0 on failure, which is opposite to what RENDERDOC_GetAPI(...) returns.
        VEX_CHECK(GRDoc_api->EndFrameCapture(nullptr, nullptr), "RenderDoc indicated an error when capturing!");
    }
}

inline void Teardown()
{
    if (GModule)
    {
#if defined(_WIN32)
        FreeLibrary(GModule);
#elif defined(__linux__)
        dlclose(GModule);
#endif
    }
}
} // namespace RenderDoc