#include "Renderer/RendererApp.h"

#include "Core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <windows.h>

#include <string>

namespace
{
void GlfwErrorCallback(int code, const char* description)
{
    YRender::LogError(std::string("GLFW error ") + std::to_string(code) + ": " + (description ? description : "unknown"));
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    (void)instance;
    (void)showCommand;

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {
        return -1;
    }

    YRender::RendererApp app;
    const int result = app.Run();

    glfwTerminate();
    return result;
}
