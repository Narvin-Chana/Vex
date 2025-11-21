#include "VexRenderDoc.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

#include <renderdoc_app.h>

#include <Vex/Logger.h>

namespace RenderDoc
{
static RENDERDOC_API_1_1_2* GRDoc_api = nullptr;

static void LoadRenderDocAPI()
{
#if defined(_WIN32)
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&GRDoc_api);
        if (ret == 1)
        {
            VEX_LOG(vex::Warning, "Unable to get RenderDoc API");
        }
    }
#elif defined(__linux__)
    if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&GRDoc_api);
        if (ret == 1)
        {
            VEX_LOG(vex::Warning, "Unable to get RenderDoc API");
        }
    }
#endif
}

void Setup()
{
    if (!GRDoc_api)
    {
        LoadRenderDocAPI();
    }
}

void StartCapture()
{
    if (GRDoc_api)
        GRDoc_api->StartFrameCapture(nullptr, nullptr);
}

void EndCapture(void* devicePtr)
{
    if (GRDoc_api)
        GRDoc_api->EndFrameCapture(nullptr, nullptr);
}
} // namespace RenderDoc