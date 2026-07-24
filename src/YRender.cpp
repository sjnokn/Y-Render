#include "Renderer/RendererApp.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    YRender::RendererApp app;
    return app.Run(instance, showCommand);
}
