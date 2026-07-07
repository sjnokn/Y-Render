#include "Core/Platform.h"
#include "Renderer/RendererApp.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    YRender::ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx");
    YRender::RendererApp app;
    const int result = app.Run(instance, showCommand);
    CoUninitialize();
    return result;
}
